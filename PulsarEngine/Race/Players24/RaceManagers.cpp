#include <kamek.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <Race/Players24/Players24.hpp>
#include <Race/Players24/Patches.hpp>

namespace Pulsar {
namespace Players24 {

static const WordPatch aiWords[] ={
    { 0x80738e6c, 0x3860009c, 0x386000fc },
    { 0x80739264, 0x93c40024, 0x93c4009c },
    { 0x80739308, 0x80630024, 0x8063009c },
    { 0x80739370, 0x807c0024, 0x807c009c },
    { 0x80739484, 0x807c0024, 0x807c009c },
    { 0x80739b48, 0x807d0024, 0x807d009c },
    { 0x80739c18, 0x80630024, 0x8063009c },
    { 0x80739ce4, 0x807f0024, 0x807f009c },
    { 0x80739028, 0x38600038, 0x38600098 },
    { 0x8073c360, 0x90040004, 0x90040038 },
    { 0x8073c370, 0x80840004, 0x80840038 },
    { 0x8073c408, 0x83ca0004, 0x83ca0038 },
    { 0x80739040, 0x38600034, 0x38600094 },
    { 0x80739db4, 0x807e0004, 0x807e0034 },
    { 0x80739dd8, 0x93fe0004, 0x93fe0034 },
    { 0x80739de4, 0x2c1d000c, 0x2c1d0018 },
    { 0x80739f30, 0x90640004, 0x90640034 },
    { 0x80739f78, 0x833a0004, 0x833a0034 },
    { 0x80739fd4, 0x2c18000c, 0x2c180018 },
    { 0x80739ffc, 0x80630004, 0x80630034 },
};
static const CallPatch aiCalls[] ={
    { 0x80738e70, 0x4baf0f5d, AllocZeroed },
    { 0x8073902c, 0x4baf0da1, AllocZeroed },
    { 0x80739044, 0x4baf0d89, AllocZeroed },
};
static const u32 aiInstances[] ={ 0x809c2be8 };
static PatchSet aiSet("AI", aiWords, P24_COUNT(aiWords), aiCalls, P24_COUNT(aiCalls), aiInstances, P24_COUNT(aiInstances));

static const WordPatch aiItemWords[] ={
    { 0x80739010, 0x38600194, 0x38600254 },
#include <Race/Players24/AIItemPatches.inc>
};
static void SortItemEntries(s32 count, u8* entries, bool renumber) {
    u8* byRank[24];
    for(int i = 0; i < 24; ++i) byRank[i] = nullptr;
    for(s32 i = 0; i < count; ++i) {
        u8* cpu = *reinterpret_cast<u8**>(entries + i * 8);
        const s32 rank = *reinterpret_cast<s32*>(cpu + 0x14);
        if(rank >= 1 && rank <= 24) byRank[rank - 1] = cpu;
    }
    s32 sorted = 0;
    for(int i = 0; i < 24; ++i) {
        u8* cpu = byRank[i];
        if(cpu == nullptr) continue;
        if(renumber) *reinterpret_cast<s32*>(cpu + 0x14) = sorted + 1;
        *reinterpret_cast<u8**>(entries + sorted * 8 + 4) = cpu;
        ++sorted;
    }
}
static void SortCPUItemEntries(u8* itemAI) {
    SortItemEntries(*reinterpret_cast<s32*>(itemAI + 0x178), itemAI + 0x194, true);
}
static void SortHumanItemEntries(u8* itemAI) {
    SortItemEntries(*reinterpret_cast<s32*>(itemAI + 0x17c), itemAI + 0x148, false);
}
static const CallPatch aiItemCalls[] ={
    { 0x80739014, 0x4baf0db9, AllocZeroed },
    { 0x80742584, 0x9421ffc0, SortCPUItemEntries, true },
    { 0x807426e4, 0x9421ffc0, SortHumanItemEntries, true },
};
static PatchSet aiItemSet("AI items", aiItemWords, P24_COUNT(aiItemWords), aiItemCalls, P24_COUNT(aiItemCalls), nullptr, 0);

static const WordPatch aiParamsWords[] ={
    { 0x8073abb0, 0x38600070, 0x386000d0 },
    { 0x8073abc4, 0x38e0000c, 0x38e00018 },
};
static void LoadActions(u8* params, const float* rawActions, u32 difficulty) {
    const float* row = rawActions + difficulty * 12 * 8;
    u8* actions = *reinterpret_cast<u8**>(params + 8);
    const u32 playerCount = Racedata::sInstance->racesScenario.playerCount;
    for(u32 id = 0; id < 24; ++id) {
        const u32 entry = playerCount <= 12 ? id % 12 : id * 12 / playerCount;
        const float* src = row + entry * 8;
        for(int i = 0; i < 8; ++i) actions[id * 8 + i] = static_cast<u8>(static_cast<s32>(src[i]));
    }
}
static const CallPatch aiParamsCalls[] ={
    { 0x8073b020, 0x1ca50060, LoadActions, true },
};
static PatchSet aiParamsSet("AI params", aiParamsWords, P24_COUNT(aiParamsWords), aiParamsCalls, P24_COUNT(aiParamsCalls), nullptr, 0);

static const WordPatch itemWords[] ={
    { 0x80799154, 0x38600430, 0x38600490 },
    { 0x80799404, 0x90030018, 0x90030430 },
    { 0x8079947c, 0x90830018, 0x90830430 },
    { 0x80799490, 0x90030018, 0x90030430 },
    { 0x80798058, 0x80830018, 0x80830430 },
    { 0x807ba0fc, 0x80630018, 0x80630430 },
};
static const u32 itemInstances[] ={ 0x809c3618 };
static PatchSet itemSet("Item::Manager", itemWords, P24_COUNT(itemWords), nullptr, 0, itemInstances, P24_COUNT(itemInstances));

static const WordPatch archiveWords[] ={
    { 0x8053fc68, 0x3860061c, 0x38600b5c },
    { 0x8053fd04, 0x38e0000c, 0x38e00018 },
    { 0x8053fd2c, 0x38630008, 0x3863061c },
    { 0x8053fd34, 0x387f0158, 0x387f08bc },
    { 0x8053fd44, 0x38e0000c, 0x38e00018 },
    { 0x8053ffe0, 0x387d0158, 0x387d08bc },
    { 0x8053ffec, 0x38c0000c, 0x38c00018 },
    { 0x8053fff4, 0x387d0008, 0x387d061c },
    { 0x80540000, 0x38c0000c, 0x38c00018 },
    { 0x80540e5c, 0x3be30008, 0x3be3061c },
    { 0x80540fb0, 0x3be30158, 0x3be308bc },
    { 0x80541104, 0x3be30008, 0x3be3061c },
    { 0x8054133c, 0x3be30008, 0x3be3061c },
    { 0x805413e4, 0x3be30008, 0x3be3061c },
    { 0x80541454, 0x3be30158, 0x3be308bc },
    { 0x805415cc, 0x38630008, 0x3863061c },
    { 0x805415dc, 0x38630158, 0x386308bc },
    { 0x80541e68, 0x38630008, 0x3863061c },
    { 0x80541e7c, 0x38630008, 0x3863061c },
    { 0x80541efc, 0x3bc30008, 0x3bc3061c },
    { 0x80542050, 0x3bc30008, 0x3bc3061c },
    { 0x805423f0, 0x3ba30008, 0x3ba3061c },
};
static const u32 archiveInstances[] ={ 0x809bd738 };
static PatchSet archiveSet("ArchiveMgr", archiveWords, P24_COUNT(archiveWords), nullptr, 0, archiveInstances, P24_COUNT(archiveInstances));

static const WordPatch inputWords[] ={
    { 0x80523158, 0x3860415c, 0x38606560 },
    { 0x80523340, 0x387d03b4, 0x387d4160 },
    { 0x80523350, 0x38e0000c, 0x38e00018 },
    { 0x80523258, 0x387e03b4, 0x387e4160 },
    { 0x80523264, 0x38c0000c, 0x38c00018 },
    { 0x80534168, 0x380303b4, 0x38034160 },
    { 0x80534900, 0x380403b4, 0x38044160 },
};
static const u32 inputInstances[] ={ 0x809bd70c };
static PatchSet inputSet("Input", inputWords, P24_COUNT(inputWords), nullptr, 0, inputInstances, P24_COUNT(inputInstances));

static const WordPatch driverWords[] ={
#include <Race/Players24/DriverMgrPatches.inc>
};
typedef void* (*DriverMgrCtor)(void* mgr);
static void* driverMgrCtor(void* mgr) { return reinterpret_cast<DriverMgrCtor>(Port(0x8078ca5c))(mgr); }
static void* ConstructDriverMgr(void* mgr) {
    driverMgrCtor(mgr);
    u32* unknown178 = reinterpret_cast<u32*>(reinterpret_cast<u8*>(mgr) + 0x4a8);
    for(int id = 12; id < 24; ++id) unknown178[id] = 4;
    return mgr;
}
static const CallPatch driverCalls[] ={
    { 0x8078ca08, 0x4ba9d3c5, AllocZeroed },
    { 0x8078ca14, 0x48000049, ConstructDriverMgr },
};
static const u32 driverInstances[] ={ 0x809c2f38 };
static PatchSet driverSet("DriverMgr", driverWords, P24_COUNT(driverWords), driverCalls, P24_COUNT(driverCalls), driverInstances, P24_COUNT(driverInstances));

static void RespawnOffsetIndex(HookRegs& regs, u32) {
    regs.gpr[28] = (regs.gpr[5] & 0xFF) % vanillaPlayers * 2;
}

static inline s32 DisplacementOf(u32 instruction) { return (s16)(instruction & 0xFFFF); }
static void StoreForUIPlayer(HookRegs& regs, u32 original) {
    if(regs.gpr[0] >= vanillaPlayers) return;
    const u32 address = regs.gpr[(original >> 16) & 0x1F] + DisplacementOf(original);
    *reinterpret_cast<u8*>(address) = regs.gpr[(original >> 21) & 0x1F];
}
static void BattleEntryFlag(HookRegs& regs, u32 original) {
    if(regs.gpr[0] >= vanillaPlayers * 0x18) regs.gpr[0] = 0;
    else regs.gpr[0] = *reinterpret_cast<const u8*>(regs.gpr[3] + DisplacementOf(original));
}

static const HookPatch respawnHooks[] ={
    { 0x805189b8, 0x54bc0dfc, RespawnOffsetIndex, false },
    { 0x80584588, 0x9b8304ec, StoreForUIPlayer, false },
    { 0x805845b4, 0x98a31f68, StoreForUIPlayer, false },
    { 0x8058c368, 0x98a304ec, StoreForUIPlayer, false },
    { 0x8058c394, 0x98a31f68, StoreForUIPlayer, false },
    { 0x80579af0, 0x880303c4, BattleEntryFlag, false },
};
static PatchSet respawnSet("Respawn", nullptr, 0, nullptr, 0, nullptr, 0, respawnHooks, P24_COUNT(respawnHooks));

}
}
