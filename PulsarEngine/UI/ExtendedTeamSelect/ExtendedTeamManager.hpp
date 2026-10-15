#ifndef __EXTENDED_TEAMSELECT_HPP__
#define __EXTENDED_TEAMSELECT_HPP__

#include <UI/UI.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/UI/Ctrl/CountDown.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/ROOM.hpp>

namespace Pulsar {
namespace UI {

enum ExtendedTeamID {
    TEAM_RED,
    TEAM_ORANGE,
    TEAM_YELLOW,
    TEAM_GREEN,
    TEAM_BLUE,
    TEAM_PURPLE,
    TEAM_COUNT
};
struct ExtendedTeamPlayer {
    u32 playerIdx;
    u32 miiIdx;
    u32 aid;
    u32 playerIdOnConsole;
    u32 active;
    u32 done;
    ExtendedTeamID team;
};

class ExtendedTeamManager {
   public:
    //Vanilla team VS (racedata modeFlags). Offline the extended teams replace red/blue when the "Ext. Team" setting is on
    static const u32 TEAM_MODE_FLAG = 0x2;
    //Every racer of an offline VS with 24 players (Race/Players24); online rooms keep 12
    static const u32 MAX_PLAYERS = 24;

    enum ExtendedROOMMessageType {
        MSG_TYPE_START_RACE = 0x81,
        MSG_TYPE_UPDATE_TEAMS = 0x82,
        MSG_TYPE_PING = 0x83,
        MSG_TYPE_ACK_START_RACE = 0x84
    };

    enum Status {
        STATUS_NONE,
        STATUS_WAITING_PRE,
        STATUS_SELECTING,
        STATUS_WAITING_POST,
        STATUS_DONE
    };

    static ExtendedTeamManager* sInstance;
    static void CreateInstance(ExtendedTeamManager* obj);
    static void DestroyInstance();

    ExtendedTeamManager();

    void SetPlayerTeam(u32 idx, ExtendedTeamID team) {
        this->players[idx].team = team;
    }

    void SetPlayerIndexes(u32 idx, u32 miiIdx, u32 aid, u32 playerIdOnConsole) {
        this->players[idx].playerIdx = idx;
        this->players[idx].miiIdx = miiIdx;
        this->players[idx].aid = aid;
        this->players[idx].playerIdOnConsole = playerIdOnConsole;
    }

    ExtendedTeamID GetPlayerTeamDeprecated(u32 idx) {
        return this->players[idx].team;
    }

    ExtendedTeamID GetPlayerTeam(u32 idx) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (this->players[i].playerIdx == idx) {
                return this->players[i].team;
            }
        }
        return TEAM_COUNT;
    }

    ExtendedTeamID GetPlayerTeamByAID(u8 aid, u8 playerIdOnConsole) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (this->players[i].aid == aid && this->players[i].playerIdOnConsole == playerIdOnConsole) {
                return this->players[i].team;
            }
        }
        return TEAM_COUNT;
    }

    void SetPlayerTeamByAID(u8 aid, u8 playerIdOnConsole, ExtendedTeamID team) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (this->players[i].aid == aid && this->players[i].playerIdOnConsole == playerIdOnConsole) {
                this->players[i].team = team;
                break;
            }
        }
    }

    u8 GetPlayerAID(u32 idx, u8 playerIdOnConsole) {
        return this->players[idx].aid;
    }

    const ExtendedTeamPlayer* GetPlayerInfo() const {
        return this->players;
    }

    void SendStartRacePacket();  // Host
    void SendUpdateTeamsPacket();  // Host
    void SendPingPacket();  // Non-Host
    void SendAckStartRacePacket();  // Non-Host

    void Update();
    void Reset();
    void ResetPlayers();

    void VotePageSync();
    void ConfigureOfflineTeams();

    Status GetStatus() {
        return this->status;
    }

    bool IsInactiveStatus() {
        return this->status == STATUS_NONE;
    }

    bool IsWaitingStatus() {
        return this->status == STATUS_WAITING_PRE || this->status == STATUS_WAITING_POST;
    }

    bool IsSelectingStatus() {
        return this->status == STATUS_SELECTING;
    }

    bool IsDoneStatus() {
        return this->status == STATUS_DONE;
    }

    void SetActiveStatusForAID(u8 aid) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (this->players[i].aid == aid) {
                this->players[i].active = 1;
            }
        }
    }

    void SetDoneStatusForAID(u8 aid) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (this->players[i].aid == aid) {
                this->players[i].done = 1;
            }
        }
    }

    void SetStatusExternal(Status status) {
        this->status = status;
    }

    //Offline VS with "Teams" chosen in the rules and the "Ext. Team" setting on (Retro Rewind's offline extended teams)
    static bool IsOfflineActivated();
    //Red/blue VS with more than 12 racers (Race/Players24): its team pages need 12 rows per team
    static bool IsRedBlue24();
    //Racers of the current race: the race scenario says 12 on the results pages of a race with 24 (Race/Players24)
    static u32 GetRacerCount();
    //SectionMgr::SetNextSection (Network/FriendRoomRegional.cpp): the main menu instead of a ceremony these teams can't have
    static SectionId SkipTeamAward(SectionId nextSectionId);
    //Racedata::InitRace of a race: whether the set will skip its ceremony, decided while the menu still says VS
    static void DecideAwardSkip();
    //A file of the loaded UI archives (Race.szs in a race, MenuSingle.szs in the menu): the pack's 24 copies
    static bool HasUIFile(const char* path);

    static bool IsActivated() {
        const RacedataSettings& settings = Racedata::sInstance->menusScenario.settings;
        if (settings.gamemode == MODE_VS_RACE) return IsOfflineActivated();
        RKNet::RoomType roomType = RKNet::Controller::sInstance->roomType;
        return System::sInstance->IsContext(PULSAR_EXTENDEDTEAMS) && (settings.gamemode == MODE_PRIVATE_VS || settings.gamemode == MODE_PRIVATE_BATTLE) && (roomType == RKNet::ROOMTYPE_FROOM_HOST || roomType == RKNet::ROOMTYPE_FROOM_NONHOST);
    }

    //Online room pages: the local player's own team, by aid there (offline every player has its id)
    static bool IsOnlineRoom() {
        const RKNet::Controller* controller = RKNet::Controller::sInstance;
        return controller != nullptr && (controller->roomType == RKNet::ROOMTYPE_FROOM_HOST || controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST);
    }

   private:
    bool AreAllOtherPlayersActive(u8 localAid);
    bool AreAllOtherPlayersDone(u8 localAid);

    ExtendedTeamPlayer players[MAX_PLAYERS];

    bool isHost;

    Status status;

   public:
    CountDown* waitingTimer;
    CountDown* lastUpdateTimer;

    u32 hasFriendRoomStarted;
};

}  // namespace UI
}  // namespace Pulsar

#endif