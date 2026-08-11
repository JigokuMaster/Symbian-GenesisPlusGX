#ifndef EMU_UTILS_H
#define EMU_UTILS_H

#include <stddef.h>
#include "SDL.h"

// Helper utilities
void ExtractBaseFilename(const char* fullPath, char* dest, size_t maxLen);
void CalculateAspectRatioRect(int srcW, int srcH, int dstW, int dstH, SDL_Rect* outRect);

class StateManager {
public:
    static const unsigned int DEFAULT_MAX_SLOTS = 1000;
    static const size_t MAX_PATH_LEN = 512;
    static const size_t MAX_NAME_LEN = 256;

    // Construct with state directory (e.g., "saves" or "saves/") and ROM path/name
    StateManager(const char* stateDir, const char* romPathOrName, unsigned int maxSlots = DEFAULT_MAX_SLOTS);
    ~StateManager();

    // 1. Safe Path & Filename Generators
    bool getSlotFilename(unsigned int slot, char* outBuffer, size_t bufferSize) const;
    bool getAutoSaveFilename(char* outBuffer, size_t bufferSize) const;

    // 2. State & Slot Operations
    bool slotExists(unsigned int slot) const;
    bool autoSaveExists() const;
    bool deleteSlot(unsigned int slot);
    bool deleteAutoSave();

    // 3. Properties and Useful Data Accessors
    unsigned int getMaxSlots() const          { return m_maxSlots; }
    int getLastSavedSlot() const              { return m_lastSavedSlot; }
    int getLastLoadedSlot() const             { return m_lastLoadedSlot; }
    const char* getStateDir() const           { return m_stateDir; }
    const char* getRomName() const            { return m_romName; }

    size_t getSlotFileSize(unsigned int slot) const;
    size_t getAutoSaveFileSize() const;

    // External caller notifications
    void notifySlotSaved(unsigned int slot);
    void notifySlotLoaded(unsigned int slot);

    // Dynamic slot state queries
    int getFirstFreeSlot() const;
    unsigned int getSlotCount(); // Scans stateDir once; caches result until slots change
    void refreshCache();         // Forces count recount on next query

private:
    char m_stateDir[MAX_PATH_LEN];
    char m_romName[MAX_NAME_LEN];
    unsigned int m_maxSlots;
    
    // Internal cache state (-1 = uninitialized/dirty)
    int m_cachedSlotCount;
    int m_lastSavedSlot;
    int m_lastLoadedSlot;

    // Helpers
    bool fileExists(const char* filepath) const;
    size_t getFileSize(const char* filepath) const;
    bool buildSlotPath(unsigned int slot, char* outBuf, size_t bufSize) const;
    bool buildAutoSavePath(char* outBuf, size_t bufSize) const;

    // Prevent copying in C++98
    StateManager(const StateManager&);
    StateManager& operator=(const StateManager&);
};

#endif // EMU_UTILS_H
