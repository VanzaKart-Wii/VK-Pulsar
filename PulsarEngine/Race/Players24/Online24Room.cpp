#include <kamek.hpp>
#include <Race/Players24/Patches.hpp>
#include <Race/Players24/Players24.hpp>
#include <Race/Players24/Online24.hpp>
#include <Debug/Debug.hpp>

extern "C" void OSReport(const char* format, ...);

namespace Pulsar {
namespace Players24 {

static const u8 bubbleCount = 24;
static const u8 noBubble = 0xFF;
static const u32 orderOffset = 0x2c68;
static const u32 pageSize = orderOffset + bubbleCount;
static const u32 bubbleStride = 0x1a0;
static const u32 bubblesOffset = 0x1c8;
static const u32 countOffset = 0x2ae8;

static u8 bubbleOwners[bubbleCount];
static float rowOffsets[bubbleCount];

static u8 FindBubble(u8 aid, u8 slot) {
    const u8 owner = aid * 2 + slot + 1;
    for(u8 bubble = 0; bubble < bubbleCount; ++bubble) if(bubbleOwners[bubble] == owner) return bubble;
    return noBubble;
}

static u8 TakeBubble(u8 aid, u8 slot) {
    u8 bubble = FindBubble(aid, slot);
    if(bubble != noBubble) return bubble;
    const u8 preferred = aid * 2 + slot;
    if(preferred < bubbleCount && bubbleOwners[preferred] == 0) bubble = preferred;
    for(u8 i = 0; bubble == noBubble && i < bubbleCount; ++i) if(bubbleOwners[i] == 0) bubble = i;
    if(bubble == noBubble) {
        OSReport("[VK 24P] no friend room bubble for aid %d slot %d\n", aid, slot);
        return preferred < bubbleCount ? preferred : 0;
    }
    bubbleOwners[bubble] = aid * 2 + slot + 1;
    return bubble;
}

static u8 AidOfBubble(u8 bubble) {
    if(bubble < bubbleCount && bubbleOwners[bubble] != 0) return (bubbleOwners[bubble] - 1) >> 1;
    return bubble >> 1;
}

static u8 BubbleOfAid(u8 aid) { return FindBubble(aid, 0); }

static void ResetBubbles(HookRegs&, u32) {
    for(u8 bubble = 0; bubble < bubbleCount; ++bubble) {
        bubbleOwners[bubble] = 0;
        rowOffsets[bubble] = 0.0f;
    }
}

static u32 RlwinmSource(u32 word) { return (word >> 21) & 0x1F; }
static u32 RlwinmDest(u32 word) { return (word >> 16) & 0x1F; }
static void BubbleToAid(HookRegs& regs, u32 original) {
    regs.gpr[RlwinmDest(original)] = AidOfBubble(regs.gpr[RlwinmSource(original)] & 0xFF);
}
static void AidToBubble(HookRegs& regs, u32 original) {
    regs.gpr[RlwinmDest(original)] = BubbleOfAid(regs.gpr[RlwinmSource(original)] & 0xFF);
}
static void AidToBubbleControl(HookRegs& regs, u32 original) {
    const u8 aid = regs.gpr[RlwinmSource(original)] & 0xFF;
    u8 bubble = BubbleOfAid(aid);
    if(bubble == noBubble) bubble = aid * 2 < bubbleCount ? aid * 2 : 0;
    regs.gpr[RlwinmDest(original)] = bubble;
}

static void FreeBubble(HookRegs& regs, u32) {
    const u8 bubble = regs.gpr[21] & 0xFF;
    if(bubble < bubbleCount) bubbleOwners[bubble] = 0;
}

static void AidFlag(HookRegs& regs, u32) {
    if(regs.gpr[23] < vanillaPlayers) *reinterpret_cast<u8*>(regs.gpr[3] + 0x2af9) = regs.gpr[25];
}

static void PlayersOfAid(HookRegs& regs, u32 aidReg, u32 countReg) {
    const u8 aid = regs.gpr[aidReg] & 0xFF;
    if(aid < vanillaPlayers) return;
    const u8 players = PlayersAtAid(aid);
    regs.gpr[countReg] = players < 2 ? players : 2;
}
static void AddLoopPlayers(HookRegs& regs, u32) { PlayersOfAid(regs, 20, 27); }
static void MiiLoopPlayers(HookRegs& regs, u32) { PlayersOfAid(regs, 19, 24); }

static void JoinBubble(HookRegs& regs, u32) { regs.gpr[5] = TakeBubble(regs.gpr[20] & 0xFF, regs.gpr[3] & 0xFF); }
static void MiiBubble(HookRegs& regs, u32) { regs.gpr[0] = TakeBubble(regs.gpr[19] & 0xFF, regs.gpr[0] & 0xFF); }

static const float bubbleSpacing = 50.0f;
static const float secondRowOffset = -55.0f;
static void BubbleX(HookRegs& regs, u32) {
    const u32 page = regs.gpr[29];
    const s32 count = *reinterpret_cast<const s32*>(page + countOffset);
    s32 position = static_cast<s32>(regs.gpr[4]) - 1;
    s32 rowCount = count;
    float rowOffset = 0.0f;
    if(count > vanillaPlayers) {
        const s32 firstRow = (count + 1) / 2;
        if(position < firstRow) rowCount = firstRow;
        else {
            position -= firstRow;
            rowCount = count - firstRow;
            rowOffset = secondRowOffset;
        }
    }
    const float x = bubbleSpacing * (static_cast<float>(position) - 0.5f * static_cast<float>(rowCount - 1));
    *reinterpret_cast<float*>(regs.gpr[3] + 0x360) = x;
    const u32 bubble = (regs.gpr[3] - page) / bubbleStride;
    if(bubble < bubbleCount) rowOffsets[bubble] = rowOffset;
}

static const float bubbleY = 140.0f;
static void BubbleY(HookRegs& regs, u32) {
    const u32 bubble = *reinterpret_cast<const u32*>(regs.gpr[29] + 0x190);
    const float offset = bubble < bubbleCount ? rowOffsets[bubble] : 0.0f;
    *reinterpret_cast<float*>(regs.gpr[3] + 0x30) = bubbleY + offset;
}

static const WordPatch words[] ={
    { 0x806241e4, 0x38602c68, 0x38600000 | pageSize },
    { 0x805d8964, 0x88032adc, 0x88030000 | orderOffset },
    { 0x805da1e0, 0x8aa32adc, 0x8aa30000 | orderOffset },
    { 0x805da2ac, 0x88032add, 0x88030000 | (orderOffset + 1) },
    { 0x805da2b0, 0x98032adc, 0x98030000 | orderOffset },
    { 0x805da2c8, 0x9a432adb, 0x9a430000 | (orderOffset - 1) },
    { 0x805da358, 0x88032adc, 0x88030000 | orderOffset },
    { 0x805da3e0, 0x98a32adc, 0x98a30000 | orderOffset },
    { 0x805da4f8, 0x88032adc, 0x88030000 | orderOffset },
    { 0x805dac80, 0x88042adc, 0x88040000 | orderOffset },
    { 0x805da6cc, 0x2813000c, 0x28130018 },
};

static const HookPatch hooks[] ={
    { 0x805d9e54, 0x93fe2ae8, ResetBubbles, true },
    { 0x805d8968, 0x5400fe3e, BubbleToAid, false },
    { 0x805da1e8, 0x56b7fe3e, BubbleToAid, false },
    { 0x805da27c, 0x7ea4ab78, FreeBubble, true },
    { 0x805da29c, 0x9b232af9, AidFlag, false },
    { 0x805da35c, 0x5400fe3e, BubbleToAid, false },
    { 0x805da3bc, 0x56950dfc, AddLoopPlayers, true },
    { 0x805da3d0, 0x7ca3aa14, JoinBubble, false },
    { 0x805da51c, 0xd0030360, BubbleX, false },
    { 0x805da58c, 0x56750dfc, MiiLoopPlayers, true },
    { 0x805da5a0, 0x7c00aa14, MiiBubble, false },
    { 0x805dab7c, 0x57f60dfc, AidToBubbleControl, false },
    { 0x805dab84, 0x57c40e3c, AidToBubble, false },
    { 0x805dac84, 0x5400fe3e, BubbleToAid, false },
    { 0x805daff8, 0x57440e3c, AidToBubble, false },
    { 0x805d978c, 0xd0230030, BubbleY, false },
};

static PatchSet roomSet("Online 24 room", words, P24_COUNT(words), nullptr, 0, nullptr, 0, hooks, P24_COUNT(hooks),
    nullptr, 0, false, true);

void ApplyRoomBubbles() {
    if(!IsSupportedDisc() || !roomSet.Check()) {
        Debug::FatalError("VanzaKart: the friend room patches (24 players) don't match this game disc.");
        return;
    }
    roomSet.Apply();
}

}
}
