#pragma once

#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include <stdint.h>
#include <string>
#include <vector>

namespace NicheGraphics::InkHUD
{

// Files on the microSD card used by the T-Deck Max screens. All of them take spiLock themselves.

extern const char *const MAX_DIARY_PATH; // the diary: one text file, a dated header before every note

bool maxSdPresent();

// Add a note to the diary, after a header with local date, time, position and altitude. False if not saved.
bool maxDiaryAppend(const std::string &note);
uint32_t maxDiaryCount(); // notes in the diary (counted once, then kept up to date)

// .txt files in /, /libri and /diario, sorted by path
void maxListTextFiles(std::vector<std::string> &paths, size_t max = 40);
uint32_t maxTextFileCount(bool refresh);

// Where the reader was in each file (the last 16 files), kept in flash
uint32_t maxReaderPosition(const std::string &path);
void maxReaderSetPosition(const std::string &path, uint32_t offset);

} // namespace NicheGraphics::InkHUD

#endif
