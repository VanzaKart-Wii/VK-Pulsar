#include <Race/SaveStates.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Kart/KartPlayer.hpp>
#include <MarioKartWii/Kart/KartBody.hpp>
#include <MarioKartWii/Kart/KartPhysics.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Kart/KartWheel.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/UI/Page/RaceHUD/RaceHUD.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/Input/ControllerHolder.hpp>
#include <Settings/Settings.hpp>

namespace Pulsar {
namespace SaveStates {

static const u32 framesToLoad = 60;

struct WheelState {
    u8 physics[0x24];
};

struct State {
    Vec3 position;
    Vec3 speed0;
    Quat mainRot;
    Vec3 engineSpeed;
    float movementEngineSpeed;
    s16 boostFrames[6];
    u16 boostTypes;
    float boostMultiplier;
    float boostAcceleration;
    float boostUnknown_0x1c;
    float boostSpeedLimit;
    WheelState wheels[4];
    ItemId itemId;
    u32 itemCount;
    bool hasGolden;
    u16 goldenTimer;
    u16 checkpoint;
    float raceCompletion;
    float raceCompletionMax;
    float firstKcpLapCompletion;
    float nextCheckpointLapCompletion;
    float nextCheckpointLapCompletionMax;
    u16 currentLap;
    u8 maxLap;
    u8 currentKCP;
    u8 maxKCP;
};

static State sState;
static bool sHasState = false;
static bool sWasUsed = false;
static u32 sFramesHeld = 0;

bool WasUsed() { return sWasUsed; }

static void Reset() {
    sHasState = false;
    sWasUsed = false;
    sFramesHeld = 0;
}
static RaceLoadHook ResetOnRaceLoad(Reset);

static bool GetPlayer(u8& playerId) {
    if(Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_LANGUAGE, RADIO_SAVESTATES) != SAVESTATES_ENABLED) return false;
    if(System::sInstance->IsContext(PULSAR_MODE_OTT)) return false;
    const Racedata* racedata = Racedata::sInstance;
    const Raceinfo* raceinfo = Raceinfo::sInstance;
    if(racedata == nullptr || raceinfo == nullptr || raceinfo->stage != RACESTAGE_RACE) return false;
    const RacedataScenario& scenario = racedata->racesScenario;
    if(scenario.settings.gamemode != MODE_TIME_TRIAL) return false;
    playerId = scenario.settings.hudPlayerIds[0];
    if(playerId >= 4 || scenario.players[playerId].playerType != PLAYER_REAL_LOCAL) return false;
    return Kart::Manager::sInstance != nullptr && Item::Manager::sInstance != nullptr;
}

static Kart::Physics& GetPhysics(Kart::Player& kart) {
    return *kart.pointers.kartBody->kartPhysicsHolder->physics;
}

static u32 GetWheelCount(Kart::Player& kart) {
    const u32 count = kart.pointers.values->wheelCount1;
    return count > 4 ? 4 : count;
}

static void Save(u8 playerId) {
    Kart::Player& kart = *Kart::Manager::sInstance->players[playerId];
    const Kart::Physics& physics = GetPhysics(kart);
    sState.position = physics.position;
    sState.speed0 = physics.speed0;
    sState.mainRot = physics.mainRot;
    sState.engineSpeed = physics.engineSpeed;

    const Kart::Movement& movement = *kart.pointers.kartMovement;
    sState.movementEngineSpeed = movement.engineSpeed;
    const Kart::Boost& boost = movement.boost;
    sState.boostFrames[0] = boost.mtFrames;
    sState.boostFrames[1] = boost.starFrames;
    sState.boostFrames[2] = boost.mushroomBoostPanelFrames;
    sState.boostFrames[3] = boost.bulletFrames;
    sState.boostFrames[4] = boost.trickZipperFrames;
    sState.boostFrames[5] = boost.megaFrames;
    sState.boostTypes = boost.types;
    sState.boostMultiplier = boost.multiplier;
    sState.boostAcceleration = boost.acceleration;
    sState.boostUnknown_0x1c = boost.unknown_0x1c;
    sState.boostSpeedLimit = boost.speedLimit;

    const u32 wheelCount = GetWheelCount(kart);
    for(int i = 0; i < wheelCount; ++i) {
        const u8* wheelPhysics = reinterpret_cast<const u8*>(kart.pointers.wheels[i]->wheelPhysics);
        memcpy(sState.wheels[i].physics, wheelPhysics + 0x20, sizeof(WheelState));
    }

    const Item::PlayerInventory& inventory = Item::Manager::sInstance->players[playerId].inventory;
    sState.itemId = inventory.currentItemId;
    sState.itemCount = inventory.currentItemCount;
    sState.hasGolden = inventory.hasGolden;
    sState.goldenTimer = inventory.goldenTimer;

    const RaceinfoPlayer& progress = *Raceinfo::sInstance->players[playerId];
    sState.checkpoint = progress.checkpoint;
    sState.raceCompletion = progress.raceCompletion;
    sState.raceCompletionMax = progress.raceCompletionMax;
    sState.firstKcpLapCompletion = progress.firstKcpLapCompletion;
    sState.nextCheckpointLapCompletion = progress.nextCheckpointLapCompletion;
    sState.nextCheckpointLapCompletionMax = progress.nextCheckpointLapCompletionMax;
    sState.currentLap = progress.currentLap;
    sState.maxLap = progress.maxLap;
    sState.currentKCP = progress.currentKCP;
    sState.maxKCP = progress.maxKCP;

    sHasState = true;
}

static void Load(u8 playerId) {
    Kart::Player& kart = *Kart::Manager::sInstance->players[playerId];
    Kart::Physics& physics = GetPhysics(kart);
    physics.position = sState.position;
    physics.speed0 = sState.speed0;
    physics.mainRot = sState.mainRot;
    physics.engineSpeed = sState.engineSpeed;

    Kart::Movement& movement = *kart.pointers.kartMovement;
    movement.engineSpeed = sState.movementEngineSpeed;
    Kart::Boost& boost = movement.boost;
    boost.mtFrames = sState.boostFrames[0];
    boost.starFrames = sState.boostFrames[1];
    boost.mushroomBoostPanelFrames = sState.boostFrames[2];
    boost.bulletFrames = sState.boostFrames[3];
    boost.trickZipperFrames = sState.boostFrames[4];
    boost.megaFrames = sState.boostFrames[5];
    boost.types = sState.boostTypes;
    boost.multiplier = sState.boostMultiplier;
    boost.acceleration = sState.boostAcceleration;
    boost.unknown_0x1c = sState.boostUnknown_0x1c;
    boost.speedLimit = sState.boostSpeedLimit;

    const u32 wheelCount = GetWheelCount(kart);
    for(int i = 0; i < wheelCount; ++i) {
        u8* wheelPhysics = reinterpret_cast<u8*>(kart.pointers.wheels[i]->wheelPhysics);
        memcpy(wheelPhysics + 0x20, sState.wheels[i].physics, sizeof(WheelState));
    }

    Item::PlayerInventory& inventory = Item::Manager::sInstance->players[playerId].inventory;
    if(sState.itemCount == 0) inventory.ClearAll();
    else {
        inventory.SetItem(sState.itemId, false);
        inventory.currentItemCount = sState.itemCount;
    }
    inventory.hasGolden = sState.hasGolden;
    inventory.goldenTimer = sState.goldenTimer;

    RaceinfoPlayer& progress = *Raceinfo::sInstance->players[playerId];
    progress.checkpoint = sState.checkpoint;
    progress.raceCompletion = sState.raceCompletion;
    progress.raceCompletionMax = sState.raceCompletionMax;
    progress.firstKcpLapCompletion = sState.firstKcpLapCompletion;
    progress.nextCheckpointLapCompletion = sState.nextCheckpointLapCompletion;
    progress.nextCheckpointLapCompletionMax = sState.nextCheckpointLapCompletionMax;
    progress.currentLap = sState.currentLap;
    progress.maxLap = sState.maxLap;
    progress.currentKCP = sState.currentKCP;
    progress.maxKCP = sState.maxKCP;

    sWasUsed = true;
}

static bool IsButtonHeld() {
    const Input::RealControllerHolder* controllerHolder = SectionMgr::sInstance->pad.padInfos[0].controllerHolder;
    if(controllerHolder == nullptr || controllerHolder->curController == nullptr) return false;
    const u16 inputs = controllerHolder->inputStates[0].buttonRaw;
    switch(controllerHolder->curController->GetType()) {
        case WHEEL:
        case NUNCHUCK:
            return (inputs & WPAD::WPAD_BUTTON_MINUS) != 0;
        case CLASSIC:
            return (inputs & WPAD::WPAD_CL_BUTTON_MINUS) != 0;
        default:
            return (inputs & PAD::PAD_BUTTON_Y) != 0;
    }
}

static void PlayFeedback(SoundIDs soundId) {
    Pages::RaceHUD* hud = Pages::RaceHUD::sInstance;
    if(hud != nullptr) hud->PlaySound(soundId, -1);
}

static void Update() {
    u8 playerId;
    if(!GetPlayer(playerId)) {
        sFramesHeld = 0;
        return;
    }
    if(IsButtonHeld()) {
        if(sFramesHeld < framesToLoad) ++sFramesHeld;
        return;
    }
    if(sFramesHeld == 0) return;
    const bool load = sFramesHeld >= framesToLoad;
    sFramesHeld = 0;
    if(!load) {
        Save(playerId);
        PlayFeedback(SOUND_ID_SMALL_HIGH_NOTE);
    }
    else if(sHasState) {
        Load(playerId);
        PlayFeedback(SOUND_ID_BACK_PRESS);
    }
}
static RaceFrameHook UpdateSaveStates(Update);

}//namespace SaveStates
}//namespace Pulsar
