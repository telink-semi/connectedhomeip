/*
 *
 *    Copyright (c) 2022-2024 Project CHIP Authors
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
#include "AppConfig.h"
#include <app/server/Server.h>

#include "ColorFormat.h"
#include "LEDManager.h"
#include "PWMManager.h"

#ifdef CONFIG_TFLM_FEATURE
#include "tflm/audio/app_audio.h"
#include "tflm/audio/app_codec.h"
#endif

#include <app-common/zap-generated/attributes/Accessors.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr_pwm_pool.h>

#include <gpio.h>
#include <i2c.h>
#include <pwm.h>

LOG_MODULE_DECLARE(app, CONFIG_CHIP_APP_LOG_LEVEL);
using namespace chip::app::Clusters;
namespace {
bool sfixture_on;
uint8_t sBrightness;
AppTask::Fixture_Action sColorAction = AppTask::INVALID_ACTION;
XyColor_t sXY;
HsvColor_t sHSV;
CtColor_t sCT;
RgbColor_t sLedRgb;

#ifdef CONFIG_TFLM_FEATURE
k_timer sAudioProcessUpdateTimer;
// Ensure the timer starts only after the commissioning
// or reconnection process is finished.
constexpr uint16_t kInitialAudioProcessUpdateTimerPeriodMs = 15000;
constexpr uint16_t kAudioProcessUpdateTimerPeriodMs        = 500; // 500ms timer period
#endif
} // namespace

AppTask AppTask::sAppTask;

bool AppTask::IsTurnedOn() const
{
    return sfixture_on;
}

#if CONFIG_TOKEN_PERIPHERALS
static PWM_POOL_DEFINE(pwm_pool);

#define MAX_PWM_PINS 6 // pins can configurable

#define PWM_MODE_LIGHT 0
#define I2C_MODE_LIGHT 1

typedef struct
{
    uint8_t id;
    uint32_t pin;
    uint32_t rate; // pwm rate
} pwm_struct_t;

typedef struct
{
    uint32_t mode;                      // zero is pwm mode, one is i2c mode.
    pwm_struct_t pwm_pin[MAX_PWM_PINS]; // if token lost, the pin value will be PA0 and PA2,if any one is zero, will jump.
    uint32_t sda;                       // if token lost, the value will be initial setting.
    uint32_t scl;                       // if token lost, the value will be initial setting.
} hw_token_t;

hw_token_t hw_token = { PWM_MODE_LIGHT,
                        { { 0, GPIO_PA0, 500 },
                          { 1, GPIO_PC2, 600 },
                          { 2, GPIO_PD0, 1000 },
                          { 3, GPIO_PC3, 2000 },
                          { 4, GPIO_PB0, 10000 },
                          { 5, 0, 0 } }, // Sequential Array
                        GPIO_PA0,
                        GPIO_PA1 };

void hw_token_pwm_init(void)
{
    struct pwm_pool_data * pwm_pool_token = &pwm_pool;

    LOG_INF("hw_token_pwm_init: out_len=%zu", pwm_pool_token->out_len);
    for (size_t i = 0; i < pwm_pool_token->out_len; i++)
    {
        const struct pwm_dt_spec * spec = &pwm_pool_token->out[i];
        LOG_INF("  out[%zu]: dev=%s, channel=%u, period=%u, flags=%u",
                i,
                spec->dev ? spec->dev->name : "null",
                spec->channel,
                spec->period,
                spec->flags);
    }

    // set all the pwm part.
    for (size_t i = 0; i < pwm_pool_token->out_len; i++)
    {
        // Sequential Array.
        uint8_t id    = pwm_pool_token->out[i].channel;
        uint32_t pin  = hw_token.pwm_pin[id].pin;
        uint32_t rate = hw_token.pwm_pin[id].rate;

        LOG_INF("hw_token_pwm_init: i=%zu, id=%u, pin=0x%04x, rate=%u", i, id, (uint16_t) pin, rate);

        if (pin == 0 || (id > (PWM5 - PWM0)))
        {
            LOG_INF("  skip i=%zu (pin==0 or id out of range)", i);
            continue;
        }
        pwm_set_pinctrl((gpio_func_pin_e) pin, (gpio_func_e) (PWM0 + id));

        int ret = pwm_set_dt(&pwm_pool_token->out[i], PWM_USEC(1000000 / rate), PWM_USEC(1000000 / rate) / 3);
        LOG_INF("  pwm_set_dt i=%zu, ret=%d", i, ret);
    }
}

void hw_token_i2c_init(int sda, int scl)
{
    /********************
     *
     *  void (*i2c_set_pin_t)(unsigned int sda_pin,unsigned int scl_pin);
     * 	intial:
     *        sdk: GPIO_PA0  scl:GPIO_PA1
     */
    i2c_set_p(sda, scl);
}
#endif /* CONFIG_TOKEN_PERIPHERALS */

/*
 * I2C Demo
 */
#if (APP_LIGHT_MODE == APP_LIGHT_I2C)
void i2c_demo_proc(void)
{
    const uint8_t tx_buf[23] = { 0xc0, 0x63, 0x3f, 0x63, 0x63, 0x63, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00,
                                 0x3f, 0x3f, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x2b, 0x06, 0xbe };
    printk("i2c demo start \n.");
    uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER;
    /* get i2c device */
    int rc;
    const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(ledcontrol_i2c));

    if (dev_i2c.bus == NULL) {
        printk("Error: I2C controller pointer is NULL. Check your DeviceTree and Binding.\n");
    } else {
        printk("I2C Controller Name: %s\n", dev_i2c.bus->name);
        
        if (!device_is_ready(dev_i2c.bus)) {
            /* If not ready, check the internal init result (0 is success) */
            printk("Device is NOT ready! Init error code: %d\n", dev_i2c.bus->state->init_res);
        } else {    
            printk("Device is ready and initialized successfully.\n");
        }
    }

    printk("Device ptr: %p\n", dev_i2c.bus);
    if (dev_i2c.bus) {
        printk("Device init status: %d\n", dev_i2c.bus->state->init_res);
    }

    if (!device_is_ready(dev_i2c.bus))
    {
        printf("Device %s is not ready\n", dev_i2c.bus->name);
        return;
    }
    rc = i2c_configure(dev_i2c.bus, i2c_cfg);
    if (rc != 0)
    {
        printf("Failed to configure i2c device\n");
        return;
    }
    i2c_write(dev_i2c.bus, tx_buf + 1, sizeof(tx_buf) - 1, tx_buf[0]);
    printk("i2c demo stop ,finish transfer\n");
}
#endif /* APP_LIGHT_MODE == APP_LIGHT_I2C */

/*
 * ADC Demo
 * defalut run telink driver adc for user.
 */
#if (APP_LIGHT_MODE == APP_LIGHT_ADC)
#if (APP_TELINK_DRIVERS_ADC)
#include <adc.h>

static k_timer AdcDriverTimer;
static constexpr uint32_t kAdcDriverTimeoutMs = 50;

volatile unsigned short adc_vol_mv_val;

#define ADC_SAMPLE_NUM (8)
unsigned short adc_sample_buffer[ADC_SAMPLE_NUM] __attribute__((aligned(4))) = { 0 };

#define ADC_SAMPLE_FREQ ADC_SAMPLE_FREQ_96K
#define ADC_SAMPLE_NDMA_DELAY_TIME ((1000 / (6 * (2 << (ADC_SAMPLE_FREQ)))) + 1) //delay 2 sample cycle

/**
 * @brief This function serves to sort adc sample code and get average value.
 * @return      adc_code_average    - the average value of adc sample code.
 */
unsigned short adc_sort_and_get_average_code(void)
{
    unsigned short adc_code_average = 0;
    int            i, j;
    unsigned short temp;
    /**** insert Sort and get average value ******/
    for (i = 1; i < ADC_SAMPLE_NUM; i++) {
        if (adc_sample_buffer[i] < adc_sample_buffer[i - 1]) {
            temp                 = adc_sample_buffer[i];
            adc_sample_buffer[i] = adc_sample_buffer[i - 1];
            /**
         * add judgment condition "j>=0" in for loop,
         * otherwise may have array out of bounds.
         * changed by chaofan.20201230.
     */
            for (j = i - 1; j >= 0 && adc_sample_buffer[j] > temp; j--) {
                adc_sample_buffer[j + 1] = adc_sample_buffer[j];
            }
            adc_sample_buffer[j + 1] = temp;
        }
    }

    //get average value from raw data(abandon 1/4 small and 1/4 big data)
    for (i = ADC_SAMPLE_NUM >> 2; i < (ADC_SAMPLE_NUM - (ADC_SAMPLE_NUM >> 2)); i++) {
        adc_code_average += adc_sample_buffer[i] / (ADC_SAMPLE_NUM >> 1);
    }
    return adc_code_average;
}

/**
 * @brief This function serves to get adc sample code by manual and convert to voltage value.
 * @return      adc_vol_mv_average  - the average value of adc voltage value.
 */
unsigned short adc_get_voltage(void)
{
    unsigned short adc_vol_mv_average = 0;
    unsigned short adc_code_average   = 0;
    for (int i = 0; i < ADC_SAMPLE_NUM; i++) {
        /**
     * move the "2 sample cycle" wait operation before adc_get_code(),
     * otherwise may have data lose due to no waiting when adc_power_on.
     * changed by chaofan.20201230.
     */
        k_busy_wait(ADC_SAMPLE_NDMA_DELAY_TIME); // wait at least 2 sample cycle(f = 96K, T = 10.4us)
        adc_sample_buffer[i] = adc_vbat_get_code();
    }
    adc_code_average   = adc_sort_and_get_average_code();
    adc_vol_mv_average = adc_vbat_calculate_voltage(adc_code_average);
    return adc_vol_mv_average;
}

void AdcDriverTimerCb(struct k_timer * dummy)
{
    adc_vol_mv_val = adc_get_voltage();
    printk("%" PRId32, adc_vol_mv_val);
    printk("\r\n");
}

static void telink_driver_adc_start(void)
{
    k_timer_init(&AdcDriverTimer, AdcDriverTimerCb, nullptr);
    k_timer_start(&AdcDriverTimer, K_MSEC(kAdcDriverTimeoutMs), K_MSEC(kAdcDriverTimeoutMs));
}

static void telink_driver_adc_proc(void)
{
    adc_vbat_sample_init();
    adc_vbat_power_on();

    telink_driver_adc_start();
}
#else
void adc_demo_proc(void)
{
/* Data of ADC io-channels specified in devicetree. */
#define DT_SPEC_AND_COMMA(node_id, prop, idx) ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

    static const struct adc_dt_spec adc_channels[] = { DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels, DT_SPEC_AND_COMMA) };

    /* Define ADC data structure. */
    uint16_t buf;
    struct adc_sequence sequence = {
        .buffer = &buf,
        /* buffer size in bytes, not number of samples */
        .buffer_size = sizeof(buf),
    };

    int err = 0;

    /* Configure channels individually prior to sampling. */
    for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++)
    {
        if (!device_is_ready(adc_channels[i].dev))
        {
            printf("ADC controller device %s not ready\n", adc_channels[i].dev->name);
            return;
        }

        err = adc_channel_setup_dt(&adc_channels[i]);
        if (err < 0)
        {
            printf("Could not setup channel #%d (%d)\n", i, err);
            return;
        }
    }

    /* ADC sampling. */
    for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++)
    { // channel default is 0.
        printf("- %s, channel %d: ", adc_channels[i].dev->name, adc_channels[i].channel_id);

        (void) adc_sequence_init_dt(&adc_channels[i], &sequence);

        err = adc_read(adc_channels[i].dev, &sequence);
        if (err < 0)
        {
            printf("Could not read (%d)\n", err);
            continue;
        }

        int32_t val_mv = 0;

        /*
         * If using differential mode, the 16 bit value
         * in the ADC sample buffer should be a signed 2's
         * complement value.
         */
        if (adc_channels[i].channel_cfg.differential)
        { // differential default is 0.
            val_mv = (int32_t) ((int16_t) buf);
        }
        else
        {
            val_mv = (int32_t) buf;
        }
        printf("%" PRId32, val_mv);

        /* conversion to mV may not be supported, skip if not */
        err = adc_raw_to_millivolts_dt(&adc_channels[i], &val_mv);
        if (err < 0)
        {
            printf(" (value in mV not available)\n");
        }
        else
        {
            printf(" = %" PRId32 " mV\n", val_mv);
        }
    }
}
#endif /* APP_TELINK_DRIVERS_ADC */
#endif /* APP_LIGHT_MODE == APP_LIGHT_ADC */

#if (0)
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>

// we need to add token partition in DTS
//=========================================
//--- a/src/platform/telink/tlsr9528a_4m_flash.overlay
//  +++ b/src/platform/telink/tlsr9528a_4m_flash.overlay
//  +       user_token_partition: partition@3a5000 {
//  +       label = "user-token";
//  +           reg = <0x3a5000 0x1000>; //user:store token info, for customer to config.
//  +       };
//  =========================================

#define TOKEN_PARTITION user_token_partition
#define TOKEN_PARTITION_DEVICE FIXED_PARTITION_DEVICE(TOKEN_PARTITION)
#define TOKEN_PARTITION_OFFSET FIXED_PARTITION_OFFSET(TOKEN_PARTITION)
#define TOKEN_PARTITION_SIZE FIXED_PARTITION_SIZE(TOKEN_PARTITION)

#define OFFSET_BASIC_CLUSTER_MODEL_IDENTIFIER 0x00
#define LENGHT_BASIC_CLUSTER_MODEL_IDENTIFIER 32

#define OFFSET_BASIC_CLUSTER_HARDWARE_VER 0x40
#define LENGHT_BASIC_CLUSTER_HARDWARE_VER 1

#define OFFSET_BASIC_CLUSTER_PRODUCT_CODE 0x50
#define LENGHT_BASIC_CLUSTER_PRODUCT_CODE 16

#define OFFSET_READ_COUNT 0x400
#define LENGHT_READ_COUNT 1

const struct device * flash_token_dev = TOKEN_PARTITION_DEVICE;
void token_flash_demo_proc(void)
{
    // read token infor form token bin.
    uint8_t tmp_model_identifier[LENGHT_BASIC_CLUSTER_MODEL_IDENTIFIER];
    flash_read(flash_token_dev, TOKEN_PARTITION_OFFSET + OFFSET_BASIC_CLUSTER_MODEL_IDENTIFIER, tmp_model_identifier,
               LENGHT_BASIC_CLUSTER_MODEL_IDENTIFIER);
    LOG_HEXDUMP_INF(tmp_model_identifier, LENGHT_BASIC_CLUSTER_MODEL_IDENTIFIER, "flash_read:model_identifier:");

    uint8_t tmp_hardware_ver[LENGHT_BASIC_CLUSTER_HARDWARE_VER];
    flash_read(flash_token_dev, TOKEN_PARTITION_OFFSET + OFFSET_BASIC_CLUSTER_HARDWARE_VER, tmp_hardware_ver,
               LENGHT_BASIC_CLUSTER_HARDWARE_VER);
    LOG_HEXDUMP_INF(tmp_hardware_ver, LENGHT_BASIC_CLUSTER_HARDWARE_VER, "flash_read:tmp_hardware_ver:");

    uint8_t tmp_product_code[LENGHT_BASIC_CLUSTER_PRODUCT_CODE];
    flash_read(flash_token_dev, TOKEN_PARTITION_OFFSET + OFFSET_BASIC_CLUSTER_PRODUCT_CODE, tmp_product_code,
               LENGHT_BASIC_CLUSTER_PRODUCT_CODE);
    LOG_HEXDUMP_INF(tmp_product_code, LENGHT_BASIC_CLUSTER_PRODUCT_CODE, "flash_read:tmp_product_code:");

    uint8_t tmp_read_count;
    flash_read(flash_token_dev, TOKEN_PARTITION_OFFSET + OFFSET_READ_COUNT, &tmp_read_count, 1);
    if (tmp_read_count == 0xff)
        tmp_read_count = 0;
    LOG_HEXDUMP_INF(&tmp_read_count, LENGHT_READ_COUNT, "flash_read:tmp_read_count:");

    // erase token infor.
    flash_erase(flash_token_dev, TOKEN_PARTITION_OFFSET, TOKEN_PARTITION_SIZE);

    // write token infor.
    flash_write(flash_token_dev, TOKEN_PARTITION_OFFSET + OFFSET_BASIC_CLUSTER_MODEL_IDENTIFIER, tmp_model_identifier,
                LENGHT_BASIC_CLUSTER_MODEL_IDENTIFIER);
    LOG_HEXDUMP_INF(tmp_model_identifier, LENGHT_BASIC_CLUSTER_MODEL_IDENTIFIER, "flash_write:model_identifier:");

    flash_write(flash_token_dev, TOKEN_PARTITION_OFFSET + OFFSET_BASIC_CLUSTER_HARDWARE_VER, tmp_hardware_ver,
                LENGHT_BASIC_CLUSTER_HARDWARE_VER);
    LOG_HEXDUMP_INF(tmp_hardware_ver, LENGHT_BASIC_CLUSTER_HARDWARE_VER, "flash_write:tmp_hardware_ver:");

    flash_write(flash_token_dev, TOKEN_PARTITION_OFFSET + OFFSET_BASIC_CLUSTER_PRODUCT_CODE, tmp_product_code,
                LENGHT_BASIC_CLUSTER_PRODUCT_CODE);
    LOG_HEXDUMP_INF(tmp_product_code, LENGHT_BASIC_CLUSTER_PRODUCT_CODE, "flash_write:tmp_product_code:");

    tmp_read_count += 1;
    flash_write(flash_token_dev, TOKEN_PARTITION_OFFSET + OFFSET_READ_COUNT, &tmp_read_count, 1);
    if (tmp_read_count == 0xff)
        tmp_read_count = 0;
    LOG_HEXDUMP_INF(&tmp_read_count, LENGHT_READ_COUNT, "flash_write:tmp_read_count:");
}
#endif

void AppTask::Init_cluster_info(void)
{
    light_para_t * p_para = &light_para;
    Protocols::InteractionModel::Status status;
    bool onoff_sts;
    status        = Clusters::OnOff::Attributes::OnOff::Get(1, &(onoff_sts));
    p_para->onoff = (uint8_t) onoff_sts;
    app::DataModel::Nullable<uint8_t> brightness;
    // Read brightness value
    status = Clusters::LevelControl::Attributes::CurrentLevel::Get(kExampleEndpointId, brightness);
    if (status == Protocols::InteractionModel::Status::Success && !brightness.IsNull())
    {
        p_para->level = brightness.Value();
    }
    // Read ColorMode value

    status = Clusters::ColorControl::Attributes::ColorMode::Get(1, (ColorControl::ColorModeEnum *) &(p_para->color_mode));

    // Read ColorTemperatureMireds value
    status = Clusters::ColorControl::Attributes::ColorTemperatureMireds::Get(1, &(p_para->color_temp_mireds));

    // Read CurrentX value
    status = Clusters::ColorControl::Attributes::CurrentX::Get(1, &(p_para->currentx));

    // Read CurrentY value
    status = Clusters::ColorControl::Attributes::CurrentY::Get(1, &(p_para->currenty));

    // Read EnhancedCurrentHue value
    status = Clusters::ColorControl::Attributes::EnhancedCurrentHue::Get(1, &(p_para->enhanced_current_hue));

    // Read CurrentHue value
    status = Clusters::ColorControl::Attributes::CurrentHue::Get(1, &(p_para->cur_hue));

    // Read CurrentSaturation value
    status = Clusters::ColorControl::Attributes::CurrentSaturation::Get(1, &(p_para->cur_saturation));

    // Read OnOffTransitionTime value
    status = Clusters::LevelControl::Attributes::OnOffTransitionTime::Get(1, &(p_para->onoff_transition));
}

void AppTask::Set_cluster_info(void)
{
    printk("%%%%%%Set_cluster_info!!!!%%%%%%\n");
    light_para_t * p_para = &light_para;
    Protocols::InteractionModel::Status status;
    printk("%%%%%%Set_cluster_info:p_para->onoff:%d!!!!%%%%%%\n", p_para->onoff);
    status = Clusters::OnOff::Attributes::OnOff::Set(1, p_para->onoff);
    // Set brightness value
    printk("%%%%%%Set_cluster_info:p_para->level:%d!!!!%%%%%%\n", p_para->level);
    status = Clusters::LevelControl::Attributes::CurrentLevel::Set(kExampleEndpointId, p_para->level);
    // Set ColorMode value
    printk("%%%%%%Set_cluster_info:p_para->color_mode:%d!!!!%%%%%%\n", p_para->color_mode);
    status = Clusters::ColorControl::Attributes::ColorMode::Set(1, (ColorControl::ColorModeEnum) p_para->color_mode);

    // Set ColorTemperatureMireds value
    status = Clusters::ColorControl::Attributes::ColorTemperatureMireds::Set(1, p_para->color_temp_mireds);

    // Set CurrentX value
    status = Clusters::ColorControl::Attributes::CurrentX::Set(1, p_para->currentx);

    // Set CurrentY value
    status = Clusters::ColorControl::Attributes::CurrentY::Set(1, p_para->currenty);

    // Set EnhancedCurrentHue value
    status = Clusters::ColorControl::Attributes::EnhancedCurrentHue::Set(1, p_para->enhanced_current_hue);

    // Set CurrentHue value
    status = Clusters::ColorControl::Attributes::CurrentHue::Set(1, p_para->cur_hue);

    // Set CurrentSaturation value
    status = Clusters::ColorControl::Attributes::CurrentSaturation::Set(1, p_para->cur_saturation);

    // Set OnOffTransitionTime value
    status = Clusters::LevelControl::Attributes::OnOffTransitionTime::Set(1, p_para->onoff_transition);
}

#ifdef CONFIG_CHIP_ENABLE_POWER_ON_FACTORY_RESET
void AppTask::PowerOnFactoryReset(void)
{
    LOG_INF("Lighting App Power On Factory Reset");
    AppEvent event;
    event.Type    = AppEvent::kEventType_DeviceAction;
    event.Handler = PowerOnFactoryResetEventHandler;
    GetAppTask().PostEvent(&event);
}
#endif /* CONFIG_CHIP_ENABLE_POWER_ON_FACTORY_RESET */

CHIP_ERROR AppTask::Init(void)
{
    SetExampleButtonCallbacks(LightingActionEventHandler);
    ReturnErrorOnFailure(InitCommonParts());

    /* user mode means led control by the customer */
#if APP_LIGHT_USER_MODE_EN
    // token_flash_demo_proc();
    /* switch from zigbee, which means uncommission state */
    if (user_para.val == USER_ZB_SW_VAL)
    {
        // read from flash, already proced in the AppTaskCommon::StartApp.
        Set_cluster_info();
    }
    else if (user_para.val == USER_MATTER_PAIR_VAL)
    {
        // need to get the para from the flash , which means commissioned.
        Init_cluster_info();
    }
    else
    {
        // will not proc.
    }

#if (!CONFIG_TOKEN_PERIPHERALS)
#if (APP_LIGHT_MODE == APP_LIGHT_I2C)
    printk("app light mode is i2c\n");
    i2c_demo_proc(); // add i2c demo code to show the para part
#elif (APP_LIGHT_MODE == APP_LIGHT_ADC)
    printk("app light mode is adc\n");
    #if (APP_TELINK_DRIVERS_ADC)
    telink_driver_adc_proc();
    #else
    adc_demo_proc();
    #endif
#elif (APP_LIGHT_MODE == APP_LIGHT_PWM)
    /*add pwm proc here */
    printk("app light mode is pwm\n");
#else
    printk("Function expansion preset position\n");
#endif
#else
    if (hw_token.mode == PWM_MODE_LIGHT)
    {
        /* add pwm proc here */
        printk("app light mode is pwm\n");
        hw_token_pwm_init();
    }
    else if (hw_token.mode == I2C_MODE_LIGHT)
    {
        #if CONFIG_I2C
        printk("app light mode is i2c\n");
        hw_token_i2c_init(hw_token.sda, hw_token.scl);
        i2c_demo_proc(); // add i2c demo code to show the para part.
        #endif
    }
    else
    {
        printk("ERROR:Function expansion preset position\n");
    }
#endif /* !CONFIG_TOKEN_PERIPHERALS */

#else
    Protocols::InteractionModel::Status status;

    app::DataModel::Nullable<uint8_t> brightness;
    // Read brightness value
    status = Clusters::LevelControl::Attributes::CurrentLevel::Get(kExampleEndpointId, brightness);
    if (status == Protocols::InteractionModel::Status::Success && !brightness.IsNull())
    {
        sBrightness = brightness.Value();
    }

    memset(&sLedRgb, sBrightness, sizeof(RgbColor_t));

    bool storedValue;
    // Read storedValue on/off value
    status = Clusters::OnOff::Attributes::OnOff::Get(1, &storedValue);
    if (status == Protocols::InteractionModel::Status::Success)
    {
        // Set actual state to stored before reboot
        SetInitiateAction(storedValue ? ON_ACTION : OFF_ACTION, static_cast<int32_t>(AppEvent::kEventType_DeviceAction), nullptr);
    }
#ifdef CONFIG_TFLM_FEATURE
    app_codec_init();
#endif
#endif /* APP_LIGHT_USER_MODE_EN */
    return CHIP_NO_ERROR;
}

#ifdef CONFIG_TFLM_FEATURE
void AppTask::AudioProcessUpdateTimerTimeoutCallback(k_timer * timer)
{
    if (!timer)
    {
        return;
    }

    AppEvent event;
    event.Type    = AppEvent::kEventType_Timer;
    event.Handler = AudioProcessUpdateTimerEventHandler;
    sAppTask.PostEvent(&event);
}

void AppTask::AudioProcessUpdateTimerEventHandler(AppEvent * aEvent)
{
    int32_t result = 0;
    bool onoff_value;

    if (aEvent->Type != AppEvent::kEventType_Timer)
    {
        return;
    }

    tflite_micro_micro_speech_process_action(&result);

    LOG_INF("result is %d", result);

    if (result == 2)
    {
        onoff_value = 1;
        PlatformMgr().LockChipStack();
        Clusters::OnOff::Attributes::OnOff::Set(1, onoff_value);
        PlatformMgr().UnlockChipStack();
    }
    else if (result == 3)
    {
        onoff_value = 0;
        PlatformMgr().LockChipStack();
        Clusters::OnOff::Attributes::OnOff::Set(1, onoff_value);
        PlatformMgr().UnlockChipStack();
    }
}

void AppTask::MicroSpeechProcessStart()
{
    k_timer_init(&sAudioProcessUpdateTimer, &AppTask::AudioProcessUpdateTimerTimeoutCallback, nullptr);
    k_timer_user_data_set(&sAudioProcessUpdateTimer, &sAppTask);
    k_timer_start(&sAudioProcessUpdateTimer, K_MSEC(kInitialAudioProcessUpdateTimerPeriodMs),
                  K_MSEC(kAudioProcessUpdateTimerPeriodMs));
}

void AppTask::MicroSpeechProcessStop()
{
    k_timer_stop(&sAudioProcessUpdateTimer);
}
#endif

void AppTask::LightingActionEventHandler(AppEvent * aEvent)
{
    Fixture_Action action = INVALID_ACTION;
    int32_t actor         = 0;

    if (aEvent->Type == AppEvent::kEventType_DeviceAction)
    {
        action = static_cast<Fixture_Action>(aEvent->DeviceEvent.Action);
        actor  = aEvent->DeviceEvent.Actor;
    }
    else if (aEvent->Type == AppEvent::kEventType_Button)
    {
        sfixture_on = !sfixture_on;

        sAppTask.UpdateClusterState();
    }
}

void AppTask::UpdateClusterState(void)
{
    Protocols::InteractionModel::Status status;
    bool isTurnedOn  = sfixture_on;
    uint8_t setLevel = sBrightness;

    // write the new on/off value
    status = Clusters::OnOff::Attributes::OnOff::Set(kExampleEndpointId, isTurnedOn);
    if (status != Protocols::InteractionModel::Status::Success)
    {
        LOG_ERR("Update OnOff fail: %x", to_underlying(status));
    }

    status = Clusters::LevelControl::Attributes::CurrentLevel::Set(kExampleEndpointId, setLevel);
    if (status != Protocols::InteractionModel::Status::Success)
    {
        LOG_ERR("Update CurrentLevel fail: %x", to_underlying(status));
    }
}

void AppTask::SetInitiateAction(Fixture_Action aAction, int32_t aActor, uint8_t * value)
{
    bool setRgbAction = false;

    if (aAction == ON_ACTION || aAction == OFF_ACTION)
    {
        if (aAction == ON_ACTION)
        {
            sfixture_on = true;
#ifdef CONFIG_PWM
            PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Red, (((uint32_t) sLedRgb.r * 1000) / UINT8_MAX));
            PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Green, (((uint32_t) sLedRgb.g * 1000) / UINT8_MAX));
            PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Blue, (((uint32_t) sLedRgb.b * 1000) / UINT8_MAX));
#else
            LedManager::getInstance().setLed(LedManager::EAppLed_App0, true);
#endif
        }
        else
        {
            sfixture_on = false;
#ifdef CONFIG_PWM
            PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Red, false);
            PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Green, false);
            PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Blue, false);
#else
            LedManager::getInstance().setLed(LedManager::EAppLed_App0, false);
#endif
        }
    }
    else if (aAction == LEVEL_ACTION)
    {
        // Save a new brightness for ColorControl
        sBrightness = *value;

        if (sColorAction == COLOR_ACTION_XY)
        {
            sLedRgb = XYToRgb(sBrightness, sXY.x, sXY.y);
        }
        else if (sColorAction == COLOR_ACTION_HSV)
        {
            sHSV.v  = sBrightness;
            sLedRgb = HsvToRgb(sHSV);
        }
        else
        {
            memset(&sLedRgb, sBrightness, sizeof(RgbColor_t));
        }

        ChipLogProgress(Zcl, "New brightness: %u | R: %u, G: %u, B: %u", sBrightness, sLedRgb.r, sLedRgb.g, sLedRgb.b);
        setRgbAction = true;
    }
    else if (aAction == COLOR_ACTION_XY)
    {
        sXY     = *reinterpret_cast<XyColor_t *>(value);
        sLedRgb = XYToRgb(sBrightness, sXY.x, sXY.y);
        ChipLogProgress(Zcl, "XY to RGB: X: %u, Y: %u, Level: %u | R: %u, G: %u, B: %u", sXY.x, sXY.y, sBrightness, sLedRgb.r,
                        sLedRgb.g, sLedRgb.b);
        setRgbAction = true;
        sColorAction = COLOR_ACTION_XY;
    }
    else if (aAction == COLOR_ACTION_HSV)
    {
        sHSV    = *reinterpret_cast<HsvColor_t *>(value);
        sHSV.v  = sBrightness;
        sLedRgb = HsvToRgb(sHSV);
        ChipLogProgress(Zcl, "HSV to RGB: H: %u, S: %u, V: %u | R: %u, G: %u, B: %u", sHSV.h, sHSV.s, sHSV.v, sLedRgb.r, sLedRgb.g,
                        sLedRgb.b);
        setRgbAction = true;
        sColorAction = COLOR_ACTION_HSV;
    }
    else if (aAction == COLOR_ACTION_CT)
    {
        sCT = *reinterpret_cast<CtColor_t *>(value);
        if (sCT.ctMireds)
        {
            sLedRgb = CTToRgb(sCT);
            ChipLogProgress(Zcl, "ColorTemp to RGB: CT: %u | R: %u, G: %u, B: %u", sCT.ctMireds, sLedRgb.r, sLedRgb.g, sLedRgb.b);
            setRgbAction = true;
            sColorAction = COLOR_ACTION_CT;
        }
    }

    if (setRgbAction)
    {
        PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Red, (((uint32_t) sLedRgb.r * 1000) / UINT8_MAX));
        PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Green, (((uint32_t) sLedRgb.g * 1000) / UINT8_MAX));
        PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Blue, (((uint32_t) sLedRgb.b * 1000) / UINT8_MAX));
    }
}

#ifdef CONFIG_CHIP_ENABLE_POWER_ON_FACTORY_RESET
static constexpr uint32_t kPowerOnFactoryResetIndicationMax    = 4;
static constexpr uint32_t kPowerOnFactoryResetIndicationTimeMs = 1000;

unsigned int AppTask::sPowerOnFactoryResetTimerCnt;
k_timer AppTask::sPowerOnFactoryResetTimer;

void AppTask::PowerOnFactoryResetEventHandler(AppEvent * aEvent)
{
    LOG_INF("Lighting App Power On Factory Reset Handler");
    sPowerOnFactoryResetTimerCnt = 1;
    PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Red, (bool) (sPowerOnFactoryResetTimerCnt % 2));
    PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Green, (bool) (sPowerOnFactoryResetTimerCnt % 2));
    PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Blue, (bool) (sPowerOnFactoryResetTimerCnt % 2));
#if !CONFIG_PWM
    LedManager::getInstance().setLed(LedManager::EAppLed_App0, (bool) (sPowerOnFactoryResetTimerCnt % 2));
#endif
    k_timer_init(&sPowerOnFactoryResetTimer, PowerOnFactoryResetTimerEvent, nullptr);
    k_timer_start(&sPowerOnFactoryResetTimer, K_MSEC(kPowerOnFactoryResetIndicationTimeMs),
                  K_MSEC(kPowerOnFactoryResetIndicationTimeMs));
}

void AppTask::PowerOnFactoryResetTimerEvent(struct k_timer * timer)
{
    sPowerOnFactoryResetTimerCnt++;
    LOG_INF("Lighting App Power On Factory Reset Handler %u", sPowerOnFactoryResetTimerCnt);
    PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Red, (bool) (sPowerOnFactoryResetTimerCnt % 2));
    PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Green, (bool) (sPowerOnFactoryResetTimerCnt % 2));
    PwmManager::getInstance().setPwm(PwmManager::EAppPwm_Blue, (bool) (sPowerOnFactoryResetTimerCnt % 2));
    if (sPowerOnFactoryResetTimerCnt > kPowerOnFactoryResetIndicationMax)
    {
        k_timer_stop(timer);
        LOG_INF("schedule factory reset");
        chip::Server::GetInstance().ScheduleFactoryReset();
    }
}
#endif /* CONFIG_CHIP_ENABLE_POWER_ON_FACTORY_RESET */

void AppTask::LinkLeds(LedManager & ledManager)
{
#if CONFIG_CHIP_ENABLE_APPLICATION_STATUS_LED
    ledManager.linkLed(LedManager::EAppLed_Status, 0);
#endif

#if !CONFIG_PWM
    ledManager.linkLed(LedManager::EAppLed_App0, 1);
#endif /* !CONFIG_PWM */
}
