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

#include "AppConfig.h"
#include "AppTask.h"
#include "ColorFormat.h"

#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>
#include <lib/support/logging/CHIPLogging.h>

LOG_MODULE_DECLARE(app, CONFIG_CHIP_APP_LOG_LEVEL);

using namespace chip;
using namespace chip::app::Clusters;

#if APP_LIGHT_USER_MODE_EN

#if CONFIG_STARTUP_OPTIMIZATE
#include "AppTaskCommon.h"

static uint8_t latest_level;
#define CLUTER_SOTRE_TIMEOUT 500
#define TRANSTION_TIMER_INIT_FLAG 0x55
#define TRANSTION_TIMER_DEINIT_FLAG 0x00

struct k_timer LevelChangeTimer;
static int timer_period   = CLUTER_SOTRE_TIMEOUT;
static uint8_t init_timer = TRANSTION_TIMER_INIT_FLAG;

static void LevelTimeoutCallback(struct k_timer * timer)
{
    if (!timer)
    {
        return;
    }

    cluster_startup_para *p_para = &g_light_cluster_para;
    if (p_para->currentLevel != latest_level)
    {
        p_para->preCurrentLevel = p_para->currentLevel;
        p_para->currentLevel    = latest_level;
        printk("[LevelTimeoutCallback] currentLevel=%d\n", p_para->currentLevel);
        if (store_cluster_para(p_para) != 0)
        {
            printk("[LevelTimeoutCallback] Fail store startup cluster para\n");
        }
    }
}

void MatterPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath, uint8_t type, uint16_t size,
                                       uint8_t * value)
{
    /* user mode, add the customer code here for cb */
    ClusterId clusterId     = attributePath.mClusterId;
    AttributeId attributeId = attributePath.mAttributeId;

    if (init_timer == TRANSTION_TIMER_INIT_FLAG)
    {
        k_timer_init(&LevelChangeTimer, &LevelTimeoutCallback, nullptr);
        init_timer = TRANSTION_TIMER_DEINIT_FLAG;
    }

    Protocols::InteractionModel::Status status;
    Clusters::ColorControl::ColorModeEnum tmpColorMode;
    Clusters::ColorControl::EnhancedColorModeEnum tmpEnhancedColorMode;
    cluster_startup_para * p_para = &g_light_cluster_para;
    if (clusterId == OnOff::Id)
    {
        if (attributeId == OnOff::Attributes::OnOff::Id)
        {
            uint8_t tmp = *value;
            if (p_para->onOff != tmp)
            {
                p_para->onOff = tmp;
                printk("[clusterId:OnOff] OnOff=%d\n", p_para->onOff);

                if (store_cluster_para(p_para) != 0)
                {
                    printk("[clusterId:OnOff] Fail store startup cluster para\n");
                }
            }
        }
        else if (attributeId == OnOff::Attributes::StartUpOnOff::Id)
        {
            uint8_t tmp = *value;
            DataModel::Nullable<chip::app::Clusters::OnOff::StartUpOnOffEnum> tmp1 =
                (chip::app::Clusters::OnOff::StartUpOnOffEnum) tmp;
            if (tmp1.IsNull())
            {
                tmp = 0xff;
            }
            if (p_para->startUpOnOff != tmp)
            {
                p_para->startUpOnOff = tmp;
                printk("[clusterId:OnOff] StartUpOnOff=%d\n", p_para->startUpOnOff);
                if (store_cluster_para(p_para) != 0)
                {
                    printk("[clusterId:OnOff] Fail store startup cluster para\n");
                }
            }
        }
    }
    else if (clusterId == LevelControl::Id)
    {
        if (attributeId == LevelControl::Attributes::CurrentLevel::Id)
        {
            latest_level = *value;
            k_timer_stop(&LevelChangeTimer);
            k_timer_start(&LevelChangeTimer, K_MSEC(timer_period), K_NO_WAIT);
        }
        else if (attributeId == LevelControl::Attributes::MinLevel::Id)
        {
            uint8_t tmp = *value;
            if (p_para->minLevel != tmp)
            {
                p_para->minLevel = tmp;
                printk("[clusterId:LevelControl] MinLevel=%d\n", p_para->minLevel);
                if (store_cluster_para(p_para) != 0)
                {
                    printk("[clusterId:OnOff] Fail store startup cluster para\n");
                }
            }
        }
        else if (attributeId == LevelControl::Attributes::MaxLevel::Id)
        {
            uint8_t tmp = *value;
            if (p_para->maxLevel != tmp)
            {
                p_para->maxLevel = tmp;
                printk("[clusterId:LevelControl] MaxLevel=%d\n", p_para->maxLevel);
                if (store_cluster_para(p_para) != 0)
                {
                    printk("[clusterId:OnOff] Fail store startup cluster para\n");
                }
            }
        }
        else if (attributeId == LevelControl::Attributes::StartUpCurrentLevel::Id)
        {
            uint8_t tmp             = *value;
            p_para->preCurrentLevel = p_para->currentLevel;

            DataModel::Nullable<uint8_t> tmp1 = tmp;
            if (tmp1.IsNull())
            {
                tmp = 0xff;
            }
            p_para->startUpCurrentLevel = tmp;
            printk("[clusterId:LevelControl] StartUpCurrentLevel=%d\n", p_para->startUpCurrentLevel);
            if (store_cluster_para(p_para) != 0)
            {
                printk("[clusterId:OnOff] Fail store startup cluster para\n");
            }
        }
    }
    else if (clusterId == ColorControl::Id)
    {
        if (attributeId == ColorControl::Attributes::CurrentHue::Id)
        {
            uint8_t tmp = *value;
            status      = Clusters::ColorControl::Attributes::ColorMode::Get(1, &tmpColorMode);
            p_para->colorMode = static_cast<uint8_t>(tmpColorMode);
            status      = Clusters::ColorControl::Attributes::EnhancedColorMode::Get(1, &tmpEnhancedColorMode);
            p_para->enhancedColorMode = static_cast<uint8_t>(tmpEnhancedColorMode);

            p_para->hsv.h = tmp;
            printk("[clusterId:ColorControl] CurrentHue=%d\n", p_para->hsv.h);
            printk("[clusterId:ColorControl] ColorMode=%d\n", p_para->colorMode);
            printk("[clusterId:ColorControl] EnhancedColorMode=%d\n", p_para->enhancedColorMode);
            if (store_cluster_para(p_para) != 0)
            {
                printk("[clusterId:OnOff] Fail store startup cluster para\n");
            }
        }
        else if (attributeId == ColorControl::Attributes::CurrentSaturation::Id)
        {
            uint8_t tmp = *value;
            status      = Clusters::ColorControl::Attributes::ColorMode::Get(1, &tmpColorMode);
            p_para->colorMode = static_cast<uint8_t>(tmpColorMode);
            status      = Clusters::ColorControl::Attributes::EnhancedColorMode::Get(1, &tmpEnhancedColorMode);
            p_para->enhancedColorMode = static_cast<uint8_t>(tmpEnhancedColorMode);

            p_para->hsv.s = tmp;
            printk("[clusterId:ColorControl] CurrentSaturation=%d\n", p_para->hsv.s);
            printk("[clusterId:ColorControl] ColorMode=%d\n", p_para->colorMode);
            printk("[clusterId:ColorControl] EnhancedColorMode=%d\n", p_para->enhancedColorMode);
            if (store_cluster_para(p_para) != 0)
            {
                printk("[clusterId:OnOff] Fail store startup cluster para\n");
            }
        }
        else if (attributeId == ColorControl::Attributes::CurrentX::Id)
        {
            uint16_t tmp = *reinterpret_cast<uint16_t *>(value);
            status       = Clusters::ColorControl::Attributes::ColorMode::Get(1, &tmpColorMode);
            p_para->colorMode = static_cast<uint8_t>(tmpColorMode);
            status       = Clusters::ColorControl::Attributes::EnhancedColorMode::Get(1, &tmpEnhancedColorMode);
            p_para->enhancedColorMode = static_cast<uint8_t>(tmpEnhancedColorMode);

            p_para->xy.x = tmp;
            printk("[clusterId:ColorControl] CurrentX=%d\n", p_para->xy.x);
            printk("[clusterId:ColorControl] ColorMode=%d\n", p_para->colorMode);
            printk("[clusterId:ColorControl] EnhancedColorMode=%d\n", p_para->enhancedColorMode);
            if (store_cluster_para(p_para) != 0)
            {
                printk("[clusterId:OnOff] Fail store startup cluster para\n");
            }
        }
        else if (attributeId == ColorControl::Attributes::CurrentY::Id)
        {

            uint16_t tmp = *reinterpret_cast<uint16_t *>(value);
            status       = Clusters::ColorControl::Attributes::ColorMode::Get(1, &tmpColorMode);
            p_para->colorMode = static_cast<uint8_t>(tmpColorMode);
            status       = Clusters::ColorControl::Attributes::EnhancedColorMode::Get(1, &tmpEnhancedColorMode);
            p_para->enhancedColorMode = static_cast<uint8_t>(tmpEnhancedColorMode);

            p_para->xy.y = tmp;
            printk("[clusterId:ColorControl] CurrentX=%d\n", p_para->xy.x);
            printk("[clusterId:ColorControl] ColorMode=%d\n", p_para->colorMode);
            printk("[clusterId:ColorControl] EnhancedColorMode=%d\n", p_para->enhancedColorMode);
            if (store_cluster_para(p_para) != 0)
            {
                printk("[clusterId:OnOff] Fail store startup cluster para\n");
            }
        }
        else if (attributeId == ColorControl::Attributes::ColorTemperatureMireds::Id)
        {
            uint16_t tmp = *reinterpret_cast<uint16_t *>(value);
            status       = Clusters::ColorControl::Attributes::ColorMode::Get(1, &tmpColorMode);
            p_para->colorMode = static_cast<uint8_t>(tmpColorMode);
            status       = Clusters::ColorControl::Attributes::EnhancedColorMode::Get(1, &tmpEnhancedColorMode);
            p_para->enhancedColorMode = static_cast<uint8_t>(tmpEnhancedColorMode);

            p_para->colorTemperatureMireds = tmp;
            printk("[clusterId:ColorControl] ColorTemperatureMireds=%d\n", p_para->colorTemperatureMireds);
            printk("[clusterId:ColorControl] ColorMode=%d\n", p_para->colorMode);
            printk("[clusterId:ColorControl] EnhancedColorMode=%d\n", p_para->enhancedColorMode);
            if (store_cluster_para(p_para) != 0)
            {
                printk("[clusterId:OnOff] Fail store startup cluster para\n");
            }
        }
        else if (attributeId == ColorControl::Attributes::ColorMode::Id)
        {
            uint8_t tmp = *value;
            printk("[clusterId:ColorControl] ColorMode=%d\n", p_para->colorMode);
            if (p_para->colorMode != tmp)
            {
                p_para->colorMode = tmp;
                if (store_cluster_para(p_para) != 0)
                {
                    printk("[clusterId:OnOff] Fail store startup cluster para\n");
                }
            }
        }
        else if (attributeId == ColorControl::Attributes::EnhancedCurrentHue::Id)
        {
            uint16_t tmp = *reinterpret_cast<uint16_t *>(value);
            status       = Clusters::ColorControl::Attributes::ColorMode::Get(1, &tmpColorMode);
            p_para->colorMode = static_cast<uint8_t>(tmpColorMode);
            status       = Clusters::ColorControl::Attributes::EnhancedColorMode::Get(1, &tmpEnhancedColorMode);
            p_para->enhancedColorMode = static_cast<uint8_t>(tmpEnhancedColorMode);

            p_para->enhancedCurrentHue = tmp;
            printk("[clusterId:ColorControl] EnhancedCurrentHue=%d\n", p_para->enhancedCurrentHue);
            printk("[clusterId:ColorControl] ColorMode=%d\n", p_para->colorMode);
            printk("[clusterId:ColorControl] EnhancedColorMode=%d\n", p_para->enhancedColorMode);
            if (store_cluster_para(p_para) != 0)
            {
                printk("[clusterId:OnOff] Fail store startup cluster para\n");
            }
        }
        else if (attributeId == ColorControl::Attributes::EnhancedColorMode::Id)
        {
            uint8_t tmp = *value;
            printk("[clusterId:ColorControl] EnhancedColorMode=%d\n", p_para->enhancedColorMode);
            if (p_para->enhancedColorMode != tmp)
            {
                p_para->enhancedColorMode = tmp;
                if (store_cluster_para(p_para) != 0)
                {
                    printk("[clusterId:OnOff] Fail store startup cluster para\n");
                }
            }
        }
        else if (attributeId == ColorControl::Attributes::StartUpColorTemperatureMireds::Id)
        {
            uint16_t tmp                       = *reinterpret_cast<uint16_t *>(value);
            DataModel::Nullable<uint16_t> tmp1 = tmp;
            if (tmp1.IsNull())
            {
                tmp = 0xffff;
            }
            status = Clusters::ColorControl::Attributes::ColorMode::Get(1, &tmpColorMode);
            p_para->colorMode = static_cast<uint8_t>(tmpColorMode);
            status = Clusters::ColorControl::Attributes::EnhancedColorMode::Get(1, &tmpEnhancedColorMode);
            p_para->enhancedColorMode = static_cast<uint8_t>(tmpEnhancedColorMode);

            if (p_para->startUpColorTemperatureMireds != tmp)
            {
                p_para->startUpColorTemperatureMireds = tmp;
                if (store_cluster_para(p_para) != 0)
                {
                    printk("[clusterId:OnOff] Fail store startup cluster para\n");
                }
            }
        }
    }
}
#else
void MatterPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath, uint8_t type, uint16_t size,
                                       uint8_t * value)
{
    /* user mode, add the customer code here for cb */
    return;
}
#endif /* CONFIG_STARTUP_OPTIMIZATE */

#else
void MatterPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath, uint8_t type, uint16_t size,
                                       uint8_t * value)
{
    static HsvColor_t hsv;
    static XyColor_t xy;
    ClusterId clusterId     = attributePath.mClusterId;
    AttributeId attributeId = attributePath.mAttributeId;

    if (clusterId == OnOff::Id && attributeId == OnOff::Attributes::OnOff::Id)
    {
        ChipLogDetail(Zcl, "Cluster OnOff: attribute OnOff set to %u", *value);
        GetAppTask().SetInitiateAction(*value ? AppTask::ON_ACTION : AppTask::OFF_ACTION,
                                       static_cast<int32_t>(AppEvent::kEventType_DeviceAction), value);
    }
    else if (clusterId == LevelControl::Id && attributeId == LevelControl::Attributes::CurrentLevel::Id)
    {
        if (GetAppTask().IsTurnedOn())
        {
            ChipLogDetail(Zcl, "Cluster LevelControl: attribute CurrentLevel set to %u", *value);
            GetAppTask().SetInitiateAction(AppTask::LEVEL_ACTION, static_cast<int32_t>(AppEvent::kEventType_DeviceAction), value);
        }
        else
        {
            ChipLogDetail(Zcl, "LED is off. Try to use move-to-level-with-on-off instead of move-to-level");
        }
    }
    else if (clusterId == ColorControl::Id)
    {
        /* Ignore several attributes that are currently not processed */
        if ((attributeId == ColorControl::Attributes::RemainingTime::Id) ||
            (attributeId == ColorControl::Attributes::EnhancedColorMode::Id) ||
            (attributeId == ColorControl::Attributes::ColorMode::Id))
        {
            return;
        }

        /* XY color space */
        if (attributeId == ColorControl::Attributes::CurrentX::Id || attributeId == ColorControl::Attributes::CurrentY::Id)
        {
            if (attributeId == ColorControl::Attributes::CurrentX::Id)
            {
                xy.x = *reinterpret_cast<uint16_t *>(value);
            }
            else if (attributeId == ColorControl::Attributes::CurrentY::Id)
            {
                xy.y = *reinterpret_cast<uint16_t *>(value);
            }

            ChipLogDetail(Zcl, "New XY color: %u|%u", xy.x, xy.y);
            GetAppTask().SetInitiateAction(AppTask::COLOR_ACTION_XY, static_cast<int32_t>(AppEvent::kEventType_DeviceAction),
                                           (uint8_t *) &xy);
        }
        /* HSV color space */
        else if (attributeId == ColorControl::Attributes::CurrentHue::Id ||
                 attributeId == ColorControl::Attributes::CurrentSaturation::Id ||
                 attributeId == ColorControl::Attributes::EnhancedCurrentHue::Id)
        {
            if (attributeId == ColorControl::Attributes::EnhancedCurrentHue::Id)
            {
                hsv.h = (uint8_t) (((*reinterpret_cast<uint16_t *>(value)) & 0xFF00) >> 8);
                hsv.s = (uint8_t) ((*reinterpret_cast<uint16_t *>(value)) & 0xFF);
            }
            else if (attributeId == ColorControl::Attributes::CurrentHue::Id)
            {
                hsv.h = *value;
            }
            else if (attributeId == ColorControl::Attributes::CurrentSaturation::Id)
            {
                hsv.s = *value;
            }
            ChipLogDetail(Zcl, "New HSV color: hue = %u| saturation = %u", hsv.h, hsv.s);
            GetAppTask().SetInitiateAction(AppTask::COLOR_ACTION_HSV, static_cast<int32_t>(AppEvent::kEventType_DeviceAction),
                                           (uint8_t *) &hsv);
        }
        /* Temperature Mireds color space */
        else if (attributeId == ColorControl::Attributes::ColorTemperatureMireds::Id)
        {
            ChipLogDetail(Zcl, "New Temperature Mireds color = %u", *(uint16_t *) value);
            GetAppTask().SetInitiateAction(AppTask::COLOR_ACTION_CT, static_cast<int32_t>(AppEvent::kEventType_DeviceAction),
                                           value);
        }
        else
        {
            ChipLogDetail(Zcl, "Ignore ColorControl attribute (%u) that is not currently processed!", attributeId);
        }
    }
}
#endif /* APP_LIGHT_USER_MODE_EN */
