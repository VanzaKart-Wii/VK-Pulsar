#include <kamek.hpp>
#include <Race/Players24/Patches.hpp>
#include <Race/Players24/Players24.hpp>
#include <Race/Players24/Online24.hpp>
#include <MarioKartWii/Mii/MiiGroup.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <core/egg/mem/Heap.hpp>
#include <Network/Rating/StaffBadge.hpp>

namespace Pulsar {
namespace Players24 {

static const u32 playerMiisTextures = 0x24;
static const u32 extraMiisSize = 0x16000;
static const u32 extraMiisMargin = 0x100000;
static MiiGroup* extraMiis = nullptr;
static u32 extraMiisLoaded = 0;
static bool MiisSetReady();

static MiiGroup* PlayerMiis() {
    const SectionMgr* mgr = SectionMgr::sInstance;
    if(mgr == nullptr || mgr->sectionParams == nullptr) return nullptr;
    return &mgr->sectionParams->playerMiis;
}

bool HasRacerMii(u8 id) {
    if(id < vanillaPlayers || id >= maxPlayers || (extraMiisLoaded >> (id - vanillaPlayers) & 1) == 0) return false;
    const GameScene* scene = GameScene::GetCurrent();
    return scene != nullptr && scene->id == SCENE_ID_RACE && IsOnline24();
}

typedef void (*CopyMiiFunc)(MiiGroup* dest, const MiiGroup* source, u32 sourceIdx, u32 destIdx);
typedef void (*PlayerMiiFunc)(MiiGroup* group, u32 idx, u32 aid, u32 slot);

extern "C" void OSReport(const char* format, ...);

static void LoadRacerMiis() {
    extraMiisLoaded = 0;
    extraMiis = nullptr;
    if(!MiisSetReady() || !IsOnline24() || AidOfPlayer(vanillaPlayers) == 0xFF) return;
    const GameScene* scene = GameScene::GetCurrent();
    const SectionMgr* mgr = SectionMgr::sInstance;
    if(scene == nullptr || scene->id != SCENE_ID_RACE || mgr == nullptr || mgr->sectionParams == nullptr) return;
    EGG::Heap* heap = scene->structsHeaps.heaps[1];
    const u32 free = heap != nullptr ? heap->getAllocatableSize(4) : 0;
    if(free < extraMiisSize + extraMiisMargin) {
        OSReport("[VK 24P] Miis 13-24: %x free in MEM2, the racers keep their character\n", free);
        return;
    }
    MiiGroup* group = new(heap, 4) MiiGroup;
    group->Init(vanillaPlayers, playerMiisTextures, heap);
    extraMiis = group;
    for(u8 id = vanillaPlayers; id < maxPlayers; ++id) {
        u8 aid, slot;
        bool isLocal;
        if(!GetRacerConsole(id, aid, slot, isLocal)) continue;
        const u32 idx = id - vanillaPlayers;
        if(isLocal) reinterpret_cast<CopyMiiFunc>(Port(0x805faf34))(group, &mgr->sectionParams->localPlayerMiis, slot, idx);
        else reinterpret_cast<PlayerMiiFunc>(Port(0x805fa8b8))(group, idx, aid, slot);
        if(group->mii[idx] != nullptr) extraMiisLoaded |= 1 << idx;
        else OSReport("[VK 24P] Mii of racer %d (aid %d, slot %d) not loaded\n", id, aid, slot);
    }
}

typedef void (*LoadSectionFunc)(SectionMgr* mgr);
static void LoadRaceSection(SectionMgr* mgr) {
    LoadRacerMiis();
    reinterpret_cast<LoadSectionFunc>(Port(0x80634fbc))(mgr);
}

Mii* RacerMiiPast12(const MiiGroup* group, u8 idx) {
    if(!HasRacerMii(idx) || group != PlayerMiis()) return nullptr;
    return extraMiis->mii[idx - vanillaPlayers];
}

static void* GetMiiTexObj(const MiiGroup* group, u32 idx, u32 texture) {
    if(idx >= group->miiCount) {
        if(!HasRacerMii(idx) || group != PlayerMiis()) return nullptr;
        group = extraMiis;
        idx -= vanillaPlayers;
    }
    return reinterpret_cast<u8*>(group->texObj[texture]) + idx * sizeof(MiiTexObj);
}

static void BalloonMiiCount(HookRegs& regs, u32) {
    u32 count = *reinterpret_cast<const u32*>(regs.gpr[3] + 0x1b0);
    if(extraMiisLoaded != 0 && IsOnline24()) count = maxPlayers;
    regs.gpr[0] = count;
}

static void StaffBadgePast12(HookRegs& regs, u32) {
    const u8 id = regs.gpr[0] >> 2;
    if(id < vanillaPlayers) {
        regs.gpr[5] = *reinterpret_cast<const u32*>(regs.gpr[5] + 0x35c);
        return;
    }
    u8 aid, slot;
    bool isLocal;
    PointRating::Staff::Role role = PointRating::Staff::ROLE_NONE;
    if(GetRacerConsole(id, aid, slot, isLocal)) {
        role = isLocal ? PointRating::Staff::GetLocal(slot) : PointRating::Staff::GetRemote(aid, slot);
    }
    regs.gpr[5] = PointRating::Staff::GetIconBMG(role);
}

static const CallPatch calls[] ={
    { 0x805fa964, 0x54a0103a, reinterpret_cast<const void*>(GetMiiTexObj), true },
    { 0x80554694, 0x480e0929, reinterpret_cast<const void*>(LoadRaceSection), false },
};
static const HookPatch hooks[] ={
    { 0x807f04ec, 0x800301b0, BalloonMiiCount, false },
    { 0x807f05d8, 0x80a5035c, StaffBadgePast12, false },
    { 0x807f523c, 0x80a5035c, StaffBadgePast12, false },
    { 0x807f6244, 0x80a5035c, StaffBadgePast12, false },
};
static PatchSet miisSet("Online 24 Miis", nullptr, 0, calls, P24_COUNT(calls), nullptr, 0, hooks, P24_COUNT(hooks),
    nullptr, 0, true);

static bool MiisSetReady() { return IsReady() && !miisSet.skipped; }

}
}
