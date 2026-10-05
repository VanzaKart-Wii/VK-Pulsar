#include <kamek.hpp>
#include <Race/Players24/Patches.hpp>
#include <Race/Players24/Players24.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/PacketMgr.hpp>
#include <MarioKartWii/RKNet/ROOM.hpp>
#include <MarioKartWii/RKNet/SELECT.hpp>
#include <MarioKartWii/RKNet/USER.hpp>
#include <MarioKartWii/RKNet/RH1.hpp>
#include <Debug/Debug.hpp>

extern "C" void OSReport(const char* format, ...);

namespace Pulsar {
namespace Network {
void RememberRecvBuffers(const void* controller);
}
namespace Players24 {

static const u32 firstNewAid = 12;
static const u32 aidCount = 24;

size_assert(RKNet::PacketMgr, 0x228);
size_assert(RKNet::ROOMHandler, 0x140);
size_assert(RKNet::USERHandler, 0x1bf0);
size_assert(RKNet::RH1Handler, 0x6e0);
size_assert(RKNet::SELECTHandler, 0x3f8);

static void CheckLayout() {
    offset_assert(RKNet::Controller, lastRACESendAid, 0x360);
    offset_assert(RKNet::Controller, splitToSendRACEPackets, 0x368);
    offset_assert(RKNet::Controller, splitReceivedRACEPackets, 0x428);
    offset_assert(RKNet::Controller, fullSendPackets, 0x4e8);
    offset_assert(RKNet::Controller, lastRACEToSendTimes, 0x548);
    offset_assert(RKNet::Controller, lastRACERecivedTimes, 0x608);
    offset_assert(RKNet::Controller, RACEToSendTimesTaken, 0x6c8);
    offset_assert(RKNet::Controller, RACEReceivedTimesTaken, 0x788);
    offset_assert(RKNet::Controller, lastSendBufferUsed, 0x848);
    offset_assert(RKNet::Controller, lastReceivedBufferUsed, 0x8a8);
    offset_assert(RKNet::Controller, selectTime350DivTime3B0, 0xba8);
    offset_assert(RKNet::Controller, localStatusData, 0x25e4);
    offset_assert(RKNet::Controller, currentSub, 0x291c);
    offset_assert(RKNet::Controller, biggestRH1Timer, 0x29c0);
    offset_assert(RKNet::ROOMHandler, lastSentToAid, 0x68);
    offset_assert(RKNet::ROOMHandler, toSendPackets, 0x80);
    offset_assert(RKNet::ROOMHandler, receivedPackets, 0xe0);
    offset_assert(RKNet::SELECTHandler, lastReceivedTimes, 0x80);
    offset_assert(RKNet::SELECTHandler, delaysFromPredictedRecvTimes, 0x140);
    offset_assert(RKNet::SELECTHandler, unknown_0x3b0, 0x200);
    offset_assert(RKNet::SELECTHandler, lastSentToAid, 0x2e0);
    offset_assert(RKNet::SELECTHandler, hasNewSELECT, 0x3e0);
    offset_assert(RKNet::USERHandler, aidBitflag, 0x9e0);
    offset_assert(RKNet::USERHandler, receivedPackets, 0x9f0);
    offset_assert(RKNet::RH1Handler, time, 0x18);
    offset_assert(RKNet::RH1Handler, rh1Data, 0x260);
}

static void ClearTimes(RKNet::Controller* controller, u32 firstAid) {
    for(u32 aid = firstAid; aid < aidCount; ++aid) {
        controller->lastRACEToSendTimes[aid] = 0;
        controller->lastRACERecivedTimes[aid] = 0;
        controller->RACEToSendTimesTaken[aid] = 0;
        controller->RACEReceivedTimesTaken[aid] = 0;
    }
}

#ifndef PROD
struct OldArea {
    u16 start;
    u16 end;
};
enum SentinelObject { SENTINEL_CONTROLLER, SENTINEL_PACKETMGR, SENTINEL_ROOM, SENTINEL_SELECT, SENTINEL_USER, SENTINEL_RH1,
    SENTINEL_COUNT };
static const OldArea controllerAreas[] ={ { 0xf0, 0x360 }, { 0x276c, 0x291c }, { 0x2960, 0x29b0 } };
static const OldArea packetMgrAreas[] ={ { 0x198, 0x1c8 } };
static const OldArea roomAreas[] ={ { 0x8, 0x68 } };
static const OldArea selectAreas[] ={ { 0x2f0, 0x3e0 } };
static const OldArea userAreas[] ={ { 0xc8, 0x9c8 } };
static const OldArea rh1Areas[] ={ { 0x20, 0x260 } };
struct Sentinel {
    const char* name;
    void* const* instance;
    const OldArea* areas;
    u32 areaCount;
};
static const Sentinel sentinels[SENTINEL_COUNT] ={
    { "Controller", reinterpret_cast<void* const*>(&RKNet::Controller::sInstance), controllerAreas, P24_COUNT(controllerAreas) },
    { "PacketMgr", reinterpret_cast<void* const*>(&RKNet::PacketMgr::sInstance), packetMgrAreas, P24_COUNT(packetMgrAreas) },
    { "ROOMHandler", reinterpret_cast<void* const*>(&RKNet::ROOMHandler::sInstance), roomAreas, P24_COUNT(roomAreas) },
    { "SELECTHandler", reinterpret_cast<void* const*>(&RKNet::SELECTHandler::sInstance), selectAreas, P24_COUNT(selectAreas) },
    { "USERHandler", reinterpret_cast<void* const*>(&RKNet::USERHandler::sInstance), userAreas, P24_COUNT(userAreas) },
    { "RH1Handler", reinterpret_cast<void* const*>(&RKNet::RH1Handler::sInstance), rh1Areas, P24_COUNT(rh1Areas) },
};
static const u8 poison = 0xA5;
static void* volatile filled[SENTINEL_COUNT];
static u32 reported = 0;

static void Fill(SentinelObject id, void* object) {
    if(object == nullptr) return;
    filled[id] = nullptr;
    const Sentinel& sentinel = sentinels[id];
    for(u32 i = 0; i < sentinel.areaCount; ++i) {
        memset(reinterpret_cast<u8*>(object) + sentinel.areas[i].start, poison, sentinel.areas[i].end - sentinel.areas[i].start);
    }
    filled[id] = object;
}

static void CheckSentinels() {
    for(u32 id = 0; id < SENTINEL_COUNT; ++id) {
        const Sentinel& sentinel = sentinels[id];
        const u8* object = reinterpret_cast<const u8*>(*sentinel.instance);
        if(object == nullptr || object != filled[id] || (reported & (1 << id))) continue;
        for(u32 i = 0; i < sentinel.areaCount; ++i) {
            for(u32 offset = sentinel.areas[i].start; offset < sentinel.areas[i].end; ++offset) {
                if(object[offset] == poison) continue;
                OSReport("[VK 24P] %s: old place +0x%x changed to %02x, a moved array is still used there\n",
                    sentinel.name, offset, object[offset]);
                reported |= 1 << id;
                break;
            }
            if(reported & (1 << id)) break;
        }
    }
}

static void NetworkLoopCheck(HookRegs&, u32) {
    static u32 calls = 0;
    if(++calls % 30 == 0) CheckSentinels();
}
static void NewUSER(HookRegs& regs, u32) { Fill(SENTINEL_USER, reinterpret_cast<void*>(regs.gpr[31])); }
static void NewRH1(HookRegs& regs, u32) { Fill(SENTINEL_RH1, reinterpret_cast<void*>(regs.gpr[30])); }
#define SENTINEL_FILL(id, object) Fill(id, object)
#else
#define SENTINEL_FILL(id, object)
#endif

static void* AidAlloc(const HookRegs& regs) {
    RKNet::Controller* controller = reinterpret_cast<RKNet::Controller*>(regs.gpr[27]);
    return EGG::Heap::alloc(regs.gpr[3], 4, controller->Heap);
}
static void SplitHolderAlloc(HookRegs& regs, u32) {
    regs.gpr[3] = reinterpret_cast<u32>(regs.gpr[28] < firstNewAid ? ::operator new(regs.gpr[3]) : AidAlloc(regs));
}
static void SplitBufferAlloc(HookRegs& regs, u32) {
    regs.gpr[3] = reinterpret_cast<u32>(regs.gpr[28] < firstNewAid ? ::operator new[](regs.gpr[3]) : AidAlloc(regs));
}

static void InitTimes(HookRegs& regs, u32) {
    ClearTimes(reinterpret_cast<RKNet::Controller*>(regs.gpr[30]), firstNewAid);
}

static void NewControllerTimes(HookRegs& regs, u32) {
    RKNet::Controller* controller = reinterpret_cast<RKNet::Controller*>(regs.gpr[27]);
    ClearTimes(controller, firstNewAid);
    memset(&controller->selectTime350DivTime3B0[0], 0, sizeof(controller->selectTime350DivTime3B0));
    OSReport("[VK 24P] network heap after the buffers of 24 aids: %d KiB free\n",
        controller->Heap->getAllocatableSize(4) >> 10);
    for(u32 aid = 0; aid < aidCount; ++aid) {
        bool ok = controller->fullSendPackets[aid] != nullptr && controller->fullSendPackets[aid]->packet != nullptr;
        for(u32 row = 0; row < 2; ++row) {
            const RKNet::SplitRACEPointers* split[2] ={ controller->splitToSendRACEPackets[row][aid],
                controller->splitReceivedRACEPackets[row][aid] };
            for(u32 k = 0; k < 2; ++k) {
                if(split[k] == nullptr) { ok = false; continue; }
                for(u32 i = 0; i < 8; ++i) {
                    if(split[k]->packetHolders[i] == nullptr || split[k]->packetHolders[i]->packet == nullptr) ok = false;
                }
            }
        }
        if(!ok) OSReport("[VK 24P] aid %d: a network buffer could not be allocated\n", aid);
    }
    Network::RememberRecvBuffers(controller);
    SENTINEL_FILL(SENTINEL_CONTROLLER, controller);
}

static void SelectTimes(HookRegs& regs, u32) {
    SENTINEL_FILL(SENTINEL_SELECT, reinterpret_cast<void*>(regs.gpr[3]));
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if(controller == nullptr) return;
    for(u32 aid = 10; aid < aidCount; ++aid) controller->selectTime350DivTime3B0[aid] = 0;
}

static const u32 packetMgrTimers = 0x1c8;
static void PacketMgrTimers(HookRegs& regs, u32) {
    u32* timers = reinterpret_cast<u32*>(regs.gpr[31] + packetMgrTimers);
    for(u32 aid = firstNewAid; aid < aidCount; ++aid) timers[aid] = regs.gpr[30];
    SENTINEL_FILL(SENTINEL_PACKETMGR, reinterpret_cast<void*>(regs.gpr[31]));
}

static void ClearROOMPackets(HookRegs& regs, u32) {
    RKNet::ROOMHandler* room = reinterpret_cast<RKNet::ROOMHandler*>(regs.gpr[3]);
    for(u32 aid = firstNewAid; aid < aidCount; ++aid) {
        reinterpret_cast<u32&>(room->toSendPackets[aid]) = 0;
        reinterpret_cast<u32&>(room->receivedPackets[aid]) = 0;
    }
    SENTINEL_FILL(SENTINEL_ROOM, room);
}

static const WordPatch words[] ={
#include <Race/Players24/Online24Aid.inc>
#include <Race/Players24/Online24Handlers.inc>
    { 0x80653228, 0x386001c8, 0x38600228 },
};

static const HookPatch hooks[] ={
    { 0x80655f10, 0x38000005, InitTimes, true },
    { 0x806573b4, 0x380000ff, NewControllerTimes, true },
    { 0x8065712c, 0x4bbd2ca1, SplitHolderAlloc, false },
    { 0x80657150, 0x4bbd2ca1, SplitBufferAlloc, false },
    { 0x806571b4, 0x4bbd2c19, SplitHolderAlloc, false },
    { 0x806571d8, 0x4bbd2c19, SplitBufferAlloc, false },
    { 0x8065ff44, 0x3c80809c, SelectTimes, true },
    { 0x80653394, 0x807d20d8, PacketMgrTimers, true },
    { 0x8065ab20, 0x98030068, ClearROOMPackets, true },
    { 0x8065abfc, 0x98030068, ClearROOMPackets, true },
    { 0x8065ad00, 0x98030068, ClearROOMPackets, true },
    { 0x8065aff0, 0x98030068, ClearROOMPackets, true },
    { 0x8065b2c8, 0x98030068, ClearROOMPackets, true },
#ifndef PROD
    { 0x80657504, 0x9421ff80, NetworkLoopCheck, true },
    { 0x806627d8, 0x907f09d8, NewUSER, true },
    { 0x80663c08, 0x3c60809c, NewRH1, true },
#endif
};

static PatchSet aidSet("Online 24 aid", words, P24_COUNT(words), nullptr, 0, nullptr, 0, hooks, P24_COUNT(hooks),
    nullptr, 0, false, true);

void ApplyAidLayout() {
    if(!IsSupportedDisc() || !aidSet.Check()) {
        Debug::FatalError("VanzaKart: the network patches (24 aids) don't match this game disc.");
        return;
    }
    aidSet.Apply();
}

}
}
