#include "deltadelfin-animation-gradient-server.h"

#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/callback.h>
#include <lib/support/logging/CHIPLogging.h>

using namespace chip::app::Clusters::DeltadelfinAnimationGradient;

void MatterDeltadelfinAnimationGradientPluginServerInitCallback()
{
    ChipLogProgress(Zcl, "%s", __func__);
}

bool emberAfDeltadelfinAnimationGradientClusterSetAnimationCallback(chip::app::CommandHandler * commandObj,
                                                                    const chip::app::ConcreteCommandPath & commandPath,
                                                                    const Commands::SetAnimation::DecodableType & commandData)
{
    ChipLogProgress(Zcl, "%s id=%u", __func__, commandData.id);

    do
    {
        uint8_t animation_count;
        auto status = Attributes::AnimationCount::Get(commandPath.mEndpointId, &animation_count);

        if (status != chip::Protocols::InteractionModel::Status::Success)
        {
            commandObj->AddStatus(commandPath, status);
            ChipLogError(Zcl, "Get AnimationCount failed: %u", static_cast<unsigned>(status));
            break;
        }

        if (commandData.id >= animation_count)
        {
            commandObj->AddStatus(commandPath, chip::Protocols::InteractionModel::Status::ConstraintError);
            ChipLogError(Zcl, "Set CurrentAnimation out of range");
            break;
        }

        status = Attributes::CurrentAnimation::Set(commandPath.mEndpointId, commandData.id);

        if (status != chip::Protocols::InteractionModel::Status::Success)
        {
            commandObj->AddStatus(commandPath, status);
            ChipLogError(Zcl, "Set CurrentAnimation failed: %u", static_cast<unsigned>(status));
            break;
        }

        commandObj->AddStatus(commandPath, chip::Protocols::InteractionModel::Status::Success);
    } while (0);
    return true;
}

bool emberAfDeltadelfinAnimationGradientClusterSetGradientCallback(chip::app::CommandHandler * commandObj,
                                                                   const chip::app::ConcreteCommandPath & commandPath,
                                                                   const Commands::SetGradient::DecodableType & commandData)
{
    ChipLogProgress(Zcl, "%s id=%u transitionTimeMs=%u", __func__, commandData.id, commandData.transitionTimeMs);

    do
    {
        uint8_t gradient_count;
        auto status = Attributes::GradientCount::Get(commandPath.mEndpointId, &gradient_count);

        if (status != chip::Protocols::InteractionModel::Status::Success)
        {
            commandObj->AddStatus(commandPath, status);
            ChipLogError(Zcl, "Get GradientCount failed: %u", static_cast<unsigned>(status));
            break;
        }

        if (commandData.id >= gradient_count)
        {
            commandObj->AddStatus(commandPath, chip::Protocols::InteractionModel::Status::ConstraintError);
            ChipLogError(Zcl, "Set CurrentGradient out of range");
            break;
        }

        status = Attributes::CurrentGradient::Set(commandPath.mEndpointId, commandData.id);

        if (status != chip::Protocols::InteractionModel::Status::Success)
        {
            commandObj->AddStatus(commandPath, status);
            ChipLogError(Zcl, "Set CurrentGradient failed: %u", static_cast<unsigned>(status));
            break;
        }

        (void) Attributes::TransitionTimeMs::Set(commandPath.mEndpointId, commandData.transitionTimeMs);

        commandObj->AddStatus(commandPath, chip::Protocols::InteractionModel::Status::Success);
    } while (0);

    return true;
}

bool emberAfDeltadelfinAnimationGradientClusterSetDisplayModeCallback(chip::app::CommandHandler * commandObj,
                                                                      const chip::app::ConcreteCommandPath & commandPath,
                                                                      const Commands::SetDisplayMode::DecodableType & commandData)
{
    ChipLogProgress(Zcl, "%s mode=%u", __func__, static_cast<unsigned>(commandData.mode));

    do
    {
        if (commandData.mode >= DisplayModeEnum::kUnknownEnumValue)
        {
            commandObj->AddStatus(commandPath, chip::Protocols::InteractionModel::Status::ConstraintError);
            ChipLogError(Zcl, "Set DisplayMode out of range");
            break;
        }

        auto status = Attributes::DisplayMode::Set(commandPath.mEndpointId, commandData.mode);

        if (status != chip::Protocols::InteractionModel::Status::Success)
        {
            commandObj->AddStatus(commandPath, status);
            ChipLogError(Zcl, "Set DisplayMode failed: %u", static_cast<unsigned>(status));
            break;
        }

        commandObj->AddStatus(commandPath, chip::Protocols::InteractionModel::Status::Success);
    } while (0);
    return true;
}
