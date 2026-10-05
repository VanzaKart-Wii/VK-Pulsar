#ifndef _PUL_PLAYERS24_PATCHES_
#define _PUL_PLAYERS24_PATCHES_
#include <kamek.hpp>

namespace Pulsar {
namespace Players24 {

bool IsSupportedDisc();
u32 Port(u32 palAddress);
bool SameInstruction(u32 palAddress, u32 palWord, u32 address, u32 word, bool dataHalf);
inline u32 Read32(u32 address) { return *reinterpret_cast<const u32*>(address); }

struct WordPatch {
    u32 address;
    u32 original;
    u32 value;
};

struct CallPatch {
    u32 address;
    u32 original;
    const void* target;
    bool isJump;
};

struct HookRegs {
    u32 gpr[32];
    u32 cr;
    u32 ctr;
    u32 xer;
    u32 lr;
};
typedef void (*HookFunc)(HookRegs& regs, u32 original);

struct HookPatch {
    u32 address;
    u32 original;
    HookFunc handler;
    bool runOriginal;
};

struct FuncHook {
    u32 address;
    u32 original;
    const void* replacement;
    void** trampoline;
};

class PatchSet {
public:
    PatchSet(const char* name, const WordPatch* words, u32 wordCount, const CallPatch* calls, u32 callCount,
        const u32* emptyInstances, u32 emptyInstanceCount, const HookPatch* hooks = nullptr, u32 hookCount = 0,
        const FuncHook* funcs = nullptr, u32 funcCount = 0, bool optional = false, bool standalone = false);
    bool Check() const;
    void Apply() const;
    static bool CheckOverlaps(const u32* extra, u32 extraCount);

    static PatchSet* first;
    PatchSet* next;
    const char* name;
    const WordPatch* words;
    u32 wordCount;
    const CallPatch* calls;
    u32 callCount;
    const u32* emptyInstances;
    u32 emptyInstanceCount;
    const HookPatch* hooks;
    u32 hookCount;
    const FuncHook* funcs;
    u32 funcCount;
    bool optional;
    bool standalone;
    mutable bool skipped;
};

size_assert(WordPatch, 0xc);
size_assert(CallPatch, 0x10);
size_assert(HookPatch, 0x10);
size_assert(FuncHook, 0x10);

#define P24_COUNT(array) (sizeof(array) / sizeof(array[0]))

void* AllocZeroed(u32 size);

}
}
#endif
