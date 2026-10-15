#include <UI/ExtendedTeamSelect/ExtendedTeamManager.hpp>
#include <MarioKartWii/UI/Page/Menu/VSTeamsView.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <Race/Players24/Players24.hpp>

namespace Pulsar {
namespace UI {

/*
The red/blue team page of the VS menu (VSTeamsView, shown after the character select) has a CtrlMenuTeamConfirmTeam
per team with 6 player boxes, and its InitSelf (807e9f5c) fills one box per racer of the team without stopping at 6:
with 24 racers (12 per team) it wrote past the control, over the other team and the page. This is that control
again with 12 boxes, from the pack's TeamConfirmNULL_24 / TeamConfirmBlueRed_24 (BlueRed0-11, two columns of 6)
when the "VS Players" setting is on at 24, and the stock files with 6 otherwise. Both stop at their boxes.

VSTeamsView::CreateControl (8084fe40) is replaced: it added teams[id] and loaded it; this control is made on the
section's heap instead, like the other controls Pulsar adds, and the page's own two stay empty.
*/
class CtrlMenuTeamConfirmTeam24 : public LayoutUIControl {
   public:
    static const u32 MAX_ELEMENTS = 12;

    void InitSelf() override;
    const char* GetClassName() const override { return "CtrlMenuTeamConfirmTeam"; }
    void Load(Team team);

    Team team;
    u32 elementCount;
    LayoutUIControl playerElements[MAX_ELEMENTS];
};

//Decided when the menu section is made: the racers are only added once VS is chosen
static bool UsesConfirm24() {
    return Players24::IsReady() && Players24::IsEnabled()
        && ExtendedTeamManager::HasUIFile("control/ctrl/TeamConfirmBlueRed_24.brctr")
        && ExtendedTeamManager::HasUIFile("control/ctrl/TeamConfirmNULL_24.brctr");
}

//CtrlMenuTeamConfirmTeam::Load (807e9dc0)
void CtrlMenuTeamConfirmTeam24::Load(Team team) {
    const bool is24 = UsesConfirm24();
    this->team = team;
    this->elementCount = is24 ? MAX_ELEMENTS : 6;

    char variant[20];
    snprintf(variant, 20, "TeamConfirmNULL%d", team);
    ControlLoader selfLoader(this);
    selfLoader.Load("control", is24 ? "TeamConfirmNULL_24" : "TeamConfirmNULL", variant, nullptr);

    this->InitControlGroup(this->elementCount);
    const char* anims[] = {
        "Loop", "Loop", nullptr,
        "Select", "SelectOn", "SelectOff", nullptr,
        "Select2", "Select2On", "Select2Off", nullptr,
        nullptr};
    for (u32 i = 0; i < this->elementCount; ++i) {
        LayoutUIControl& element = this->playerElements[i];
        snprintf(variant, 20, "BlueRed%d", i);
        this->AddControl(i, &element);
        ControlLoader loader(&element);
        loader.Load("control", is24 ? "TeamConfirmBlueRed_24" : "TeamConfirmBlueRed", variant, anims);
        if (team == TEAM_RED) element.SetPaneVisibility("blue_null", false);
        else if (team == TEAM_BLUE) element.SetPaneVisibility("red_null", false);
    }
}

//CtrlMenuTeamConfirmTeam::InitSelf (807e9f5c), filling only as many boxes as there are
void CtrlMenuTeamConfirmTeam24::InitSelf() {
    for (u32 i = 0; i < this->elementCount; ++i) {
        LayoutUIControl& element = this->playerElements[i];
        element.isHidden = true;
        for (u32 group = 0; group < 3; ++group) element.animator.GetAnimationGroupById(group).PlayAnimationAtFrame(0, 0.0f);
    }
    if (this->parentGroup->parentPage->currentState <= STATE_DEACTIVATED) return;

    const RacedataScenario& menu = Racedata::sInstance->menusScenario;
    MiiGroup& miis = SectionMgr::sInstance->sectionParams->playerMiis;
    const u8 localCount = GetLocalPlayerCount();
    const u8 idCount = Players24::IsReady() ? Players24::maxPlayers : Players24::vanillaPlayers;
    u32 slot = 0;
    for (u8 id = 0; id < idCount && slot < this->elementCount; ++id) {
        const RacedataPlayer& player = Players24::GetPlayer(menu, id);
        if (player.playerType == PLAYER_NONE || player.team != this->team) continue;
        LayoutUIControl& element = this->playerElements[slot++];

        const CharacterId character = player.characterId;
        if (static_cast<u32>(character - MII_S_A_MALE) <= 0x14) { //Mii::IsMiiId
            element.SetMiiPane("chara_icon", miis, id, 2);
            element.SetMiiPane("chara_icon_sha", miis, id, 2);
            Text::Info info;
            info.miis[0] = miis.GetMii(id);
            element.SetTextBoxMessage("mii_name", BMG_MII_NAME, &info);
        } else {
            element.SetTextBoxMessage("mii_name", GetCharacterBMGId(character, true), nullptr);
            const char* icon = GetCharacterIconPaneName(character);
            element.SetPicturePane("chara_icon", icon);
            element.SetPicturePane("chara_icon_sha", icon);
        }
        if (SectionMgr::sInstance->curSection->sectionId == SECTION_SINGLE_P_FROM_MENU && id == 0) {
            Text::Info info;
            info.miis[0] = miis.GetMii(id);
            element.SetTextBoxMessage("mii_name", BMG_MII_NAME, &info);
        }

        element.isHidden = false;
        const u32 otherFrame = id >= localCount ? 1 : 0; //local players light up
        element.animator.GetAnimationGroupById(0).PlayAnimationAtFrame(0, 0.0f);
        element.animator.GetAnimationGroupById(1).PlayAnimationAtFrame(otherFrame, 0.0f);
        element.animator.GetAnimationGroupById(2).PlayAnimationAtFrame(otherFrame, 0.0f);
        element.SetPaneVisibility("Null_00", false);
    }
}

//VSTeamsView::CreateControl (8084fe40)
static UIControl* VSTeamsView_CreateControl(Pages::VSTeamsView& page, u32 id) {
    CtrlMenuTeamConfirmTeam24* control = new CtrlMenuTeamConfirmTeam24;
    page.AddControl(page.controlCount++, *control, 0);
    control->Load(static_cast<Team>(id));
    return control;
}
kmBranch(0x8084fe40, VSTeamsView_CreateControl);

}  // namespace UI
}  // namespace Pulsar
