#include <UI/Minimap.hpp>
#include <MarioKartWii/UI/Layout/ControlGroup.hpp>
#include <Settings/Settings.hpp>

namespace Pulsar {
namespace UI {

void ApplyMinimapSetting(CtrlRace2DMapCharacter& character) {
    const u8 setting = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_LANGUAGE, RADIO_MINIMAP);
    if(setting != MINIMAP_HIDDEN) return;
    const ControlGroup* group = character.parentGroup;
    if(group == nullptr || group->parentControl == nullptr) return;
    group->parentControl->isHidden = true;
}

}//namespace UI
}//namespace Pulsar
