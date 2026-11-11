#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include "./MaxInfo.h"

#include "GPSStatus.h"
#include "NodeDB.h"
#include "PowerStatus.h"
#include "gps/RTC.h"
#include "FSCommon.h"
#include "SPILock.h"
#include "main.h"

#include <algorithm>

static const char *BASE_FILE = "/prefs/maxbase.bin";
static constexpr uint32_t BASE_MAGIC = 0xBA5E0001;

struct BaseRecord {
    uint32_t magic;
    int32_t lat, lon;
};

static BaseRecord baseCache;
static bool baseLoaded = false;

bool NicheGraphics::InkHUD::maxOwnPosition(int32_t &lat, int32_t &lon)
{
    if (localPosition.latitude_i != 0 || localPosition.longitude_i != 0) {
        lat = localPosition.latitude_i;
        lon = localPosition.longitude_i;
        return true;
    }
    meshtastic_NodeInfoLite *self = nodeDB->getMeshNode(nodeDB->getNodeNum());
    meshtastic_PositionLite pos;
    if (self && nodeDB->hasValidPosition(self) && nodeDB->copyNodePosition(self->num, pos)) {
        lat = pos.latitude_i;
        lon = pos.longitude_i;
        return true;
    }
    return false;
}

bool NicheGraphics::InkHUD::maxBaseGet(int32_t &lat, int32_t &lon)
{
    if (!baseLoaded) {
        baseLoaded = true;
        baseCache.magic = 0;
#ifdef FSCom
        concurrency::LockGuard guard(spiLock);
        if (FSCom.exists(BASE_FILE)) {
            auto f = FSCom.open(BASE_FILE, FILE_O_READ);
            if (f) {
                if (f.read((uint8_t *)&baseCache, sizeof(baseCache)) != sizeof(baseCache))
                    baseCache.magic = 0;
                f.close();
            }
        }
#endif
    }
    if (baseCache.magic != BASE_MAGIC)
        return false;
    lat = baseCache.lat;
    lon = baseCache.lon;
    return true;
}

bool NicheGraphics::InkHUD::maxBaseSetHere()
{
    int32_t lat, lon;
    if (!maxOwnPosition(lat, lon))
        return false;
    baseCache.magic = BASE_MAGIC;
    baseCache.lat = lat;
    baseCache.lon = lon;
    baseLoaded = true;
#ifdef FSCom
    concurrency::LockGuard guard(spiLock);
    FSCom.mkdir("/prefs");
    auto f = FSCom.open(BASE_FILE, FILE_O_WRITE);
    if (f) {
        f.write((const uint8_t *)&baseCache, sizeof(baseCache));
        f.flush();
        f.close();
    }
#endif
    return true;
}

void NicheGraphics::InkHUD::gatherMaxInfo(MaxUi::Info &info)
{
    // Time (local, as set by the time zone setting). Ignore clocks which are clearly not set yet.
#ifdef BUILD_EPOCH
    constexpr uint32_t validAfterEpoch = BUILD_EPOCH - (SEC_PER_DAY * 30 * 6);
#else
    constexpr uint32_t validAfterEpoch = 1738368000 - (SEC_PER_DAY * 30 * 6);
#endif
    const uint32_t now = getValidTime(RTCQuality::RTCQualityDevice, true);
    if (now > validAfterEpoch)
        MaxUi::setTime(info, now);
    else
        info.timeValid = false;

    // GPS
    info.gpsPresent = gpsStatus != nullptr && config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED;
    if (gpsStatus) {
        info.gpsLock = gpsStatus->getHasLock();
        info.sats = (uint8_t)gpsStatus->getNumSatellites();
        info.lat = gpsStatus->getLatitude();
        info.lon = gpsStatus->getLongitude();
    }

    // Battery
    if (powerStatus) {
        info.hasBatt = powerStatus->getHasBattery();
        info.batt = powerStatus->getBatteryChargePercent();
        info.charging = powerStatus->getIsCharging();
    }

    // Nodes heard recently, and messages not yet read
    info.nodes = (uint16_t)nodeDB->getNumOnlineMeshNodes(true);
    info.unread = tDeckMaxUnread;
    info.diag = tDeckMaxDiag;

    // Direction of travel: only meaningful while moving (walking pace or more)
    if (info.gpsLock && localPosition.ground_speed >= 2)
        info.headingDeg = (int)(localPosition.ground_track / 100000) % 360;

    int32_t lat, lon;
    if (maxOwnPosition(lat, lon)) {
        // Way back to the base
        int32_t bLat, bLon;
        if (maxBaseGet(bLat, bLon)) {
            float e, n;
            MaxUi::localOffset(lat, lon, bLat, bLon, e, n);
            info.hasBase = true;
            info.baseDist = sqrtf(e * e + n * n);
            info.baseBearing = MaxUi::bearingDeg(e, n);
        }

        // Next sunrise or sunset: date and position only, no internet
        const uint32_t utc = getValidTime(RTCQuality::RTCQualityDevice, false);
        uint32_t when;
        bool sunset;
        if (utc > validAfterEpoch && MaxUi::nextSunEvent(utc, lat * 1e-7, lon * 1e-7, when, sunset)) {
            info.sunValid = true;
            info.sunIsSunset = sunset;
            info.sunMinutes = (uint16_t)std::min<uint32_t>((when - utc) / 60, 65535);
        }
    }
}

#endif
