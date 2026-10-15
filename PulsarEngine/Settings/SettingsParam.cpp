#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <Config.hpp>
#include <Settings/SettingsParam.hpp>
#include <UI/UI.hpp>

namespace Pulsar {

namespace Settings {

#define ROW(filePage, fileSlot, bmgPage, bmgRow, optionCount) { filePage, fileSlot, bmgPage, bmgRow, optionCount }
#define NAMES(index) UI::BMG_SETTINGS_PAGE + (index), UI::BMG_SETTINGS_TITLE + (index)
#define USERNAMES(index) UI::BMG_USERSETTINGSOFFSET + UI::BMG_SETTINGS_PAGE + (index), UI::BMG_USERSETTINGSOFFSET + UI::BMG_SETTINGS_TITLE + (index)

//File pages: 0 menu, 1 race, 2 host, 3 OTT, 4 KO, 5 Extended Teams, 6 language. Every row keeps the slot and the texts
//it had before the panel was reorganised.
const PageParams Params::uiPages[Params::uiPageCount] ={
    { NAMES(0), 2, 1, //Menu
        { ROW(0, SETTINGMENU_RADIO_FASTMENUS, 0, 0, 2), ROW(0, SETTINGMENU_RADIO_LAYOUT, 0, 1, 2) },
        { ROW(0, SETTINGMENU_SCROLL_HUDCOLOR, 0, 1, 13) } },
    { NAMES(1), 6, 3, //Race
        { ROW(1, SETTINGRACE_RADIO_MII, 1, 0, 2), ROW(1, SETTINGRACE_RADIO_BATTLE, 1, 2, 2), ROW(1, SETTINGRACE_RADIO_FPS, 1, 3, 2),
          ROW(1, SETTINGRACE_RADIO_SOM, 1, 4, 3), ROW(1, SETTINGRACE_RADIO_TT, 1, 5, 4),
          ROW(0, SETTINGMENU_RADIO_INPUTVIEWER, 0, 3, 2) },
        { ROW(1, SETTINGRACE_SCROLL_SOM, 1, 0, 4), ROW(1, SETTINGRACE_SCROLL_ITEMMODE, 1, 1, 3),
          ROW(1, SETTINGRACE_SCROLL_PLAYERS, 1, 2, 2) } },
    { NAMES(5), 1, 1, //Online
        { ROW(0, SETTINGMENU_RADIO_RANDOMCOMBO, 0, 5, 2) },
        { ROW(0, SETTINGMENU_SCROLL_BOOT, 0, 0, 5) } },
    { NAMES(6), 3, 0, //Music
        { ROW(0, SETTINGMENU_RADIO_MUSIC, 0, 2, 3), ROW(1, SETTINGRACE_RADIO_SPEEDUP, 1, 1, 2),
          ROW(0, SETTINGMENU_RADIO_BRSAR, 0, 4, 2) },
        { } },
    { NAMES(2), 6, 1, //Host
        { ROW(2, 0, 2, 0, 2), ROW(2, 1, 2, 1, 2), ROW(2, 2, 2, 2, 4), ROW(2, 3, 2, 3, 2), ROW(2, 4, 2, 4, 2),
          ROW(2, 5, 2, 5, 4) },
        { ROW(2, SETTINGHOST_SCROLL_GP_RACES, 2, 0, 7) } },
    { NAMES(3), 5, 0, //OTT
        { ROW(3, 0, 3, 0, 3), ROW(3, 1, 3, 1, 3), ROW(3, 2, 3, 2, 2), ROW(3, 3, 3, 3, 2), ROW(3, 4, 3, 4, 2) },
        { } },
    { NAMES(4), 2, 2, //KO
        { ROW(4, SETTINGKO_ENABLED, 4, 0, 2), ROW(4, SETTINGKO_FINAL, 4, 1, 2) },
        { ROW(4, SETTINGKO_KOPERRACE, 4, 0, 4), ROW(4, SETTINGKO_RACESPERKO, 4, 1, 4) } },
    { USERNAMES(0), 2, 1, //Extended Teams
        { ROW(5, RADIO_EXTENDEDTEAMSENABLED, 5, 0, 2), ROW(5, RADIO_EXTENDEDTEAMSLINE, 5, 1, 2) },
        { ROW(5, SCROLLER_EXTENDEDTEAMSCOUNT, 5, 0, 4) } },
    { USERNAMES(1), 2, 1, //Misc
        { ROW(6, RADIO_PATCHES, 6, 0, 2), ROW(6, RADIO_HARDAI, 6, 1, 2) },
        { ROW(6, SCROLLER_LANGUAGE, 6, 0, 2) } },
};

#undef ROW
#undef NAMES
#undef USERNAMES

}//namespace Settings
}//namespace Pulsar
