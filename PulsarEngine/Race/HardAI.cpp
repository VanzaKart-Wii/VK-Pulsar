#include <kamek.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <Settings/Settings.hpp>

/*
Misc settings "Impossible CPUs". AI::Params reads the CPUs' speed and action tables from the Common archive (8073aca8).
With the setting on, they come from harder tables that Assets/CommonAssets.szs carries next to the game's:
kartAISpdParamHard.bin and kartAIActionParamHard.bin. Off, or with a pack without them, the game's own tables.
*/

namespace Pulsar {
namespace Race {

static bool IsHardAI() {
    return Settings::Mgr::IsCreated()
        && Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_LANGUAGE, RADIO_HARDAI) == HARDAI_ENABLED;
}

static void* GetAIParams(const ArchiveMgr* archive, ArchiveSource type, const char* name, const char* hardName, u32* size) {
    if(IsHardAI()) {
        void* file = archive->GetFile(type, hardName, size);
        if(file != nullptr) return file;
    }
    return archive->GetFile(type, name, size);
}

static void* GetAISpeedParams(const ArchiveMgr* archive, ArchiveSource type, const char* name, u32* size) {
    return GetAIParams(archive, type, name, "kartAISpdParamHard.bin", size);
}
kmCall(0x8073ae9c, GetAISpeedParams);

static void* GetAIActionParams(const ArchiveMgr* archive, ArchiveSource type, const char* name, u32* size) {
    return GetAIParams(archive, type, name, "kartAIActionParamHard.bin", size);
}
kmCall(0x8073af44, GetAIActionParams);

}//namespace Race
}//namespace Pulsar
