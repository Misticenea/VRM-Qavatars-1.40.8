#include "UI/AvatarsFlowCoordinator.hpp"

#include "bsml/shared/Helpers/creation.hpp"
#include "HMUI/ViewController.hpp"

DEFINE_TYPE(VRMQavatars::UI::FlowCoordinators, AvatarsFlowCoordinator);

namespace VRMQavatars::UI
{
    void FlowCoordinators::AvatarsFlowCoordinator::Awake() {
        if (!MirrorViewController || static_cast<void*>(MirrorViewController->m_CachedPtr.m_value) == nullptr) {
            MirrorViewController = BSML::Helpers::CreateViewController<ViewControllers::MirrorViewController*>();
        }
        if (!AvatarSelectionViewController || static_cast<void*>(AvatarSelectionViewController->m_CachedPtr.m_value) == nullptr) {
            AvatarSelectionViewController = BSML::Helpers::CreateViewController<ViewControllers::AvatarSelectionViewController*>();
        }
        if (!AvatarSettingsViewController || static_cast<void*>(AvatarSettingsViewController->m_CachedPtr.m_value) == nullptr) {
            AvatarSettingsViewController = BSML::Helpers::CreateViewController<ViewControllers::AvatarSettingsViewController*>();
        }
    }

    void FlowCoordinators::AvatarsFlowCoordinator::DidActivate(const bool firstActivation, bool, bool) {
        if (!firstActivation) return;
        showBackButton = true;

        SetTitle(il2cpp_utils::newcsstr("VRM Qavatars"), HMUI::ViewController::AnimationType::In);
        ProvideInitialViewControllers(AvatarSelectionViewController, AvatarSettingsViewController, MirrorViewController, nullptr, nullptr);
    }

    void FlowCoordinators::AvatarsFlowCoordinator::BackButtonWasPressed(HMUI::ViewController* topViewController) {
        this->_parentFlowCoordinator->DismissFlowCoordinator(this, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, false);
    }
}
