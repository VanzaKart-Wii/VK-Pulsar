#ifndef _PUL_PLAYERS24_
#define _PUL_PLAYERS24_
#include <kamek.hpp>
#include <MarioKartWii/Race/Racedata.hpp>

namespace Pulsar {
namespace Players24 {

const u8 vanillaPlayers = 12;
const u8 maxPlayers = 24;
const u8 idShift = 64;
const s32 shadowOffset = -(s32)((idShift - vanillaPlayers) * sizeof(RacedataPlayer));
const u32 shadowPrefix = (u32)-shadowOffset;

bool IsReady();
bool IsEnabled();
u8 GetRacePlayerCount();
bool UpdatePoints24(RacedataScenario& menu);
inline bool HasExtraRacers() { return GetRacePlayerCount() > vanillaPlayers; }
inline bool IsVSMode(GameMode mode) { return mode == MODE_VS_RACE || mode == MODE_PRIVATE_VS || mode == MODE_PUBLIC_VS; }

void ApplyConsoles();
void ApplyAidLayout();
void ApplyOnlinePages();
void ApplyRoomBubbles();

const char* Layout24(const char* name);

inline RacedataPlayer& GetPlayer(RacedataScenario& scenario, u8 id) {
    if(id < vanillaPlayers || !IsReady()) return scenario.players[id];
    RacedataScenario* shadow = reinterpret_cast<RacedataScenario*>(reinterpret_cast<u8*>(&scenario) + shadowOffset);
    return shadow->players[id - vanillaPlayers];
}
inline const RacedataPlayer& GetPlayer(const RacedataScenario& scenario, u8 id) {
    return GetPlayer(const_cast<RacedataScenario&>(scenario), id);
}

}
}
#endif
