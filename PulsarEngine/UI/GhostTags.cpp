#include <UI/GhostTags.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/UI/Text/Text.hpp>
#include <Settings/Settings.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

static bool IsGhostMode(const RacedataScenario& scenario) {
    return scenario.settings.gamemode == MODE_TIME_TRIAL || scenario.settings.gamemode == MODE_GHOST_RACE;
}

static u8 GetSetting() {
    return Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_LANGUAGE, RADIO_GHOSTTAGS);
}

bool AreGhostTagsHidden() {
    const Racedata* racedata = Racedata::sInstance;
    return racedata != nullptr && IsGhostMode(racedata->racesScenario) && GetSetting() == GHOSTTAGS_HIDDEN;
}

static wchar_t sTagTexts[4][16];

void ApplyGhostTagContent(CtrlRaceNameBalloon& balloon, u8 playerId) {
    const u8 setting = GetSetting();
    if(setting != GHOSTTAGS_TIME && setting != GHOSTTAGS_DATE) return;
    Racedata* racedata = Racedata::sInstance;
    if(racedata == nullptr || playerId >= 4) return;
    const RacedataScenario& scenario = racedata->racesScenario;
    if(!IsGhostMode(scenario) || scenario.players[playerId].playerType != PLAYER_GHOST) return;

    const u32 offset = scenario.players[0].playerType != PLAYER_GHOST ? 1 : 0;
    if(playerId < offset) return;
    const RKGHeader& header = (&racedata->ghosts[0])[playerId - offset].header;
    if(header.magic[0] != 'R' || header.magic[1] != 'K' || header.magic[2] != 'G' || header.magic[3] != 'D') return;

    wchar_t* text = sTagTexts[playerId];
    if(setting == GHOSTTAGS_TIME) {
        swprintf(text, 16, L"%d:%02d.%03d", header.minutes, header.seconds, header.milliseconds);
    }
    else swprintf(text, 16, L"%02d/%02d/%04d", header.day, header.month, 2000 + header.year);
    Text::Info info;
    info.strings[0] = text;
    balloon.SetTextBoxMessage("chara_name", BMG_TEXT, &info);
}

}//namespace UI
}//namespace Pulsar
