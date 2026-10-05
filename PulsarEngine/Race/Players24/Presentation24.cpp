#include <kamek.hpp>
#include <Race/Players24/Patches.hpp>
#include <Race/Players24/Players24.hpp>

namespace Pulsar {
namespace Players24 {

static void CameraRacerCount(HookRegs& regs, u32) {
    const u8 count = *reinterpret_cast<const u8*>(regs.gpr[3] + 0x24);
    regs.gpr[0] = count < vanillaPlayers ? count : vanillaPlayers;
}

static void BalloonPlayerId(HookRegs& regs, u32) {
    const s32 id = *reinterpret_cast<const s32*>(regs.gpr[3] + 0x14);
    regs.gpr[0] = (id >= 0 && id < vanillaPlayers) ? static_cast<u32>(id) : 0xFFFFFFFF;
}

static const HookPatch hooks[] ={
    { 0x805ac644, 0x88030024, CameraRacerCount, false },
    { 0x805acf34, 0x88030024, CameraRacerCount, false },
    { 0x807f1ac8, 0x80030014, BalloonPlayerId, false },
};

static PatchSet presentationSet("Presentation 24", nullptr, 0, nullptr, 0, nullptr, 0, hooks, P24_COUNT(hooks),
    nullptr, 0, true);

}
}
