/*
 *
 *    Copyright (c) 2023-2024 Project CHIP Authors
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
#include "AliroDelegate.h"
#include "ButtonManager.h"
#include "LEDManager.h"
#include <LockManager.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app/data-model/Nullable.h>
#include <errno.h>

LOG_MODULE_DECLARE(app, CONFIG_CHIP_APP_LOG_LEVEL);

using namespace ::chip::app::Clusters::DoorLock;
using namespace chip;
using namespace chip::app;
using namespace ::chip::DeviceLayer;
using namespace ::chip::DeviceLayer::Internal;

AppTask AppTask::sAppTask;

CHIP_ERROR AppTask::Init(void)
{
    SetExampleButtonCallbacks(LockActionEventHandler);
    ReturnErrorOnFailure(InitCommonParts());

    LedManager::getInstance().setLed(LedManager::EAppLed_App0, LockMgr().IsLocked());

    chip::app::DataModel::Nullable<chip::app::Clusters::DoorLock::DlLockState> state;
    chip::EndpointId endpointId{ kExampleEndpointId };
    chip::DeviceLayer::PlatformMgr().LockChipStack();
    chip::app::Clusters::DoorLock::Attributes::LockState::Get(endpointId, state);

    chip::DeviceLayer::PlatformMgr().UnlockChipStack();

    CHIP_ERROR err = LockMgr().Init(state, LockStateChanged);

    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("LockMgr().Init() failed");
        return err;
    }

    // Register a Door Lock delegate to handle Aliro provisioning attributes/commands.
    ReturnErrorOnFailure(DoorLockServer::Instance().SetDelegate(kExampleEndpointId, &AliroDelegate::GetInstance()));

    telink_aliro_nfc_callbacks aliroCallbacks = {};
    aliroCallbacks.get_lock_state             = GetAliroLockState;
    aliroCallbacks.request_lock_state         = RequestAliroLockState;

    if (telink_aliro_nfc_init(&aliroCallbacks) != 0)
    {
        LOG_ERR("Aliro NFC initialization failed");
        return CHIP_ERROR_INTERNAL;
    }

    return CHIP_NO_ERROR;
}

int AppTask::GetAliroLockState(enum telink_aliro_lock_state * state, void * context)
{
    (void) context;

    if (state == nullptr)
    {
        return -EINVAL;
    }

    switch (LockMgr().getLockState())
    {
    case LockManager::kState_LockCompleted:
        *state = TELINK_ALIRO_LOCK_STATE_SECURED;
        break;
    case LockManager::kState_UnlockCompleted:
        *state = TELINK_ALIRO_LOCK_STATE_UNSECURED;
        break;
    case LockManager::kState_LockInitiated:
        *state = TELINK_ALIRO_LOCK_STATE_BUSY_SECURED;
        break;
    case LockManager::kState_UnlockInitiated:
        *state = TELINK_ALIRO_LOCK_STATE_BUSY_UNSECURED;
        break;
    default:
        *state = TELINK_ALIRO_LOCK_STATE_JAMMED;
        break;
    }

    return 0;
}

int AppTask::RequestAliroLockState(enum telink_aliro_lock_state state, void * context)
{
    (void) context;

    if (state != TELINK_ALIRO_LOCK_STATE_SECURED && state != TELINK_ALIRO_LOCK_STATE_UNSECURED)
    {
        return -ENOTSUP;
    }

    AppEvent event           = {};
    event.Type               = AppEvent::kEventType_DeviceAction;
    event.DeviceEvent.Action = static_cast<uint8_t>(state);
    event.Handler            = AliroLockActionEventHandler;
    GetAppTask().PostEvent(&event);
    return 0;
}

void AppTask::AliroLockActionEventHandler(AppEvent * event)
{
    if (event == nullptr)
    {
        return;
    }

    LockManager::Action_t action;
    switch (event->DeviceEvent.Action)
    {
    case TELINK_ALIRO_LOCK_STATE_SECURED:
        action = LockManager::LOCK_ACTION;
        break;
    case TELINK_ALIRO_LOCK_STATE_UNSECURED:
        action = LockManager::UNLOCK_ACTION;
        break;
    default:
        return;
    }

    if (!LockMgr().LockAction(AppEvent::kEventType_DeviceAction, action, LockManager::OperationSource::kAliro, kExampleEndpointId))
    {
        LOG_ERR("Aliro lock action failed");
    }
}

/* This is a button handler only */
void AppTask::LockActionEventHandler(AppEvent * aEvent)
{
    switch (LockMgr().getLockState())
    {
    case LockManager::kState_NotFulyLocked:
    case LockManager::kState_LockCompleted:
        LockMgr().LockAction(AppEvent::kEventType_DeviceAction, LockManager::UNLOCK_ACTION, LockManager::OperationSource::kButton,
                             kExampleEndpointId);
        break;
    case LockManager::kState_UnlockCompleted:
        LockMgr().LockAction(AppEvent::kEventType_DeviceAction, LockManager::LOCK_ACTION, LockManager::OperationSource::kButton,
                             kExampleEndpointId);
        break;
    default:
        LOG_INF("Lock is in intermediate state, ignoring button");
        break;
    }
}

void AppTask::LockStateChanged(LockManager::State_t state)
{
    switch (state)
    {
    case LockManager::State_t::kState_LockInitiated:
        LOG_INF("Callback: Lock action initiated");
        LedManager::getInstance().setLed(LedManager::EAppLed_App0, 50, 50);
        break;
    case LockManager::State_t::kState_LockCompleted:
        LOG_INF("Callback: Lock action completed");
        LedManager::getInstance().setLed(LedManager::EAppLed_App0, true);
        break;
    case LockManager::State_t::kState_UnlockInitiated:
        LOG_INF("Callback: Unlock action initiated");
        LedManager::getInstance().setLed(LedManager::EAppLed_App0, 50, 50);
        break;
    case LockManager::State_t::kState_UnlockCompleted:
        LOG_INF("Callback: Unlock action completed");
        LedManager::getInstance().setLed(LedManager::EAppLed_App0, false);
        break;
    case LockManager::State_t::kState_NotFulyLocked:
        LOG_INF("Callback: Lock not fully locked. Unexpected state");
        LedManager::getInstance().setLed(LedManager::EAppLed_App0, 10, 90);
        break;
    }
}

void AppTask::LinkButtons(ButtonManager & buttonManager)
{
    buttonManager.addCallback(FactoryResetButtonEventHandler, 0, true);
    buttonManager.addCallback(ExampleActionButtonEventHandler, 1, true);
}

void AppTask::LinkLeds(LedManager & ledManager)
{
#if CONFIG_CHIP_ENABLE_APPLICATION_STATUS_LED
    ledManager.linkLed(LedManager::EAppLed_Status, 0);
    ledManager.linkLed(LedManager::EAppLed_App0, 1);
#else
    ledManager.linkLed(LedManager::EAppLed_App0, 0);
#endif // CONFIG_CHIP_ENABLE_APPLICATION_STATUS_LED
}
