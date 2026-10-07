#include <kamek.hpp>
#include <Race/Players24/Patches.hpp>
#include <Race/Players24/Players24.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/Mii/MiiGroup.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/PacketMgr.hpp>
#include <MarioKartWii/RKNet/SELECT.hpp>
#include <MarioKartWii/RKNet/RH1.hpp>
#include <core/rvl/DWC/DWCMatch.hpp>
#include <Race/Players24/Online24.hpp>
#include <Network/PacketExpansion.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/RKNet/USER.hpp>
#include <MarioKartWii/UI/Text/Text.hpp>
#include <UI/UI.hpp>

extern "C" void OSReport(const char* format, ...);

namespace Pulsar {
namespace Players24 {

static const u8 noPlayer = 0xFF;
#ifdef P24TEST
static const u8 cpusPerConsole = 6;
#else
static const u8 cpusPerConsole = 0;
#endif
static const u8 maxAids = 24;
static const u32 aidTableOffset = 0x29c8;

static bool OnlineSetReady();

bool IsOnline24() {
    if(!OnlineSetReady()) return false;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if(controller == nullptr) return false;
    if(controller->roomType != RKNet::ROOMTYPE_FROOM_HOST && controller->roomType != RKNet::ROOMTYPE_FROOM_NONHOST) return false;
    const System* system = System::sInstance;
    if(system == nullptr || !system->IsContext(PULSAR_PLAYERS24)) return false;
    return !system->IsContext(PULSAR_VR) && !system->IsContext(PULSAR_MODE_KO) && !system->IsContext(PULSAR_MODE_OTT)
        && !system->IsContext(PULSAR_EXTENDEDTEAMS);
}

static bool InRace24() { return RKNet::PacketMgr::sInstance != nullptr && IsOnline24(); }

static u8* AidTable(const RKNet::Controller& controller) {
    return const_cast<u8*>(reinterpret_cast<const u8*>(&controller)) + aidTableOffset;
}

static const RKNet::ControllerSub& CurrentSub(const RKNet::Controller& controller) {
    return controller.subs[controller.currentSub];
}

u8 AidOfPlayer(u8 id) {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if(controller == nullptr || id >= maxPlayers) return noPlayer;
    if(!OnlineSetReady()) return id < vanillaPlayers ? controller->aidsBelongingToPlayerIds[id] : noPlayer;
    return AidTable(*controller)[id];
}

static u8 humanCount = 0;

u8 GetOwnCPUId() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if(controller == nullptr || !OnlineSetReady()) return noPlayer;
    const RKNet::ControllerSub& sub = CurrentSub(*controller);
    if(sub.localPlayerCount != 1) return noPlayer;
    const u8* table = AidTable(*controller);
    for(u8 id = humanCount; id < maxPlayers; ++id) if(table[id] == sub.localAid) return id;
    return noPlayer;
}

bool IsOnlineCPU(u8 id) {
    if(!IsOnline24() || id < humanCount || id >= maxPlayers) return false;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    return controller != nullptr && AidTable(*controller)[id] < maxAids;
}

bool IsConsoles24() { return cpusPerConsole == 0 && IsOnline24(); }

bool HasNoMiiSlot(u8 id) {
    if(!IsOnline24() || id >= maxPlayers) return false;
#ifdef P24TEST
    if(id >= vanillaPlayers && HasRacerMii(id)) return false;
#endif
    if(IsOnlineCPU(id)) return true;
    return id >= vanillaPlayers && !HasRacerMii(id);
}

bool ShowsCharacter(u8 id) { return HasNoMiiSlot(id); }

bool KeepsCharacterHead(u8 id) {
    if(!IsOnline24() || id >= maxPlayers) return false;
    return id >= vanillaPlayers || IsOnlineCPU(id) || HasExtraRacers();
}

static const wchar_t defaultMiiName[] = L"Player";
typedef bool (*IsDefaultMiiFunc)(const void* createId, u16* index);
static bool ReadRacerName(u8 id, wchar_t* dst, u32 length) {
    if(dst == nullptr || length == 0 || !IsOnline24() || id >= maxPlayers || IsOnlineCPU(id)) return false;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const u8 aid = AidOfPlayer(id);
    if(controller == nullptr || aid >= maxPlayers) return false;
    u8 slot = 0;
    for(u8 other = 0; other < id; ++other) if(AidOfPlayer(other) == aid) ++slot;
    const wchar_t* name = nullptr;
    if(aid == CurrentSub(*controller).localAid) {
        if(SectionMgr::sInstance == nullptr || SectionMgr::sInstance->sectionParams == nullptr) return false;
        const Mii* mii = SectionMgr::sInstance->sectionParams->localPlayerMiis.GetMii(slot);
        if(mii != nullptr) name = mii->isUserMii ? defaultMiiName : mii->info.name;
    }
    else {
        const RKNet::USERHandler* user = RKNet::USERHandler::sInstance;
        if(user == nullptr || slot >= 2 || (user->aidBitflag & (1 << aid)) == 0) return false;
        const u8* packet = reinterpret_cast<const u8*>(&user->receivedPackets[aid].rflPacket);
        if((*reinterpret_cast<const u32*>(packet) & (1 << slot)) == 0) return false;
        const u8* storeData = packet + 8 + slot * 0x4c;
        const bool isDefault = reinterpret_cast<IsDefaultMiiFunc>(Port(0x800ca820))(storeData + 0x18, nullptr);
        name = isDefault ? defaultMiiName : reinterpret_cast<const wchar_t*>(storeData + 2);
    }
    if(name == nullptr || name[0] == 0) return false;
    u32 i = 0;
    for(; i + 1 < length && i < 10 && name[i] != 0; ++i) dst[i] = name[i];
    dst[i] = 0;
    return true;
}

static wchar_t racerNames[maxPlayers][11];
static void CacheRacerNames() {
    for(u8 id = 0; id < maxPlayers; ++id) {
        if(!ReadRacerName(id, racerNames[id], 11)) racerNames[id][0] = 0;
    }
}

bool GetRacerConsole(u8 id, u8& aid, u8& playerIndexOnConsole, bool& isLocal) {
    if(id >= maxPlayers || !IsOnline24() || IsOnlineCPU(id)) return false;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    aid = AidOfPlayer(id);
    if(controller == nullptr || aid >= maxPlayers) return false;
    playerIndexOnConsole = 0;
    for(u8 other = 0; other < id; ++other) if(AidOfPlayer(other) == aid) ++playerIndexOnConsole;
    isLocal = aid == CurrentSub(*controller).localAid;
    return true;
}

bool GetRacerName(u8 id, wchar_t* dst, u32 length) {
    if(dst == nullptr || length == 0 || id >= maxPlayers || !IsOnline24() || IsOnlineCPU(id)) return false;
    if(racerNames[id][0] == 0) return ReadRacerName(id, dst, length);
    u32 i = 0;
    for(; i + 1 < length && i < 10 && racerNames[id][i] != 0; ++i) dst[i] = racerNames[id][i];
    dst[i] = 0;
    return true;
}

bool CanHostRoom24() {
    return OnlineSetReady() && cpusPerConsole == 0 && IsEnabled();
}

bool HostPlayers24Bit() {
    if(cpusPerConsole > 0) return OnlineSetReady() && IsEnabled();
    return IsHostingRoom24();
}

static bool AidHasCPU(u32 aid) {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if(controller == nullptr || cpusPerConsole == 0) return false;
    const u8* table = AidTable(*controller);
    for(u8 id = humanCount; id < maxPlayers; ++id) if(table[id] == aid) return true;
    return false;
}

static u8 hostTable[maxPlayers];
static bool hasHostTable = false;

static void ResetTable(u8* table) {
    for(u8 id = 0; id < maxPlayers; ++id) table[id] = noPlayer;
    hasHostTable = false;
    humanCount = 0;
}

void SyncAidTable() {
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if(controller == nullptr || !OnlineSetReady()) return;
    u8* table = AidTable(*controller);
    for(u8 id = 0; id < vanillaPlayers; ++id) table[id] = controller->aidsBelongingToPlayerIds[id];
    for(u8 id = vanillaPlayers; id < maxPlayers; ++id) table[id] = noPlayer;
}

static void ReadHostTable() {
    const RKNet::SELECTHandler* select = RKNet::SELECTHandler::sInstance;
    if(select == nullptr) return;
    const u8* map = reinterpret_cast<const u8*>(select) + 0x30;
    const u8* ext = reinterpret_cast<const Network::ExpSELECTHandler*>(select)->toSendPacket.playerIdToAidExt;
    bool isValid = false;
    for(u8 id = 0; id < vanillaPlayers; ++id) if(map[id] < maxAids || ext[id] < maxAids) isValid = true;
    if(!isValid) return;
    for(u8 id = 0; id < vanillaPlayers; ++id) {
        hostTable[id] = map[id];
        hostTable[vanillaPlayers + id] = ext[id];
    }
    hasHostTable = true;
}

static void ExtendAidTable(RKNet::Controller& controller) {
    ReadHostTable();
    u8 source[maxPlayers];
    for(u8 id = 0; id < maxPlayers; ++id) {
        if(hasHostTable) source[id] = hostTable[id];
        else source[id] = id < vanillaPlayers ? controller.aidsBelongingToPlayerIds[id] : noPlayer;
    }
    u8 idsPerAid[maxAids];
    for(u8 aid = 0; aid < maxAids; ++aid) idsPerAid[aid] = 0;
    for(u8 id = 0; id < maxPlayers; ++id) if(source[id] < maxAids) ++idsPerAid[source[id]];
    u8* table = AidTable(controller);
    u8 count = 0;
    for(u8 id = 0; id < maxPlayers; ++id) if(source[id] < maxAids) table[count++] = source[id];
    humanCount = count;
    for(u8 round = 0; round < cpusPerConsole; ++round) {
        for(u8 id = 0; id < maxPlayers; ++id) {
            const u8 aid = source[id];
            if(aid < maxAids && idsPerAid[aid] == 1 && count < maxPlayers) table[count++] = aid;
        }
    }
    while(count < maxPlayers) table[count++] = noPlayer;
    for(u8 id = 0; id < vanillaPlayers; ++id) controller.aidsBelongingToPlayerIds[id] = table[id];
}

typedef void (*ControllerFunc)(RKNet::Controller* controller);
static void* updateAidsOriginal = nullptr;
static void UpdateAidsBelongingToPlayerIds(RKNet::Controller* controller) {
    reinterpret_cast<ControllerFunc>(updateAidsOriginal)(controller);
    SyncAidTable();
    if(IsOnline24()) ExtendAidTable(*controller);
}

static void* resetAidsOriginal = nullptr;
static void ResetAidsBelongingToPlayerIds(RKNet::Controller* controller) {
    reinterpret_cast<ControllerFunc>(resetAidsOriginal)(controller);
    ResetTable(AidTable(*controller));
}

static void NewController(HookRegs& regs, u32) {
    if(regs.gpr[3] != 0) ResetTable(reinterpret_cast<u8*>(regs.gpr[3]) + aidTableOffset);
}
static void InitController(HookRegs& regs, u32) {
    ResetTable(reinterpret_cast<u8*>(regs.gpr[30]) + aidTableOffset);
}

typedef s32 (*LocalIdFunc)(const RKNet::Controller* controller, s32 index);
static void* localIdOriginal = nullptr;
static s32 LocalIdPast12(const RKNet::Controller* controller, s32 index) {
    s32 found = 0;
    if(RKNet::PacketMgr::sInstance != nullptr) {
        const RacedataScenario& race = Racedata::sInstance->racesScenario;
        for(u8 id = 0; id < maxPlayers; ++id) {
            if(GetPlayer(race, id).playerType != PLAYER_REAL_LOCAL) continue;
            if(found == index) return id;
            ++found;
        }
        return -1;
    }
    const u8 localAid = CurrentSub(*controller).localAid;
    for(u8 id = 0; id < maxPlayers; ++id) {
        if(AidOfPlayer(id) != localAid) continue;
        if(found == index) return id;
        ++found;
    }
    return -1;
}

static s32 GetLocalPlayerId(const RKNet::Controller* controller, s32 index) {
    s32 id = reinterpret_cast<LocalIdFunc>(localIdOriginal)(controller, index);
    if(id < 0 && controller != nullptr && Racedata::sInstance != nullptr && IsConsoles24()) id = LocalIdPast12(controller, index);
    if(!IsOnline24()) return id;
    const u8 cpu = GetOwnCPUId();
    if(cpu == noPlayer) return id;
    if(RKNet::PacketMgr::sInstance == nullptr) return id == cpu ? -1 : id;
    return index == 1 ? cpu : id;
}

static CharacterId cpuCharacter = static_cast<CharacterId>(noPlayer);
static KartId cpuKart = STANDARD_KART_M;
static void PickCPUCombo() {
    if(cpuCharacter != static_cast<CharacterId>(noPlayer)) return;
    Random random;
    u32 skip = 0;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const DWC::MatchControl* match = DWC::MatchControl::sInstance;
    if(controller != nullptr && match != nullptr) {
        const u8 localAid = CurrentSub(*controller).localAid;
        skip = localAid;
        for(u32 i = 0; i < 32; ++i) if(match->nodes[i].aid == localAid) skip += match->nodes[i].pid % 97;
    }
    for(u32 i = 0; i < skip; ++i) random.Next();
    cpuCharacter = static_cast<CharacterId>(random.NextLimited(24));
    const s32 weightClass = GetCharacterWeightClass(cpuCharacter);
    cpuKart = static_cast<KartId>(STANDARD_KART_S + (weightClass >= 0 && weightClass <= 2 ? weightClass : 1));
}

void FillCPUSelectData(RKNet::SELECTPacket& packet) {
    if(!IsOnline24() || cpusPerConsole == 0) return;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if(controller == nullptr || CurrentSub(*controller).localPlayerCount != 1) return;
    PickCPUCombo();
    RKNet::SELECTPlayerData& data = packet.playersData[1];
    data = packet.playersData[0];
    data.character = cpuCharacter;
    data.kart = cpuKart;
    const u8 cpu = GetOwnCPUId();
    if(cpu == noPlayer) {
        data.prevRaceRank = 0;
        data.sumPoints = 0;
        return;
    }
    const RacedataPlayer& player = GetPlayer(Racedata::sInstance->menusScenario, cpu);
    data.prevRaceRank = player.prevFinishPos;
    data.sumPoints = player.previousScore;
}

typedef u32 (*SelectGetFunc)(const RKNet::SELECTHandler* handler, u8 aid, u8 slot);
static u32 SelectGet(u32 palAddress, u8 aid, u8 slot) {
    return reinterpret_cast<SelectGetFunc>(Port(palAddress))(RKNet::SELECTHandler::sInstance, aid, slot);
}

typedef void (*CopyMiiFunc)(MiiGroup* group, u32 srcIdx, u32 destIdx);
typedef void (*MenuDataSetupFunc)(void* r3, void* r4, void* r5, void* r6);
static void* menuDataSetupOriginal = nullptr;
static void MenuDataSetup(void* r3, void* r4, void* r5, void* r6) {
    reinterpret_cast<MenuDataSetupFunc>(menuDataSetupOriginal)(r3, r4, r5, r6);
    if(!IsOnline24()) return;
    CacheRacerNames();
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if(controller == nullptr || RKNet::SELECTHandler::sInstance == nullptr) return;
    const u8 localAid = CurrentSub(*controller).localAid;
    const u8* table = AidTable(*controller);
    RacedataScenario& menu = Racedata::sInstance->menusScenario;
    for(u8 id = vanillaPlayers; id < humanCount; ++id) {
        RacedataPlayer& player = GetPlayer(menu, id);
        const u8 aid = table[id];
        u8 slot = 0;
        for(u8 other = 0; other < id; ++other) if(table[other] == aid) ++slot;
        player.playerType = aid == localAid ? PLAYER_REAL_LOCAL : PLAYER_REAL_ONLINE;
        const u32 character = SelectGet(0x806604d4, aid, slot);
        const u32 kart = SelectGet(0x80660524, aid, slot);
        player.characterId = static_cast<CharacterId>(character <= 0x2f ? character : 0);
        player.kartId = static_cast<KartId>(kart <= 0x23 ? kart : 1);
        const RKNet::SELECTPlayerData* data = reinterpret_cast<const RKNet::SELECTPlayerData*>(SelectGet(0x806605c4, aid, slot));
        if(data != nullptr) {
            player.prevFinishPos = static_cast<u8>(data->prevRaceRank);
            player.previousScore = data->sumPoints;
        }
        player.team = GetPlayer(menu, 0).team;
        player.rating.points = 5000;
    }
    for(u8 id = humanCount; id < maxPlayers; ++id) {
        RacedataPlayer& player = GetPlayer(menu, id);
        const u8 aid = table[id];
        if(aid >= maxAids) {
            if(id >= vanillaPlayers) player.playerType = PLAYER_NONE;
            continue;
        }
        if(cpusPerConsole == 0) continue;
        u8 owner = 0;
        while(owner < humanCount && table[owner] != aid) ++owner;
        const RacedataPlayer& ownerPlayer = GetPlayer(menu, owner);
        player.hudSlotId = -1;
        player.realControllerChannel = -1;
        if(aid == localAid) {
            PickCPUCombo();
            player.playerType = PLAYER_CPU;
            player.characterId = cpuCharacter;
            player.kartId = cpuKart;
        }
        else {
            player.playerType = PLAYER_REAL_ONLINE;
            const u32 character = SelectGet(0x806604d4, aid, 1);
            const u32 kart = SelectGet(0x80660524, aid, 1);
            player.characterId = static_cast<CharacterId>(character <= 0x2f ? character : 0);
            player.kartId = static_cast<KartId>(kart <= 0x23 ? kart : 1);
        }
        const RKNet::SELECTPlayerData* data = reinterpret_cast<const RKNet::SELECTPlayerData*>(SelectGet(0x806605c4, aid, 1));
        if(data != nullptr) {
            player.prevFinishPos = static_cast<u8>(data->prevRaceRank);
            player.previousScore = data->sumPoints;
        }
        player.team = ownerPlayer.team;
        player.rating.points = ownerPlayer.rating.points;
        if(id < vanillaPlayers && SectionMgr::sInstance != nullptr && SectionMgr::sInstance->sectionParams != nullptr) {
            MiiGroup& miis = SectionMgr::sInstance->sectionParams->playerMiis;
            reinterpret_cast<CopyMiiFunc>(Port(0x805fabf4))(&miis, owner, id);
        }
    }
}

typedef void (*PacketMgrFunc)(RKNet::PacketMgr* mgr);
static void* packetMgrUpdateOriginal = nullptr;
static void PacketMgrUpdate(RKNet::PacketMgr* mgr) {
    const bool raise = HasExtraRacers() && IsOnline24();
    u8& count = Racedata::sInstance->racesScenario.playerCount;
    const u8 saved = count;
    if(raise) count = GetRacePlayerCount();
    reinterpret_cast<PacketMgrFunc>(packetMgrUpdateOriginal)(mgr);
    if(raise) count = saved;
}

static inline u32 RegD(u32 word) { return (word >> 21) & 0x1F; }
static inline u32 RegA(u32 word) { return (word >> 16) & 0x1F; }
static inline u8 LoadByte(const HookRegs& regs, u32 word) {
    return *reinterpret_cast<const u8*>(regs.gpr[RegA(word)] + static_cast<s16>(word & 0xFFFF));
}

static void DoubledLoop(HookRegs& regs, u32 original) {
    regs.ctr = regs.gpr[RegD(original)] << (IsOnline24() ? 1 : 0);
}

static void LocalCountWithCPU(HookRegs& regs, u32 original) {
    u32 count = LoadByte(regs, original);
    if(InRace24() && GetOwnCPUId() != noPlayer) ++count;
    regs.gpr[RegD(original)] = count;
}

typedef void (*SetTextBoxMessageFunc)(void* control, const char* pane, u32 bmgId, const void* info);
static void PlayerNameInResults(HookRegs& regs, u32) {
    void* control = reinterpret_cast<void*>(regs.gpr[3]);
    const char* pane = reinterpret_cast<const char*>(regs.gpr[4]);
    static wchar_t names[maxPlayers][16];
    const u8 id = regs.gpr[31];
    if(id < maxPlayers && GetRacerName(id, names[id], 16)) {
        Text::Info info;
        info.strings[0] = names[id];
        reinterpret_cast<SetTextBoxMessageFunc>(Port(0x8063dcbc))(control, pane, UI::BMG_TEXT, &info);
        return;
    }
    reinterpret_cast<SetTextBoxMessageFunc>(Port(0x8063dcbc))(control, pane, regs.gpr[5], reinterpret_cast<const void*>(regs.gpr[6]));
}

static void CPUNameInResults(HookRegs& regs, u32 original) {
    u32 type = *reinterpret_cast<const u32*>(regs.gpr[RegA(original)] + static_cast<s16>(original & 0xFFFF));
    if(HasNoMiiSlot(regs.gpr[31])) type = PLAYER_CPU;
    regs.gpr[RegD(original)] = type;
}

static void CharacterIconForCPU(HookRegs& regs, u32 original, u8 id) {
    regs.gpr[RegD(original)] = ShowsCharacter(id) ? 0 : 1;
}
static void ResultIconForCPU(HookRegs& regs, u32 original) { CharacterIconForCPU(regs, original, regs.gpr[30]); }
static void TotalIconForCPU(HookRegs& regs, u32 original) { CharacterIconForCPU(regs, original, regs.gpr[28]); }
static void NoBalloonForCPU(HookRegs& regs, u32 original) {
    u32 type = *reinterpret_cast<const u32*>(regs.gpr[RegA(original)] + static_cast<s16>(original & 0xFFFF));
    if(ShowsCharacter(regs.gpr[30])) type = PLAYER_CPU;
    regs.gpr[RegD(original)] = type;
}

typedef void (*SetMiiPaneFunc)(void* control, const char* pane, void* miiGroup, u32 id, u32 type);
typedef void (*SetPictureFunc)(void* control, const char* pane, const char* texture);
typedef const char* (*CharacterIconFunc)(u32 character);
typedef u32 (*CharacterBmgFunc)(u32 character, u32 r4);
static u32 CharacterOf(u8 id) { return GetPlayer(Racedata::sInstance->menusScenario, id).characterId; }
static void WifiResultIcon(HookRegs& regs, u32) {
    void* control = reinterpret_cast<void*>(regs.gpr[3]);
    const char* pane = reinterpret_cast<const char*>(regs.gpr[4]);
    const u8 id = regs.gpr[29];
    if(ShowsCharacter(id)) {
        const char* texture = reinterpret_cast<CharacterIconFunc>(Port(0x80860acc))(CharacterOf(id));
        reinterpret_cast<SetPictureFunc>(Port(0x8063e0f0))(control, pane, texture);
    }
    else reinterpret_cast<SetMiiPaneFunc>(Port(0x8063e3dc))(control, pane, reinterpret_cast<void*>(regs.gpr[5]),
        regs.gpr[6], regs.gpr[7]);
}
static void WifiResultName(HookRegs& regs, u32) {
    void* control = reinterpret_cast<void*>(regs.gpr[3]);
    const char* pane = reinterpret_cast<const char*>(regs.gpr[4]);
    const u8 id = regs.gpr[29];
    static wchar_t names[maxPlayers][16];
    if(GetRacerName(id, names[id], 16) && (id >= vanillaPlayers || ShowsCharacter(id))) {
        Text::Info info;
        info.strings[0] = names[id];
        reinterpret_cast<SetTextBoxMessageFunc>(Port(0x8063dcbc))(control, pane, UI::BMG_TEXT, &info);
    }
    else if(HasNoMiiSlot(id)) {
        const u32 bmgId = reinterpret_cast<CharacterBmgFunc>(Port(0x80833774))(CharacterOf(id), 1);
        reinterpret_cast<SetTextBoxMessageFunc>(Port(0x8063dcbc))(control, pane, bmgId, nullptr);
    }
    else reinterpret_cast<SetTextBoxMessageFunc>(Port(0x8063dcbc))(control, pane, regs.gpr[5],
        reinterpret_cast<const void*>(regs.gpr[6]));
}

bool FitsWifiResults(u8 id) {
    if(!IsOnline24() || id >= maxPlayers) return true;
    const RacedataPlayer& player = GetPlayer(Racedata::sInstance->menusScenario, id);
    if(player.playerType == PLAYER_NONE) return true;
    return static_cast<u8>(player.unknown_0xe0 - 1) < maxPlayers;
}

static void CPUAsOnlineLocal(HookRegs& regs, u32 original) {
    u32 type = *reinterpret_cast<const u32*>(regs.gpr[RegA(original)] + static_cast<s16>(original & 0xFFFF));
    if(type == PLAYER_CPU && IsOnline24() && regs.gpr[28] == GetOwnCPUId()) type = PLAYER_REAL_LOCAL;
    regs.gpr[RegD(original)] = type;
}

static void RemoteCountWithCPU(HookRegs& regs, u32 original) {
    const u8 aid = regs.gpr[28];
    u32 count = aid < vanillaPlayers ? LoadByte(regs, original) : PlayersAtAid(aid);
    if(aid >= vanillaPlayers && !IsOnline24()) count = 0;
    if(count == 1 && InRace24() && AidHasCPU(regs.gpr[28])) count = 2;
    regs.gpr[RegD(original)] = count;
}

#ifdef P24TEST
static u8 SlotOf(u8 id) {
    const u8 aid = AidOfPlayer(id);
    u8 slot = 0;
    for(u8 other = 0; other < id; ++other) if(AidOfPlayer(other) == aid) ++slot;
    return slot;
}
static u8 FirstCPUOf(u8 aid) {
    for(u8 id = humanCount; id < maxPlayers; ++id) if(AidOfPlayer(id) == aid) return id;
    return noPlayer;
}
typedef void (*SetRacedataFunc)(RKNet::PacketMgr* mgr, const void* data, u32 size, u32 id);
static void* setRacedataOriginal = nullptr;
static void SetRACEDATAs(RKNet::PacketMgr* mgr, const void* data, u32 size, u32 id) {
    if(IsOnline24() && id < maxPlayers && SlotOf(id) >= 2) return;
    reinterpret_cast<SetRacedataFunc>(setRacedataOriginal)(mgr, data, size, id);
}
static void CheckRumble(HookRegs& regs, u32) {
    const u32 holder = regs.gpr[3];
    u32 rumble = *reinterpret_cast<const u32*>(holder + 0x20);
    const bool isMem = (rumble >= 0x80000000 && rumble < 0x81800000) || (rumble >= 0x90000000 && rumble < 0x94000000);
    if(rumble != 0 && !isMem) {
        static bool reported = false;
        if(!reported) {
            reported = true;
            const u32 input = *reinterpret_cast<const u32*>(Port(0x809bd70c));
            OSReport("[VK 24P] holder %08x: rumble %08x, Input::Manager %08x (+%x)\n", holder, rumble, input, holder - input);
            const Raceinfo* raceinfo = Raceinfo::sInstance;
            for(u8 id = 0; raceinfo != nullptr && id < GetRacePlayerCount(); ++id) {
                if((u32)raceinfo->players[id]->realControllerHolder == holder) OSReport("[VK 24P]   holder of id %d\n", id);
            }
            const u32* words = reinterpret_cast<const u32*>(holder);
            for(int i = 0; i < 0x60; i += 8) {
                OSReport("[VK 24P]   +%03x %08x %08x %08x %08x %08x %08x %08x %08x\n", i * 4, words[i], words[i + 1],
                    words[i + 2], words[i + 3], words[i + 4], words[i + 5], words[i + 6], words[i + 7]);
            }
        }
        rumble = 0;
    }
    regs.gpr[0] = rumble;
}

typedef const void* (*GetRacedataFunc)(const RKNet::PacketMgr* mgr, u32 id);
static void* getRacedataOriginal = nullptr;
static const void* GetRACEDATAPacket(const RKNet::PacketMgr* mgr, u32 id) {
    if(IsOnline24() && id < maxPlayers && SlotOf(id) >= 2) id = FirstCPUOf(AidOfPlayer(id));
    return reinterpret_cast<GetRacedataFunc>(getRacedataOriginal)(mgr, id);
}
#endif

static const WordPatch words[] ={
#include <Race/Players24/Online24Net.inc>
};

static const HookPatch hooks[] ={
    { 0x80655b58, 0x2c030000, NewController, true },
    { 0x80656144, 0x981e2920, InitController, true },
    { 0x806539b4, 0x7c0903a6, DoubledLoop, false },
    { 0x80653af0, 0x7c0903a6, DoubledLoop, false },
    { 0x806555ac, 0x7e8903a6, DoubledLoop, false },
    { 0x80658a78, 0x7c0903a6, DoubledLoop, false },
    { 0x80659d80, 0x7c0903a6, DoubledLoop, false },
    { 0x80659eb0, 0x7c0903a6, DoubledLoop, false },
    { 0x8065becc, 0x7f6903a6, DoubledLoop, false },
    { 0x8065dddc, 0x7e4903a6, DoubledLoop, false },
    { 0x8065e688, 0x7ee903a6, DoubledLoop, false },
    { 0x80664404, 0x7c0903a6, DoubledLoop, false },
    { 0x80654ddc, 0x7fe903a6, DoubledLoop, false },
    { 0x80655208, 0x7fa903a6, DoubledLoop, false },
    { 0x8065509c, 0x88040058, LocalCountWithCPU, false },
    { 0x80655108, 0x88030058, LocalCountWithCPU, false },
    { 0x8065c66c, 0x88040058, LocalCountWithCPU, false },
    { 0x8065dce8, 0x88030058, LocalCountWithCPU, false },
    { 0x8065f6bc, 0x88040058, LocalCountWithCPU, false },
    { 0x8065e06c, 0x8803005b, RemoteCountWithCPU, false },
    { 0x8058f610, 0x80030038, CPUAsOnlineLocal, false },
    { 0x807f5358, 0x81080010, CPUNameInResults, false },
    { 0x807f53e8, 0x4be488d5, PlayerNameInResults, false },
    { 0x807f6068, 0x38800001, ResultIconForCPU, false },
    { 0x807f502c, 0x38800001, TotalIconForCPU, false },
    { 0x807f0470, 0x80030038, NoBalloonForCPU, false },
    { 0x806458e8, 0x4bff8af5, WifiResultIcon, false },
    { 0x80645908, 0x4bff8ad5, WifiResultIcon, false },
    { 0x8064593c, 0x4bff8381, WifiResultName, false },
#ifdef P24TEST
    { 0x805211b4, 0x80030020, CheckRumble, false },
#endif
};

static const FuncHook funcs[] ={
    { 0x80659bc0, 0x9421fff0, reinterpret_cast<const void*>(UpdateAidsBelongingToPlayerIds), &updateAidsOriginal },
    { 0x80659d20, 0x380000ff, reinterpret_cast<const void*>(ResetAidsBelongingToPlayerIds), &resetAidsOriginal },
    { 0x80659d58, 0x3ca0809c, reinterpret_cast<const void*>(GetLocalPlayerId), &localIdOriginal },
    { 0x80650e24, 0x9421ff90, reinterpret_cast<const void*>(MenuDataSetup), &menuDataSetupOriginal },
    { 0x80653728, 0x9421ffe0, reinterpret_cast<const void*>(PacketMgrUpdate), &packetMgrUpdateOriginal },
#ifdef P24TEST
    { 0x80653960, 0x9421ffe0, reinterpret_cast<const void*>(SetRACEDATAs), &setRacedataOriginal },
    { 0x80653abc, 0x3c60809c, reinterpret_cast<const void*>(GetRACEDATAPacket), &getRacedataOriginal },
#endif
};
static const u32 onlineInstances[] ={ 0x809c20d8, 0x809c20f0 };
static PatchSet onlineSet("Online 24", words, P24_COUNT(words), nullptr, 0, onlineInstances, P24_COUNT(onlineInstances),
    hooks, P24_COUNT(hooks), funcs, P24_COUNT(funcs), true);

bool OnlineSetReady() { return IsReady() && !onlineSet.skipped && OnlineItemsReady() && OnlineRaceReady(); }

}
}
