/*
 * Channel Sounding RAS Reflector for Matter (Telink).
 * Adapted from samples/net/openthread/ot_ble_test/src/cs_reflector.c
 *
 * Copyright (c) 2026 Telink Semiconductor
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/cs.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>

namespace chip {
namespace DeviceLayer {
namespace Internal {

/**
 * Channel Sounding RAS (Ranging Service) Reflector.
 *
 * Provides Bluetooth 5.4/6.0 Channel Sounding distance measurement
 * capabilities via the standard RAS GATT service (UUID 0x185B).
 *
 * Usage:
 *   CsReflector::Init();                          // once at boot
 *   CsReflector::OnConnected(conn, err);          // on each BLE connection
 *   CsReflector::OnDisconnected(conn);            // on each BLE disconnection
 */
class CsReflector
{
public:
    static void Init(void);
    static void OnConnected(struct bt_conn * conn, uint8_t err);
    static void OnDisconnected(struct bt_conn * conn);

    /* ── Callbacks exposed for C callback structs ───────────────── */
    static ssize_t RasGattRead(struct bt_conn * conn, const struct bt_gatt_attr * attr, void * buf, uint16_t len, uint16_t offset);
    static void RasGattCccCfgChanged(const struct bt_gatt_attr * attr, uint16_t value);
    static ssize_t RasGattCpWrite(struct bt_conn * conn, const struct bt_gatt_attr * attr, const void * buf, uint16_t len,
                                  uint16_t offset, uint8_t flags);
    static void SecurityChangedCb(struct bt_conn * conn, bt_security_t level, enum bt_security_err err);
    static void RemoteCapabilitiesCb(struct bt_conn * conn, struct bt_conn_le_cs_capabilities * params);
    static void ConfigCreatedCb(struct bt_conn * conn, struct bt_conn_le_cs_config * config);
    static void SecurityEnabledCb(struct bt_conn * conn);
    static void ProcedureEnabledCb(struct bt_conn * conn, struct bt_conn_le_cs_procedure_enable_complete * params);
    static void SubeventResultCb(struct bt_conn * conn, struct bt_conn_le_cs_subevent_result * result);
    static void PairingCompleteCb(struct bt_conn * conn, bool bonded);
    static void PairingFailedCb(struct bt_conn * conn, enum bt_security_err reason);
    static void MtuExchangeCb(struct bt_conn * conn, uint8_t err, struct bt_gatt_exchange_params * params);
    static void MtuUpdatedCb(struct bt_conn * conn, uint16_t tx, uint16_t rx);
    static void RasSendWorkHandler(struct k_work * work);
    static void CsSetDefaultSettingsWorkHandler(struct k_work * work);

private:
    /* ── RAS helpers ────────────────────────────────────────────── */
    static uint8_t RasBuildSegHeader(bool firstSeg, bool lastSeg, uint8_t segIdx);
    static void RasBuildRangingHeader(uint8_t * buf, uint16_t procedureCounter, uint8_t configId, int8_t txPower,
                                      uint8_t numAntennaPaths);
    static void RasBuildSubeventHeader(uint8_t * buf, struct bt_conn_le_cs_subevent_result * result);
    static uint16_t RasConvertStepData(uint8_t * buf, struct bt_conn_le_cs_subevent_result * result);
    static uint16_t RasBuildSubeventData(uint8_t * buf, struct bt_conn_le_cs_subevent_result * result, bool isFirst);
    static void RasNotifyWithFrag(struct bt_conn * conn, const struct bt_gatt_attr * attr, const void * data, uint16_t len,
                                  bool isProcFirst, bool isProcLast);
};

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip
