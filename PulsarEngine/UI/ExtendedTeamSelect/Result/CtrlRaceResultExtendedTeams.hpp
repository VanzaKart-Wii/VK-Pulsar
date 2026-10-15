#ifndef __EXTENDEDTEAMS_CTRL_RESULT_HPP
#define __EXTENDEDTEAMS_CTRL_RESULT_HPP

#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceResultTeam.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamSelect.hpp>

namespace Pulsar {
namespace UI {

class CtrlRaceResultExtendedTeams : public LayoutUIControl {
   public:
    //12 rows for two teams past 12 racers (the pack's ResultVSTeam_24, BlueRed0-11), 6 otherwise
    static const u32 MAX_ITEMS = 12;

    void InitSelf() override;
    void OnUpdate() override;
    const char* GetClassName() const override;

    void Load(ExtendedTeamID teamID, int numTeams, int teamIdx);
    //Two teams past 12 racers (Race/Players24): a column of red/blue's 24 layout with 12 rows; for red/blue teamID is
    //the Team (red 0, blue 1)
    void LoadWide(u32 column, ExtendedTeamID teamID, bool isRedBlue);
    static bool HasWideLayouts();
    bool IsResultAnimDone() const;

    ExtendedTeamID teamId;
    CtrlRaceResultTeam::Player players[MAX_ITEMS];

    LayoutUIControl items[MAX_ITEMS];
    LayoutUIControl* resultTeamPoint;

    Text::Info textInfo;
    float currentScore;
    u32 teamScore;
    u32 itemCount; //rows loaded
    bool isRedBlue;

   private:
    bool IsMember(u8 playerId) const;
    void LoadTeamPoint(const char* brctr);
    void LoadItem(u32 idx, const char* brctr, bool recolor);
};

}  // namespace UI
}  // namespace Pulsar

#endif
