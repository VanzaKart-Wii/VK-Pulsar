#include <SlotExpansion/CupsConfig.hpp>
#include <SlotExpansion/UI/ExpansionUIMisc.hpp>
#include <IO/IO.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {

static const char catalogHeader[] =
    "# VK-Pulsar - Catalogo piste / Track catalog\n"
    "#\n"
    "# IT: Attivalo in Impostazioni > Menu > Layout > Personalizzato.\n"
    "#     Una pista per riga, nell'ordine in cui vuoi vederla nelle coppe.\n"
    "#     Per nascondere una pista cancella la riga o mettici # davanti.\n"
    "#     Offline il random sceglie solo tra le piste di questo elenco.\n"
    "#     Il codice a sinistra identifica la pista: non modificarlo.\n"
    "# EN: Enable it in Settings > Menu > Layout > Custom.\n"
    "#     One track per line, in the order you want to see it in the cups.\n"
    "#     To hide a track delete its line or put # in front of it.\n"
    "#     Offline, random only picks among the tracks of this list.\n"
    "#     The code on the left identifies the track: don't change it.\n"
    "\n";

static const char newTracksHeader[] =
    "\n# Piste nuove del pack: togli il # per aggiungerle / New tracks of the pack: remove the # to add them\n";

static const u32 maxNameLength = 60;
static const u32 lineLength = 8 + 1 + maxNameLength + 1;
static const u32 chunkSize = 0x800;
static const u32 lineBufferSize = 0x40;

static s32 HexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool IsBlank(char c) {
    return c == ' ' || c == '\t';
}

static bool ParseCRC32(const char* cur, const char* end, u32& crc32) {
    u32 value = 0;
    u32 digits = 0;
    while (cur < end && digits < 8) {
        const s32 digit = HexDigit(*cur);
        if (digit < 0) break;
        value = (value << 4) | digit;
        ++digits;
        ++cur;
    }
    if (digits != 8) return false;
    if (cur < end && !IsBlank(*cur) && *cur != '\r' && *cur != '\n') return false;
    crc32 = value;
    return true;
}

static u32 WriteTrackLine(char* dest, u32 crc32, PulsarId id, bool isHidden) {
    char name[0x100];
    name[0] = '\0';
    if (UI::GetCustomMsg(UI::GetTrackBMGId(id, true)) != nullptr) UI::GetTrackBMG(name, id);
    name[maxNameLength] = '\0';
    for (char* c = name; *c != '\0'; ++c) if (static_cast<u8>(*c) < 0x20) *c = ' ';
    return snprintf(dest, lineLength + 3, isHidden ? "# %08x %s\n" : "%08x %s\n", crc32, name);
}

static void ParseCatalogLine(const char* line, u32 length, const Track* tracks, u32 trackCount, bool* isListed, u16* catalog, u32& count) {
    const char* cur = line;
    const char* end = line + length;
    while (cur < end && IsBlank(*cur)) ++cur;
    bool isHidden = false;
    if (cur < end && *cur == '#') {
        isHidden = true;
        ++cur;
        while (cur < end && IsBlank(*cur)) ++cur;
    }
    u32 crc32;
    if (!ParseCRC32(cur, end, crc32)) return;
    for (u32 i = 0; i < trackCount; ++i) {
        if (tracks[i].crc32 != crc32 || isListed[i]) continue;
        isListed[i] = true;
        if (!isHidden) catalog[count++] = i;
        return;
    }
}

void CupsConfig::EnableCatalog() {
    this->isCatalogAllowed = true;
    if (this->isCustomLayout) {
        this->isCatalogRequested = true;
        this->LoadCatalog();
    }
    this->UpdateCustomLayout();
}

void CupsConfig::LoadCatalogTask(void*) {
    CupsConfig::sInstance->LoadCatalog();
}

void CupsConfig::RequestCatalog() {
    if (!this->isCustomLayout || !this->isCatalogAllowed || this->isCatalogRequested) return;
    this->isCatalogRequested = true;
    System::sInstance->taskThread->Request(&CupsConfig::LoadCatalogTask, nullptr, 0);
}

void CupsConfig::LoadCatalog() {
    const u32 trackCount = (this->hasOddCups ? this->definedCTsCupCount - 1 : this->definedCTsCupCount) * 4;
    System* system = System::sInstance;
    IO* io = IO::sInstance;
    if (trackCount == 0 || io == nullptr || this->catalog != nullptr) return;

    char path[IOS::ipcMaxPath];
    snprintf(path, IOS::ipcMaxPath, "%s/Catalog.txt", system->GetModFolder());

    u16* catalog = new (system->heap) u16[trackCount];
    u32 count = 0;
    bool* isListed = new (system->heap) bool[trackCount];
    for (u32 i = 0; i < trackCount; ++i) isListed[i] = false;
    char* chunk = io->Alloc<char>(chunkSize);

    s32 fileSize = 0;
    bool endsWithNewLine = true;
    if (io->OpenFile(path, FILE_MODE_READ)) {
        fileSize = io->GetFileSize();
        char line[lineBufferSize];
        u32 lineUsed = 0;
        s32 read;
        while (fileSize > 0 && (read = io->Read(chunkSize, chunk)) > 0) {
            for (s32 i = 0; i < read; ++i) {
                const char c = chunk[i];
                if (c == '\n') {
                    ParseCatalogLine(line, lineUsed, this->mainTracks, trackCount, isListed, catalog, count);
                    lineUsed = 0;
                }
                else if (lineUsed < lineBufferSize) line[lineUsed++] = c;
            }
            endsWithNewLine = chunk[read - 1] == '\n';
        }
        if (lineUsed != 0) ParseCatalogLine(line, lineUsed, this->mainTracks, trackCount, isListed, catalog, count);
        io->Close();
    }

    const bool isNewFile = fileSize <= 0;
    bool hasMissing = false;
    for (u32 i = 0; i < trackCount; ++i) {
        if (isListed[i]) continue;
        hasMissing = true;
        if (isNewFile) catalog[count++] = i;
    }

    if (hasMissing) {
        bool isOpen;
        if (isNewFile) isOpen = io->CreateAndOpen(path, FILE_MODE_WRITE);
        else {
            isOpen = io->OpenFile(path, FILE_MODE_READ_WRITE);
            if (isOpen) io->Seek(fileSize);
        }
        if (isOpen) {
            u32 length = 0;
            if (isNewFile) length = snprintf(chunk, chunkSize, "%s", catalogHeader);
            else {
                if (!endsWithNewLine) chunk[length++] = '\n';
                length += snprintf(chunk + length, chunkSize - length, "%s", newTracksHeader);
            }
            for (u32 i = 0; i < trackCount; ++i) {
                if (isListed[i]) continue;
                if (length + lineLength + 3 > chunkSize) {
                    io->Write(length, chunk);
                    length = 0;
                }
                length += WriteTrackLine(chunk + length, this->mainTracks[i].crc32, static_cast<PulsarId>(i + PULSARID_FIRSTCT), !isNewFile);
            }
            if (length != 0) io->Write(length, chunk);
            io->Close();
        }
    }

    EGG::Heap::free(chunk, nullptr);
    delete[] isListed;
    this->catalog = catalog;
    this->catalogCount = count;
}

}//namespace Pulsar
