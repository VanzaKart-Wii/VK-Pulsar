#include <kamek.hpp>
#include <Race/Players24/Patches.hpp>
#include <Race/Players24/Players24.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/RKNet/PacketMgr.hpp>
#include <MarioKartWii/RKNet/ITEM.hpp>
#include <Race/Players24/Online24.hpp>

namespace Pulsar {
namespace Players24 {

struct ItemHandler24 {
    RKNet::ITEMPacket toSendPackets[2];
    u8 stockArrays[0xd0 - 0x10];
    u32 itemObjSums[15][3];
    RKNet::ITEMPacket receivedPackets[24];
    u32 itemStatus[24];
    u32 startTimes[24];
};
size_assert(ItemHandler24, 0x304);

static const u8 noItem = 0x14;

typedef bool (*PlayerQueryFunc)(const RKNet::PacketMgr* mgr, u32 playerId);
typedef u32 (*HudSlotFunc)(const RKNet::PacketMgr* mgr, u32 playerId);
static bool IsLocal(u8 id) { return reinterpret_cast<PlayerQueryFunc>(Port(0x806548a8))(RKNet::PacketMgr::sInstance, id); }
static bool IsConnected(u8 id) { return reinterpret_cast<PlayerQueryFunc>(Port(0x80654820))(RKNet::PacketMgr::sInstance, id); }
static u32 HudSlot(u8 id) { return reinterpret_cast<HudSlotFunc>(Port(0x80654918))(RKNet::PacketMgr::sInstance, id); }

static const u8* PacketOf(const ItemHandler24& handler, u8 id) {
    if(IsLocal(id)) return reinterpret_cast<const u8*>(&handler.toSendPackets[HudSlot(id)]);
    if(IsConnected(id)) return reinterpret_cast<const u8*>(&handler.receivedPackets[id]);
    return nullptr;
}
static u8 StoredItem(const ItemHandler24& handler, u8 id) { const u8* p = PacketOf(handler, id); return p ? p[1] : noItem; }
static u8 DraggedItem(const ItemHandler24& handler, u8 id) { const u8* p = PacketOf(handler, id); return p ? p[2] : noItem; }
static u8 StoredMode(const ItemHandler24& handler, u8 id) { const u8* p = PacketOf(handler, id); return p ? p[3] : 0; }
static u8 DraggedMode(const ItemHandler24& handler, u8 id) { const u8* p = PacketOf(handler, id); return p ? p[4] : 0; }

static u32 ItemObject(u8 item) { return *reinterpret_cast<const u32*>(Port(0x809c36a0) + item * 0x1c + 4); }
static u32 ItemCount(u8 item) { return *reinterpret_cast<const u32*>(Port(0x809c36a0) + item * 0x1c + 8); }

static u32 GetStoredItemCount(const ItemHandler24& handler, u32 object, u8* playerIds, u8 racers) {
    u32 count = 0;
    for(u8 id = 0; id < racers; ++id) {
        if(StoredMode(handler, id) != 1 || ItemObject(StoredItem(handler, id)) != object) continue;
        u32 pos = count++;
        while(pos > 0 && handler.startTimes[playerIds[pos - 1]] < handler.startTimes[id]) {
            playerIds[pos] = playerIds[pos - 1];
            --pos;
        }
        playerIds[pos] = id;
    }
    return count;
}

typedef bool (*ObjectCappedFunc)(void* obj, u32 object, u32 r5);
static void UpdateItemStatusAndSums24(ItemHandler24& handler) {
    for(u8 id = 0; id < maxPlayers; ++id) handler.itemStatus[id] = 0;
    const u8 racers = Racedata::sInstance->racesScenario.playerCount;
    const u8* objectInfos = *reinterpret_cast<u8* const*>(Port(0x809c3618));
    void* capper = *reinterpret_cast<void* const*>(Port(0x809c3670));
    const u8* limits = reinterpret_cast<const u8*>(Port(0x809c2f48));
    for(u32 object = 0; object < 15; ++object) {
        const u32 extra = *reinterpret_cast<const u32*>(objectInfos + object * 0x24 + 0x5c);
        bool isCapped = reinterpret_cast<ObjectCappedFunc>(Port(0x807bb380))(capper, object, 0);
        u32 stored = 0;
        u32 dragged = 0;
        for(u8 id = 0; id < racers; ++id) {
            const u8 item = StoredItem(handler, id);
            if(item != noItem && ItemObject(item) == object) {
                switch(StoredMode(handler, id)) {
                    case 2: stored += ItemCount(item); break;
                    case 4: stored += 3; break;
                    case 5: stored += 2; break;
                    case 6: stored += 1; break;
                }
            }
            const u8 draggedItem = DraggedItem(handler, id);
            if(draggedItem != noItem && ItemObject(draggedItem) == object) {
                switch(DraggedMode(handler, id)) {
                    case 1: case 5: dragged += 3; break;
                    case 2: case 6: dragged += 2; break;
                    case 3: case 7: dragged += 1; break;
                }
            }
            if(object == 0xd) {
                const u8* kart = reinterpret_cast<const u8*>(Kart::Manager::sInstance->GetKartPlayer(id));
                const u8* pointers = *reinterpret_cast<u8* const*>(kart);
                const u8* status = *reinterpret_cast<u8* const*>(pointers + 4);
                if(*reinterpret_cast<const u32*>(status + 0xc) & 0x08000000) isCapped = true;
            }
        }
        handler.itemObjSums[object][0] = stored;
        handler.itemObjSums[object][1] = dragged;
        u8 holders[maxPlayers];
        const u32 holderCount = GetStoredItemCount(handler, object, holders, racers);
        const u32 total = stored + dragged + extra;
        const u32 limit = *reinterpret_cast<const u32*>(limits + object * 0x74 + 4);
        u32 accepted = 0;
        for(u32 i = 0; i < holderCount; ++i) {
            const u8 id = holders[i];
            const u32 count = ItemCount(StoredItem(handler, id));
            if(!isCapped && total + accepted + count <= limit) {
                handler.itemStatus[id] = 1;
                accepted += count;
            }
            else handler.itemStatus[id] = 2;
        }
        handler.itemObjSums[object][2] = accepted;
    }
}

typedef void (*ItemHandlerFunc)(RKNet::ITEMHandler* handler);
static void* updateStatusOriginal = nullptr;
static void UpdateItemStatusAndSums(RKNet::ITEMHandler* handler) {
    if(IsOnline24()) UpdateItemStatusAndSums24(*reinterpret_cast<ItemHandler24*>(handler));
    else reinterpret_cast<ItemHandlerFunc>(updateStatusOriginal)(handler);
}

static const WordPatch words[] ={
#include <Race/Players24/Online24Items.inc>
};
static const CallPatch calls[] ={
    { 0x8065c10c, 0x4bbcdcc1, AllocZeroed },
};
static const FuncHook funcs[] ={
    { 0x8065e0a0, 0x9421ff70, reinterpret_cast<const void*>(UpdateItemStatusAndSums), &updateStatusOriginal },
};
static const u32 instances[] ={ 0x809c20f8 };
static PatchSet itemSet("Online 24 items", words, P24_COUNT(words), calls, P24_COUNT(calls), instances,
    P24_COUNT(instances), nullptr, 0, funcs, P24_COUNT(funcs), true);

bool OnlineItemsReady() { return IsReady() && !itemSet.skipped; }

}
}
