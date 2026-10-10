#include <kamek.hpp>
#include <Race/Players24/Patches.hpp>
#include <Race/Players24/Players24.hpp>
#include <Race/Players24/Online24.hpp>
#include <MarioKartWii/Mii/MiiGroup.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <core/egg/mem/ExpHeap.hpp>
#include <core/RK/RKSystem.hpp>
#include <Network/Rating/StaffBadge.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>

namespace Pulsar {
namespace Players24 {

static const u32 extraMiisTextures = 0x4;
static const u32 extraMiisSize = 0x10000;
static const u32 extraMiisMargin = 0x120000 + 0x200000;
static MiiGroup* extraMiis = nullptr;
static u32 extraMiisLoaded = 0;
static bool MiisSetReady();

static const u32 menuMiisHeapSize = 0x10000;
static const u32 menuMiisNeeded = 0xf000;
static const u32 systemHeapMargin = 0x40000;
static EGG::ExpHeap* menuMiisHeap = nullptr;
static bool inMenuHeap = false;

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

#ifdef P24TEST
void ReportRaceMemory(const char* step) {
    const GameScene* scene = GameScene::GetCurrent();
    if(scene == nullptr || scene->id != SCENE_ID_RACE) return;
    EGG::Heap* mem1 = scene->structsHeaps.heaps[0];
    EGG::Heap* mem2 = scene->structsHeaps.heaps[1];
    OSReport("[VK 24P] memory %s: MEM1 %u KiB, MEM2 %u KiB free\n", step,
        mem1 != nullptr ? mem1->getAllocatableSize(4) >> 10 : 0, mem2 != nullptr ? mem2->getAllocatableSize(4) >> 10 : 0);
}
#endif

static u32 reportedNoTexture = 0;

static void FillGroup(MiiGroup* group, const SectionMgr* mgr) {
    for(u8 id = vanillaPlayers; id < maxPlayers; ++id) {
        u8 aid, slot;
        bool isLocal;
        if(!GetRacerConsole(id, aid, slot, isLocal)) {
            if(AidOfPlayer(id) < maxPlayers) OSReport("[VK 24P] Mii of racer %d: no console (aid %d)\n", id, AidOfPlayer(id));
#ifdef P24TEST
            const RKNet::Controller* controller = RKNet::Controller::sInstance;
            aid = AidOfPlayer(id);
            if(controller == nullptr || aid >= maxPlayers) continue;
            slot = 0;
            isLocal = aid == controller->subs[controller->currentSub & 1].localAid;
#else
            continue;
#endif
        }
        const u32 idx = id - vanillaPlayers;
        if(isLocal) reinterpret_cast<CopyMiiFunc>(Port(0x805faf34))(group, &mgr->sectionParams->localPlayerMiis, slot, idx);
        else reinterpret_cast<PlayerMiiFunc>(Port(0x805fa8b8))(group, idx, aid, slot);
        if(group->mii[idx] != nullptr) extraMiisLoaded |= 1 << idx;
        else OSReport("[VK 24P] Mii of racer %d (aid %d, slot %d%s) not loaded\n", id, aid, slot, isLocal ? ", ours" : "");
    }
    typedef void (*DrawQueuedFunc)(void* maker);
    void* maker = *reinterpret_cast<void**>(Port(0x809c2df0));
    if(maker != nullptr && extraMiisLoaded != 0) reinterpret_cast<DrawQueuedFunc>(Port(0x80782528))(maker);
}

static void ResetRacerMiis() {
    if(inMenuHeap && extraMiis != nullptr) delete extraMiis;
    inMenuHeap = false;
    extraMiis = nullptr;
    extraMiisLoaded = 0;
    reportedNoTexture = 0;
}

static EGG::ExpHeap* MenuMiisHeap() {
    if(menuMiisHeap != nullptr) return menuMiisHeap;
    EGG::ExpHeap* system = RKSystem::mInstance.EGGSystem;
    const u32 free = system != nullptr ? system->getAllocatableSize(4) : 0;
    if(free >= menuMiisHeapSize + systemHeapMargin) menuMiisHeap = EGG::ExpHeap::Create(menuMiisHeapSize, system, 0);
    OSReport("[VK 24P] Miis 13-24: system heap %u KiB free, %s\n", free >> 10,
        menuMiisHeap != nullptr ? "64 KiB kept for them" : "they load with the race");
    return menuMiisHeap;
}

void LoadRacerMiisInMenu() {
    ResetRacerMiis();
    if(!IsOnline24() || AidOfPlayer(vanillaPlayers) == 0xFF || !MiisSetReady()) return;
    const SectionMgr* mgr = SectionMgr::sInstance;
    if(mgr == nullptr || mgr->sectionParams == nullptr || mgr->curSection == nullptr) return;
    EGG::ExpHeap* heap = MenuMiisHeap();
    if(heap == nullptr) return;
    if(heap->getAllocatableSize(0x20) < menuMiisNeeded) {
        OSReport("[VK 24P] Miis 13-24: %u KiB in their heap, they load with the race\n", heap->getAllocatableSize(0x20) >> 10);
        return;
    }
    const EGG::Heap* previous = heap->BecomeCurrentHeap();
    MiiGroup* group = new(heap, 4) MiiGroup;
    group->Init(vanillaPlayers, extraMiisTextures, heap);
    previous->BecomeCurrentHeap();
    extraMiis = group;
    inMenuHeap = true;
    FillGroup(group, mgr);
    OSReport("[VK 24P] Miis 13-24: loaded %03x in the menu, %u KiB left in their heap\n", extraMiisLoaded,
        heap->getAllocatableSize(4) >> 10);
}

static void LoadRacerMiis() {
    if(inMenuHeap) return;
    ResetRacerMiis();
    if(!IsOnline24()) return;
    if(AidOfPlayer(vanillaPlayers) == 0xFF) {
        if(RoomPlayerCount() > vanillaPlayers) OSReport("[VK 24P] Miis 13-24: none (no id 12, %d in the room)\n", RoomPlayerCount());
        return;
    }
    if(!MiisSetReady()) {
        OSReport("[VK 24P] Miis 13-24: none (patches not in)\n");
        return;
    }
    const GameScene* scene = GameScene::GetCurrent();
    const SectionMgr* mgr = SectionMgr::sInstance;
    if(scene == nullptr || scene->id != SCENE_ID_RACE || mgr == nullptr || mgr->sectionParams == nullptr) {
        OSReport("[VK 24P] Miis 13-24: none (scene %d)\n", scene != nullptr ? scene->id : -1);
        return;
    }
    EGG::Heap* heap = scene->structsHeaps.heaps[1];
    const u32 free = heap != nullptr ? heap->getAllocatableSize(4) : 0;
    if(free < extraMiisSize + extraMiisMargin) {
        OSReport("[VK 24P] Miis 13-24: %x free in MEM2, the racers keep their character\n", free);
        return;
    }
    MiiGroup* group = new(heap, 4) MiiGroup;
    group->Init(vanillaPlayers, extraMiisTextures, heap);
    extraMiis = group;
    FillGroup(group, mgr);
    OSReport("[VK 24P] Miis 13-24: loaded %03x, %u KiB free in MEM2\n", extraMiisLoaded, heap->getAllocatableSize(4) >> 10);
}

static bool raceSceneLoading = false;

typedef void (*LoadSectionFunc)(SectionMgr* mgr);
static void LoadRaceSection(SectionMgr* mgr) {
    raceSceneLoading = true;
    reinterpret_cast<LoadSectionFunc>(Port(0x80634fbc))(mgr);
    raceSceneLoading = false;
}

typedef void (*InitSectionFunc)(void* section, u32 id);
static void InitSection(void* section, u32 id) {
    if(raceSceneLoading) {
        ReportRaceMemory("before the Miis");
        LoadRacerMiis();
        ReportRaceMemory("after the Miis");
    }
    else if(extraMiis != nullptr && !inMenuHeap && GameScene::GetCurrent() != nullptr
        && GameScene::GetCurrent()->id != SCENE_ID_RACE) {
        extraMiis = nullptr;
        extraMiisLoaded = 0;
    }
    reinterpret_cast<InitSectionFunc>(Port(0x80621e00))(section, id);
    if(raceSceneLoading) ReportRaceMemory("after the race UI");
}

Mii* RacerMiiPast12(const MiiGroup* group, u8 idx) {
    if(!HasRacerMii(idx) || group != PlayerMiis()) return nullptr;
    return extraMiis->mii[idx - vanillaPlayers];
}

static void* GetMiiTexObj(const MiiGroup* group, u32 idx, u32 texture) {
    if(idx >= group->miiCount) {
        if(!HasRacerMii(idx) || group != PlayerMiis() || extraMiis->texObj[texture] == nullptr) {
            if(group == PlayerMiis() && idx < maxPlayers && (reportedNoTexture >> (idx - vanillaPlayers) & 1) == 0) {
                reportedNoTexture |= 1 << (idx - vanillaPlayers);
                OSReport("[VK 24P] Mii texture %d of racer %d asked: %s\n", texture, idx,
                    HasRacerMii(idx) ? "not in the group" : "no Mii");
            }
            return nullptr;
        }
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
    { 0x80635064, 0x4bfecd9d, reinterpret_cast<const void*>(InitSection), false },
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
