#include "CalibrationHelper.hpp"

#include <UnityEngine/Time.hpp>
#include <UnityEngine/WaitForEndOfFrame.hpp>
#include <chatplex-sdk-bs/shared/CP_SDK/Unity/MTCoroutineStarter.hpp>

#include "GlobalNamespace/OVRPlugin.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "utils/structs/OffsetPose.hpp"
#include <algorithm>
#include "AvatarManager.hpp"
#include "HandController.hpp"
#include "TPoseHelper.hpp"
#include "config/ConfigManager.hpp"
#include "customTypes/VRMSpringBone.hpp"

namespace VRMQavatars
{
    float GetArmScale();

    void CalibrationHelper::PrepareContextForCalibration()
    {
        auto rootGameObject = AvatarManager::currentContext->rootGameObject->get_transform();

        for (const auto springBone : rootGameObject->GetComponentsInChildren<VRMSpringBone*>())
            springBone->set_enabled(false);

        const auto vrik = rootGameObject->GetComponent<FinalIK::VRIK*>();
        vrik->set_enabled(false);

        rootGameObject->set_position({0.0f, 0.0f, 0.0f});
        rootGameObject->set_eulerAngles({0.0f, 0.0f, 0.0f});
        rootGameObject->set_localScale({1.0f, 1.0f, 1.0f});

        vrik->solver->Reset();
        vrik->solver->FixTransforms();

        TPoseHelper::LoadPose();

        auto armScale = GetArmScale();
        auto legScale = Config::ConfigManager::GetAvatarConfig().LegCalibrationScale.GetValue();

        auto hips = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::Hips);
        auto LLowerArm = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::LeftLowerArm);
        auto RLowerArm = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::RightLowerArm);
        auto LHand = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::LeftHand);
        auto RHand = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::RightHand);
        auto LLowerLeg = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::LeftLowerLeg);
        auto RLowerLeg = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::RightLowerLeg);
        auto LFoot = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::LeftFoot);
        auto RFoot = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::RightFoot);

        // lengthen the arms by moving the elbow and wrist joints instead of scaling the bones,
        // so the arms keep their thickness and the hands keep their size
        for (auto joint : {LLowerArm, RLowerArm, LHand, RHand})
            joint->set_localPosition(Sombrero::FastVector3(joint->get_localPosition()) * armScale);

        // lengthen the legs by moving the knee and ankle joints instead of scaling the bones,
        // so the legs keep their thickness and the feet keep their size
        const float footHeight = LFoot->get_position().y;
        for (auto joint : {LLowerLeg, RLowerLeg, LFoot, RFoot})
            joint->set_localPosition(Sombrero::FastVector3(joint->get_localPosition()) * legScale);

        // raise the hips so the feet stay on the ground
        hips->set_position(Sombrero::FastVector3(hips->get_position()) + Sombrero::FastVector3(0.0f, footHeight - LFoot->get_position().y, 0.0f));
    }

    Sombrero::FastVector3 CalibrationHelper::GetPosition(const GlobalNamespace::OVRPlugin::Node node)
    {
        const auto pose = GlobalNamespace::OVRPlugin::GetNodePose(node, GlobalNamespace::OVRPlugin::Step::Render);
        const auto pos = Sombrero::FastVector3(pose.Position.x, pose.Position.y, -pose.Position.z);
        return pos;
    }

    float GetAvatarHandHeight()
    {
        const auto vrik = AvatarManager::vrik;
        auto avatarRightHandPos = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::RightHand)->get_position();
        auto avatarLeftHandPos = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::LeftHand)->get_position();
        const float avatarHandAverageY = (avatarLeftHandPos.y + avatarRightHandPos.y) / 2.0f;
        return avatarHandAverageY;
    }

    // arm scale applied to the avatar: the manual setting, times the measured one when matching eye height
    float GetArmScale()
    {
        float scale = Config::ConfigManager::GetAvatarConfig().ArmCalibrationScale.GetValue();
        if(Config::ConfigManager::GetGlobalConfig().CalibrationType.GetValue() == 3)
            scale *= Config::ConfigManager::GetAvatarConfig().AutoArmScale.GetValue();
        if(Config::ConfigManager::GetGlobalConfig().UncannyLongArmsFunnyUnderscoresReference.GetValue())
            scale *= 5.0f;
        return scale;
    }

    // where the hand targets (avatar wrists) are for the current controller poses, see TargetManager::Update
    Sombrero::FastVector3 GetPlayerWristPosition(const bool left)
    {
        const Structs::OffsetPose offset = Config::ConfigManager::GetOffsetSettings().handOffset;
        const auto pose = GlobalNamespace::OVRPlugin::GetNodePose(left ? GlobalNamespace::OVRPlugin::Node::HandLeft : GlobalNamespace::OVRPlugin::Node::HandRight, GlobalNamespace::OVRPlugin::Step::Render);
        const Sombrero::FastVector3 pos = {pose.Position.x, pose.Position.y, -pose.Position.z};
        const Sombrero::FastQuaternion rot = {-pose.Orientation.x, -pose.Orientation.y, pose.Orientation.z, pose.Orientation.w};
        const Sombrero::FastQuaternion offsetRot = rot * Sombrero::FastQuaternion(UnityEngine::Quaternion::Euler(offset.rotX, offset.rotY, left ? offset.rotZ : -offset.rotZ));
        return pos + offsetRot * Sombrero::FastVector3(left ? offset.posX : -offset.posX, offset.posY, offset.posZ);
    }

    // arm scale so the avatar's T-pose wrist span matches the player's, the shoulder width is left as is
    // nothing when the player's arms clearly aren't spread out
    std::optional<float> MeasureArmScale()
    {
        const auto animator = AvatarManager::vrik->animator;
        const Sombrero::FastVector3 leftShoulder = animator->GetBoneTransform(UnityEngine::HumanBodyBones::LeftUpperArm)->get_position();
        const Sombrero::FastVector3 rightShoulder = animator->GetBoneTransform(UnityEngine::HumanBodyBones::RightUpperArm)->get_position();
        const Sombrero::FastVector3 leftHand = animator->GetBoneTransform(UnityEngine::HumanBodyBones::LeftHand)->get_position();
        const Sombrero::FastVector3 rightHand = animator->GetBoneTransform(UnityEngine::HumanBodyBones::RightHand)->get_position();

        const float shoulderWidth = UnityEngine::Vector3::Distance(leftShoulder, rightShoulder);
        const float avatarArmLength = (UnityEngine::Vector3::Distance(leftShoulder, leftHand) + UnityEngine::Vector3::Distance(rightShoulder, rightHand)) / 2.0f;
        const float playerSpan = UnityEngine::Vector3::Distance(GetPlayerWristPosition(true), GetPlayerWristPosition(false));
        const float playerArmLength = (playerSpan - shoulderWidth) / 2.0f;

        VRMLogger.info("arm calibration: player span {} avatar shoulder width {} avatar arm {} player arm {}", playerSpan, shoulderWidth, avatarArmLength, playerArmLength);

        // a T-pose wrist span is around 0.85x the eye height
        const float eyeHeight = CalibrationHelper::GetPosition(GlobalNamespace::OVRPlugin::Node::Head).y;
        if(playerSpan < eyeHeight * 0.6f || avatarArmLength <= 0.01f || playerArmLength <= 0.01f)
        {
            VRMLogger.info("arms not spread out, keeping the previous arm scale");
            return std::nullopt;
        }
        return playerArmLength / avatarArmLength;
    }

    float GetAvatarEyeHeight()
    {
        const auto vrik = AvatarManager::vrik;
        auto leftEye = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::LeftEye);
        auto rightEye = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::RightEye);
        if(leftEye && rightEye)
            return (leftEye->get_position().y + rightEye->get_position().y) / 2.0f;
        // default VRM first person offset
        return vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::Head)->get_position().y + 0.06f;
    }

    float GetAvatarHandDist()
    {
        const auto vrik = AvatarManager::vrik;
        auto avatarRightHandPos = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::RightHand)->get_position();
        auto avatarLeftHandPos = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::LeftHand)->get_position();
        const float avatarHandDist = UnityEngine::Vector3::Distance(avatarLeftHandPos, avatarRightHandPos);
        return avatarHandDist;
    }

    float GetBase()
    {
        const auto type = Config::ConfigManager::GetGlobalConfig().CalibrationType.GetValue();
        if(type == 0)
        {
            return GetAvatarHandDist();
        }
        if(type == 1)
        {
            return GetAvatarHandHeight();
        }
        if(type == 3)
        {
            return GetAvatarEyeHeight();
        }
        return 1.0f;
    }

    float GetCalibrateScale(const std::optional<float> baseScale)
    {
        VRMLogger.info("scale: {} {}", baseScale.has_value(), baseScale.has_value() ? baseScale.value() : 0.0f);
        const auto type = Config::ConfigManager::GetGlobalConfig().CalibrationType.GetValue();
        if(type == 0)
        {
            const auto leftHandPos = CalibrationHelper::GetPosition(GlobalNamespace::OVRPlugin::Node::HandLeft);
            const auto rightHandPos = CalibrationHelper::GetPosition(GlobalNamespace::OVRPlugin::Node::HandRight);
            const float avatarHandDist = baseScale.has_value() ? baseScale.value() : GetAvatarHandDist();
            const float readDist = UnityEngine::Vector3::Distance(leftHandPos, rightHandPos);
            const float scale = readDist / avatarHandDist;
            VRMLogger.info("{} {} {}", readDist, avatarHandDist, scale);
            return scale * 0.85f;
        }
        if(type == 1)
        {
            const auto leftHandPos = CalibrationHelper::GetPosition(GlobalNamespace::OVRPlugin::Node::HandLeft);
            const auto rightHandPos = CalibrationHelper::GetPosition(GlobalNamespace::OVRPlugin::Node::HandRight);
            const float avatarHandAverageY = baseScale.has_value() ? baseScale.value() : GetAvatarHandHeight();
            const float readHandAverageY = (leftHandPos.y + rightHandPos.y) / 2.0f;
            const float scale = readHandAverageY / avatarHandAverageY;
            VRMLogger.info("{} {} {}", readHandAverageY, avatarHandAverageY, scale);
            return scale;
        }
        if(type == 3)
        {
            // the headset sits at the eyes, so match the avatar's eye height to it
            const float avatarEyeHeight = baseScale.has_value() ? baseScale.value() : GetAvatarEyeHeight();
            const float readEyeHeight = CalibrationHelper::GetPosition(GlobalNamespace::OVRPlugin::Node::Head).y;
            const float scale = readEyeHeight / avatarEyeHeight;
            VRMLogger.info("{} {} {}", readEyeHeight, avatarEyeHeight, scale);
            return scale;
        }
        return Config::ConfigManager::GetGlobalConfig().FixedScale.GetValue();
    }

    void CalibrationHelper::Calibrate(const std::optional<float> scale)
    {
        auto rootGameObject = AvatarManager::currentContext->rootGameObject->get_transform();

        const auto vrik = AvatarManager::vrik;
        const auto targetManager = AvatarManager::targetManager;

        PrepareContextForCalibration();

        const float calibScale = scale.has_value() ? scale.value() : GetCalibrateScale(std::nullopt);
        VRMLogger.info("calibration scale {}", calibScale);

        auto& avaConfig = Config::ConfigManager::GetAvatarConfig();
        avaConfig.HasCalibrated.SetValue(true);
        avaConfig.CalibratedScale.SetValue(calibScale);

        rootGameObject->set_localScale(UnityEngine::Vector3(calibScale, calibScale, calibScale));

        // when calibrating live by eye height, also match the arm length to the player's T-pose
        if(!scale.has_value() && Config::ConfigManager::GetGlobalConfig().CalibrationType.GetValue() == 3)
        {
            if(const auto armScale = MeasureArmScale())
            {
                // the measured arms already include the current auto scale
                const float autoArmScale = std::clamp(avaConfig.AutoArmScale.GetValue() * armScale.value(), 0.5f, 2.0f);
                VRMLogger.info("auto arm scale {}", autoArmScale);
                avaConfig.AutoArmScale.SetValue(autoArmScale);

                PrepareContextForCalibration();
                rootGameObject->set_localScale(UnityEngine::Vector3(calibScale, calibScale, calibScale));
            }
        }

        // the headset is at the eyes, not the head bone, so measure where the eyes are relative to the head bone
        auto head = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::Head);
        auto leftEye = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::LeftEye);
        auto rightEye = vrik->animator->GetBoneTransform(UnityEngine::HumanBodyBones::RightEye);
        const Sombrero::FastQuaternion invHeadRot = UnityEngine::Quaternion::Inverse(head->get_rotation());
        if(leftEye && rightEye)
        {
            const auto eyes = Sombrero::FastVector3::Lerp(leftEye->get_position(), rightEye->get_position(), 0.5f);
            targetManager->headToEyesOffset = invHeadRot * (eyes - Sombrero::FastVector3(head->get_position()));
        }
        else
        {
            // default VRM first person offset
            targetManager->headToEyesOffset = Sombrero::FastVector3(0.0f, 0.06f, 0.0f) * calibScale;
        }
        VRMLogger.info("head to eyes offset ({}, {}, {})", targetManager->headToEyesOffset.x, targetManager->headToEyesOffset.y, targetManager->headToEyesOffset.z);

        HandController::ApplyHandPose(vrik->animator, Config::ConfigManager::GetFingerPoseSettings().grabPose);

        vrik->solver->spine->headTarget = targetManager->headTarget->get_transform();
        vrik->solver->leftArm->target = targetManager->leftHandTarget->get_transform();
        vrik->solver->rightArm->target = targetManager->rightHandTarget->get_transform();

        vrik->Initiate();
        // Initiate only runs once per avatar, but FixTransforms restores the stored pose every frame,
        // so store it again or recalibrated arm/leg lengths get reset to the ones from when the avatar was loaded
        vrik->solver->StoreDefaultLocalState();
        targetManager->intialized = true;
        vrik->set_enabled(true);

        for (const auto springBone : rootGameObject->GetComponentsInChildren<VRMSpringBone*>())
            springBone->set_enabled(true);
    }

    void CalibrationHelper::StartCalibration()
    {
        if(calibrating || AvatarManager::currentContext == nullptr)
            return;
        CP_SDK::Unity::MTCoroutineStarter::Start(custom_types::Helpers::CoroutineHelper::New(StartCalibrationProc()));
    }

    custom_types::Helpers::Coroutine CalibrationHelper::StartCalibrationProc()
    {
        calibrating = true;
        float time = 0.0f;
        const auto context = AvatarManager::currentContext;
        auto rootGameObject = context->rootGameObject->get_transform();

        PrepareContextForCalibration();

        auto baseScale = GetBase();

        while(time < 4.0f)
        {
            time += UnityEngine::Time::get_deltaTime();

            auto headPos = GetPosition(GlobalNamespace::OVRPlugin::Node::Head);
            const auto leftHandPos = GetPosition(GlobalNamespace::OVRPlugin::Node::HandLeft);
            const auto rightHandPos = GetPosition(GlobalNamespace::OVRPlugin::Node::HandRight);

            headPos.y = 0.0f;

            const float yRotation = Sombrero::FastQuaternion::LookRotation(leftHandPos - rightHandPos, Sombrero::FastVector3::forward()).get_eulerAngles().y + 90.0f;

            rootGameObject->set_position(headPos);
            rootGameObject->set_rotation(UnityEngine::Quaternion::Euler(0.0f, yRotation, 0.0f));
            const float scale = GetCalibrateScale(baseScale);
            rootGameObject->set_localScale(Sombrero::FastVector3(scale, scale, scale));

            co_yield reinterpret_cast<System::Collections::IEnumerator*>(CRASH_UNLESS(UnityEngine::WaitForEndOfFrame::New_ctor()));

            // the avatar was switched during calibration, its objects are gone
            if(AvatarManager::currentContext != context)
            {
                calibrating = false;
                co_return;
            }
        }
        Calibrate(std::nullopt);
        calibrating = false;
        co_return;
    }
}
