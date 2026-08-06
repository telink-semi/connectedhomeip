/*
 *
 *    Copyright (c) 2023-2026 Project CHIP Authors
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

#include <LockManager.h>

#include <AliroDelegate.h>
#include <AppTask.h>
#include <app/clusters/door-lock-server/door-lock-server.h>
#include <cstring>
#include <lib/support/logging/CHIPLogging.h>

LOG_MODULE_DECLARE(app, CONFIG_CHIP_APP_LOG_LEVEL);

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters::DoorLock;

LockManager LockManager::sLock;

CHIP_ERROR LockManager::Init(DataModel::Nullable<DlLockState> state, StateChangeCallback callback)
{
    mStateChangeCallback = callback;
    mState               = !state.IsNull() && state.Value() == DlLockState::kLocked ? kState_LockCompleted : kState_UnlockCompleted;

    k_timer_init(&mActuatorTimer, &LockManager::ActuatorTimerEventHandler, nullptr);
    k_timer_user_data_set(&mActuatorTimer, this);
    return CHIP_NO_ERROR;
}

bool LockManager::LockAction(int32_t appSource, Action_t action, OperationSource source, EndpointId endpointId)
{
    (void) appSource;
    return StartAction(action, source, endpointId, DataModel::NullNullable, DataModel::NullNullable);
}

bool LockManager::LockAction(int32_t appSource, Action_t action, OperationSource source, EndpointId endpointId,
                             OperationErrorEnum & err, const DataModel::Nullable<FabricIndex> & fabricIdx,
                             const DataModel::Nullable<NodeId> & nodeId, const Optional<ByteSpan> & pinCode)
{
    (void) appSource;
    (void) pinCode;
    err = OperationErrorEnum::kUnspecified;
    return StartAction(action, source, endpointId, fabricIdx, nodeId);
}

bool LockManager::StartAction(Action_t action, OperationSource source, EndpointId endpointId,
                              const DataModel::Nullable<FabricIndex> & fabricIdx, const DataModel::Nullable<NodeId> & nodeId)
{
    DlLockState target;

    switch (action)
    {
    case LOCK_ACTION:
        if (mState == kState_LockCompleted)
        {
            return true;
        }
        target = DlLockState::kLocked;
        mState = kState_LockInitiated;
        break;
    case UNLOCK_ACTION:
        if (mState == kState_UnlockCompleted)
        {
            return true;
        }
        target = DlLockState::kUnlocked;
        mState = kState_UnlockInitiated;
        break;
    default:
        return false;
    }

    if (!DoorLockServer::Instance().SetLockState(endpointId, target, source, DataModel::NullNullable, DataModel::NullNullable,
                                                 fabricIdx, nodeId))
    {
        mState = kState_NotFulyLocked;
        if (mStateChangeCallback != nullptr)
        {
            mStateChangeCallback(mState);
        }
        return false;
    }

    if (mStateChangeCallback != nullptr)
    {
        mStateChangeCallback(mState);
    }
    k_timer_start(&mActuatorTimer, K_MSEC(LOCK_MANAGER_ACTUATOR_MOVEMENT_TIME_MS), K_NO_WAIT);
    return true;
}

void LockManager::ActuatorTimerEventHandler(k_timer * timer)
{
    AppEvent event;
    event.Type               = AppEvent::kEventType_Timer;
    event.TimerEvent.Context = k_timer_user_data_get(timer);
    event.Handler            = reinterpret_cast<EventHandler>(LockManager::ActuatorAppEventHandler);
    GetAppTask().PostEvent(&event);
}

void LockManager::ActuatorAppEventHandler(const AppEvent & event)
{
    auto * lock = static_cast<LockManager *>(event.TimerEvent.Context);
    if (lock == nullptr)
    {
        return;
    }

    if (lock->mState == kState_LockInitiated)
    {
        lock->mState = kState_LockCompleted;
    }
    else if (lock->mState == kState_UnlockInitiated)
    {
        lock->mState = kState_UnlockCompleted;
    }
    else
    {
        return;
    }

    if (lock->mStateChangeCallback != nullptr)
    {
        lock->mStateChangeCallback(lock->mState);
    }
}

bool LockManager::GetUser(EndpointId endpointId, uint16_t userIndex, EmberAfPluginDoorLockUserInfo & user)
{
    (void) endpointId;
    if (userIndex == 0 || userIndex > APP_MAX_USERS)
    {
        return false;
    }

    const UserSlot & slot = mUsers[userIndex - 1];
    user.userStatus       = slot.status;
    if (slot.status == UserStatusEnum::kAvailable)
    {
        return true;
    }

    user.userName           = CharSpan(slot.name, strlen(slot.name));
    user.credentials        = Span<const CredentialStruct>(slot.credentials, slot.credentialCount);
    user.userUniqueId       = slot.uniqueId;
    user.userType           = slot.type;
    user.credentialRule     = slot.credentialRule;
    user.creationSource     = DlAssetSource::kMatterIM;
    user.createdBy          = slot.createdBy;
    user.modificationSource = DlAssetSource::kMatterIM;
    user.lastModifiedBy     = slot.lastModifiedBy;
    return true;
}

bool LockManager::SetUser(EndpointId endpointId, uint16_t userIndex, FabricIndex creator, FabricIndex modifier,
                          const CharSpan & userName, uint32_t uniqueId, UserStatusEnum userStatus, UserTypeEnum userType,
                          CredentialRuleEnum credentialRule, const CredentialStruct * credentials, size_t totalCredentials)
{
    (void) endpointId;
    if (userIndex == 0 || userIndex > APP_MAX_USERS || userName.size() > DOOR_LOCK_MAX_USER_NAME_SIZE ||
        totalCredentials > APP_MAX_CREDENTIALS_PER_USER || (totalCredentials != 0 && credentials == nullptr))
    {
        return false;
    }

    UserSlot & slot = mUsers[userIndex - 1];
    if (userStatus == UserStatusEnum::kAvailable)
    {
        slot = UserSlot{};
        return true;
    }

    memcpy(slot.name, userName.data(), userName.size());
    slot.name[userName.size()] = '\0';
    if (totalCredentials != 0)
    {
        memcpy(slot.credentials, credentials, totalCredentials * sizeof(CredentialStruct));
    }
    slot.credentialCount = totalCredentials;
    slot.uniqueId        = uniqueId;
    slot.status          = userStatus;
    slot.type            = userType;
    slot.credentialRule  = credentialRule;
    slot.createdBy       = creator;
    slot.lastModifiedBy  = modifier;
    return true;
}

bool LockManager::GetCredential(EndpointId endpointId, uint16_t credentialIndex, CredentialTypeEnum credentialType,
                                EmberAfPluginDoorLockCredentialInfo & credential)
{
    (void) endpointId;
    if (AliroDelegate::IsAliroCredentialType(credentialType))
    {
        return AliroDelegate::GetInstance().GetCredential(credentialIndex, credentialType, credential);
    }

    if (credentialType == CredentialTypeEnum::kProgrammingPIN && credentialIndex == 0)
    {
        credential.status = DlCredentialStatus::kAvailable;
        return true;
    }

    if (credentialType != CredentialTypeEnum::kPin || credentialIndex == 0 || credentialIndex > APP_MAX_PIN_CREDENTIALS)
    {
        return false;
    }

    const PinCredentialSlot & slot = mPinCredentials[credentialIndex - 1];
    credential.status              = slot.status;
    if (slot.status == DlCredentialStatus::kAvailable)
    {
        return true;
    }

    credential.credentialType     = CredentialTypeEnum::kPin;
    credential.credentialData     = ByteSpan(slot.data, slot.dataSize);
    credential.creationSource     = DlAssetSource::kMatterIM;
    credential.createdBy          = slot.createdBy;
    credential.modificationSource = DlAssetSource::kMatterIM;
    credential.lastModifiedBy     = slot.lastModifiedBy;
    return true;
}

bool LockManager::SetCredential(EndpointId endpointId, uint16_t credentialIndex, FabricIndex creator, FabricIndex modifier,
                                DlCredentialStatus credentialStatus, CredentialTypeEnum credentialType,
                                const ByteSpan & credentialData)
{
    (void) endpointId;
    if (AliroDelegate::IsAliroCredentialType(credentialType))
    {
        return AliroDelegate::GetInstance().SetCredential(credentialIndex, creator, modifier, credentialStatus, credentialType,
                                                          credentialData);
    }

    if (credentialType != CredentialTypeEnum::kPin || credentialIndex == 0 || credentialIndex > APP_MAX_PIN_CREDENTIALS ||
        credentialData.size() > sizeof(mPinCredentials[0].data))
    {
        return false;
    }

    PinCredentialSlot & slot = mPinCredentials[credentialIndex - 1];
    if (credentialStatus == DlCredentialStatus::kAvailable)
    {
        slot = PinCredentialSlot{};
        return true;
    }

    slot.status         = credentialStatus;
    slot.createdBy      = creator;
    slot.lastModifiedBy = modifier;
    slot.dataSize       = credentialData.size();
    memcpy(slot.data, credentialData.data(), credentialData.size());
    return true;
}

bool LockManager::ValidateAliroEndpointKey(const ByteSpan & key) const
{
    CredentialTypeEnum credentialType;
    uint16_t credentialIndex;

    VerifyOrReturnValue(AliroDelegate::GetInstance().FindEndpointKey(key, credentialType, credentialIndex), false);

    for (const auto & user : mUsers)
    {
        if (user.status == UserStatusEnum::kAvailable)
        {
            continue;
        }

        for (size_t i = 0; i < user.credentialCount; ++i)
        {
            if (user.credentials[i].credentialType == credentialType &&
                user.credentials[i].credentialIndex == credentialIndex)
            {
                return true;
            }
        }
    }

    return false;
}
