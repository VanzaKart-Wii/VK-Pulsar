#ifndef _PUL_PLAYERS24_ONLINE_
#define _PUL_PLAYERS24_ONLINE_
#include <kamek.hpp>
#include <MarioKartWii/RKNet/SELECT.hpp>

namespace Pulsar {
namespace Players24 {

bool IsOnline24();
bool IsConsoles24();
bool HasNoMiiSlot(u8 id);
bool ShowsCharacter(u8 id);
bool GetRacerName(u8 id, wchar_t* dst, u32 length);
bool CanHostRoom24();
bool IsHostingRoom24();
bool HostPlayers24Bit();
u32 DecideVRRows();
u8 PlayersAtAid(u8 aid);
u8 RoomPlayerCount();
u8 GetOwnCPUId();
bool IsOnlineCPU(u8 id);
bool FitsWifiResults(u8 id);
u8 AidOfPlayer(u8 id);
void SyncAidTable();
void FillCPUSelectData(RKNet::SELECTPacket& packet);

bool OnlineItemsReady();
bool OnlineRaceReady();

}
}
#endif
