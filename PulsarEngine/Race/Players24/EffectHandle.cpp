#include <kamek.hpp>
#include <Race/Players24/Players24.hpp>
#include <Race/Players24/Patches.hpp>

namespace Pulsar {
namespace Players24 {

static const u32 emptyHandle[2] ={ 0, 0 };
static void CheckEffectHandle(HookRegs& regs, u32) {
    if(regs.gpr[3] < 0x80000000) regs.gpr[3] = reinterpret_cast<u32>(emptyHandle);
}

static const HookPatch hooks[] ={
    { 0x800375f0, 0x80830004, CheckEffectHandle, true },
};
static PatchSet effectHandleSet("Effect handle", nullptr, 0, nullptr, 0, nullptr, 0, hooks, P24_COUNT(hooks),
    nullptr, 0, true);

}
}
