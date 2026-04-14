/*
 *
 *    Copyright (c) 2022-2026 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "AppTaskCommon.h"
#include "AppTask.h"

#include "BLEManagerImpl.h"
#include "ButtonManager.h"
#include "FabricTableDelegate.h"
#include "LEDManager.h"
#include "PWMManager.h"

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include "ThreadUtil.h"
#elif CHIP_DEVICE_CONFIG_ENABLE_WIFI
#include <platform/Zephyr/InetUtils.h>
#include <platform/telink/wifi/TelinkWiFiDriver.h>
#endif

#include <DeviceInfoProviderImpl.h>
#include <app/clusters/identify-server/identify-server.h>
#include <app/clusters/ota-requestor/OTATestEventTriggerHandler.h>
#include <app/persistence/AttributePersistenceProviderInstance.h>
#include <app/persistence/DefaultAttributePersistenceProvider.h>
#include <app/persistence/DeferredAttributePersistenceProvider.h>
#include <app/server/Server.h>
#include <app/util/endpoint-config-api.h>
#include <setup_payload/OnboardingCodesUtil.h>
#ifdef CONFIG_CHIP_TELINK_ALL_DEVICES_APP
#include "AllDevicesServer.h"
#else
#include <app/util/attribute-storage.h>
#include <data-model-providers/codegen/Instance.h>
#endif

#if CONFIG_BOOTLOADER_MCUBOOT
#include <OTAUtil.h>
#endif

#include "AppConfig.h"
#include <analog.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/reboot.h>

#ifdef CONFIG_MCUMGR_TRANSPORT_BT
#include <DFUOverSMP.h>
#endif

#if CONFIG_CHIP_OTA_REQUESTOR
#include <app/clusters/ota-requestor/OTARequestorInterface.h>
#endif

bool AppTaskCommon::sIsCommissioningFailed = false;

extern "C" {
#if defined(CONFIG_PM) &&                                                                                                          \
    (defined(CONFIG_SOC_SERIES_RISCV_TELINK_B9X_RETENTION) || defined(CONFIG_SOC_SERIES_RISCV_TELINK_TLX_RETENTION))
#include <zephyr/sys/reboot.h>

extern bool pm_has_deep_sleep_retention_occurred(void);
#endif
}

#if defined(CONFIG_PM) && !defined(CONFIG_CHIP_ENABLE_PM_DURING_BLE)
#include <zephyr/pm/policy.h>
#endif

using namespace chip::app;

LOG_MODULE_DECLARE(app, CONFIG_CHIP_APP_LOG_LEVEL);

namespace {
constexpr int kFactoryResetCalcTimeout = 3000;
constexpr int kFactoryResetTriggerCntr = 3;
constexpr int kAppEventQueueSize       = 10;

constexpr uint32_t kIdentifyBlinkRateMs         = 200;
constexpr uint32_t kIdentifyOkayOnRateMs        = 50;
constexpr uint32_t kIdentifyOkayOffRateMs       = 950;
constexpr uint32_t kIdentifyFinishOnRateMs      = 950;
constexpr uint32_t kIdentifyFinishOffRateMs     = 50;
constexpr uint32_t kIdentifyChannelChangeRateMs = 1000;
constexpr uint32_t kIdentifyBreatheRateMs       = 1000;

#if APP_SET_NETWORK_COMM_ENDPOINT_SEC
constexpr EndpointId kNetworkCommissioningEndpointSecondary = 0xFFFE;
#endif

K_MSGQ_DEFINE(sAppEventQueue, sizeof(AppEvent), kAppEventQueueSize, alignof(AppEvent));

k_timer sFactoryResetTimer;
uint8_t sFactoryResetCntr = 0;

bool sIsNetworkProvisioned = false;
bool sIsNetworkEnabled     = false;
bool sIsNetworkAttached    = false;
bool sHaveBLEConnections   = false;

const struct device * flash_para_dev = USER_PARTITION_DEVICE;
const struct device * zb_para_dev    = ZB_NVS_PARTITION_DEVICE;
uint8_t sBoot_zb                     = 0;
constexpr int kDnssTimeout           = 60000; // for init will cost for about 5s
#if !CONFIG_MCUMGR_TRANSPORT_BT
/* Create sDnssTimer when dfu disable */
static k_timer sDnssTimer;
#endif /* !CONFIG_MCUMGR_TRANSPORT_BT */

#if APP_SET_DEVICE_INFO_PROVIDER
chip::DeviceLayer::DeviceInfoProviderImpl gExampleDeviceInfoProvider;
#endif

#ifndef IDENTIFY_CLUSTER_DISABLED

void OnIdentifyTriggerEffect(Identify * identify)
{
    AppTaskCommon::IdentifyEffectHandler(identify->mCurrentEffectIdentifier);
}

Identify sIdentify = {
    kExampleEndpointId,           AppTask::IdentifyStartHandler,
    AppTask::IdentifyStopHandler, Clusters::Identify::IdentifyTypeEnum::kVisibleIndicator,
    OnIdentifyTriggerEffect,
};

#endif

/**
 * @brief Set deferred attributes storage
 *
 * @see Define a custom attribute persister which makes actual write of the CurrentHue, CurrentSaturation, CurrentLevel attributes
 * value to the non-volatile storage only when it has remained constant for 5 seconds. This is to reduce the flash wearout when the
 * attribute changes frequently as a result of MoveToLevel command. DeferredAttribute object describes a deferred attribute, but
 * also holds a buffer with a value to be written, so it must live so long as the DeferredAttributePersistenceProvider object.
 *
 * @param ATTRIBUTES_ARRAY_SIZE The lenght of the DeferredAttribute array
 * @param DEFERRED_STORAGE_TIME The deferred time(ms) to store attributes
 */
#define ATTRIBUTES_ARRAY_SIZE (3U)
#define DEFERRED_STORAGE_TIME (500U)

DeferredAttribute gPersisters[] = {
#if CONFIG_DEFERRED_ATTR_STORAGE
    DeferredAttribute(
        ConcreteAttributePath(kExampleEndpointId, Clusters::ColorControl::Id, Clusters::ColorControl::Attributes::CurrentHue::Id)),
    DeferredAttribute(ConcreteAttributePath(kExampleEndpointId, Clusters::ColorControl::Id,
                                            Clusters::ColorControl::Attributes::CurrentSaturation::Id)),
    DeferredAttribute(
        ConcreteAttributePath(kExampleEndpointId, Clusters::LevelControl::Id, Clusters::LevelControl::Attributes::CurrentLevel::Id))
#endif // CONFIG_DEFERRED_ATTR_STORAGE
};

// Deferred persistence will be auto-initialized as soon as the default persistence is initialized
DefaultAttributePersistenceProvider gSimpleAttributePersistence;
DeferredAttributePersistenceProvider gDeferredAttributePersister(gSimpleAttributePersistence,
                                                                 Span<DeferredAttribute>(gPersisters, ATTRIBUTES_ARRAY_SIZE),
                                                                 System::Clock::Milliseconds32(DEFERRED_STORAGE_TIME));

// NOTE! This key is for test/certification only and should not be available in production devices!
uint8_t sTestEventTriggerEnableKey[TestEventTriggerDelegate::kEnableKeyLength] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                                                                   0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };

class AppCallbacks : public AppDelegate
{
    bool isComissioningStarted;

public:
    void OnCommissioningSessionEstablishmentStarted() override { AppTaskCommon::sIsCommissioningFailed = false; }
    void OnCommissioningSessionStarted() override { isComissioningStarted = true; }
    void OnCommissioningSessionStopped() override { isComissioningStarted = false; }
    void OnCommissioningSessionEstablishmentError(CHIP_ERROR err) override { AppTaskCommon::sIsCommissioningFailed = true; }
#if CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE
    void OnCommissioningWindowClosed() override
    {
        if (!isComissioningStarted)
            chip::DeviceLayer::Internal::BLEMgr().Shutdown();
    }
#endif
};

AppCallbacks sCallbacks;
} // namespace

#if APP_LIGHT_USER_MODE_EN
#if CONFIG_STARTUP_OPTIMIZATE
const struct device * cluster_para_dev = USER_CLUSTER_PARTITION_DEVICE;
uint32_t cluster_para_addr             = USER_CLUSTER_PARTITION_OFFSET;
#define CLUSTER_PARA_LEN (sizeof(cluster_startup_para))
#define USER_CLUSTER_PARTITION_END (USER_CLUSTER_PARTITION_OFFSET + USER_CLUSTER_PARTITION_SIZE)

void clear_cluster_para(void)
{
    flash_erase(cluster_para_dev, USER_CLUSTER_PARTITION_OFFSET, USER_CLUSTER_PARTITION_SIZE);
    cluster_para_addr = USER_CLUSTER_PARTITION_OFFSET;
}

void init_cluster_partition(void)
{
    uint32_t i, cur_addr;
    cluster_startup_para t_cmp;
    cluster_startup_para t_cmp_back;
    memset((void *) (&t_cmp_back), 0xff, CLUSTER_PARA_LEN);

    for (i = 0;; i++)
    {
        cur_addr = USER_CLUSTER_PARTITION_OFFSET + i * CLUSTER_PARA_LEN;
        if (cur_addr >= USER_CLUSTER_PARTITION_END)
        {
            clear_cluster_para();
            break;
        }

        flash_read(cluster_para_dev, cur_addr, &t_cmp, CLUSTER_PARA_LEN);
        if (memcmp(&t_cmp, &t_cmp_back, CLUSTER_PARA_LEN) == 0) // read t_cmp is 0xff
        {
            cluster_para_addr = cur_addr;
            return;
        }
    }
}

int store_cluster_para(cluster_startup_para * data)
{
    if (data == NULL)
    {
        return -1;
    }
    if (cluster_para_addr >= (USER_CLUSTER_PARTITION_END - CLUSTER_PARA_LEN))
    {
        clear_cluster_para();
    }

    flash_write(cluster_para_dev, cluster_para_addr, data, CLUSTER_PARA_LEN);
    cluster_para_addr += CLUSTER_PARA_LEN;
    return 0;
}

int read_cluster_para(cluster_startup_para * data)
{
    if (data == NULL)
    {
        return -1;
    }
    if (cluster_para_addr >= USER_CLUSTER_PARTITION_END)
    {
        clear_cluster_para();
        return -1;
    }
    if ((cluster_para_addr - CLUSTER_PARA_LEN) < USER_CLUSTER_PARTITION_OFFSET)
    {
        return -1;
    }

    cluster_startup_para t_cmp;
    cluster_startup_para t_cmp_back;
    memset((void *) (&t_cmp_back), 0xff, CLUSTER_PARA_LEN);

    flash_read(cluster_para_dev, (cluster_para_addr - CLUSTER_PARA_LEN), &t_cmp, CLUSTER_PARA_LEN);
    if (memcmp(&t_cmp, &t_cmp_back, CLUSTER_PARA_LEN) == 0) // read t_cmp is 0xff, error
    {
        clear_cluster_para();
        return -1;
    }
    memcpy(data, &t_cmp, CLUSTER_PARA_LEN);
    return 0;
}
#endif /* CONFIG_STARTUP_OPTIMIZATE */
#endif /* APP_LIGHT_USER_MODE_EN */

/* Not modify reg addr by user */
#define MATTER_ANALOG_REG_OTA_ADR (0x3b)
#define MATTER_ANALOG_OTA_FLAG_VAL (0x55)

void AppTaskCommon::OtaSetAnaFlag(void)
{
    analog_write(MATTER_ANALOG_REG_OTA_ADR, MATTER_ANALOG_OTA_FLAG_VAL);
}

bool AppTaskCommon::OtaGetAnaFlag(void)
{
    if (analog_read(MATTER_ANALOG_REG_OTA_ADR) == MATTER_ANALOG_OTA_FLAG_VAL)
    {
        return true;
    }
    else
    {
        return false;
    }
}

class PlatformMgrDelegate : public DeviceLayer::PlatformManagerDelegate
{
    // Disable openthread before reset to prevent writing to NVS
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    void OnShutDown() override
    {
        if (ThreadStackManagerImpl().IsThreadEnabled())
        {
            otInstanceFinalize(openthread_get_default_instance());
        }
    }
#endif // CHIP_DEVICE_CONFIG_ENABLE_THREAD
};

#if CONFIG_CHIP_LIB_SHELL
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>

static int cmd_telink_reboot(const struct shell * shell, size_t argc, char ** argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(shell, "Rebooting board");
    sys_reboot(SYS_REBOOT_WARM);

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_telink, SHELL_CMD(reboot, NULL, "Reboot board command", cmd_telink_reboot),
                               SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(telink, &sub_telink, "Telink commands", NULL);
#endif // CONFIG_CHIP_LIB_SHELL

#ifdef CONFIG_CHIP_ENABLE_POWER_ON_FACTORY_RESET
void AppTaskCommon::PowerOnFactoryReset(void)
{
    LOG_INF("schedule factory reset");
    chip::Server::GetInstance().ScheduleFactoryReset();
}
#endif /* CONFIG_CHIP_ENABLE_POWER_ON_FACTORY_RESET */

light_para_t light_para;
user_para_t user_para;
unsigned char para_lightness = 0;
CHIP_ERROR AppTaskCommon::StartApp(void)
{
    /* Proc ota boot flag , and erase flag */
    flash_read(flash_para_dev, USER_PARTITION_OFFSET, &user_para, sizeof(user_para));
    /* Boot from Zigbee , need to clean the user parameters sector first and set a flag */
    if (user_para.val == USER_ZB_SW_VAL)
    {
        // if switch from zb , need to get all the cluster info from zb
        flash_read(flash_para_dev, USER_PARTITION_OFFSET + sizeof(user_para), &light_para, sizeof(light_para));
        // flash_erase(flash_para_dev, USER_PARTITION_OFFSET, USER_PARTITION_SIZE);
        sBoot_zb = 1;
        /* Ensure lightness is at least 2 to avoid display error on HomePod Mini */
        if (light_para.level < 2)
        {
            light_para.level = 2;
        }
        /* Pass the value to the init part to avoid gaps in pwm_pool init */
        if (light_para.onoff)
        {
            para_lightness = light_para.level;
        }
        k_timer_init(&sDnssTimer, &AppTask::DnssTimerTimeoutCallback, nullptr);
        k_timer_start(&sDnssTimer, K_MSEC(kDnssTimeout), K_NO_WAIT);
        printk("Matter: start timer to protect Dnss initialized \r\n");
    }

    CHIP_ERROR err = GetAppTask().Init();

    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("AppTask Init fail");
        return err;
    }

    AppEvent event = {};

#if !CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    StartThreadButtonEventHandler();
#elif CHIP_DEVICE_CONFIG_ENABLE_WIFI
    StartWiFiButtonEventHandler();
#endif
#endif /* CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE */

#ifdef CONFIG_BOOTLOADER_MCUBOOT
    if (!sIsNetworkProvisioned)
    {
        LOG_INF("Confirm image");
        OtaConfirmNewImage();
    }
#endif /* CONFIG_BOOTLOADER_MCUBOOT */

    while (true)
    {
        GetEvent(&event);
        DispatchEvent(&event);
    }
}

#ifdef CONFIG_MCUMGR_TRANSPORT_BT
/* Demonstration of the fail handling */
void HandleDFUFail(VerificationFailReason reason)
{
    LOG_INF("DFU image verification failed with reason: %d", reason);
}
#endif

void AppTaskCommon::PrintFirmwareInfo(void)
{
    LOG_INF("SW Version: %u, %s", CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION, CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING);

#if CONFIG_CHIP_APP_LOG_LEVEL > 3
    LOG_DBG("Matter revision: ");
    LOG_DBG("\t board: %s", CONFIG_BOARD);
    LOG_DBG("\t branch: %s %.8s%s %s", MATTER_BRANCH, MATTER_COMMIT_HASH, MATTER_LOCAL_STATUS, MATTER_COMMIT_DATE);
    LOG_DBG("\t remote: %s", MATTER_REMOTE_URL);
    LOG_DBG("\t build timestamp: %s", BUILD_TIMESTAMP);

    LOG_DBG("Zephyr revision: ");
    LOG_DBG("\t branch: %s %.8s%s %s", ZEPHYR_BRANCH, ZEPHYR_COMMIT_HASH, ZEPHYR_LOCAL_STATUS, ZEPHYR_COMMIT_DATE);
    LOG_DBG("\t remote: %s", ZEPHYR_REMOTE_URL);
    LOG_DBG("\t HAL commit: %.8s%s %s", TELINK_HAL_COMMIT_HASH, TELINK_HAL_LOCAL_STATUS, TELINK_HAL_COMMIT_DATE);
#endif
}

#if INDEPENDENT_FACTORY_RESET_BUTTON
void AppTaskCommon::IndependentFactoryReset(void)
{
    // Get Button Instance
    ButtonManager & buttonManager = ButtonManager::getInstance();
    // Button binding to factory_reset and add callback
    buttonManager.addCallback(FactoryResetButtonEventHandler, 0, true);

#if CONFIG_CHIP_BUTTON_MANAGER_IRQ_MODE // Independent Button Mode
    buttonManager.linkBackend(ButtonPool::getInstance());
#else
    buttonManager.linkBackend(ButtonMatrix::getInstance());
#endif // CONFIG_CHIP_BUTTON_MANAGER_IRQ_MODE
}
#endif

CHIP_ERROR AppTaskCommon::InitCommonParts(void)
{
    PrintFirmwareInfo();

/* if use user mode, should disable the hardware init to avoid conflict */
#if APP_LIGHT_USER_MODE_EN

#if INDEPENDENT_FACTORY_RESET_BUTTON
    IndependentFactoryReset(); // Open the factory_reset button separately.
#endif

#else
    InitLeds();
    UpdateStatusLED();

    InitPwms();

    InitButtons();
#endif

#ifdef CONFIG_TFLM_FEATURE
    mThreadStateChangedEventCaptured = false;
#endif

    // Initialize function button timer
    k_timer_init(&sFactoryResetTimer, &AppTask::FactoryResetTimerTimeoutCallback, nullptr);
    k_timer_user_data_set(&sFactoryResetTimer, this);

    // Initialize CHIP server
#if CONFIG_CHIP_FACTORY_DATA
    ReturnErrorOnFailure(mFactoryDataProvider.Init());
    SetDeviceInstanceInfoProvider(&mFactoryDataProvider);
    SetDeviceAttestationCredentialsProvider(&mFactoryDataProvider);
    SetCommissionableDataProvider(&mFactoryDataProvider);
    // Read EnableKey from the factory data.
    MutableByteSpan enableKey(sTestEventTriggerEnableKey);
    if (mFactoryDataProvider.GetEnableKey(enableKey) != CHIP_NO_ERROR)
    {
        LOG_ERR("GetEnableKey failed. Could not delegate test event trigger");
        memset(sTestEventTriggerEnableKey, 0, sizeof(sTestEventTriggerEnableKey));
    }
#else
    SetDeviceAttestationCredentialsProvider(Examples::GetExampleDACProvider());
#endif

    static CommonCaseDeviceServerInitParams initParams;
    static SimpleTestEventTriggerDelegate sTestEventTriggerDelegate{};
    VerifyOrDie(sTestEventTriggerDelegate.Init(ByteSpan(sTestEventTriggerEnableKey)) == CHIP_NO_ERROR);
#if CONFIG_CHIP_OTA_REQUESTOR
    static OTATestEventTriggerHandler sOtaTestEventTriggerHandler{};
    VerifyOrDie(sTestEventTriggerDelegate.AddHandler(&sOtaTestEventTriggerHandler) == CHIP_NO_ERROR);
#endif
    LogErrorOnFailure(initParams.InitializeStaticResourcesBeforeServerInit());
    VerifyOrDie(gSimpleAttributePersistence.Init(initParams.persistentStorageDelegate) == CHIP_NO_ERROR);
#if APP_SET_DEVICE_INFO_PROVIDER
    gExampleDeviceInfoProvider.SetStorageDelegate(initParams.persistentStorageDelegate);
    chip::DeviceLayer::SetDeviceInfoProvider(&gExampleDeviceInfoProvider);
#endif
    initParams.appDelegate              = &sCallbacks;
    initParams.testEventTriggerDelegate = &sTestEventTriggerDelegate;

#ifdef CONFIG_CHIP_TELINK_ALL_DEVICES_APP
    // all-devices owns data model provider setup because the concrete device
    // type is selected at runtime.
    ReturnErrorOnFailure(chip::app::all_devices::InitAllDevicesServer(initParams));
#else
    // ZAP/codegen applications use the generated data model.
    initParams.dataModelProvider = CodegenDataModelProviderInstance(initParams.persistentStorageDelegate);
    ReturnErrorOnFailure(chip::Server::GetInstance().Init(initParams));

    /* Add deferred storage attribute for provider */
    app::SetAttributePersistenceProvider(&gDeferredAttributePersister);

    ConfigurationMgr().LogDeviceConfig();
    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));

    AppFabricTableDelegate::Init();
#endif // CONFIG_CHIP_TELINK_ALL_DEVICES_APP

#if APP_SET_NETWORK_COMM_ENDPOINT_SEC
    // We only have network commissioning on endpoint 0.
    // Set up a valid Network Commissioning cluster on endpoint 0 is done in
    // src/platform/OpenThread/GenericThreadStackManagerImpl_OpenThread.hpp
    emberAfEndpointEnableDisable(kNetworkCommissioningEndpointSecondary, false);
#endif

#ifdef CONFIG_MCUMGR_TRANSPORT_BT
    GetDFUOverSMP().Init();
    GetDFUOverSMP().SetFailCallback(HandleDFUFail);
#endif

    // We need to disable OpenThread to prevent writing to the NVS storage when factory reset occurs
    // The OpenThread thread is running during factory reset. The nvs_clear function is called during
    // factory reset, which makes the NVS storage innaccessible, but the OpenThread knows nothing
    // about this and tries to store the parameters to NVS. Because of this the OpenThread need to be
    // shut down before NVS. This delegate fixes the issue "Failed to store setting , ret -13",
    // which means that the NVS is already disabled.
    // For this the OnShutdown function is used
    PlatformMgr().SetDelegate(new PlatformMgrDelegate);

    // Add CHIP event handler and start CHIP thread.
    // Note that all the initialization code should happen prior to this point to avoid data races
    // between the main and the CHIP threads.
    LogErrorOnFailure(PlatformMgr().AddEventHandler(ChipEventHandler, 0));

    return CHIP_NO_ERROR;
}

void AppTaskCommon::IdentifyStartHandler(Identify *)
{
    AppEvent event;

    event.Type    = AppEvent::kEventType_IdentifyStart;
    event.Handler = [](AppEvent * event) {
        ChipLogProgress(Zcl, "OnIdentifyStart");
        PwmManager::getInstance().setPwmBlink(PwmManager::EAppPwm_Indication, kIdentifyBlinkRateMs, kIdentifyBlinkRateMs);
    };
    GetAppTask().PostEvent(&event);
}

void AppTaskCommon::IdentifyStopHandler(Identify *)
{
    AppEvent event;

    event.Type    = AppEvent::kEventType_IdentifyStop;
    event.Handler = [](AppEvent * event) {
        ChipLogProgress(Zcl, "OnIdentifyStop");
        PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Indication, false);
    };
    GetAppTask().PostEvent(&event);
}

#ifdef CONFIG_CHIP_PW_RPC
void AppTaskCommon::ButtonEventHandler(ButtonId_t btnId, bool btnPressed)
{
    if (!btnPressed)
    {
        return;
    }

    switch (btnId)
    {
    case kButtonId_ExampleAction:
        ExampleActionButtonEventHandler();
        break;
    case kButtonId_FactoryReset:
        FactoryResetButtonEventHandler();
        break;
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    case kButtonId_StartThread:
        StartThreadButtonEventHandler();
        break;
#elif CHIP_DEVICE_CONFIG_ENABLE_WIFI
    case kButtonId_StartWiFi:
        StartWiFiButtonEventHandler();
        break;
#endif
    case kButtonId_StartBleAdv:
        StartBleAdvButtonEventHandler();
        break;
    }
}
#endif

void AppTaskCommon::InitLeds()
{
    LedManager & ledManager = LedManager::getInstance();

    LinkLeds(ledManager);

    ledManager.linkBackend(LedPool::getInstance());
}

void AppTaskCommon::LinkLeds(LedManager & ledManager)
{
#if CONFIG_CHIP_ENABLE_APPLICATION_STATUS_LED
    ledManager.linkLed(LedManager::EAppLed_Status, 0);
#endif // CONFIG_CHIP_ENABLE_APPLICATION_STATUS_LED
}

void AppTaskCommon::InitPwms()
{
    PwmManager & pwmManager = PwmManager::getInstance();

    LinkPwms(pwmManager);

#if CONFIG_WS2812_STRIP_GPIO_TELINK
    pwmManager.linkBackend(Ws2812Strip::getInstance());
#elif CONFIG_PWM
    pwmManager.linkBackend(PwmPool::getInstance());
#else
    pwmManager.linkBackend(PwmDummy::getInstance());
#endif
}

void AppTaskCommon::LinkPwms(PwmManager & pwmManager)
{
#if CONFIG_BOARD_TLSR9118BDK40D_V1 && CONFIG_PWM // TLSR9118BDK40D_V1 EVK supports single LED PWM channel
    pwmManager.linkPwm(PwmManager::EAppPwm_Red, 0);
#elif CONFIG_WS2812_STRIP_GPIO_TELINK
    pwmManager.linkPwm(PwmManager::EAppPwm_Red, 0);
    pwmManager.linkPwm(PwmManager::EAppPwm_Green, 1);
    pwmManager.linkPwm(PwmManager::EAppPwm_Blue, 2);
#elif CONFIG_PWM
    pwmManager.linkPwm(PwmManager::EAppPwm_Indication, 0);
    pwmManager.linkPwm(PwmManager::EAppPwm_Red, 1);
    pwmManager.linkPwm(PwmManager::EAppPwm_Green, 2);
    pwmManager.linkPwm(PwmManager::EAppPwm_Blue, 3);
#endif
}

void AppTaskCommon::InitButtons(void)
{
    ButtonManager & buttonManager = ButtonManager::getInstance();

    LinkButtons(buttonManager);

#if CONFIG_CHIP_BUTTON_MANAGER_IRQ_MODE
    buttonManager.linkBackend(ButtonPool::getInstance());
#else
    buttonManager.linkBackend(ButtonMatrix::getInstance());
#endif // CONFIG_CHIP_BUTTON_MANAGER_IRQ_MODE
}

void AppTaskCommon::LinkButtons(ButtonManager & buttonManager)
{
    buttonManager.addCallback(FactoryResetButtonEventHandler, 0, true);
    buttonManager.addCallback(ExampleActionButtonEventHandler, 1, true);
#if CONFIG_TELINK_OTA_BUTTON_TEST
    buttonManager.addCallback(TestOTAButtonEventHandler, 2, true);
#else
    buttonManager.addCallback(StartBleAdvButtonEventHandler, 2, true);
#endif
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    buttonManager.addCallback(StartThreadButtonEventHandler, 3, true);
#elif CHIP_DEVICE_CONFIG_ENABLE_WIFI
    buttonManager.addCallback(StartWiFiButtonEventHandler, 3, true);
#endif
}

void AppTaskCommon::UpdateStatusLED()
{
    if (sIsNetworkProvisioned && sIsNetworkEnabled)
    {
        if (sIsNetworkAttached)
        {
            LedManager::getInstance().setLed(LedManager::EAppLed_Status, 950, 50);
        }
        else
        {
            LedManager::getInstance().setLed(LedManager::EAppLed_Status, 100, 100);
        }
    }
    else
    {
        LedManager::getInstance().setLed(LedManager::EAppLed_Status, 50, 950);
    }
}

void AppTaskCommon::IdentifyEffectHandler(Clusters::Identify::EffectIdentifierEnum aEffect)
{
    switch (aEffect)
    {
    case Clusters::Identify::EffectIdentifierEnum::kBlink:
        ChipLogProgress(Zcl, "Clusters::Identify::EffectIdentifierEnum::kBlink");
        PwmManager::getInstance().setPwmBlink(PwmManager::EAppPwm_Indication, kIdentifyBlinkRateMs, kIdentifyBlinkRateMs);
        break;
    case Clusters::Identify::EffectIdentifierEnum::kBreathe:
        ChipLogProgress(Zcl, "Clusters::Identify::EffectIdentifierEnum::kBreathe");
        PwmManager::getInstance().setPwmBreath(PwmManager::EAppPwm_Indication, kIdentifyBreatheRateMs);
        break;
    case Clusters::Identify::EffectIdentifierEnum::kOkay:
        ChipLogProgress(Zcl, "Clusters::Identify::EffectIdentifierEnum::kOkay");
        PwmManager::getInstance().setPwmBlink(PwmManager::EAppPwm_Indication, kIdentifyOkayOnRateMs, kIdentifyOkayOffRateMs);
        break;
    case Clusters::Identify::EffectIdentifierEnum::kChannelChange:
        ChipLogProgress(Zcl, "Clusters::Identify::EffectIdentifierEnum::kChannelChange");
        PwmManager::getInstance().setPwmBlink(PwmManager::EAppPwm_Indication, kIdentifyChannelChangeRateMs,
                                              kIdentifyChannelChangeRateMs);
        break;
    case Clusters::Identify::EffectIdentifierEnum::kFinishEffect:
        ChipLogProgress(Zcl, "Clusters::Identify::EffectIdentifierEnum::kFinishEffect");
        PwmManager::getInstance().setPwmBlink(PwmManager::EAppPwm_Indication, kIdentifyFinishOnRateMs, kIdentifyFinishOffRateMs);
        break;
    case Clusters::Identify::EffectIdentifierEnum::kStopEffect:
        ChipLogProgress(Zcl, "Clusters::Identify::EffectIdentifierEnum::kStopEffect");
        PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Indication, false);
        break;
    default:
        ChipLogProgress(Zcl, "No identifier effect");
        return;
    }
}

void AppTaskCommon::StartBleAdvButtonEventHandler(void)
{
    AppEvent event;

    event.Type               = AppEvent::kEventType_Button;
    event.ButtonEvent.Action = kButtonPushEvent;
    event.Handler            = StartBleAdvHandler;
    GetAppTask().PostEvent(&event);
}

void AppTaskCommon::StartBleAdvHandler(AppEvent * aEvent)
{
    LOG_INF("StartBleAdvHandler");

    // Disable manual Matter service BLE advertising after device provisioning.
    if (sIsNetworkProvisioned)
    {
        LOG_INF("Device already commissioned");
        return;
    }

    if (ConnectivityMgr().IsBLEAdvertisingEnabled())
    {
        LOG_INF("BLE adv already enabled");
        return;
    }

#if defined(CONFIG_PM) &&                                                                                                          \
    (defined(CONFIG_SOC_SERIES_RISCV_TELINK_B9X_RETENTION) || defined(CONFIG_SOC_SERIES_RISCV_TELINK_TLX_RETENTION))
    if (pm_has_deep_sleep_retention_occurred())
    {
        ChipLogError(DeviceLayer, "BLE state in non-retention RAM corrupted after deep sleep retention. Rebooting...");
        sys_reboot(SYS_REBOOT_WARM);
    }
#endif

    if (chip::Server::GetInstance().GetCommissioningWindowManager().OpenBasicCommissioningWindow() != CHIP_NO_ERROR)
    {
        LOG_ERR("OpenBasicCommissioningWindow fail");
    }
}

void AppTaskCommon::FactoryResetButtonEventHandler(void)
{
    AppEvent event;

    event.Type               = AppEvent::kEventType_Button;
    event.ButtonEvent.Action = kButtonPushEvent;
    event.Handler            = FactoryResetHandler;
    GetAppTask().PostEvent(&event);
}

void AppTaskCommon::FactoryResetHandler(AppEvent * aEvent)
{
    if (sFactoryResetCntr == 0)
    {
        k_timer_start(&sFactoryResetTimer, K_MSEC(kFactoryResetCalcTimeout), K_NO_WAIT);
    }

    sFactoryResetCntr++;
    LOG_INF("Factory Reset TC: %d/%d", sFactoryResetCntr, kFactoryResetTriggerCntr);

    if (sFactoryResetCntr == kFactoryResetTriggerCntr)
    {
        k_timer_stop(&sFactoryResetTimer);
        sFactoryResetCntr = 0;

        // Erase user parameters partition and reset to Zigbee mode upon factory reset
        flash_erase(flash_para_dev, USER_PARTITION_OFFSET, USER_PARTITION_SIZE);
        // Need to erase zb nvs part
        flash_erase(zb_para_dev, ZB_NVS_START_ADR, ZB_NVS_SEC_SIZE);
#if APP_LIGHT_USER_MODE_EN
#if CONFIG_STARTUP_OPTIMIZATE
        // Need to erase cluster para part
        flash_erase(cluster_para_dev, USER_CLUSTER_PARTITION_OFFSET, USER_CLUSTER_PARTITION_SIZE);
#endif /* CONFIG_STARTUP_OPTIMIZATE */
#endif /* APP_LIGHT_USER_MODE_EN */
        printk("Factory reset triggered by button, resetting to Zigbee mode");

        chip::Server::GetInstance().ScheduleFactoryReset();
    }
}

void AppTaskCommon::FactoryResetTimerTimeoutCallback(k_timer * timer)
{
    if (!timer)
    {
        return;
    }

    AppEvent event;
    event.Type    = AppEvent::kEventType_Timer;
    event.Handler = FactoryResetTimerEventHandler;
    GetAppTask().PostEvent(&event);
}

void SwitchBackToZigbee()
{
    uint8_t switch_flag = USER_MATTER_BACK_ZB;
    flash_write(flash_para_dev, USER_PARTITION_OFFSET, &switch_flag, 1);
    sys_reboot(SYS_REBOOT_WARM);
}

void AppTaskCommon::DnssTimerTimeoutCallback(k_timer * timer)
{
    printk("Matter: DnssTimer expired.\r\n");
    /*If initialization of Dnss takes longer than 90 seconds, the device will reboot and revert to Zigbee mode*/
    if (sBoot_zb)
    {
        SwitchBackToZigbee();
    }
}

void AppTaskCommon::FactoryResetTimerEventHandler(AppEvent * aEvent)
{
    if (aEvent->Type != AppEvent::kEventType_Timer)
    {
        return;
    }

    sFactoryResetCntr = 0;
    LOG_INF("Factory Reset TC is cleared");
}

#if CONFIG_TELINK_OTA_BUTTON_TEST
void AppTaskCommon::TestOTAButtonEventHandler(void)
{
    AppEvent event;

    event.Type               = AppEvent::kEventType_Button;
    event.ButtonEvent.Action = kButtonPushEvent;
    event.Handler            = TestOTAHandler;
    GetAppTask().PostEvent(&event);
}

void AppTaskCommon::TestOTAHandler(AppEvent * aEvent)
{
    LOG_INF("TestOTAHandler");

    chip::DeviceLayer::OTAImageProcessorImpl imageProcessor;
    imageProcessor.Apply();
}
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
void AppTaskCommon::StartThreadButtonEventHandler(void)
{
    AppEvent event;

    event.Type               = AppEvent::kEventType_Button;
    event.ButtonEvent.Action = kButtonPushEvent;
    event.Handler            = StartThreadHandler;
    GetAppTask().PostEvent(&event);
}

void AppTaskCommon::StartThreadHandler(AppEvent * aEvent)
{
    LOG_INF("StartThreadHandler");
    if (!sIsNetworkProvisioned)
    {
        LogErrorOnFailure(ThreadStackMgrImpl().SetThreadEnabled(true));
        StartDefaultThreadNetwork();
    }
    else
    {
        LOG_INF("Device already commissioned");
    }
}

#elif CHIP_DEVICE_CONFIG_ENABLE_WIFI
void AppTaskCommon::StartWiFiButtonEventHandler(void)
{
    AppEvent event;

    event.Type               = AppEvent::kEventType_Button;
    event.ButtonEvent.Action = kButtonPushEvent;
    event.Handler            = StartWiFiHandler;
    GetAppTask().PostEvent(&event);
}

void AppTaskCommon::StartWiFiHandler(AppEvent * aEvent)
{
    LOG_INF("StartWiFiHandler");

    if (!strlen(CONFIG_DEFAULT_WIFI_SSID) || !strlen(CONFIG_DEFAULT_WIFI_PASSWORD))
    {
        LOG_ERR("default WiFi SSID/Password are not set");
    }

    if (!sIsNetworkProvisioned)
    {
        net_if_up(InetUtils::GetWiFiInterface());
        NetworkCommissioning::TelinkWiFiDriver().StartDefaultWiFiNetwork();
    }
    else
    {
        LOG_INF("Device already commissioned");
    }
}
#endif

void AppTaskCommon::ExampleActionButtonEventHandler(void)
{
    AppEvent event;

    if (!GetAppTask().ExampleActionEventHandler)
    {
        return;
    }

    event.Type               = AppEvent::kEventType_Button;
    event.ButtonEvent.Action = kButtonPushEvent;
    event.Handler            = GetAppTask().ExampleActionEventHandler;
    GetAppTask().PostEvent(&event);
}

void AppTaskCommon::SetExampleButtonCallbacks(EventHandler aAction_CB)
{
    ExampleActionEventHandler = aAction_CB;
}

#ifdef CONFIG_TFLM_FEATURE
void AppTaskCommon::TriggerMicroSpeechCallback()
{
    AppEvent event;
    event.Type    = AppEvent::kEventType_Timer;
    event.Handler = TriggerMicroSpeechEventHandler;
    GetAppTask().PostEvent(&event);
}

void AppTaskCommon::TriggerMicroSpeechEventHandler(AppEvent * aEvent)
{
    LOG_INF("**************TriggerMicroSpeechEventHandler**************");
    AppTask::MicroSpeechProcessStart();
}
#endif

void AppTaskCommon::OtaEventsHandler(const ChipDeviceEvent * event)
{
    switch (event->OtaStateChanged.newState)
    {
    case DeviceLayer::kOtaDownloadInProgress:
        ChipLogProgress(DeviceLayer, "OTA image download in progress\n");
        break;
    case DeviceLayer::kOtaDownloadComplete:
        ChipLogProgress(DeviceLayer, "OTA image download complete\n");
        break;
    case DeviceLayer::kOtaDownloadFailed:
        ChipLogProgress(DeviceLayer, "OTA image download failed\n");
        break;
    case DeviceLayer::kOtaDownloadAborted:
        ChipLogProgress(DeviceLayer, "OTA image download aborted\n");
        break;
    case DeviceLayer::kOtaApplyInProgress:
        ChipLogProgress(DeviceLayer, "OTA image apply in progress\n");
        break;
    case DeviceLayer::kOtaApplyComplete:
        ChipLogProgress(DeviceLayer, "OTA image apply complete\n");
        AppTaskCommon::OtaSetAnaFlag(); // set flag when ota apply complete.
        break;
    case DeviceLayer::kOtaApplyFailed:
        ChipLogProgress(DeviceLayer, "OTA image apply failed\n");
        break;
    default:
        break;
    }
}

k_timer KOtaQueryImageTimer;
constexpr int KOtaQueryImageTimeout = 120000; // for init will cost for about 120s
void KOtaQueryImageTimerTimeoutCallback(k_timer * timer)
{
    LOG_INF("=======proc KOtaQueryImageTimerTimeoutCallback\n");
    InitBasicOTARequestor();
    chip::OTARequestorInterface * requestor = chip::GetRequestorInstance();
    if (chip::Server::GetInstance().GetFabricTable().FabricCount() != 0)
    {
        // Schedule a query. At the end of this query/update process the Default Provider timer is started
        chip::DeviceLayer::SystemLayer().ScheduleLambda([requestor] { requestor->TriggerImmediateQuery(); });
        // GetRequestorInstance()->TriggerImmediateQuery();
    }
}

int KOtaQueryImageTimer_proc(void)
{
    k_timer_init(&KOtaQueryImageTimer, &KOtaQueryImageTimerTimeoutCallback, nullptr);
    k_timer_start(&KOtaQueryImageTimer, K_MSEC(KOtaQueryImageTimeout), K_NO_WAIT);
    LOG_INF("=======KOtaQueryImageTimer start\n");

    return 1;
}

void AppTaskCommon::ChipEventHandler(const ChipDeviceEvent * event, intptr_t /* arg */)
{
    switch (event->Type)
    {
    case DeviceEventType::kCHIPoBLEAdvertisingChange:
        sHaveBLEConnections = ConnectivityMgr().NumBLEConnections() != 0;
        UpdateStatusLED();
        if (event->CHIPoBLEAdvertisingChange.Result == kActivity_Started)
        {
#if defined(CONFIG_PM) && !defined(CONFIG_CHIP_ENABLE_PM_DURING_BLE)
            pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
#endif

#ifdef CONFIG_CHIP_NFC_ONBOARDING_PAYLOAD
            if (event->CHIPoBLEAdvertisingChange.Result == kActivity_Started)
            {
                if (NFCOnboardingPayloadMgr().IsTagEmulationStarted())
                {
                    LOG_INF("NFC Tag emulation is already started");
                }
                else
                {
                    ShareQRCodeOverNFC(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));
                }
            }
#endif
        }
#ifdef CONFIG_CHIP_NFC_ONBOARDING_PAYLOAD
        else if (event->CHIPoBLEAdvertisingChange.Result == kActivity_Stopped)
        {
            NFCOnboardingPayloadMgr().StopTagEmulation();
        }
#endif
        break;
    case DeviceEventType::kCHIPoBLEConnectionClosed:
#if CHIP_DEVICE_CONFIG_SUPPORTS_CONCURRENT_CONNECTION
        if (chip::Server::GetInstance().GetFailSafeContext().IsFailSafeArmed())
#else
        if (ConnectivityMgr().GetBleLayer()->IsInitialized())
#endif
        {
            // Unexpected BLE disconnect during commissioning
            ChipLogDetail(DeviceLayer, "BLE disconnected during commissioning");
            chip::Server::GetInstance().GetFailSafeContext().ForceFailSafeTimerExpiry();
        }
        else
        {
            // Expected BLE disconnect, e.g. after commissioning is complete
            bt_disable();
#if defined(CONFIG_PM) && !defined(CONFIG_CHIP_ENABLE_PM_DURING_BLE)
            pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD && !CHIP_DEVICE_CONFIG_SUPPORTS_CONCURRENT_CONNECTION
            ChipLogProgress(DeviceLayer, "Switch to Thread");
            LogErrorOnFailure(ThreadStackMgrImpl().SetThreadEnabled(true));

            ChipDeviceEvent opEvent;
            opEvent.Type     = DeviceEventType::kOperationalNetworkStarted;
            CHIP_ERROR error = PlatformMgr().PostEvent(&opEvent);
            if (error != CHIP_NO_ERROR)
            {
                ChipLogError(DeviceLayer, "PostEvent err: %" CHIP_ERROR_FORMAT, error.Format());
            }
#endif
        }
        break;

    case DeviceEventType::kCommissioningComplete: {
        unsigned char val = USER_MATTER_PAIR_VAL;

/* just a demo to show how to change the cluster after commission , only in the zb switch and touchlink is paired*/
#if 0
        if(user_para.val == USER_ZB_SW_VAL && user_para.on_net ){
            Protocols::InteractionModel::Status status;
            /* Switch from the touch link, need to restore previous values */
            status = Clusters::OnOff::Attributes::OnOff::Set(kExampleEndpointId, light_para.onoff);
            if (status != Protocols::InteractionModel::Status::Success)
            {
                LOG_ERR("Update OnOff fail: %x", to_underlying(status));
            }
            status = Clusters::LevelControl::Attributes::CurrentLevel::Set(kExampleEndpointId, light_para.level);
            {
                LOG_ERR("Update brightness fail: %x", to_underlying(status));
            }
        }
#endif

        /*clear zigbee switch flag*/
        sBoot_zb = 0;
        /*write commission suc flag*/
        flash_erase(flash_para_dev, USER_PARTITION_OFFSET, USER_PARTITION_SIZE);
        flash_write(flash_para_dev, USER_PARTITION_OFFSET, &val, 1);
#if APP_LIGHT_USER_MODE_EN
#if CONFIG_STARTUP_OPTIMIZATE
        cluster_startup_para cluster_para;
        cluster_para.onoff = light_para.onoff;
        cluster_para.level = light_para.level;
        if (store_cluster_para(&cluster_para) != 0)
        {
            printk("[ChipEventHandler] Fail store startup cluster para\n");
        }
#endif /* CONFIG_STARTUP_OPTIMIZATE */
#endif /* APP_LIGHT_USER_MODE_EN */
        printk("Commissioning complete, set Matter commissionined flag");
    }
    break;
    case DeviceEventType::kFailSafeTimerExpired: {
        /* Erase and reset to Zigbee mode if commissioning fails */
        if (sBoot_zb)
        {
            printk("FailSafeTimer expired, Matter commissioning failed, rebooting to Zigbee mode.\r\n");
            SwitchBackToZigbee();
        }
        else
        {
            printk("FailSafeTimer expired, Matter commissioning failed.\r\n");
        }
    }
    break;

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    case DeviceEventType::kDnssdInitialized:
#if CONFIG_CHIP_OTA_REQUESTOR
        InitBasicOTARequestor();
        {
            // metadata is only 1(debug firmware) and 2(develop firmware) and null(product firmware).
            // other values are illegal.
            static uint8_t metadata                 = MATTER_FW_TYPE;
            chip::OTARequestorInterface * requestor = chip::GetRequestorInstance();
            if ((metadata == FW_TYPE_DEBUG) || (metadata == FW_TYPE_DEVELOP))
            {
                printk("set metadata start");
                requestor->SetMetadataForProvider(chip::ByteSpan(&metadata, 1));
                printk("set metadata end");
            }
            else
            {
                // metadata is is null(product firmare) as default.
            }
            KOtaQueryImageTimer_proc();
        }
        if (GetRequestorInstance()->GetCurrentUpdateState() == Clusters::OtaSoftwareUpdateRequestor::OTAUpdateStateEnum::kIdle)
        {
#endif
#ifdef CONFIG_BOOTLOADER_MCUBOOT
            OtaConfirmNewImage();
#endif /* CONFIG_BOOTLOADER_MCUBOOT */
#if CONFIG_CHIP_OTA_REQUESTOR
        }
#endif
        if (sBoot_zb)
        {
            k_timer_stop(&sDnssTimer);
            printk("Dnss Timer stopped, Matter commissioning kDnssdInitialized.\r\n");
        }
        break;
    case DeviceEventType::kThreadStateChange:
        sIsNetworkProvisioned = ConnectivityMgr().IsThreadProvisioned();
        sIsNetworkEnabled     = ConnectivityMgr().IsThreadEnabled();
        sIsNetworkAttached    = ConnectivityMgr().IsThreadAttached();
#ifdef CONFIG_TFLM_FEATURE
        if (sIsNetworkProvisioned && sIsNetworkAttached)
        {
            if (GetAppTask().GetThreadStateChangedEventCapturedFlag() == false)
            {
                LOG_INF("**************TriggerMicroSpeechCallback invoked**************");
                GetAppTask().SetThreadStateChangedEventCapturedFlag();
                AppTaskCommon::TriggerMicroSpeechCallback();
            }
            else
            {
                LOG_INF("**************TriggerMicroSpeechCallback skipped**************");
            }
        }
#endif

#elif CHIP_DEVICE_CONFIG_ENABLE_WIFI
    case DeviceEventType::kWiFiConnectivityChange:
        sIsNetworkProvisioned = ConnectivityMgr().IsWiFiStationProvisioned();
        sIsNetworkEnabled     = ConnectivityMgr().IsWiFiStationEnabled();
        sIsNetworkAttached    = ConnectivityMgr().IsWiFiStationConnected();
#if CHIP_DEVICE_CONFIG_SUPPORTS_CONCURRENT_CONNECTION
        if (sIsNetworkProvisioned && (ConnectivityMgr().NumBLEConnections() == 0))
        {
            /* Disable BLE to ability to enter deep sleep mode once Wi-Fi is provisioned
            and there are no active BLE connections (BLE is only needed for commissioning) */
            bt_disable();
        }
#endif
#if CONFIG_CHIP_OTA_REQUESTOR
        if (event->WiFiConnectivityChange.Result == kConnectivity_Established)
        {
            InitBasicOTARequestor();
        }
#endif
#endif /* CHIP_DEVICE_CONFIG_ENABLE_THREAD */
#if CONFIG_CHIP_ENABLE_APPLICATION_STATUS_LED
        UpdateStatusLED();
#endif
        break;
    case DeviceEventType::kOtaStateChanged:
        AppTaskCommon::OtaEventsHandler(event);
        break;
    default:
        break;
    }
}

void AppTaskCommon::PostEvent(AppEvent * aEvent)
{
    if (!aEvent)
        return;
    if (k_msgq_put(&sAppEventQueue, aEvent, K_NO_WAIT) != 0)
    {
        LOG_INF("PostEvent fail");
    }
}

void AppTaskCommon::DispatchEvent(AppEvent * aEvent)
{
    if (!aEvent)
        return;
    if (aEvent->Handler)
    {
        aEvent->Handler(aEvent);
    }
    else
    {
        LOG_INF("Dropping event without handler");
    }
}

void AppTaskCommon::GetEvent(AppEvent * aEvent)
{
    k_msgq_get(&sAppEventQueue, aEvent, K_FOREVER);
}
