#include <kamek.hpp>
#include <Race/Players24/Patches.hpp>
#include <Race/Players24/Players24.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/RKNet/PacketMgr.hpp>
#include <Race/Players24/Online24.hpp>

extern "C" void OSReport(const char* format, ...);

namespace Pulsar {
namespace Players24 {

struct TimerData {
    u32 vtable;
    u16 minutes;
    u8 seconds;
    u8 padding;
    u16 milliseconds;
    bool isActive;
    u8 padding2;
};
size_assert(TimerData, 0xc);

struct RacerState {
    TimerData finishTime;
    u32 received;
    u32 acknowledged;
    u32 disconnected;
    u16 finishedBefore;
};
static RacerState states[maxPlayers];

static u8* VanillaEntry(u8* gm, u8 id) { return gm + 8 + id * 0x14; }
static u8* LocalIds(u8* gm) { return gm + 0x108; }
static const u32 rh2Size = 0x28;
static u8 rh2Packet[rh2Size];
static const u8 noPlayer = 0xFF;

#ifdef P24TEST
static const u8 maxLocalRacers = 7;
static const u32 finishedBeforeBits = 0;
#else
static const u8 maxLocalRacers = 2;
static const u32 finishedBeforeBits = 16;
#endif
static u8 localRacers[maxLocalRacers];
static u8 localRacerCount = 0;
static bool IsLocalRacer(u8 id) {
    for(u8 i = 0; i < localRacerCount; ++i) if(localRacers[i] == id) return true;
    return false;
}
static u8 RacerCount() { return Racedata::sInstance->racesScenario.playerCount; }
static RaceinfoPlayer* RacerInfo(u8 id) { return Raceinfo::sInstance->players[id]; }
static u16 FinishedBefore(const RaceinfoPlayer* player) {
    return *reinterpret_cast<const u16*>(reinterpret_cast<const u8*>(player) + 0x50);
}

class BitWriter {
public:
    BitWriter(u8* buffer) : buffer(buffer), position(0) { memset(buffer, 0, rh2Size); }
    void Write(u32 value, u32 bits) {
        for(s32 bit = bits - 1; bit >= 0; --bit, ++position) {
            if(value >> bit & 1) buffer[position >> 3] |= 0x80 >> (position & 7);
        }
    }
private:
    u8* buffer;
    u32 position;
};
class BitReader {
public:
    BitReader(const u8* buffer) : buffer(buffer), position(0) {}
    u32 Read(u32 bits) {
        u32 value = 0;
        for(u32 i = 0; i < bits; ++i, ++position) value = value << 1 | (buffer[position >> 3] >> (7 - (position & 7)) & 1);
        return value;
    }
private:
    const u8* buffer;
    u32 position;
};

typedef void (*GMFunc)(u8* gm);
typedef void (*GMIdFunc)(u8* gm, u8 id);
typedef u32 (*GMSizeFunc)(u8* gm);
typedef void (*EndRaceEarlyFunc)(RaceinfoPlayer* player, u32 reason, bool r5);

static void* initOriginal = nullptr;
static void Init(u8* gm) {
    if(!IsOnline24()) {
        reinterpret_cast<GMFunc>(initOriginal)(gm);
        return;
    }
    u8& count = Racedata::sInstance->racesScenario.playerCount;
    const u8 saved = count;
    if(count > vanillaPlayers) count = vanillaPlayers;
    reinterpret_cast<GMFunc>(initOriginal)(gm);
    count = saved;
    memset(states, 0, sizeof(states));
    const u32 timerVtable = *reinterpret_cast<const u32*>(VanillaEntry(gm, 0));
    for(u8 id = 0; id < maxPlayers; ++id) states[id].finishTime.vtable = timerVtable;
    const RacedataScenario& race = Racedata::sInstance->racesScenario;
    u8& localIdCount = LocalIds(gm)[3];
    for(u8 id = vanillaPlayers; id < saved && localIdCount < 2; ++id) {
        if(GetPlayer(race, id).playerType == PLAYER_REAL_LOCAL) LocalIds(gm)[localIdCount++] = id;
    }
    localRacerCount = 0;
    for(u8 i = 0; i < 2; ++i) if(LocalIds(gm)[i] != noPlayer) localRacers[localRacerCount++] = LocalIds(gm)[i];
    for(u8 id = 0; id < saved && localRacerCount < maxLocalRacers; ++id) {
        if(GetPlayer(race, id).playerType == PLAYER_CPU) localRacers[localRacerCount++] = id;
    }
    const u8 cpu = GetOwnCPUId();
    u8& localCount = LocalIds(gm)[3];
    if(cpu < saved && localCount < 2) LocalIds(gm)[localCount++] = cpu;
}

static void EndOurCPUs(u8* gm, u32 reason) {
    for(u8 i = 0; i < localRacerCount; ++i) {
        const u8 id = localRacers[i];
        if(LocalIds(gm)[0] == id || LocalIds(gm)[1] == id) continue;
        RaceinfoPlayer* player = RacerInfo(id);
        if(!(player->stateFlags & 2)) reinterpret_cast<EndRaceEarlyFunc>(Port(0x805342e8))(player, reason, true);
    }
}

typedef const u8* (*GetRH2Func)(const RKNet::PacketMgr* mgr, u32 id);
static void* readOriginal = nullptr;
static void ReadRacer(u8* gm, u8 id) {
    if(!IsOnline24()) {
        reinterpret_cast<GMIdFunc>(readOriginal)(gm, id);
        return;
    }
    if(id >= maxPlayers || IsLocalRacer(id)) return;
    const u8* packet = reinterpret_cast<GetRH2Func>(Port(0x80653cb8))(RKNet::PacketMgr::sInstance, id);
    if(packet == nullptr) return;
    RacerState& state = states[id];
    BitReader reader(packet);
    state.received = reader.Read(24);
    state.acknowledged = reader.Read(24);
    state.disconnected = reader.Read(24);
    state.finishTime.isActive = false;
    const u32 entries = reader.Read(3);
    for(u32 i = 0; i < entries && i < 7; ++i) {
        const u32 entryId = reader.Read(5);
        const u16 minutes = reader.Read(6);
        const u8 seconds = reader.Read(6);
        const u16 milliseconds = reader.Read(10);
        const u16 finishedBefore = reader.Read(finishedBeforeBits);
        if(entryId != id) continue;
        if(minutes != 0 || seconds != 0 || milliseconds != 0) {
            state.finishTime.isActive = true;
            state.finishTime.minutes = minutes;
            state.finishTime.seconds = seconds;
            state.finishTime.milliseconds = milliseconds;
        }
        state.finishedBefore = finishedBefore;
    }
    if(id < vanillaPlayers) {
        u8* entry = VanillaEntry(gm, id);
        memcpy(entry + 4, reinterpret_cast<const u8*>(&state.finishTime) + 4, 8);
        *reinterpret_cast<u16*>(entry + 0xc) = state.received;
        *reinterpret_cast<u16*>(entry + 0xe) = state.acknowledged;
        *reinterpret_cast<u16*>(entry + 0x10) = state.disconnected;
        *reinterpret_cast<u16*>(entry + 0x12) = state.finishedBefore;
    }
}

typedef void (*EndRaceFunc)(RaceinfoPlayer* player, const TimerData* finishTime, bool hasNoCameras, u32 r6);
static void* applyOriginal = nullptr;
static void ApplyFinishes(u8* gm) {
    if(!IsOnline24()) {
        reinterpret_cast<GMFunc>(applyOriginal)(gm);
        return;
    }
    const u8 count = RacerCount();
    u32 finished = 0;
    for(u8 id = 0; id < count; ++id) {
        if(!IsLocalRacer(id)) finished |= states[id].acknowledged & ~states[id].disconnected;
    }
    const bool isSpectating = LocalIds(gm)[3] == 0;
    for(u8 id = 0; id < count; ++id) {
        RaceinfoPlayer* player = RacerInfo(id);
        if((player->stateFlags & 2) || !(finished >> id & 1)) continue;
        if(isSpectating && !states[id].finishTime.isActive) continue;
        reinterpret_cast<EndRaceFunc>(Port(0x805347f4))(player, &states[id].finishTime, false, 5);
    }
}

static void* writeOriginal = nullptr;
static u32 WriteRH2(u8* gm) {
    if(!IsOnline24()) return reinterpret_cast<GMSizeFunc>(writeOriginal)(gm);
    const u8 count = RacerCount();
    u32 received = 0;
    u32 acknowledged = 0;
    u32 disconnected = 0;
    for(u8 id = 0; id < count; ++id) {
        const u32 bit = 1 << id;
        if(IsLocalRacer(id)) {
            bool isAcknowledged = true;
            for(u8 other = 0; other < count && isAcknowledged; ++other) {
                if(IsLocalRacer(other) || (RacerInfo(other)->stateFlags & 0x10)) continue;
                if(!(states[other].received & bit)) isAcknowledged = false;
            }
            if(isAcknowledged) acknowledged |= bit;
        }
        else {
            if(states[id].finishTime.isActive) received |= bit;
            if(states[id].received & bit) acknowledged |= bit;
            if(RacerInfo(id)->stateFlags & 0x10) disconnected |= bit;
        }
    }
    BitWriter writer(rh2Packet);
    writer.Write(received, 24);
    writer.Write(acknowledged, 24);
    writer.Write(disconnected, 24);
    writer.Write(localRacerCount, 3);
    for(u8 i = 0; i < localRacerCount; ++i) {
        const u8 id = localRacers[i];
        const RaceinfoPlayer* player = RacerInfo(id);
        const Timer* time = player->raceFinishTime;
        u16 minutes = 0;
        u8 seconds = 0;
        u16 milliseconds = 0;
        if((player->stateFlags & 2) && time->isActive) {
            minutes = time->minutes > 99 ? 99 : time->minutes;
            if(minutes > 62) {
                minutes = 63;
                milliseconds = id;
            }
            else {
                seconds = time->seconds;
                milliseconds = time->milliseconds;
            }
        }
        writer.Write(id, 5);
        writer.Write(minutes, 6);
        writer.Write(seconds, 6);
        writer.Write(milliseconds, 10);
        writer.Write(FinishedBefore(player), finishedBeforeBits);
    }
    const u32 bits = 75 + localRacerCount * (27 + finishedBeforeBits);
    return (bits + 31) / 32 * 4;
}

static void* positionsOriginal = nullptr;
static void HandlePositionTracking(u8* gm) {
    if(!IsOnline24()) {
        reinterpret_cast<GMFunc>(positionsOriginal)(gm);
        return;
    }
    Raceinfo* raceinfo = Raceinfo::sInstance;
    u8 count = RacerCount();
    if(count > maxPlayers) count = maxPlayers;
    u8 ids[maxPlayers];
    double keys[maxPlayers];
    for(u8 slot = 0; slot < maxPlayers; ++slot) ids[slot] = noPlayer;
    for(u8 id = 0; id < count; ++id) {
        u8 slot = raceinfo->players[id]->position - 1;
        if(slot >= count || ids[slot] != noPlayer) {
            slot = 0;
            while(ids[slot] != noPlayer) ++slot;
        }
        ids[slot] = id;
    }
    for(u8 slot = 0; slot < count; ++slot) {
        const u8 id = ids[slot];
        const RaceinfoPlayer* player = raceinfo->players[id];
        if(player->stateFlags & 2) {
            const Timer* time = player->raceFinishTime;
            const u32 total = (time->minutes * 60 + time->seconds) * 1000 + time->milliseconds;
            keys[slot] = 6000099.0 - total + (double)0.00008f * id;
        }
        else if(player->stateFlags & 0x30) keys[slot] = -(double)(id + 1);
        else keys[slot] = player->raceCompletion;
    }
    for(u8 slot = 1; slot < count; ++slot) {
        const double key = keys[slot];
        const u8 id = ids[slot];
        u8 pos = slot;
        while(pos > 0 && keys[pos - 1] < key) {
            keys[pos] = keys[pos - 1];
            ids[pos] = ids[pos - 1];
            --pos;
        }
        keys[pos] = key;
        ids[pos] = id;
    }
    for(u8 slot = 0; slot < count; ++slot) {
        raceinfo->players[ids[slot]]->position = slot + 1;
        raceinfo->playerIdInEachPosition[slot] = ids[slot];
    }
}

static void* localPlayersOriginal = nullptr;
static void UpdateLocalPlayers(u8* gm) {
    reinterpret_cast<GMFunc>(localPlayersOriginal)(gm);
    if(!IsOnline24()) return;
    EndOurCPUs(gm, 2);
    const u8 cpu = GetOwnCPUId();
    if(cpu >= RacerCount() || !IsLocalRacer(cpu)) return;
    RaceinfoPlayer* player = RacerInfo(cpu);
    if(!(player->stateFlags & 2)) reinterpret_cast<EndRaceEarlyFunc>(Port(0x805342e8))(player, 2, true);
}

typedef bool (*GMBoolFunc)(u8* gm);
static void* timeLimitOriginal = nullptr;
static bool CheckTimeLimit(u8* gm) {
    const bool isOver = reinterpret_cast<GMBoolFunc>(timeLimitOriginal)(gm);
    if(isOver && IsOnline24()) EndOurCPUs(gm, 3);
    return isOver;
}

static const FuncHook funcs[] ={
    { 0x8053e370, 0x3c808089, reinterpret_cast<const void*>(Init), &initOriginal },
    { 0x8053e47c, 0x9421ffd0, reinterpret_cast<const void*>(ReadRacer), &readOriginal },
    { 0x8053e680, 0x9421ffe0, reinterpret_cast<const void*>(ApplyFinishes), &applyOriginal },
    { 0x8053e7ac, 0x9421ffc0, reinterpret_cast<const void*>(WriteRH2), &writeOriginal },
    { 0x8053f4a0, 0x9421fea0, reinterpret_cast<const void*>(HandlePositionTracking), &positionsOriginal },
    { 0x8053fb98, 0x9421ffe0, reinterpret_cast<const void*>(UpdateLocalPlayers), &localPlayersOriginal },
    { 0x8053ec40, 0x9421ffe0, reinterpret_cast<const void*>(CheckTimeLimit), &timeLimitOriginal },
};
typedef void (*SetRH2Func)(RKNet::PacketMgr* mgr, const u8* packet, u32 size);
static void SendRH2(RKNet::PacketMgr* mgr, const u8* packet, u32 size) {
    reinterpret_cast<SetRH2Func>(Port(0x80653c08))(mgr, IsOnline24() ? rh2Packet : packet, size);
}
static const CallPatch calls[] ={
    { 0x8053f31c, 0x481148ed, SendRH2 },
};
static PatchSet raceSet("Online 24 race", nullptr, 0, calls, P24_COUNT(calls), nullptr, 0, nullptr, 0, funcs,
    P24_COUNT(funcs), true);

bool OnlineRaceReady() { return IsReady() && !raceSet.skipped; }

}
}
