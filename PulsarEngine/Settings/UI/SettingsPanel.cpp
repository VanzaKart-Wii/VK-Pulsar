#include <Settings/UI/SettingsPanel.hpp>
#include <Settings/UI/SettingsPageSelect.hpp>
#include <Settings/Settings.hpp>
#include <Settings/UI/ExpOptionsPage.hpp>
#include <Settings/UI/ExpFroomPage.hpp>
#include <Settings/UI/ExpWFCMainPage.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <VanzaKart.hpp>
#include <UI/ChangeCombo/ChangeCombo.hpp>
#include <Network/PacketExpansion.hpp>
#include <MarioKartWii/UI/Ctrl/CountDown.hpp>


namespace Pulsar {
namespace UI {

//SETTINGS PANEL
SettingsPanel::SettingsPanel()
{
    sheetIdx = 0;
    externControlCount = 1;  // Only save button, no left/right navigation
    internControlCount = Settings::Params::maxRadioCount + Settings::Params::maxScrollerCount;
    hasBackButton = false;
    nextPageId = static_cast<PageId>(id);
    //titleBmg = BMG_SETTINGS_TITLE;
    activePlayerBitfield = 1;
    movieStartFrame = -1;
    extraControlNumber = 0;
    isLocked = false;
    controlCount = 0;
    nextSection = SECTION_NONE;
    controlSources = 2;

    SectionId id = SectionMgr::sInstance->curSection->sectionId;
    if(id == SECTION_OPTIONS) prevPageId = PAGE_OPTIONS;
    else if((id == SECTION_P1_WIFI) || (id == SECTION_P1_WIFI_FROM_FROOM_RACE) || (id == SECTION_P1_WIFI_FROM_FIND_FRIEND)
        || (id == SECTION_P2_WIFI) || (id == SECTION_P2_WIFI_FROM_FROOM_RACE)) prevPageId = PAGE_WFC_MAIN;
    else if(id >= SECTION_LICENSE_SETTINGS_MENU && id <= SECTION_SINGLE_P_LIST_RACE_GHOST) prevPageId = PAGE_SINGLE_PLAYER_MENU;

    onMessageBoxClickHandler.ptmf = &Menu::ChangeToPrevSection;

    onRadioButtonClickHandler.subject = this;
    onRadioButtonClickHandler.ptmf = &SettingsPanel::OnRadioButtonClick;
    onRadioButtonChangeHandler.subject = this;
    onRadioButtonChangeHandler.ptmf = &SettingsPanel::OnRadioButtonChange;

    onUpDownClickHandler.subject = this;
    onUpDownClickHandler.ptmf = &SettingsPanel::OnUpDownClick;
    onUpDownSelectHandler.subject = this;
    onUpDownSelectHandler.ptmf = &SettingsPanel::OnUpDownSelect;

    onTextChangeHandler.subject = this;
    onTextChangeHandler.ptmf = &SettingsPanel::OnTextChange;

    onButtonSelectHandler.subject = this;
    onButtonSelectHandler.ptmf = &SettingsPanel::OnExternalButtonSelect;
    onButtonDeselectHandler.subject = this;
    onButtonDeselectHandler.ptmf = &Pages::VSSettings::OnButtonDeselect;
    onBackPressHandler.subject = this;
    onBackPressHandler.ptmf = &SettingsPanel::OnBackPress;
    onStartPressHandler.subject = this;
    onStartPressHandler.ptmf = &MenuInteractable::HandleStartPress;

    onButtonClickHandler.subject = this;
    onButtonClickHandler.ptmf = &SettingsPanel::OnSaveButtonClick;
    onRightButtonClickHandler.subject = this;
    onRightButtonClickHandler.ptmf = &SettingsPanel::OnRightButtonClick;
    onLeftButtonClickHandler.subject = this;
    onLeftButtonClickHandler.ptmf = &SettingsPanel::OnLeftButtonClick;

    this->controlsManipulatorManager.Init(1, false);
    this->SetManipulatorManager(controlsManipulatorManager);
    this->controlsManipulatorManager.SetGlobalHandler(START_PRESS, onStartPressHandler, false, false);
    this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, onBackPressHandler, false, false);
};

SettingsPanel::~SettingsPanel() {
    Settings::Mgr* mgr = Settings::Mgr::sInstance;
    mgr->SetLastSelectedCup(CupsConfig::sInstance->lastSelectedCup);
    mgr->RequestSave();
    //delete[] radioButtonControls;
    //delete[] upDownControls;
    //delete[] textUpDown;
}

void SettingsPanel::OnInit() {
    //radioButtonControls = new RadioButtonControl[this->radioCount];
    //upDownControls = new UpDownControl[this->scrollersCount];
    //textUpDown = new TextUpDownValueControl[this->scrollersCount];

    for(int i = 0; i < Settings::Params::uiPageCount; ++i) {
        const Settings::PageParams& page = Settings::Params::uiPages[i];
        //A value past the options a row has (a save from a build that had more of them) would make the
        //control select a button that doesn't exist and crash, so it falls back to the first option
        for(int radioIdx = 0; radioIdx < page.radioCount; ++radioIdx) {
            u8 value = GetFileValue(page.radios[radioIdx]);
            if(value >= page.radios[radioIdx].optionCount) value = 0;
            this->radioSettings[i][radioIdx] = value;
        }
        for(int scrollerIdx = 0; scrollerIdx < page.scrollerCount; ++scrollerIdx) {
            u8 value = GetFileValue(page.scrollers[scrollerIdx]);
            if(value >= page.scrollers[scrollerIdx].optionCount) value = 0;
            this->scrollerSettings[i][scrollerIdx] = value;
        }
    }
    MenuInteractable::OnInit();
    this->SetTransitionSound(0, 0);
}

UIControl* SettingsPanel::CreateExternalControl(u32 id) {
    const char* variant = "SAVE";
    if(id == 1) variant = "RIGHT";
    else if(id == 2) variant = "LEFT";
    PushButton* button = new(PushButton);
    this->AddControl(this->controlCount++, *button, 0);
    button->Load(UI::buttonFolder, "Settings", variant, this->activePlayerBitfield, 0, false);
    return button;
};

UIControl* SettingsPanel::CreateControl(u32 id) {
    const Settings::Mgr& settings = Settings::Mgr::Get();
    if(id < Settings::Params::maxRadioCount) {
        RadioButtonControl& radioButtonControl = this->radioButtonControls[id];
        this->AddControl(this->controlCount++, radioButtonControl, 0);

        char variant[12];
        char option0Variant[12];
        char option1Variant[12];
        char option2Variant[12];
        char option3Variant[12];
        snprintf(variant, 12, "Row%d", id);
        snprintf(option0Variant, 12, "%sOption%d", variant, 0);
        snprintf(option1Variant, 12, "%sOption%d", variant, 1);
        snprintf(option2Variant, 12, "%sOption%d", variant, 2);
        snprintf(option3Variant, 12, "%sOption%d", variant, 3);

        const char* optionVariants[5] ={ option0Variant, option1Variant, option2Variant, option3Variant, nullptr };
        radioButtonControl.Load(4, 0, UI::controlFolder, "RadioBase", variant, "RadioOption", optionVariants, 1, 0, 0);
        radioButtonControl.SetOnClickHandler(this->onRadioButtonClickHandler);
        radioButtonControl.SetOnChangeHandler(this->onRadioButtonChangeHandler);
        radioButtonControl.id = id;

    }
    else if(id < (Settings::Params::maxRadioCount + Settings::Params::maxScrollerCount)) {
        id -= Settings::Params::maxRadioCount;
        UpDownControl& upDownControl = this->upDownControls[id];
        this->AddControl(this->controlCount, upDownControl, 0);
        ++this->controlCount;

        char variant[12];
        snprintf(variant, 12, "UpDown%d", id);

        upDownControl.Load(7, 0, UI::controlFolder, "UpDownBase", variant, "UpDownR", "Right", "UpDownL",
            "Left", &this->textUpDown[id], 1, 0, false, true, true);
        upDownControl.SetOnClickHandler(this->onUpDownClickHandler);
        upDownControl.SetOnSelectHandler(this->onUpDownSelectHandler);
        upDownControl.id = id;

        TextUpDownValueControl& valueControl = this->textUpDown[id];
        valueControl.Load(UI::controlFolder, "UpDownValue", "Value", "UpDownText", "Text");
        valueControl.SetOnTextChangeHandler(this->onTextChangeHandler);

    }
    return nullptr;
}

void SettingsPanel::SetButtonHandlers(PushButton& button) {
    PtmfHolder_2A<MenuInteractable, void, PushButton&, u32>* onClickHandler = &this->onButtonClickHandler;
    if(button.buttonId == 1) onClickHandler = &this->onRightButtonClickHandler;
    else if(button.buttonId == 2) onClickHandler = &this->onLeftButtonClickHandler;
    button.SetOnClickHandler(*onClickHandler, 0);
    button.SetOnSelectHandler(this->onButtonSelectHandler);
    button.SetOnDeselectHandler(this->onButtonDeselectHandler);
}

void SettingsPanel::OnActivate() {
    const Settings::PageParams& page = Settings::Params::uiPages[this->sheetIdx];
    this->titleBmg = page.titleBmg;
    this->externControls[0]->SelectInitial(0);
    this->bottomText->SetMessage(BMG_SETTINGS_BOTTOM); //no need for any offset here as this is the default "save" bottom msg
    // Check if we're in any of the voting sections
    SectionId id = SectionMgr::sInstance->curSection->sectionId;
    bool isVotingSection = (id >= SECTION_P1_WIFI_FROOM_VS_VOTING && id <= SECTION_P2_WIFI_FROOM_COIN_VOTING) 
    || (id == SECTION_P1_WIFI_VS_VOTING);

    for(int i = 0; i < Settings::Params::maxRadioCount; ++i) {
        RadioButtonControl& radio = this->radioButtonControls[i];
        const bool isDisabled = i >= page.radioCount;
        radio.isHidden = isDisabled;
        radio.manipulator.inaccessible = isDisabled;

        if(!isDisabled) {
            const Settings::RowParams& row = page.radios[radio.id];
            radio.buttonsCount = row.optionCount;
            const u8 setting = this->radioSettings[this->sheetIdx][radio.id];
            radio.chosenButtonId = setting;
            radio.selectedButtonId = setting;
            const u32 bmgCategory = BmgBase(row.bmgPage) + BMG_RADIO_SETTINGS;
            radio.SetMessage(row.bmgRow + bmgCategory);

            for(int j = 0; j < 4; ++j) {
                const bool isHidden = j >= row.optionCount;
                radio.optionButtonsArray[j].isHidden = isHidden;
                if(!isHidden) radio.optionButtonsArray[j].SetMessage((row.bmgRow + 1 << 4) + j + bmgCategory);
            }
        }
    }
    for(int i = 0; i < Settings::Params::maxScrollerCount; ++i) {
        UpDownControl& scroller = this->upDownControls[i];
        TextUpDownValueControl& valueControl = this->textUpDown[i];
        const bool isDisabled = i >= page.scrollerCount;
        scroller.isHidden = isDisabled;
        scroller.manipulator.inaccessible = isDisabled;
        scroller.optionsCount = isDisabled ? 0 : page.scrollers[i].optionCount;
        valueControl.isHidden = isDisabled;
        if(!isDisabled) {
            scroller.curSelectedOption = this->scrollerSettings[this->sheetIdx][i];
            scroller.SetMessage(page.scrollers[i].bmgRow + BmgBase(page.scrollers[i].bmgPage) + BMG_SCROLLER_SETTINGS);
            scroller.SelectInitial(scroller.curSelectedOption);
        }
    }
    MenuInteractable::OnActivate();

    //In a voting section the scrollers of the user pages are hidden (the Misc page can't be opened there)
    const bool hideScrollers = isVotingSection && this->sheetIdx >= Settings::UIPAGE_EXTENDEDTEAMS;
    for(int i = 0; i < Settings::Params::maxScrollerCount; ++i) {
        UpDownControl& upDown = this->upDownControls[i];
        TextUpDownValueControl& text = this->textUpDown[i];
        const bool isDisabled = hideScrollers || i >= page.scrollerCount;
        upDown.isHidden = isDisabled;
        upDown.manipulator.inaccessible = isDisabled;
        text.isHidden = isDisabled;
    }

    // Make sure radio buttons remain accessible
    for(int i = 0; i < Settings::Params::maxRadioCount; ++i) {
        RadioButtonControl& radio = this->radioButtonControls[i];
        const bool isDisabled = i >= page.radioCount;
        radio.isHidden = isDisabled;
        radio.manipulator.inaccessible = isDisabled;
    }
}

const ut::detail::RuntimeTypeInfo* SettingsPanel::GetRuntimeTypeInfo() const {
    return Pages::VSSettings::typeInfo;
}

void SettingsPanel::OnExternalButtonSelect(PushButton& button, u32 r5) {
    // Only save button exists now (id == 0)
    this->bottomText->SetMessage(BMG_SETTINGS_BOTTOM);
}

int SettingsPanel::GetActivePlayerBitfield() const {
    return this->activePlayerBitfield;
}

int SettingsPanel::GetPlayerBitfield() const {
    return this->playerBitfield;
}

ManipulatorManager& SettingsPanel::GetManipulatorManager() {
    return this->controlsManipulatorManager;
}

void SettingsPanel::LoadPrevMenuAndSaveSettings(PushButton& button) {
    const u8 languageBefore = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_LANGUAGE, SCROLLER_LANGUAGE);
    const u8 patchesBefore = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_LANGUAGE, RADIO_PATCHES);

    this->LoadPrevPage(button);
    // Navigate back to SettingsPageSelect instead of the original menu
    this->nextPageId = static_cast<PageId>(SettingsPageSelect::id);
    this->EndStateAnimated(0, button.GetAnimationFrameSize());
    this->SaveSettings(true);

    const u8 languageAfter = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_LANGUAGE, SCROLLER_LANGUAGE);
    const u8 patchesAfter = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_LANGUAGE, RADIO_PATCHES);
    //The menus are loaded again, in the new language or with/without the loose files of "My Stuff"
    if(languageAfter != languageBefore || patchesAfter != patchesBefore) {
        if(languageAfter != languageBefore) Languages::ApplyArchiveLanguage();
        SectionMgr* sectionMgr = SectionMgr::sInstance;
        sectionMgr->nextSectionId = sectionMgr->curSection->sectionId;
        sectionMgr->RequestSceneReinit(0, 0xFF);
    }
}

//Where a row's value is saved, and where its texts start
u8 SettingsPanel::GetFileValue(const Settings::RowParams& row) {
    const Settings::Mgr& settings = Settings::Mgr::Get();
    if(row.filePage < Settings::Params::pulsarPageCount) {
        return settings.GetSettingValue(static_cast<Settings::Type>(row.filePage), row.fileSlot);
    }
    return settings.GetUserSettingValue(static_cast<Settings::UserType>(row.filePage - Settings::Params::pulsarPageCount),
        row.fileSlot);
}

void SettingsPanel::SetFileValue(const Settings::RowParams& row, u8 value) {
    Settings::Mgr* settings = Settings::Mgr::sInstance;
    if(row.filePage < Settings::Params::pulsarPageCount) {
        settings->SetSettingValue(static_cast<Settings::Type>(row.filePage), row.fileSlot, value);
    }
    else settings->SetUserSettingValue(static_cast<Settings::UserType>(row.filePage - Settings::Params::pulsarPageCount),
        row.fileSlot, value);
}

u32 SettingsPanel::BmgBase(u8 bmgPage) {
    if(bmgPage < Settings::Params::pulsarPageCount) return bmgPage << 12;
    return BMG_USERSETTINGSOFFSET + (bmgPage - Settings::Params::pulsarPageCount << 12);
}

//On Save Click/Back Press, is called and updates PulsarSettings
void SettingsPanel::SaveSettings(bool writeFile) {
    for(int i = 0; i < Settings::Params::uiPageCount; ++i) {
        const Settings::PageParams& page = Settings::Params::uiPages[i];
        for(int radioIdx = 0; radioIdx < page.radioCount; ++radioIdx) {
            SetFileValue(page.radios[radioIdx], this->radioSettings[i][radioIdx]);
        }
        for(int scrollerIdx = 0; scrollerIdx < page.scrollerCount; ++scrollerIdx) {
            SetFileValue(page.scrollers[scrollerIdx], this->scrollerSettings[i][scrollerIdx]);
        }
    }
    Settings::Mgr::sInstance->Update();
}

void SettingsPanel::OnBackPress(u32 hudSlotId) {
    PushButton& okButton = *this->externControls[0];
    okButton.SelectFocus();
    this->LoadPrevMenuAndSaveSettings(okButton);
}

void SettingsPanel::OnSaveButtonClick(PushButton& button, u32 hudSlotId) {
    this->LoadPrevMenuAndSaveSettings(button);
}

void SettingsPanel::OnRightButtonClick(PushButton& button, u32 hudSlotId) {
    this->OnButtonClick(button, 1);
}

void SettingsPanel::OnLeftButtonClick(PushButton& button, u32 hudSlotId) {
    this->OnButtonClick(button, -1);
}

void SettingsPanel::OnButtonClick(PushButton& button, u32 direction) {
    this->nextPageId = this->pageId;
    const int nextIdx = this->GetNextSheetIdx(direction);
    this->sheetIdx = nextIdx;

    this->EndStateAnimated(0, button.GetAnimationFrameSize());
    this->SaveSettings(false);
}

void SettingsPanel::OnRadioButtonClick(RadioButtonControl& radioButtonControl, u32 hudSlotId, u32 optionId) {
    this->radioSettings[this->sheetIdx][radioButtonControl.id] = optionId;
}

void SettingsPanel::OnRadioButtonChange(RadioButtonControl& radioButtonControl, u32 hudSlotId, u32 optionId) {
    const Settings::RowParams& row = Settings::Params::uiPages[this->sheetIdx].radios[radioButtonControl.id];
    this->bottomText->SetMessage(BmgBase(row.bmgPage) + BMG_RADIO_SETTINGS + (row.bmgRow + 1 << 8) + optionId);
}

void SettingsPanel::OnUpDownClick(UpDownControl& upDownControl, u32 hudSlotId) {
    PushButton& okButton = *this->externControls[0];
    okButton.Select(0);
}

void SettingsPanel::OnTextChange(TextUpDownValueControl::TextControl& text, u32 optionId) {
    if(this->currentState < STATE_ACTIVATING) {
        return;
    }

    u32 id = this->GetTextId(text);
    this->scrollerSettings[this->sheetIdx][id] = optionId;

    const Settings::RowParams& row = Settings::Params::uiPages[this->sheetIdx].scrollers[id];
    const u32 bmgId = BmgBase(row.bmgPage) + BMG_SCROLLER_SETTINGS + optionId;
    text.SetMessage(bmgId + (row.bmgRow + 1 << 4));
    if(!this->externControls[0]->IsSelected()) {
        this->bottomText->SetMessage(bmgId + (row.bmgRow + 1 << 8));
    }
};

void SettingsPanel::OnUpDownSelect(UpDownControl& upDownControl, u32 hudSlotId) {
    const Settings::RowParams& row = Settings::Params::uiPages[this->sheetIdx].scrollers[upDownControl.id];
    const u32 bmgId = BmgBase(row.bmgPage) + BMG_SCROLLER_SETTINGS + (row.bmgRow + 1 << 8) + upDownControl.curSelectedOption;
    this->bottomText->SetMessage(bmgId);
}

int SettingsPanel::GetNextSheetIdx(s32 direction) {
    return (this->sheetIdx + direction + Settings::Params::uiPageCount) % Settings::Params::uiPageCount;
}

void SettingsPanel::BeforeControlUpdate() {
    SectionId id = SectionMgr::sInstance->curSection->sectionId;
    bool isVotingSection = (id >= SECTION_P1_WIFI_FROOM_VS_VOTING && id <= SECTION_P2_WIFI_FROOM_COIN_VOTING) || (id == SECTION_P1_WIFI_VS_VOTING);
    if (isVotingSection) {
        Pages::SELECTStageMgr* selectStageMgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
        CountDown* timer = &selectStageMgr->countdown;
        if (timer->countdown <= 0) {
            this->OnBackPress(0);
        }
    }
}


}//namespace UI
}//namespace Pulsar




