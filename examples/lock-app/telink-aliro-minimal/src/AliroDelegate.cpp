/*
 *
 *    Copyright (c) 2026 Project CHIP Authors
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

#include "AliroDelegate.h"

#include <crypto/CHIPCryptoPAL.h>
#include <lib/core/CHIPEncoding.h>
#include <lib/support/Span.h>
#include <lib/support/logging/CHIPLogging.h>
#include <telink_aliro/telink_aliro.h>

using namespace chip;
using namespace chip::app::Clusters::DoorLock;

AliroDelegate AliroDelegate::sInstance;

// ---------------------------------------------------------------------------
// DoorLock::Delegate - Aliro provisioning attributes
// ---------------------------------------------------------------------------

CHIP_ERROR AliroDelegate::GetAliroReaderVerificationKey(MutableByteSpan & verificationKey)
{
    ChipLogProgress(Zcl, "[Aliro] Read ReaderVerificationKey (configured=%u)", mAliroStateInitialized);

    if (!mAliroStateInitialized)
    {
        verificationKey.reduce_size(0);
        return CHIP_NO_ERROR;
    }

    return CopySpanToMutableSpan(ByteSpan(mAliroReaderVerificationKey), verificationKey);
}

CHIP_ERROR AliroDelegate::GetAliroReaderGroupIdentifier(MutableByteSpan & groupIdentifier)
{
    ChipLogProgress(Zcl, "[Aliro] Read ReaderGroupIdentifier (configured=%u)", mAliroStateInitialized);

    if (!mAliroStateInitialized)
    {
        groupIdentifier.reduce_size(0);
        return CHIP_NO_ERROR;
    }

    return CopySpanToMutableSpan(ByteSpan(mAliroReaderGroupIdentifier), groupIdentifier);
}

CHIP_ERROR AliroDelegate::GetAliroReaderGroupSubIdentifier(MutableByteSpan & groupSubIdentifier)
{
    ChipLogProgress(Zcl, "[Aliro] Read ReaderGroupSubIdentifier (configured=%u)", mAliroStateInitialized);

    if (!mAliroStateInitialized)
    {
        groupSubIdentifier.reduce_size(0);
        return CHIP_NO_ERROR;
    }

    return CopySpanToMutableSpan(ByteSpan(mAliroReaderGroupSubIdentifier), groupSubIdentifier);
}

CHIP_ERROR AliroDelegate::CopyProtocolVersionIntoSpan(uint16_t protocolVersionValue, MutableByteSpan & protocolVersion)
{
    static_assert(sizeof(protocolVersionValue) == kAliroProtocolVersionSize);

    if (protocolVersion.size() < kAliroProtocolVersionSize)
    {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    // Per Aliro spec, protocol version encoding is big-endian.
    Encoding::BigEndian::Put16(protocolVersion.data(), protocolVersionValue);
    protocolVersion.reduce_size(kAliroProtocolVersionSize);
    return CHIP_NO_ERROR;
}

CHIP_ERROR AliroDelegate::GetAliroExpeditedTransactionSupportedProtocolVersionAtIndex(size_t index,
                                                                                      MutableByteSpan & protocolVersion)
{
    ChipLogProgress(Zcl, "[Aliro] Read ExpeditedProtocolVersion (index=%u)", static_cast<unsigned>(index));

    // Only claim support for the one known protocol version for now: 0x0100.
    constexpr uint16_t knownProtocolVersion = 0x0100;

    if (index > 0)
    {
        return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED;
    }

    return CopyProtocolVersionIntoSpan(knownProtocolVersion, protocolVersion);
}

CHIP_ERROR AliroDelegate::GetAliroGroupResolvingKey(MutableByteSpan & groupResolvingKey)
{
    ChipLogProgress(Zcl, "[Aliro] Read GroupResolvingKey (configured=%u)", mAliroStateInitialized);

    if (!mAliroStateInitialized)
    {
        groupResolvingKey.reduce_size(0);
        return CHIP_NO_ERROR;
    }

    return CopySpanToMutableSpan(ByteSpan(mAliroGroupResolvingKey), groupResolvingKey);
}

CHIP_ERROR AliroDelegate::GetAliroSupportedBLEUWBProtocolVersionAtIndex(size_t index, MutableByteSpan & protocolVersion)
{
    ChipLogProgress(Zcl, "[Aliro] Read BLEUWBProtocolVersion (index=%u)", static_cast<unsigned>(index));

    (void) index;
    (void) protocolVersion;
    return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED;
}

uint8_t AliroDelegate::GetAliroBLEAdvertisingVersion()
{
    ChipLogProgress(Zcl, "[Aliro] Read BLEAdvertisingVersion");

    // For now the only defined value of the BLE advertising version for Aliro is 0.
    return 0;
}

uint16_t AliroDelegate::GetNumberOfAliroCredentialIssuerKeysSupported()
{
    ChipLogProgress(Zcl, "[Aliro] Read NumberOfCredentialIssuerKeysSupported");
    return APP_MAX_ALIRO_ISSUER_KEYS;
}

uint16_t AliroDelegate::GetNumberOfAliroEndpointKeysSupported()
{
    ChipLogProgress(Zcl, "[Aliro] Read NumberOfEndpointKeysSupported");
    return APP_MAX_ALIRO_ENDPOINT_KEYS;
}

CHIP_ERROR AliroDelegate::SetAliroReaderConfig(const ByteSpan & signingKey, const ByteSpan & verificationKey,
                                               const ByteSpan & groupIdentifier, const Optional<ByteSpan> & groupResolvingKey)
{
    uint8_t groupSubIdentifier[sizeof(mAliroReaderGroupSubIdentifier)];

    VerifyOrReturnError(verificationKey.size() == sizeof(mAliroReaderVerificationKey), CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(groupIdentifier.size() == sizeof(mAliroReaderGroupIdentifier), CHIP_ERROR_INVALID_ARGUMENT);

    if (groupResolvingKey.HasValue())
    {
        VerifyOrReturnError(groupResolvingKey.Value().size() == sizeof(mAliroGroupResolvingKey), CHIP_ERROR_INVALID_ARGUMENT);
    }

    ReturnErrorOnFailure(Crypto::DRBG_get_bytes(groupSubIdentifier, sizeof(groupSubIdentifier)));

    int err = telink_aliro_nfc_set_reader_config(signingKey.data(), signingKey.size(), verificationKey.data(),
                                                 verificationKey.size(), groupIdentifier.data(), groupIdentifier.size(),
                                                 groupSubIdentifier, sizeof(groupSubIdentifier));
    VerifyOrReturnError(err == 0, CHIP_ERROR_INTERNAL,
                        ChipLogError(Zcl, "Unable to apply Aliro NFC reader configuration: %d", err));

    memcpy(mAliroReaderVerificationKey, verificationKey.data(), sizeof(mAliroReaderVerificationKey));
    memcpy(mAliroReaderGroupIdentifier, groupIdentifier.data(), sizeof(mAliroReaderGroupIdentifier));
    memcpy(mAliroReaderGroupSubIdentifier, groupSubIdentifier, sizeof(mAliroReaderGroupSubIdentifier));

    if (groupResolvingKey.HasValue())
    {
        memcpy(mAliroGroupResolvingKey, groupResolvingKey.Value().data(), sizeof(mAliroGroupResolvingKey));
    }

    mAliroStateInitialized = true;
    return CHIP_NO_ERROR;
}

CHIP_ERROR AliroDelegate::ClearAliroReaderConfig()
{
    int err = telink_aliro_nfc_clear_reader_config();
    VerifyOrReturnError(err == 0, CHIP_ERROR_INTERNAL,
                        ChipLogError(Zcl, "Unable to clear Aliro NFC reader configuration: %d", err));

    memset(mAliroReaderVerificationKey, 0, sizeof(mAliroReaderVerificationKey));
    memset(mAliroReaderGroupIdentifier, 0, sizeof(mAliroReaderGroupIdentifier));
    memset(mAliroReaderGroupSubIdentifier, 0, sizeof(mAliroReaderGroupSubIdentifier));
    memset(mAliroGroupResolvingKey, 0, sizeof(mAliroGroupResolvingKey));
    mAliroStateInitialized = false;
    return CHIP_NO_ERROR;
}

// ---------------------------------------------------------------------------
// Aliro credential storage
// ---------------------------------------------------------------------------

/* static */ bool AliroDelegate::IsAliroCredentialType(CredentialTypeEnum type)
{
    switch (type)
    {
    case CredentialTypeEnum::kAliroCredentialIssuerKey:
    case CredentialTypeEnum::kAliroEvictableEndpointKey:
    case CredentialTypeEnum::kAliroNonEvictableEndpointKey:
        return true;
    default:
        return false;
    }
}

/* static */ bool AliroDelegate::IsEndpointCredentialType(CredentialTypeEnum type)
{
    return type == CredentialTypeEnum::kAliroEvictableEndpointKey ||
        type == CredentialTypeEnum::kAliroNonEvictableEndpointKey;
}

AliroDelegate::CredentialSlot * AliroDelegate::FindSlot(uint16_t index, CredentialTypeEnum type)
{
    if (type == CredentialTypeEnum::kAliroCredentialIssuerKey)
    {
        return index > 0 && index <= APP_MAX_ALIRO_ISSUER_KEYS ? &mIssuerKeys[index - 1] : nullptr;
    }

    VerifyOrReturnValue(IsEndpointCredentialType(type), nullptr);
    for (auto & slot : mEndpointKeys)
    {
        if (slot.status != DlCredentialStatus::kAvailable && slot.type == type && slot.index == index)
        {
            return &slot;
        }
    }

    return nullptr;
}

const AliroDelegate::CredentialSlot * AliroDelegate::FindSlot(uint16_t index, CredentialTypeEnum type) const
{
    return const_cast<AliroDelegate *>(this)->FindSlot(index, type);
}

AliroDelegate::CredentialSlot * AliroDelegate::FindAvailableEndpointSlot()
{
    for (auto & slot : mEndpointKeys)
    {
        if (slot.status == DlCredentialStatus::kAvailable)
        {
            return &slot;
        }
    }

    return nullptr;
}

size_t AliroDelegate::SlotCountForType(CredentialTypeEnum type) const
{
    return type == CredentialTypeEnum::kAliroCredentialIssuerKey ? APP_MAX_ALIRO_ISSUER_KEYS
        : (type == CredentialTypeEnum::kAliroEvictableEndpointKey || type == CredentialTypeEnum::kAliroNonEvictableEndpointKey)
        ? APP_MAX_ALIRO_ENDPOINT_KEYS
        : 0;
}

bool AliroDelegate::GetCredential(uint16_t index, CredentialTypeEnum type, EmberAfPluginDoorLockCredentialInfo & out)
{
    VerifyOrReturnValue(index > 0 && index <= SlotCountForType(type), false);

    const CredentialSlot * slot = FindSlot(index, type);

    out.status             = slot != nullptr ? slot->status : DlCredentialStatus::kAvailable;
    out.credentialType     = type;
    out.createdBy          = slot != nullptr ? slot->createdBy : 0;
    out.lastModifiedBy     = slot != nullptr ? slot->lastModifiedBy : 0;
    out.creationSource     = DlAssetSource::kMatterIM;
    out.modificationSource = DlAssetSource::kMatterIM;
    out.credentialData     = slot != nullptr ? chip::ByteSpan{ slot->data, slot->dataSize } : chip::ByteSpan{};

    ChipLogProgress(Zcl, "AliroDelegate::GetCredential [type=%u,index=%u,status=%d]", to_underlying(type), index,
                    static_cast<int>(out.status));

    return true;
}

bool AliroDelegate::SetCredential(uint16_t index, chip::FabricIndex creator, chip::FabricIndex modifier, DlCredentialStatus status,
                                  CredentialTypeEnum type, const chip::ByteSpan & data)
{
    VerifyOrReturnValue(index > 0 && index <= SlotCountForType(type), false);
    CredentialSlot * slot = FindSlot(index, type);

    if (status == DlCredentialStatus::kAvailable)
    {
        if (slot != nullptr)
        {
            memset(slot->data, 0, sizeof(slot->data));
            *slot = CredentialSlot{};
        }
        return true;
    }

    VerifyOrReturnValue(data.size() <= kAliroCredentialMaxSize, false);

    if (slot == nullptr && IsEndpointCredentialType(type))
    {
        slot = FindAvailableEndpointSlot();
    }
    VerifyOrReturnValue(slot != nullptr, false);

    memset(slot->data, 0, sizeof(slot->data));
    memcpy(slot->data, data.data(), data.size());
    slot->dataSize       = data.size();
    slot->status         = status;
    slot->type           = type;
    slot->index          = index;
    slot->createdBy      = creator;
    slot->lastModifiedBy = modifier;

    ChipLogProgress(Zcl, "AliroDelegate::SetCredential [type=%u,index=%u,dataSize=%u]", to_underlying(type), index,
                    static_cast<unsigned int>(data.size()));

    return true;
}

bool AliroDelegate::FindEndpointKey(const chip::ByteSpan & key, CredentialTypeEnum & type, uint16_t & index) const
{
    VerifyOrReturnValue(key.size() == kAliroCredentialMaxSize, false);

    for (const auto & slot : mEndpointKeys)
    {
        if (slot.status != DlCredentialStatus::kAvailable && slot.dataSize == key.size() &&
            memcmp(slot.data, key.data(), key.size()) == 0)
        {
            type  = slot.type;
            index = slot.index;
            return true;
        }
    }

    return false;
}
