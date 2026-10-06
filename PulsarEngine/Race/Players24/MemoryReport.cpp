#include <kamek.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Section/Section.hpp>
#include <MarioKartWii/Mii/MiiGroup.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <Race/Players24/Players24.hpp>
#include <Race/Players24/Patches.hpp>

extern "C" void OSReport(const char* format, ...);

namespace Pulsar {
namespace Players24 {

struct BlockHead {
    u16 signature;
    u16 attribute;
    u32 size;
    BlockHead* prev;
    BlockHead* next;
};

static bool IsMem(u32 address) {
    return (address >= 0x80000000 && address < 0x81800000) || (address >= 0x90000000 && address < 0x94000000);
}

static const u8* MemHead(const void* heap) {
    if(heap == nullptr) return nullptr;
    const u8* head = *reinterpret_cast<u8* const*>(reinterpret_cast<const u8*>(heap) + 0x10);
    if(!IsMem((u32)head)) return nullptr;
    return head;
}

static void GetFreeSpace(const u8* head, u32& total, u32& largest) {
    total = 0;
    largest = 0;
    if(head == nullptr) return;
    const BlockHead* block = *reinterpret_cast<BlockHead* const*>(head + 0x3c);
    for(u32 count = 0; block != nullptr && IsMem((u32)block) && count < 4096; block = block->next, ++count) {
        total += block->size;
        if(block->size > largest) largest = block->size;
    }
}

static void ReportHead(const char* name, const u8* head) {
    u32 total, largest;
    GetFreeSpace(head, total, largest);
    OSReport("[VK 24P]   %s %08x: %d KiB free, largest %d KiB\n", name, (u32)head, total >> 10, largest >> 10);
}
static void ReportHeap(const char* name, const void* heap) { ReportHead(name, MemHead(heap)); }

static void ReportUsedBlocks(const u8* head) {
    if(head == nullptr) return;
    static u32 sizes[512];
    static u32 counts[512];
    u32 kinds = 0;
    u32 total = 0;
    u32 blocks = 0;
    const BlockHead* block = *reinterpret_cast<BlockHead* const*>(head + 0x44);
    for(; block != nullptr && IsMem((u32)block) && blocks < 65536; block = block->next, ++blocks) {
        total += block->size;
        u32 i = 0;
        while(i < kinds && sizes[i] != block->size) ++i;
        if(i == kinds) {
            if(kinds == 512) continue;
            sizes[kinds] = block->size;
            counts[kinds++] = 0;
        }
        ++counts[i];
    }
    OSReport("[VK 24P]   used: %d blocks, %d KiB; by size (bytes x count):\n", blocks, total >> 10);
    for(u32 shown = 0; shown < 16 && kinds > 0; ++shown) {
        u32 best = 0;
        for(u32 i = 1; i < kinds; ++i) if(sizes[i] * counts[i] > sizes[best] * counts[best]) best = i;
        OSReport("[VK 24P]     0x%x x %d = %d KiB\n", sizes[best], counts[best], (sizes[best] * counts[best]) >> 10);
        sizes[best] = sizes[--kinds];
        counts[best] = counts[kinds];
    }
}

static void GetCallers(u32 sp, u32* callers, int count) {
    for(int i = 0; i < count; ++i) callers[i] = 0;
    u32 frame = sp;
    for(int i = 0; i < count && IsMem(frame); ++i) {
        frame = *reinterpret_cast<u32*>(frame);
        if(!IsMem(frame)) break;
        callers[i] = *reinterpret_cast<u32*>(frame + 4);
    }
}

#ifdef P24TEST
struct AllocRecord { u32 block; u32 size; u32 callers[5]; };
static AllocRecord allocRecords[4096];
static AllocRecord smallRecords[256];
static AllocRecord bigRecords[512];
static u32 smallCount = 0;
static u32 bigCount = 0;
static void Fill(AllocRecord& record, u32 block, u32 size, u32 sp) {
    record.block = block;
    record.size = size;
    GetCallers(sp, record.callers, 5);
}
static void RecordAlloc(u32 block, u32 size, u32 sp) {
    Fill(allocRecords[(block >> 4) & 4095], block, size, sp);
    if(size <= 0x30) Fill(smallRecords[smallCount++ & 255], block, size, sp);
    if(size >= 0x400) Fill(bigRecords[bigCount++ & 511], block, size, sp);
}
static void ReportRecord(const AllocRecord& record) {
    OSReport("[VK 24P]     block %08x, 0x%x bytes, by %08x %08x %08x %08x %08x\n", record.block, record.size,
        record.callers[0], record.callers[1], record.callers[2], record.callers[3], record.callers[4]);
}
static bool ReportAllocOf(u32 data) {
    if(allocRecords[(data >> 4) & 4095].block == data) { ReportRecord(allocRecords[(data >> 4) & 4095]); return true; }
    for(u32 i = 0; i < 256; ++i) if(smallRecords[i].block == data) { ReportRecord(smallRecords[i]); return true; }
    for(u32 i = 0; i < 512; ++i) if(bigRecords[i].block == data) { ReportRecord(bigRecords[i]); return true; }
    OSReport("[VK 24P]     allocation not recorded\n");
    return false;
}
static void ReportOwner(u32 pointer) {
    for(u32 i = 0; i < 512; ++i) {
        const AllocRecord& record = bigRecords[i];
        if(record.block != 0 && pointer >= record.block && pointer < record.block + record.size) {
            OSReport("[VK 24P]   %08x is inside:\n", pointer);
            ReportRecord(record);
            return;
        }
    }
    const AllocRecord& record = allocRecords[(pointer >> 4) & 4095];
    if(record.block == pointer) {
        OSReport("[VK 24P]   %08x is:\n", pointer);
        ReportRecord(record);
        return;
    }
    OSReport("[VK 24P]   %08x: owner not recorded\n", pointer);
}
static void ReportBlock(const char* what, const BlockHead* block) {
    const u32* words = reinterpret_cast<const u32*>(block);
    OSReport("[VK 24P]   %s %08x: header %08x %08x %08x %08x\n", what, (u32)block, words[0], words[1], words[2], words[3]);
    ReportAllocOf((u32)block + 0x10);
}
static void CheckUsedBlocks(HookRegs& regs, u32) {
    const u8* head = reinterpret_cast<const u8*>(regs.gpr[3]);
    if(!IsMem((u32)head)) return;
    const BlockHead* block = *reinterpret_cast<BlockHead* const*>(head + 0x44);
    const BlockHead* previous = nullptr;
    static const BlockHead* visited[4096];
    u32 count = 0;
    for(; block != nullptr; previous = block, block = block->next) {
        const bool isValid = IsMem((u32)block) && ((u32)block & 3) == 0 && block->signature == 0x5544;
        if(isValid) {
            if(count < 4096) visited[count++] = block;
            continue;
        }
        OSReport("[VK 24P] heap %08x: bad link %08x after %d good blocks\n", (u32)head, (u32)block, count);
        const BlockHead* suspects[2] ={ previous, 0 };
        if(IsMem((u32)block) && ((u32)block & 3) == 0) suspects[1] = block;
        for(int s = 0; s < 2; ++s) {
            const BlockHead* suspect = suspects[s];
            if(suspect == nullptr) continue;
            ReportBlock(s == 0 ? "previous in list" : "linked block", suspect);
            const u32* words = reinterpret_cast<const u32*>(suspect);
            for(int w = 0; w < 4; ++w) if(IsMem(words[w]) && words[w] != (u32)suspect->prev) ReportOwner(words[w]);
            const BlockHead* below = nullptr;
            for(u32 i = 0; i < count; ++i) {
                if(visited[i] < suspect && (below == nullptr || visited[i] > below)) below = visited[i];
            }
            if(below != nullptr) {
                ReportBlock("  block below it", below);
                const u32* tail = reinterpret_cast<const u32*>(suspect) - 4;
                OSReport("[VK 24P]       ends at %08x, last words %08x %08x %08x %08x\n", (u32)below + 0x10 + below->size,
                    tail[0], tail[1], tail[2], tail[3]);
            }
        }
        return;
    }
}
static void CheckQR2Record(HookRegs& regs, u32) {
    u32 record = regs.gpr[3];
    if(record == 0) record = *reinterpret_cast<const u32*>(regs.gpr[13] - 0x76e0);
    const u32 thread = *reinterpret_cast<const u32*>(0x800000E4);
    const u32 stackBase = IsMem(thread) ? *reinterpret_cast<const u32*>(thread + 0x304) : 0;
    const u32 stackEnd = IsMem(thread) ? *reinterpret_cast<const u32*>(thread + 0x308) : 0;
    OSReport("[VK 24P] qr2 record %08x, sp %08x, thread %08x stack %08x-%08x (%d bytes left)\n", record, regs.gpr[1],
        thread, stackEnd, stackBase, regs.gpr[1] - stackEnd);
    if(!IsMem(record)) return;
    const u32* words = reinterpret_cast<const u32*>(record);
    for(int i = 0xb0 / 4; i < 0x110 / 4; i += 8) {
        OSReport("[VK 24P]   +%03x %08x %08x %08x %08x %08x %08x %08x %08x\n", i * 4, words[i], words[i + 1],
            words[i + 2], words[i + 3], words[i + 4], words[i + 5], words[i + 6], words[i + 7]);
    }
    ReportOwner(record);
}
#endif

static const u8* lastDetailedHeap = nullptr;
static const u8* watchedMem1 = nullptr;
static u32 lowestMem1 = 0xFFFFFFFF;
struct BigAlloc { u32 block; u32 size; u32 callers[8]; };
static const u32 bigAllocCount = 64;
static BigAlloc bigAllocs[bigAllocCount];
static u32 bigAllocNext = 0;
static bool IsReturnAddress(u32 address) {
    if(address < 0x80004004 || address >= 0x80900000 || (address & 3) != 0) return false;
    const u32 previous = *reinterpret_cast<const u32*>(address - 4);
    if((previous & 0xFC000003) == 0x48000001) return true;
    return previous == 0x4E800421 || previous == 0x4E800021;
}
static void RecordBigAlloc(u32 block, u32 size, u32 sp) {
    BigAlloc& record = bigAllocs[bigAllocNext++ % bigAllocCount];
    record.block = block;
    record.size = size;
    GetCallers(sp, record.callers, 8);
    if(size < 0x40000) return;
    u32 found[12];
    u32 count = 0;
    for(u32 offset = 0; offset < 0x800 && count < 12; offset += 4) {
        const u32 word = *reinterpret_cast<const u32*>(sp + offset);
        if(IsReturnAddress(word)) found[count++] = word;
    }
    for(u32 i = count; i < 12; ++i) found[i] = 0;
    OSReport("[VK 24P] big block %08x %d KiB, stack %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n", block,
        size >> 10, found[0], found[1], found[2], found[3], found[4], found[5], found[6], found[7], found[8], found[9],
        found[10], found[11]);
}
static void ReportBigBlocks(const char* what, const u8* head) {
    if(head == nullptr) return;
    const BlockHead* block = *reinterpret_cast<BlockHead* const*>(head + 0x44);
    for(u32 count = 0; block != nullptr && IsMem((u32)block) && count < 65536; block = block->next, ++count) {
        if(block->size < 0x10000) continue;
        const u32 data = (u32)block + 0x10;
        const BigAlloc* found = nullptr;
        for(u32 i = 0; i < bigAllocCount; ++i) if(bigAllocs[i].block == data) found = &bigAllocs[i];
        if(found != nullptr) {
            OSReport("[VK 24P]   %s block %08x %d KiB, by %08x %08x %08x %08x %08x %08x %08x %08x\n", what, data,
                block->size >> 10, found->callers[0], found->callers[1], found->callers[2], found->callers[3],
                found->callers[4], found->callers[5], found->callers[6], found->callers[7]);
        }
        else OSReport("[VK 24P]   %s block %08x %d KiB, allocated before the report started\n", what, data, block->size >> 10);
    }
}

static void CheckAllocResult(HookRegs& regs, u32) {
#ifdef P24TEST
    if(regs.gpr[31] != 0) RecordAlloc(regs.gpr[31], regs.gpr[30], regs.gpr[1]);
#endif
    if(regs.gpr[31] != 0 && regs.gpr[30] >= 0x10000) RecordBigAlloc(regs.gpr[31], regs.gpr[30], regs.gpr[1]);
    if(regs.gpr[31] != 0 && regs.gpr[29] == (u32)watchedMem1) {
        u32 total, largest;
        GetFreeSpace(watchedMem1, total, largest);
        if(total < lowestMem1) lowestMem1 = total;
    }
    if(regs.gpr[31] != 0) return;
    u32 callers[5];
    GetCallers(regs.gpr[1], callers, 5);
    OSReport("[VK 24P] allocation failed: 0x%x bytes, callers %08x %08x %08x %08x %08x\n", regs.gpr[30],
        callers[0], callers[1], callers[2], callers[3], callers[4]);
    const u8* head = reinterpret_cast<const u8*>(regs.gpr[29]);
    ReportHead("failing heap", head);
    if(head != lastDetailedHeap) {
        lastDetailedHeap = head;
        ReportUsedBlocks(head);
        ReportBigBlocks("failing heap", head);
    }
}
static const u32 emptyHandle[2] ={ 0, 0 };
static void CheckEffectHandle(HookRegs& regs, u32) {
    if(regs.gpr[3] >= 0x80000000) return;
    static u32 reported[8];
    static u32 reportedCount = 0;
    u32 callers[5];
    GetCallers(regs.gpr[1], callers, 5);
    bool isNew = true;
    for(u32 i = 0; i < reportedCount; ++i) if(reported[i] == callers[0]) isNew = false;
    if(isNew && reportedCount < 8) {
        reported[reportedCount++] = callers[0];
        OSReport("[VK 24P] null effect handle %08x, lr %08x, callers %08x %08x %08x %08x %08x\n", regs.gpr[3], regs.lr,
            callers[0], callers[1], callers[2], callers[3], callers[4]);
    }
    regs.gpr[3] = reinterpret_cast<u32>(emptyHandle);
}

static const HookPatch allocHooks[] ={
    { 0x80198e18, 0x7fe3fb78, CheckAllocResult, true },
    { 0x800375f0, 0x80830004, CheckEffectHandle, true },
#ifdef P24TEST
    { 0x801992a8, 0x9421ffe0, CheckUsedBlocks, true },
    { 0x800d879c, 0x48037f85, CheckQR2Record, true },
#endif
};
static PatchSet memorySet("Memory report", nullptr, 0, nullptr, 0, nullptr, 0, allocHooks, P24_COUNT(allocHooks),
    nullptr, 0, true);

static void ReportSceneMemory() {
    if(!IsReady() || !HasExtraRacers()) return;
    const GameScene* scene = GameScene::GetCurrent();
    if(scene == nullptr) return;
    u32 total1, largest1, total2, largest2;
    GetFreeSpace(MemHead(scene->structsHeaps.heaps[0]), total1, largest1);
    GetFreeSpace(MemHead(scene->structsHeaps.heaps[1]), total2, largest2);
    OSReport("[VK 24P] race load: free MEM1 %d KiB, MEM2 %d KiB\n", total1 >> 10, total2 >> 10);
}
static RaceLoadHook raceMemoryReport(ReportSceneMemory);

static void ReportSectionMemory() {
    const GameScene* scene = GameScene::GetCurrent();
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if(scene == nullptr || sectionMgr == nullptr || sectionMgr->curSection == nullptr) return;
    const u8* mem1 = MemHead(scene->structsHeaps.heaps[0]);
    u32 total1, largest1, total2, largest2;
    GetFreeSpace(mem1, total1, largest1);
    GetFreeSpace(MemHead(scene->structsHeaps.heaps[1]), total2, largest2);
    const s32 lowest = lowestMem1 == 0xFFFFFFFF ? -1 : static_cast<s32>(lowestMem1 >> 10);
    OSReport("[VK 24P] section 0x%x: MEM1 heap %08x %d KiB free (largest %d), lowest since the last line %d KiB; MEM2 %d KiB free\n",
        sectionMgr->curSection->sectionId, (u32)mem1, total1 >> 10, largest1 >> 10, lowest, total2 >> 10);
    ReportBigBlocks("MEM1", mem1);
    watchedMem1 = mem1;
    lowestMem1 = total1;
}
static SectionLoadHook sectionMemoryReport(ReportSectionMemory);

#ifdef P24TEST
static bool IsPhysicalRAM(u32 address) {
    return address < 0x01800000 || (address >= 0x10000000 && address < 0x14000000);
}
static void CheckMiiTextures(const char* when) {
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if(sectionMgr == nullptr || sectionMgr->sectionParams == nullptr) return;
    const MiiGroup& group = sectionMgr->sectionParams->playerMiis;
    u32 bad = 0;
    for(u32 array = 0; array < 7; ++array) {
        const MiiTexObj* textures = group.texObj[array];
        if(textures == nullptr) continue;
        for(u32 slot = 0; slot < group.miiCount && slot < 32; ++slot) {
            const u32* words = reinterpret_cast<const u32*>(&textures[slot]);
            const u32 image = (words[3] & 0x00FFFFFF) << 5;
            const bool hasMii = reinterpret_cast<const u8*>(&textures[slot])[0x20] != 0;
            if(IsPhysicalRAM(image) && image != 0) continue;
            if(bad++ < 16) {
                OSReport("[VK 24P] %s: Mii texture slot %d array %d image %08x (image3 %08x, hasMii %d, mii %08x)\n", when,
                    slot, array, image, words[3], hasMii, group.mii != nullptr ? (u32)group.mii[slot] : 0);
            }
        }
    }
    OSReport("[VK 24P] %s: %d Mii textures out of RAM (miiCount %d)\n", when, bad, group.miiCount);
}
static void CheckMiiTexturesAtLoad() { CheckMiiTextures("race load"); }
static RaceLoadHook raceMiiTextures(CheckMiiTexturesAtLoad);
static void CheckMiiTexturesInRace() {
    static u32 frame = 0;
    const Raceinfo* raceinfo = Raceinfo::sInstance;
    const u32 raceFrame = raceinfo != nullptr ? raceinfo->raceFrames : 0;
    if(raceFrame == frame) return;
    frame = raceFrame;
    if(raceFrame == 1 || raceFrame == 120) CheckMiiTextures(raceFrame == 1 ? "race frame 1" : "race frame 120");
}
static RaceFrameHook raceMiiTexturesFrames(CheckMiiTexturesInRace);

static u32 drawnControl = 0;
static u32 drawnMaterial = 0;
static void RememberControl(HookRegs& regs, u32) { drawnControl = regs.gpr[3]; }
static void RememberMaterial(HookRegs& regs, u32) { drawnMaterial = regs.gpr[3]; }
typedef const char* (*ClassNameFunc)(u32 control);

static void CheckLoadedTexture(HookRegs& regs, u32) {
    static u32 reports = 0;
    if(reports >= 8) return;
    const u32 object = regs.gpr[3];
    if(!IsMem(object)) return;
    const u32 image3 = reinterpret_cast<const u32*>(object)[3];
    const u32 image = (image3 & 0x00FFFFFF) << 5;
    if(IsPhysicalRAM(image)) return;
    ++reports;
    u32 callers[5];
    GetCallers(regs.gpr[1], callers, 5);
    OSReport("[VK 24P] texture out of RAM: TexObj %08x image %08x (image3 %08x), lr %08x, callers %08x %08x %08x %08x %08x\n",
        object, image, image3, regs.lr, callers[0], callers[1], callers[2], callers[3], callers[4]);
    if(IsMem(drawnMaterial)) {
        char name[0x16];
        memcpy(name, reinterpret_cast<const char*>(drawnMaterial + 0x44), 0x15);
        name[0x15] = '\0';
        OSReport("[VK 24P]   material %08x \"%s\"\n", drawnMaterial, name);
    }
    if(IsMem(drawnControl)) {
        const u32 vtable = *reinterpret_cast<const u32*>(drawnControl);
        const char* className = "?";
        if(IsMem(vtable) && IsMem(reinterpret_cast<const u32*>(vtable)[0x2c / 4])) {
            className = reinterpret_cast<ClassNameFunc>(reinterpret_cast<const u32*>(vtable)[0x2c / 4])(drawnControl);
        }
        OSReport("[VK 24P]   control %08x vtable %08x class %s\n", drawnControl, vtable, IsMem((u32)className) ? className : "?");
    }
}
static const HookPatch textureHooks[] ={
    { 0x80170dc8, 0x9421ffe0, CheckLoadedTexture, true },
    { 0x8063db84, 0x9421fff0, RememberControl, true },
    { 0x80081210, 0x9421fee0, RememberMaterial, true },
};
static PatchSet textureSet("Texture check", nullptr, 0, nullptr, 0, nullptr, 0, textureHooks, P24_COUNT(textureHooks),
    nullptr, 0, true);
#endif

}
}
