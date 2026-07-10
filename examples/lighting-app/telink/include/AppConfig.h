/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
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

// ---- Lighting Example App Config ----

#define APP_SET_DEVICE_INFO_PROVIDER 1
#define APP_SET_NETWORK_COMM_ENDPOINT_SEC 0

// -----User setting part
#define APP_LIGHT_USER_MODE_EN 1

#define APP_LIGHT_PWM 0
#define APP_LIGHT_I2C 1
#define APP_LIGHT_ADC 2

#if CONFIG_I2C
#define APP_LIGHT_MODE APP_LIGHT_I2C
#elif CONFIG_ADC
#define APP_LIGHT_MODE APP_LIGHT_ADC
#define APP_TELINK_DRIVERS_ADC 0
#else
#define APP_LIGHT_MODE APP_LIGHT_PWM
#endif

// Independent Factory Reset Button
#define INDEPENDENT_FACTORY_RESET_BUTTON 0

#define FW_TYPE_PRODUCT 0
#define FW_TYPE_DEBUG 1
#define FW_TYPE_DEVELOP 2

#define MATTER_FW_TYPE FW_TYPE_PRODUCT
