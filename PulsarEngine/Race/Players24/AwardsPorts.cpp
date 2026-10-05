#include <kamek.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/UI/Text/Text.hpp>
#include <Race/Players24/Players24.hpp>
#include <Race/Players24/Patches.hpp>

namespace Pulsar {
namespace Players24 {

static void CopyPlayerData(RacedataPlayer& dest, const RacedataPlayer& src) {
    memcpy(reinterpret_cast<u8*>(&dest) + 4, reinterpret_cast<const u8*>(&src) + 4, sizeof(RacedataPlayer) - 4);
}

static void CopyAwardsSnapshot(HookRegs&, u32) {
    if(!IsReady()) return;
    Racedata* racedata = Racedata::sInstance;
    for(u8 id = vanillaPlayers; id < maxPlayers; ++id) {
        CopyPlayerData(GetPlayer(racedata->awardScenario, id), GetPlayer(racedata->racesScenario, id));
    }
}

static bool HasExtraStandings() {
    if(!IsReady()) return false;
    return GetPlayer(Racedata::sInstance->awardScenario, vanillaPlayers).playerType != PLAYER_NONE;
}

static bool UsesAwardsLayout24(const HookRegs&) { return HasExtraStandings(); }

static void AwardsRowVariant(HookRegs& regs, u32) {
    u32 row = regs.gpr[28] & 0xFF;
    if(row >= 12 && !UsesAwardsLayout24(regs)) row -= 12;
    regs.gpr[6] = row;
}

static void AwardsRowFile(HookRegs& regs, u32) {
    const char* name = reinterpret_cast<const char*>(regs.gpr[23]);
    const char* name24 = nullptr;
    if(UsesAwardsLayout24(regs)) name24 = Layout24(name);
    regs.gpr[5] = reinterpret_cast<u32>(name24 != nullptr ? name24 : name);
}

static void LayoutAwardsRow(HookRegs& regs, u32) {
    if(HasExtraStandings() && !UsesAwardsLayout24(regs)) {
        const float scale = 0.62f;
        const float offset = regs.gpr[28] < 12 ? -195.0f : 195.0f;
        float* transform = reinterpret_cast<float*>(regs.gpr[22] + 4);
        for(int block = 4; block > 0; --block, transform += 6) {
            if(block == 2) continue;
            transform[0] = transform[0] * scale + offset;
            transform[3] *= scale;
            transform[4] *= scale;
        }
    }
    regs.gpr[28] += 1;
}

typedef void (*SetMessageFunc)(void* control, const char* textBox, u32 bmgId, const Text::Info* info);
static void setTextBoxMessage(void* control, const char* textBox, u32 bmgId, const Text::Info* info) {
    reinterpret_cast<SetMessageFunc>(Port(0x8063dcbc))(control, textBox, bmgId, info);
}
void SetRankMessage(void* control, const char* textBox, u32 bmgId, const Text::Info* info) {
    if(bmgId > 0x520 && bmgId <= 0x52c) {
        Text::Info rank;
        rank.intToPass[0] = bmgId - 0x514;
        setTextBoxMessage(control, textBox, 0x522, &rank);
        return;
    }
    setTextBoxMessage(control, textBox, bmgId, info);
}

static const WordPatch words[] ={
    { 0x805308d0, 0x281e000c, 0x281e0018 },
    { 0x80530a14, 0x281c000c, 0x281c0018 },
    { 0x80530bbc, 0x2819000c, 0x28190018 },
    { 0x80530bc8, 0x281a000c, 0x281a0018 },
    { 0x80530d00, 0x281a000c, 0x281a0018 },
    { 0x80530d0c, 0x2819000c, 0x28190018 },
    { 0x80530e24, 0x281b000c, 0x281b0018 },
    { 0x80530e30, 0x281a000c, 0x281a0018 },
    { 0x806238fc, 0x38601770, 0x386028e0 },
    { 0x805bc0e4, 0x38e0000c, 0x38e00018 },
    { 0x805bc138, 0x38c0000c, 0x38c00018 },
    { 0x805bc288, 0x3880000f, 0x3880001b },
    { 0x805bc57c, 0x281c000c, 0x281c0018 },
    { 0x805bc8e0, 0x38000006, 0x3800000c },
    { 0x805bc8ec, 0x3880000c, 0x38800018 },
    { 0x805bce34, 0x281c000c, 0x281c0018 },
    { 0x805bcf14, 0x2800000c, 0x28000018 },
    { 0x805bbd90, 0xa35f18e0, 0xa35f18e2 },
#include <Race/Players24/AwardsFields.inc>
};

static const CallPatch calls[] ={
    { 0x805bbc4c, 0x48082071, SetRankMessage },
};

static const HookPatch hooks[] ={
    { 0x80530888, 0x3bc00000, CopyAwardsSnapshot, true },
    { 0x805bc46c, 0x5786063e, AwardsRowVariant, false },
    { 0x805bc4c4, 0x7ee5bb78, AwardsRowFile, false },
    { 0x805bc578, 0x3b9c0001, LayoutAwardsRow, false },
};

static PatchSet awardsSet("Awards", words, P24_COUNT(words), calls, P24_COUNT(calls), nullptr, 0,
    hooks, P24_COUNT(hooks));

}
}
