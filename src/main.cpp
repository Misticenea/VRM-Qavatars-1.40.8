#include "main.hpp"

#include "scotland2/shared/loader.hpp"

#include <conditional-dependencies/shared/main.hpp>
#include <GlobalNamespace/Saber.hpp>
#include <UnityEngine/Resources.hpp>

#include "custom-types/shared/coroutine.hpp"
#include "custom-types/shared/register.hpp"

#include "GlobalNamespace/MainMenuViewController.hpp"
#include "GlobalNamespace/MainCamera.hpp"
#include "GlobalNamespace/MainEffectContainerSO.hpp"
#include "GlobalNamespace/MainEffectSO.hpp"

#include "UnityEngine/SceneManagement/SceneManager.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/QualitySettings.hpp"
#include "UnityEngine/AssetBundleRequest.hpp"
#include "UnityEngine/AsyncOperation.hpp"
#include "UnityEngine/AssetBundleCreateRequest.hpp"
#include "UnityEngine/QualitySettings.hpp"

#include "AssetLib/shaders/shaderLoader.hpp"
#include "AssetLib/shaders/ShaderSO.hpp"
#include "AssetLib/modelImporter.hpp"

#include "UI/AvatarsFlowCoordinator.hpp"

#include "AvatarManager.hpp"
#include "LightManager.hpp"
#include "SceneEventManager.hpp"
#include "GroundOffsetManager.hpp"
#include "MaterialTracker.hpp"
#include "MirrorManager.hpp"
#include "ReplayCorrection.hpp"

#include "config/ConfigManager.hpp"

#include "VMC/VMCClient.hpp"
#include "VMC/VMCServer.hpp"

#include "customTypes/FinalIK/VRIK.hpp"
#include "customTypes/Installers/MenuInstaller.hpp"

#include "lapiz/shared/zenject/Zenjector.hpp"
#include "lapiz/shared/zenject/Location.hpp"

modloader::ModInfo modInfo{MOD_ID, VERSION, GIT_COMMIT};

// FP/TP stuff should be in its own mod...
MAKE_HOOK_MATCH(MainCameraHook, &GlobalNamespace::MainCamera::Awake, void, GlobalNamespace::MainCamera* self)
{
    MainCameraHook(self);
    constexpr int fpmask =
           2147483647 &
           ~(1 << 3);
    self->get_camera()->set_cullingMask(fpmask);
}

//I know
//I know
//I know
//Scene manager is bad
//Open a PR (please)
MAKE_HOOK_MATCH(SceneManager_Internal_ActiveSceneChanged, &UnityEngine::SceneManagement::SceneManager::Internal_ActiveSceneChanged, void, UnityEngine::SceneManagement::Scene prevScene, UnityEngine::SceneManagement::Scene nextScene) {
    SceneManager_Internal_ActiveSceneChanged(prevScene, nextScene);
    VRMQavatars::SceneEventManager::GameSceneChanged(nextScene);
}

MAKE_HOOK_MATCH(SaberPatch, &GlobalNamespace::Saber::ManualUpdate, void, GlobalNamespace::Saber* self) {
    SaberPatch(self);

    // the saber's parent is the controller, whose parent is the tracking space origin (same as the Replay mod assumes)
    auto saberTransform = self->get_transform();
    auto controller = saberTransform->get_parent();
    UnityEngine::Transform* origin = controller ? controller->get_parent().ptr() : nullptr;
    Sombrero::FastVector3 pos = saberTransform->get_position();
    Sombrero::FastQuaternion rot = saberTransform->get_rotation();
    if(origin)
    {
        pos = origin->InverseTransformPoint(pos);
        rot = Sombrero::FastQuaternion(UnityEngine::Quaternion::Inverse(origin->get_rotation())) * rot;
    }
    const bool left = self->get_saberType() == GlobalNamespace::SaberType::SaberA;

    const static auto replay = CondDeps::Find<bool>("replay", "IsInReplay");
    if(replay.has_value() && replay.value()())
    {
        if(VRMQavatars::AvatarManager::currentContext != nullptr)
        {
            const auto targetManager = VRMQavatars::AvatarManager::currentContext->rootGameObject->GetComponent<VRMQavatars::TargetManager*>();
            if(left)
            {
                targetManager->replayLeftSaberPos = pos;
                targetManager->replayLeftSaberRot = rot;
            }
            else
            {
                targetManager->replayRightSaberPos = pos;
                targetManager->replayRightSaberRot = rot;
            }
        }
    }
    else
    {
        VRMQavatars::ReplayCorrection::Sample(left ? VRMQavatars::ReplayCorrection::LeftHand : VRMQavatars::ReplayCorrection::RightHand, pos, rot);
    }
}

MAKE_HOOK_MATCH(BloomHook, &GlobalNamespace::MainEffectContainerSO::Init, void, GlobalNamespace::MainEffectContainerSO* self, GlobalNamespace::MainEffectSO* mainEffect) {
    BloomHook(self, mainEffect);
    VRMQavatars::MaterialTracker::bloomEnabled = mainEffect->get_hasPostProcessEffect();
    VRMQavatars::MaterialTracker::UpdateMaterials();
}

extern "C" void setup(CModInfo* info) {
    info->id = modInfo.id.c_str();
    info->version = modInfo.version.c_str();
    info->version_long = modInfo.versionLong;

    getGlobalConfig().Init(Configuration::getConfigFilePath(modInfo));
    VRMLogger.info("hii");
}

extern "C" void late_load() {
    il2cpp_functions::Init();

    mkpath(vrm_path);
    mkpath(avaconfig_path);

    custom_types::Register::AutoRegister(); 

    INSTALL_HOOK(VRMLogger, MainCameraHook);
    INSTALL_HOOK(VRMLogger, SceneManager_Internal_ActiveSceneChanged);
    INSTALL_HOOK(VRMLogger, SaberPatch);
    INSTALL_HOOK(VRMLogger, BloomHook);

    auto zenjeqtor = Lapiz::Zenject::Zenjector::Get();

    zenjeqtor->Install<VRMQavatars::MenuInstaller*>(Lapiz::Zenject::Location::Menu);

    BSML::Register::RegisterMainMenu<FlowCoordinators::AvatarsFlowCoordinator*>("Avatars", "VRM Custom Avatars");
}