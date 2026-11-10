#pragma once

#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include "./MaxUi.h"

namespace NicheGraphics::InkHUD
{

// Collect time, GPS, battery, nodes and messages for the T-Deck Max screens
void gatherMaxInfo(MaxUi::Info &info);

} // namespace NicheGraphics::InkHUD

#endif
