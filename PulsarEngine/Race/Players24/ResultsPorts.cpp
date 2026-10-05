#include <kamek.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/UI/Text/Text.hpp>
#include <Race/Players24/Players24.hpp>
#include <Race/Players24/Patches.hpp>

namespace Pulsar {
namespace Players24 {

void SetRankMessage(void* control, const char* textBox, u32 bmgId, const Text::Info* info);

const char* Layout24(const char* name) {
    static const char* const names[][2] ={
        { "ResultVS", "ResultVS_24" },
        { "ResultGP", "ResultGP_24" },
        { "WifiAwardItem", "WifiAwardItem_24" },
        { "AwardItemWin", "AwardItemWin_24" },
        { "AwardItemLose", "AwardItemLose_24" },
    };
    for(u32 i = 0; i < P24_COUNT(names); ++i) if(strcmp(name, names[i][0]) == 0) return names[i][1];
    return nullptr;
}

static bool HasResultColumns() {
    if(!IsReady() || !HasExtraRacers()) return false;
    return IsVSMode(Racedata::sInstance->racesScenario.settings.gamemode);
}

typedef const char* (*BRCTRNameFunc)(const void* control);
static const char* ResultRowBRCTR(const void* control) {
    const u32* vtable = *reinterpret_cast<u32* const*>(control);
    return reinterpret_cast<BRCTRNameFunc>(vtable[0x44 / 4])(control);
}
static bool UsesLayout24(const void* control) { return HasResultColumns() && Layout24(ResultRowBRCTR(control)) != nullptr; }

static void ResultRowVariant(HookRegs& regs, u32) {
    u32 row = *reinterpret_cast<u8*>(regs.gpr[30] + 0x174);
    if(row > 12 && !UsesLayout24(reinterpret_cast<const void*>(regs.gpr[30]))) row -= 12;
    regs.gpr[6] = row;
}

static void ResultRowFile(HookRegs& regs, u32) {
    const char* name = reinterpret_cast<const char*>(regs.gpr[3]);
    const char* name24 = nullptr;
    if(HasResultColumns()) name24 = Layout24(name);
    regs.gpr[5] = reinterpret_cast<u32>(name24 != nullptr ? name24 : name);
}

static void ResultRowColumns(HookRegs& regs, u32) {
    if(!HasResultColumns() || UsesLayout24(reinterpret_cast<const void*>(regs.gpr[30]))) return;
    const u8* control = reinterpret_cast<const u8*>(regs.gpr[30]);
    const float scale = 0.62f;
    const float offset = control[0x174] <= 12 ? -195.0f : 195.0f;
    float* transform = reinterpret_cast<float*>(regs.gpr[30] + 4);
    for(int block = 4; block > 0; --block, transform += 6) {
        if(block == 2) continue;
        transform[0] = transform[0] * scale + offset;
        transform[3] *= scale;
        transform[4] *= scale;
    }
}

static const WordPatch words[] ={
    { 0x8085cb74, 0x88030024, 0x7fe0fb78 },
};

static const CallPatch calls[] ={
    { 0x807f6028, 0x4be47c95, SetRankMessage },
};

static const HookPatch hooks[] ={
    { 0x807f4e98, 0x88de0174, ResultRowVariant, false },
    { 0x807f4ef8, 0x7c651b78, ResultRowFile, false },
    { 0x807f4fb4, 0x800100b4, ResultRowColumns, true },
};

static PatchSet resultsSet("Results", words, P24_COUNT(words), calls, P24_COUNT(calls), nullptr, 0,
    hooks, P24_COUNT(hooks));

static const u32 wifiPlayerCountOffset = 0x2766;

static bool HasWifiRows24(const u8* page) { return page[wifiPlayerCountOffset] > vanillaPlayers; }

static void WifiRowVariant(HookRegs& regs, u32) {
    u32 row = regs.gpr[29] & 0xFF;
    if(row >= vanillaPlayers && !HasWifiRows24(reinterpret_cast<const u8*>(regs.gpr[28]))) row -= vanillaPlayers;
    regs.gpr[6] = row;
}

static void WifiRowFile(HookRegs& regs, u32 original) {
    const u32 name = regs.gpr[25] + static_cast<s16>(original & 0xFFFF);
    const bool rows24 = HasWifiRows24(reinterpret_cast<const u8*>(regs.gpr[28]));
    regs.gpr[5] = rows24 ? reinterpret_cast<u32>(Layout24(reinterpret_cast<const char*>(name))) : name;
}

static const WordPatch wifiWords[] ={
    { 0x806239bc, 0x386015fc, 0x3860276c },
    { 0x80645c68, 0x38e0000c, 0x38e00018 },
    { 0x80645cbc, 0x38c0000c, 0x38c00018 },
    { 0x80645df8, 0x3880000e, 0x3880001a },
    { 0x8064603c, 0x281d000c, 0x281d0018 },
    { 0x80646644, 0x281d000c, 0x281d0018 },
    { 0x80646734, 0x2800000c, 0x28000018 },
    { 0x80645d5c, 0x980315f4, 0x98032764 },
    { 0x80645d68, 0x980315f6, 0x98032766 },
    { 0x80645da0, 0x980315f5, 0x98032765 },
    { 0x80645dac, 0x980315f5, 0x98032765 },
    { 0x80645ecc, 0x881c15f6, 0x881c2766 },
    { 0x80645edc, 0x881c15f4, 0x881c2764 },
    { 0x80646078, 0x980315f8, 0x98032768 },
    { 0x806460d8, 0x980315f7, 0x98032767 },
    { 0x8064610c, 0x98a315f7, 0x98a32767 },
    { 0x80646110, 0x880315f7, 0x88032767 },
    { 0x80646148, 0x880315f7, 0x88032767 },
    { 0x80646168, 0x880315f7, 0x88032767 },
    { 0x806461b4, 0x880315f7, 0x88032767 },
    { 0x806461d4, 0x880315f8, 0x88032768 },
    { 0x80646204, 0x980315f8, 0x98032768 },
    { 0x80646220, 0x980315f4, 0x98032764 },
    { 0x8064622c, 0x980315f6, 0x98032766 },
    { 0x80646264, 0x980315f5, 0x98032765 },
    { 0x80646270, 0x980315f5, 0x98032765 },
    { 0x806463b4, 0x880315f4, 0x88032764 },
    { 0x80646444, 0x881f15f5, 0x881f2765 },
    { 0x80646558, 0x881f15f5, 0x881f2765 },
    { 0x806465f8, 0x88a315f5, 0x88a32765 },
    { 0x806465fc, 0x880315f4, 0x88032764 },
    { 0x80646624, 0x88be15f4, 0x88be2764 },
    { 0x8064668c, 0x88be15f4, 0x88be2764 },
    { 0x806466a8, 0x881e15f6, 0x881e2766 },
    { 0x806466d4, 0x88be15f4, 0x88be2764 },
    { 0x806466fc, 0x8b7e15f6, 0x8b7e2766 },
    { 0x80646714, 0x88be15f4, 0x88be2764 },
};

static const CallPatch wifiCalls[] ={
    { 0x806458a0, 0x4bff841d, SetRankMessage },
};

static const HookPatch wifiHooks[] ={
    { 0x80645f3c, 0x57a6063e, WifiRowVariant, false },
    { 0x80645fa0, 0x38b9011e, WifiRowFile, false },
};

static PatchSet wifiResultsSet("Online results", wifiWords, P24_COUNT(wifiWords), wifiCalls, P24_COUNT(wifiCalls),
    nullptr, 0, wifiHooks, P24_COUNT(wifiHooks));

}
}
