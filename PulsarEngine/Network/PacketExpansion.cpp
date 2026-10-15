#include <Network/PacketExpansion.hpp>
#include <runtimeWrite.hpp>

extern "C" void OSReport(const char* format, ...);

namespace Pulsar {
namespace Network {

static const u32 totalRACESize = sizeof(RKNet::RACEPacketHeader) + sizeof(PulRH1) + sizeof(PulRH2) + sizeof(PulSELECT) + 2 * sizeof(PulRACEDATA) + sizeof(PulUSER) + 2 * sizeof(PulITEM) + sizeof(PulEVENT);
class CustomRKNetController { //Exists to make received packets a pointer array so that the size can be variable
public:

    //static CustomRKNetController* Get() { return reinterpret_cast<CustomRKNetController*>(RKNet::Controller::sInstance); }
    virtual ~CustomRKNetController(); //8065741c vtable 808c097c

    u32 unkVtable;//unknown class vtable 808c0988
    OS::Mutex mutex; //0x8

    EGG::Heap* Heap; //0x20
    EGG::TaskThread* taskThread; //0x24
    RKNet::ConnectionState connectionState; //0x28 

    RKNet::ErrorParams errorParams; //0x2c

    u32 unknown_0x34;

    RKNet::ControllerSub subs[2]; //0x38
    RKNet::RoomType roomType; //0xe8
    u8 unknown_0xec[4];

    u8 vanillaPerAidArrays[0x360 - 0xf0]; //0xf0 moved below for 24 aids (Race/Players24/Online24Aid.inc)
    u8 lastRACESendAid; //0x360 aid a packet was last sent to

    //MODIFICATIONS: the vanilla fullReceivedPackets[12][0x2e0] area, DWC receives in fullPulRecvPackets instead
    u8 padding[7]; //0x361
    RKNet::SplitRACEPointers* splitToSendRACEPackets[2][24]; //0x368 split pointers for the outgoing packets, double buffered, indexed by aid
    RKNet::SplitRACEPointers* splitReceivedRACEPackets[2][24]; //0x428 split pointers for the incoming packets, double buffered, indexed by aid
    RKNet::PacketHolder<void>* fullSendPackets[24]; //0x4e8 combined outgoing packets, indexed by aid
    u64 lastRACEToSendTimes[24]; //0x548 time when last sent to that aid
    u64 lastRACERecivedTimes[24]; //0x608 time when last received from that aid
    u64 RACEToSendTimesTaken[24]; //0x6c8 last send time minus the time of the send before it
    u64 RACEReceivedTimesTaken[24]; //0x788 last receive time minus the time of the receive before it
    u32 lastSendBufferUsed[24]; //0x848 last full send buffer used for each aid, 0 or 1
    u32 lastReceivedBufferUsed[24][8]; //0x8a8 last Received buffer used for each packet per aid, 1 or 0
    u64 selectTime350DivTime3B0[24]; //0xba8
    void* fullPulRecvPackets[24]; //0xc68
    u8 unused[0x25b0 - 0xcc8]; //0xcc8
    u32 toDisconnectAids; //0x25b0 for wiimmfi only?
    u8 unused2[0x25e4 - 0x25b4]; //0x25b4 fullPulRecvPackets[12] before the 24 aids
    //END

    RKNet::StatusData localStatusData; //0x25e4 8 bytes, see http://wiki.tockdom.com/wiki/MKWii_Network_Protocol/Server/gpcm.gs.nintendowifi.net#locstring
    RKNet::Friend friends[30]; //0x25ec
    bool friendsListIsChanged; //0x2754 true if unprocessed changes have happeend
    bool shutdownScheduled; //0x2755 will cause shutdown of wifi connection on next run of the main loop if true
    bool friendStatusUpdateScheduled; //0x2756 if true, the main loop will update all friend statuses on the next iteration
    bool nwc24Related; //0x2757
    bool hasprofanity; //0x2758 1 if name is bad
    u8 padding2[3];
    s32 badWordsNum; //0x275c number of bad strings in the profanity check
    u8 unknown_0x2760[4];
    s32 vr; //0x2764
    s32 br; //0x2768
    u8 vanillaBufferUsed[0x291c - 0x276c]; //0x276c moved
    s32 currentSub; //0x291c index of the current sub to use, 0 or 1
    u8 aidsBelongingToPlayerIds[12]; //0x2920 index is player id, value is aid
    u32 disconnectedAids; //0x292c 1 if disconnected, index 1 << aid
    u32 disconnectedPlayerIds; //0x2930 index 1 << playerId
    u8 unknown_0x2934[0x29c8 - 0x2934];

};
size_assert(CustomRKNetController, 0x29c8);

//Same layout as RKNet::Controller (Race/Players24/Online24Aid.cpp); never called, checked at compile time
static void CheckLayout() {
    offset_assert(CustomRKNetController, splitReceivedRACEPackets, 0x428);
    offset_assert(CustomRKNetController, lastReceivedBufferUsed, 0x8a8);
    offset_assert(CustomRKNetController, fullPulRecvPackets, 0xc68);
    offset_assert(CustomRKNetController, toDisconnectAids, 0x25b0);
    offset_assert(CustomRKNetController, localStatusData, 0x25e4);
    offset_assert(CustomRKNetController, shutdownScheduled, 0x2755);
    offset_assert(CustomRKNetController, currentSub, 0x291c);
    offset_assert(ExpSELECTHandler, receivedPackets, 0x78);
    offset_assert(ExpSELECTHandler, lastReceivedTimes, 0x80);
    offset_assert(ExpSELECTHandler, unknown_0x350, 0x140);
    offset_assert(ExpSELECTHandler, unknown_0x3b0, 0x200);
    offset_assert(ExpSELECTHandler, lastSentToAid, 0x2e0);
    offset_assert(ExpSELECTHandler, hasNewSELECT, 0x3e0);
    offset_assert(ExpSELECTHandler, aidsThatHaveVoted, 0x3f0);
}

void* CreateSendAndRecvBuffers() {

    register RKNet::PacketHolder<void>* holder;
    register CustomRKNetController* controller;
    register u8 aid;
    asm(mr holder, r24;);
    asm(mr controller, r27;);
    asm(mr aid, r20;);

    holder->bufferSize = totalRACESize;
    if(aid < 12) {
        controller->fullPulRecvPackets[aid] = new u8[totalRACESize];
        void* send = new u8[totalRACESize];
        return send;
    }
    /*
    Aids 12-23 (Race/Players24/Online24Aid.cpp): the Controller is created at boot, when the current heap can't
    take twice the buffers, so these come from the Controller's own heap (DWC's, about 600 KiB free).
    */
    controller->fullPulRecvPackets[aid] = EGG::Heap::alloc(totalRACESize, 0x20, controller->Heap);
    void* send = EGG::Heap::alloc(totalRACESize, 0x20, controller->Heap);
    return send;
}

#ifndef PROD
//Copy of the receive buffer pointers taken when the Controller is built: the log says if one changes afterwards
static void* createdRecvPackets[24];
static const void* createdController = nullptr;
//End of the constructor (Online24Aid.cpp): sInstance isn't set yet, the object comes from the hook
void RememberRecvBuffers(const void* object) {
    const CustomRKNetController* controller = reinterpret_cast<const CustomRKNetController*>(object);
    createdController = object;
    for(int aid = 0; aid < 24; ++aid) createdRecvPackets[aid] = controller->fullPulRecvPackets[aid];
    OSReport("[VK 24P] Controller %p built, receive buffers aid 0 %p, aid 23 %p\n", object,
        createdRecvPackets[0], createdRecvPackets[23]);
}
static void CheckRecvBuffers(const CustomRKNetController* controller, const char* where) {
    if(controller != createdController) OSReport("[VK 24P] %s: Controller %p, built %p\n", where, controller, createdController);
    for(int aid = 0; aid < 24; ++aid) {
        if(controller->fullPulRecvPackets[aid] == createdRecvPackets[aid]) continue;
        OSReport("[VK 24P] %s: receive buffer of aid %d is %p, was %p at creation\n", where, aid,
            controller->fullPulRecvPackets[aid], createdRecvPackets[aid]);
    }
}
#else
void RememberRecvBuffers(const void*) {}
static void CheckRecvBuffers(const CustomRKNetController*, const char*) {}
#endif

//A missing receive buffer (allocation failed or overwritten) is made again from the Controller's heap
static void* RecvBuffer(CustomRKNetController* controller, u8 aid) {
    if(controller->fullPulRecvPackets[aid] == nullptr) {
        controller->fullPulRecvPackets[aid] = EGG::Heap::alloc(totalRACESize, 0x20, controller->Heap);
        OSReport("[VK 24P] receive buffer of aid %d made again: %p\n", aid, controller->fullPulRecvPackets[aid]);
    }
    return controller->fullPulRecvPackets[aid];
}
kmCall(0x806570b4, CreateSendAndRecvBuffers);

/*
static const u32 sizeArray[8] = {
        sizeof(RKNet::RACEPacketHeader),
        sizeof(PulRH1),
        sizeof(PulRH2),
        sizeof(PulSELECT),
        2 * sizeof(PulRACEDATA),
        sizeof(PulUSER),
        2 * sizeof(PulITEM),
        sizeof(PulEVENT)
};
*/


/*
    Checked on the P, E and K discs, not on J: the vanilla sizes are verified first, so a wrong
    address in versions.txt leaves the game alone instead of patching half of whatever is there.
*/
kmRuntimeUse(0x8089a19b);
kmRuntimeUse(0x8089a19f);
kmRuntimeUse(0x8089a1a3);
kmRuntimeUse(0x8089a1a7);
kmRuntimeUse(0x8089a1ab);
kmRuntimeUse(0x8089a1af);
kmRuntimeUse(0x8089a1b3);
static void ExpandPacketSizes() {
    struct Size { u32 address; u8 vanilla; u8 expanded; };
    const Size sizes[7] ={
        { kmRuntimeAddr(0x8089a19b), 0x28, sizeof(PulRH1) },
        { kmRuntimeAddr(0x8089a19f), 0x28, sizeof(PulRH2) },
        { kmRuntimeAddr(0x8089a1a3), 0x38, sizeof(PulSELECT) },
        { kmRuntimeAddr(0x8089a1a7), 0x80, 2 * sizeof(PulRACEDATA) },
        { kmRuntimeAddr(0x8089a1ab), 0xc0, sizeof(PulUSER) },
        { kmRuntimeAddr(0x8089a1af), 0x10, 2 * sizeof(PulITEM) },
        { kmRuntimeAddr(0x8089a1b3), 0xf8, sizeof(PulEVENT) },
    };
    for (int i = 0; i < 7; ++i) {
        if (*(volatile u8*)sizes[i].address != sizes[i].vanilla) {
            OSReport("[PacketExpansion] size table not found at 0x%08X, packets not expanded\n", sizes[i].address);
            return;
        }
    }
    for (int i = 0; i < 7; ++i) KamekRuntimeWrite::Write8(sizes[i].address, sizes[i].expanded);
}
static BootHook PacketSizesHook(ExpandPacketSizes, 4);

void SetProperRecvBuffers(u8 aid, void* usualBuffer, u32 usualSize) {
    register CustomRKNetController* controller;
    asm(mr controller, r31;);
    if(aid == 0) CheckRecvBuffers(controller, "ConnectToWFC");
    void* buffer = RecvBuffer(controller, aid);
    if(buffer == nullptr) return;
    memset(buffer, 0, totalRACESize);
    DWC::SetRecvBuffer(aid, buffer, totalRACESize);
}
kmWrite32(0x80658c78, 0x60000000); //prevent usual memset
kmCall(0x80658c90, SetProperRecvBuffers);

void ProperRecvBuffersClear() {
    CustomRKNetController* controller = reinterpret_cast<CustomRKNetController*>(RKNet::Controller::sInstance);
    CheckRecvBuffers(controller, "Controller::Init");
    for(int aid = 0; aid < 24; ++aid) {
        void* buffer = RecvBuffer(controller, aid);
        if(buffer != nullptr) memset(buffer, 0, totalRACESize);
    }
}
kmCall(0x8065607c, ProperRecvBuffersClear);


void CheckPacket(CustomRKNetController* controller, RKNet::RACEPacketHeader& packet, u32 size, u32 sizeUnused, u32 aid) {
    using namespace RKNet;

    const u32 recvCRC = packet.crc32;
    packet.crc32 = 0;
    const u32 calcCRC = OS::CalcCRC32(&packet, size);
    packet.crc32 = recvCRC;
    bool disconnect = false;
    if(recvCRC != calcCRC) {
        disconnect = true;
        //DIAGNOSTIC: only fires on the reject path, so it cannot flood the log.
        OSReport("[VK KICK] aid=%u CRC mismatch recv=0x%08X calc=0x%08X size=%u\n", aid, recvCRC, calcCRC, size);
    }
    else {
        u32* lastUsedBufferAid = &controller->lastReceivedBufferUsed[aid][0];
        SplitRACEPointers* recv = controller->splitReceivedRACEPackets[lastUsedBufferAid[0]][aid];
        PacketHolder<void>** holderRecv = &recv->packetHolders[0];
        const u8* sizes = &packet.sizes[0];

        for(int i = 0; i < 8; ++i) {
            const PacketHolder<void>* curHolder = holderRecv[i]; //starts at header etc...

            const u8 curSize = sizes[i]; //transmitted in packet
            if(curSize != 0) {
                if(curHolder->bufferSize < curSize) {
                    disconnect = true;
                    //DIAGNOSTIC: a sender whose sub-packet is bigger than our buffer is
                    //running a different Code.pul; this says which packet and by how much.
                    OSReport("[VK KICK] aid=%u subPacket=%d sentSize=%u ourBuffer=%u\n", aid, i, curSize, curHolder->bufferSize);
                }
            }
        }
    }
    if(disconnect) controller->toDisconnectAids |= 1 << aid;
    else  reinterpret_cast<RKNet::Controller*>(controller)->ProcessRACEPacket(aid, packet, size);
}
kmBranch(0x80658608, CheckPacket);

bool DisconnectBadAids() {
    register CustomRKNetController* controller;
    asm(mr controller, r15;);

    int old = OS::DisableInterrupts();
    for(int aid = 0; aid < 24; ++aid) {
        if(controller->toDisconnectAids & (1 << aid)) DWC::CloseConnectionHard(aid); //only the aids that sent a bad packet
    }
    controller->toDisconnectAids = 0;
    OS::RestoreInterrupts(old);

    return controller->shutdownScheduled;
}
kmCall(0x806579b4, DisconnectBadAids);
kmWrite32(0x806579b8, 0x2c030000);

//Various hardcoded size patches
kmWrite32(0x80661100, 0x418000d8); //bne -> blt for comparison against sizeof(SELECTPacket)
kmWrite32(0x8065adc8, 0x41800014); //bne -> blt for comparison against sizeof(ROOMPacket)
kmWrite32(0x80665244, 0x418000d0); //bne -> blt for comparison against sizeof(RACEHEADER1Packet)

ExpSELECTHandler* CreateRecvArr(ExpSELECTHandler* handler) { //wiimmfi hook prevents a more natural hook...
    register RKNet::OnlineMode mode;
    asm(mr mode, r31;);
    handler->mode = mode;
    handler->receivedPackets = new PulSELECT[24]; //one per aid, 24 consoles (Race/Players24/Online24Handlers.inc)
    return handler;
}
kmCall(0x8065fec0, CreateRecvArr);

void DeleteSELECT(ExpSELECTHandler* handler) {
    delete handler->receivedPackets;
    delete reinterpret_cast<RKNet::SELECTHandler*>(handler);
}
kmCall(0x8065ff84, DeleteSELECT);


u8 GetLastRecvSECTIONSize(u8 aid, u8 sectionIdx) {
    const CustomRKNetController* controller = reinterpret_cast<CustomRKNetController*>(RKNet::Controller::sInstance);
    RKNet::RACEPacketHeader* header = reinterpret_cast<RKNet::RACEPacketHeader*>(controller->fullPulRecvPackets[aid]);
    return header->sizes[sectionIdx];
}

}//namespace Network
}//namespace Pulsar