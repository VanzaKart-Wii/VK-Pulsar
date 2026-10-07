#include <kamek.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <Race/Players24/Players24.hpp>
#include <Race/Players24/Patches.hpp>
#include <Race/Players24/Online24.hpp>

extern "C" void OSReport(const char* format, ...);

namespace Pulsar {
namespace Players24 {

static inline u32 RegD(u32 word) { return (word >> 21) & 0x1F; }
static inline u32 RegA(u32 word) { return (word >> 16) & 0x1F; }
static inline u32 RegB(u32 word) { return (word >> 11) & 0x1F; }

static bool IsSplitActive() {
    if(!IsReady() || !HasExtraRacers()) return false;
    const Racedata* racedata = Racedata::sInstance;
    return racedata != nullptr && racedata->racesScenario.settings.gamemode != MODE_AWARD;
}
static void SetRaceCount(u8 count) { Racedata::sInstance->racesScenario.playerCount = count; }

static u8 countBeforeEffectInfo = 0;
static bool isEffectInfoCountRaised = false;
static void BeginEffectInfo(HookRegs& regs, u32) {
    isEffectInfoCountRaised = false;
    if(!IsReady() || regs.gpr[4] != 2) return;
    RacedataScenario& menu = Racedata::sInstance->menusScenario;
    if(!IsVSMode(menu.settings.gamemode)) return;
    u8 count = 0;
    for(u8 id = 0; id < maxPlayers; ++id) if(GetPlayer(menu, id).playerType != PLAYER_NONE) ++count;
    if(count <= vanillaPlayers) return;
    countBeforeEffectInfo = Racedata::sInstance->racesScenario.playerCount;
    SetRaceCount(count);
    isEffectInfoCountRaised = true;
}
static void EndEffectInfo(HookRegs&, u32) {
    if(isEffectInfoCountRaised) SetRaceCount(countBeforeEffectInfo);
    isEffectInfoCountRaised = false;
}

static void BeginRaceCreation(HookRegs&, u32) {
    if(IsSplitActive()) SetRaceCount(GetRacePlayerCount());
}
typedef EGG::Heap* (*CreateHeapFunc)(GameScene* scene, u32 size, u32 parentHeapIdx);
static const u32 raceSceneHeapSize = 0x180000;
static const u32 mem2Spare = 0x10000;
static const u32 mem1Spare = 0x100000;
static EGG::Heap* CreateRaceSceneHeap(GameScene* scene, u32 size, u32 parentHeapIdx) {
    if(IsReady() && HasExtraRacers()) {
        EGG::Heap* mem1 = scene->structsHeaps.heaps[0];
        EGG::Heap* mem2 = scene->structsHeaps.heaps[1];
        const u32 free1 = mem1 != nullptr ? mem1->getAllocatableSize(-8) : 0;
        const u32 free2 = mem2 != nullptr ? mem2->getAllocatableSize(-8) : 0;
        size = raceSceneHeapSize;
        if(free2 >= size + mem2Spare) parentHeapIdx = 1;
        else if(free1 >= size + mem1Spare) parentHeapIdx = 0;
        else {
            const u32 room1 = free1 > mem1Spare ? free1 - mem1Spare : 0;
            const u32 room2 = free2 > mem2Spare ? free2 - mem2Spare : 0;
            parentHeapIdx = room1 > room2 ? 0 : 1;
            const u32 room = (room1 > room2 ? room1 : room2) & ~0x1F;
            size = room > size ? size : room < 0x80000 ? 0x80000 : room;
        }
    }
    return reinterpret_cast<CreateHeapFunc>(Port(0x8051b8e4))(scene, size, parentHeapIdx);
}

static const u32 raceUIRoom = 0x180000;
static void RaceUIHeap(HookRegs& regs, u32) {
    GameScene* scene = reinterpret_cast<GameScene*>(regs.gpr[4]);
    u32 heapIdx = 1;
    if(IsReady() && HasExtraRacers() && scene->id == SCENE_ID_RACE) {
        EGG::Heap* mem1 = scene->structsHeaps.heaps[0];
        EGG::Heap* mem2 = scene->structsHeaps.heaps[1];
        const u32 free1 = mem1 != nullptr ? mem1->getAllocatableSize(-8) : 0;
        const u32 free2 = mem2 != nullptr ? mem2->getAllocatableSize(-8) : 0;
        if(free2 < raceUIRoom && free1 >= raceUIRoom + mem1Spare) heapIdx = 0;
    }
    regs.gpr[4] = reinterpret_cast<u32>(scene->structsHeaps.heaps[heapIdx]);
}

static const u32 raceEffectsRoom = 0x200000;
typedef void (*InitRaceEffectsFunc)(void* mgr);
typedef EGG::Heap* (*BecomeCurrentFunc)(const EGG::Heap* heap);
static void InitRaceEffects(void* mgr) {
    EGG::Heap* previous = nullptr;
    const GameScene* scene = GameScene::GetCurrent();
    if(IsReady() && HasExtraRacers() && scene != nullptr && scene->id == SCENE_ID_RACE) {
        EGG::Heap* mem2 = scene->structsHeaps.heaps[1];
        if(mem2 != nullptr && mem2->getAllocatableSize(-8) >= raceEffectsRoom) {
            previous = reinterpret_cast<BecomeCurrentFunc>(Port(0x80229d74))(mem2);
        }
    }
    ReportRaceMemory("before the effects");
    reinterpret_cast<InitRaceEffectsFunc>(Port(0x8067ca4c))(mgr);
    ReportRaceMemory("after the effects");
    if(previous != nullptr) reinterpret_cast<BecomeCurrentFunc>(Port(0x80229d74))(previous);
}

static void BeginRaceGraphics(HookRegs&, u32) {
    if(!IsSplitActive()) return;
    SetRaceCount(vanillaPlayers);
}
static void EndRaceGraphics(HookRegs&, u32) {
    if(!IsSplitActive()) return;
    SetRaceCount(vanillaPlayers);
}
static void BeginSimulationFrame(HookRegs&, u32) {
    if(IsSplitActive()) SetRaceCount(GetRacePlayerCount());
}
static void BeginPresentationFrame(HookRegs&, u32) {
    if(IsSplitActive()) SetRaceCount(vanillaPlayers);
}

static void EffectGroup(HookRegs& regs, u32 original) {
    u32 group = regs.gpr[RegA(original)] + 2;
    if(group >= 14) group += 2;
    regs.gpr[RegD(original)] = group;
}

static void AIGroupCounts(HookRegs& regs, u32 original) {
    regs.gpr[26] = *reinterpret_cast<u32*>(regs.gpr[RegA(original)] + regs.gpr[RegB(original)]);
    const Racedata* racedata = Racedata::sInstance;
    const s32 cpus = (s32)regs.gpr[22] - racedata->racesScenario.localPlayerCount;
    if(cpus <= 12) return;
    regs.gpr[28] = 3;
    regs.gpr[27] = cpus - 6;
    regs.gpr[26] = 3;
    regs.cr = (regs.cr & 0x0FFFFFFF) | 0x40000000;
}

static const u32 expandedGridColumns = 6;
static bool IsExpandedGridSlot(const HookRegs& regs) { return regs.gpr[28] >= 0x90 || regs.gpr[26] >= 12; }
static void GridFirstColumn(HookRegs& regs, u32 original) {
    if(IsExpandedGridSlot(regs)) regs.gpr[0] = (u32)-10;
    else regs.gpr[0] = *reinterpret_cast<u8*>(regs.gpr[RegA(original)] + regs.gpr[RegB(original)]);
}
static void GridRow(HookRegs& regs, u32 original) {
    if(IsExpandedGridSlot(regs)) regs.gpr[0] = regs.gpr[26] / expandedGridColumns;
    else regs.gpr[0] = *reinterpret_cast<u8*>(regs.gpr[RegA(original)] + regs.gpr[RegB(original)]);
}
static void GridColumn(HookRegs& regs, u32 original) {
    if(IsExpandedGridSlot(regs)) {
        const u32 slot = regs.gpr[26];
        const u32 row = slot / expandedGridColumns;
        const u32 column = slot - row * expandedGridColumns;
        regs.gpr[0] = (u32)((s32)(column * 4) - 10);
    }
    else regs.gpr[0] = *reinterpret_cast<u8*>(regs.gpr[RegA(original)] + regs.gpr[RegB(original)]);
}

static void LightningIndex(HookRegs& regs, u32 original) {
    const u32 count = regs.gpr[RegA(original)];
    s32 index;
    if(count <= 12) index = (s32)regs.gpr[0] - (s32)count;
    else index = ((s32)regs.gpr[0] - 12) * 11 / (s32)(count - 1);
    if(index < 0) index = 0;
    if(index > 11) index = 11;
    regs.gpr[0] = index;
}

static void BlooperClear(HookRegs& regs, u32 original) {
    u32* manager = reinterpret_cast<u32*>(regs.gpr[RegA(original)]);
    manager[0x10 / 4] = regs.gpr[0];
    for(int i = 0; i < 12; ++i) manager[0x40 / 4 + i] = regs.gpr[0];
}
static void BlooperDelay(HookRegs& regs, u32 original) {
    u32 offset = regs.gpr[RegB(original)];
    if(offset >= 0x30) offset -= 0x30;
    regs.gpr[RegD(original)] = *reinterpret_cast<u32*>(regs.gpr[RegA(original)] + offset);
}

static void ItemTableSize(HookRegs& regs, u32) {
    const u32 count = regs.gpr[31] < 24 ? 24 : regs.gpr[31];
    regs.gpr[3] = count * 0x26;
}

static void* scaleItemTableOriginal = nullptr;
typedef void (*ScaleFunc)(u8* slotData, u32* table);
static void ScaleItemTable(u8* slotData, u32* table) {
    u32 count = *reinterpret_cast<u32*>(slotData + 0x44);
    if(count <= 12) {
        reinterpret_cast<ScaleFunc>(scaleItemTableOriginal)(slotData, table);
        return;
    }
    if(count > 24) count = 24;
    table[0] = count;
    u8* probabilities = reinterpret_cast<u8*>(table[1]);
    const bool hasRoulette = table == reinterpret_cast<u32*>(slotData + 0x10);
    u8* roulette = *reinterpret_cast<u8**>(slotData + 0x2c);
    const u32 last = count - 1;
    for(s32 column = last; column >= 0; --column) {
        const u32 source = column * 11 / last;
        const u16* src = reinterpret_cast<const u16*>(probabilities + source * 0x26);
        u16* dest = reinterpret_cast<u16*>(probabilities + column * 0x26);
        for(int i = 0; i < 19; ++i) dest[i] = src[i];
        if(hasRoulette) {
            const u32* srcRoulette = reinterpret_cast<const u32*>(roulette + source * 0x50);
            u32* destRoulette = reinterpret_cast<u32*>(roulette + column * 0x50);
            for(int i = 0; i < 20; ++i) destRoulette[i] = srcRoulette[i];
        }
    }
}

static void* updatePositionsOriginal = nullptr;
typedef void (*UpdatePositionsFunc)(u8* raceMode);

static bool IsFinished(const u8* player) { return (*reinterpret_cast<const u32*>(player + 0x38) & 2) != 0; }
static int CompareFinishTimes(const u8* current, const u8* other) {
    const u8* currentTime = *reinterpret_cast<u8* const*>(current + 0x40);
    const u8* otherTime = *reinterpret_cast<u8* const*>(other + 0x40);
    if(otherTime == nullptr) return 0;
    if(currentTime == nullptr) return 1;
    const u16 otherMin = *reinterpret_cast<const u16*>(otherTime + 4);
    const u16 currentMin = *reinterpret_cast<const u16*>(currentTime + 4);
    if(otherMin != currentMin) return otherMin < currentMin ? 1 : -1;
    if(otherTime[6] != currentTime[6]) return otherTime[6] < currentTime[6] ? 1 : -1;
    const u16 otherMs = *reinterpret_cast<const u16*>(otherTime + 8);
    const u16 currentMs = *reinterpret_cast<const u16*>(currentTime + 8);
    if(otherMs != currentMs) return otherMs < currentMs ? 1 : -1;
    return 0;
}

static void UpdateRacePositions(u8* raceMode) {
    u32 count = Racedata::sInstance->racesScenario.playerCount;
    if(count <= 12) {
        reinterpret_cast<UpdatePositionsFunc>(updatePositionsOriginal)(raceMode);
        return;
    }
    if(count > 24) count = 24;
    u8* raceinfo = *reinterpret_cast<u8**>(raceMode + 4);
    u8** players = *reinterpret_cast<u8***>(raceinfo + 0xc);
    u8* playerIdInEachPosition = *reinterpret_cast<u8**>(raceinfo + 0x18);
    u8 previous[24];
    for(u32 id = 0; id < count; ++id) {
        const u8 position = players[id][0x20];
        previous[id] = (position >= 1 && position <= count) ? position : id + 1;
    }
    for(u32 id = 0; id < count; ++id) {
        u8* current = players[id];
        const float progress = *reinterpret_cast<float*>(current + 0xc);
        u8 rank = 1;
        for(u32 otherId = 0; otherId < count; ++otherId) {
            if(otherId == id) continue;
            const u8* other = players[otherId];
            int order;
            if(IsFinished(current)) order = IsFinished(other) ? CompareFinishTimes(current, other) : -1;
            else if(IsFinished(other)) order = 1;
            else {
                const float otherProgress = *reinterpret_cast<const float*>(other + 0xc);
                order = otherProgress > progress ? 1 : otherProgress < progress ? -1 : 0;
            }
            if(order == 0) {
                if(previous[otherId] != previous[id]) order = previous[otherId] < previous[id] ? 1 : -1;
                else order = otherId < id ? 1 : -1;
            }
            if(order > 0) ++rank;
        }
        current[0x20] = rank;
        playerIdInEachPosition[rank - 1] = id;
    }
}

static bool Uses24Scoring(const RacedataScenario& menu) {
    if(!IsReady() || !HasExtraRacers()) return false;
    const RacedataSettings& settings = menu.settings;
    return IsVSMode(settings.gamemode) && settings.gametype == 0 && (settings.modeFlags & 2) == 0;
}

static void ComputeOverallRanks(RacedataScenario& menu) {
    const u8 count = GetRacePlayerCount();
    for(u8 id = 0; id < count; ++id) {
        const RacedataPlayer& player = GetPlayer(menu, id);
        u8 rank = 1;
        for(u8 other = 0; other < count; ++other) {
            if(other == id) continue;
            const RacedataPlayer& rival = GetPlayer(menu, other);
            if(rival.score > player.score || (rival.score == player.score && rival.finishPos < player.finishPos)) ++rank;
            else if(rival.score == player.score && rival.finishPos == player.finishPos && other < id) ++rank;
        }
        GetPlayer(menu, id).unknown_0xe0 = rank;
    }
}

static void FixOverallRanks(HookRegs&, u32) {
    RacedataScenario& menu = Racedata::sInstance->menusScenario;
    if(Uses24Scoring(menu)) ComputeOverallRanks(menu);
}

bool UpdatePoints24(RacedataScenario& menu) {
    if(!Uses24Scoring(menu)) return false;
    const u8* raceinfo = *reinterpret_cast<u8* const*>(Port(0x809bd730));
    if(raceinfo == nullptr) return false;
    u8* const* players = *reinterpret_cast<u8* const* const*>(raceinfo + 0xc);
    const u8 count = GetRacePlayerCount();
    for(u8 id = 0; id < count; ++id) {
        const u8 rank = players[id][0x20];
        if(rank < 1 || rank > count) continue;
        RacedataPlayer& player = GetPlayer(menu, id);
        player.finishPos = rank;
        const u16 points = rank == 1 ? 27 : rank == 2 ? 24 : 25 - rank;
        player.score = player.previousScore + points;
    }
    ComputeOverallRanks(menu);
    return true;
}

static const WordPatch words[] ={
#include <Race/Players24/Offline24Patches.inc>
};

static const HookPatch hooks[] ={
    { 0x8085cc2c, 0xbac10008, FixOverallRanks, true },
    { 0x8051a6c0, 0x48160ec1, BeginEffectInfo, true },
    { 0x8051a6c4, 0x3c608038, EndEffectInfo, true },
    { 0x80554270, 0x4bfc6d15, BeginRaceCreation, true },
    { 0x80621f4c, 0x80840c98, RaceUIHeap, false },
    { 0x805af698, 0x480000dd, BeginRaceGraphics, true },
    { 0x805af69c, 0x480000c0, EndRaceGraphics, true },
    { 0x80554b4c, 0x807fd730, BeginSimulationFrame, true },
    { 0x80554ce8, 0x48127ea1, BeginPresentationFrame, true },
    { 0x8068f064, 0x38840002, EffectGroup, false },
    { 0x8067cd0c, 0x389d0002, EffectGroup, false },
    { 0x8067d60c, 0x38b30002, EffectGroup, false },
    { 0x8067d6a0, 0x38b30002, EffectGroup, false },
    { 0x8067d8ac, 0x38be0002, EffectGroup, false },
    { 0x80741aa8, 0x7f43302e, AIGroupCounts, false },
    { 0x80514554, 0x7c1fe0ae, GridFirstColumn, false },
    { 0x80514598, 0x7c1a00ae, GridRow, false },
    { 0x805146c8, 0x7c1a00ae, GridColumn, false },
    { 0x805804f0, 0x7c070050, LightningIndex, false },
    { 0x805805c0, 0x7c070050, LightningIndex, false },
    { 0x80580738, 0x7c080050, LightningIndex, false },
    { 0x807a86b4, 0x901b0010, BlooperClear, false },
    { 0x807a905c, 0x901b0010, BlooperClear, false },
    { 0x807a9744, 0x90030010, BlooperClear, false },
    { 0x807a9344, 0x7c7c002e, BlooperDelay, false },
    { 0x807a94e4, 0x7c7a002e, BlooperDelay, false },
    { 0x807a95c8, 0x7c7ab82e, BlooperDelay, false },
    { 0x807baa20, 0x1c7f0026, ItemTableSize, false },
};

static const FuncHook funcs[] ={
    { 0x805336d8, 0x9421ff40, UpdateRacePositions, &updatePositionsOriginal },
    { 0x807bad20, 0x81040004, ScaleItemTable, &scaleItemTableOriginal },
};

static const CallPatch calls[] ={
    { 0x80554570, 0x4bfc7375, CreateRaceSceneHeap },
    { 0x80554864, 0x481281e9, InitRaceEffects },
};

static PatchSet offline24Set("Offline24 ports", words, P24_COUNT(words), calls, P24_COUNT(calls), nullptr, 0,
    hooks, P24_COUNT(hooks), funcs, P24_COUNT(funcs));

}
}
