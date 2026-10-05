#include <kamek.hpp>
#include <Race/Players24/Patches.hpp>
#include <Race/Players24/Players24.hpp>

namespace Pulsar {
namespace Players24 {

static const u32 actorsOffset = 0x3fec;
static const u32 soundsOffset = 0x3f58;
static const u32 soundsPerPlayer = 3;

static u32 extraActors[maxPlayers - vanillaPlayers];
static s32 extraSounds[maxPlayers - vanillaPlayers][soundsPerPlayer];

static u32* ActorSlot(u8* mgr, u32 idx) {
    if(idx < vanillaPlayers) return reinterpret_cast<u32*>(mgr + actorsOffset + idx * 4);
    if(idx < maxPlayers) return &extraActors[idx - vanillaPlayers];
    return nullptr;
}
static s32* SoundSlot(u8* mgr, u32 idx, u32 sound) {
    if(sound >= soundsPerPlayer) return nullptr;
    if(idx < vanillaPlayers) return reinterpret_cast<s32*>(mgr + soundsOffset + idx * 0xc + sound * 4);
    if(idx < maxPlayers) return &extraSounds[idx - vanillaPlayers][sound];
    return nullptr;
}

static void SetActor(u8* mgr, u32 idx, u32 actor) {
    u32* slot = ActorSlot(mgr, idx & 0xFF);
    if(slot != nullptr) *slot = actor;
}
static u32 GetActor(u8* mgr, u32 idx) {
    const u32* slot = ActorSlot(mgr, idx & 0xFF);
    return slot == nullptr ? 0 : *slot;
}
static u32 GetCurrentActor(u8* mgr) {
    if(*reinterpret_cast<const u32*>(mgr + 0x3f48) == 0) return 0;
    const s16 idx = *reinterpret_cast<const s16*>(mgr + 0x3f54);
    if(idx < 0) return 0;
    const u32* slot = ActorSlot(mgr, idx);
    return slot == nullptr ? 0 : *slot;
}
static void SetSound(u8* mgr, u32 idx, u32 sound, s32 value) {
    s32* slot = SoundSlot(mgr, idx, sound);
    if(slot != nullptr) *slot = value;
}
static s32 GetSound(u8* mgr, u32 idx, u32 sound) {
    const s32* slot = SoundSlot(mgr, idx, sound);
    return slot == nullptr ? -1 : *slot;
}

static void ResetSounds(HookRegs&, u32) {
    for(u32 i = 0; i < maxPlayers - vanillaPlayers; ++i)
        for(u32 j = 0; j < soundsPerPlayer; ++j) extraSounds[i][j] = -1;
}
static void ResetActors(HookRegs&, u32) {
    for(u32 i = 0; i < maxPlayers - vanillaPlayers; ++i) extraActors[i] = 0;
}

typedef void (*ActorFunc)(u32 actor);
static void UpdateExtraActors(HookRegs&, u32) {
    for(u32 i = 0; i < maxPlayers - vanillaPlayers; ++i)
        if(extraActors[i] != 0) reinterpret_cast<ActorFunc>(Port(0x808646c4))(extraActors[i]);
}

struct ActorPosition { u32 words[3]; float value; };
typedef void (*ListenerFunc)(u8* mgr, ActorPosition* position);
typedef void (*ActorPositionFunc)(u32 actor, ActorPosition* position);
static void ListenExtraActors(HookRegs& regs, u32) {
    if(regs.gpr[30] != vanillaPlayers) return;
    u8* mgr = reinterpret_cast<u8*>(regs.gpr[29]);
    for(u32 i = 0; i < maxPlayers - vanillaPlayers; ++i) {
        const u32 actor = extraActors[i];
        if(actor == 0) continue;
        const u8* fields = reinterpret_cast<const u8*>(actor);
        if(fields[0x6fb] == 0) continue;
        ActorPosition position;
        for(u32 w = 0; w < 3; ++w) position.words[w] = *reinterpret_cast<const u32*>(fields + 0x120 + w * 4);
        position.value = *reinterpret_cast<const float*>(fields + 0x12c);
        if(fields[0x6fa] != 0) return;
        reinterpret_cast<ListenerFunc>(Port(0x80869488))(mgr, &position);
        reinterpret_cast<ActorPositionFunc>(Port(0x80866198))(actor, &position);
    }
}

static void VoiceOffsetIndex(HookRegs& regs, u32) {
    regs.gpr[0] = (regs.gpr[0] & 0xFF) % vanillaPlayers * 4;
}

static const HookPatch hooks[] ={
    { 0x80863f94, 0x5400103a, VoiceOffsetIndex, false },
    { 0x808682d8, 0x7fe3fb78, ResetSounds, true },
    { 0x808683fc, 0x90833fec, ResetActors, true },
    { 0x80868554, 0x38000000, ResetActors, true },
    { 0x80868608, 0x807d3fe8, UpdateExtraActors, true },
    { 0x80869788, 0x80010034, ListenExtraActors, true },
};

static void* unusedTrampolines[5];
static const FuncHook funcs[] ={
    { 0x80869098, 0x548015ba, reinterpret_cast<const void*>(SetActor), &unusedTrampolines[0] },
    { 0x808690a8, 0x548015ba, reinterpret_cast<const void*>(GetActor), &unusedTrampolines[1] },
    { 0x808690b8, 0x80033f48, reinterpret_cast<const void*>(GetCurrentActor), &unusedTrampolines[2] },
    { 0x8086906c, 0x1c84000c, reinterpret_cast<const void*>(SetSound), &unusedTrampolines[3] },
    { 0x80869054, 0x1c84000c, reinterpret_cast<const void*>(GetSound), &unusedTrampolines[4] },
};

static PatchSet audioSet("Audio 24", nullptr, 0, nullptr, 0, nullptr, 0, hooks, P24_COUNT(hooks), funcs,
    P24_COUNT(funcs), true);

}
}
