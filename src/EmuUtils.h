#ifndef EMU_UTILS_H
#define EMU_UTILS_H

#include <stddef.h>
#include "SDL.h"

// Helper utilities

void ExtractBaseFilename(const char* fullPath, char* dest, size_t maxLen);
void CalculateAspectRatioRect(int srcW, int srcH, int dstW, int dstH, SDL_Rect* outRect);

/*
 * High-Performance Software Stretch Routine for RGB565.
 * Replaces SDL_SoftStretch with optimized 16.16 fixed-point math.
 * 
 * Supports source cropping (SrcRect), destination positioning (DstRect),
 * scaling UP/DOWN, and fused 50% CRT scanlines.
 */

inline void FastStretchRectRGB565(const unsigned short* src, int srcStride,
                              int srcX, int srcY, int srcW, int srcH,
                              unsigned short* dst, int dstStride,
                              int dstX, int dstY, int dstW, int dstH,
                              int enableScanlines) {
    int x, y;
    unsigned long xStep, xAcc, yStep, yAcc;
    int xLut[640]; /* Pre-calculated X offsets (max target width 640px) */

    /* Basic safety checks */
    if (!src || !dst || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) {
        return;
    }

    /* Clamp destination width to stack LUT bounds */
    if (dstW > 640) {
        dstW = 640;
    }

    /* 1. Pre-calculate X lookup table relative to srcX offset */
    xStep = ((unsigned long)srcW << 16) / (unsigned long)dstW;
    xAcc = 0;
    for (x = 0; x < dstW; ++x) {
        xLut[x] = srcX + (int)(xAcc >> 16);
        xAcc += xStep;
    }

    /* 2. Setup Y fixed-point accumulator */
    yStep = ((unsigned long)srcH << 16) / (unsigned long)dstH;
    yAcc = 0;

    /* 3. Render loop */
    for (y = 0; y < dstH; ++y) {
        int currentSrcY = srcY + (int)(yAcc >> 16);
        const unsigned short* srcRow = src + (currentSrcY * srcStride);
        
        /* Offset destination pointer to (dstX, dstY + y) */
        unsigned short* dstRow = dst + ((dstY + y) * dstStride) + dstX;

        yAcc += yStep;

        /* Fused scanline row pass */
        if (enableScanlines && (y & 1)) {
            for (x = 0; x < dstW; ++x) {
                unsigned short color = srcRow[xLut[x]];
                dstRow[x] = (unsigned short)((color >> 1) & 0x7BEFU);
            }
        } else {
            /* Standard scaled row pass */
            for (x = 0; x < dstW; ++x) {
                dstRow[x] = srcRow[xLut[x]];
            }
        }
    }
}


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
