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

#pragma once
#include <analog.h>
#include "AppConfig.h"
#include "AppEventCommon.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <platform/CHIPDeviceLayer.h>

#if CONFIG_CHIP_FACTORY_DATA
#include <platform/telink/FactoryDataProvider.h>
#endif

#ifdef CONFIG_CHIP_PW_RPC
#include "Rpc.h"
#endif

#include <credentials/examples/DeviceAttestationCredsExample.h>

#include <cstdint>
#include <zephyr/fs/nvs.h>
#include <zephyr/settings/settings.h>

using namespace ::chip;
using namespace ::chip::app;
using namespace ::chip::Credentials;
using namespace ::chip::DeviceLayer;

namespace {
inline constexpr EndpointId kExampleEndpointId = 1;
inline constexpr uint8_t kDefaultMinLevel      = 0;
inline constexpr uint8_t kDefaultMaxLevel      = 254;
inline constexpr uint8_t kButtonPushEvent      = 1;
inline constexpr uint8_t kButtonReleaseEvent   = 0;
} // namespace

class LedManager;
class PwmManager;
class ButtonManager;

struct Identify;

class AppTaskCommon
{
public:
#ifdef CONFIG_CHIP_ENABLE_POWER_ON_FACTORY_RESET
    void PowerOnFactoryReset(void);
#endif /* CONFIG_CHIP_ENABLE_POWER_ON_FACTORY_RESET */
    CHIP_ERROR StartApp();
    void PostEvent(AppEvent * event);

    static bool sIsCommissioningFailed;
    static bool IsCommissioningFailed(void) { return sIsCommissioningFailed; }

#ifdef CONFIG_TFLM_FEATURE
    void SetThreadStateChangedEventCapturedFlag(void) { mThreadStateChangedEventCaptured = true; }

    bool GetThreadStateChangedEventCapturedFlag(void) { return mThreadStateChangedEventCaptured; }
#endif

    static void IdentifyEffectHandler(Clusters::Identify::EffectIdentifierEnum aEffect);
    static void IdentifyStartHandler(Identify *);
    static void IdentifyStopHandler(Identify *);

#ifdef CONFIG_CHIP_PW_RPC
    enum ButtonId_t
    {
        kButtonId_ExampleAction = 1,
        kButtonId_FactoryReset,
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
        kButtonId_StartThread,
#elif CHIP_DEVICE_CONFIG_ENABLE_WIFI
        kButtonId_StartWiFi,
#endif
        kButtonId_StartBleAdv
    } ButtonId;
#endif

#ifdef CONFIG_TFLM_FEATURE
    static void TriggerMicroSpeechCallback();
    static void TriggerMicroSpeechEventHandler(AppEvent * aEvent);
#endif

protected:
    CHIP_ERROR InitCommonParts(void);
    void PrintFirmwareInfo(void);

    void DispatchEvent(AppEvent * event);
    void GetEvent(AppEvent * aEvent);

    void InitLeds();
    virtual void LinkLeds(LedManager & ledManager);
    void InitPwms();
    virtual void LinkPwms(PwmManager & pwmManager);
    void InitButtons(void);
    virtual void LinkButtons(ButtonManager & buttonManager);
    static void DnssTimerTimeoutCallback(k_timer * timer);
    static void FactoryResetTimerTimeoutCallback(k_timer * timer);
    static void FactoryResetTimerEventHandler(AppEvent * aEvent);
    static void FactoryResetButtonEventHandler(void);
    static void FactoryResetHandler(AppEvent * aEvent);

    static void StartBleAdvButtonEventHandler(void);
    static void StartBleAdvHandler(AppEvent * aEvent);

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    static void StartThreadButtonEventHandler(void);
    static void StartThreadHandler(AppEvent * aEvent);
#elif CHIP_DEVICE_CONFIG_ENABLE_WIFI
    static void StartWiFiButtonEventHandler(void);
    static void StartWiFiHandler(AppEvent * aEvent);
#endif

#if CONFIG_TELINK_OTA_BUTTON_TEST
    static void TestOTAButtonEventHandler(void);
    static void TestOTAHandler(AppEvent * aEvent);
#endif

    static void ExampleActionButtonEventHandler(void);

    void SetExampleButtonCallbacks(EventHandler aAction_CB);
    EventHandler ExampleActionEventHandler;

    static void ChipEventHandler(const chip::DeviceLayer::ChipDeviceEvent * event, intptr_t arg);

    static void UpdateStatusLED(void);

    static void OtaEventsHandler(const ChipDeviceEvent * event);
    static void OtaSetAnaFlag(void);
    static bool OtaGetAnaFlag(void);

#if CONFIG_CHIP_FACTORY_DATA
    chip::DeviceLayer::FactoryDataProvider<chip::DeviceLayer::ExternalFlashFactoryData> mFactoryDataProvider;
#endif

#ifdef CONFIG_CHIP_PW_RPC
    friend class chip::rpc::TelinkButton;
    static void ButtonEventHandler(ButtonId_t btnId, bool btnPressed);
#endif

#ifdef CONFIG_TFLM_FEATURE
    bool mThreadStateChangedEventCaptured;
#endif
};


#define USER_MATTER_PAIR_VAL    0x55  // jump to matter
#define USER_INIT_VAL           0xff  // init state or others will go into zb 
#define USER_ZB_SW_VAL          0xaa  // jump to matter,use XIP
#define USER_MATTER_BACK_ZB     0xa0  // only commisiion fail will back to zb 
#define USER_PARA_MAC_OFFSET    0x100

#define USER_PARTITION user_para_partition
#define USER_PARTITION_DEVICE FIXED_PARTITION_DEVICE(USER_PARTITION)
#define USER_PARTITION_OFFSET FIXED_PARTITION_OFFSET(USER_PARTITION)
#define USER_PARTITION_SIZE FIXED_PARTITION_SIZE(USER_PARTITION)

#define ZB_NVS_PARTITION zigbee_partition
#define ZB_NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(ZB_NVS_PARTITION)
#define ZB_NVS_START_ADR FIXED_PARTITION_OFFSET(ZB_NVS_PARTITION)
#define ZB_NVS_SEC_SIZE FIXED_PARTITION_SIZE(ZB_NVS_PARTITION)

typedef struct{
    uint8_t val ;
    uint8_t on_net;
}user_para_t;

typedef struct{
    uint8_t     onoff;
    uint8_t     level;
    uint16_t    color_temp_mireds;
    uint16_t    currentx;
    uint16_t    currenty;
    uint16_t    enhanced_current_hue;
    uint16_t    onoff_transition;
    uint8_t     cur_hue;
    uint8_t     cur_saturation;
    uint8_t     color_mode;
}light_para_t;

extern user_para_t user_para;
extern light_para_t  light_para;
extern unsigned char para_lightness;