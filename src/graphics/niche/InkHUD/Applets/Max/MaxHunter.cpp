#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include "./MaxHunter.h"

#include "./MaxInfo.h"
#include "./MaxUi.h"
#include "NodeDB.h"
#include "SPILock.h"
#include "concurrency/OSThread.h"
#include "gps/RTC.h"
#include "main.h"
#include "mesh/Throttle.h"

#ifdef HAS_SDCARD
#include <SD.h>
#endif

#include <algorithm>
#include <map>
#include <time.h>
#include <vector>

using namespace NicheGraphics;

namespace
{

constexpr uint32_t SCAN_MS = 30 * 1000;
constexpr uint32_t RELOG_MS = 10 * 60 * 1000; // the same node is written again at most this often
const char *HUNT_DIR = "/nodi";
const char *SEEN_FILE = "/nodi/visti.bin";

#ifdef BUILD_EPOCH
constexpr uint32_t VALID_AFTER = BUILD_EPOCH - (SEC_PER_DAY * 30 * 6);
#else
constexpr uint32_t VALID_AFTER = 1738368000 - (SEC_PER_DAY * 30 * 6);
#endif

// Keep names safe for a CSV cell
void csvName(char *out, size_t n, const char *name)
{
    size_t j = 0;
    for (size_t i = 0; name[i] && j + 1 < n; i++)
        out[j++] = (name[i] == ',' || name[i] == '"' || name[i] == '\n' || name[i] == '\r') ? ' ' : name[i];
    out[j] = '\0';
}

void formatDegrees(char *out, size_t n, int32_t v)
{
    const bool negative = v < 0;
    const uint32_t a = negative ? (uint32_t)(-(int64_t)v) : (uint32_t)v;
    snprintf(out, n, "%s%lu.%07lu", negative ? "-" : "", (unsigned long)(a / 10000000UL), (unsigned long)(a % 10000000UL));
}

class NodeHunter : public concurrency::OSThread
{
  public:
    NodeHunter() : concurrency::OSThread("MaxHunter") { setIntervalFromNow(SCAN_MS); }

    uint32_t total() const { return seen.size(); }
    uint32_t added = 0;

  protected:
    int32_t runOnce() override
    {
        const NodeNum self = nodeDB->getNodeNum();
        const bool sd = sdPresent();
        if (sd && !seenLoaded)
            loadSeen();

        for (size_t i = 0; i < nodeDB->getNumMeshNodes(); i++) {
            meshtastic_NodeInfoLite *node = nodeDB->getMeshNodeByIndex(i);
            if (!node || node->num == self || node->last_heard == 0 || nodeInfoLiteViaMqtt(node))
                continue;

            auto it = lastHeard.find(node->num);
            const bool known = it != lastHeard.end();
            const bool heardAgain = known && it->second != node->last_heard;
            lastHeard[node->num] = node->last_heard;
            // The first scan only notes what the node database already had from before
            if (!baselineDone || (known && !heardAgain))
                continue;

            if (!sd)
                continue;
            const bool isNew = !std::binary_search(seen.begin(), seen.end(), node->num);
            if (isNew)
                addSeen(node->num);

            auto logged = lastLogged.find(node->num);
            if (!isNew && logged != lastLogged.end() && Throttle::isWithinTimespanMs(logged->second, RELOG_MS))
                continue;
            if (writeRow(node, isNew))
                lastLogged[node->num] = millis();
        }
        baselineDone = true;
        return SCAN_MS;
    }

  private:
    std::vector<uint32_t> seen; // sorted
    bool seenLoaded = false;
    bool baselineDone = false;
    std::map<uint32_t, uint32_t> lastHeard;  // node -> last_heard at the previous scan
    std::map<uint32_t, uint32_t> lastLogged; // node -> millis() of the last row written

    static bool sdPresent()
    {
#ifdef HAS_SDCARD
        return SD.cardType() != CARD_NONE;
#else
        return false;
#endif
    }

    void loadSeen()
    {
        seenLoaded = true;
#ifdef HAS_SDCARD
        concurrency::LockGuard guard(spiLock);
        File f = SD.open(SEEN_FILE, FILE_READ);
        if (!f)
            return;
        uint32_t num;
        while (f.read((uint8_t *)&num, sizeof(num)) == sizeof(num) && seen.size() < 5000)
            seen.push_back(num);
        f.close();
        std::sort(seen.begin(), seen.end());
        seen.erase(std::unique(seen.begin(), seen.end()), seen.end());
#endif
    }

    void addSeen(uint32_t num)
    {
        seen.insert(std::upper_bound(seen.begin(), seen.end(), num), num);
        added++;
#ifdef HAS_SDCARD
        concurrency::LockGuard guard(spiLock);
        if (!SD.exists(HUNT_DIR))
            SD.mkdir(HUNT_DIR);
        File f = SD.open(SEEN_FILE, FILE_APPEND);
        if (f) {
            f.write((const uint8_t *)&num, sizeof(num));
            f.close();
        }
#endif
    }

    // One CSV row in today's file
    bool writeRow(meshtastic_NodeInfoLite *node, bool isNew)
    {
#ifdef HAS_SDCARD
        const uint32_t local = getValidTime(RTCQuality::RTCQualityDevice, true);
        if (local < VALID_AFTER)
            return false; // no date for the file name yet
        const time_t t = local;
        struct tm tm;
        gmtime_r(&t, &tm);
        char day[16], clock[12], path[40];
        strftime(day, sizeof(day), "%Y-%m-%d", &tm);
        strftime(clock, sizeof(clock), "%H:%M:%S", &tm);
        snprintf(path, sizeof(path), "%s/%s.csv", HUNT_DIR, day);

        char name[32];
        csvName(name, sizeof(name), nodeInfoLiteHasUser(node) ? node->long_name : "");
        char hops[8] = "";
        if (node->has_hops_away)
            snprintf(hops, sizeof(hops), "%u", (unsigned)node->hops_away);

        char myLat[16] = "", myLon[16] = "", itsLat[16] = "", itsLon[16] = "", dist[12] = "";
        int32_t la, lo;
        const bool mine = maxOwnPosition(la, lo);
        if (mine) {
            formatDegrees(myLat, sizeof(myLat), la);
            formatDegrees(myLon, sizeof(myLon), lo);
        }
        meshtastic_PositionLite pos;
        if (nodeDB->hasValidPosition(node) && nodeDB->copyNodePosition(node->num, pos)) {
            formatDegrees(itsLat, sizeof(itsLat), pos.latitude_i);
            formatDegrees(itsLon, sizeof(itsLon), pos.longitude_i);
            if (mine) {
                float e, n;
                MaxUi::localOffset(la, lo, pos.latitude_i, pos.longitude_i, e, n);
                snprintf(dist, sizeof(dist), "%lu", (unsigned long)sqrtf(e * e + n * n));
            }
        }

        char row[200];
        snprintf(row, sizeof(row), "%s,!%08lx,%s,%.1f,%s,%s,%s,%s,%s,%s,%s\n", clock, (unsigned long)node->num, name, node->snr,
                 hops, myLat, myLon, itsLat, itsLon, dist, isNew ? "si" : "no");

        concurrency::LockGuard guard(spiLock);
        if (!SD.exists(HUNT_DIR))
            SD.mkdir(HUNT_DIR);
        const bool fresh = !SD.exists(path);
        File f = SD.open(path, FILE_APPEND);
        if (!f)
            return false;
        if (fresh)
            f.print("ora,nodo,nome,snr_db,salti,mia_lat,mia_lon,sua_lat,sua_lon,distanza_m,nuovo\n");
        const size_t len = strlen(row);
        const bool ok = f.write((const uint8_t *)row, len) == len;
        f.close();
        return ok;
#else
        (void)node;
        (void)isNew;
        return false;
#endif
    }
};

NodeHunter *hunter = nullptr;

} // namespace

void NicheGraphics::InkHUD::maxHunterBegin()
{
    if (!hunter)
        hunter = new NodeHunter();
}

uint32_t NicheGraphics::InkHUD::maxHunterTotal()
{
    return hunter ? hunter->total() : 0;
}

uint32_t NicheGraphics::InkHUD::maxHunterNew()
{
    return hunter ? hunter->added : 0;
}

#endif
