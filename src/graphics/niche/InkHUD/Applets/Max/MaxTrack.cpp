#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include "./MaxTrack.h"

#include "./MaxUi.h"
#include "FSCommon.h"
#include "GPSStatus.h"
#include "SPILock.h"
#include "concurrency/OSThread.h"
#include "gps/RTC.h"
#include "main.h"
#include "mesh/Throttle.h"

#ifdef HAS_SDCARD
#include <SD.h>
#endif

#include <string.h>
#include <time.h>

using namespace NicheGraphics;

namespace
{

constexpr uint32_t CHECK_MS = 10 * 1000;
constexpr uint32_t RETRY_MS = 60 * 1000;           // after a write error
constexpr float MIN_METERS = 10;                   // a new point once we moved this far...
constexpr uint32_t MAX_SILENCE_MS = 5 * 60 * 1000; // ...or this long after the last one
constexpr uint32_t NEW_SEGMENT_MS = 2 * 60 * 1000; // a fix lost for longer than this starts a new segment

const char *PREFS_FILE = "/prefs/maxgpx.bin";
constexpr uint32_t PREFS_MAGIC = 0x69780001;
const char *TRACK_DIR = "/tracce";
const char TAIL[] = "</trkseg></trk></gpx>\n";

#ifdef BUILD_EPOCH
constexpr uint32_t VALID_AFTER = BUILD_EPOCH - (SEC_PER_DAY * 30 * 6);
#else
constexpr uint32_t VALID_AFTER = 1738368000 - (SEC_PER_DAY * 30 * 6);
#endif

struct Prefs {
    uint32_t magic;
    uint8_t enabled;
};

// Degrees * 1e7 as a decimal string, without going through float
void formatDegrees(char *out, size_t n, int32_t v)
{
    const bool negative = v < 0;
    const uint32_t a = negative ? (uint32_t)(-(int64_t)v) : (uint32_t)v;
    snprintf(out, n, "%s%lu.%07lu", negative ? "-" : "", (unsigned long)(a / 10000000UL), (unsigned long)(a % 10000000UL));
}

class GpxLogger : public concurrency::OSThread
{
  public:
    GpxLogger() : concurrency::OSThread("MaxGpx")
    {
        loadPrefs();
        state = enabled ? InkHUD::GpxState::WAITING_FIX : InkHUD::GpxState::OFF;
        setIntervalFromNow(CHECK_MS);
    }

    bool enabled = true;
    InkHUD::GpxState state = InkHUD::GpxState::WAITING_FIX;
    uint32_t points = 0;

    void setEnabled(bool on)
    {
        enabled = on;
        savePrefs();
        // Switching back on starts a new file
        fileReady = false;
        hasLast = false;
        points = 0;
        state = on ? InkHUD::GpxState::WAITING_FIX : InkHUD::GpxState::OFF;
    }

  protected:
    int32_t runOnce() override
    {
        if (!enabled) {
            state = InkHUD::GpxState::OFF;
            return CHECK_MS;
        }
#ifndef HAS_SDCARD
        state = InkHUD::GpxState::NO_CARD;
        return CHECK_MS;
#else
        if (SD.cardType() == CARD_NONE) {
            state = InkHUD::GpxState::NO_CARD;
            return CHECK_MS;
        }
        if (!gpsStatus || !gpsStatus->getHasLock()) {
            state = InkHUD::GpxState::WAITING_FIX;
            return CHECK_MS;
        }
        const uint32_t now = getValidTime(RTCQuality::RTCQualityDevice, false);
        const int32_t lat = gpsStatus->getLatitude(), lon = gpsStatus->getLongitude();
        if (now < VALID_AFTER || (lat == 0 && lon == 0)) {
            state = InkHUD::GpxState::WAITING_FIX;
            return CHECK_MS;
        }

        // A fix that was lost for a while: the next point opens a new segment
        if (hasLast && !Throttle::isWithinTimespanMs(lastFixMs, NEW_SEGMENT_MS))
            newSegment = true;
        lastFixMs = millis();

        bool record = !hasLast;
        if (!record) {
            float e, n;
            MaxUi::localOffset(lastLat, lastLon, lat, lon, e, n);
            record = sqrtf(e * e + n * n) >= MIN_METERS || !Throttle::isWithinTimespanMs(lastPointMs, MAX_SILENCE_MS);
        }
        if (!record) {
            state = InkHUD::GpxState::RECORDING;
            return CHECK_MS;
        }

        if (!fileReady && !createFile()) {
            state = InkHUD::GpxState::CARD_ERROR;
            return RETRY_MS;
        }

        char la[16], lo[16], when[24], point[200];
        formatDegrees(la, sizeof(la), lat);
        formatDegrees(lo, sizeof(lo), lon);
        const time_t t = now;
        struct tm tm;
        gmtime_r(&t, &tm);
        strftime(when, sizeof(when), "%Y-%m-%dT%H:%M:%SZ", &tm);
        snprintf(point, sizeof(point), "%s<trkpt lat=\"%s\" lon=\"%s\"><ele>%ld</ele><time>%s</time><sat>%lu</sat></trkpt>\n",
                 newSegment ? "</trkseg><trkseg>\n" : "", la, lo, (long)gpsStatus->getAltitude(), when,
                 (unsigned long)gpsStatus->getNumSatellites());
        if (!appendBeforeTail(point)) {
            state = InkHUD::GpxState::CARD_ERROR;
            return RETRY_MS;
        }

        hasLast = true;
        newSegment = false;
        lastLat = lat;
        lastLon = lon;
        lastPointMs = millis();
        points++;
        state = InkHUD::GpxState::RECORDING;
        return CHECK_MS;
#endif
    }

  private:
    char path[48] = "";
    bool fileReady = false;
    bool hasLast = false;
    bool newSegment = false;
    int32_t lastLat = 0, lastLon = 0;
    uint32_t lastPointMs = 0, lastFixMs = 0;

    void loadPrefs()
    {
#ifdef FSCom
        concurrency::LockGuard guard(spiLock);
        if (FSCom.exists(PREFS_FILE)) {
            auto f = FSCom.open(PREFS_FILE, FILE_O_READ);
            if (f) {
                Prefs p = {};
                if (f.read((uint8_t *)&p, sizeof(p)) == sizeof(p) && p.magic == PREFS_MAGIC)
                    enabled = p.enabled != 0;
                f.close();
            }
        }
#endif
    }

    void savePrefs()
    {
#ifdef FSCom
        const Prefs p = {PREFS_MAGIC, (uint8_t)(enabled ? 1 : 0)};
        concurrency::LockGuard guard(spiLock);
        FSCom.mkdir("/prefs");
        auto f = FSCom.open(PREFS_FILE, FILE_O_WRITE);
        if (f) {
            f.write((const uint8_t *)&p, sizeof(p));
            f.flush();
            f.close();
        }
#endif
    }

#ifdef HAS_SDCARD
    // A new file named after the local date and time, with the header and the closing tags already in place
    bool createFile()
    {
        const time_t local = getValidTime(RTCQuality::RTCQualityDevice, true);
        struct tm tm;
        gmtime_r(&local, &tm);
        char stamp[20], title[20];
        strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H%M", &tm);
        strftime(title, sizeof(title), "%Y-%m-%d %H:%M", &tm);

        concurrency::LockGuard guard(spiLock);
        if (!SD.exists(TRACK_DIR))
            SD.mkdir(TRACK_DIR);
        snprintf(path, sizeof(path), "%s/%s.gpx", TRACK_DIR, stamp);
        for (int i = 2; SD.exists(path) && i < 10; i++)
            snprintf(path, sizeof(path), "%s/%s_%d.gpx", TRACK_DIR, stamp, i);

        File f = SD.open(path, FILE_WRITE);
        if (!f)
            return false;
        f.print("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<gpx version=\"1.1\" creator=\"T-Deck Max\" xmlns=\"http://www.topografix.com/GPX/1/1\">\n<trk><name>");
        f.print(title);
        f.print("</name><trkseg>\n");
        const bool ok = f.print(TAIL) == strlen(TAIL);
        f.close();
        fileReady = ok;
        hasLast = false;
        newSegment = false;
        points = 0;
        return ok;
    }

    // Overwrite the closing tags with the new text, then put them back after it
    bool appendBeforeTail(const char *text)
    {
        concurrency::LockGuard guard(spiLock);
        File f = SD.open(path, "r+");
        if (!f)
            return false;
        const size_t size = f.size(), tailLen = strlen(TAIL), textLen = strlen(text);
        bool ok = size >= tailLen && f.seek(size - tailLen);
        ok = ok && f.write((const uint8_t *)text, textLen) == textLen;
        ok = ok && f.write((const uint8_t *)TAIL, tailLen) == tailLen;
        f.close();
        return ok;
    }
#endif
};

GpxLogger *logger = nullptr;

} // namespace

void NicheGraphics::InkHUD::maxGpxBegin()
{
    if (!logger)
        logger = new GpxLogger();
}

bool NicheGraphics::InkHUD::maxGpxEnabled()
{
    return logger && logger->enabled;
}

void NicheGraphics::InkHUD::maxGpxSetEnabled(bool enabled)
{
    if (logger)
        logger->setEnabled(enabled);
}

NicheGraphics::InkHUD::GpxState NicheGraphics::InkHUD::maxGpxState()
{
    return logger ? logger->state : GpxState::OFF;
}

uint32_t NicheGraphics::InkHUD::maxGpxPoints()
{
    return logger ? logger->points : 0;
}

bool NicheGraphics::InkHUD::maxSdCardMb(uint32_t &mb)
{
#ifdef HAS_SDCARD
    if (SD.cardType() == CARD_NONE)
        return false;
    mb = (uint32_t)(SD.cardSize() / (1024 * 1024));
    return true;
#else
    (void)mb;
    return false;
#endif
}

#endif
