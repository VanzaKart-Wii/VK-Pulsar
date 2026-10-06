#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <core/rvl/OS/OSCache.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <Settings/Settings.hpp>
#include <Race/Players24/Players24.hpp>
#include <Race/Players24/Patches.hpp>

extern "C" void OSReport(const char* format, ...);
extern "C" void ICInvalidateRange(void* addr, u32 size);

namespace Pulsar {
namespace Players24 {

static const u32 sites[] = {
#include <Race/Players24/RacedataSites.inc>
};
static const u32 siteCount = sizeof(sites) / sizeof(u32);
static const u32 stubSize = 5;
static u32 stubs[siteCount * stubSize];

static bool isReady = false;
bool IsReady() { return isReady; }
bool IsEnabled() {
    return Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_RACE, SETTINGRACE_SCROLL_PLAYERS) == RACESETTING_PLAYERS_24;
}

static void CopyPlayer(RacedataPlayer& dest, const RacedataPlayer& src) {
    memcpy(reinterpret_cast<u8*>(&dest) + 4, reinterpret_cast<const u8*>(&src) + 4, sizeof(RacedataPlayer) - 4);
}

static void CopyShadowPlayers(RacedataScenario& dest, const RacedataScenario& src) {
    for(u8 id = vanillaPlayers; id < maxPlayers; ++id) CopyPlayer(GetPlayer(dest, id), GetPlayer(src, id));
}

static void SetShadowType(RacedataScenario& scenario, PlayerType type) {
    for(u8 id = vanillaPlayers; id < maxPlayers; ++id) GetPlayer(scenario, id).playerType = type;
}

static inline u32 Addic(u32 d, u32 a, s16 imm) { return (12 << 26) | (d << 21) | (a << 16) | (u16)imm; }
static inline u32 Rlwinm(u32 a, u32 s, u32 sh, u32 mb, u32 me) { return (21 << 26) | (s << 21) | (a << 16) | (sh << 11) | (mb << 6) | (me << 1); }
static inline u32 Mulli(u32 d, u32 a, s16 imm) { return (7 << 26) | (d << 21) | (a << 16) | (u16)imm; }
static inline u32 BranchTo(u32 from, u32 to) { return 0x48000000 | ((to - from) & 0x03FFFFFC); }
static inline bool InBranchRange(u32 from, u32 to) {
    const s32 delta = (s32)to - (s32)from;
    return delta >= -0x02000000 && delta <= 0x01FFFFFC;
}

static void WriteStub(u32* stub, u32 site, u32 original) {
    const u32 d = (original >> 21) & 0x1F;
    const u32 a = (original >> 16) & 0x1F;
    stub[0] = Addic(d, a, 52);
    stub[1] = Rlwinm(d, d, 0, 26, 24);
    stub[2] = Addic(d, d, -52);
    stub[3] = Mulli(d, d, 0xf0);
    stub[4] = BranchTo((u32)&stub[4], site + 4);
}

static const u32 allocCall = 0x8052fe7c;
static const u32 allocCallOriginal = 0x4bcf9f51;
typedef RacedataPlayer* (*PlayerCtor)(RacedataPlayer* player);
static RacedataPlayer* racedataPlayerCtor(RacedataPlayer* player) {
    return reinterpret_cast<PlayerCtor>(Port(0x8052d96c))(player);
}

static void* AllocRacedata(u32 size) {
    u8* block = static_cast<u8*>(::operator new(shadowPrefix + size));
    if(block == nullptr) return nullptr;
    memset(block, 0, shadowPrefix);
    u8* racedata = block + shadowPrefix;
    Racedata* shadow = reinterpret_cast<Racedata*>(racedata + shadowOffset);
    RacedataScenario* scenarios[3] ={ &shadow->racesScenario, &shadow->menusScenario, &shadow->awardScenario };
    for(int i = 0; i < 3; ++i) {
        for(int id = 0; id < vanillaPlayers; ++id) {
            RacedataPlayer& player = scenarios[i]->players[id];
            racedataPlayerCtor(&player);
            player.playerType = PLAYER_NONE;
            player.prevFinishPos = vanillaPlayers + id + 1;
            player.unknown_0xe0 = vanillaPlayers + id + 1;
        }
    }
    return racedata;
}

static const u32 initCall = 0x80543910;
static const u32 initCallOriginal = 0x4bfea431;
typedef void (*RacedataInitFunc)(Racedata* racedata);
static void racedataInit(Racedata* racedata) { reinterpret_cast<RacedataInitFunc>(Port(0x8052dd40))(racedata); }

static void InitRacedata(Racedata* racedata) {
    racedataInit(racedata);
    RacedataScenario& menu = racedata->menusScenario;
    for(u8 id = vanillaPlayers; id < maxPlayers; ++id) {
        RacedataPlayer& player = GetPlayer(menu, id);
        CopyPlayer(player, menu.players[1]);
        player.playerType = PLAYER_NONE;
        player.hudSlotId = -1;
        player.realControllerChannel = -1;
        player.prevFinishPos = id + 1;
        player.unknown_0xe0 = id + 1;
    }
    CopyShadowPlayers(racedata->racesScenario, menu);
}

static void ClearScenario(RacedataScenario& scenario) {
    RacedataSettings& settings = scenario.settings;
    settings.modeFlags &= ~7;
    settings.itemMode = static_cast<ItemMode>(0);
    settings.cpuMode = static_cast<CpuMode>(1);
    settings.lapCount = 3;
    settings.engineClass = static_cast<EngineClass>(1);
    for(u8 id = 0; id < maxPlayers; ++id) {
        RacedataPlayer& player = GetPlayer(scenario, id);
        player.previousScore = 0;
        player.gpHiddenScore = 0;
        player.prevFinishPos = id + 1;
        player.unknown_0xe0 = id + 1;
    }
    settings.raceNumber = 0;
    settings.lapCount = 3;
}

static void ResetScenarios(Racedata& racedata) {
    ClearScenario(racedata.menusScenario);
    ClearScenario(racedata.awardScenario);
    SetShadowType(racedata.menusScenario, PLAYER_NONE);
}

static u8 UpdatePrevFromCur(Racedata& racedata) {
    RacedataScenario& menu = racedata.menusScenario;
    if(menu.settings.raceNumber < 100) ++menu.settings.raceNumber;
    else menu.settings.raceNumber = 0;
    for(u8 id = 0; id < maxPlayers; ++id) {
        RacedataPlayer& player = GetPlayer(menu, id);
        player.prevFinishPos = player.finishPos;
        player.previousScore = player.score;
    }
    return menu.settings.raceNumber;
}

static void ComputePlayerCounts(const RacedataScenario& scenario, u8* playerCount, u8* screenCount, u8* localPlayerCount) {
    u8 players = 0;
    u8 screens = 0;
    u8 locals = 0;
    for(u8 id = 0; id < maxPlayers; ++id) {
        const PlayerType type = GetPlayer(scenario, id).playerType;
        if(type == PLAYER_NONE) continue;
        ++players;
        if(type != PLAYER_REAL_LOCAL) continue;
        if(screens < 4) ++screens;
        ++locals;
    }
    if(screens == 0) screens = 1;
    if(screens == 3) screens = 4;
    const u32 gameType = scenario.settings.gametype;
    if(gameType == 2) screens = 1;
    else if(gameType == 3) screens = 2;
    else if(gameType == 4) screens = 4;
    if(scenario.settings.gamemode == MODE_AWARD) {
        if(gameType == 7) {
            if(players > 3) players = 3;
        }
        else if(players > 6) players = 6;
    }
    *playerCount = players;
    *screenCount = screens;
    *localPlayerCount = locals;
}

typedef void (*ScenarioFunc)(RacedataScenario* scenario);
typedef void (*CountsFunc)(RacedataScenario* scenario, u8* playerCount, u8* screenCount, u8* localPlayerCount);
typedef void (*InitScreensFunc)(RacedataScenario* scenario, u8 screenCount);
typedef void (*InitControllersFunc)(RacedataScenario* scenario, const RacedataScenario* prev);
typedef RacedataScenario* (*CopyFunc)(RacedataScenario* dest, const RacedataScenario* src);
static RacedataScenario* copyScenario(RacedataScenario* dest, const RacedataScenario* src) {
    return reinterpret_cast<CopyFunc>(Port(0x805305ac))(dest, src);
}

static void* CallTarget(u32 palBlAddress) {
    const u32 blAddress = Port(palBlAddress);
    const u32 word = Read32(blAddress);
    s32 offset = word & 0x03FFFFFC;
    if(offset & 0x02000000) offset -= 0x04000000;
    return reinterpret_cast<void*>(blAddress + offset);
}
static const u32 initCompetitionCall = 0x8052fbd0;
static const u32 updateFromPrevCall = 0x8052fbd8;
static const u32 countsCall = 0x8052fc78;
static const u32 initScreensCall = 0x8052fe04;
static const u32 initControllersCall = 0x8052fe10;
static const u32 initRNGCall = 0x8052fe18;

static bool IsOnlineMode(u32 gamemode) { return gamemode >= MODE_PRIVATE_VS && gamemode <= MODE_PRIVATE_BATTLE; }

static bool HasShadowRacers(const RacedataScenario& scenario) {
    for(u8 id = vanillaPlayers; id < maxPlayers; ++id) if(GetPlayer(scenario, id).playerType != PLAYER_NONE) return true;
    return false;
}

static void PackOnlineGrid(RacedataScenario& scenario, const u8* ranks) {
    u8 grid[maxPlayers];
    u8 next = 1;
    for(u8 rank = 1; rank <= maxPlayers; ++rank) {
        for(u8 id = 0; id < maxPlayers; ++id) {
            if(GetPlayer(scenario, id).playerType != PLAYER_NONE && ranks[id] == rank) grid[id] = next++;
        }
    }
    for(u8 id = 0; id < maxPlayers; ++id) {
        if(GetPlayer(scenario, id).playerType != PLAYER_NONE && (ranks[id] == 0 || ranks[id] > maxPlayers)) grid[id] = next++;
    }
    for(u8 id = 0; id < maxPlayers; ++id) {
        RacedataPlayer& player = GetPlayer(scenario, id);
        if(player.playerType != PLAYER_NONE) player.prevFinishPos = grid[id];
    }
}

typedef ControllerType (*GetControllerTypeFunc)(const void* controller);
static void GiveScreensPastId12(RacedataScenario& scenario, u8 screenCount) {
    u8 locals = 0;
    for(u8 id = 0; id < vanillaPlayers; ++id) if(scenario.players[id].playerType == PLAYER_REAL_LOCAL) ++locals;
    for(u8 id = vanillaPlayers; id < maxPlayers; ++id) {
        RacedataPlayer& player = GetPlayer(scenario, id);
        if(player.playerType != PLAYER_REAL_LOCAL) continue;
        const u8 slot = locals;
        if(slot < screenCount && slot < 4) {
            for(u8 other = 0; other < maxPlayers; ++other) {
                RacedataPlayer& lent = GetPlayer(scenario, other);
                if(other != id && lent.hudSlotId == slot && lent.playerType != PLAYER_REAL_LOCAL) lent.hudSlotId = -1;
            }
            player.hudSlotId = slot;
            scenario.settings.hudPlayerIds[slot] = id;
        }
        player.realControllerChannel = locals;
        const u8* inputMgr = *reinterpret_cast<u8* const*>(Port(0x809bd70c));
        const void* controller = inputMgr == nullptr ? nullptr : *reinterpret_cast<void* const*>(inputMgr + locals * 0xec + 8);
        if(controller == nullptr) player.controllerType = static_cast<ControllerType>(-1);
        else {
            const u32* vtable = *reinterpret_cast<u32* const*>(controller);
            player.controllerType = reinterpret_cast<GetControllerTypeFunc>(vtable[0x10 / 4])(controller);
        }
        ++locals;
    }
}

static void InitScenario(RacedataScenario& scenario, const RacedataScenario* prev) {
    u8 playerCount = 0;
    u8 screenCount = 0;
    u8 localPlayerCount = 0;
    RacedataSettings& settings = scenario.settings;
    if(settings.modeFlags & 4) reinterpret_cast<ScenarioFunc>(CallTarget(initCompetitionCall))(&scenario);
    const bool packGrid = IsOnlineMode(settings.gamemode) && HasShadowRacers(scenario);
    u8 ranks[maxPlayers];
    if(packGrid) for(u8 id = 0; id < maxPlayers; ++id) ranks[id] = GetPlayer(scenario, id).prevFinishPos;
    reinterpret_cast<ScenarioFunc>(CallTarget(updateFromPrevCall))(&scenario);
    if(packGrid) PackOnlineGrid(scenario, ranks);
    for(u8 id = 0; id < maxPlayers; ++id) {
        RacedataPlayer& player = GetPlayer(scenario, id);
        player.hudSlotId = -1;
        player.realControllerChannel = -1;
    }
    for(int i = 0; i < 4; ++i) settings.hudPlayerIds[i] = 0xFF;
    reinterpret_cast<CountsFunc>(CallTarget(countsCall))(&scenario, &playerCount, &screenCount, &localPlayerCount);
    const u8 realScreenCount = screenCount;
    if(settings.gametype == 5) screenCount = 1;
    if(settings.raceNumber == 0) {
        if(!IsOnlineMode(settings.gamemode)) {
            for(u8 id = 0; id < playerCount; ++id) {
                RacedataPlayer& player = GetPlayer(scenario, id);
                player.previousScore = 0;
                player.prevFinishPos = playerCount - id;
                player.unknown_0xe0 = playerCount - id;
            }
        }
    }
    reinterpret_cast<InitScreensFunc>(CallTarget(initScreensCall))(&scenario, screenCount);
    GiveScreensPastId12(scenario, screenCount);
    for(u8 id = vanillaPlayers; id < maxPlayers; ++id) {
        RacedataPlayer& player = GetPlayer(scenario, id);
        if(player.playerType == PLAYER_CPU) player.controllerType = static_cast<ControllerType>(-1);
    }
    reinterpret_cast<InitControllersFunc>(CallTarget(initControllersCall))(&scenario, prev);
    reinterpret_cast<ScenarioFunc>(CallTarget(initRNGCall))(&scenario);
    scenario.playerCount = playerCount;
    scenario.screenCount = screenCount;
    scenario.unknown_0x7 = realScreenCount;
    scenario.localPlayerCount = localPlayerCount;
}

static u8 racePlayerCount = 0;
u8 GetRacePlayerCount() { return racePlayerCount; }

static void InitRace(Racedata& racedata) {
    InitScenario(racedata.menusScenario, &racedata.racesScenario);
    copyScenario(&racedata.racesScenario, &racedata.menusScenario);
    CopyShadowPlayers(racedata.racesScenario, racedata.menusScenario);
    racePlayerCount = racedata.racesScenario.playerCount;
}

static u8 CountMenuPlayers() {
    const RacedataScenario& menu = Racedata::sInstance->menusScenario;
    u8 count = 0;
    for(u8 id = 0; id < maxPlayers; ++id) if(GetPlayer(menu, id).playerType != PLAYER_NONE) ++count;
    return count;
}

typedef u8 (*LocalCountFunc)();
static u8 getLocalPlayerCount() { return reinterpret_cast<LocalCountFunc>(Port(0x808605fc))(); }
typedef bool (*UnlockedFunc)(CharacterId character);
static bool isCharacterUnlocked(CharacterId character) {
    return reinterpret_cast<UnlockedFunc>(Port(0x8081d020))(character);
}
static u8 CountMenuPlayers();

static void PickCPUCharacters() {
    const u8 first = getLocalPlayerCount();
    const u8 count = CountMenuPlayers();
    RacedataScenario& menu = Racedata::sInstance->menusScenario;
    s32 classCounts[3] ={ 0, 0, 0 };
    CharacterId chosen[maxPlayers];
    Random random;
    for(u8 id = 0; id < first && id < maxPlayers; ++id) {
        chosen[id] = GetPlayer(menu, id).characterId;
        ++classCounts[GetCharacterWeightClass(chosen[id])];
    }
    const s32 perClass = (count + count % 3) / 3;
    for(u8 id = first; id < count && id < maxPlayers; ++id) {
        CharacterId character;
        for(u32 tries = 0;; ++tries) {
            character = static_cast<CharacterId>(random.NextLimited(24));
            if(!isCharacterUnlocked(character)) continue;
            if(tries >= 500) break;
            if(classCounts[GetCharacterWeightClass(character)] >= perClass) continue;
            bool isTaken = false;
            for(u8 other = 0; other < id; ++other) if(chosen[other] == character) isTaken = true;
            if(!isTaken) break;
        }
        chosen[id] = character;
        ++classCounts[GetCharacterWeightClass(character)];
        GetPlayer(menu, id).characterId = character;
    }
}

static const u32 singlePlayerCall = 0x8084f670;
static const u32 singlePlayerCallOriginal = 0x4bfe80b1;
static void OnSinglePlayerModeChosen(Pages::Menu& page, PageId id, PushButton& button) {
    RacedataScenario& menu = Racedata::sInstance->menusScenario;
    const bool fill = IsEnabled() && menu.settings.gamemode == MODE_VS_RACE;
    SetShadowType(menu, fill ? PLAYER_CPU : PLAYER_NONE);
    page.LoadNextPageById(id, button);
}

struct FuncReplacement {
    u32 address;
    u32 original;
    void* replacement;
};
static const FuncReplacement replacements[] ={
    { 0x8052e454, 0x80831780, ResetScenarios },
    { 0x80531ce4, 0x8883177c, UpdatePrevFromCur },
    { 0x8052f788, 0x38000003, ComputePlayerCounts },
    { 0x8052fb90, 0x9421ffe0, InitScenario },
    { 0x805302c4, 0x9421ffd0, InitRace },
    { 0x80860500, 0x3c60809c, CountMenuPlayers },
    { 0x8083ec28, 0x9421ff70, PickCPUCharacters },
};
static const u32 replacementCount = sizeof(replacements) / sizeof(FuncReplacement);

struct RacedataCall {
    u32 address;
    u32 original;
    void* target;
};
static const RacedataCall calls[] ={
    { allocCall, allocCallOriginal, AllocRacedata },
    { initCall, initCallOriginal, InitRacedata },
    { singlePlayerCall, singlePlayerCallOriginal, OnSinglePlayerModeChosen },
};
static const u32 callCount = sizeof(calls) / sizeof(RacedataCall);

static bool Disabled(const char* problem, u32 palAddress, u32 address) {
    OSReport("[VK 24P] %s at %08x (PAL %08x), 24 players disabled\n", problem, address, palAddress);
    return false;
}

static u32 portedSites[siteCount];
static u32 portedReplacements[replacementCount];
static u32 portedCalls[callCount];
static bool CheckRacedataPatches() {
    for(u32 i = 0; i < siteCount; ++i) {
        const u32 address = Port(sites[i]);
        if(address == 0) return Disabled("no address", sites[i], 0);
        const u32 word = Read32(address);
        if((word & 0xFC00FFFF) != 0x1C0000F0 || !InBranchRange(address, (u32)&stubs[i * stubSize])) {
            return Disabled("unexpected instruction", sites[i], address);
        }
        portedSites[i] = address;
    }
    for(u32 i = 0; i < replacementCount; ++i) {
        const FuncReplacement& replacement = replacements[i];
        const u32 address = Port(replacement.address);
        if(address == 0) return Disabled("no address", replacement.address, 0);
        if(!SameInstruction(replacement.address, replacement.original, address, Read32(address), true)
            || !InBranchRange(address, (u32)replacement.replacement)) {
            return Disabled("unexpected instruction", replacement.address, address);
        }
        portedReplacements[i] = address;
    }
    for(u32 i = 0; i < callCount; ++i) {
        const u32 address = Port(calls[i].address);
        if(address == 0) return Disabled("no address", calls[i].address, 0);
        if(!SameInstruction(calls[i].address, calls[i].original, address, Read32(address), false)
            || !InBranchRange(address, (u32)calls[i].target)) {
            return Disabled("unexpected instruction", calls[i].address, address);
        }
        portedCalls[i] = address;
    }
    const u32 initCalls[] ={ initCompetitionCall, updateFromPrevCall, countsCall, initScreensCall, initControllersCall, initRNGCall };
    for(u32 i = 0; i < sizeof(initCalls) / sizeof(u32); ++i) {
        const u32 address = Port(initCalls[i]);
        if(address == 0 || (Read32(address) & 0xFC000003) != 0x48000001) return Disabled("expected a bl", initCalls[i], address);
    }
    return true;
}

static void ApplyPatches() {
    if(!IsSupportedDisc()) {
        OSReport("[VK 24P] unknown disc, 24 players unavailable\n");
        return;
    }
    if(!CheckRacedataPatches()) return;
    static u32 racedataAddresses[siteCount + replacementCount + callCount];
    u32 racedataAddressCount = 0;
    for(u32 i = 0; i < siteCount; ++i) racedataAddresses[racedataAddressCount++] = sites[i];
    for(u32 i = 0; i < replacementCount; ++i) racedataAddresses[racedataAddressCount++] = replacements[i].address;
    for(u32 i = 0; i < callCount; ++i) racedataAddresses[racedataAddressCount++] = calls[i].address;
    if(!PatchSet::CheckOverlaps(racedataAddresses, racedataAddressCount)) {
        OSReport("[VK 24P] 24 players disabled\n");
        return;
    }
    u32 setCount = 0;
    for(const PatchSet* set = PatchSet::first; set != nullptr; set = set->next, ++setCount) {
        if(set->standalone) continue;
        set->skipped = false;
        if(!set->Check()) {
            if(set->optional) {
                OSReport("[VK 24P] %s skipped\n", set->name);
                set->skipped = true;
                continue;
            }
            OSReport("[VK 24P] 24 players disabled\n");
            return;
        }
    }

    for(u32 i = 0; i < siteCount; ++i) WriteStub(&stubs[i * stubSize], portedSites[i], Read32(portedSites[i]));
    OS::DCFlushRange(stubs, sizeof(stubs));
    ICInvalidateRange(stubs, sizeof(stubs));
    for(u32 i = 0; i < siteCount; ++i) KamekRuntimeWrite::Branch(portedSites[i], (u32)&stubs[i * stubSize], false);
    for(u32 i = 0; i < replacementCount; ++i) {
        KamekRuntimeWrite::Branch(portedReplacements[i], (u32)replacements[i].replacement, false);
    }
    for(u32 i = 0; i < callCount; ++i) KamekRuntimeWrite::Branch(portedCalls[i], (u32)calls[i].target, true);
    for(const PatchSet* set = PatchSet::first; set != nullptr; set = set->next) if(!set->skipped && !set->standalone) set->Apply();
    isReady = true;
    OSReport("[VK 24P] Racedata extended: %d sites, %d functions, %d patch sets\n", siteCount, replacementCount, setCount);
}

static void ApplyAtBoot() {
    ApplyPatches();
    ApplyConsoles();
    ApplyAidLayout();
    ApplyOnlinePages();
    ApplyRoomBubbles();
}
static BootHook Players24Racedata(ApplyAtBoot, 0);

}
}
