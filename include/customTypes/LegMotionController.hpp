#pragma once

#include <chatplex-sdk-bs/shared/CP_SDK/Utils/Il2cpp.hpp>

#include "main.hpp"

#include "custom-types/shared/macros.hpp"

#include "UnityEngine/MonoBehaviour.hpp"

#include "GlobalNamespace/OVRInput.hpp"

#include "customTypes/FinalIK/VRIK.hpp"
#include "customTypes/TargetManager.hpp"
#include "config/ConfigManager.hpp"

namespace VRMQavatars
{
    // fakes leg motion for players without foot trackers:
    // steps synced to the song's beat, actions bound to controller inputs and lifting the feet when the player jumps.
    // everything is applied as offsets on top of VRIK's own locomotion, so the feet return to it when idle
    class LegMotionController : public UnityEngine::MonoBehaviour
    {
        CP_SDK_IL2CPP_INHERIT("VRMQavatars", LegMotionController, UnityEngine::MonoBehaviour);
        CP_SDK_IL2CPP_DECLARE_CTOR(LegMotionController);
        CP_SDK_IL2CPP_DECLARE_DTOR_MONOBEHAVIOUR(LegMotionController);
    public:
        DECLARE_INSTANCE_METHOD(void, Update);
    public:
        // action names stored in LegTriggerSettings, the first one is "None"
        static const std::vector<std::string> actionNames;
        static const std::vector<std::u16string> beatStyleNames;

        // reloads the settings of the current avatar's controller
        static void Reload();

        void LoadSettings();

        // position offset in the avatar's space, side is positive outwards for both feet
        struct FootOffset
        {
            float side = 0.0f;
            float up = 0.0f;
            float forward = 0.0f;
            float heel = 0.0f;
        };

        struct ActionState
        {
            float weight = 0.0f;
            float time = -1.0f;
            bool wasDown = false;
        };

        struct Binding
        {
            GlobalNamespace::OVRInput::Button button;
            GlobalNamespace::OVRInput::Controller controller;
            int action;
        };

        FinalIK::VRIK* vrik = nullptr;
        TargetManager* targetManager = nullptr;

        LegMotionSettings settings;
        std::vector<Binding> bindings;
        std::vector<ActionState> actionStates;

        float beatWeight = 0.0f;

        bool hasHeadHeight = false;
        float lastHeadHeight = 0.0f;
        float lastBaseY = 0.0f;
        float headVelocity = 0.0f;
        float standHeight = 0.0f;
        float lowTime = 0.0f;
        bool airborne = false;
        float airTime = 0.0f;
        float jumpLift = 0.0f;

    private:
        void UpdateActions(float dt, FootOffset* feet, float* busy);
        void UpdateBeat(float dt, FootOffset* feet, const float* busy);
        void UpdateJump(float dt, FootOffset* feet);
    };
}

CP_SDK_IL2CPP_INHERIT_HELPERS(VRMQavatars::LegMotionController);
