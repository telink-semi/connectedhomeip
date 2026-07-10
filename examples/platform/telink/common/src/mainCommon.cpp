/*
 *
 *    Copyright (c) 2021-2026 Project CHIP Authors
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

#include "AppTask.h"

#include <lib/support/CHIPMem.h>
#include <platform/CHIPDeviceLayer.h>

#ifndef CONFIG_CHIP_TELINK_ALL_DEVICES_APP
#include <app/clusters/network-commissioning/network-commissioning.h>
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
#include <platform/telink/wifi/TelinkWiFiDriver.h>
#endif

#include <zephyr/kernel.h>

#if CHIP_ENABLE_OPENTHREAD
#include <platform/OpenThread/GenericNetworkCommissioningThreadDriver.h>
#endif

#ifdef CONFIG_USB_DEVICE_STACK
#include <zephyr/usb/usb_device.h>
#endif /* CONFIG_USB_DEVICE_STACK */

#ifdef CONFIG_CHIP_PW_RPC
#include "Rpc.h"
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD && !CHIP_DEVICE_CONFIG_SUPPORTS_CONCURRENT_CONNECTION
K_SEM_DEFINE(gThreadPrescanDoneSem, 0, 1);

class InitScanCallback : public DeviceLayer::NetworkCommissioning::ThreadDriver::ScanCallback
{
public:
    void OnFinished(NetworkCommissioning::Status err, CharSpan debugText,
                    NetworkCommissioning::ThreadScanResponseIterator * networks) override
    {
        k_sem_give(&gThreadPrescanDoneSem);
    }
};
#endif

LOG_MODULE_REGISTER(app, CONFIG_CHIP_APP_LOG_LEVEL);

using namespace ::chip;
using namespace ::chip::Inet;
using namespace ::chip::DeviceLayer;

#ifndef CONFIG_CHIP_TELINK_ALL_DEVICES_APP
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
app::Clusters::NetworkCommissioning::Instance sWiFiCommissioningInstance(0, &(NetworkCommissioning::TelinkWiFiDriver::Instance()));
#endif

#if CHIP_ENABLE_OPENTHREAD
app::Clusters::NetworkCommissioning::InstanceAndDriver<NetworkCommissioning::GenericThreadDriver>
    sThreadNetworkDriver(0 /*endpointId*/);
#endif
#endif // CONFIG_CHIP_TELINK_ALL_DEVICES_APP

#ifdef CONFIG_CHIP_ENABLE_POWER_ON_FACTORY_RESET
static constexpr uint32_t kFactoryResetOnBootMaxCnt       = 5;
static constexpr char kFactoryResetOnBootStoreKey[]       = "TelinkFactoryResetOnBootCnt";
static constexpr uint32_t kFactoryResetUsualBootTimeoutMs = 5000;

static k_timer FactoryResetUsualBootTimer;

static void FactoryResetUsualBoot(struct k_timer * dummy)
{
    (void) dummy;
    LogErrorOnFailure(chip::DeviceLayer::PersistedStorage::KeyValueStoreMgr().Delete(kFactoryResetOnBootStoreKey));
    LOG_INF("Schedule factory counter deleted");
}

static void FactoryResetOnBoot(void)
{
    uint32_t FactoryResetOnBootCnt;
    CHIP_ERROR FactoryResetOnBootErr = chip::DeviceLayer::PersistedStorage::KeyValueStoreMgr().Get(
        kFactoryResetOnBootStoreKey, &FactoryResetOnBootCnt, sizeof(FactoryResetOnBootCnt));

    if (FactoryResetOnBootErr == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND)
    {
        FactoryResetOnBootCnt = 1;
        if (chip::DeviceLayer::PersistedStorage::KeyValueStoreMgr().Put(kFactoryResetOnBootStoreKey, &FactoryResetOnBootCnt,
                                                                        sizeof(FactoryResetOnBootCnt)) != CHIP_NO_ERROR)
        {
            LOG_ERR("FactoryResetOnBootCnt write fail");
        }
        else
        {
            LOG_INF("Schedule factory counter %u", FactoryResetOnBootCnt);
        }
    }
    else if (FactoryResetOnBootErr == CHIP_NO_ERROR)
    {
        FactoryResetOnBootCnt++;
        if (chip::DeviceLayer::PersistedStorage::KeyValueStoreMgr().Put(kFactoryResetOnBootStoreKey, &FactoryResetOnBootCnt,
                                                                        sizeof(FactoryResetOnBootCnt)) != CHIP_NO_ERROR)
        {
            LOG_ERR("FactoryResetOnBootCnt write fail");
        }
        else
        {
            LOG_INF("Schedule factory counter %u", FactoryResetOnBootCnt);
            if (FactoryResetOnBootCnt >= kFactoryResetOnBootMaxCnt)
            {
                GetAppTask().PowerOnFactoryReset();
            }
        }
    }
    else
    {
        LOG_ERR("FactoryResetOnBootCnt read fail");
    }
    k_timer_init(&FactoryResetUsualBootTimer, FactoryResetUsualBoot, nullptr);
    k_timer_start(&FactoryResetUsualBootTimer, K_MSEC(kFactoryResetUsualBootTimeoutMs), K_NO_WAIT);
}
#endif /* CONFIG_CHIP_ENABLE_POWER_ON_FACTORY_RESET */

#define MATTER_NVS_DEMO_EN 0
#if MATTER_NVS_DEMO_EN
void matter_nvs_demo(void)
{
    static constexpr char kFactoryResetOnBootStoreKey[] = "TelinkFactoryResetOnBootCnt";
    uint32_t test_flag                                  = 0x55;

    if (chip::DeviceLayer::PersistedStorage::KeyValueStoreMgr().Put(kFactoryResetOnBootStoreKey, &test_flag, sizeof(test_flag)) !=
        CHIP_NO_ERROR)
    {
        printk("FactoryResetOnBootCnt write fail\n");
    }

    test_flag = 0xaa;
    CHIP_ERROR FactoryResetOnBootErr =
        chip::DeviceLayer::PersistedStorage::KeyValueStoreMgr().Get(kFactoryResetOnBootStoreKey, &test_flag, sizeof(test_flag));
    if (FactoryResetOnBootErr != CHIP_NO_ERROR)
    {
        printk("FactoryResetOnBootCnt get fail\n");
    }
    printk("NVS read value is %x \n", test_flag);
    (void) chip::DeviceLayer::PersistedStorage::KeyValueStoreMgr().Delete(kFactoryResetOnBootStoreKey);
    test_flag = 0xbb;
    FactoryResetOnBootErr =
        chip::DeviceLayer::PersistedStorage::KeyValueStoreMgr().Get(kFactoryResetOnBootStoreKey, &test_flag, sizeof(test_flag));
    if (FactoryResetOnBootErr != CHIP_NO_ERROR)
    {
        printk("FactoryResetOnBootCnt delete after read fail\n");
    }
}
#endif /* MATTER_NVS_DEMO_EN */

#if APP_LIGHT_USER_MODE_EN
#if CONFIG_STARTUP_OPTIMIZATE
#include "AppTaskCommon.h"

#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>

#include <PWMManager.h>
#include <pwm.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr_pwm_pool.h>

struct k_timer PwmChangeTimer;
static PWM_POOL_DEFINE(pwm_pool);
struct pwm_pool_data * pwm_data             = &pwm_pool;
static const struct device * flash_para_dev = USER_PARTITION_DEVICE;

#define ENUM_BLUE (PwmManager::EAppPwm_Blue)
/* EAppPwm_Blue enum is 4, corresponds to channel 2 in dts */
#define PWM_CHANNEL_BLUE ((uint32_t) ENUM_BLUE - 2)
/* pwm channel 0 is BIT(8) in driver */
#define PWM_CHANNEL_TO_BIT(CHANNEL) ((CHANNEL == 0) ? FLD_PWM0_EN : BIT(CHANNEL))
#define BIT_PWM_CHANNEL_BLUE PWM_CHANNEL_TO_BIT(PWM_CHANNEL_BLUE)

#define PWM_CHANGE_TOTAL_TIME_MS 400
#define PWM_CHANGE_PRE_STEP_MS 8
#define PWM_STEP_CNT_MAX (PWM_CHANGE_TOTAL_TIME_MS / PWM_CHANGE_PRE_STEP_MS)
#define PWM_PULSE_CYCLE(period, level, cnt) ((period / (255 * PWM_STEP_CNT_MAX)) * (level) * (cnt))

static uint32_t cnt          = 1;
static uint8_t cur_level     = 0;
static uint32_t timer_period = PWM_CHANGE_PRE_STEP_MS;

static void init_startup_para(void)
{
    cluster_startup_para light_cluster_para;
    if (read_cluster_para(&light_cluster_para) != 0)
    {
    }

    if (light_cluster_para.onoff == 1)
    {
        cur_level = light_cluster_para.level;
    }
    else
    { // OFF || ERROR
        cur_level    = 0;
        timer_period = 0;
    }
}

static void PwmSetTimeoutCallback(struct k_timer * timer)
{
    if (!timer)
    {
        return;
    }

    pwm_set_dt(&pwm_data->out[ENUM_BLUE], pwm_data->out[ENUM_BLUE].period,
               PWM_PULSE_CYCLE(pwm_data->out[ENUM_BLUE].period, cur_level, cnt));
    pwm_set_start((pwm_en_e) (BIT_PWM_CHANNEL_BLUE));
    if (cnt >= PWM_STEP_CNT_MAX)
    {
        k_timer_stop(timer);
    }
    cnt++;
}
#endif /* CONFIG_STARTUP_OPTIMIZATE */
#endif /* APP_LIGHT_USER_MODE_EN */

void early_proc_cluster(void)
{
#if APP_LIGHT_USER_MODE_EN
#if CONFIG_STARTUP_OPTIMIZATE
    unsigned char val;
    flash_read(flash_para_dev, USER_PARTITION_OFFSET, &val, 1);
    if (val == USER_MATTER_PAIR_VAL)
    {
        init_cluster_partition();
        init_startup_para();
        if (timer_period != 0)
        {
            k_timer_init(&PwmChangeTimer, &PwmSetTimeoutCallback, nullptr);
            k_timer_start(&PwmChangeTimer, K_MSEC(timer_period), K_MSEC(timer_period));
        }
    }
#endif /* CONFIG_STARTUP_OPTIMIZATE */
#endif /* APP_LIGHT_USER_MODE_EN */
}

typedef void (*p_early_proc)(void);
p_early_proc early_proc_cluster_f = early_proc_cluster;

int main(void)
{
#if CONFIG_WATCHDOG_AUTO
/* Not modify by user */
#define MATTER_ANALOG_REG_WDT_ADR (0x3c)
#define MATTER_WDT_BY_CONTROL BIT(0)
    if (!(analog_read(MATTER_ANALOG_REG_WDT_ADR) & MATTER_WDT_BY_CONTROL))
    {
        printk("watchdog startup...\r\n");
    }
#endif /* CONFIG_WATCHDOG_AUTO */

#if APP_LIGHT_USER_MODE_EN
#if CONFIG_STARTUP_OPTIMIZATE
    printk("[init_startup_para] cur_level:%d, timer_period:%d\n", cur_level, timer_period);
#endif /* CONFIG_STARTUP_OPTIMIZATE */
#endif /* APP_LIGHT_USER_MODE_EN */

#if defined(CONFIG_USB_DEVICE_STACK) && !defined(CONFIG_CHIP_PW_RPC)
    usb_enable(NULL);
#endif /* CONFIG_USB_DEVICE_STACK */

    CHIP_ERROR err = CHIP_NO_ERROR;

#ifdef CONFIG_CHIP_PW_RPC
    rpc::Init();
#endif

    err = chip::Platform::MemoryInit();
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("MemoryInit fail");
        goto exit;
    }

    err = PlatformMgr().InitChipStack();
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("InitChipStack fail");
        goto exit;
    }

    err = PlatformMgr().StartEventLoopTask();
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("StartEventLoopTask fail");
        goto exit;
    }

#ifdef CONFIG_CHIP_ENABLE_POWER_ON_FACTORY_RESET
    FactoryResetOnBoot();
#endif /* CONFIG_CHIP_ENABLE_POWER_ON_FACTORY_RESET */

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    err = ThreadStackMgr().InitThreadStack();
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("InitThreadStack fail");
        goto exit;
    }

#if defined(CONFIG_CHIP_THREAD_DEVICE_ROLE_ROUTER)
    err = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_Router);
#elif defined(CONFIG_CHIP_THREAD_DEVICE_ROLE_END_DEVICE)
    err = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_MinimalEndDevice);
#elif defined(CONFIG_CHIP_THREAD_DEVICE_ROLE_SLEEPY_END_DEVICE)
    err = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_SleepyEndDevice);
#else
#error THREAD_DEVICE_ROLE not selected
#endif
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("SetThreadDeviceType fail");
        goto exit;
    }

#ifndef CONFIG_CHIP_TELINK_ALL_DEVICES_APP
    LogErrorOnFailure(sThreadNetworkDriver.Init());
#endif

#if !CHIP_DEVICE_CONFIG_SUPPORTS_CONCURRENT_CONNECTION
    if (!chip::DeviceLayer::ConnectivityMgr().IsThreadProvisioned())
    {
        static InitScanCallback sInitScanCallback;
        LogErrorOnFailure(chip::DeviceLayer::ThreadStackMgrImpl().StartThreadScan(&sInitScanCallback));
        k_sem_take(&gThreadPrescanDoneSem, K_SECONDS(15));
    }
#endif

#elif CHIP_DEVICE_CONFIG_ENABLE_WIFI
#ifndef CONFIG_CHIP_TELINK_ALL_DEVICES_APP
    LogErrorOnFailure(sWiFiCommissioningInstance.Init());
#endif
#else
    err = CHIP_ERROR_INTERNAL;
    goto exit;
#endif /* CHIP_DEVICE_CONFIG_ENABLE_THREAD */

#if MATTER_NVS_DEMO_EN
    matter_nvs_demo();
#endif /* MATTER_NVS_DEMO_EN */

    err = GetAppTask().StartApp();

exit:
    LOG_ERR("Exit err %d", err.Format());
    return (err == CHIP_NO_ERROR) ? EXIT_SUCCESS : EXIT_FAILURE;
}
