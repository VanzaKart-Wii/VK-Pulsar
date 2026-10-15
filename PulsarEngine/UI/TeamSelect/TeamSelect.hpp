#ifndef _TEAMSELECT_
#define _TEAMSELECT_
#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Page/Menu/MiiSelect.hpp>
#include <UI/UI.hpp>
#include <UI/ToggleControls.hpp>


//Custom Page that allows the host (and only the host) to arrange teams as they see fit. This also has a ToggleButton to disable it if wanted
namespace Pulsar {
namespace UI {


class TeamSelect : public Pages::MenuInteractable {
public:
    static const PulPageId id = PULPAGE_TEAMSELECT;
    TeamSelect();
    void OnInit() override;
    void BeforeEntranceAnimations() override;
    void BeforeControlUpdate() override;
    int GetActivePlayerBitfield() const override;
    int GetPlayerBitfield() const override;
    ManipulatorManager& GetManipulatorManager() override;; //offset 0x70
    UIControl* CreateExternalControl(u32 id) override; //0x84
    UIControl* CreateControl(u32 id) override; //0x88
    void SetButtonHandlers(PushButton& button) override; //80853aac 0x8C
    static inline Team GetPlayerTeam(u8 teamsArrayIdx) { return static_cast<Team>(teams[teamsArrayIdx]); }
    //The players the page shows, in order: their index in teams[] and in the room's Mii group (also SetTeams)
    static u32 GetEntries(u8* teamsArrayIdxs, u8* miiIdxs, u32 maxCount);

private:
    void OnArrowClick(PushButton& button, u32 hudSlotId);
    void OnArrowSelect(PushButton& button, u32 hudSlotId);
    void OnArrowDeselect(PushButton& button, u32 hudSlotId) {};
    void OnButtonClick(PushButton& button, u32 hudSlotId);
    void OnButtonSelect(PushButton& button, u32 hudSlotId) {};
    void OnButtonDeselect(PushButton& button, u32 hudSlotId) {};
    void OnBackPress(u32 hudSlotId);
    void OnToggleButtonClick(ToggleButton& button, u32);
    void SetColours(u32 idx, u8 team);
    static u8 CalcTeamsArrayIdx(u32 teamsArrayIdx);
    static u32 CalcIdx(u8 teamsArrayIdx);
    void PlaceRow24(LayoutUIControl& control, u32 idx);
    static void RotateArrow(PushButton& button, u8 team);
    static void RotateArrowPane(PushButton& button, const char* pane, u8 team);

    PtmfHolder_2A<TeamSelect, void, PushButton&, u32> onArrowClickHandler;
    PtmfHolder_2A<TeamSelect, void, PushButton&, u32> onArrowSelectHandler;
    PtmfHolder_2A<TeamSelect, void, PushButton&, u32> onArrowDeselectHandler;
    PtmfHolder_2A<TeamSelect, void, ToggleButton&, u32> onToggleButtonClick;
    //12 rows, 24 when the host can open a 24 players room (Race/Players24): one per console there, teams[aid]
    static const u32 maxRows = 24;
    u32 rowCount;
    PushButton arrows[maxRows];
    LayoutUIControl miis[maxRows];
    MiiName name;
    MiiGroup* miiGroup; //take friendroom's
    u8 arrowMiiIdx[maxRows];
    ToggleButton toggle;
    static const char* miiBg;
    static const char* border;
    static u8 teams[24];

public:
    static bool isEnabled;



};
}//namespace UI
}//namespace Pulsar
#endif