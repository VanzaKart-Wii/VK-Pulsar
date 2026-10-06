#include <kamek.hpp>
#include <Race/Players24/Patches.hpp>
#include <Race/Players24/Players24.hpp>
#include <Race/Players24/Online24.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/USER.hpp>
#include <MarioKartWii/Mii/MiiGroup.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <MarioKartWii/UI/Page/Other/VR.hpp>
#include <MarioKartWii/UI/Page/Other/Votes.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <Debug/Debug.hpp>
#include <include/c_stdio.h>

extern "C" void OSReport(const char* format, ...);

namespace Pulsar {
namespace Players24 {

static const u32 wideRows = 24;
static const u32 voteFixedControls = 3;

static bool IsWideRoom() { return RoomPlayerCount() > vanillaPlayers; }

static bool HasLayout24(const char* name) {
    char path[64];
    snprintf(path, sizeof(path), "control/ctrl/%s.brctr", name);
    const ArchiveMgr* archives = ArchiveMgr::sInstance;
    const bool found = archives != nullptr && archives->GetFile(ARCHIVE_HOLDER_UI, path) != nullptr;
    if(!found) OSReport("[VK 24P] %s missing from the UI archives: 12 rows\n", path);
    return found;
}
static u32 DecideRows(const char* layout24) {
    const bool hasLayout24 = HasLayout24(layout24);
    return IsWideRoom() && hasLayout24 ? wideRows : vanillaPlayers;
}

static u32 selectRows = vanillaPlayers;
static u32 vrRows = vanillaPlayers;
static u32 voteRows = vanillaPlayers;

static void CompareRows(HookRegs& regs, s32 value, s32 rows) {
    const u32 result = value < rows ? 0x80000000 : value > rows ? 0x40000000 : 0x20000000;
    regs.cr = (regs.cr & 0x0FFFFFFF) | result;
}

u32 DecideVRRows() {
    vrRows = DecideRows("WifiMemberConfirm_24");
    return vrRows;
}

static void SelectMiiCount(HookRegs& regs, u32) {
    selectRows = IsWideRoom() ? wideRows : vanillaPlayers;
    regs.gpr[4] = selectRows;
}

typedef void (*CopyMiiFunc)(MiiGroup* group, MiiGroup* source, u32 sourceIdx, u32 destIdx);
typedef void (*PlayerMiiFunc)(MiiGroup* group, u32 idx, u32 aid, u32 slot);

static void AddPlayersPastAid12(HookRegs& regs, u32) {
    if(selectRows <= vanillaPlayers) return;
    Pages::SELECTStageMgr* page = reinterpret_cast<Pages::SELECTStageMgr*>(regs.gpr[15]);
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::USERHandler* user = RKNet::USERHandler::sInstance;
    if(controller == nullptr || user == nullptr) return;
    const u8 localAid = controller->subs[controller->currentSub & 1].localAid;
    u32 count = regs.gpr[18];
    const Team team = count > 0 ? page->infos[0].team : static_cast<Team>(2);
    SectionParams* params = SectionMgr::sInstance->sectionParams;
    for(u8 aid = vanillaPlayers; aid < 24; ++aid) {
        const u8 players = PlayersAtAid(aid);
        for(u8 slot = 0; slot < players && count < selectRows; ++slot, ++count) {
            PlayerInfo& info = page->infos[count];
            info.aid = aid;
            info.hudSlotid = slot;
            info.team = team;
            const RKNet::USERPacket& packet = aid == localAid ? user->toSendPacket : user->receivedPackets[aid];
            info.vr = packet.vr;
            info.br = packet.br;
            if(aid == localAid) {
                reinterpret_cast<CopyMiiFunc>(Port(0x805faf34))(&page->miiGroup, &params->localPlayerMiis, slot, count);
            }
            else reinterpret_cast<PlayerMiiFunc>(Port(0x805fa8b8))(&page->miiGroup, count, aid, slot);
        }
    }
    regs.gpr[18] = count;
}

static void VRLoadLoop(HookRegs& regs, u32) { CompareRows(regs, regs.gpr[29], vrRows); }
static void VRFile(HookRegs& regs, u32 original) {
    regs.gpr[5] = vrRows > vanillaPlayers ? reinterpret_cast<u32>("WifiMemberConfirm_24")
        : regs.gpr[26] + static_cast<s16>(original & 0xFFFF);
}
static void VRTeamLoop(HookRegs& regs, u32) { CompareRows(regs, regs.gpr[26], vrRows); }
static void VRLoop(HookRegs& regs, u32) { CompareRows(regs, regs.gpr[25], vrRows); }
static void VRHideRows(HookRegs& regs, u32) {
    Pages::VR* page = reinterpret_cast<Pages::VR*>(regs.gpr[24]);
    for(u32 row = vanillaPlayers; row < vrRows; ++row) page->vrControls[row].isHidden = true;
}

static void VoteControlCount(HookRegs& regs, u32) {
    voteRows = DecideRows("Vote_24");
    regs.gpr[4] = voteRows + voteFixedControls;
}
static void VoteSlot(HookRegs& regs, u32) {
    const u32 vote = regs.gpr[26];
    regs.gpr[4] = vote < vanillaPlayers ? vote : vote + voteFixedControls;
}
static void VoteLoadLoop(HookRegs& regs, u32) { CompareRows(regs, regs.gpr[26], voteRows); }
static void VoteFile(HookRegs& regs, u32 original) {
    regs.gpr[5] = voteRows > vanillaPlayers ? reinterpret_cast<u32>("Vote_24")
        : regs.gpr[28] + static_cast<s16>(original & 0xFFFF);
}
static void VoteOrderInit(HookRegs& regs, u32) {
    Pages::Vote* page = reinterpret_cast<Pages::Vote*>(regs.gpr[28]);
    for(u32 i = vanillaPlayers; i < wideRows; ++i) page->order[i] = regs.gpr[31];
}
static void VoteOrderReset(HookRegs& regs, u32) {
    Pages::Vote* page = reinterpret_cast<Pages::Vote*>(regs.gpr[29]);
    for(u32 i = vanillaPlayers; i < wideRows; ++i) page->order[i] = 0xFFFFFFFF;
}
static void VoteLoopR29(HookRegs& regs, u32) { CompareRows(regs, regs.gpr[29], voteRows); }
static void VoteLoopR28(HookRegs& regs, u32) { CompareRows(regs, regs.gpr[28], voteRows); }
static void VoteLoopR26(HookRegs& regs, u32) { CompareRows(regs, regs.gpr[26], voteRows); }

static const WordPatch words[] ={
#include <Race/Players24/Online24Pages.inc>
    { 0x805d47f0, 0x80c40c94, 0x80c40c98 },
};

static const HookPatch hooks[] ={
    { 0x8064fdf8, 0x3880000c, SelectMiiCount, false },
    { 0x80651b90, 0x924f0284, AddPlayersPastAid12, true },
    { 0x8064a820, 0x2c1d000c, VRLoadLoop, false },
    { 0x8064a7b4, 0x38ba003b, VRFile, false },
    { 0x8064a9fc, 0x2c1a000c, VRTeamLoop, false },
    { 0x8064aa8c, 0x2c19000c, VRLoop, false },
    { 0x8064a934, 0x3b800000, VRHideRows, true },
    { 0x80643248, 0x3880000f, VoteControlCount, false },
    { 0x8064332c, 0x7f44d378, VoteSlot, false },
    { 0x806433ac, 0x2c1a000c, VoteLoadLoop, false },
    { 0x80643388, 0x38bc0065, VoteFile, false },
    { 0x8064309c, 0x83e1002c, VoteOrderInit, true },
    { 0x80643468, 0x80e6d748, VoteOrderReset, true },
    { 0x80643b4c, 0x2c1d000c, VoteLoopR29, false },
    { 0x80643ef4, 0x2c1d000c, VoteLoopR29, false },
    { 0x80644304, 0x2c1c000c, VoteLoopR28, false },
    { 0x80644564, 0x2c1a000c, VoteLoopR26, false },
};

static PatchSet pagesSet("Online 24 pages", words, P24_COUNT(words), nullptr, 0, nullptr, 0, hooks, P24_COUNT(hooks),
    nullptr, 0, false, true);

void ApplyOnlinePages() {
    if(!IsSupportedDisc() || !pagesSet.Check()) {
        Debug::FatalError("VanzaKart: the online pages patches (24 players) don't match this game disc.");
        return;
    }
    pagesSet.Apply();
}

}
}
