#include <kamek.hpp>
#include <Race/Players24/Patches.hpp>
#include <Race/Players24/Players24.hpp>
#include <Race/Players24/Online24.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/SELECT.hpp>
#include <MarioKartWii/Mii/MiiGroup.hpp>
#include <core/rvl/RFL/RFLMiddleDB.hpp>
#include <core/egg/mem/Heap.hpp>
#include <Network/PacketExpansion.hpp>

extern "C" void OSReport(const char* format, ...);

namespace Pulsar {
namespace Players24 {

static const u8 maxAids = 24;
static const u8 noPlayer = 0xFF;

static const RKNet::ControllerSub& CurrentSub(const RKNet::Controller& controller) {
    return controller.subs[controller.currentSub & 1];
}

static u8 playersAt[2][maxAids];

u8 PlayersAtAid(u8 aid) {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if(controller == nullptr || aid >= maxAids) return 0;
    const RKNet::ControllerSub& sub = CurrentSub(*controller);
    if(aid == sub.localAid) return sub.localPlayerCount;
    if((sub.availableAids & (1 << aid)) == 0) return 0;
    return playersAt[controller->currentSub & 1][aid];
}

typedef BOOL (*GetConnectionUserDataFunc)(u8 aid, RKNet::ConnectionUserData* data);
typedef void (*ControllerFunc)(RKNet::Controller* controller);
static void* updateSubsOriginal = nullptr;
static void UpdateSubsAndVR(RKNet::Controller* controller) {
    reinterpret_cast<ControllerFunc>(updateSubsOriginal)(controller);
    const u32 current = controller->currentSub & 1;
    const RKNet::ControllerSub& sub = controller->subs[current];
    for(u8 aid = 0; aid < maxAids; ++aid) {
        u8 count = 0;
        if(aid < vanillaPlayers) count = sub.connectionUserDatas[aid].playersAtConsole;
        else if(aid == sub.localAid) count = sub.localPlayerCount;
        else {
            RKNet::ConnectionUserData data;
            data.playersAtConsole = 0;
            if(reinterpret_cast<GetConnectionUserDataFunc>(Port(0x800d4ac8))(aid, &data)) count = data.playersAtConsole;
        }
        playersAt[current][aid] = count;
    }
}

u8 RoomPlayerCount() {
    u8 total = 0;
    for(u8 aid = 0; aid < maxAids; ++aid) total += PlayersAtAid(aid);
    return total;
}

typedef void (*SelectFunc)(RKNet::SELECTHandler* handler);
static void AllocatePlayerIds(RKNet::SELECTHandler* handler) {
    if(!IsConsoles24()) {
        reinterpret_cast<SelectFunc>(Port(0x80662034))(handler);
        return;
    }
    u8 table[maxPlayers];
    for(u8 id = 0; id < maxPlayers; ++id) table[id] = noPlayer;
    u8 count = 0;
    for(u8 aid = 0; aid < maxAids; ++aid) {
        const u8 players = PlayersAtAid(aid);
        for(u8 i = 0; i < players && count < maxPlayers; ++i) table[count++] = aid;
    }
    Network::PulSELECT& packet = reinterpret_cast<Network::ExpSELECTHandler*>(handler)->toSendPacket;
    for(u8 id = 0; id < vanillaPlayers; ++id) {
        packet.playerIdToAid[id] = table[id];
        packet.playerIdToAidExt[id] = table[vanillaPlayers + id];
    }
}

static bool hostingRoom24 = false;
static void FriendRoomSize(HookRegs& regs, u32) {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    hostingRoom24 = CanHostRoom24() && controller != nullptr && CurrentSub(*controller).localPlayerCount == 1;
    regs.gpr[3] = hostingRoom24 ? maxPlayers : vanillaPlayers;
}
static void PublicRoomSize(HookRegs&, u32) {
    hostingRoom24 = false;
}
bool IsHostingRoom24() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    return hostingRoom24 && controller != nullptr && controller->roomType == RKNet::ROOMTYPE_FROOM_HOST;
}

static void RoomPlayerLimit(HookRegs& regs, u32) {
    if(!hostingRoom24) regs.gpr[0] = vanillaPlayers;
    else regs.gpr[0] = regs.gpr[3] > 1 ? 0 : maxPlayers;
}

static RFL::MiddleDB extraPlayerMiddleDB[maxAids - vanillaPlayers];
static bool hasExtraMiddleDB = false;

static RFL::MiddleDB* PlayerMiddleDB(u32 aid) {
    if(aid < vanillaPlayers || aid >= maxAids || !hasExtraMiddleDB) return nullptr;
    return &extraPlayerMiddleDB[aid - vanillaPlayers];
}

typedef u32 (*MiddleDBSizeFunc)(u16 count);
typedef void (*InitMiddleDBFunc)(RFL::MiddleDB* db, RFL::MiddleDBType type, void* buffer, u16 count);
static void CreateExtraMiddleDBs(EGG::Heap* heap) {
    if(hasExtraMiddleDB || heap == nullptr) return;
    const u32 size = reinterpret_cast<MiddleDBSizeFunc>(Port(0x800c8850))(2);
    for(u32 i = 0; i < maxAids - vanillaPlayers; ++i) {
        void* buffer = EGG::Heap::alloc(size, 4, heap);
        if(buffer == nullptr) {
            OSReport("[VK 24P] no Mii buffers for aids 12-23\n");
            return;
        }
        reinterpret_cast<InitMiddleDBFunc>(Port(0x800c8860))(&extraPlayerMiddleDB[i], RFL::RFLMiddleDBType_WiFi, buffer, 2);
    }
    hasExtraMiddleDB = true;
}
static void NewMiiManager(HookRegs& regs, u32) { CreateExtraMiddleDBs(reinterpret_cast<EGG::Heap*>(regs.gpr[30])); }
static const u32 miiManagerHeap = 0x26c;

static void PlayerMiddleDBSource(HookRegs& regs, u32) {
    RFL::MiddleDB* db = PlayerMiddleDB(regs.gpr[5] & 0xFFFF);
    if(db != nullptr) regs.gpr[0] = reinterpret_cast<u32>(db);
}

typedef BOOL (*WiFiInfo2MiddleDBFunc)(void* manager, const void* info, u32 aid, u32 slot);
typedef s32 (*WiFiInfo2MiddleDBCoreFunc)(RFL::MiddleDB* db, const void* info, u32 aid, u32 slot);
static void MiiOfAid(HookRegs& regs, u32) {
    const u32 aid = regs.gpr[5] & 0xFFFF;
    if(aid >= vanillaPlayers) {
        if(!hasExtraMiddleDB) CreateExtraMiddleDBs(*reinterpret_cast<EGG::Heap**>(regs.gpr[3] + miiManagerHeap));
        RFL::MiddleDB* db = PlayerMiddleDB(aid);
        if(db == nullptr) {
            regs.gpr[3] = 0;
            return;
        }
        const s32 error = reinterpret_cast<WiFiInfo2MiddleDBCoreFunc>(Port(0x800cc550))(db,
            reinterpret_cast<const void*>(regs.gpr[4]), aid, regs.gpr[6]);
        regs.gpr[3] = error == 0 || error == 10;
        return;
    }
    regs.gpr[3] = reinterpret_cast<WiFiInfo2MiddleDBFunc>(Port(0x80529748))(reinterpret_cast<void*>(regs.gpr[3]),
        reinterpret_cast<const void*>(regs.gpr[4]), regs.gpr[5], regs.gpr[6]);
}

static Mii* GetMiiChecked(const MiiGroup* group, u8 idx) {
    if(idx >= group->miiCount) return RacerMiiPast12(group, idx);
    return group->mii[idx];
}

typedef s32 (*GetPlayerIdFunc)(void* manager, u8 aid, u8 hudSlotId);
static s32 PlayerIdForUI(void* manager, u8 aid, u8 hudSlotId) {
    const s32 id = reinterpret_cast<GetPlayerIdFunc>(Port(0x80650ddc))(manager, aid, hudSlotId);
    if(id < 0 && IsConsoles24()) return 0;
    return id;
}

static const WordPatch words[] ={
    { 0x80660094, 0x2803000b, 0x28030017 },
    { 0x80660994, 0x2803000b, 0x28030017 },
    { 0x80661968, 0x2803000b, 0x28030017 },
};

static const CallPatch calls[] ={
    { 0x80661498, 0x48000b9d, reinterpret_cast<const void*>(AllocatePlayerIds), false },
    { 0x805fa930, 0x80630004, reinterpret_cast<const void*>(GetMiiChecked), true },
    { 0x80644388, 0x4800ca55, reinterpret_cast<const void*>(PlayerIdForUI), false },
    { 0x805f58c0, 0x4805b51d, reinterpret_cast<const void*>(PlayerIdForUI), false },
};
static const HookPatch hooks[] ={
    { 0x80657788, 0x3860000c, FriendRoomSize, false },
    { 0x80659574, 0x3860000c, PublicRoomSize, true },
    { 0x806590e8, 0x3860000c, PublicRoomSize, true },
    { 0x806587c4, 0x3800000c, RoomPlayerLimit, false },
    { 0x80662f84, 0x4bec67c5, MiiOfAid, false },
    { 0x80527030, 0x38000000, NewMiiManager, true },
    { 0x8052652c, 0x90070000, PlayerMiddleDBSource, true },
};
static const FuncHook funcs[] ={
    { 0x80658de0, 0x9421ffd0, reinterpret_cast<const void*>(UpdateSubsAndVR), &updateSubsOriginal },
};
static PatchSet consolesSet("Online 24 consoles", words, P24_COUNT(words), calls, P24_COUNT(calls), nullptr, 0, hooks,
    P24_COUNT(hooks), funcs, P24_COUNT(funcs), false, true);

void ApplyConsoles() {
    if(!IsSupportedDisc() || !consolesSet.Check()) return;
    consolesSet.Apply();
}

}
}
