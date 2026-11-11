#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include "./RadarApplet.h"

#include "./MaxUi.h"
#include "GPSStatus.h"
#include "NodeDB.h"
#include "mesh/Throttle.h"

#include <algorithm>
#include <vector>

using namespace NicheGraphics;

static constexpr uint32_t RECORD_INTERVAL_MS = 20 * 1000;
static constexpr float RECORD_MIN_METERS = 15;
static constexpr uint32_t REDRAW_MIN_MS = 30 * 1000;

InkHUD::RadarApplet::RadarApplet() : concurrency::OSThread("RadarApplet")
{
    setIntervalFromNow(RECORD_INTERVAL_MS);
}

void InkHUD::RadarApplet::txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color)
{
    setFont(font == MaxUi::SMALL ? fontSmall : (font == MaxUi::MEDIUM ? fontMedium : fontLarge));
    setTextColor(color);
    HorizontalAlignment ha = LEFT;
    if (align == MaxUi::CENTER)
        ha = CENTER;
    else if (align == MaxUi::RIGHT)
        ha = RIGHT;
    printAt(x, y, text, ha, valign == MaxUi::MIDDLE ? MIDDLE : TOP);
    setTextColor(BLACK);
}

// Our position: from the GPS, otherwise whatever our node knows (the phone can provide it)
bool InkHUD::RadarApplet::ourPosition(int32_t &lat, int32_t &lon)
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

int32_t InkHUD::RadarApplet::runOnce()
{
    int32_t lat, lon;
    if (ourPosition(lat, lon)) {
        bool record = trackCount == 0;
        if (!record) {
            const int last = (trackHead + TRACK_MAX - 1) % TRACK_MAX;
            float e, n;
            MaxUi::localOffset(trackLat[last], trackLon[last], lat, lon, e, n);
            record = sqrtf(e * e + n * n) >= RECORD_MIN_METERS;
        }
        if (record) {
            trackLat[trackHead] = lat;
            trackLon[trackHead] = lon;
            trackHead = (trackHead + 1) % TRACK_MAX;
            if (trackCount < TRACK_MAX)
                trackCount++;
        }
    }

    // Redraw while on screen, when something changed, but not too often (e-ink)
    if (isForeground() && !Throttle::isWithinTimespanMs(lastRenderMs, REDRAW_MIN_MS)) {
        int32_t la = 0, lo = 0;
        ourPosition(la, lo);
        if (la != renderedLat || lo != renderedLon || nodeDB->getNumMeshNodes() != renderedNodes)
            requestUpdate(Drivers::EInk::UpdateTypes::FAST);
    }
    return RECORD_INTERVAL_MS;
}

void InkHUD::RadarApplet::onRender(bool full)
{
    (void)full;
    lastRenderMs = millis();
    renderedNodes = nodeDB->getNumMeshNodes();

    static MaxUi::Radar rd; // about 1.5 KB: keep it off the stack
    rd.clear();
    int32_t lat = 0, lon = 0;
    rd.hasPos = ourPosition(lat, lon);
    renderedLat = lat;
    renderedLon = lon;

    if (gpsStatus) {
        rd.gpsLock = gpsStatus->getHasLock();
        rd.sats = (int)gpsStatus->getNumSatellites();
    }
    if (rd.gpsLock) {
        rd.speedKmh = (int)localPosition.ground_speed;
        rd.altM = localPosition.altitude;
        rd.hasAlt = localPosition.has_altitude;
        if (localPosition.ground_speed >= 2) // walking pace or more: the course is meaningful
            rd.headingDeg = (int)(localPosition.ground_track / 100000) % 360;
    }

    if (rd.hasPos) {
        // Track, oldest first
        for (int i = 0; i < trackCount; i++) {
            const int idx = (trackHead + TRACK_MAX - trackCount + i) % TRACK_MAX;
            MaxUi::localOffset(lat, lon, trackLat[idx], trackLon[idx], rd.trackE[rd.nTrack], rd.trackN[rd.nTrack]);
            rd.nTrack++;
        }

        // Nodes with a position: favorites first, then the nearest
        struct Cand {
            meshtastic_NodeInfoLite *node;
            float e, n, d;
            bool fav;
        };
        static Cand cands[MaxUi::RADAR_MAX_BLIPS];
        int nc = 0;
        const NodeNum me = nodeDB->getNodeNum();
        for (size_t i = 0; i < nodeDB->getNumMeshNodes(); i++) {
            meshtastic_NodeInfoLite *node = nodeDB->getMeshNodeByIndex(i);
            if (!node || node->num == me || !nodeDB->hasValidPosition(node))
                continue;
            meshtastic_PositionLite pos;
            if (!nodeDB->copyNodePosition(node->num, pos))
                continue;
            Cand c;
            c.node = node;
            c.fav = nodeInfoLiteIsFavorite(node);
            MaxUi::localOffset(lat, lon, pos.latitude_i, pos.longitude_i, c.e, c.n);
            c.d = sqrtf(c.e * c.e + c.n * c.n);
            // keep the best RADAR_MAX_BLIPS: replace the worst one if this one is better
            auto better = [](const Cand &a, const Cand &b) { return a.fav != b.fav ? a.fav : a.d < b.d; };
            if (nc < MaxUi::RADAR_MAX_BLIPS) {
                cands[nc++] = c;
            } else {
                int worst = 0;
                for (int k = 1; k < nc; k++)
                    if (better(cands[worst], cands[k]))
                        worst = k;
                if (better(c, cands[worst]))
                    cands[worst] = c;
            }
        }
        std::sort(cands, cands + nc, [](const Cand &a, const Cand &b) { return a.fav != b.fav ? a.fav : a.d < b.d; });

        int sel = -1;
        for (int i = 0; i < nc; i++)
            if (cands[i].node->num == selectedNode)
                sel = i;
        if (sel < 0 && nc > 0) {
            sel = 0;
            selectedNode = cands[0].node->num;
        }

        for (int i = 0; i < nc; i++) {
            MaxUi::RadarBlip &b = rd.blips[i];
            b.e = cands[i].e;
            b.n = cands[i].n;
            b.fav = cands[i].fav;
            b.selected = i == sel;
            snprintf(b.shortName, sizeof(b.shortName), "%s", nodeInfoLiteHasUser(cands[i].node) ? cands[i].node->short_name : "?");
        }
        rd.nBlips = nc;
        rd.selected = sel;
        if (sel >= 0) {
            const meshtastic_NodeInfoLite *n = cands[sel].node;
            if (nodeInfoLiteHasUser(n) && n->long_name[0])
                snprintf(rd.selName, sizeof(rd.selName), "%s", n->long_name);
            else if (nodeInfoLiteHasUser(n) && n->short_name[0])
                snprintf(rd.selName, sizeof(rd.selName), "%s", n->short_name);
            else
                snprintf(rd.selName, sizeof(rd.selName), "!%08x", (unsigned)n->num);
            rd.selDist = cands[sel].d;
            rd.selBearing = MaxUi::bearingDeg(cands[sel].e, cands[sel].n);
        }
    }

    MaxUi::renderRadar(*this, width(), height(), rd);
}

bool InkHUD::RadarApplet::onTouchPoint(uint16_t x, uint16_t y, bool longPress)
{
    if (longPress)
        return false;

    if (MaxUi::homeMenuAt(width(), height(), x, y)) {
        inkhud->openMenu();
        return true;
    }
    if ((x - 22) * (x - 22) + (y - 22) * (y - 22) <= 20 * 20) {
        inkhud->showApplet(MaxUi::A_HOME);
        return true;
    }

    // Tap on the radar: next node (same order as drawn: favorites first, then the nearest)
    if (MaxUi::radarAt(width(), x, y)) {
        int32_t lat, lon;
        if (!ourPosition(lat, lon))
            return true;
        struct Cand {
            NodeNum num;
            float d;
            bool fav;
        };
        std::vector<Cand> list;
        const NodeNum me = nodeDB->getNodeNum();
        for (size_t i = 0; i < nodeDB->getNumMeshNodes(); i++) {
            meshtastic_NodeInfoLite *node = nodeDB->getMeshNodeByIndex(i);
            meshtastic_PositionLite pos;
            if (!node || node->num == me || !nodeDB->hasValidPosition(node) || !nodeDB->copyNodePosition(node->num, pos))
                continue;
            float e, n;
            MaxUi::localOffset(lat, lon, pos.latitude_i, pos.longitude_i, e, n);
            list.push_back({node->num, sqrtf(e * e + n * n), nodeInfoLiteIsFavorite(node)});
        }
        std::sort(list.begin(), list.end(), [](const Cand &a, const Cand &b) { return a.fav != b.fav ? a.fav : a.d < b.d; });
        if (list.size() > (size_t)MaxUi::RADAR_MAX_BLIPS)
            list.resize(MaxUi::RADAR_MAX_BLIPS);
        if (list.empty())
            return true;
        size_t next = 0;
        for (size_t i = 0; i < list.size(); i++)
            if (list[i].num == selectedNode)
                next = (i + 1) % list.size();
        selectedNode = list[next].num;
        requestUpdate(Drivers::EInk::UpdateTypes::FAST);
        return true;
    }
    return false;
}

#endif
