#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include "./MaxInfo.h"

#include "GPSStatus.h"
#include "NodeDB.h"
#include "PowerStatus.h"
#include "gps/RTC.h"

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
}

#endif
