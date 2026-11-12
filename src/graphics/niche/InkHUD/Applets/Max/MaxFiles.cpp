#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include "./MaxFiles.h"

#include "./MaxInfo.h"
#include "FSCommon.h"
#include "GPSStatus.h"
#include "SPILock.h"
#include "gps/RTC.h"
#include "main.h"

#ifdef HAS_SDCARD
#include <SD.h>
#endif

#include <algorithm>
#include <ctype.h>
#include <string.h>
#include <time.h>

using namespace NicheGraphics;

const char *const NicheGraphics::InkHUD::MAX_DIARY_PATH = "/diario/diario.txt";

namespace
{

#ifdef BUILD_EPOCH
constexpr uint32_t VALID_AFTER = BUILD_EPOCH - (SEC_PER_DAY * 30 * 6);
#else
constexpr uint32_t VALID_AFTER = 1738368000 - (SEC_PER_DAY * 30 * 6);
#endif

bool diaryCounted = false;
uint32_t diaryNotes = 0;
bool filesCounted = false;
uint32_t textFiles = 0;

const char *POSITIONS_FILE = "/prefs/maxread.bin";
constexpr uint32_t POSITIONS_MAGIC = 0x7EAD0001;
constexpr int POSITIONS_MAX = 16;

struct Positions {
    uint32_t magic = 0;
    uint32_t hash[POSITIONS_MAX] = {};
    uint32_t offset[POSITIONS_MAX] = {};
    uint8_t next = 0; // slot to reuse when a new file comes along
};
Positions positions;
bool positionsLoaded = false;

uint32_t pathHash(const std::string &path)
{
    uint32_t h = 2166136261u; // FNV-1a
    for (unsigned char c : path) {
        h ^= c;
        h *= 16777619u;
    }
    return h ? h : 1;
}

void loadPositions()
{
    if (positionsLoaded)
        return;
    positionsLoaded = true;
#ifdef FSCom
    concurrency::LockGuard guard(spiLock);
    if (FSCom.exists(POSITIONS_FILE)) {
        auto f = FSCom.open(POSITIONS_FILE, FILE_O_READ);
        if (f) {
            if (f.read((uint8_t *)&positions, sizeof(positions)) != sizeof(positions) || positions.magic != POSITIONS_MAGIC)
                positions = Positions();
            f.close();
        }
    }
#endif
}

void savePositions()
{
#ifdef FSCom
    positions.magic = POSITIONS_MAGIC;
    concurrency::LockGuard guard(spiLock);
    FSCom.mkdir("/prefs");
    auto f = FSCom.open(POSITIONS_FILE, FILE_O_WRITE);
    if (f) {
        f.write((const uint8_t *)&positions, sizeof(positions));
        f.flush();
        f.close();
    }
#endif
}

bool endsWithTxt(const char *name)
{
    const size_t n = strlen(name);
    return n > 4 && name[n - 4] == '.' && tolower((unsigned char)name[n - 3]) == 't' &&
           tolower((unsigned char)name[n - 2]) == 'x' && tolower((unsigned char)name[n - 1]) == 't';
}

} // namespace

bool NicheGraphics::InkHUD::maxSdPresent()
{
#ifdef HAS_SDCARD
    return SD.cardType() != CARD_NONE;
#else
    return false;
#endif
}

bool NicheGraphics::InkHUD::maxDiaryAppend(const std::string &note)
{
#ifdef HAS_SDCARD
    if (!maxSdPresent())
        return false;

    // Header: "== 12/11/2026 09:46 | 42.35123, 13.39123 | 714 m =="
    char when[24] = "ora sconosciuta";
    const uint32_t local = getValidTime(RTCQuality::RTCQualityDevice, true);
    if (local > VALID_AFTER) {
        const time_t t = local;
        struct tm tm;
        gmtime_r(&t, &tm);
        strftime(when, sizeof(when), "%d/%m/%Y %H:%M", &tm);
    }
    char where[40] = "senza posizione";
    int32_t lat, lon;
    if (maxOwnPosition(lat, lon))
        snprintf(where, sizeof(where), "%.5f, %.5f", lat * 1e-7, lon * 1e-7);
    char height[16] = "";
    if (gpsStatus && gpsStatus->getHasLock())
        snprintf(height, sizeof(height), " | %ld m", (long)gpsStatus->getAltitude());

    std::string entry = "== ";
    entry += when;
    entry += " | ";
    entry += where;
    entry += height;
    entry += " ==\n";
    entry += note;
    entry += "\n\n";

    concurrency::LockGuard guard(spiLock);
    if (!SD.exists("/diario"))
        SD.mkdir("/diario");
    File f = SD.open(MAX_DIARY_PATH, FILE_APPEND);
    if (!f)
        return false;
    const bool ok = f.write((const uint8_t *)entry.data(), entry.size()) == entry.size();
    f.close();
    if (ok)
        diaryNotes++;
    return ok;
#else
    (void)note;
    return false;
#endif
}

uint32_t NicheGraphics::InkHUD::maxDiaryCount()
{
#ifdef HAS_SDCARD
    if (diaryCounted || !maxSdPresent())
        return diaryNotes;
    diaryCounted = true;
    diaryNotes = 0;

    // Count the headers: lines starting with "== "
    concurrency::LockGuard guard(spiLock);
    File f = SD.open(MAX_DIARY_PATH, FILE_READ);
    if (!f)
        return 0;
    uint8_t buf[256];
    uint8_t matched = 0; // characters of "== " matched at the start of the current line
    bool lineStart = true;
    size_t n;
    while ((n = f.read(buf, sizeof(buf))) > 0) {
        for (size_t i = 0; i < n; i++) {
            const char c = (char)buf[i];
            if (c == '\n') {
                lineStart = true;
                matched = 0;
                continue;
            }
            if (lineStart) {
                if (c == "== "[matched]) {
                    if (++matched == 3) {
                        diaryNotes++;
                        lineStart = false;
                    }
                } else {
                    lineStart = false;
                }
            }
        }
    }
    f.close();
#endif
    return diaryNotes;
}

void NicheGraphics::InkHUD::maxListTextFiles(std::vector<std::string> &paths, size_t max)
{
    paths.clear();
#ifdef HAS_SDCARD
    if (!maxSdPresent())
        return;
    static const char *dirs[] = {"/", "/libri", "/diario"};
    concurrency::LockGuard guard(spiLock);
    for (const char *dir : dirs) {
        File root = SD.open(dir);
        if (!root)
            continue;
        if (!root.isDirectory()) {
            root.close();
            continue;
        }
        File f = root.openNextFile();
        while (f && paths.size() < max) {
            if (!f.isDirectory() && endsWithTxt(f.name()))
                paths.push_back(f.path());
            f.close();
            f = root.openNextFile();
        }
        if (f)
            f.close();
        root.close();
    }
    std::sort(paths.begin(), paths.end());
#else
    (void)max;
#endif
    textFiles = paths.size();
    filesCounted = true;
}

uint32_t NicheGraphics::InkHUD::maxTextFileCount(bool refresh)
{
    if (refresh || !filesCounted) {
        std::vector<std::string> paths;
        maxListTextFiles(paths);
    }
    return textFiles;
}

uint32_t NicheGraphics::InkHUD::maxReaderPosition(const std::string &path)
{
    loadPositions();
    const uint32_t h = pathHash(path);
    for (int i = 0; i < POSITIONS_MAX; i++)
        if (positions.hash[i] == h)
            return positions.offset[i];
    return 0;
}

void NicheGraphics::InkHUD::maxReaderSetPosition(const std::string &path, uint32_t offset)
{
    loadPositions();
    const uint32_t h = pathHash(path);
    int slot = -1;
    for (int i = 0; i < POSITIONS_MAX; i++)
        if (positions.hash[i] == h)
            slot = i;
    if (slot < 0) {
        slot = positions.next % POSITIONS_MAX;
        positions.next = (slot + 1) % POSITIONS_MAX;
        positions.hash[slot] = h;
    } else if (positions.offset[slot] == offset) {
        return;
    }
    positions.offset[slot] = offset;
    savePositions();
}

#endif
