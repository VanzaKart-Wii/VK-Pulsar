#include <UI/ExtendedTeamSelect/Result/CtrlRaceResultExtendedTeams.hpp>
#include <CustomCharacters/CustomCharacters.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <Race/Players24/Players24.hpp>

namespace Pulsar {
namespace UI {

//Red/blue: the stock column compares the racer's team in the race scenario (CtrlRaceResultTeam::InitSelf)
bool CtrlRaceResultExtendedTeams::IsMember(u8 playerId) const {
    if (this->isRedBlue) return Players24::GetPlayer(Racedata::sInstance->racesScenario, playerId).team == static_cast<Team>(this->teamId);
    return ExtendedTeamManager::sInstance->GetPlayerTeam(playerId) == this->teamId;
}

/*
Every member counts for the team's points; the rows show the best itemCount of them (all of them as long as no team
has more members than rows, which ExpGPVSLeaderboardUpdate::GetNextPage makes sure of).
*/
void CtrlRaceResultExtendedTeams::InitSelf() {
    if (!Raceinfo::sInstance->IsAtLeastStage(RACESTAGE_FINISHED)) {
        return;
    }

    this->textInfo = Text::Info();

    this->teamScore = 0;
    this->currentScore = this->teamScore;

    int playerCount = 0;
    CtrlRaceResultTeam::Player members[ExtendedTeamManager::MAX_PLAYERS];

    RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    const PageId pageId = this->parentGroup->parentPage->pageId;
    for (int i = 0; i < scenario.playerCount && i < ExtendedTeamManager::MAX_PLAYERS; i++) {
        if (this->IsMember(i)) {
            CtrlRaceResultTeam::Player& member = members[playerCount];
            member.playerIdx = i;
            member.battleScore = 0;
            member.prevBattleScore = 0;
            member.prevBattleScoreFloat = 0.0f;
            if (pageId == (PageId)PULPAGE_EXTENDEDTEAMS_RESULT_TOTAL) {
                const RacedataPlayer& player = Players24::GetPlayer(scenario, i);
                member.battleScore = player.score;
                member.prevBattleScore = player.previousScore;
                member.prevBattleScoreFloat = player.previousScore;

                this->currentScore += player.previousScore;
                this->teamScore += player.score;
            }

            playerCount++;
        }
    }

    qsort(members, playerCount, sizeof(CtrlRaceResultTeam::Player), (int (*)(const void*, const void*))CtrlRaceResultTeam::ComparePlayers);
    if (playerCount > (int)this->itemCount) playerCount = this->itemCount;
    memcpy(this->players, members, sizeof(CtrlRaceResultTeam::Player) * playerCount);
    for (int i = playerCount; i < (int)this->itemCount; i++) {
        this->players[i].playerIdx = -1;
        this->players[i].battleScore = 0;
        this->players[i].prevBattleScore = 0;
    }

    int rank = 0;
    for (int i = 0; i < (int)this->itemCount; i++) {
        if (this->players[i].playerIdx == -1) {
            this->items[i].isHidden = true;
            continue;
        } else {
            this->items[i].isHidden = false;
        }

        const u8 playerId = this->players[i].playerIdx;

        u8 localPlayerCount = scenario.localPlayerCount;
        PlayerType playerType = Players24::GetPlayer(scenario, playerId).playerType;
        CharacterId characterId = Players24::GetPlayer(scenario, playerId).characterId;
        MiiGroup& miiGroup = SectionMgr::sInstance->sectionParams->playerMiis;

        if (playerType == PLAYER_REAL_LOCAL) {
            this->items[i].animator.GetAnimationGroupById(0).PlayAnimationAtFrame(0, 0.0f);
            this->items[i].animator.GetAnimationGroupById(1).PlayAnimationAtFrame(0, 0.0f);
            this->items[i].animator.GetAnimationGroupById(2).PlayAnimationAtFrame(0, 0.0f);
        }

        Text::Info info;
        bool isOnline = false;
        // Not a Mii character
        if (characterId <= ROSALINA) {
            if (scenario.settings.gamemode >= MODE_PRIVATE_VS && scenario.settings.gamemode <= MODE_PRIVATE_BATTLE) {
                isOnline = true;
            }

            if ((isOnline || (localPlayerCount > 1)) && (playerType != PLAYER_CPU)) {
                info.miis[0] = miiGroup.GetMii(playerId);
                this->items[i].SetTextBoxMessage("mii_name", BMG_MII_NAME, &info);
            } else {
                if (!CustomCharacters::SetRaceNameTextIfCustom(this->items[i], "mii_name", playerId)) {
                    u32 characterBmg = GetCharacterBMGId(characterId, true);
                    this->items[i].SetTextBoxMessage("mii_name", characterBmg, nullptr);
                }
            }
        } else {
            info.miis[0] = miiGroup.GetMii(playerId);
            this->items[i].SetTextBoxMessage("mii_name", BMG_MII_NAME, &info);
        }

        // Is a Mii character
        if (isOnline || characterId >= MII_S_A_MALE) {
            this->items[i].SetMiiPane("chara_icon", miiGroup, playerId, 2);
            this->items[i].SetMiiPane("chara_icon_sha", miiGroup, playerId, 2);
        } else {
            this->items[i].SetPicturePane("chara_icon", GetCharacterIconPaneName(characterId));
            this->items[i].SetPicturePane("chara_icon_sha", GetCharacterIconPaneName(characterId));
        }

        int prevScore = this->players[i].prevBattleScore;
        this->textInfo.intToPass[0] = prevScore;

        this->items[i].SetTextBoxMessage("pts", BMG_SCORE_PTS, &this->textInfo);
        this->items[i].SetTextBoxMessage("point", BMG_SCORE_POINTS, &this->textInfo);

        rank++;
    }

    if (this->resultTeamPoint) {
        this->textInfo = Text::Info();
        this->resultTeamPoint->ResetMsg();

        this->textInfo.intToPass[0] = this->currentScore;
        u32 teamNameBmgID = BMG_NUMBER_RACE;
        this->resultTeamPoint->SetTextBoxMessage("point", teamNameBmgID, &this->textInfo);
    }
}

void CtrlRaceResultExtendedTeams::OnUpdate() {
    if (this->parentGroup->parentPage->currentState != STATE_ACTIVE) {
        return;
    }

    for (int i = 0; i < (int)this->itemCount; i++) {
        if (this->players[i].playerIdx != -1) {
            u32 score = this->players[i].battleScore;
            if (score - this->players[i].prevBattleScore > 0.0) {
                this->players[i].prevBattleScoreFloat += 0.5f;
                this->players[i].prevBattleScore = this->players[i].prevBattleScoreFloat;
                this->textInfo.intToPass[0] = this->players[i].prevBattleScore;
                this->items[i].SetTextBoxMessage("pts", BMG_SCORE_PTS, &this->textInfo);
                this->items[i].SetTextBoxMessage("point", BMG_SCORE_POINTS, &this->textInfo);
                break;
            }

            this->textInfo.intToPass[0] = score;
            this->items[i].SetTextBoxMessage("pts", BMG_SCORE_PTS, &this->textInfo);
        }
    }

    if (this->resultTeamPoint) {
        this->textInfo = Text::Info();

        if (this->teamScore - this->currentScore > 0.0) {
            this->currentScore += 1.0;
            this->textInfo.intToPass[0] = this->currentScore;

            this->PlaySound(0xde, 0xffffffff);

            u32 teamNameBmgID = BMG_NUMBER_RACE;
            this->resultTeamPoint->SetTextBoxMessage("point", teamNameBmgID, &this->textInfo);
        }
    }
}

const char* CtrlRaceResultExtendedTeams::GetClassName() const {
    return "CtrlRaceResultExpandedTeams";
}

//One row of the column: the extended teams recolour the red parts, red/blue hides the other team's parts as the game
void CtrlRaceResultExtendedTeams::LoadItem(u32 i, const char* brctr, bool recolor) {
    char variant[20];
    const char* anims[] = {
        "Loop", "Loop", nullptr,
        "Select", "SelectOn", "SelectOff", nullptr,
        "Select2", "Select2On", "Select2Off", nullptr,
        nullptr};

    snprintf(variant, 20, "BlueRed%d", i);
    this->AddControl(i, &this->items[i]);

    ControlLoader itemLoader(&this->items[i]);
    itemLoader.Load("result", brctr, variant, anims);

    if (!recolor) {
        this->items[i].SetPaneVisibility(static_cast<Team>(this->teamId) == TEAM_RED ? "blue_null" : "red_null", false);
    } else {
        this->items[i].SetPaneVisibility("blue_null", false);

        nw4r::lyt::Pane* teamColorPane1 = this->items[i].layout.GetPaneByName("black_parts_r_l");
        nw4r::lyt::Pane* teamColorPane2 = this->items[i].layout.GetPaneByName("black_parts_r_r");
        nw4r::lyt::Material* teamColor1 = teamColorPane1->GetMaterial();
        nw4r::lyt::Material* teamColor2 = teamColorPane2->GetMaterial();
        teamColorPane1->alpha = teamColorPane2->alpha = 255;

        u8 r, g, b;
        ExtendedTeamSelect::GetTeamColor(this->teamId, r, g, b);

        for (int j = 0; j < 2; j++) {
            teamColor1->tevColours[j].r = teamColor2->tevColours[j].r = r;
            teamColor1->tevColours[j].g = teamColor2->tevColours[j].g = g;
            teamColor1->tevColours[j].b = teamColor2->tevColours[j].b = b;
            teamColor1->tevColours[j].a = teamColor2->tevColours[j].a = 255;
        }
    }

    this->items[i].animator.GetAnimationGroupById(0).PlayAnimationAtFrame(0, 0.0f);
    this->items[i].animator.GetAnimationGroupById(1).PlayAnimationAtFrame(1, 0.0f);
    this->items[i].animator.GetAnimationGroupById(2).PlayAnimationAtFrame(1, 0.0f);

    this->items[i].SetSoundIds(0xe1, 0xe0);

    this->textInfo.intToPass[0] = 0;
    this->items[i].SetTextBoxMessage("pts", BMG_SCORE_PTS, &this->textInfo);
}

//The team's points above the column: red/blue keeps its own colours, the extended teams recolour the red one
void CtrlRaceResultExtendedTeams::LoadTeamPoint(const char* brctr) {
    this->resultTeamPoint = new LayoutUIControl();
    this->AddControl(this->itemCount, this->resultTeamPoint);

    const char* teamPointAnims[] = {
        "team", "blue", "red", nullptr,
        nullptr};

    const bool isRed = !this->isRedBlue || static_cast<Team>(this->teamId) == TEAM_RED;
    ControlLoader pointLoader(this->resultTeamPoint);
    pointLoader.Load("result", brctr, isRed ? "red" : "blue", teamPointAnims);
    if (this->isRedBlue) {
        this->resultTeamPoint->animator.GetAnimationGroupById(0).PlayAnimationAtFrame(isRed ? 1 : 0, 0.0f);
        return;
    }
    this->resultTeamPoint->animator.GetAnimationGroupById(0).isActive = false;

    nw4r::lyt::TextBox* pane1 = (nw4r::lyt::TextBox*)this->resultTeamPoint->layout.GetPaneByName("point");
    nw4r::lyt::TextBox* pane2 = (nw4r::lyt::TextBox*)this->resultTeamPoint->layout.GetPaneByName("pts");
    nw4r::lyt::Material* mat1 = pane1->GetMaterial();
    nw4r::lyt::Material* mat2 = pane2->GetMaterial();

    u8 r, g, b;
    ExtendedTeamSelect::GetTeamColor(this->teamId, r, g, b);

    mat1->tevColours[1].r = mat2->tevColours[1].r = r;
    mat1->tevColours[1].g = mat2->tevColours[1].g = g;
    mat1->tevColours[1].b = mat2->tevColours[1].b = b;
}

void CtrlRaceResultExtendedTeams::Load(ExtendedTeamID teamID, int numTeams, int teamIdx) {
    char variant[20];
    this->teamId = teamID;
    this->itemCount = 6;
    this->isRedBlue = false;

    snprintf(variant, 20, "ResultVSTeamNULL%d_%d", numTeams, teamIdx);

    ControlLoader selfLoader(this);
    selfLoader.Load("result", "ExpandedTeamResult", variant, nullptr);

    this->InitControlGroup(this->itemCount + 1);

    for (u32 i = 0; i < this->itemCount; i++) this->LoadItem(i, "ResultVSTeam", true);

    this->LoadTeamPoint("ResultTeamPoint");
}

bool CtrlRaceResultExtendedTeams::HasWideLayouts() {
    return ExtendedTeamManager::HasUIFile("result/ctrl/ResultVSTeam_24.brctr")
        && ExtendedTeamManager::HasUIFile("result/ctrl/ResultVSTeamNULL_24.brctr")
        && ExtendedTeamManager::HasUIFile("result/ctrl/ResultTeamPoint_24.brctr");
}

/*
Two teams past 12 racers, one of the two columns of TeamVSLeaderboardTotal (CtrlRaceResultTeam::Load) with the pack's
24 copies: ResultVSTeamNULL_24 (the columns), ResultVSTeam_24 (BlueRed0-11, two rows of 6 per team) and
ResultTeamPoint_24. Red/blue as the game draws it, the extended teams recoloured. A pack without them gets the stock
files and the best 6 of each team.
*/
void CtrlRaceResultExtendedTeams::LoadWide(u32 column, ExtendedTeamID teamID, bool isRedBlue) {
    char variant[20];
    const bool has24 = HasWideLayouts();
    this->teamId = teamID;
    this->itemCount = has24 ? MAX_ITEMS : 6;
    this->isRedBlue = isRedBlue;

    snprintf(variant, 20, "ResultVSTeamNULL%d", column);
    ControlLoader selfLoader(this);
    selfLoader.Load("result", has24 ? "ResultVSTeamNULL_24" : "ResultVSTeamNULL", variant, nullptr);

    this->InitControlGroup(this->itemCount + 1);
    for (u32 i = 0; i < this->itemCount; i++) this->LoadItem(i, has24 ? "ResultVSTeam_24" : "ResultVSTeam", !isRedBlue);

    this->LoadTeamPoint(has24 ? "ResultTeamPoint_24" : "ResultTeamPoint");
}

bool CtrlRaceResultExtendedTeams::IsResultAnimDone() const {
    for (int i = 0; i < (int)this->itemCount; i++) {
        if ((this->players[i].playerIdx != -1) && (this->players[i].battleScore - this->players[i].prevBattleScore > 0.0)) {
            return true;
        }
    }

    if (this->teamScore - this->currentScore <= 0.0) {
        return false;
    }

    return true;
}

}  // namespace UI
}  // namespace Pulsar
