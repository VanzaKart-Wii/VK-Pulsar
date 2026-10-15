#include <UI/ExtendedTeamSelect/ExtendedTeamManager.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamSelect.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/System/random.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <Race/Players24/Players24.hpp>
#include <MarioKartWii/UI/Page/Other/FriendRoom.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>

extern "C" void OSReport(const char* format, ...);

namespace Pulsar {
namespace UI {

ExtendedTeamManager* ExtendedTeamManager::sInstance = nullptr;

static void ResetTeamPlayer(ExtendedTeamPlayer& player, u32 index) {
    player.playerIdx = 0xFF;
    player.miiIdx = 0xFF;
    player.aid = 0xFF;
    player.playerIdOnConsole = 0xFF;
    player.active = 0;
    player.done = 0;
    player.team = static_cast<ExtendedTeamID>(index % TEAM_COUNT);
}

ExtendedTeamManager::ExtendedTeamManager() {
    this->ResetPlayers();
    this->isHost = false;
    this->status = STATUS_NONE;
    this->hasFriendRoomStarted = false;
    this->waitingTimer = nullptr;
    this->lastUpdateTimer = nullptr;
}

void ExtendedTeamManager::CreateInstance(ExtendedTeamManager* obj) {
    sInstance = obj;
}

void ExtendedTeamManager::DestroyInstance() {
    if (sInstance) {
        if (sInstance->waitingTimer) {
            delete sInstance->waitingTimer;
            sInstance->waitingTimer = nullptr;
        }
        if (sInstance->lastUpdateTimer) {
            delete sInstance->lastUpdateTimer;
            sInstance->lastUpdateTimer = nullptr;
        }
        delete sInstance;
        sInstance = nullptr;
    }
}

void ExtendedTeamManager::SendStartRacePacket() {
    Pages::FriendRoomManager* friendRoomManager = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    RKNet::ControllerSub* sub = &controller->subs[controller->currentSub];

    if (sub->localAid == sub->hostAid) {
        friendRoomManager->lastMessageId++;

        RKNet::ROOMPacket packet;
        packet.messageType = MSG_TYPE_START_RACE;
        packet.message = 0;
        packet.unknown_0x3 = friendRoomManager->lastMessageId;

        for (int i = 0; i < 12; ++i) {
            if (i != sub->localAid) {
                RKNet::ROOMHandler::sInstance->toSendPackets[i] = packet;
            }
        }

        friendRoomManager->lastSentPacket = packet;
        friendRoomManager->localAid = sub->localAid;

        if (this->status == STATUS_SELECTING) {
            this->waitingTimer->SetInitial(2.5f);
            this->waitingTimer->isActive = true;

            this->status = STATUS_WAITING_POST;
        }
    }
}

void ExtendedTeamManager::SendUpdateTeamsPacket() {
    Pages::FriendRoomManager* friendRoomManager = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    RKNet::ControllerSub* sub = &controller->subs[controller->currentSub];

    if (sub->localAid == sub->hostAid) {
        friendRoomManager->lastMessageId++;

        RKNet::ROOMPacket packet;
        packet.messageType = MSG_TYPE_UPDATE_TEAMS;
        packet.message = 0;
        packet.unknown_0x3 = friendRoomManager->lastMessageId;

        for (int i = 0; i < 12; ++i) {
            if (i != sub->localAid) {
                RKNet::ROOMHandler::sInstance->toSendPackets[i] = packet;
            }
        }

        friendRoomManager->lastSentPacket = packet;
        friendRoomManager->localAid = sub->localAid;
    }
}

void ExtendedTeamManager::SendPingPacket() {
    Pages::FriendRoomManager* friendRoomManager = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    RKNet::ControllerSub* sub = &controller->subs[controller->currentSub];

    friendRoomManager->lastMessageId++;

    RKNet::ROOMPacket packet;
    packet.messageType = MSG_TYPE_PING;
    u8 team1 = this->GetPlayerTeamByAID(sub->localAid, 0);
    u8 team2 = this->GetPlayerTeamByAID(sub->localAid, 1);
    packet.message = (team1 & 0x0F) | ((team2 & 0x0F) << 4);
    packet.unknown_0x3 = friendRoomManager->lastMessageId;

    RKNet::ROOMHandler::sInstance->toSendPackets[sub->hostAid] = packet;

    friendRoomManager->lastSentPacket = packet;
    friendRoomManager->localAid = sub->localAid;
}

void ExtendedTeamManager::SendAckStartRacePacket() {
    Pages::FriendRoomManager* friendRoomManager = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    RKNet::ControllerSub* sub = &controller->subs[controller->currentSub];

    friendRoomManager->lastMessageId++;

    RKNet::ROOMPacket packet;
    packet.messageType = MSG_TYPE_ACK_START_RACE;
    packet.message = 0;
    packet.unknown_0x3 = friendRoomManager->lastMessageId;

    RKNet::ROOMHandler::sInstance->toSendPackets[sub->hostAid] = packet;

    friendRoomManager->lastSentPacket = packet;
    friendRoomManager->localAid = sub->localAid;
}

bool ExtendedTeamManager::AreAllOtherPlayersActive(u8 localAid) {
    for (int i = 0; i < 12; i++) {
        if (this->players[i].playerIdx != 0xFF && this->players[i].aid != localAid && !this->players[i].active) {
            return false;
        }
    }
    return true;
}

bool ExtendedTeamManager::AreAllOtherPlayersDone(u8 localAid) {
    for (int i = 0; i < 12; i++) {
        if (this->players[i].playerIdx != 0xFF && this->players[i].aid != localAid && !this->players[i].done) {
            return false;
        }
    }
    return true;
}

void ExtendedTeamManager::Update() {
    if (this->waitingTimer == nullptr || this->lastUpdateTimer == nullptr) {
        return;
    }

    Pages::FriendRoomManager* friendRoomManager = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();
    this->isHost = RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST;
    u8 localAid = RKNet::Controller::sInstance->subs[RKNet::Controller::sInstance->currentSub].localAid;
    if (!this->isHost) {
        if (this->status == STATUS_NONE) {
            if (this->hasFriendRoomStarted) {
                this->status = STATUS_SELECTING;
            }
        } else if (this->status == STATUS_SELECTING) {
            if (!this->lastUpdateTimer->isActive) {
                this->lastUpdateTimer->SetInitial(0.666f);
                this->lastUpdateTimer->isActive = true;

                this->SendPingPacket();
            }
        } else if (this->status == STATUS_DONE) {
            if (!this->lastUpdateTimer->isActive) {
                this->lastUpdateTimer->SetInitial(0.666f);
                this->lastUpdateTimer->isActive = true;

                this->SendAckStartRacePacket();
            }
        }
    } else {
        if (this->status == STATUS_NONE) {
            if (this->hasFriendRoomStarted) {
                this->status = STATUS_WAITING_PRE;
                this->waitingTimer->SetInitial(5.0f);
                this->waitingTimer->isActive = true;
            }
        } else if (this->status == STATUS_WAITING_PRE) {
            if (!this->waitingTimer->isActive) {
                this->status = STATUS_SELECTING;
            } else {
                if (this->AreAllOtherPlayersActive(localAid)) {
                    this->status = STATUS_SELECTING;
                    this->waitingTimer->isActive = false;
                }
            }
        } else if (this->status == STATUS_SELECTING) {
            // ...
        } else if (this->status == STATUS_WAITING_POST) {
            if (!this->waitingTimer->isActive) {
                this->status = STATUS_DONE;
            } else {
                if (this->AreAllOtherPlayersDone(localAid)) {
                    this->status = STATUS_DONE;
                    this->waitingTimer->isActive = false;
                }
            }

            if (!this->lastUpdateTimer->isActive) {
                this->lastUpdateTimer->SetInitial(0.666f);
                this->lastUpdateTimer->isActive = true;

                this->SendStartRacePacket();
            }
        }
    }

    this->lastUpdateTimer->Update();
    this->waitingTimer->Update();
}

void ExtendedTeamManager::VotePageSync() {
    ExtendedTeamPlayer newPlayers[MAX_PLAYERS];
    for (int i = 0; i < MAX_PLAYERS; i++) {
        ResetTeamPlayer(newPlayers[i], i);
    }

    Pages::SELECTStageMgr* voteMgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
    RKNet::Controller* controller = RKNet::Controller::sInstance;

    for (int i = 0; i < 12; i++) {
        u8 aid = controller->aidsBelongingToPlayerIds[i];
        if (aid == 0xFF) {
            continue;
        }

        u8 localPlayerId;
        if (i < 1) {
            localPlayerId = 0;
        } else {
            u8 aid2P = controller->aidsBelongingToPlayerIds[i - 1];
            if (aid != aid2P) {
                localPlayerId = 0;
            } else {
                localPlayerId = 1;
            }
        }

        for (int j = 0; j < 12; j++) {
            if (voteMgr->infos[j].aid == aid && voteMgr->infos[j].hudSlotid == localPlayerId) {
                newPlayers[i].playerIdx = i;
                newPlayers[i].miiIdx = aid * 2 + localPlayerId;
                newPlayers[i].aid = aid;
                newPlayers[i].playerIdOnConsole = localPlayerId;
                newPlayers[i].team = this->GetPlayerTeamByAID(aid, localPlayerId);
                break;
            }
        }
    }

    memcpy(this->players, newPlayers, sizeof(newPlayers));
}

bool ExtendedTeamManager::IsRedBlue24() {
    const RacedataSettings& settings = Racedata::sInstance->racesScenario.settings;
    const bool isVS = settings.gamemode == MODE_VS_RACE || settings.gamemode == MODE_PRIVATE_VS; //offline, 24 players rooms
    if (!isVS || (settings.modeFlags & TEAM_MODE_FLAG) == 0) return false;
    return Players24::IsReady() && Players24::HasExtraRacers();
}

/*
The end of an offline set of extended teams (from Retro Rewind) or of red/blue with 24 racers goes to the main menu.
The ceremony, VS (0x36) or teams (0x37, the team sections 0x24-0x27), crashed loading winningrun_demo with the
extended teams (805541e4: no racer loaded, then a null pointer), and red/blue would cast its 12 winners where it has
room for 6 (Racedata::InitAwards). Every way to the ceremony goes through SetNextSection, but by then InitAwards has
made the menu scenario the ceremony's (gamemode 11, "mode=11" in UpdateContext's log): the choice is made at the start
of every race instead.
*/
static bool skipsAward = false;

void ExtendedTeamManager::DecideAwardSkip() {
    skipsAward = IsOfflineActivated() || IsRedBlue24();
}

SectionId ExtendedTeamManager::SkipTeamAward(SectionId nextSectionId) {
    if (nextSectionId != SECTION_VS_RACE_AWARD && nextSectionId != SECTION_AWARD_37) return nextSectionId;
    if (!skipsAward) return nextSectionId;
    return SECTION_MAIN_MENU_FROM_MENU;
}

u32 ExtendedTeamManager::GetRacerCount() {
    if (Players24::IsReady() && Players24::GetRacePlayerCount() > 0) return Players24::GetRacePlayerCount();
    return Racedata::sInstance->racesScenario.playerCount;
}

bool ExtendedTeamManager::HasUIFile(const char* path) {
    const ArchiveMgr* archives = ArchiveMgr::sInstance;
    const bool found = archives != nullptr && archives->GetFile(ARCHIVE_HOLDER_UI, path) != nullptr;
    if (!found) OSReport("[VK 24P] %s missing from the UI archives: 6 rows per team\n", path);
    return found;
}

bool ExtendedTeamManager::IsOfflineActivated() {
    const RacedataSettings& settings = Racedata::sInstance->menusScenario.settings;
    return settings.gamemode == MODE_VS_RACE && (settings.modeFlags & TEAM_MODE_FLAG)
        && Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_EXTENDEDTEAMS, RADIO_EXTENDEDTEAMSENABLED) == EXTENDEDTEAMS_ENABLED;
}

/*
Retro Rewind's offline extended teams (rr-pulsar, ExtendedTeamManager::ConfigureOfflineTeams), called after every
Racedata::InitRace of an offline team VS. The racers are shuffled into the number of teams of the setting (24 racers
make 12/8/6/4 per team, 12 racers 6/4/3/2) on the first race of a set, and keep their teams for the races after it;
the team colours are shuffled on the first race too. Retro Rewind shuffles an order it never uses (the teams follow
the player ids); here the order decides the teams.
*/
void ExtendedTeamManager::ConfigureOfflineTeams() {
    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    const u32 playerCount = scenario.playerCount > MAX_PLAYERS ? MAX_PLAYERS : scenario.playerCount;
    if (scenario.settings.raceNumber != 0) {
        bool isConfigured = true; //the same racers as the teams were made for (a set restarted from the menu starts at 0)
        for (u32 id = 0; id < playerCount; ++id) {
            if (this->players[id].playerIdx != id) isConfigured = false;
        }
        if (isConfigured) return;
    }

    u32 teamCount = 2;
    switch (Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_EXTENDEDTEAMS, SCROLLER_EXTENDEDTEAMSCOUNT)) {
        case EXTENDEDTEAMS_COUNT_3: teamCount = 3; break;
        case EXTENDEDTEAMS_COUNT_4: teamCount = 4; break;
        case EXTENDEDTEAMS_COUNT_6: teamCount = 6; break;
    }
    if (teamCount > playerCount) teamCount = playerCount;

    this->ResetPlayers();

    u8 order[MAX_PLAYERS];
    for (u32 i = 0; i < playerCount; ++i) order[i] = i;
    Random random;
    for (u32 i = playerCount; i > 1; --i) {
        const u32 other = random.NextLimited(i);
        const u8 id = order[i - 1];
        order[i - 1] = order[other];
        order[other] = id;
    }

    for (u32 slot = 0; slot < playerCount; ++slot) {
        const u8 id = order[slot];
        this->SetPlayerIndexes(id, id, 0xFF, 0xFF);
        this->SetPlayerTeam(id, static_cast<ExtendedTeamID>(slot * teamCount / playerCount));
    }

    ExtendedTeamSelect::RandomizeTeamColors();
}

void ExtendedTeamManager::Reset() {
    this->status = STATUS_NONE;
    this->isHost = false;
    this->hasFriendRoomStarted = false;

    if (this->waitingTimer == nullptr) {
        this->waitingTimer = new (Pulsar::System::sInstance->heap, 0x4) CountDown();
    }
    if (this->lastUpdateTimer == nullptr) {
        this->lastUpdateTimer = new (Pulsar::System::sInstance->heap, 0x4) CountDown();
    }

    this->lastUpdateTimer->isActive = false;
    this->waitingTimer->isActive = false;
    ExtendedTeamSelect::ResetTeamColors();
}

void ExtendedTeamManager::ResetPlayers() {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        ResetTeamPlayer(this->players[i], i);
    }
}

}  // namespace UI
}  // namespace Pulsar
