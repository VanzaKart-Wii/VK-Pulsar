#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/SELECT.hpp>
#include <MarioKartWii/UI/Page/Other/FriendRoom.hpp>
#include <UI/TeamSelect/TeamSelect.hpp>
#include <UI/UI.hpp>
#include <Race/Players24/Online24.hpp>
#include <Network/PacketExpansion.hpp>


namespace Pulsar {
namespace UI {

u8 TeamSelect::teams[24] = { 0 };
bool TeamSelect::isEnabled = false;
const char* TeamSelect::border = "border";
const char* TeamSelect::miiBg = "bg";

TeamSelect::TeamSelect() {
    //Decided when the room's section is made, before the room exists: 24 rows if the host can open one of 24
    rowCount = Players24::CanHostRoom24() ? maxRows : 12;
    hasBackButton = true;
    externControlCount = 0;
    internControlCount = rowCount * 2 + 2;
    extraControlNumber = 0;
    controlSources = 2;
    titleBmg = BMG_TEAM_SELECT;
    nextPageId = PAGE_NONE;
    prevPageId = PAGE_FRIEND_ROOM;
    nextSection = SECTION_NONE;
    movieStartFrame = -1;
    isLocked = false;
    activePlayerBitfield = 1;

    onArrowClickHandler.subject = this;
    onArrowClickHandler.ptmf = &TeamSelect::OnArrowClick;
    onArrowSelectHandler.subject = this;
    onArrowSelectHandler.ptmf = &TeamSelect::OnArrowSelect;
    onArrowDeselectHandler.subject = this;
    onArrowDeselectHandler.ptmf = &TeamSelect::OnArrowDeselect;
    onButtonClickHandler.subject = this;
    onButtonClickHandler.ptmf = &TeamSelect::OnButtonClick;
    onButtonSelectHandler.subject = this;
    onButtonSelectHandler.ptmf = &TeamSelect::OnButtonSelect;
    onButtonDeselectHandler.subject = this;
    onButtonDeselectHandler.ptmf = &TeamSelect::OnButtonDeselect;
    onBackPressHandler.subject = this;
    onBackPressHandler.ptmf = &TeamSelect::OnBackPress;
    onToggleButtonClick.subject = this;
    onToggleButtonClick.ptmf = &TeamSelect::OnToggleButtonClick;


    this->controlsManipulatorManager.Init(1, false);
    this->SetManipulatorManager(controlsManipulatorManager);
    this->controlsManipulatorManager.SetGlobalHandler(START_PRESS, onStartPressHandler, false, false);
    this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, onBackPressHandler, false, false);
    activePlayerBitfield = 1;

}


void TeamSelect::OnInit() {
    this->miiGroup = &SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>()->miiGroup;
    Menu::OnInit();
    /*
    for(u8 aid = 0; aid < 12; aid++) {
        u32 idx = this->CalcIdx(aid);
        if(idx != 0xFF && this->teams[aid] == 1) {
            this->RotateArrow(this->arrows[idx], 1);
        }

    }
    */
}


void TeamSelect::BeforeEntranceAnimations() {
    Pages::Menu::BeforeEntranceAnimations();
    if(this->toggle.GetState() != this->isEnabled) this->toggle.ToggleState(this->isEnabled);
    this->toggle.SelectInitial(0);
    const u32 bmgId = this->toggle.GetState() == false ? BMG_TEAMS_DISABLED : BMG_TEAMS_ENABLED;
    this->toggle.SetMessage(bmgId);
    this->isLocked = false;

    for(int idx = 0; idx < this->rowCount; ++idx) {
        this->miis[idx].animator.GetAnimationGroupById(0).PlayAnimationAtFrameAndDisable(0, 0.0f);
    }
    for(u8 teamsArrayIdx = 0; teamsArrayIdx < 24; ++teamsArrayIdx) {
        u32 idx = this->CalcIdx(teamsArrayIdx);
        u8 curTeam = 0;
        if(idx != 0xFF) {
            u8 curTeam = this->teams[teamsArrayIdx];
            if(curTeam == 1) this->RotateArrow(this->arrows[idx], 1);
            this->SetColours(idx, curTeam);
            this->miis[idx].animator.GetAnimationGroupById(0).PlayAnimationAtFrameAndDisable(!curTeam, 0.0f);
        }
    }

}

/*
The players shown, console by console: in a 12 players room every player of aids 0-11, teams[aid + 12 * slot] and Mii
aid * 2 + slot; in a 24 players room (one player per console) every console of aids 0-23, teams[aid] and the Mii of
its bubble (Race/Players24/Online24Room.cpp). SetTeams reads teams[] the same way.
*/
u32 TeamSelect::GetEntries(u8* teamsArrayIdxs, u8* miiIdxs, u32 maxCount) {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub* sub = &controller->subs[0];
    if(sub->connectionUserDatas[0].playersAtConsole == 0) sub = &controller->subs[1];

    u32 count = 0;
    if(Players24::IsHostingRoom24()) {
        for(u8 aid = 0; aid < maxRows && count < maxCount; ++aid) {
            if(Players24::PlayersAtAid(aid) == 0) continue;
            teamsArrayIdxs[count] = aid;
            miiIdxs[count] = Players24::RoomMiiOfAid(aid);
            ++count;
        }
        return count;
    }
    for(u8 aid = 0; aid < 12; ++aid) {
        if(sub->availableAids & (1 << aid)) {
            for(u8 player = 0; player < sub->connectionUserDatas[aid].playersAtConsole && count < maxCount; ++player) {
                teamsArrayIdxs[count] = aid + 12 * player;
                miiIdxs[count] = aid * 2 + player;
                ++count;
            }
        }
    }
    return count;
}

void TeamSelect::BeforeControlUpdate() {
    u8 teamsArrayIdxs[maxRows];
    u8 miiIdxs[maxRows];
    const u32 count = GetEntries(teamsArrayIdxs, miiIdxs, this->rowCount);

    int idx = 0;
    for(; idx < count; ++idx) {
        const u8 team = this->teams[teamsArrayIdxs[idx]];
        LayoutUIControl& mii = this->miis[idx];
        const bool hasMii = this->miiGroup->GetMii(miiIdxs[idx]) != nullptr;
        if(hasMii) {
            mii.SetMiiPane("chara", *this->miiGroup, miiIdxs[idx], 2);
            mii.SetMiiPane("chara_shadow", *this->miiGroup, miiIdxs[idx], 2);
        }
        mii.isHidden = !hasMii;
        mii.animator.GetAnimationGroupById(0).PlayAnimationAtFrameAndDisable(!team, 0.0f);
        this->arrowMiiIdx[idx] = miiIdxs[idx];
        this->SetColours(idx, team);
        this->RotateArrow(this->arrows[idx], team);

        PushButton& arrow = this->arrows[idx];
        arrow.isHidden = false;
        arrow.manipulator.inaccessible = false;
        if(arrow.IsSelected() && hasMii) {
            this->text.miis[0] = this->miiGroup->GetMii(this->arrowMiiIdx[arrow.buttonId]);
            this->name.SetMessage(BMG_MII_NAME, &this->text);
        }
    }
    for(int remIdx = idx; remIdx < this->rowCount; ++remIdx) {

        this->miis[remIdx].isHidden = true;
        this->arrows[remIdx].isHidden = true;
        this->arrows[remIdx].manipulator.inaccessible = true;
    }
}

/*
24 rows: four columns of 6, from the row variants of the first column (Arrow0-5, Mii0-5 at x -200, a row every 54
from y 140) moved and scaled to 0.62, like the 24 players results (Race/Players24/ResultsPorts.cpp). The rows end
above the name and the toggle (y -125 and -165). The control's four transforms at +4, the third being the
animation's delta, which stays.
*/
void TeamSelect::PlaceRow24(LayoutUIControl& control, u32 idx) {
    static const float columnX[4] ={ -315.0f, -105.0f, 105.0f, 315.0f };
    const float scale = 0.62f;
    const float x = columnX[idx / 6];
    const float y = 150.0f - 44.0f * (idx % 6);
    PositionAndScale* transforms = control.positionAndscale;
    const float baseX = transforms[0].position.x;
    const float baseY = transforms[0].position.y;
    for(int i = 0; i < 4; ++i) {
        if(i == 2) continue;
        transforms[i].position.x = x + (transforms[i].position.x - baseX) * scale;
        transforms[i].position.y = y + (transforms[i].position.y - baseY) * scale;
        transforms[i].scale.x *= scale;
        transforms[i].scale.z *= scale; //Vec2 is x, z
    }
}

UIControl* TeamSelect::CreateControl(u32 id) {
    if(id >= this->internControlCount) return nullptr;
    const u32 count = this->controlCount;
    this->controlCount++;
    char variant[0x40];
    const bool is24 = this->rowCount > 12;
    if(id < this->rowCount) {
        this->AddControl(count, this->arrows[id], 0);
        snprintf(variant, 0x40, "Arrow%d", is24 ? id % 6 : id);
        this->arrows[id].Load(UI::buttonFolder, "TeamArrow", variant, 1, 0, false);
        if(is24) this->PlaceRow24(this->arrows[id], id);
        this->arrows[id].buttonId = id;
        this->arrows[id].SetOnClickHandler(this->onArrowClickHandler, 0);
        this->arrows[id].SetOnSelectHandler(this->onArrowSelectHandler);
        return &this->arrows[id];
    }
    else if(id < this->rowCount * 2) {
        id = id - this->rowCount;
        this->AddControl(count, this->miis[id], 0);
        ControlLoader loader(&this->miis[id]);
        snprintf(variant, 0x40, "Mii%d", is24 ? id % 6 : id);
        const char* brctr = "TeamMii";
        static const char* miiAnim[5] ={ "Translate", "TranslateRight", "TranslateLeft", nullptr, nullptr };
        loader.Load(UI::controlFolder, brctr, variant, miiAnim);
        if(is24) this->PlaceRow24(this->miis[id], id);
        return &this->miis[id];
    }
    else if(id == this->rowCount * 2) {
        this->AddControl(count, this->name, 0);
        ControlLoader loader(&this->name);
        const char* brctr = "TeamName";
        loader.Load(UI::controlFolder, brctr, brctr, nullptr);
        return &this->name;

    }
    else /*if(id == 25)*/ {
        this->AddControl(count, this->toggle, 0);
        this->toggle.Load(1, UI::buttonFolder, "TeamSelectEnable", "Enable");
        this->toggle.SetOnClickHandler(this->onToggleButtonClick, 0);
        return &this->toggle;
    }
}

void TeamSelect::OnArrowClick(PushButton& button, u32 hudSlotId) {
    const s32 id = button.buttonId;
    u8 teamsArrayIdx = this->CalcTeamsArrayIdx(id);
    if(teamsArrayIdx != 0xFF) {
        const u8 newTeam = TeamSelect::teams[teamsArrayIdx] ^ 1;
        TeamSelect::teams[teamsArrayIdx] = newTeam;
        this->RotateArrow(button, newTeam);
        this->SetColours(id, newTeam);
        this->miis[id].animator.GetAnimationGroupById(0).PlayAnimationAtFrameAndDisable(!newTeam, 0.0f);
    }
}

void TeamSelect::OnArrowSelect(PushButton& button, u32 hudSlotId) {
    Mii* mii = this->miiGroup->GetMii(this->arrowMiiIdx[button.buttonId]);
    if(mii == nullptr) return; //a console of a 24 players room whose Mii isn't there
    this->text.miis[0] = mii;
    this->name.SetMessage(BMG_MII_NAME, &this->text);
}

void TeamSelect::OnToggleButtonClick(ToggleButton& button, u32) {
    const u32 bmgId = button.GetState() == false ? BMG_TEAMS_DISABLED : BMG_TEAMS_ENABLED;
    this->isEnabled = button.GetState();
    button.SetMessage(bmgId);
}

int TeamSelect::GetActivePlayerBitfield() const {
    return this->activePlayerBitfield;
}

int TeamSelect::GetPlayerBitfield() const {
    return this->playerBitfield;
}

ManipulatorManager& TeamSelect::GetManipulatorManager() {
    return this->controlsManipulatorManager;
}

UIControl* TeamSelect::CreateExternalControl(u32 id) {
    return nullptr;
}

void TeamSelect::SetButtonHandlers(PushButton& button) {
    button.SetOnClickHandler(this->onButtonClickHandler, 0);
    button.SetOnSelectHandler(this->onButtonSelectHandler);
    button.SetOnDeselectHandler(this->onButtonDeselectHandler);
}

void TeamSelect::OnBackPress(u32 hudSlotId) {
    this->EndStateAnimated(0.0f, 1);
}

void TeamSelect::OnButtonClick(PushButton& button, u32 hudSlotId) {
    this->EndStateAnimated(1, button.GetAnimationFrameSize());
}

u8 TeamSelect::CalcTeamsArrayIdx(u32 idx) {
    u8 teamsArrayIdxs[maxRows];
    u8 miiIdxs[maxRows];
    const u32 count = GetEntries(teamsArrayIdxs, miiIdxs, maxRows);
    return idx < count ? teamsArrayIdxs[idx] : 0xFF;
}

u32 TeamSelect::CalcIdx(u8 teamsArrayIdx) {
    u8 teamsArrayIdxs[maxRows];
    u8 miiIdxs[maxRows];
    const u32 count = GetEntries(teamsArrayIdxs, miiIdxs, maxRows);
    for(u32 idx = 0; idx < count; ++idx) if(teamsArrayIdxs[idx] == teamsArrayIdx) return idx;
    return 0xFF;
}

void TeamSelect::RotateArrow(PushButton& button, u8 team) {
    RotateArrowPane(button, "chara_shadow", team);
    RotateArrowPane(button, "color_yajirushi", team);
    RotateArrowPane(button, "chara_c_down", team);
    RotateArrowPane(button, "chara_light_01", team);
}

void TeamSelect::RotateArrowPane(PushButton& button, const char* name, u8 team) {
    lyt::Picture* pane = static_cast<lyt::Picture*>(button.layout.GetPaneByName(name));
    nw4r::math::VEC2* vec = reinterpret_cast<nw4r::math::VEC2*>(pane->texCoords.data);

    bool isRed = team == 1;
    vec[0].x = isRed ? 0.0f : 1.0f;
    vec[1].x = isRed ? 1.0f : 0.0f;
    vec[2].x = isRed ? 0.0f : 1.0f;
    vec[3].x = isRed ? 1.0f : 0.0f;

}

void TeamSelect::SetColours(u32 idx, u8 team) {
    s16 r = 33;
    s16 b = 0xFF;
    s16 g = 33;
    if(team == 1) {
        r = 0xDC;
        b = 0;
        g = 33;
    }
    nw4r::lyt::Material* arrowBorder = this->arrows[idx].layout.GetPaneByName(this->border)->GetMaterial();
    arrowBorder->tevColours[0].r = r;
    arrowBorder->tevColours[0].g = 0;
    arrowBorder->tevColours[0].b = b;
    arrowBorder->tevColours[0].a = 255;
    arrowBorder->tevColours[1].r = r;
    arrowBorder->tevColours[1].g = 0;
    arrowBorder->tevColours[1].b = b;
    arrowBorder->tevColours[1].a = 255;


    nw4r::lyt::Material* miiBg = this->miis[idx].layout.GetPaneByName(this->miiBg)->GetMaterial();
    miiBg->tevColours[1].r = r;
    miiBg->tevColours[1].g = 0;
    miiBg->tevColours[1].b = b;
    miiBg->tevColours[1].a = 255;


    nw4r::lyt::Material* miiBorder = this->miis[idx].layout.GetPaneByName(this->border)->GetMaterial();
    if(r == 0xDC) r = 0xb3;
    else {
        r = 0;
        g = 0;
    }
    miiBorder->tevColours[0].r = r;
    miiBorder->tevColours[0].g = 0;
    miiBorder->tevColours[0].b = b;
    miiBorder->tevColours[0].a = 255;
    miiBorder->tevColours[1].r = r;
    miiBorder->tevColours[1].g = 0;
    miiBorder->tevColours[1].b = b;
    miiBorder->tevColours[1].a = 255;
}

//Sets Team using the TeamSelectPage if it has been enabled by the host; verifies the validity of the teams
static void SetTeams(RKNet::SELECTHandler* handler, u32& teams) {
    //24 players room: every console, aids 0-23 in teams[aid]; aids 12-23 go in the packet's extension
    if(Players24::IsConsoles24()) {
        u32 teamsExt = 0;
        bool isValid = false;
        if(handler->mode == RKNet::ONLINEMODE_PRIVATE_VS && UI::TeamSelect::isEnabled) {
            u8 teamsArrayIdxs[24];
            u8 miiIdxs[24];
            const u32 count = UI::TeamSelect::GetEntries(teamsArrayIdxs, miiIdxs, 24);
            for(u32 i = 0; i < count; ++i) {
                const u8 aid = teamsArrayIdxs[i];
                const u32 team = UI::TeamSelect::GetPlayerTeam(aid) & 1; //1 red (SetColours), a set bit for GetTeam
                if(team != (UI::TeamSelect::GetPlayerTeam(teamsArrayIdxs[0]) & 1)) isValid = true;
                if(aid < 12) teams |= team << (aid * 2);
                else teamsExt |= team << (aid - 12);
            }
        }
        if(!isValid) {
            teams = 0;
            Players24::DecideTeams24(handler, teams); //everyone in one team, or the page off: drawn
            return;
        }
        reinterpret_cast<Network::ExpSELECTHandler*>(handler)->toSendPacket.teamsExt = teamsExt;
        return;
    }
    Players24::DecideTeams24(handler, teams); //outside them: only empties the extension
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub* sub = &controller->subs[0];
    if(sub->connectionUserDatas[0].playersAtConsole == 0) sub = &controller->subs[1];

    bool isValid = false;
    Team firstTeam = UI::TeamSelect::GetPlayerTeam(0); //guaranteed to exist since a room always has aid0
    if((handler->mode == RKNet::ONLINEMODE_PRIVATE_VS || handler->mode == RKNet::ONLINEMODE_PRIVATE_BATTLE)
        && UI::TeamSelect::isEnabled) {

        for(int aid = 0; aid < sub->playerCount; ++aid) {
            if(sub->availableAids & (1 << aid)) {
                for(int player = 0; player < sub->connectionUserDatas[aid].playersAtConsole; ++player) {
                    Team curSlotTeam = UI::TeamSelect::GetPlayerTeam(aid + 12 * player);
                    if(curSlotTeam != firstTeam) isValid = true;
                    teams = teams | (curSlotTeam & 1) << (aid * 2 + player);
                }
            }
        }
    }
    if(!isValid) handler->DecidePrivateTeams(teams);
}
kmCall(0x806619d8, SetTeams);
}//namespace UI
}//namespace Pulsar