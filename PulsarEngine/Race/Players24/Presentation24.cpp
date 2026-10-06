#include <kamek.hpp>
#include <Race/Players24/Patches.hpp>
#include <Race/Players24/Players24.hpp>
#include <Race/Players24/Online24.hpp>

namespace Pulsar {
namespace Players24 {

static void CameraRacerCount(HookRegs& regs, u32) {
    const u8 count = *reinterpret_cast<const u8*>(regs.gpr[3] + 0x24);
    regs.gpr[0] = count < vanillaPlayers ? count : vanillaPlayers;
}

static void BalloonPlayerId(HookRegs& regs, u32) {
    const s32 id = *reinterpret_cast<const s32*>(regs.gpr[3] + 0x14);
    regs.gpr[0] = (id >= 0 && id < maxPlayers) ? static_cast<u32>(id) : 0xFFFFFFFF;
}

static float balloonPositions[4][maxPlayers - vanillaPlayers][3];
static u32 BalloonBase(u32 balloons, u32 id) {
    if(id < vanillaPlayers || id >= maxPlayers) return balloons + id * 0xc;
    const u8 screen = *reinterpret_cast<const u8*>(balloons + 3) & 3;
    return reinterpret_cast<u32>(balloonPositions[screen][id - vanillaPlayers]) - 0x20;
}
static void BalloonRacerCount(HookRegs& regs, u32) {
    const u8 count = *reinterpret_cast<const u8*>(regs.gpr[3] + 0x24);
    regs.gpr[18] = IsOnline24() && HasExtraRacers() ? GetRacePlayerCount() : count;
}
static void BalloonPositionWrite(HookRegs& regs, u32) { regs.gpr[16] = BalloonBase(regs.gpr[30], regs.gpr[31] & 0xFF); }
static void BalloonPositionRead(HookRegs& regs, u32) { regs.gpr[4] = BalloonBase(regs.gpr[30], regs.gpr[0] / 0xc); }
static void BalloonControlPosition(HookRegs& regs, u32) { regs.gpr[6] = BalloonBase(regs.gpr[4], regs.gpr[27] & 0xFF); }
static void BalloonGetPosition(HookRegs& regs, u32) { regs.gpr[3] = BalloonBase(regs.gpr[3], regs.gpr[4] & 0xFF); }

static void ScreenEffectsId(HookRegs& regs, u32) {
    const u8 id = *reinterpret_cast<const u8*>(regs.gpr[30] + 0x12e);
    regs.gpr[0] = id < maxPlayers ? id % vanillaPlayers : id;
}

static const HookPatch hooks[] ={
    { 0x805ac644, 0x88030024, CameraRacerCount, false },
    { 0x805acf34, 0x88030024, CameraRacerCount, false },
    { 0x807f1ac8, 0x80030014, BalloonPlayerId, false },
    { 0x807f14cc, 0x8a430024, BalloonRacerCount, false },
    { 0x807f1758, 0x7e1e0214, BalloonPositionWrite, false },
    { 0x807f1ad8, 0x7c9e0214, BalloonPositionRead, false },
    { 0x807f1128, 0x7cc40214, BalloonControlPosition, false },
    { 0x807f1da8, 0x7c630214, BalloonGetPosition, false },
    { 0x8069075c, 0x881e012e, ScreenEffectsId, false },
};

static PatchSet presentationSet("Presentation 24", nullptr, 0, nullptr, 0, nullptr, 0, hooks, P24_COUNT(hooks),
    nullptr, 0, true);

}
}
