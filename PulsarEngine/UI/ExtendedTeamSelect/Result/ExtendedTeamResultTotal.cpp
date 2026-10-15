#include <UI/ExtendedTeamSelect/Result/ExtendedTeamResultTotal.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>

namespace Pulsar {
namespace UI {

PageId ExtendedTeamResultTotal::GetNextPage() const {
    SectionId currentId = SectionMgr::sInstance->curSection->sectionId;
    if ((currentId != SECTION_P1_WIFI_FRIEND_VS) && (currentId != SECTION_P1_WIFI_FRIEND_BALLOON) && (currentId != SECTION_P1_WIFI_FRIEND_COIN) && (currentId != SECTION_P1_WIFI_FRIEND_TEAMVS)) {
        if (currentId != SECTION_GP) {
            return PAGE_VS_RACEENDMENU;
        }
        return PAGE_GP_ENDMENU;
    }

    return PAGE_WIFI_VS_RESULTS;
}

void ExtendedTeamResultTotal::OnInit() {
    RacedataScenario& scenario = Racedata::sInstance->racesScenario;

    //Red/blue past 12 racers: the two stock columns with 12 rows each (TeamVSLeaderboardTotal can only show 6)
    if (ExtendedTeamManager::IsRedBlue24()) {
        this->resultCount = 2;
        this->InitControlGroup(this->resultCount);
        for (int i = 0; i < this->resultCount; i++) {
            this->AddControl(i, this->results[i], 0);
            this->results[i].LoadWide(i, static_cast<ExtendedTeamID>(i), true);
        }
        return;
    }

    int teamCount = 0;
    bool teamPresent[TEAM_COUNT];
    for (int i = 0; i < TEAM_COUNT; i++) {
        teamPresent[i] = false;
    }

    const u32 racerCount = ExtendedTeamManager::GetRacerCount();
    for (int i = 0; i < racerCount; i++) {
        ExtendedTeamID team = ExtendedTeamManager::sInstance->GetPlayerTeam(i);
        if (!teamPresent[team]) {
            teamCount++;
            teamPresent[team] = true;
        }
    }

    int controlLoadedCount = 0;
    this->InitControlGroup(teamCount);
    for (int i = 0; i < TEAM_COUNT; i++) {
        if (!teamPresent[i])
            continue;

        this->AddControl(controlLoadedCount, this->results[controlLoadedCount], 0);
        //Two teams of up to 12 (ExpGPVSLeaderboardUpdate::GetNextPage): red/blue's 24 columns, recoloured
        if (teamCount == 2 && racerCount > 12) results[controlLoadedCount].LoadWide(controlLoadedCount, (ExtendedTeamID)i, false);
        else results[controlLoadedCount].Load((ExtendedTeamID)i, teamCount, controlLoadedCount);

        controlLoadedCount++;
    }
    this->resultCount = controlLoadedCount;
}

bool ExtendedTeamResultTotal::CanEnd() {
    for (int i = 0; i < this->resultCount; i++) {
        if (this->results[i].IsResultAnimDone()) {
            return true;
        }
    }

    return false;
}

void ExtendedTeamResultTotal::FillRows() {
}

}  // namespace UI
}  // namespace Pulsar