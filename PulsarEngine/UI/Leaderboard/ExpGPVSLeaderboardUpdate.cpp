#include <UI/Leaderboard/ExpGPVSLeaderboardUpdate.hpp>
#include <UI/Leaderboard/LeaderboardDisplay.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamManager.hpp>
#include <UI/ExtendedTeamSelect/Result/CtrlRaceResultExtendedTeams.hpp>
#include <Settings/Settings.hpp>

namespace Pulsar {
namespace UI {

void ExpGPVSLeaderboardUpdate::OnUpdate() {
    if (checkLeaderboardDisplaySwapInputs()) {
        nextLeaderboardDisplayType();
        fillLeaderboardResults(GetRowCount(), this->results);
    }
}

void ExpGPVSLeaderboardUpdate::BeforeEntranceAnimations() {
    setLeaderboardDisplayType(LEADERBOARD_DISPLAY_NAMES);

    if (System::sInstance->IsContext(PULSAR_MODE_OTT)) {
        setLeaderboardDisplayType(LEADERBOARD_DISPLAY_TIMES);
    }

    fillLeaderboardResults(GetRowCount(), this->results);
}

static const int ALLOWED_PLAYER_PER_TEAM_COUNT[TEAM_COUNT + 1] = {0, 0, 6, 4, 3, 2, 2};

PageId ExpGPVSLeaderboardUpdate::GetNextPage() const {
    PageId nextPageId = Pages::GPVSLeaderboardUpdate::GetNextPage();
    if (nextPageId == PAGE_TEAMVS_TOTAL_LEADERBOARDS && ExtendedTeamManager::IsRedBlue24()) {
        return static_cast<PageId>(PULPAGE_EXTENDEDTEAMS_RESULT_TOTAL);
    }
    if (ExtendedTeamManager::IsActivated() && nextPageId == PAGE_GPVS_TOTAL_LEADERBOARDS) {
        //The real racer count: past 12 the race scenario says 12 on the results pages (Race/Players24)
        const u32 racerCount = ExtendedTeamManager::GetRacerCount();
        int teamCount = 0;
        int numPlayerPerTeam[TEAM_COUNT] = {0};

        for (int i = 0; i < racerCount; i++) {
            ExtendedTeamID team = ExtendedTeamManager::sInstance->GetPlayerTeam(i);
            numPlayerPerTeam[team]++;
        }

        for (int i = 0; i < TEAM_COUNT; i++) {
            if (numPlayerPerTeam[i] > 0) {
                teamCount++;
            }
        }

        //Two teams past 12 racers get red/blue's 24 columns (CtrlRaceResultExtendedTeams::LoadWide), 12 rows each
        const int allowed = teamCount == 2 && racerCount > 12 && CtrlRaceResultExtendedTeams::HasWideLayouts()
            ? CtrlRaceResultExtendedTeams::MAX_ITEMS : ALLOWED_PLAYER_PER_TEAM_COUNT[teamCount];
        for (int i = 0; i < TEAM_COUNT; i++) {
            if (numPlayerPerTeam[i] > allowed) {
                return static_cast<PageId>(PULPAGE_EXTENDEDTEAMS_RESULT_TOTAL_IRREGULAR);
            }
        }

        return static_cast<PageId>(PULPAGE_EXTENDEDTEAMS_RESULT_TOTAL);
    }

    return nextPageId;
}

}  // namespace UI
}  // namespace Pulsar
