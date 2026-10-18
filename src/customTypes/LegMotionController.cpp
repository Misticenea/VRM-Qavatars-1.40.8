#include "customTypes/LegMotionController.hpp"

#include <algorithm>
#include <cmath>

#include "AvatarManager.hpp"
#include "BeatTracker.hpp"
#include "SceneEventManager.hpp"

#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"

#include "sombrero/shared/FastVector3.hpp"

namespace VRMQavatars
{
    CP_SDK_IL2CPP_INHERIT_INIT(LegMotionController);

    namespace
    {
        constexpr float PI = 3.14159265f;

        enum Feet
        {
            Left = 1,
            Right = 2,
            Both = 3
        };

        enum Envelope
        {
            Hold,  // follows the input while it is held
            Swing, // one shot, out and back
            Stomp  // one shot, slow lift and fast drop
        };

        struct ActionDef
        {
            int feet;
            Envelope envelope;
            float duration;
            LegMotionController::FootOffset offset;
        };

        // same order as LegMotionController::actionNames, offsets in meters
        const std::vector<ActionDef> actions = {
            { 0, Hold, 0.0f, {} },
            { Left, Hold, 0.0f, { 0.0f, 0.32f, 0.12f, 0.0f } },
            { Right, Hold, 0.0f, { 0.0f, 0.32f, 0.12f, 0.0f } },
            { Left, Swing, 0.5f, { 0.0f, 0.30f, 0.42f, 0.0f } },
            { Right, Swing, 0.5f, { 0.0f, 0.30f, 0.42f, 0.0f } },
            { Left, Stomp, 0.4f, { 0.0f, 0.22f, 0.04f, 0.0f } },
            { Right, Stomp, 0.4f, { 0.0f, 0.22f, 0.04f, 0.0f } },
            { Both, Hold, 0.0f, { 0.0f, 0.0f, 0.0f, 0.07f } },
            { Both, Hold, 0.0f, { 0.0f, 0.25f, -0.04f, 0.0f } },
            { Right, Hold, 0.0f, { -0.22f, 0.02f, 0.10f, 0.0f } },
            { Both, Hold, 0.0f, { 0.15f, 0.0f, 0.0f, 0.0f } },
        };

        float SmoothStep(const float x)
        {
            return x * x * (3.0f - 2.0f * x);
        }

        float Lerp(const float a, const float b, const float t)
        {
            return a + (b - a) * t;
        }

        float MoveTowards(const float current, const float target, const float maxDelta)
        {
            if(std::abs(target - current) <= maxDelta)
                return target;
            return current + (target > current ? maxDelta : -maxDelta);
        }

        void AddOffset(LegMotionController::FootOffset& foot, const LegMotionController::FootOffset& offset, const float weight)
        {
            foot.side += offset.side * weight;
            foot.up += offset.up * weight;
            foot.forward += offset.forward * weight;
            foot.heel += offset.heel * weight;
        }

        int ActionIndex(const std::string& name)
        {
            for (int i = 1; i < LegMotionController::actionNames.size(); ++i)
            {
                if(LegMotionController::actionNames[i] == name)
                    return i;
            }
            return 0;
        }
    }

    const std::vector<std::string> LegMotionController::actionNames = {
        "None",
        "Left Knee Up",
        "Right Knee Up",
        "Left Kick",
        "Right Kick",
        "Left Stomp",
        "Right Stomp",
        "Tiptoe",
        "Tuck Jump",
        "Cross Legs",
        "Wide Stance"
    };

    const std::vector<std::u16string> LegMotionController::beatStyleNames = {
        u"Tap",
        u"Bounce",
        u"Side Step"
    };

    CP_SDK_IL2CPP_DECLARE_CTOR_IMPL(LegMotionController)
    {

    }

    CP_SDK_IL2CPP_DECLARE_DTOR_MONOBEHAVIOUR_IMPL(LegMotionController)
    {

    }

    void LegMotionController::Reload()
    {
        if(AvatarManager::currentContext == nullptr) return;
        const auto controller = AvatarManager::currentContext->rootGameObject->GetComponent<LegMotionController*>();
        if(controller != nullptr)
            controller->LoadSettings();
    }

    void LegMotionController::LoadSettings()
    {
        using Button = GlobalNamespace::OVRInput::Button;
        using Controller = GlobalNamespace::OVRInput::Controller;

        settings = Config::ConfigManager::GetLegMotionSettings();
        const auto triggers = Config::ConfigManager::GetLegTriggerSettings();

        const std::vector<std::tuple<std::string, Button, Controller>> inputs = {
            { triggers.A, Button::One, Controller::RTouch },
            { triggers.B, Button::Two, Controller::RTouch },
            { triggers.X, Button::One, Controller::LTouch },
            { triggers.Y, Button::Two, Controller::LTouch },
            { triggers.LGrip, Button::PrimaryHandTrigger, Controller::LTouch },
            { triggers.RGrip, Button::PrimaryHandTrigger, Controller::RTouch },
            { triggers.LTrigger, Button::PrimaryIndexTrigger, Controller::LTouch },
            { triggers.RTrigger, Button::PrimaryIndexTrigger, Controller::RTouch },
            { triggers.LStickUp, Button::PrimaryThumbstickUp, Controller::LTouch },
            { triggers.LStickDown, Button::PrimaryThumbstickDown, Controller::LTouch },
            { triggers.LStickLeft, Button::PrimaryThumbstickLeft, Controller::LTouch },
            { triggers.LStickRight, Button::PrimaryThumbstickRight, Controller::LTouch },
            { triggers.RStickUp, Button::PrimaryThumbstickUp, Controller::RTouch },
            { triggers.RStickDown, Button::PrimaryThumbstickDown, Controller::RTouch },
            { triggers.RStickLeft, Button::PrimaryThumbstickLeft, Controller::RTouch },
            { triggers.RStickRight, Button::PrimaryThumbstickRight, Controller::RTouch },
        };

        bindings.clear();
        for (const auto& [name, button, controller] : inputs)
        {
            const int action = ActionIndex(name);
            if(action > 0)
                bindings.push_back({ button, controller, action });
        }

        actionStates.assign(actionNames.size(), {});
    }

    void LegMotionController::UpdateActions(const float dt, FootOffset* feet, float* busy)
    {
        if(actionStates.size() != actionNames.size()) return;

        std::vector<bool> down(actionNames.size(), false);
        for (const auto& binding : bindings)
        {
            if(GlobalNamespace::OVRInput::Get(binding.button, binding.controller))
                down[binding.action] = true;
        }

        const float intensity = settings.actionIntensity;
        for (int i = 1; i < actions.size(); ++i)
        {
            const auto& def = actions[i];
            auto& state = actionStates[i];

            float weight = 0.0f;
            if(def.envelope == Hold)
            {
                state.weight = MoveTowards(state.weight, down[i] ? 1.0f : 0.0f, dt * 6.0f);
                weight = SmoothStep(state.weight);
            }
            else
            {
                // one shots start on the press and play to the end
                if(down[i] && !state.wasDown && state.time < 0.0f)
                    state.time = 0.0f;
                if(state.time >= 0.0f)
                {
                    state.time += dt;
                    const float x = state.time / def.duration;
                    if(x >= 1.0f)
                        state.time = -1.0f;
                    else if(def.envelope == Swing)
                        weight = std::sin(PI * x);
                    else
                        weight = x < 0.65f ? std::sin(PI * 0.5f * x / 0.65f) : std::cos(PI * 0.5f * (x - 0.65f) / 0.35f);
                }
            }
            state.wasDown = down[i];

            if(weight <= 0.0f) continue;
            for (int foot = 0; foot < 2; ++foot)
            {
                if(!(def.feet & (1 << foot))) continue;
                AddOffset(feet[foot], def.offset, weight * intensity);
                busy[foot] = std::max(busy[foot], weight);
            }
        }
    }

    void LegMotionController::UpdateBeat(const float dt, FootOffset* feet, const float* busy)
    {
        const bool active = settings.beatEnabled && SceneEventManager::inGame && BeatTracker::IsPlaying();
        beatWeight = MoveTowards(beatWeight, active ? 1.0f : 0.0f, dt * 3.0f);
        if(beatWeight <= 0.0f) return;

        // one step per beatsPerStep beats, alternating the feet and landing on the beat
        const float steps = BeatTracker::beat / static_cast<float>(std::max(1, settings.beatsPerStep));
        const float stepIndex = std::floor(steps);
        const float phase = steps - stepIndex;
        const int foot = static_cast<int>(std::fmod(std::abs(stepIndex), 2.0f));
        const float intensity = settings.beatIntensity * beatWeight;

        switch (settings.beatStyle)
        {
            default:
            case 0: // tap
            {
                const float weight = std::sin(PI * phase) * intensity * (1.0f - busy[foot]);
                AddOffset(feet[foot], { 0.0f, 0.06f, 0.03f, 0.0f }, weight);
                break;
            }
            case 1: // bounce on the toes
            {
                const float weight = std::sin(PI * phase) * intensity;
                for (int i = 0; i < 2; ++i)
                    AddOffset(feet[i], { 0.0f, 0.0f, 0.0f, 0.05f }, weight * (1.0f - busy[i]));
                break;
            }
            case 2: // side step, out with the foot lifted, touch down at the side and back
            {
                const float weight = intensity * (1.0f - busy[foot]);
                feet[foot].side += 0.12f * std::sin(PI * phase) * weight;
                feet[foot].up += 0.04f * std::abs(std::sin(2.0f * PI * phase)) * weight;
                break;
            }
        }
    }

    void LegMotionController::UpdateJump(const float dt, FootOffset* feet)
    {
        const auto headTarget = vrik->solver->spine->headTarget;
        const auto root = vrik->references->root;
        if(!settings.jumpEnabled || headTarget == nullptr || dt <= 0.0f)
        {
            airborne = false;
            hasHeadHeight = false;
            jumpLift = MoveTowards(jumpLift, 0.0f, dt * 2.0f);
        }
        else
        {
            // measure the head in tracking space, noodle player tracks move the head with them
            // and the avatar root only follows in steps
            float baseY = root->get_position().y;
            if(SceneEventManager::inGame && targetManager != nullptr && targetManager->noodleTrack)
                baseY = targetManager->noodleTrack->get_transform()->get_position().y;

            const float height = headTarget->get_position().y - baseY;
            if(!hasHeadHeight)
            {
                hasHeadHeight = true;
                lastHeadHeight = height;
                standHeight = height;
                headVelocity = 0.0f;
            }
            else if(baseY != lastBaseY)
            {
                // the base moved, not the head
                lastHeadHeight -= baseY - lastBaseY;
                standHeight -= baseY - lastBaseY;
            }
            lastBaseY = baseY;
            headVelocity = Lerp(headVelocity, (height - lastHeadHeight) / dt, std::min(1.0f, dt * 20.0f));
            lastHeadHeight = height;

            float targetLift = 0.0f;
            if(!airborne)
            {
                if(headVelocity > settings.jumpVelocity && height > standHeight + 0.03f)
                {
                    airborne = true;
                    airTime = 0.0f;
                }
                else if(headVelocity < 0.5f)
                {
                    // the standing height follows the highest recent head height, but stays put while crouching
                    // so standing up quickly from a crouch is not taken as a jump
                    if(height > standHeight)
                        standHeight = height;
                    else if(height > standHeight - 0.1f)
                        standHeight = std::max(height, standHeight - 0.05f * dt);

                    lowTime = height < standHeight - 0.1f ? lowTime + dt : 0.0f;
                    if(lowTime > 5.0f)
                    {
                        // the player is seated or the headset was recentered
                        standHeight = height;
                        lowTime = 0.0f;
                    }
                }
            }
            if(airborne)
            {
                airTime += dt;
                targetLift = std::max(0.0f, height - standHeight);
                if((height <= standHeight + 0.01f && headVelocity <= 0.0f) || airTime > 1.5f)
                {
                    if(airTime > 1.5f)
                        standHeight = height;
                    airborne = false;
                    targetLift = 0.0f;
                }
            }
            jumpLift = Lerp(jumpLift, targetLift, std::min(1.0f, dt * (airborne ? 30.0f : 15.0f)));
        }

        if(jumpLift <= 0.001f) return;

        // lift the feet with the head so the legs leave the ground instead of stretching, tuck adds a knee bend
        const float tuck = settings.jumpTuck;
        for (int i = 0; i < 2; ++i)
        {
            feet[i].up += jumpLift * (1.0f + tuck);
            feet[i].forward -= jumpLift * tuck * 0.3f;
        }
    }

    void LegMotionController::Update()
    {
        if(vrik == nullptr || !vrik->get_enabled()) return;
        const auto solver = vrik->solver;
        const auto root = vrik->references->root;
        if(!solver->initiated || !solver->hasLegs || root == nullptr) return;

        const float dt = UnityEngine::Time::get_deltaTime();

        FootOffset feet[2];
        float busy[2] = { 0.0f, 0.0f };

        UpdateActions(dt, feet, busy);
        UpdateBeat(dt, feet, busy);
        UpdateJump(dt, feet);

        const Sombrero::FastVector3 right = root->get_right();
        const Sombrero::FastVector3 up = root->get_up();
        const Sombrero::FastVector3 forward = root->get_forward();

        FinalIK::Leg* legs[2] = { solver->leftLeg, solver->rightLeg };
        for (int i = 0; i < 2; ++i)
        {
            const auto leg = legs[i];
            // feet driven by trackers are left alone
            if(leg->positionWeight > 0.0f) continue;

            const auto& foot = feet[i];
            const float outwards = i == 0 ? -1.0f : 1.0f;

            // the solver resets its offsets after solving, locomotion adds to them, so add ours too
            leg->footPositionOffset += right * (foot.side * outwards) + up * foot.up + forward * foot.forward;
            if(foot.heel > 0.0f)
            {
                if(leg->hasToes)
                    leg->heelPositionOffset += up * foot.heel;
                else
                    leg->footPositionOffset += up * (foot.heel * 0.5f);
            }
        }
    }
}
