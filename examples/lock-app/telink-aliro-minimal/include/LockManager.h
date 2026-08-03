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

#pragma once

#include <AppConfig.h>
#include <AppEventCommon.h>
#include <app/clusters/door-lock-server/door-lock-server.h>
#include <app/data-model/Nullable.h>
#include <lib/core/CHIPError.h>
#include <zephyr/kernel.h>

class LockManager
{
public:
    using CredentialRuleEnum = chip::app::Clusters::DoorLock::CredentialRuleEnum;
    using CredentialStruct = chip::app::Clusters::DoorLock::Structs::CredentialStruct::Type;
    using CredentialTypeEnum = chip::app::Clusters::DoorLock::CredentialTypeEnum;
    using DlCredentialStatus = ::DlCredentialStatus;
    using DlLockState = chip::app::Clusters::DoorLock::DlLockState;
    using OperationErrorEnum = chip::app::Clusters::DoorLock::OperationErrorEnum;
    using OperationSource = chip::app::Clusters::DoorLock::OperationSourceEnum;
    using UserStatusEnum = chip::app::Clusters::DoorLock::UserStatusEnum;
    using UserTypeEnum = chip::app::Clusters::DoorLock::UserTypeEnum;

    enum Action_t
    {
        LOCK_ACTION = 0,
        UNLOCK_ACTION,
        INVALID_ACTION
    };

    enum State_t
    {
        kState_LockInitiated = 0,
        kState_LockCompleted,
        kState_UnlockInitiated,
        kState_UnlockCompleted,
        kState_NotFulyLocked
    };

    using StateChangeCallback = void (*)(State_t);

    CHIP_ERROR Init(chip::app::DataModel::Nullable<DlLockState> state, StateChangeCallback callback);

    bool LockAction(int32_t appSource, Action_t action, OperationSource source, chip::EndpointId endpointId);
    bool LockAction(int32_t appSource, Action_t action, OperationSource source, chip::EndpointId endpointId,
                    OperationErrorEnum & err,
                    const chip::app::DataModel::Nullable<chip::FabricIndex> & fabricIdx = chip::app::DataModel::NullNullable,
                    const chip::app::DataModel::Nullable<chip::NodeId> & nodeId = chip::app::DataModel::NullNullable,
                    const chip::Optional<chip::ByteSpan> & pinCode = chip::NullOptional);

    bool IsLocked() const { return mState == kState_LockCompleted; }
    State_t getLockState() const { return mState; }

    bool GetUser(chip::EndpointId endpointId, uint16_t userIndex, EmberAfPluginDoorLockUserInfo & user);
    bool SetUser(chip::EndpointId endpointId, uint16_t userIndex, chip::FabricIndex creator, chip::FabricIndex modifier,
                 const chip::CharSpan & userName, uint32_t uniqueId, UserStatusEnum userStatus, UserTypeEnum userType,
                 CredentialRuleEnum credentialRule, const CredentialStruct * credentials, size_t totalCredentials);

    bool GetCredential(chip::EndpointId endpointId, uint16_t credentialIndex, CredentialTypeEnum credentialType,
                       EmberAfPluginDoorLockCredentialInfo & credential);
    bool SetCredential(chip::EndpointId endpointId, uint16_t credentialIndex, chip::FabricIndex creator,
                       chip::FabricIndex modifier, DlCredentialStatus credentialStatus, CredentialTypeEnum credentialType,
                       const chip::ByteSpan & credentialData);

private:
    struct UserSlot
    {
        char name[DOOR_LOCK_USER_NAME_BUFFER_SIZE] = {};
        CredentialStruct credentials[APP_MAX_CREDENTIALS_PER_USER]               = {};
        size_t credentialCount                                                   = 0;
        uint32_t uniqueId                                                        = 0;
        UserStatusEnum status                                                    = UserStatusEnum::kAvailable;
        UserTypeEnum type                                                        = UserTypeEnum::kUnrestrictedUser;
        CredentialRuleEnum credentialRule                                       = CredentialRuleEnum::kSingle;
        chip::FabricIndex createdBy                                              = 0;
        chip::FabricIndex lastModifiedBy                                         = 0;
    };

    friend LockManager & LockMgr();

    bool StartAction(Action_t action, OperationSource source, chip::EndpointId endpointId,
                     const chip::app::DataModel::Nullable<chip::FabricIndex> & fabricIdx,
                     const chip::app::DataModel::Nullable<chip::NodeId> & nodeId);
    static void ActuatorTimerEventHandler(k_timer * timer);
    static void ActuatorAppEventHandler(const AppEvent & event);

    static LockManager sLock;
    State_t mState                           = kState_NotFulyLocked;
    StateChangeCallback mStateChangeCallback = nullptr;
    k_timer mActuatorTimer                   = {};
    UserSlot mUsers[APP_MAX_USERS]           = {};
};

inline LockManager & LockMgr()
{
    return LockManager::sLock;
}
