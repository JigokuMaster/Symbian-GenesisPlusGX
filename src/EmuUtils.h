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




/*
 * High-Performance Software Stretch Routines for RGB565.
 * Replaces SDL_SoftStretch with optimized 16.16 fixed-point math.
 * 
 * IMPORTANT: 'srcStride' and 'dstStride' MUST be passed in PIXELS (16-bit words),
 * NOT BYTES! Use (surface->pitch / 2) or (surface->pitch / sizeof(uint16_t)).
 */

static inline void FastStretchRectRGB565(const unsigned short* src, int srcStride,
                                         int srcX, int srcY, int srcW, int srcH,
                                         unsigned short* dst, int dstStride,
                                         int dstX, int dstY, int dstW, int dstH,
                                         int enableScanlines) {
    int x, y;
    unsigned long xStep, xAcc, yStep, yAcc;
    int xLut[640]; /* Pre-calculated X offsets (max target width 640px) */

    /* Safety checks */
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

        /* Fused 50% CRT scanline pass */
        if (enableScanlines && (y & 1)) {
            for (x = 0; x < dstW; ++x) {
                unsigned short color = srcRow[xLut[x]];
                /* Shift RGB565 channels by 1 bit while masking color bleed */
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

/*
 * High-Performance Software Bilinear Stretch Routine for RGB565.
 * Uses 16.16 fixed-point math and pre-calculated X/Y interpolation weights.
 * C99 / C++98 compliant, zero STL dependencies.
 */
static inline void FastBilinearRectRGB565(const unsigned short* src, int srcStride,
                                           int srcX, int srcY, int srcW, int srcH,
                                           unsigned short* dst, int dstStride,
                                           int dstX, int dstY, int dstW, int dstH,
                                           int enableScanlines) {
    int x, y;
    unsigned long xStep, yStep, yAcc;
    
    /* Stack LUT for X indices and 8-bit fractions (max target width 640px) */
    int x0Lut[640];
    int x1Lut[640];
    int fxLut[640];

    /* Safety checks */
    if (!src || !dst || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) {
        return;
    }

    if (dstW > 640) {
        dstW = 640;
    }

    /* Special case: 1x1 source scaling fallback to avoid div-by-zero */
    if (srcW == 1 || dstW == 1) {
        FastStretchRectRGB565(src, srcStride, srcX, srcY, srcW, srcH,
                             dst, dstStride, dstX, dstY, dstW, dstH, enableScanlines);
        return;
    }

    /* 1. Pre-calculate X indices and 8-bit blend fractions [0..256] */
    xStep = ((unsigned long)(srcW - 1) << 16) / (unsigned long)(dstW - 1);
    for (x = 0; x < dstW; ++x) {
        unsigned long xAcc = (unsigned long)x * xStep;
        int x0 = srcX + (int)(xAcc >> 16);
        int x1 = (x0 < srcX + srcW - 1) ? x0 + 1 : x0;
        
        x0Lut[x] = x0;
        x1Lut[x] = x1;
        fxLut[x] = (int)((xAcc >> 8) & 0xFF); /* 8-bit fraction */
    }

    /* 2. Setup Y step accumulator */
    yStep = (srcH > 1 && dstH > 1) ? (((unsigned long)(srcH - 1) << 16) / (unsigned long)(dstH - 1)) : 0;
    yAcc = 0;

    /* 3. Render loop */
    for (y = 0; y < dstH; ++y) {
        int y0 = srcY + (int)(yAcc >> 16);
        int y1 = (y0 < srcY + srcH - 1) ? y0 + 1 : y0;
        int fy = (int)((yAcc >> 8) & 0xFF);

        const unsigned short* srcRow0 = src + (y0 * srcStride);
        const unsigned short* srcRow1 = src + (y1 * srcStride);
        unsigned short* dstRow = dst + ((dstY + y) * dstStride) + dstX;

        yAcc += yStep;

        for (x = 0; x < dstW; ++x) {
            int x0 = x0Lut[x];
            int x1 = x1Lut[x];
            int fx = fxLut[x];

            /* Fetch 4 neighbor pixels */
            unsigned short c00 = srcRow0[x0];
            unsigned short c10 = srcRow0[x1];
            unsigned short c01 = srcRow1[x0];
            unsigned short c11 = srcRow1[x1];

            /* Unpack RGB565 channels */
            int r00 = (c00 >> 11) & 0x1F, g00 = (c00 >> 5) & 0x3F, b00 = c00 & 0x1F;
            int r10 = (c10 >> 11) & 0x1F, g10 = (c10 >> 5) & 0x3F, b10 = c10 & 0x1F;
            int r01 = (c01 >> 11) & 0x1F, g01 = (c01 >> 5) & 0x3F, b01 = c01 & 0x1F;
            int r11 = (c11 >> 11) & 0x1F, g11 = (c11 >> 5) & 0x3F, b11 = c11 & 0x1F;

            /* Horizontal linear interpolation */
            int r0 = r00 + (((r10 - r00) * fx) >> 8);
            int g0 = g00 + (((g10 - g00) * fx) >> 8);
            int b0 = b00 + (((b10 - b00) * fx) >> 8);

            int r1 = r01 + (((r11 - r01) * fx) >> 8);
            int g1 = g01 + (((g11 - g01) * fx) >> 8);
            int b1 = b01 + (((b11 - b01) * fx) >> 8);

            /* Vertical linear interpolation */
            int r = r0 + (((r1 - r0) * fy) >> 8);
            int g = g0 + (((g1 - g0) * fy) >> 8);
            int b = b0 + (((b1 - b0) * fy) >> 8);

            unsigned short finalColor = (unsigned short)((r << 11) | (g << 5) | b);

            /* Fused scanlines pass */
            if (enableScanlines && (y & 1)) {
                dstRow[x] = (unsigned short)((finalColor >> 1) & 0x7BEFU);
            } else {
                dstRow[x] = finalColor;
            }
        }
    }
}

#endif // EMU_UTILS_H
