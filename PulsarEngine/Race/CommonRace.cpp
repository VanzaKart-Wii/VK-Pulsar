#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <Gamemodes/OnlineTT/OnlineTT.hpp>
#include <Gamemodes/KO/KOMgr.hpp>
#include <Race/Players24/Players24.hpp>

namespace Pulsar {
//For hooks which are shared by different things

namespace Race {
void UpdatePoints(RacedataScenario& scenario) {
    const GameType oldType = scenario.settings.gametype;
    const System* system = System::sInstance;
    Racedata* racedata = Racedata::sInstance;

    if(system->IsContext(PULSAR_MODE_KO) && system->koMgr->isSpectating) scenario.settings.gametype = GAMETYPE_DEFAULT;
    bool hasVSGhost = false;
    if(system->IsContext(PULSAR_MODE_OTT)) {
        const u8 last = racedata->racesScenario.playerCount - 1;
        if(Players24::GetPlayer(racedata->racesScenario, last).playerType == PLAYER_GHOST) {
            hasVSGhost = true;
            Players24::GetPlayer(scenario, last).finishPos = racedata->racesScenario.playerCount;
            racedata->racesScenario.playerCount--;
        }
    }
    if(!Players24::UpdatePoints24(scenario)) scenario.UpdatePoints();
    racedata->menusScenario.settings.gametype = oldType;
}
kmCall(0x8085c878, UpdatePoints);
}//namespace Race
}//namespace Pulsar