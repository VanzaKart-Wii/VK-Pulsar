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

void CupsConfig::LoadCatalog() {
    const u32 trackCount = (this->hasOddCups ? this->definedCTsCupCount - 1 : this->definedCTsCupCount) * 4;
    if (trackCount == 0) return;
    System* system = System::sInstance;
    IO* io = IO::sInstance;
    if (io == nullptr) return;

    char path[IOS::ipcMaxPath];
    snprintf(path, IOS::ipcMaxPath, "%s/Catalog.txt", system->GetModFolder());

    this->catalog = new (system->heap) u16[trackCount];
    this->catalogCount = 0;
    bool* isListed = new (system->heap) bool[trackCount];
    for (u32 i = 0; i < trackCount; ++i) isListed[i] = false;

    s32 fileSize = 0;
    bool endsWithNewLine = true;
    if (io->OpenFile(path, FILE_MODE_READ)) {
        fileSize = io->GetFileSize();
        if (fileSize > 0) {
            char* text = io->Alloc<char>(fileSize);
            const s32 read = io->Read(fileSize, text);
            if (read > 0) {
                const char* cur = text;
                const char* end = text + read;
                endsWithNewLine = end[-1] == '\n';
                while (cur < end) {
                    const char* lineEnd = cur;
                    while (lineEnd < end && *lineEnd != '\n') ++lineEnd;
                    while (cur < lineEnd && IsBlank(*cur)) ++cur;
                    bool isHidden = false;
                    if (cur < lineEnd && *cur == '#') {
                        isHidden = true;
                        ++cur;
                        while (cur < lineEnd && IsBlank(*cur)) ++cur;
                    }
                    u32 crc32;
                    if (ParseCRC32(cur, lineEnd, crc32)) {
                        for (u32 i = 0; i < trackCount; ++i) {
                            if (this->mainTracks[i].crc32 != crc32 || isListed[i]) continue;
                            isListed[i] = true;
                            if (!isHidden) this->catalog[this->catalogCount++] = i;
                            break;
                        }
                    }
                    cur = lineEnd + 1;
                }
            }
            EGG::Heap::free(text, nullptr);
        }
        io->Close();
    }

    const bool isNewFile = fileSize <= 0;
    u32 missingCount = 0;
    for (u32 i = 0; i < trackCount; ++i) if (!isListed[i]) ++missingCount;

    if (missingCount != 0) {
        const u32 bufferSize = sizeof(catalogHeader) + sizeof(newTracksHeader) + missingCount * (lineLength + 3) + 2;
        char* buffer = io->Alloc<char>(bufferSize);
        u32 length = 0;
        if (isNewFile) length += snprintf(buffer, bufferSize, "%s", catalogHeader);
        else {
            if (!endsWithNewLine) buffer[length++] = '\n';
            length += snprintf(buffer + length, bufferSize - length, "%s", newTracksHeader);
        }
        for (u32 i = 0; i < trackCount; ++i) {
            if (isListed[i]) continue;
            length += WriteTrackLine(buffer + length, this->mainTracks[i].crc32, static_cast<PulsarId>(i + PULSARID_FIRSTCT), !isNewFile);
            if (isNewFile) this->catalog[this->catalogCount++] = i;
        }
        bool isOpen;
        if (isNewFile) isOpen = io->CreateAndOpen(path, FILE_MODE_WRITE);
        else {
            isOpen = io->OpenFile(path, FILE_MODE_READ_WRITE);
            if (isOpen) io->Seek(fileSize);
        }
        if (isOpen) {
            io->Write(length, buffer);
            io->Close();
        }
        EGG::Heap::free(buffer, nullptr);
    }

    delete[] isListed;
    this->UpdateCustomLayout();
}

}//namespace Pulsar
