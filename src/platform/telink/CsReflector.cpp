/*
 * Channel Sounding RAS Reflector for Matter (Telink).
 * Adapted from samples/net/openthread/ot_ble_test/src/cs_reflector.c
 *
 * Copyright (c) 2026 Telink Semiconductor
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CsReflector.h"

#include <platform/internal/CHIPDeviceLayerInternal.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/cs.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>

#include <cstring>

namespace chip {
namespace DeviceLayer {
namespace Internal {

using namespace chip::Logging;

/* ── RAS Service UUIDs ─────────────────────────────────────────────── */
#define BT_UUID_RAS_SERVICE_VAL 0x185B
#define BT_UUID_RAS_FEATURE_VAL 0x2C14
#define BT_UUID_RAS_REAL_TIME_DATA_VAL 0x2C15
#define BT_UUID_RAS_ON_DEMAND_DATA_VAL 0x2C16
#define BT_UUID_RAS_CONTROL_POINT_VAL 0x2C17
#define BT_UUID_RAS_DATA_READY_VAL 0x2C18
#define BT_UUID_RAS_DATA_OVERWRITTEN_VAL 0x2C19

#define BT_UUID_RAS_SERVICE BT_UUID_DECLARE_16(BT_UUID_RAS_SERVICE_VAL)
#define BT_UUID_RAS_FEATURE BT_UUID_DECLARE_16(BT_UUID_RAS_FEATURE_VAL)
#define BT_UUID_RAS_REAL_TIME_DATA BT_UUID_DECLARE_16(BT_UUID_RAS_REAL_TIME_DATA_VAL)
#define BT_UUID_RAS_ON_DEMAND_DATA BT_UUID_DECLARE_16(BT_UUID_RAS_ON_DEMAND_DATA_VAL)
#define BT_UUID_RAS_CONTROL_POINT BT_UUID_DECLARE_16(BT_UUID_RAS_CONTROL_POINT_VAL)
#define BT_UUID_RAS_DATA_READY BT_UUID_DECLARE_16(BT_UUID_RAS_DATA_READY_VAL)
#define BT_UUID_RAS_DATA_OVERWRITTEN BT_UUID_DECLARE_16(BT_UUID_RAS_DATA_OVERWRITTEN_VAL)

/* ── RAS GATT attribute indices ────────────────────────────────────── */
enum : uint8_t
{
    kRasIdxSvc,
    kRasIdxFeatureChar,
    kRasIdxFeatureVal,
    kRasIdxRtDataChar,
    kRasIdxRtDataVal,
    kRasIdxRtDataCcc,
    kRasIdxOdDataChar,
    kRasIdxOdDataVal,
    kRasIdxOdDataCcc,
    kRasIdxCpChar,
    kRasIdxCpVal,
    kRasIdxCpCcc,
    kRasIdxDrChar,
    kRasIdxDrVal,
    kRasIdxDrCcc,
    kRasIdxDoChar,
    kRasIdxDoVal,
    kRasIdxDoCcc,
    kRasIdxMax,
};

/* ── Constants ─────────────────────────────────────────────────────── */
#define CS_CONFIG_ID 0
#define NUM_MODE_0_STEPS 1

#define RAS_RANGING_HEADER_LEN 4
#define RAS_SUBEVENT_HEADER_LEN 8
#define RAS_SEG_HEADER_LEN 1

#define RAS_PROCEDURE_MAX_SIZE 1000
#define RAS_PROCEDURE_SLOTS 1

#define MAX_RAS_DATA_SIZE 1000
#define RAS_CHUNK_BUF_SIZE 248

/* ── RAS feature bitmask (Core Spec v6.0 Vol 3B 7.3) ───────────────── */
static uint8_t sRasFeature = (1 << 0) | (1 << 1) | (1 << 2);

/* ── CCC enable flags ──────────────────────────────────────────────── */
static uint16_t sRasRealtimeCcc;
static uint16_t sRasOndemandCcc;
static uint16_t sRasControlpointCcc;
static uint16_t sRasDatareadyCcc;
static uint16_t sRasDataoverwrittenCcc;

/* ── Latest procedure counter ──────────────────────────────────────── */
static uint16_t sLatestProcedureCounter;

/* ── Connection ────────────────────────────────────────────────────── */
static struct bt_conn * sConnection;

/* ── RAS Procedure Slot ────────────────────────────────────────────── */
struct RasProcedureSlot
{
    uint8_t data[RAS_PROCEDURE_MAX_SIZE];
    uint16_t len;
    uint16_t procedureCounter;
    uint8_t configId;
    bool inUse;
};

static RasProcedureSlot sRasProcSlots[RAS_PROCEDURE_SLOTS];
static int sRasCurrentSlot;
static uint16_t sRasCurrentProcedureCounter;
static bool sRasFirstSubevent;

static int8_t sRasSelectedTxPower;
static uint8_t sRasSegCounter;

/* ── RAS work queue data ───────────────────────────────────────────── */
static uint8_t sRasWorkBuf[MAX_RAS_DATA_SIZE];
static uint16_t sRasWorkLen;
static const struct bt_gatt_attr * sRasWorkAttr;
static struct bt_conn * sRasWorkConn;
static bool sRasWorkIsFirst;
static bool sRasWorkIsLast;
static uint8_t sRasWorkSegStart;
static volatile bool sRasSending;

static K_WORK_DEFINE(sRasSendWork, CsReflector::RasSendWorkHandler);
static uint8_t sRasChunkBuf[RAS_CHUNK_BUF_SIZE];

static K_WORK_DEFINE(sCsSetDefaultSettingsWork, CsReflector::CsSetDefaultSettingsWorkHandler);

/* ── RAS GATT service definition ───────────────────────────────────── */
static struct bt_gatt_attr sRasAttrs[] = {
    BT_GATT_PRIMARY_SERVICE(BT_UUID_RAS_SERVICE),

    BT_GATT_CHARACTERISTIC(BT_UUID_RAS_FEATURE, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, CsReflector::RasGattRead, NULL, NULL),

    BT_GATT_CHARACTERISTIC(BT_UUID_RAS_REAL_TIME_DATA, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ,
                           CsReflector::RasGattRead, NULL, NULL),
    BT_GATT_CCC(CsReflector::RasGattCccCfgChanged, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(BT_UUID_RAS_ON_DEMAND_DATA, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ,
                           CsReflector::RasGattRead, NULL, NULL),
    BT_GATT_CCC(CsReflector::RasGattCccCfgChanged, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(BT_UUID_RAS_CONTROL_POINT, BT_GATT_CHRC_WRITE | BT_GATT_CHRC_INDICATE, BT_GATT_PERM_WRITE, NULL,
                           CsReflector::RasGattCpWrite, NULL),
    BT_GATT_CCC(CsReflector::RasGattCccCfgChanged, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(BT_UUID_RAS_DATA_READY, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ,
                           CsReflector::RasGattRead, NULL, NULL),
    BT_GATT_CCC(CsReflector::RasGattCccCfgChanged, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(BT_UUID_RAS_DATA_OVERWRITTEN, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(CsReflector::RasGattCccCfgChanged, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
};

static struct bt_gatt_service sRasService = BT_GATT_SERVICE(sRasAttrs);

/* ── GATT callbacks ────────────────────────────────────────────────── */
static struct bt_gatt_cb sGattCallbacks = {
    .att_mtu_updated = CsReflector::MtuUpdatedCb,
};

/* ── Auth info callbacks ───────────────────────────────────────────── */
static struct bt_conn_auth_info_cb sConnAuthInfoCallbacks = {
    .pairing_complete = CsReflector::PairingCompleteCb,
    .pairing_failed   = CsReflector::PairingFailedCb,
};

/* ── CS connection callbacks ───────────────────────────────────────── */
BT_CONN_CB_DEFINE(sConnCsCallbacks) = {
#if defined(CONFIG_BT_SMP)
    .security_changed = CsReflector::SecurityChangedCb,
#endif
    .le_cs_remote_capabilities_available = CsReflector::RemoteCapabilitiesCb,
    .le_cs_config_created                = CsReflector::ConfigCreatedCb,
    .le_cs_subevent_data_available       = CsReflector::SubeventResultCb,
    .le_cs_security_enabled              = CsReflector::SecurityEnabledCb,
    .le_cs_procedure_enabled             = CsReflector::ProcedureEnabledCb,
};

/* ════════════════════════════════════════════════════════════════════
 *  RAS Helpers
 * ════════════════════════════════════════════════════════════════════ */

uint8_t CsReflector::RasBuildSegHeader(bool firstSeg, bool lastSeg, uint8_t segIdx)
{
    uint8_t hdr = 0;

    if (firstSeg)
        hdr |= (1 << 0);
    if (lastSeg)
        hdr |= (1 << 1);
    hdr |= ((segIdx & 0x3F) << 2);
    return hdr;
}

void CsReflector::RasBuildRangingHeader(uint8_t * buf, uint16_t procedureCounter, uint8_t configId, int8_t txPower,
                                        uint8_t numAntennaPaths)
{
    uint16_t word0 = (procedureCounter & 0x0FFF) | ((configId & 0x0F) << 12);

    buf[0] = static_cast<uint8_t>(word0 >> 0);
    buf[1] = static_cast<uint8_t>(word0 >> 8);
    buf[2] = static_cast<uint8_t>(txPower);

    buf[3] = 0;
    for (uint8_t i = 0; i < numAntennaPaths && i < 4; i++)
    {
        buf[3] |= (1 << i);
    }
}

void CsReflector::RasBuildSubeventHeader(uint8_t * buf, struct bt_conn_le_cs_subevent_result * result)
{
    buf[0] = (result->header.start_acl_conn_event >> 0) & 0xFF;
    buf[1] = (result->header.start_acl_conn_event >> 8) & 0xFF;

    buf[2] = (result->header.frequency_compensation >> 0) & 0xFF;
    buf[3] = (result->header.frequency_compensation >> 8) & 0xFF;

    buf[4] = ((result->header.procedure_done_status & 0x0F) << 0) | ((result->header.subevent_done_status & 0x0F) << 4);

    buf[5] = ((result->header.procedure_abort_reason & 0x0F) << 0) | ((result->header.subevent_abort_reason & 0x0F) << 4);

    buf[6] = static_cast<uint8_t>(result->header.reference_power_level);

    buf[7] = result->header.num_steps_reported;
}

uint16_t CsReflector::RasConvertStepData(uint8_t * buf, struct bt_conn_le_cs_subevent_result * result)
{
    struct net_buf_simple * sbuf = result->step_data_buf;
    uint8_t numSteps             = result->header.num_steps_reported;
    bool subeventAborted         = (result->header.subevent_done_status == BT_CONN_LE_CS_SUBEVENT_ABORTED);
    uint8_t abortStep            = result->header.abort_step;
    uint8_t * wptr               = buf;
    uint16_t offset              = 0;

    if (!sbuf || sbuf->len == 0 || numSteps == 0)
        return 0;

    for (uint8_t i = 0; i < numSteps; i++)
    {
        bool stepAborted = subeventAborted && (i >= abortStep);

        if (offset + 3 > sbuf->len)
        {
            *wptr++ = 0x80;
            break;
        }

        uint8_t stepMode    = sbuf->data[offset];
        uint8_t stepDataLen = sbuf->data[offset + 2];

        *wptr++ = (stepMode & 0x03) | (stepAborted ? 0x80 : 0x00);

        offset += 3;

        if (!stepAborted && stepDataLen > 0)
        {
            if (offset + stepDataLen <= sbuf->len)
            {
                memcpy(wptr, &sbuf->data[offset], stepDataLen);
                wptr += stepDataLen;
            }
            offset += stepDataLen;
        }
        else if (stepDataLen > 0)
        {
            offset += stepDataLen;
        }
    }

    return static_cast<uint16_t>(wptr - buf);
}

uint16_t CsReflector::RasBuildSubeventData(uint8_t * buf, struct bt_conn_le_cs_subevent_result * result, bool isFirst)
{
    uint8_t * wptr = buf;

    if (isFirst)
    {
        RasBuildRangingHeader(wptr, result->header.procedure_counter, result->header.config_id, sRasSelectedTxPower,
                              result->header.num_antenna_paths);
        wptr += RAS_RANGING_HEADER_LEN;
    }

    RasBuildSubeventHeader(wptr, result);
    wptr += RAS_SUBEVENT_HEADER_LEN;

    if (result->step_data_buf && result->step_data_buf->len > 0)
    {
        wptr += RasConvertStepData(wptr, result);
    }

    return static_cast<uint16_t>(wptr - buf);
}

void CsReflector::RasNotifyWithFrag(struct bt_conn * conn, const struct bt_gatt_attr * attr, const void * data, uint16_t len,
                                    bool isProcFirst, bool isProcLast)
{
    if (sRasSending)
    {
        ChipLogDetail(DeviceLayer, "CS: RAS send busy, dropping data");
        return;
    }

    if (len > MAX_RAS_DATA_SIZE)
    {
        ChipLogDetail(DeviceLayer, "CS: RAS data too large %u > %u, truncating", len, MAX_RAS_DATA_SIZE);
        len = MAX_RAS_DATA_SIZE;
    }

    memcpy(sRasWorkBuf, data, len);
    sRasWorkLen      = len;
    sRasWorkAttr     = attr;
    sRasWorkConn     = bt_conn_ref(conn);
    sRasWorkIsFirst  = isProcFirst;
    sRasWorkIsLast   = isProcLast;
    sRasWorkSegStart = sRasSegCounter;
    sRasSending      = true;

    ChipLogDetail(DeviceLayer, "CS: RAS submit work %u bytes first=%d last=%d", len, isProcFirst, isProcLast);

    k_work_submit(&sRasSendWork);
}

/* ════════════════════════════════════════════════════════════════════
 *  Work Handlers
 * ════════════════════════════════════════════════════════════════════ */

void CsReflector::RasSendWorkHandler(struct k_work * /*work*/)
{
    uint16_t mtu        = bt_gatt_get_mtu(sRasWorkConn);
    uint16_t maxPayload = (mtu > 3) ? (mtu - 3) : 20;
    uint16_t offset     = 0;
    uint8_t segIdx      = sRasWorkSegStart;

    maxPayload -= RAS_SEG_HEADER_LEN;

    while (offset < sRasWorkLen)
    {
        uint16_t remaining = sRasWorkLen - offset;
        uint16_t chunkLen  = (remaining < maxPayload) ? remaining : maxPayload;
        bool firstSeg      = (sRasWorkIsFirst && offset == 0);
        bool lastSeg       = (sRasWorkIsLast && offset + chunkLen >= sRasWorkLen);
        uint8_t segHdr     = RasBuildSegHeader(firstSeg, lastSeg, segIdx);

        sRasChunkBuf[0] = segHdr;
        memcpy(&sRasChunkBuf[1], &sRasWorkBuf[offset], chunkLen);

        int err = bt_gatt_notify(sRasWorkConn, sRasWorkAttr, sRasChunkBuf, 1 + chunkLen);
        if (err)
        {
            ChipLogDetail(DeviceLayer, "CS: RAS chunk fail err=%d seg=%u", err, segIdx);
            break;
        }

        ChipLogDetail(DeviceLayer, "CS: RAS chunk ok seg=%u first=%d last=%d off=%u len=%u", segIdx, firstSeg, lastSeg, offset,
                      chunkLen);

        offset += chunkLen;
        segIdx = (segIdx + 1) & 0x3F;
    }

    sRasSegCounter = segIdx;
    ChipLogDetail(DeviceLayer, "CS: RAS send done, total=%u seg_counter=%u", sRasWorkLen, sRasSegCounter);

    bt_conn_unref(sRasWorkConn);
    sRasWorkConn = nullptr;
    sRasSending  = false;
}

void CsReflector::CsSetDefaultSettingsWorkHandler(struct k_work * /*work*/)
{
    const struct bt_le_cs_set_default_settings_param defaultSettings = {
        .enable_initiator_role     = false,
        .enable_reflector_role     = true,
        .cs_sync_antenna_selection = BT_LE_CS_ANTENNA_SELECTION_OPT_REPETITIVE,
        .max_tx_power              = BT_HCI_OP_LE_CS_MAX_MAX_TX_POWER,
    };

    if (!sConnection)
    {
        ChipLogProgress(DeviceLayer, "CS: no connection, skip default settings");
        return;
    }

    int err = bt_le_cs_set_default_settings(sConnection, &defaultSettings);
    if (err)
    {
        ChipLogProgress(DeviceLayer, "CS: failed to configure default CS settings (err %d)", err);
    }
    else
    {
        ChipLogProgress(DeviceLayer, "CS: default settings configured (reflector role)");
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  CS Subevent Result
 * ════════════════════════════════════════════════════════════════════ */

static uint8_t sRasSubeventBuf[1000];

void CsReflector::SubeventResultCb(struct bt_conn * conn, struct bt_conn_le_cs_subevent_result * result)
{
    uint16_t procCounter = result->header.procedure_counter;

    ChipLogDetail(DeviceLayer, "CS: subevent proc=%u cfg=%u steps=%u AP=%u done=%u", procCounter, result->header.config_id,
                  result->header.num_steps_reported, result->header.num_antenna_paths, result->header.procedure_done_status);

    if (!sRasFirstSubevent || procCounter != sRasCurrentProcedureCounter)
    {
        sRasCurrentProcedureCounter = procCounter;
        sRasFirstSubevent           = true;
        sRasSegCounter              = 0;

        sRasCurrentSlot = -1;
        for (int i = 0; i < RAS_PROCEDURE_SLOTS; i++)
        {
            if (!sRasProcSlots[i].inUse)
            {
                sRasCurrentSlot = i;
                break;
            }
        }
        if (sRasCurrentSlot < 0)
        {
            sRasCurrentSlot = 0;
            ChipLogDetail(DeviceLayer, "CS: RAS overwriting oldest slot");
        }

        sRasProcSlots[sRasCurrentSlot].inUse            = true;
        sRasProcSlots[sRasCurrentSlot].len              = 0;
        sRasProcSlots[sRasCurrentSlot].procedureCounter = procCounter;
        sRasProcSlots[sRasCurrentSlot].configId         = result->header.config_id;

        ChipLogDetail(DeviceLayer, "CS: RAS new procedure %u, slot %d", procCounter, sRasCurrentSlot);
    }

    bool isFirst      = sRasFirstSubevent;
    sRasFirstSubevent = false;

    uint16_t subeventLen = RasBuildSubeventData(sRasSubeventBuf, result, isFirst);

    if (sRasCurrentSlot >= 0)
    {
        RasProcedureSlot * slot = &sRasProcSlots[sRasCurrentSlot];
        uint16_t newLen         = slot->len + subeventLen;

        if (newLen <= RAS_PROCEDURE_MAX_SIZE)
        {
            memcpy(&slot->data[slot->len], sRasSubeventBuf, subeventLen);
            slot->len = newLen;
        }
        else
        {
            ChipLogDetail(DeviceLayer, "CS: RAS slot %d overflow, dropping subevent", sRasCurrentSlot);
        }
    }

    if (sRasRealtimeCcc & BT_GATT_CCC_NOTIFY)
    {
        bool isProcLast = (result->header.procedure_done_status == BT_CONN_LE_CS_PROCEDURE_COMPLETE);

        RasNotifyWithFrag(conn, &sRasAttrs[kRasIdxRtDataVal], sRasSubeventBuf, subeventLen, isFirst, isProcLast);
    }

    sLatestProcedureCounter = procCounter;

    if (result->header.procedure_done_status == BT_CONN_LE_CS_PROCEDURE_COMPLETE)
    {
        ChipLogProgress(DeviceLayer, "CS: RAS procedure %u complete, %u bytes in slot %d", procCounter,
                        sRasProcSlots[sRasCurrentSlot].len, sRasCurrentSlot);

        if (sRasDatareadyCcc & BT_GATT_CCC_NOTIFY)
        {
            uint8_t drData[2];
            sys_put_le16(sLatestProcedureCounter & 0x0FFF, drData);
            bt_gatt_notify(conn, &sRasAttrs[kRasIdxDrVal], drData, sizeof(drData));
            ChipLogDetail(DeviceLayer, "CS: RAS notified Data Ready procedure=%u", sLatestProcedureCounter);
        }
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  RAS GATT Callbacks
 * ════════════════════════════════════════════════════════════════════ */

ssize_t CsReflector::RasGattRead(struct bt_conn * conn, const struct bt_gatt_attr * attr, void * buf, uint16_t len, uint16_t offset)
{
    if (attr == &sRasAttrs[kRasIdxFeatureVal])
    {
        return bt_gatt_attr_read(conn, attr, buf, len, offset, &sRasFeature, sizeof(sRasFeature));
    }
    ChipLogDetail(DeviceLayer, "CS: RAS GATT read: handle 0x%x len=%u offset=%u", attr->handle, len, offset);
    return 0;
}

void CsReflector::RasGattCccCfgChanged(const struct bt_gatt_attr * attr, uint16_t value)
{
    ChipLogDetail(DeviceLayer, "CS: RAS CCC changed: handle 0x%x val=0x%04x", attr->handle, value);

    if (attr == &sRasAttrs[kRasIdxRtDataCcc])
        sRasRealtimeCcc = value;
    else if (attr == &sRasAttrs[kRasIdxOdDataCcc])
        sRasOndemandCcc = value;
    else if (attr == &sRasAttrs[kRasIdxCpCcc])
        sRasControlpointCcc = value;
    else if (attr == &sRasAttrs[kRasIdxDrCcc])
        sRasDatareadyCcc = value;
    else if (attr == &sRasAttrs[kRasIdxDoCcc])
        sRasDataoverwrittenCcc = value;
}

ssize_t CsReflector::RasGattCpWrite(struct bt_conn * conn, const struct bt_gatt_attr * attr, const void * buf, uint16_t len,
                                    uint16_t offset, uint8_t flags)
{
    const uint8_t * data = static_cast<const uint8_t *>(buf);
    uint8_t opcode       = data[0];
    static uint8_t cpRsp[3];

    ChipLogDetail(DeviceLayer, "CS: RAS Control Point: opcode=0x%02x len=%u", opcode, len);

    switch (opcode)
    {
    case 0x01:
        ChipLogDetail(DeviceLayer, "CS:   => Set RAS Configuration");
        break;
    case 0x02:
        ChipLogDetail(DeviceLayer, "CS:   => RAS Control Point Command");
        break;
    case 0x03:
        ChipLogDetail(DeviceLayer, "CS:   => Get Procedure Data");
        break;
    case 0x04:
        ChipLogDetail(DeviceLayer, "CS:   => Abort RAS Procedure");
        break;
    default:
        ChipLogDetail(DeviceLayer, "CS:   => Unknown opcode");
        break;
    }

    if (sRasControlpointCcc & BT_GATT_CCC_INDICATE)
    {
        static struct bt_gatt_indicate_params indParams;

        cpRsp[0] = opcode;
        cpRsp[1] = 0x00; /* success */
        cpRsp[2] = 0x00;

        indParams.attr    = const_cast<struct bt_gatt_attr *>(attr);
        indParams.data    = cpRsp;
        indParams.len     = sizeof(cpRsp);
        indParams.func    = nullptr;
        indParams.destroy = nullptr;

        bt_gatt_indicate(conn, &indParams);
        ChipLogDetail(DeviceLayer, "CS:   => Indication sent");
    }
    return len;
}

/* ════════════════════════════════════════════════════════════════════
 *  CS Connection Callbacks
 * ════════════════════════════════════════════════════════════════════ */

void CsReflector::SecurityChangedCb(struct bt_conn * conn, bt_security_t level, enum bt_security_err err)
{
    if (err)
    {
        ChipLogProgress(DeviceLayer, "CS: security failed: level %u err %d", level, err);
        return;
    }
    ChipLogProgress(DeviceLayer, "CS: security changed: level %u", level);
}

void CsReflector::RemoteCapabilitiesCb(struct bt_conn * conn, struct bt_conn_le_cs_capabilities * params)
{
    (void) params;
    ChipLogProgress(DeviceLayer, "CS: capability exchange completed");
}

void CsReflector::ConfigCreatedCb(struct bt_conn * conn, struct bt_conn_le_cs_config * config)
{
    ChipLogProgress(DeviceLayer, "CS: config creation complete. ID: %d", config->id);
}

void CsReflector::SecurityEnabledCb(struct bt_conn * conn)
{
    ChipLogProgress(DeviceLayer, "CS: security enabled");
    k_work_submit(&sCsSetDefaultSettingsWork);
}

void CsReflector::ProcedureEnabledCb(struct bt_conn * conn, struct bt_conn_le_cs_procedure_enable_complete * params)
{
    ChipLogProgress(DeviceLayer, "CS: procedures %s, selected_tx_power=%d dBm", params->state ? "enabled" : "disabled",
                    params->selected_tx_power);

    if (params->selected_tx_power != 0x7F)
        sRasSelectedTxPower = params->selected_tx_power;
    else
        sRasSelectedTxPower = 0;
}

/* ════════════════════════════════════════════════════════════════════
 *  Auth Info Callbacks
 * ════════════════════════════════════════════════════════════════════ */

void CsReflector::PairingCompleteCb(struct bt_conn * conn, bool bonded)
{
    ChipLogProgress(DeviceLayer, "CS: pairing complete: %s", bonded ? "bonded" : "not bonded");
}

void CsReflector::PairingFailedCb(struct bt_conn * conn, enum bt_security_err reason)
{
    ChipLogProgress(DeviceLayer, "CS: pairing failed: reason %d", reason);
}

/* ════════════════════════════════════════════════════════════════════
 *  GATT / MTU Callbacks
 * ════════════════════════════════════════════════════════════════════ */

void CsReflector::MtuExchangeCb(struct bt_conn * conn, uint8_t err, struct bt_gatt_exchange_params * params)
{
    ChipLogProgress(DeviceLayer, "CS: MTU exchange %s (%u)", err == 0U ? "success" : "failed", bt_gatt_get_mtu(conn));
}

void CsReflector::MtuUpdatedCb(struct bt_conn * conn, uint16_t tx, uint16_t rx)
{
    ChipLogProgress(DeviceLayer, "CS: MTU updated: TX=%u RX=%u", tx, rx);
}

/* ════════════════════════════════════════════════════════════════════
 *  Public API
 * ════════════════════════════════════════════════════════════════════ */

void CsReflector::Init(void)
{
    ChipLogProgress(DeviceLayer, "CS: RAS Reflector init");

    bt_conn_auth_info_cb_register(&sConnAuthInfoCallbacks);
    bt_gatt_cb_register(&sGattCallbacks);

    int err = bt_gatt_service_register(&sRasService);
    if (err)
    {
        ChipLogError(DeviceLayer, "CS: RAS service register failed (err %d)", err);
        return;
    }
    ChipLogProgress(DeviceLayer, "CS: RAS service registered");
}

void CsReflector::OnConnected(struct bt_conn * conn, uint8_t err)
{
    if (err)
        return;

    if (sConnection)
    {
        bt_conn_unref(sConnection);
        sConnection = nullptr;
    }

    sConnection = bt_conn_ref(conn);

    static struct bt_gatt_exchange_params mtuExchangeParams;
    mtuExchangeParams.func = MtuExchangeCb;
    int mtuErr             = bt_gatt_exchange_mtu(sConnection, &mtuExchangeParams);
    if (mtuErr)
    {
        ChipLogProgress(DeviceLayer, "CS: MTU exchange failed (err %d)", mtuErr);
    }

    /*     int secErr = bt_conn_set_security(sConnection, BT_SECURITY_L2);
        if (secErr)
        {
            ChipLogProgress(DeviceLayer, "CS: failed to request security (err %d)", secErr);
        } */
}

void CsReflector::OnDisconnected(struct bt_conn * conn)
{
    if (sConnection)
    {
        bt_conn_unref(sConnection);
        sConnection = nullptr;
    }
    sRasFirstSubevent = false;
    sRasSending       = false;
}

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip
