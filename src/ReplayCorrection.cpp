#include "ReplayCorrection.hpp"

#include "main.hpp"

#include "config/ConfigManager.hpp"

#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Vector3.hpp"

namespace VRMQavatars
{
    namespace
    {
        // samples are only taken while the tracked part is nearly still,
        // since the raw and game poses can be sampled at slightly different times
        constexpr float kMaxSpeed = 0.3f;
        constexpr float kMaxAngularSpeed = 45.0f;
        constexpr float kSmoothing = 0.05f;
        constexpr int kRequiredSamples = 90;

        struct PartState
        {
            bool valid = false;
            int samples = 0;
            Sombrero::FastVector3 pos;
            Sombrero::FastQuaternion rot = Sombrero::FastQuaternion::identity();

            float lastTime = -1.0f;
            Sombrero::FastVector3 lastRawPos;
            Sombrero::FastQuaternion lastRawRot = Sombrero::FastQuaternion::identity();
        };

        PartState states[ReplayCorrection::PartCount];
        bool loaded = false;
        bool dirty = false;

        GlobalNamespace::OVRPlugin::Node GetNode(const ReplayCorrection::Part part)
        {
            return part == ReplayCorrection::LeftHand ? GlobalNamespace::OVRPlugin::Node::HandLeft : GlobalNamespace::OVRPlugin::Node::HandRight;
        }

        void LoadPart(PartState& state, const bool measured, UnityEngine::Vector3 pos, UnityEngine::Vector3 rot)
        {
            state.valid = measured;
            state.samples = measured ? kRequiredSamples : 0;
            state.pos = pos;
            state.rot = UnityEngine::Quaternion::Euler(rot);
        }

        void Load()
        {
            if(loaded)
                return;
            loaded = true;

            auto settings = Config::ConfigManager::GetGlobalConfig().ReplayCorrection.GetValue();
            LoadPart(states[ReplayCorrection::LeftHand], settings.handsMeasured, settings.leftPos, settings.leftRot);
            LoadPart(states[ReplayCorrection::RightHand], settings.handsMeasured, settings.rightPos, settings.rightRot);
        }

        bool IsReady(const PartState& state)
        {
            return state.valid && state.samples >= kRequiredSamples;
        }
    }

    void ReplayCorrection::GetTrackedPose(const GlobalNamespace::OVRPlugin::Node node, Sombrero::FastVector3& pos, Sombrero::FastQuaternion& rot)
    {
        const auto pose = GlobalNamespace::OVRPlugin::GetNodePose(node, GlobalNamespace::OVRPlugin::Step::Render);
        pos = {pose.Position.x, pose.Position.y, -pose.Position.z};
        rot = {-pose.Orientation.x, -pose.Orientation.y, pose.Orientation.z, pose.Orientation.w};
    }

    void ReplayCorrection::Sample(const Part part, const Sombrero::FastVector3& gamePos, const Sombrero::FastQuaternion& gameRot)
    {
        Load();
        auto& state = states[part];

        Sombrero::FastVector3 rawPos;
        Sombrero::FastQuaternion rawRot;
        GetTrackedPose(GetNode(part), rawPos, rawRot);

        const float now = UnityEngine::Time::get_unscaledTime();
        const float dt = now - state.lastTime;
        bool still = false;
        if(state.lastTime >= 0.0f && dt > 0.0f && dt < 0.1f)
        {
            const float speed = UnityEngine::Vector3::Distance(rawPos, state.lastRawPos) / dt;
            const float angularSpeed = UnityEngine::Quaternion::Angle(rawRot, state.lastRawRot) / dt;
            still = speed < kMaxSpeed && angularSpeed < kMaxAngularSpeed;
        }
        state.lastTime = now;
        state.lastRawPos = rawPos;
        state.lastRawRot = rawRot;

        if(!still)
            return;

        // game pose = raw pose * delta, so delta = inverse(raw) * game
        const Sombrero::FastQuaternion invRawRot = UnityEngine::Quaternion::Inverse(rawRot);
        const Sombrero::FastVector3 deltaPos = invRawRot * (gamePos - rawPos);
        const Sombrero::FastQuaternion deltaRot = invRawRot * gameRot;

        if(!state.valid)
        {
            state.valid = true;
            state.pos = deltaPos;
            state.rot = deltaRot;
        }
        else
        {
            state.pos = Sombrero::FastVector3::Lerp(state.pos, deltaPos, kSmoothing);
            state.rot = UnityEngine::Quaternion::Slerp(state.rot, deltaRot, kSmoothing);
        }

        if(state.samples < kRequiredSamples)
            state.samples++;
        if(IsReady(state))
            dirty = true;
    }

    void ReplayCorrection::Apply(const Part part, Sombrero::FastVector3& pos, Sombrero::FastQuaternion& rot)
    {
        Load();
        const auto& state = states[part];
        if(!IsReady(state))
            return;

        // raw = game * inverse(delta)
        const Sombrero::FastQuaternion rawRot = rot * UnityEngine::Quaternion::Inverse(state.rot);
        pos = pos - rawRot * state.pos;
        rot = rawRot;
    }

    void ReplayCorrection::SaveIfDirty()
    {
        if(!dirty)
            return;
        dirty = false;

        auto& config = Config::ConfigManager::GetGlobalConfig().ReplayCorrection;
        auto settings = config.GetValue();

        const auto& left = states[LeftHand];
        const auto& right = states[RightHand];
        if(!IsReady(left) || !IsReady(right))
            return;

        settings.handsMeasured = true;
        settings.leftPos = UnityEngine::Vector3(left.pos);
        settings.leftRot = Sombrero::FastQuaternion(left.rot).get_eulerAngles();
        settings.rightPos = UnityEngine::Vector3(right.pos);
        settings.rightRot = Sombrero::FastQuaternion(right.rot).get_eulerAngles();

        VRMLogger.info("Saving replay correction: left ({}, {}, {}) right ({}, {}, {})",
            left.pos.x, left.pos.y, left.pos.z,
            right.pos.x, right.pos.y, right.pos.z);

        config.SetValue(settings);
    }
}
