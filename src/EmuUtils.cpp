/*
 * File: Emu.cpp
 * Initial boilerplate/template generated via Gemini.
 * Adapted, refactored, and maintained by JigokuMaster.
 */

#include "EmuUtils.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>


void ExtractBaseFilename(const char* fullPath, char* dest, size_t maxLen)
{
    if (!fullPath || maxLen <= 0) { dest[0] = '\0'; return; }
    
    // Find last path slash divider
    const char* lastSlash = strrchr(fullPath, '/');
    const char* lastBackslash = strrchr(fullPath, '\\');
    const char* start = fullPath;
    
    if (lastSlash && lastSlash > start) start = lastSlash + 1;
    if (lastBackslash && lastBackslash > start) start = lastBackslash + 1;
    
    // Copy out into base destination buffer
    strncpy(dest, start, maxLen - 1);
    dest[maxLen - 1] = '\0';
}



void CalculateAspectRatioRect(int srcW, int srcH, int dstW, int dstH, SDL_Rect* outRect) {
    if (!outRect || srcW <= 0 || srcH <= 0) return;

    float scaleX = (float)dstW / (float)srcW;
    float scaleY = (float)dstH / (float)srcH;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;

    outRect->w = (Uint16)(srcW * scale);
    outRect->h = (Uint16)(srcH * scale);
    outRect->x = (Sint16)((dstW - outRect->w) / 2);
    outRect->y = (Sint16)((dstH - outRect->h) / 2);
}

// StateManager Implementation

StateManager::StateManager(const char* stateDir, const char* romPathOrName, unsigned int maxSlots)
    : m_maxSlots(maxSlots), 
      m_cachedSlotCount(-1), 
      m_lastSavedSlot(-1), 
      m_lastLoadedSlot(-1) 
{
    // 1. Process and normalize State Directory
    if (stateDir && stateDir[0] != '\0') {
        strncpy(m_stateDir, stateDir, sizeof(m_stateDir) - 1);
        m_stateDir[sizeof(m_stateDir) - 1] = '\0';

        // Remove trailing slashes for clean path formatting
        size_t len = strlen(m_stateDir);
        while (len > 0 && (m_stateDir[len - 1] == '/' || m_stateDir[len - 1] == '\\')) {
            m_stateDir[len - 1] = '\0';
            len--;
        }
    } else {
        strcpy(m_stateDir, ".");
    }

    // 2. Extract clean ROM Base Name
    ExtractBaseFilename(romPathOrName, m_romName, sizeof(m_romName));
    if (m_romName[0] == '\0') {
        strcpy(m_romName, "game");
    }
}

StateManager::~StateManager() {
}

// Path Construction Helpers: state_dir/romname.sav-slotNumber
bool StateManager::buildSlotPath(unsigned int slot, char* outBuf, size_t bufSize) const {
    if (!outBuf || bufSize == 0 || slot >= m_maxSlots) return false;


    char sep = '/';
#ifdef __SYMBIAN32__
    sep = '\\';
#endif

    int written = snprintf(outBuf, bufSize, "%s%c%s.sav-%u", m_stateDir, sep, m_romName, slot);
    return (written > 0 && static_cast<size_t>(written) < bufSize);
}

bool StateManager::buildAutoSavePath(char* outBuf, size_t bufSize) const {
    if (!outBuf || bufSize == 0) return false;

    char sep = '/';
#ifdef __SYMBIAN32__
    sep = '\\';
#endif
    int written = snprintf(outBuf, bufSize, "%s%c%s.autosav", m_stateDir, sep, m_romName);
    return (written > 0 && static_cast<size_t>(written) < bufSize);
}

bool StateManager::getSlotFilename(unsigned int slot, char* outBuffer, size_t bufferSize) const {
    return buildSlotPath(slot, outBuffer, bufferSize);
}

bool StateManager::getAutoSaveFilename(char* outBuffer, size_t bufferSize) const {
    return buildAutoSavePath(outBuffer, bufferSize);
}

// File Operations & Checks
bool StateManager::fileExists(const char* filepath) const {
    if (!filepath) return false;
    FILE* f = fopen(filepath, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

size_t StateManager::getFileSize(const char* filepath) const {
    if (!filepath) return 0;
    FILE* f = fopen(filepath, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);

    return (sz > 0) ? static_cast<size_t>(sz) : 0;
}

bool StateManager::slotExists(unsigned int slot) const {
    char path[MAX_PATH_LEN];
    if (!buildSlotPath(slot, path, sizeof(path))) return false;
    return fileExists(path);
}

bool StateManager::autoSaveExists() const {
    char path[MAX_PATH_LEN];
    if (!buildAutoSavePath(path, sizeof(path))) return false;
    return fileExists(path);
}

bool StateManager::deleteSlot(unsigned int slot) {
    char path[MAX_PATH_LEN];
    if (!buildSlotPath(slot, path, sizeof(path))) return false;

    if (remove(path) == 0) {
        refreshCache(); // Invalidate cached slot count
        return true;
    }
    return false;
}

bool StateManager::deleteAutoSave() {
    char path[MAX_PATH_LEN];
    if (!buildAutoSavePath(path, sizeof(path))) return false;
    return (remove(path) == 0);
}

size_t StateManager::getSlotFileSize(unsigned int slot) const {
    char path[MAX_PATH_LEN];
    if (!buildSlotPath(slot, path, sizeof(path))) return 0;
    return getFileSize(path);
}

size_t StateManager::getAutoSaveFileSize() const {
    char path[MAX_PATH_LEN];
    if (!buildAutoSavePath(path, sizeof(path))) return 0;
    return getFileSize(path);
}

// Notifications and Cache Refreshing
void StateManager::notifySlotSaved(unsigned int slot) {

    if ( slot >= m_cachedSlotCount) {
	m_cachedSlotCount++;
    }

    if (slot < m_maxSlots) {
        m_lastSavedSlot = static_cast<int>(slot);
        refreshCache(); // Mark cache dirty so next query counts correctly
    }

}

void StateManager::notifySlotLoaded(unsigned int slot) {
    if (slot < m_maxSlots) {
        m_lastLoadedSlot = static_cast<int>(slot);
    }
}

void StateManager::refreshCache() {
    m_cachedSlotCount = -1;
}

int StateManager::getFirstFreeSlot() const {
    for (unsigned int i = 0; i < m_maxSlots; ++i) {
        if (!slotExists(i)) {
            return static_cast<int>(i);
        }
    }
    return -1; // All slots occupied
}

unsigned int StateManager::getSlotCount() {
    if (m_cachedSlotCount >= 0) {
        return static_cast<unsigned int>(m_cachedSlotCount);
    }

    DIR* dir = opendir(m_stateDir);
    if (!dir) {
        m_cachedSlotCount = 0;
        return 0;
    }

    // Prepare matching prefix: "romname.sav-"
    char expectedPrefix[MAX_NAME_LEN + 8];
    snprintf(expectedPrefix, sizeof(expectedPrefix), "%s.sav-", m_romName);
    size_t prefixLen = strlen(expectedPrefix);


    unsigned int count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        // Check if filename starts with "romname.sav-"
        if (strncmp(entry->d_name, expectedPrefix, prefixLen) == 0) {
            // Ignore preview thumbnails or non-save extensions (e.g. .bmp)
            if (strstr(entry->d_name, ".bmp") == NULL) {
                unsigned int slotNum = 0;
                if (sscanf(entry->d_name + prefixLen, "%u", &slotNum) == 1) {
                    if (slotNum < m_maxSlots) {
                        count++;
                    }
                }
            }
        }
    }
    closedir(dir);

    m_cachedSlotCount = static_cast<int>(count);
    return count;
}
