#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <Race/Players24/Patches.hpp>

#include <core/rvl/OS/OSCache.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/egg/mem/Heap.hpp>

extern "C" void OSReport(const char* format, ...);
extern "C" void ICInvalidateRange(void* addr, u32 size);

namespace Pulsar {
namespace Players24 {

PatchSet* PatchSet::first = nullptr;

PatchSet::PatchSet(const char* name, const WordPatch* words, u32 wordCount, const CallPatch* calls, u32 callCount,
    const u32* emptyInstances, u32 emptyInstanceCount, const HookPatch* hooks, u32 hookCount,
    const FuncHook* funcs, u32 funcCount, bool optional, bool standalone)
    : name(name), words(words), wordCount(wordCount), calls(calls), callCount(callCount),
    emptyInstances(emptyInstances), emptyInstanceCount(emptyInstanceCount), hooks(hooks), hookCount(hookCount),
    funcs(funcs), funcCount(funcCount), optional(optional), standalone(standalone), skipped(false) {
    this->next = first;
    first = this;
}

static bool InBranchRange(u32 from, u32 to) {
    const s32 delta = (s32)to - (s32)from;
    return delta >= -0x02000000 && delta <= 0x01FFFFFC;
}

static const u32 stubWords = 16;
static const u32 maxHooks = 160;
static const u32 commonWords = 64;
static u32 hookCommon[commonWords];
static bool isHookCommonBuilt = false;
static u32 hookPool[maxHooks * stubWords];
static u32 hookPoolUsed = 0;
static const u32 maxTrampolines = 32;
static u32 trampolinePool[maxTrampolines * 2];
static u32 trampolinesUsed = 0;

static inline u32 DForm(u32 op, u32 d, u32 a, s32 imm) { return (op << 26) | (d << 21) | (a << 16) | ((u32)imm & 0xFFFF); }
static inline u32 Stw(u32 s, s32 d, u32 a) { return DForm(36, s, a, d); }
static inline u32 Lwz(u32 d, s32 off, u32 a) { return DForm(32, d, a, off); }
static inline u32 Addi(u32 d, u32 a, s32 imm) { return DForm(14, d, a, imm); }
static inline u32 MfSpr(u32 d, u32 spr) { return 0x7C0002A6 | (d << 21) | ((spr & 0x1F) << 16) | ((spr >> 5) << 11); }
static inline u32 MtSpr(u32 spr, u32 s) { return 0x7C0003A6 | (s << 21) | ((spr & 0x1F) << 16) | ((spr >> 5) << 11); }
static inline u32 Branch(u32 from, u32 to, bool link) { return 0x48000000 | ((to - from) & 0x03FFFFFC) | (link ? 1 : 0); }
static inline bool IsBranch(u32 word) { return (word >> 26) == 18; }
static inline bool IsConditionalBranch(u32 word) { const u32 op = word >> 26; return op == 16 || op == 19; }
static inline u32 BranchTarget(u32 at, u32 word) {
    s32 disp = word & 0x03FFFFFC;
    if(disp & 0x02000000) disp -= 0x04000000;
    return (word & 2) ? (u32)disp : at + disp;
}

struct RegionRange {
    u32 start;
    u32 end;
    s32 delta;
};
#include <Race/Players24/RegionMap.inc>

bool IsSupportedDisc() {
    const u32 id = Read32(0x80000000);
    if((id >> 8) != 0x524D43) return false;
    const char region = id & 0xFF;
    return region == 'P' || region == 'E' || region == 'J' || region == 'K';
}

u32 Port(u32 palAddress) {
    const RegionRange* ranges;
    u32 count;
    switch(Read32(0x80000000) & 0xFF) {
        case 'P': return palAddress;
        case 'E': ranges = rangesE; count = P24_COUNT(rangesE); break;
        case 'J': ranges = rangesJ; count = P24_COUNT(rangesJ); break;
        case 'K': ranges = rangesK; count = P24_COUNT(rangesK); break;
        default: return 0;
    }
    u32 low = 0;
    u32 high = count;
    while(low < high) {
        const u32 mid = (low + high) / 2;
        if(ranges[mid].end < palAddress) low = mid + 1;
        else high = mid;
    }
    if(low == count || ranges[low].start > palAddress) return 0;
    return palAddress + ranges[low].delta;
}

static inline bool IsDForm(u32 word) {
    const u32 op = word >> 26;
    return (op >= 7 && op <= 15 && op != 11 && op != 10) || (op >= 24 && op <= 29) || (op >= 32 && op <= 55);
}

bool SameInstruction(u32 palAddress, u32 palWord, u32 address, u32 word, bool dataHalf) {
    if(word == palWord) return true;
    if(IsBranch(palWord) && IsBranch(word) && (palWord & 3) == (word & 3)) {
        const u32 target = Port(BranchTarget(palAddress, palWord));
        return target != 0 && target == BranchTarget(address, word);
    }
    return dataHalf && IsDForm(palWord) && (palWord & 0xFFFF0000) == (word & 0xFFFF0000);
}

static const u32 hookFrame = 0x120;
static void BuildHookCommon() {
    u32* code = hookCommon;
    u32 i = 0;
    code[i++] = Stw(2, 0x18, 1);
    code[i++] = DForm(47, 3, 1, 0x1C);
    code[i++] = Addi(0, 1, hookFrame);
    code[i++] = Stw(0, 0x14, 1);
    code[i++] = 0x7C000026;
    code[i++] = Stw(0, 0x90, 1);
    code[i++] = MfSpr(0, 9);
    code[i++] = Stw(0, 0x94, 1);
    code[i++] = MfSpr(0, 1);
    code[i++] = Stw(0, 0x98, 1);
    for(u32 f = 0; f < 14; ++f) code[i++] = DForm(54, f, 1, 0xA0 + f * 8);
    code[i++] = MfSpr(5, 8);
    code[i++] = Stw(5, 0x110, 1);
    code[i++] = Lwz(12, 0, 5);
    code[i++] = Lwz(4, 4, 5);
    code[i++] = MtSpr(9, 12);
    code[i++] = Addi(3, 1, 0x10);
    code[i++] = 0x4E800421;
    for(u32 f = 0; f < 14; ++f) code[i++] = DForm(50, f, 1, 0xA0 + f * 8);
    code[i++] = Lwz(0, 0x94, 1);
    code[i++] = MtSpr(9, 0);
    code[i++] = Lwz(0, 0x98, 1);
    code[i++] = MtSpr(1, 0);
    code[i++] = Lwz(0, 0x90, 1);
    code[i++] = 0x7C0FF120;
    code[i++] = Lwz(5, 0x110, 1);
    code[i++] = Addi(0, 5, 8);
    code[i++] = MtSpr(8, 0);
    code[i++] = DForm(46, 3, 1, 0x1C);
    code[i++] = Lwz(2, 0x18, 1);
    code[i++] = 0x4E800020;
    OS::DCFlushRange(code, commonWords * 4);
    ICInvalidateRange(code, commonWords * 4);
    isHookCommonBuilt = true;
}

static u32* BuildHookStub(const HookPatch& hook, u32 address, u32 original) {
    if(!isHookCommonBuilt) BuildHookCommon();
    u32* stub = &hookPool[hookPoolUsed * stubWords];
    ++hookPoolUsed;
    u32 i = 0;
    stub[i++] = DForm(37, 1, 1, -(s32)hookFrame);
    stub[i++] = Stw(0, 0x10, 1);
    stub[i++] = MfSpr(0, 8);
    stub[i++] = Stw(0, 0x9C, 1);
    stub[i] = Branch((u32)&stub[i], (u32)hookCommon, true); ++i;
    stub[i++] = (u32)hook.handler;
    stub[i++] = original;
    stub[i++] = Lwz(0, 0x9C, 1);
    stub[i++] = MtSpr(8, 0);
    stub[i++] = Lwz(0, 0x10, 1);
    stub[i++] = Addi(1, 1, hookFrame);
    if(!hook.runOriginal) stub[i] = 0x60000000;
    else if(IsBranch(original)) stub[i] = Branch((u32)&stub[i], BranchTarget(address, original), original & 1);
    else stub[i] = original;
    ++i;
    stub[i] = Branch((u32)&stub[i], address + 4, false);
    return stub;
}

static bool Unported(const char* name, u32 palAddress) {
    OSReport("[VK 24P] %s: no address for %08x on this disc\n", name, palAddress);
    return false;
}

bool PatchSet::Check() const {
    for(u32 i = 0; i < this->wordCount; ++i) {
        const WordPatch& patch = this->words[i];
        const u32 address = Port(patch.address);
        if(address == 0) return Unported(this->name, patch.address);
        const u32 word = Read32(address);
        if(word != patch.original) {
            OSReport("[VK 24P] %s: unexpected instruction %08x at %08x\n", this->name, word, address);
            return false;
        }
    }
    for(u32 i = 0; i < this->callCount; ++i) {
        const CallPatch& call = this->calls[i];
        const u32 address = Port(call.address);
        if(address == 0) return Unported(this->name, call.address);
        const u32 word = Read32(address);
        if(!SameInstruction(call.address, call.original, address, word, false) || !InBranchRange(address, (u32)call.target)) {
            OSReport("[VK 24P] %s: unexpected instruction %08x at %08x\n", this->name, word, address);
            return false;
        }
    }
    for(u32 i = 0; i < this->emptyInstanceCount; ++i) {
        const u32 instance = Port(this->emptyInstances[i]);
        if(instance == 0) return Unported(this->name, this->emptyInstances[i]);
        if(*reinterpret_cast<void* const*>(instance) != nullptr) {
            OSReport("[VK 24P] %s: instance at %08x already created\n", this->name, instance);
            return false;
        }
    }
    static u32 hooksNeeded = 0;
    static u32 trampolinesNeeded = 0;
    for(u32 i = 0; i < this->hookCount; ++i) {
        const HookPatch& hook = this->hooks[i];
        const u32 address = Port(hook.address);
        if(address == 0) return Unported(this->name, hook.address);
        const u32 word = Read32(address);
        const bool badOriginal = hook.runOriginal && IsConditionalBranch(word);
        if(!SameInstruction(hook.address, hook.original, address, word, true) || badOriginal
            || !InBranchRange(address, (u32)hookPool) || !InBranchRange((u32)hookPool, (u32)hook.handler)
            || ++hooksNeeded > maxHooks) {
            OSReport("[VK 24P] %s: cannot hook %08x (found %08x)\n", this->name, address, word);
            return false;
        }
    }
    for(u32 i = 0; i < this->funcCount; ++i) {
        const FuncHook& func = this->funcs[i];
        const u32 address = Port(func.address);
        if(address == 0) return Unported(this->name, func.address);
        const u32 word = Read32(address);
        if(!SameInstruction(func.address, func.original, address, word, true) || IsBranch(word)
            || IsConditionalBranch(word) || !InBranchRange(address, (u32)func.replacement)
            || ++trampolinesNeeded > maxTrampolines) {
            OSReport("[VK 24P] %s: cannot replace %08x (found %08x)\n", this->name, address, word);
            return false;
        }
    }
    return true;
}

static const u32 maxAddresses = 4096;
static u32* allAddresses = nullptr;
static u32 addressCount = 0;
static bool AddAddress(u32 address) {
    if(addressCount >= maxAddresses) return false;
    u32 i = addressCount++;
    while(i > 0 && allAddresses[i - 1] > address) {
        allAddresses[i] = allAddresses[i - 1];
        --i;
    }
    allAddresses[i] = address;
    return true;
}

bool PatchSet::CheckOverlaps(const u32* extra, u32 extraCount) {
    EGG::Heap* heap = RKSystem::mInstance.EGGSystem;
    allAddresses = nullptr;
    if(heap != nullptr) allAddresses = static_cast<u32*>(EGG::Heap::alloc(maxAddresses * sizeof(u32), -4, heap));
    if(allAddresses == nullptr) {
        OSReport("[VK 24P] no memory to check the patched addresses\n");
        return false;
    }
    addressCount = 0;
    bool ok = true;
    for(u32 i = 0; i < extraCount; ++i) ok &= AddAddress(extra[i]);
    for(const PatchSet* set = first; set != nullptr; set = set->next) {
        for(u32 i = 0; i < set->wordCount; ++i) ok &= AddAddress(set->words[i].address);
        for(u32 i = 0; i < set->callCount; ++i) ok &= AddAddress(set->calls[i].address);
        for(u32 i = 0; i < set->hookCount; ++i) ok &= AddAddress(set->hooks[i].address);
        for(u32 i = 0; i < set->funcCount; ++i) ok &= AddAddress(set->funcs[i].address);
    }
    if(!ok) OSReport("[VK 24P] too many patched addresses to check\n");
    for(u32 i = 1; ok && i < addressCount; ++i) {
        if(allAddresses[i] == allAddresses[i - 1]) {
            OSReport("[VK 24P] %08x is patched twice\n", allAddresses[i]);
            ok = false;
        }
    }
    EGG::Heap::free(allAddresses, heap);
    allAddresses = nullptr;
    return ok;
}

void PatchSet::Apply() const {
    for(u32 i = 0; i < this->wordCount; ++i) KamekRuntimeWrite::Write32(Port(this->words[i].address), this->words[i].value);
    for(u32 i = 0; i < this->callCount; ++i) {
        KamekRuntimeWrite::Branch(Port(this->calls[i].address), (u32)this->calls[i].target, !this->calls[i].isJump);
    }
    for(u32 i = 0; i < this->hookCount; ++i) {
        const u32 address = Port(this->hooks[i].address);
        u32* stub = BuildHookStub(this->hooks[i], address, Read32(address));
        OS::DCFlushRange(stub, stubWords * 4);
        ICInvalidateRange(stub, stubWords * 4);
        KamekRuntimeWrite::Branch(address, (u32)stub, false);
    }
    for(u32 i = 0; i < this->funcCount; ++i) {
        const FuncHook& func = this->funcs[i];
        const u32 address = Port(func.address);
        u32* trampoline = &trampolinePool[trampolinesUsed * 2];
        ++trampolinesUsed;
        trampoline[0] = Read32(address);
        trampoline[1] = Branch((u32)&trampoline[1], address + 4, false);
        OS::DCFlushRange(trampoline, 8);
        ICInvalidateRange(trampoline, 8);
        *func.trampoline = trampoline;
        KamekRuntimeWrite::Branch(address, (u32)func.replacement, false);
    }
}

void* AllocZeroed(u32 size) {
    void* object = ::operator new(size);
    if(object != nullptr) memset(object, 0, size);
    return object;
}

}
}
