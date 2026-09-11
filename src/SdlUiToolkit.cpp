#include "SdlUiToolkit.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static void PopulateDefaultTheme(UiTheme& theme) {
    theme.bgColor = 0x1111;         
    theme.textColor = 0xCCCC;       
    theme.selectedBgColor = 0x03E0; 
    theme.selectedTextColor = 0xFFFF;
    theme.borderColor = 0x4444;     
    theme.titleBgColor = 0x2124; 
    theme.titleTextColor = 0xFFFF;
}


// =========================================================================
// SdlListbox Widget Implementation
// =========================================================================
SdlListbox::SdlListbox(int x, int y, int width, int height, int itemHeight)
    : iX(x), iY(y), iWidth(width), iHeight(height), iInitialMaxHeight(height),
      iItemHeight(itemHeight), iSelectedIndex(0), iScrollOffset(0), iItemCount(0), 
      iTextYOffset(0), iCenterText(false), iAutoSizeToItems(false) {
    iTitle[0] = '\0';
    iMaxVisibleItems = iHeight / iItemHeight;
    PopulateDefaultTheme(iTheme);
    Clear();
}

SdlListbox::SdlListbox(SDL_Surface* screen, UiLayoutProfile profile, int itemHeight)
    : iSelectedIndex(0), iScrollOffset(0), iItemCount(0), iItemHeight(itemHeight), 
      iTextYOffset(0), iCenterText(false), iAutoSizeToItems(false) {
    iTitle[0] = '\0';

    if (profile == LAYOUT_FULLSCREEN) {
        iX = 5; iY = 5; iWidth = screen->w - 10; iHeight = screen->h - 10;
    } else if (profile == LAYOUT_CENTER_MODAL) { 
        iWidth = (screen->w * 70) / 100; iHeight = (screen->h * 70) / 100;
        iX = (screen->w - iWidth) / 2; iY = (screen->h - iHeight) / 2;
    } else { // LAYOUT_TITLEBAR_FULLSCREEN
        iX = 5; iY = 5; iWidth = screen->w - 10; iHeight = screen->h - 10;
        iTextYOffset = iItemHeight; 
    }

    iInitialMaxHeight = iHeight;
    iMaxVisibleItems = (iHeight - iTextYOffset - 2) / iItemHeight;
    PopulateDefaultTheme(iTheme);
    Clear();
}

SdlListbox::~SdlListbox() {}

void SdlListbox::SetTheme(const UiTheme& theme) { iTheme = theme; }

void SdlListbox::SetTitle(const char* title) {
    if (title) { strncpy(iTitle, title, MAX_UI_STR_LEN - 1); iTitle[MAX_UI_STR_LEN - 1] = '\0'; }
}

void SdlListbox::RecalculateHeight() {
    if (!iAutoSizeToItems) {
        iHeight = iInitialMaxHeight;
    } else {
        int calculatedHeight = (iItemCount * iItemHeight) + iTextYOffset + 4;
        if (calculatedHeight > iInitialMaxHeight) {
            iHeight = iInitialMaxHeight;
        } else {
            iHeight = calculatedHeight;
        }
    }
    
    int availableHeight = iHeight - iTextYOffset - 4;
    iMaxVisibleItems = availableHeight / iItemHeight;
    if (iMaxVisibleItems < 1) iMaxVisibleItems = 1;
}

void SdlListbox::Clear() {
    memset(iItems, 0, sizeof(iItems));
    memset(iWideItems, 0, sizeof(iWideItems));
    memset(iIsItemWide, 0, sizeof(iIsItemWide));
    iItemCount = 0; iSelectedIndex = 0; iScrollOffset = 0;
    RecalculateHeight();
}

bool SdlListbox::AddItem(const char* item) {
    if (iItemCount >= MAX_UI_ITEMS || item == NULL) return false;
    strncpy(iItems[iItemCount], item, MAX_UI_STR_LEN - 1);
    iItems[iItemCount][MAX_UI_STR_LEN - 1] = '\0';
    iIsItemWide[iItemCount] = false;
    iItemCount++;
    RecalculateHeight();
    return true;
}

bool SdlListbox::AddItemW(const Uint16* wideItem) {
    if (iItemCount >= MAX_UI_ITEMS || wideItem == NULL) return false;
    int i = 0;
    while (i < MAX_UI_STR_LEN - 1 && wideItem[i] != 0) {
        iWideItems[iItemCount][i] = wideItem[i];
        i++;
    }
    iWideItems[iItemCount][i] = 0;
    iIsItemWide[iItemCount] = true;
    iItemCount++;
    RecalculateHeight();
    return true;
}

bool SdlListbox::HandleInput(SDL_Event& event) {
    if (event.type != SDL_KEYDOWN) return false;
    switch (event.key.keysym.sym) {
        case SDLK_UP:
            if (iItemCount > 0) {
                if (iSelectedIndex > 0) iSelectedIndex--; else iSelectedIndex = iItemCount - 1;
                EnsureVisible();
            }
            return false;
        case SDLK_DOWN:
            if (iItemCount > 0) {
                if (iSelectedIndex < iItemCount - 1) iSelectedIndex++; else iSelectedIndex = 0;
                EnsureVisible();
            }
            return false;
        case SDLK_RETURN:
            return true; 
        default:
            break;
    }
    return false;
}

void SdlListbox::SetSelectedIndex(int index) {
    if (index >= 0 && index < iItemCount) { iSelectedIndex = index; EnsureVisible(); }
}

void SdlListbox::EnsureVisible() {
    if (iSelectedIndex < iScrollOffset) iScrollOffset = iSelectedIndex;
    if (iSelectedIndex >= iScrollOffset + iMaxVisibleItems) iScrollOffset = iSelectedIndex - iMaxVisibleItems + 1;
}

void SdlListbox::DrawBackground(SDL_Surface* screen) {
    SDL_Rect rect = { (Sint16)iX, (Sint16)iY, (Uint16)iWidth, (Uint16)iHeight };
    SDL_FillRect(screen, &rect, iTheme.bgColor);

    if (iTextYOffset > 0) {
        SDL_Rect titleRect = { (Sint16)(iX + 1), (Sint16)(iY + 1), (Uint16)(iWidth - 2), (Uint16)iTextYOffset };
        SDL_FillRect(screen, &titleRect, iTheme.titleBgColor);
        SDL_Rect titleDivider = { (Sint16)iX, (Sint16)(iY + iTextYOffset + 1), (Uint16)iWidth, 1 };
        SDL_FillRect(screen, &titleDivider, iTheme.borderColor);
    }

    SDL_Rect top = { (Sint16)iX, (Sint16)iY, (Uint16)iWidth, 1 };
    SDL_Rect bottom = { (Sint16)iX, (Sint16)(iY + iHeight - 1), (Uint16)iWidth, 1 };
    SDL_Rect left = { (Sint16)iX, (Sint16)iY, 1, (Uint16)iHeight };
    SDL_Rect right = { (Sint16)(iX + iWidth - 1), (Sint16)iY, 1, (Uint16)iHeight };
    SDL_FillRect(screen, &top, iTheme.borderColor); SDL_FillRect(screen, &bottom, iTheme.borderColor);
    SDL_FillRect(screen, &left, iTheme.borderColor); SDL_FillRect(screen, &right, iTheme.borderColor);
}

void SdlListbox::DrawSelectionBanner(SDL_Surface* screen, int itemY) {
    SDL_Rect rect = { (Sint16)(iX + 1), (Sint16)(itemY + 1), (Uint16)(iWidth - 2), (Uint16)(iItemHeight - 1) };
    SDL_FillRect(screen, &rect, iTheme.selectedBgColor);
}

// -------------------------------------------------------------------------
// BMP RENDERING
// -------------------------------------------------------------------------
void SdlListbox::RenderBMP(SDL_Surface* screen, SDL_Surface* fontSurface) {
    DrawBackground(screen);
    SDL_Rect oldClip; SDL_GetClipRect(screen, &oldClip);

    if (iTextYOffset > 0 && strlen(iTitle) > 0) {
        SDL_Rect titleClip = { (Sint16)(iX + 4), (Sint16)(iY + 1), (Uint16)(iWidth - 8), (Uint16)iTextYOffset };
        SDL_SetClipRect(screen, &titleClip);
        DrawTextBMP(screen, fontSurface, iTitle, iX + 8, iY + (iTextYOffset - 8) / 2 + 1, iCenterText);
    }

    SDL_Rect itemClip = { (Sint16)(iX + 2), (Sint16)(iY + iTextYOffset + 2), (Uint16)(iWidth - 4), (Uint16)(iHeight - iTextYOffset - 4) };
    SDL_SetClipRect(screen, &itemClip);

    int visibleCount = (iItemCount < iMaxVisibleItems) ? iItemCount : iMaxVisibleItems;
    for (int i = 0; i < visibleCount; ++i) {
        int itemIndex = iScrollOffset + i; if (itemIndex >= iItemCount) break;
        int itemY = iY + iTextYOffset + 2 + (i * iItemHeight);

        if (itemIndex == iSelectedIndex) DrawSelectionBanner(screen, itemY);
        if (!iIsItemWide[itemIndex]) {
            DrawTextBMP(screen, fontSurface, iItems[itemIndex], iX + 8, itemY + (iItemHeight - 8) / 2 + 1, iCenterText);
        }
    }
    SDL_SetClipRect(screen, &oldClip);
}

void SdlListbox::DrawTextBMP(SDL_Surface* dest, SDL_Surface* font, const char* text, int x, int y, bool center) {
    if (!text || !font) return;
    int len = strlen(text); 
    int curX = x;
    int charW = 8; int charH = 8;

    if (center) {
        int totalWidth = len * charW;
        curX = iX + (iWidth - totalWidth) / 2;
    }

    for (int i = 0; i < len; ++i) {
        unsigned char c = text[i]; if (c < 32 || c > 126) c = '?';
        int asciiIdx = c - 32; 
        int charsPerRow = font->w / charW; if (charsPerRow <= 0) continue;

        SDL_Rect srcRect = { (Sint16)((asciiIdx % charsPerRow) * charW), (Sint16)((asciiIdx / charsPerRow) * charH), (Uint16)charW, (Uint16)charH };
        SDL_Rect destRect = { (Sint16)curX, (Sint16)y, (Uint16)charW, (Uint16)charH };
        SDL_BlitSurface(font, &srcRect, dest, &destRect);
        curX += charW;
    }
}

// -------------------------------------------------------------------------
// TTF RENDERING
// -------------------------------------------------------------------------
#ifdef HAVE_SDL_TTF
void SdlListbox::RenderTTF(SDL_Surface* screen, TTF_Font* ttfFont) {
    DrawBackground(screen);
    SDL_Rect oldClip; SDL_GetClipRect(screen, &oldClip);

    if (iTextYOffset > 0 && strlen(iTitle) > 0) {
        SDL_Rect titleClip = { (Sint16)(iX + 4), (Sint16)(iY + 1), (Uint16)(iWidth - 8), (Uint16)iTextYOffset };
        SDL_SetClipRect(screen, &titleClip);
        DrawTextTTF(screen, ttfFont, iTitle, iX + 8, iY + (iTextYOffset - 14) / 2 + 1, iTheme.titleTextColor, iCenterText);
    }

    SDL_Rect itemClip = { (Sint16)(iX + 2), (Sint16)(iY + iTextYOffset + 2), (Uint16)(iWidth - 4), (Uint16)(iHeight - iTextYOffset - 4) };
    SDL_SetClipRect(screen, &itemClip);

    int visibleCount = (iItemCount < iMaxVisibleItems) ? iItemCount : iMaxVisibleItems;
    for (int i = 0; i < visibleCount; ++i) {
        int itemIndex = iScrollOffset + i; if (itemIndex >= iItemCount) break;
        int itemY = iY + iTextYOffset + 2 + (i * iItemHeight);
        Uint32 color = iTheme.textColor;

        if (itemIndex == iSelectedIndex) { DrawSelectionBanner(screen, itemY); color = iTheme.selectedTextColor; }
        int textY = itemY + (iItemHeight - 14) / 2 + 1;
        if (iIsItemWide[itemIndex]) DrawTextTTFW(screen, ttfFont, iWideItems[itemIndex], iX + 8, textY, color, iCenterText);
        else DrawTextTTF(screen, ttfFont, iItems[itemIndex], iX + 8, textY, color, iCenterText);
    }
    SDL_SetClipRect(screen, &oldClip);
}

void SdlListbox::DrawTextTTF(SDL_Surface* dest, TTF_Font* font, const char* text, int x, int y, Uint32 color, bool center) {
    if (!text || text[0] == '\0') return;
    SDL_Color sdlColor; SDL_GetRGB(color, dest->format, &sdlColor.r, &sdlColor.g, &sdlColor.b);
    SDL_Surface* textSurf = TTF_RenderText_Blended(font, text, sdlColor);
    if (textSurf) { 
        int renderX = x;
        if (center) renderX = iX + (iWidth - textSurf->w) / 2;
        SDL_Rect d = { (Sint16)renderX, (Sint16)y, 0, 0 }; 
        SDL_BlitSurface(textSurf, NULL, dest, &d); 
        SDL_FreeSurface(textSurf); 
    }
}

void SdlListbox::DrawTextTTFW(SDL_Surface* dest, TTF_Font* font, const Uint16* text, int x, int y, Uint32 color, bool center) {
    if (!text || text[0] == 0) return;
    SDL_Color sdlColor; SDL_GetRGB(color, dest->format, &sdlColor.r, &sdlColor.g, &sdlColor.b);
    SDL_Surface* textSurf = TTF_RenderUNICODE_Blended(font, text, sdlColor);
    if (textSurf) { 
        int renderX = x;
        if (center) renderX = iX + (iWidth - textSurf->w) / 2;
        SDL_Rect d = { (Sint16)renderX, (Sint16)y, 0, 0 }; 
        SDL_BlitSurface(textSurf, NULL, dest, &d); 
        SDL_FreeSurface(textSurf); 
    }
}
#endif

// -------------------------------------------------------------------------
// STB TRUETYPE RENDERING
// -------------------------------------------------------------------------
#ifdef HAVE_STB_TRUETYPE
void SdlListbox::RenderSTB(SDL_Surface* screen, const unsigned char* ttfBuffer, float fontHeight) {
    if (!ttfBuffer) return;
    stbtt_fontinfo font; if (!stbtt_InitFont(&font, ttfBuffer, 0)) return;
    float scale = stbtt_ScaleForPixelHeight(&font, fontHeight);
    int ascent, descent, lineGap; stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);

    DrawBackground(screen);
    SDL_Rect oldClip; SDL_GetClipRect(screen, &oldClip);

    if (iTextYOffset > 0 && strlen(iTitle) > 0) {
        SDL_Rect titleClip = { (Sint16)(iX + 4), (Sint16)(iY + 1), (Uint16)(iWidth - 8), (Uint16)iTextYOffset };
        SDL_SetClipRect(screen, &titleClip);
        int titleBaseline = iY + (iTextYOffset - (int)fontHeight) / 2 + (int)(ascent * scale) + 1;
        DrawTextSTB(screen, font, scale, titleBaseline, iTitle, iX + 8, iTheme.titleTextColor, iCenterText);
    }

    SDL_Rect itemClip = { (Sint16)(iX + 2), (Sint16)(iY + iTextYOffset + 2), (Uint16)(iWidth - 4), (Uint16)(iHeight - iTextYOffset - 4) };
    SDL_SetClipRect(screen, &itemClip);

    int visibleCount = (iItemCount < iMaxVisibleItems) ? iItemCount : iMaxVisibleItems;
    for (int i = 0; i < visibleCount; ++i) {
        int itemIndex = iScrollOffset + i; if (itemIndex >= iItemCount) break;
        int itemY = iY + iTextYOffset + 2 + (i * iItemHeight);
        Uint32 color = iTheme.textColor;

        if (itemIndex == iSelectedIndex) { DrawSelectionBanner(screen, itemY); color = iTheme.selectedTextColor; }
        int baselineY = itemY + (iItemHeight - (int)(fontHeight)) / 2 + (int)(ascent * scale) + 1;
        if (iIsItemWide[itemIndex]) DrawTextSTBW(screen, font, scale, baselineY, iWideItems[itemIndex], iX + 8, color, iCenterText);
        else DrawTextSTB(screen, font, scale, baselineY, iItems[itemIndex], iX + 8, color, iCenterText);
    }
    SDL_SetClipRect(screen, &oldClip);
}

void SdlListbox::DrawTextSTB(SDL_Surface* dest, stbtt_fontinfo& font, float scale, int baseline, const char* text, int x, Uint32 color, bool center) {
    if (!text) return;
    Uint8 r, g, b; SDL_GetRGB(color, dest->format, &r, &g, &b);
    int len = strlen(text); 
    int currentX = x;

    if (center) {
        int totalWidth = 0;
        for (int i = 0; i < len; ++i) {
            int advance, lsb;
            stbtt_GetCodepointHMetrics(&font, text[i], &advance, &lsb);
            totalWidth += (int)(advance * scale);
        }
        currentX = iX + (iWidth - totalWidth) / 2;
    }

    for (int i = 0; i < len; ++i) {
        int advance, lsb; stbtt_GetCodepointHMetrics(&font, text[i], &advance, &lsb);
        int bitmapW, bitmapH, xOffset, yOffset;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, scale, scale, text[i], &bitmapW, &bitmapH, &xOffset, &yOffset);
        if (bitmap) {
            if (SDL_MUSTLOCK(dest)) SDL_LockSurface(dest);
            int targetYStart = baseline + yOffset;
            for (int srcY = 0; srcY < bitmapH; ++srcY) {
                int targetY = targetYStart + srcY; if (targetY < 0 || targetY >= dest->h) continue;
                Uint8* pixelLinePtr = (Uint8*)dest->pixels + targetY * dest->pitch;
                int baseTargetX = currentX + (int)(lsb * scale) + xOffset;
                for (int srcX = 0; srcX < bitmapW; ++srcX) {
                    int targetX = baseTargetX + srcX; if (targetX < 0 || targetX >= dest->w) continue;
                    unsigned char alpha = bitmap[srcY * bitmapW + srcX]; if (alpha == 0) continue;
                    Uint8* pixelPtr = pixelLinePtr + targetX * dest->format->BytesPerPixel;
                    Uint32 destPixel = (dest->format->BytesPerPixel == 2) ? *(Uint16*)pixelPtr : *(Uint32*)pixelPtr;
                    Uint8 destR, destG, destB; SDL_GetRGB(destPixel, dest->format, &destR, &destG, &destB);
                    Uint32 finalPixel = SDL_MapRGB(dest->format, (Uint8)(((r - destR) * alpha) / 255 + destR), (Uint8)(((g - destG) * alpha) / 255 + destG), (Uint8)(((b - destB) * alpha) / 255 + destB));
                    if (dest->format->BytesPerPixel == 2) *(Uint16*)pixelPtr = (Uint16)finalPixel; else *(Uint32*)pixelPtr = finalPixel;
                }
            }
            if (SDL_MUSTLOCK(dest)) SDL_UnlockSurface(dest);
            stbtt_FreeBitmap(bitmap, NULL);
        }
        currentX += (int)(advance * scale);
    }
}

void SdlListbox::DrawTextSTBW(SDL_Surface* dest, stbtt_fontinfo& font, float scale, int baseline, const Uint16* text, int x, Uint32 color, bool center) {
    if (!text) return;
    Uint8 r, g, b; SDL_GetRGB(color, dest->format, &r, &g, &b);
    int currentX = x; 
    
    if (center) {
        int totalWidth = 0;
        int idx = 0;
        while (text[idx] != 0) {
            int advance, lsb;
            stbtt_GetCodepointHMetrics(&font, text[idx], &advance, &lsb);
            totalWidth += (int)(advance * scale);
            idx++;
        }
        currentX = iX + (iWidth - totalWidth) / 2;
    }

    int i = 0;
    while (text[i] != 0) {
        int advance, lsb; stbtt_GetCodepointHMetrics(&font, text[i], &advance, &lsb);
        int bitmapW, bitmapH, xOffset, yOffset;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, scale, scale, text[i], &bitmapW, &bitmapH, &xOffset, &yOffset);
        if (bitmap) {
            if (SDL_MUSTLOCK(dest)) SDL_LockSurface(dest);
            int targetYStart = baseline + yOffset;
            for (int srcY = 0; srcY < bitmapH; ++srcY) {
                int targetY = targetYStart + srcY; if (targetY < 0 || targetY >= dest->h) continue;
                Uint8* pixelLinePtr = (Uint8*)dest->pixels + targetY * dest->pitch;
                int baseTargetX = currentX + (int)(lsb * scale) + xOffset;
                for (int srcX = 0; srcX < bitmapW; ++srcX) {
                    int targetX = baseTargetX + srcX; if (targetX < 0 || targetX >= dest->w) continue;
                    unsigned char alpha = bitmap[srcY * bitmapW + srcX]; if (alpha == 0) continue;
                    Uint8* pixelPtr = pixelLinePtr + targetX * dest->format->BytesPerPixel;
                    Uint32 destPixel = (dest->format->BytesPerPixel == 2) ? *(Uint16*)pixelPtr : *(Uint32*)pixelPtr;
                    Uint8 destR, destG, destB; SDL_GetRGB(destPixel, dest->format, &destR, &destG, &destB);
                    Uint32 finalPixel = SDL_MapRGB(dest->format, (Uint8)(((r - destR) * alpha) / 255 + destR), (Uint8)(((g - destG) * alpha) / 255 + destG), (Uint8)(((b - destB) * alpha) / 255 + destB));
                    if (dest->format->BytesPerPixel == 2) *(Uint16*)pixelPtr = (Uint16)finalPixel; else *(Uint32*)pixelPtr = finalPixel;
                }
            }
            if (SDL_MUSTLOCK(dest)) SDL_UnlockSurface(dest);
            stbtt_FreeBitmap(bitmap, NULL);
        }
        currentX += (int)(advance * scale); i++;
    }
}
#endif


#if 0 // cmnt
// =========================================================================
// SdlListbox Widget Implementation
// =========================================================================
SdlListbox::SdlListbox(int x, int y, int width, int height, int itemHeight)
    : iX(x), iY(y), iWidth(width), iHeight(height), iItemHeight(itemHeight),
      iSelectedIndex(0), iScrollOffset(0), iItemCount(0), iTextYOffset(0) {
    iTitle[0] = '\0';
    iMaxVisibleItems = iHeight / iItemHeight;
    PopulateDefaultTheme(iTheme);
    Clear();
}

SdlListbox::SdlListbox(SDL_Surface* screen, UiLayoutProfile profile, int itemHeight)
    : iSelectedIndex(0), iScrollOffset(0), iItemCount(0), iItemHeight(itemHeight), iTextYOffset(0) {
    iTitle[0] = '\0';

    if (profile == LAYOUT_FULLSCREEN) {
        iX = 5; iY = 5; iWidth = screen->w - 10; iHeight = screen->h - 10;
    } else if (profile == LAYOUT_CENTER_MODAL) { 
        iWidth = (screen->w * 70) / 100; iHeight = (screen->h * 70) / 100;
        iX = (screen->w - iWidth) / 2; iY = (screen->h - iHeight) / 2;
    } else { // LAYOUT_TITLEBAR_FULLSCREEN
        iX = 5; iY = 5; iWidth = screen->w - 10; iHeight = screen->h - 10;
        iTextYOffset = iItemHeight; 
    }

    iMaxVisibleItems = (iHeight - iTextYOffset - 2) / iItemHeight;
    PopulateDefaultTheme(iTheme);
    Clear();
}

SdlListbox::~SdlListbox() {}
void SdlListbox::SetTheme(const UiTheme& theme) { iTheme = theme; }

void SdlListbox::SetTitle(const char* title) {
    if (title) { strncpy(iTitle, title, MAX_UI_STR_LEN - 1); iTitle[MAX_UI_STR_LEN - 1] = '\0'; }
}

void SdlListbox::Clear() {
    memset(iItems, 0, sizeof(iItems));
    memset(iWideItems, 0, sizeof(iWideItems));
    memset(iIsItemWide, 0, sizeof(iIsItemWide));
    iItemCount = 0; iSelectedIndex = 0; iScrollOffset = 0;
}

// Fully Restored: Standard layout and string assignment functionality
bool SdlListbox::AddItem(const char* item) {
    if (iItemCount >= MAX_UI_ITEMS || item == NULL) return false;
    strncpy(iItems[iItemCount], item, MAX_UI_STR_LEN - 1);
    iItems[iItemCount][MAX_UI_STR_LEN - 1] = '\0';
    iIsItemWide[iItemCount] = false;
    iItemCount++;
    return true;
}

// Fully Restored: Wide character array string assignment functionality
bool SdlListbox::AddItemW(const Uint16* wideItem) {
    if (iItemCount >= MAX_UI_ITEMS || wideItem == NULL) return false;
    int i = 0;
    while (i < MAX_UI_STR_LEN - 1 && wideItem[i] != 0) {
        iWideItems[iItemCount][i] = wideItem[i];
        i++;
    }
    iWideItems[iItemCount][i] = 0;
    iIsItemWide[iItemCount] = true;
    iItemCount++;
    return true;
}

bool SdlListbox::HandleInput(SDL_Event& event) {
    if (event.type != SDL_KEYDOWN) return false;
    switch (event.key.keysym.sym) {
        case SDLK_UP:
            if (iItemCount > 0) {
                if (iSelectedIndex > 0) iSelectedIndex--; else iSelectedIndex = iItemCount - 1;
                EnsureVisible();
            }
            return false;
        case SDLK_DOWN:
            if (iItemCount > 0) {
                if (iSelectedIndex < iItemCount - 1) iSelectedIndex++; else iSelectedIndex = 0;
                EnsureVisible();
            }
            return false;
        case SDLK_RETURN:
            return true; 
        default:
            break;
    }
    return false;
}

void SdlListbox::SetSelectedIndex(int index) {
    if (index >= 0 && index < iItemCount) { iSelectedIndex = index; EnsureVisible(); }
}

void SdlListbox::EnsureVisible() {
    if (iSelectedIndex < iScrollOffset) iScrollOffset = iSelectedIndex;
    if (iSelectedIndex >= iScrollOffset + iMaxVisibleItems) iScrollOffset = iSelectedIndex - iMaxVisibleItems + 1;
}

void SdlListbox::DrawBackground(SDL_Surface* screen) {
    SDL_Rect rect = { (Sint16)iX, (Sint16)iY, (Uint16)iWidth, (Uint16)iHeight };
    SDL_FillRect(screen, &rect, iTheme.bgColor);

    if (iTextYOffset > 0) {
        SDL_Rect titleRect = { (Sint16)(iX + 1), (Sint16)(iY + 1), (Uint16)(iWidth - 2), (Uint16)iTextYOffset };
        SDL_FillRect(screen, &titleRect, iTheme.titleBgColor);
        SDL_Rect titleDivider = { (Sint16)iX, (Sint16)(iY + iTextYOffset + 1), (Uint16)iWidth, 1 };
        SDL_FillRect(screen, &titleDivider, iTheme.borderColor);
    }

    SDL_Rect top = { (Sint16)iX, (Sint16)iY, (Uint16)iWidth, 1 };
    SDL_Rect bottom = { (Sint16)iX, (Sint16)(iY + iHeight - 1), (Uint16)iWidth, 1 };
    SDL_Rect left = { (Sint16)iX, (Sint16)iY, 1, (Uint16)iHeight };
    SDL_Rect right = { (Sint16)(iX + iWidth - 1), (Sint16)iY, 1, (Uint16)iHeight };
    SDL_FillRect(screen, &top, iTheme.borderColor); SDL_FillRect(screen, &bottom, iTheme.borderColor);
    SDL_FillRect(screen, &left, iTheme.borderColor); SDL_FillRect(screen, &right, iTheme.borderColor);
}

void SdlListbox::DrawSelectionBanner(SDL_Surface* screen, int itemY) {
    // Corrected: Uses clear internal bounds tracking to prevent border overlap
    SDL_Rect rect = { (Sint16)(iX + 1), (Sint16)(itemY + 1), (Uint16)(iWidth - 2), (Uint16)(iItemHeight - 1) };
    SDL_FillRect(screen, &rect, iTheme.selectedBgColor);
}

void SdlListbox::RenderBMP(SDL_Surface* screen, SDL_Surface* fontSurface) {
    DrawBackground(screen);
    SDL_Rect oldClip; SDL_GetClipRect(screen, &oldClip);

    if (iTextYOffset > 0 && strlen(iTitle) > 0) {
        SDL_Rect titleClip = { (Sint16)(iX + 4), (Sint16)(iY + 1), (Uint16)(iWidth - 8), (Uint16)iTextYOffset };
        SDL_SetClipRect(screen, &titleClip);
        DrawTextBMP(screen, fontSurface, iTitle, iX + 8, iY + (iTextYOffset - 8) / 2 + 1);
    }

    SDL_Rect itemClip = { (Sint16)(iX + 2), (Sint16)(iY + iTextYOffset + 2), (Uint16)(iWidth - 4), (Uint16)(iHeight - iTextYOffset - 4) };
    SDL_SetClipRect(screen, &itemClip);

    int visibleCount = (iItemCount < iMaxVisibleItems) ? iItemCount : iMaxVisibleItems;
    for (int i = 0; i < visibleCount; ++i) {
        int itemIndex = iScrollOffset + i; if (itemIndex >= iItemCount) break;
        int itemY = iY + iTextYOffset + 2 + (i * iItemHeight);

        if (itemIndex == iSelectedIndex) DrawSelectionBanner(screen, itemY);
        if (!iIsItemWide[itemIndex]) {
            DrawTextBMP(screen, fontSurface, iItems[itemIndex], iX + 8, itemY + (iItemHeight - 8) / 2 + 1);
        }
    }
    SDL_SetClipRect(screen, &oldClip);
}

void SdlListbox::DrawTextBMP(SDL_Surface* dest, SDL_Surface* font, const char* text, int x, int y) {
    if (!text || !font) return;
    int len = strlen(text); int curX = x;
    for (int i = 0; i < len; ++i) {
        unsigned char c = text[i]; if (c < 32 || c > 126) c = '?';
        int asciiIdx = c - 32; int charW = 8; int charH = 8;
        int charsPerRow = font->w / charW; if (charsPerRow <= 0) continue;

        SDL_Rect srcRect = { (Sint16)((asciiIdx % charsPerRow) * charW), (Sint16)((asciiIdx / charsPerRow) * charH), (Uint16)charW, (Uint16)charH };
        SDL_Rect destRect = { (Sint16)curX, (Sint16)y, (Uint16)charW, (Uint16)charH };
        SDL_BlitSurface(font, &srcRect, dest, &destRect);
        curX += charW;
    }
}

#ifdef HAVE_SDL_TTF
void SdlListbox::RenderTTF(SDL_Surface* screen, TTF_Font* ttfFont) {
    DrawBackground(screen);
    SDL_Rect oldClip; SDL_GetClipRect(screen, &oldClip);

    if (iTextYOffset > 0 && strlen(iTitle) > 0) {
        SDL_Rect titleClip = { (Sint16)(iX + 4), (Sint16)(iY + 1), (Uint16)(iWidth - 8), (Uint16)iTextYOffset };
        SDL_SetClipRect(screen, &titleClip);
        DrawTextTTF(screen, ttfFont, iTitle, iX + 8, iY + (iTextYOffset - 14) / 2 + 1, iTheme.titleTextColor);
    }

    SDL_Rect itemClip = { (Sint16)(iX + 2), (Sint16)(iY + iTextYOffset + 2), (Uint16)(iWidth - 4), (Uint16)(iHeight - iTextYOffset - 4) };
    SDL_SetClipRect(screen, &itemClip);

    int visibleCount = (iItemCount < iMaxVisibleItems) ? iItemCount : iMaxVisibleItems;
    for (int i = 0; i < visibleCount; ++i) {
        int itemIndex = iScrollOffset + i; if (itemIndex >= iItemCount) break;
        int itemY = iY + iTextYOffset + 2 + (i * iItemHeight);
        Uint32 color = iTheme.textColor;

        if (itemIndex == iSelectedIndex) { DrawSelectionBanner(screen, itemY); color = iTheme.selectedTextColor; }
        int textY = itemY + (iItemHeight - 14) / 2 + 1;
        if (iIsItemWide[itemIndex]) DrawTextTTFW(screen, ttfFont, iWideItems[itemIndex], iX + 8, textY, color);
        else DrawTextTTF(screen, ttfFont, iItems[itemIndex], iX + 8, textY, color);
    }
    SDL_SetClipRect(screen, &oldClip);
}

void SdlListbox::DrawTextTTF(SDL_Surface* dest, TTF_Font* font, const char* text, int x, int y, Uint32 color) {
    if (!text || text[0] == '\0') return;
    SDL_Color sdlColor; SDL_GetRGB(color, dest->format, &sdlColor.r, &sdlColor.g, &sdlColor.b);
    SDL_Surface* textSurf = TTF_RenderText_Blended(font, text, sdlColor);
    if (textSurf) { SDL_Rect d = { (Sint16)x, (Sint16)y, 0, 0 }; SDL_BlitSurface(textSurf, NULL, dest, &d); SDL_FreeSurface(textSurf); }
}

void SdlListbox::DrawTextTTFW(SDL_Surface* dest, TTF_Font* font, const Uint16* text, int x, int y, Uint32 color) {
    if (!text || text[0] == 0) return;
    SDL_Color sdlColor; SDL_GetRGB(color, dest->format, &sdlColor.r, &sdlColor.g, &sdlColor.b);
    SDL_Surface* textSurf = TTF_RenderUNICODE_Blended(font, text, sdlColor);
    if (textSurf) { SDL_Rect d = { (Sint16)x, (Sint16)y, 0, 0 }; SDL_BlitSurface(textSurf, NULL, dest, &d); SDL_FreeSurface(textSurf); }
}
#endif

#ifdef HAVE_STB_TRUETYPE
void SdlListbox::RenderSTB(SDL_Surface* screen, const unsigned char* ttfBuffer, float fontHeight) {
    if (!ttfBuffer) return;
    stbtt_fontinfo font; if (!stbtt_InitFont(&font, ttfBuffer, 0)) return;
    float scale = stbtt_ScaleForPixelHeight(&font, fontHeight);
    int ascent, descent, lineGap; stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);

    DrawBackground(screen);
    SDL_Rect oldClip; SDL_GetClipRect(screen, &oldClip);

    if (iTextYOffset > 0 && strlen(iTitle) > 0) {
        SDL_Rect titleClip = { (Sint16)(iX + 4), (Sint16)(iY + 1), (Uint16)(iWidth - 8), (Uint16)iTextYOffset };
        SDL_SetClipRect(screen, &titleClip);
        int titleBaseline = iY + (iTextYOffset - (int)fontHeight) / 2 + (int)(ascent * scale) + 1;
        DrawTextSTB(screen, font, scale, titleBaseline, iTitle, iX + 8, iTheme.titleTextColor);
    }

    SDL_Rect itemClip = { (Sint16)(iX + 2), (Sint16)(iY + iTextYOffset + 2), (Uint16)(iWidth - 4), (Uint16)(iHeight - iTextYOffset - 4) };
    SDL_SetClipRect(screen, &itemClip);

    int visibleCount = (iItemCount < iMaxVisibleItems) ? iItemCount : iMaxVisibleItems;
    for (int i = 0; i < visibleCount; ++i) {
        int itemIndex = iScrollOffset + i; if (itemIndex >= iItemCount) break;
        int itemY = iY + iTextYOffset + 2 + (i * iItemHeight);
        Uint32 color = iTheme.textColor;

        if (itemIndex == iSelectedIndex) { DrawSelectionBanner(screen, itemY); color = iTheme.selectedTextColor; }
        int baselineY = itemY + (iItemHeight - (int)(fontHeight)) / 2 + (int)(ascent * scale) + 1;
        if (iIsItemWide[itemIndex]) DrawTextSTBW(screen, font, scale, baselineY, iWideItems[itemIndex], iX + 8, color);
        else DrawTextSTB(screen, font, scale, baselineY, iItems[itemIndex], iX + 8, color);
    }
    SDL_SetClipRect(screen, &oldClip);
}

void SdlListbox::DrawTextSTB(SDL_Surface* dest, stbtt_fontinfo& font, float scale, int baseline, const char* text, int x, Uint32 color) {
    if (!text) return;
    Uint8 r, g, b; SDL_GetRGB(color, dest->format, &r, &g, &b);
    int len = strlen(text); int currentX = x;
    for (int i = 0; i < len; ++i) {
        int advance, lsb; stbtt_GetCodepointHMetrics(&font, text[i], &advance, &lsb);
        int bitmapW, bitmapH, xOffset, yOffset;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, scale, scale, text[i], &bitmapW, &bitmapH, &xOffset, &yOffset);
        if (bitmap) {
            if (SDL_MUSTLOCK(dest)) SDL_LockSurface(dest);
            int targetYStart = baseline + yOffset;
            for (int srcY = 0; srcY < bitmapH; ++srcY) {
                int targetY = targetYStart + srcY; if (targetY < 0 || targetY >= dest->h) continue;
                Uint8* pixelLinePtr = (Uint8*)dest->pixels + targetY * dest->pitch;
                int baseTargetX = currentX + (int)(lsb * scale) + xOffset;
                for (int srcX = 0; srcX < bitmapW; ++srcX) {
                    int targetX = baseTargetX + srcX; if (targetX < 0 || targetX >= dest->w) continue;
                    unsigned char alpha = bitmap[srcY * bitmapW + srcX]; if (alpha == 0) continue;
                    Uint8* pixelPtr = pixelLinePtr + targetX * dest->format->BytesPerPixel;
                    Uint32 destPixel = (dest->format->BytesPerPixel == 2) ? *(Uint16*)pixelPtr : *(Uint32*)pixelPtr;
                    Uint8 destR, destG, destB; SDL_GetRGB(destPixel, dest->format, &destR, &destG, &destB);
                    Uint32 finalPixel = SDL_MapRGB(dest->format, (Uint8)(((r - destR) * alpha) / 255 + destR), (Uint8)(((g - destG) * alpha) / 255 + destG), (Uint8)(((b - destB) * alpha) / 255 + destB));
                    if (dest->format->BytesPerPixel == 2) *(Uint16*)pixelPtr = (Uint16)finalPixel; else *(Uint32*)pixelPtr = finalPixel;
                }
            }
            if (SDL_MUSTLOCK(dest)) SDL_UnlockSurface(dest);
            stbtt_FreeBitmap(bitmap, NULL);
        }
        currentX += (int)(advance * scale);
    }
}

void SdlListbox::DrawTextSTBW(SDL_Surface* dest, stbtt_fontinfo& font, float scale, int baseline, const Uint16* text, int x, Uint32 color) {
    if (!text) return;
    Uint8 r, g, b; SDL_GetRGB(color, dest->format, &r, &g, &b);
    int currentX = x; int i = 0;
    while (text[i] != 0) {
        int advance, lsb; stbtt_GetCodepointHMetrics(&font, text[i], &advance, &lsb);
        int bitmapW, bitmapH, xOffset, yOffset;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, scale, scale, text[i], &bitmapW, &bitmapH, &xOffset, &yOffset);
        if (bitmap) {
            if (SDL_MUSTLOCK(dest)) SDL_LockSurface(dest);
            int targetYStart = baseline + yOffset;
            for (int srcY = 0; srcY < bitmapH; ++srcY) {
                int targetY = targetYStart + srcY; if (targetY < 0 || targetY >= dest->h) continue;
                Uint8* pixelLinePtr = (Uint8*)dest->pixels + targetY * dest->pitch;
                int baseTargetX = currentX + (int)(lsb * scale) + xOffset;
                for (int srcX = 0; srcX < bitmapW; ++srcX) {
                    int targetX = baseTargetX + srcX; if (targetX < 0 || targetX >= dest->w) continue;
                    unsigned char alpha = bitmap[srcY * bitmapW + srcX]; if (alpha == 0) continue;
                    Uint8* pixelPtr = pixelLinePtr + targetX * dest->format->BytesPerPixel;
                    Uint32 destPixel = (dest->format->BytesPerPixel == 2) ? *(Uint16*)pixelPtr : *(Uint32*)pixelPtr;
                    Uint8 destR, destG, destB; SDL_GetRGB(destPixel, dest->format, &destR, &destG, &destB);
                    Uint32 finalPixel = SDL_MapRGB(dest->format, (Uint8)(((r - destR) * alpha) / 255 + destR), (Uint8)(((g - destG) * alpha) / 255 + destG), (Uint8)(((b - destB) * alpha) / 255 + destB));
                    if (dest->format->BytesPerPixel == 2) *(Uint16*)pixelPtr = (Uint16)finalPixel; else *(Uint32*)pixelPtr = finalPixel;
                }
            }
            if (SDL_MUSTLOCK(dest)) SDL_UnlockSurface(dest);
            stbtt_FreeBitmap(bitmap, NULL);
        }
        currentX += (int)(advance * scale); i++;
    }
}
#endif

#endif // cmnt

// =========================================================================
// SdlMessageBox Widget Implementation
// =========================================================================

static void PopulateMsgBoxTheme(MsgBoxTheme& theme) {
    theme.bgColor = 0x1111;          // Dark slate grey match
    theme.textColor = 0xCCCC;        // Standard body grey
    theme.borderColor = 0x4444;      // Wire border accent
    theme.titleBgColor = 0x2124;     // Viewpager style title block background
    theme.titleTextColor = 0xFFFF;   // Solid white title text
    theme.footerTextColor = 0x03E0;  // Green accent color matching list selection tones
}

SdlMessageBox::SdlMessageBox(SDL_Surface* screen, int itemHeight)
    : iItemHeight(itemHeight), iLineCount(0), iX(0), iY(0), 
    iWidth(0), iHeight(0), iScreen(screen), iConfirmKey(SDLK_UNKNOWN) {
    PopulateMsgBoxTheme(iTheme);
    Clear();
}

SdlMessageBox::~SdlMessageBox() {}

void SdlMessageBox::SetTheme(const MsgBoxTheme& theme) { iTheme = theme; }

void SdlMessageBox::SetTitle(const char* title) {
    if (title) { strncpy(iTitle, title, MAX_MSG_BOX_STR - 1); iTitle[MAX_MSG_BOX_STR - 1] = '\0'; }
}

void SdlMessageBox::SetConfirmText(const char* confirmText) {
    if (confirmText) { strncpy(iConfirmText, confirmText, MAX_MSG_BOX_STR - 1); iConfirmText[MAX_MSG_BOX_STR - 1] = '\0'; }
}

void SdlMessageBox::SetConfirmKey(const SDLKey k) {
    iConfirmKey = k;
}


void SdlMessageBox::Clear() {
    iTitle[0] = '\0';
    for (int i = 0; i < MAX_MSG_LINES; i++) iLines[i][0] = '\0';
    strcpy(iConfirmText, "< Press ENTER to close >");
    iLineCount = 0;
}

bool SdlMessageBox::AddLine(const char* messageLine) {
    if (iLineCount >= MAX_MSG_LINES || !messageLine) return false;
    strncpy(iLines[iLineCount], messageLine, MAX_MSG_BOX_STR - 1);
    iLines[iLineCount][MAX_MSG_BOX_STR - 1] = '\0';
    iLineCount++;
    return true;
}

void SdlMessageBox::CalculateGeometry(SDL_Surface* screen) {
    // Standard ViewPager percentage mapping
    iWidth = (screen->w * 80) / 100;
    
    int titleHeight = (strlen(iTitle) > 0) ? (iItemHeight + 6) : 0;
    int textRowsHeight = ((iLineCount > 0) ? iLineCount : 1) * iItemHeight;
    int footerHeight = iItemHeight + 12;
    
    iHeight = titleHeight + textRowsHeight + footerHeight + 16;

    // Boundary constraints clamp
    if (iHeight < (screen->h * 32) / 100) iHeight = (screen->h * 32) / 100;
    if (iHeight > (screen->h * 80) / 100) iHeight = (screen->h * 80) / 100;

    iX = (screen->w - iWidth) / 2;
    iY = (screen->h - iHeight) / 2;
}

void SdlMessageBox::DrawFrameCommon(SDL_Surface* screen, int& innerYStart) {
    CalculateGeometry(screen);

    // Box background profile
    SDL_Rect rect = { (Sint16)iX, (Sint16)iY, (Uint16)iWidth, (Uint16)iHeight };
    SDL_FillRect(screen, NULL, 0);
    SDL_FillRect(screen, &rect, iTheme.bgColor);

    // Bounding outer frame wire
    SDL_Rect top = { (Sint16)iX, (Sint16)iY, (Uint16)iWidth, 1 };
    SDL_Rect bottom = { (Sint16)iX, (Sint16)(iY + iHeight - 1), (Uint16)iWidth, 1 };
    SDL_Rect left = { (Sint16)iX, (Sint16)iY, 1, (Uint16)iHeight };
    SDL_Rect right = { (Sint16)(iX + iWidth - 1), (Sint16)iY, 1, (Uint16)iHeight };
    SDL_FillRect(screen, &top, iTheme.borderColor); SDL_FillRect(screen, &bottom, iTheme.borderColor);
    SDL_FillRect(screen, &left, iTheme.borderColor); SDL_FillRect(screen, &right, iTheme.borderColor);

    innerYStart = iY + 6;
    
    if (strlen(iTitle) > 0) {
        int titleBarHeight = iItemHeight + 4;
        SDL_Rect titleRect = { (Sint16)(iX + 1), (Sint16)(iY + 1), (Uint16)(iWidth - 2), (Uint16)titleBarHeight };
        SDL_FillRect(screen, &titleRect, iTheme.titleBgColor);
        
        SDL_Rect divider = { (Sint16)iX, (Sint16)(iY + titleBarHeight + 1), (Uint16)iWidth, 1 };
        SDL_FillRect(screen, &divider, iTheme.borderColor);
        innerYStart += titleBarHeight + 4;
    }
}

bool SdlMessageBox::ShowBMP(SDL_Surface* fontSurface) {
    SDL_Surface* screen = iScreen;
    bool waiting = true;
    bool confirmed = false;
    SDL_Event ev;

    while (waiting && SDL_WaitEvent(&ev)) {
        if (ev.type == SDL_KEYDOWN) {
            if (ev.key.keysym.sym == SDLK_RETURN) { confirmed = true; waiting = false; }
            if (ev.key.keysym.sym == SDLK_ESCAPE) { confirmed = false; waiting = false; }
        }

        int startY;
        DrawFrameCommon(screen, startY);

        // Render Title row header
        if (strlen(iTitle) > 0) {
            DrawTextBMP(screen, fontSurface, iTitle, iX + (iWidth / 2), iY + (iItemHeight - 8) / 2 + 3, true);
        }

        // Render stacked message lines
        int currentTextY = startY + 4;
        for (int i = 0; i < iLineCount; i++) {
            DrawTextBMP(screen, fontSurface, iLines[i], iX + (iWidth / 2), currentTextY + (iItemHeight - 8) / 2, true);
            currentTextY += iItemHeight;
        }

        // Render non-highlighted, clean centered text string footer row
        int footerY = iY + iHeight - iItemHeight - 10;
        DrawTextBMP(screen, fontSurface, iConfirmText, iX + (iWidth / 2), footerY, true);

        SDL_Flip(screen);
    }
    return confirmed;
}

void SdlMessageBox::DrawTextBMP(SDL_Surface* dest, SDL_Surface* font, const char* text, int x, int y, bool center) {
    if (!text || !font) return;
    int len = strlen(text);
    int charW = 8; int charH = 8;
    int curX = center ? (x - (len * charW) / 2) : x;

    for (int i = 0; i < len; ++i) {
        unsigned char c = text[i]; if (c < 32 || c > 126) c = '?';
        int asciiIdx = c - 32;
        int charsPerRow = font->w / charW; if (charsPerRow <= 0) continue;

        SDL_Rect srcRect = { (Sint16)((asciiIdx % charsPerRow) * charW), (Sint16)((asciiIdx / charsPerRow) * charH), (Uint16)charW, (Uint16)charH };
        SDL_Rect destRect = { (Sint16)curX, (Sint16)y, (Uint16)charW, (Uint16)charH };
        SDL_BlitSurface(font, &srcRect, dest, &destRect);
        curX += charW;
    }
}

void SdlMessageBox::RenderBMP(SDL_Surface* fontSurface)
{

}

#ifdef HAVE_STB_TRUETYPE

void SdlMessageBox::RenderSTB(const unsigned char* ttfBuffer, float fontHeight)
{
    SDL_Surface* screen = iScreen;

    stbtt_fontinfo font; if (!stbtt_InitFont(&font, ttfBuffer, 0)) return;
    float scale = stbtt_ScaleForPixelHeight(&font, fontHeight);
    int ascent, descent, lineGap; stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);

    int startY;
    DrawFrameCommon(screen, startY);

    // Render Title
    if (strlen(iTitle) > 0) {
        int titleBaseline = iY + (iItemHeight - (int)fontHeight) / 2 + (int)(ascent * scale) + 2;
        DrawTextSTB(screen, font, scale, titleBaseline, iTitle, iX + (iWidth / 2), iTheme.titleTextColor, true);
    }

    // Render Body text stream lines
    int currentTextY = startY + 4;
    for (int i = 0; i < iLineCount; i++) {
        int baselineY = currentTextY + (iItemHeight - (int)fontHeight) / 2 + (int)(ascent * scale);
        DrawTextSTB(screen, font, scale, baselineY, iLines[i], iX + (iWidth / 2), iTheme.textColor, true);
        currentTextY += iItemHeight;
    }

    // Render Footer string
    int footerY = iY + iHeight - iItemHeight - 10;
    int footerBaseline = footerY + (iItemHeight - (int)fontHeight) / 2 + (int)(ascent * scale);
    DrawTextSTB(screen, font, scale, footerBaseline, iConfirmText, iX + (iWidth / 2), iTheme.footerTextColor, true);
}

bool SdlMessageBox::ShowSTB(const unsigned char* ttfBuffer, float fontHeight) {
    if (!ttfBuffer) return false;
    bool waiting = true;
    bool confirmed = false;
    SDL_Event ev;
    while (waiting && SDL_WaitEvent(&ev)) {
        if (ev.type == SDL_KEYDOWN) {
	    if (iConfirmKey == SDLK_UNKNOWN){
		confirmed = true; waiting = false; 
	    }
	    else {
		if (ev.key.keysym.sym == iConfirmKey) { confirmed = true; waiting = false; }
		if (ev.key.keysym.sym == SDLK_ESCAPE ) { confirmed = false; waiting = false; }
	    }
        }
	RenderSTB(ttfBuffer, fontHeight);
        SDL_Flip(iScreen);
    }
    return confirmed;
}

void SdlMessageBox::DrawTextSTB(SDL_Surface* dest, stbtt_fontinfo& font, float scale, int baseline, const char* text, int x, Uint32 color, bool center) {
    if (!text) return;
    Uint8 r, g, b; SDL_GetRGB(color, dest->format, &r, &g, &b);
    int len = strlen(text);
    
    int currentX = x;
    if (center) {
        int totalWidth = 0;
        for (int i = 0; i < len; ++i) {
            int advance, lsb; stbtt_GetCodepointHMetrics(&font, text[i], &advance, &lsb);
            totalWidth += (int)(advance * scale);
        }
        currentX = x - (totalWidth / 2);
    }

    for (int i = 0; i < len; ++i) {
        int advance, lsb; stbtt_GetCodepointHMetrics(&font, text[i], &advance, &lsb);
        int bitmapW, bitmapH, xOffset, yOffset;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, scale, scale, text[i], &bitmapW, &bitmapH, &xOffset, &yOffset);
        if (bitmap) {
            if (SDL_MUSTLOCK(dest)) SDL_LockSurface(dest);
            int targetYStart = baseline + yOffset;
            for (int srcY = 0; srcY < bitmapH; ++srcY) {
                int targetY = targetYStart + srcY; if (targetY < 0 || targetY >= dest->h) continue;
                Uint8* pixelLinePtr = (Uint8*)dest->pixels + targetY * dest->pitch;
                int baseTargetX = currentX + (int)(lsb * scale) + xOffset;
                for (int srcX = 0; srcX < bitmapW; ++srcX) {
                    int targetX = baseTargetX + srcX; if (targetX < 0 || targetX >= dest->w) continue;
                    unsigned char alpha = bitmap[srcY * bitmapW + srcX]; if (alpha == 0) continue;
                    Uint8* pixelPtr = pixelLinePtr + targetX * dest->format->BytesPerPixel;
                    Uint32 destPixel = (dest->format->BytesPerPixel == 2) ? *(Uint16*)pixelPtr : *(Uint32*)pixelPtr;
                    Uint8 destR, destG, destB; SDL_GetRGB(destPixel, dest->format, &destR, &destG, &destB);
                    Uint32 finalPixel = SDL_MapRGB(dest->format, (Uint8)(((r - destR) * alpha) / 255 + destR), (Uint8)(((g - destG) * alpha) / 255 + destG), (Uint8)(((b - destB) * alpha) / 255 + destB));
                    if (dest->format->BytesPerPixel == 2) *(Uint16*)pixelPtr = (Uint16)finalPixel; else *(Uint32*)pixelPtr = finalPixel;
                }
            }
            if (SDL_MUSTLOCK(dest)) SDL_UnlockSurface(dest);
            stbtt_FreeBitmap(bitmap, NULL);
        }
        currentX += (int)(advance * scale);
    }
}
#endif

// =========================================================================
// SdlViewPager Widget Implementation (With Visual Thumbnails)
// =========================================================================


SdlViewPager::SdlViewPager(SDL_Surface* screen, int titleBarHeight)
    : iTitleBarHeight(titleBarHeight),
      iCurrentPage(0),
      iTotalPages(0),
      iScreen(screen),
      iInternalSurface(NULL) {
    
    // Scale container proportions using standard integer scaling ratios for Symbian architectures
    iWidth = (screen->w * 85) / 100;
    iHeight = (screen->h * 75) / 100; 
    iX = (screen->w - iWidth) / 2; 
    iY = (screen->h - iHeight) / 2;
    
    iTitle[0] = '\0';
    PopulateDefaultTheme(iTheme);
}

SdlViewPager::~SdlViewPager() {
    if (iInternalSurface != NULL) {
        SDL_FreeSurface(iInternalSurface);
        iInternalSurface = NULL;
    }
}

void SdlViewPager::SetTheme(const UiTheme& theme) { 
    iTheme = theme; 
}

void SdlViewPager::SetTitle(const char* title) {
    if (title) { 
        strncpy(iTitle, title, 63); 
        iTitle[63] = '\0'; 
    }
}

void SdlViewPager::AllocateOrResizeInternalSurface(SDL_Surface* src, int targetW, int targetH) {
    if (iInternalSurface != NULL) {
        if (iInternalSurface->w == targetW && iInternalSurface->h == targetH) {
            return; // Matches layout geometry exactly
        }
        SDL_FreeSurface(iInternalSurface);
        iInternalSurface = NULL;
    }
    
    SDL_PixelFormat* fmt = src->format;
    iInternalSurface = SDL_CreateRGBSurface(src->flags, targetW, targetH, fmt->BitsPerPixel, fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);

  
}

void SdlViewPager::SetBitmap(SDL_Surface* externalBitmap) {
    if (!externalBitmap) {
        if (iInternalSurface != NULL) {
            SDL_FreeSurface(iInternalSurface);
            iInternalSurface = NULL;
        }
        return;
    }

    // Isolate bounding dimensions within wireframe window
    int maxImageW = iWidth - 2; 
    int maxImageH = iHeight - iTitleBarHeight - 4;

    // Use standard float mappings matching classic C++98 constraints
    float srcAspect = (float)externalBitmap->w / (float)externalBitmap->h;
    float dstAspect = (float)maxImageW / (float)maxImageH;

    int finalW = maxImageW;
    int finalH = maxImageH;

    if (srcAspect > dstAspect) {
        finalH = (int)((float)maxImageW / srcAspect) -2 ;
    } else {
        finalW = (int)((float)maxImageH * srcAspect) - 2;
    }

    AllocateOrResizeInternalSurface(externalBitmap, finalW, finalH);

    if (iInternalSurface != NULL) {
        SDL_FillRect(iInternalSurface, NULL, iTheme.bgColor);
        
        SDL_Rect destStretchRect;
        destStretchRect.x = 0;
        destStretchRect.y = 0;
        destStretchRect.w = (Uint16)finalW;
        destStretchRect.h = (Uint16)finalH;
        
        // SoftStretch executes calculations internally to compress graphics smoothly
        SDL_SoftStretch(externalBitmap, NULL, iInternalSurface, &destStretchRect);
    }
}

void SdlViewPager::DrawFrameCommon(SDL_Surface* screen, SDL_Rect& viewableImageBounds) { 
    SDL_Rect rect;
    rect.x = (Sint16)iX; rect.y = (Sint16)iY; rect.w = (Uint16)iWidth; rect.h = (Uint16)iHeight;
    SDL_FillRect(screen, &rect, iTheme.bgColor);

    SDL_Rect titleRect;
    titleRect.x = (Sint16)(iX + 1); titleRect.y = (Sint16)(iY + 1); titleRect.w = (Uint16)(iWidth - 2); titleRect.h = (Uint16)iTitleBarHeight;
    SDL_FillRect(screen, &titleRect, iTheme.titleBgColor);

    // Explicit borders
    SDL_Rect top; top.x = (Sint16)iX; top.y = (Sint16)iY; top.w = (Uint16)iWidth; top.h = 1;
    SDL_Rect bottom; bottom.x = (Sint16)iX; bottom.y = (Sint16)(iY + iHeight - 1); bottom.w = (Uint16)iWidth; bottom.h = 1;
    SDL_Rect left; left.x = (Sint16)iX; left.y = (Sint16)iY; left.w = 1; left.h = (Uint16)iHeight;
    SDL_Rect right; right.x = (Sint16)(iX + iWidth - 1); right.y = (Sint16)iY; right.w = 1; right.h = (Uint16)iHeight;
    
    SDL_FillRect(screen, &top, iTheme.borderColor); SDL_FillRect(screen, &bottom, iTheme.borderColor);
    SDL_FillRect(screen, &left, iTheme.borderColor); SDL_FillRect(screen, &right, iTheme.borderColor);

    SDL_Rect titleDivider;
    titleDivider.x = (Sint16)iX; titleDivider.y = (Sint16)(iY + iTitleBarHeight + 1); titleDivider.w = (Uint16)iWidth; titleDivider.h = 1;
    SDL_FillRect(screen, &titleDivider, iTheme.borderColor);

    int maxImageW = iWidth - 2;
    int maxImageH = iHeight - iTitleBarHeight - 2;
    int areaXStart = iX + 1;
    int areaYStart = iY + iTitleBarHeight + 2;

    if (iInternalSurface != NULL) {
        viewableImageBounds.x = (Sint16)(areaXStart + (maxImageW - iInternalSurface->w) / 2);
        viewableImageBounds.y = (Sint16)(areaYStart + (maxImageH - iInternalSurface->h) / 2);
        viewableImageBounds.w = (Uint16)iInternalSurface->w;
        viewableImageBounds.h = (Uint16)iInternalSurface->h;

        SDL_BlitSurface(iInternalSurface, NULL, screen, &viewableImageBounds);
    }
}

void SdlViewPager::RenderBMP(SDL_Surface* screen, SDL_Surface* fontSurface) {
    SDL_Rect imgPos;
    DrawFrameCommon(screen, imgPos);

    if (strlen(iTitle) > 0) {
        DrawTextBMP(screen, fontSurface, iTitle, iX + 8, iY + (iTitleBarHeight - 8) / 2 + 1);
    }

    /*if (iTotalPages > 0) {
        char pageCounterStr[16];
        sprintf(pageCounterStr, "%d/%d", iCurrentPage + 1, iTotalPages);
        int strPixelsLen = strlen(pageCounterStr) * 8;
        DrawTextBMP(screen, fontSurface, pageCounterStr, iX + iWidth - strPixelsLen - 8, iY + (iTitleBarHeight - 8) / 2 + 1);
    }*/
}

void SdlViewPager::DrawTextBMP(SDL_Surface* dest, SDL_Surface* font, const char* text, int x, int y) {
    if (!text || !font) return;
    int len = strlen(text); 
    int curX = x;
    int charW = 8; 
    int charH = 8;
    int charsPerRow = font->w / charW; 
    
    if (charsPerRow <= 0) return;

    for (int i = 0; i < len; ++i) {
        unsigned char c = text[i]; 
        if (c < 32 || c > 126) c = '?';
        int asciiIdx = c - 32;

        SDL_Rect srcRect;
        srcRect.x = (Sint16)((asciiIdx % charsPerRow) * charW);
        srcRect.y = (Sint16)((asciiIdx / charsPerRow) * charH);
        srcRect.w = (Uint16)charW;
        srcRect.h = (Uint16)charH;

        SDL_Rect destRect;
        destRect.x = (Sint16)curX;
        destRect.y = (Sint16)y;
        destRect.w = (Uint16)charW;
        destRect.h = (Uint16)charH;

        SDL_BlitSurface(font, &srcRect, dest, &destRect);
        curX += charW;
    }
}

#ifdef HAVE_STB_TRUETYPE
void SdlViewPager::RenderSTB(SDL_Surface* screen, const unsigned char* ttfBuffer, float fontHeight) {
    if (!ttfBuffer) return;
    stbtt_fontinfo font; 
    if (!stbtt_InitFont(&font, ttfBuffer, 0)) return;
    
    float scale = stbtt_ScaleForPixelHeight(&font, fontHeight);
    int ascent, descent, lineGap; 
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);

    SDL_Rect imgPos;
    DrawFrameCommon(screen, imgPos);

    int baselineY = iY + (iTitleBarHeight - (int)fontHeight) / 2 + (int)(ascent * scale) + 1;

    if (strlen(iTitle) > 0) {
        DrawTextSTB(screen, font, scale, baselineY, iTitle, iX + 8, iTheme.titleTextColor);
    }

    /*if (iTotalPages > 0) {
        char pageCounterStr[16];
        sprintf(pageCounterStr, "%d/%d", iCurrentPage + 1, iTotalPages);
        
        int totalWidth = 0;
        int pLen = strlen(pageCounterStr);
        for (int i = 0; i < pLen; ++i) {
            int advance, lsb; 
            stbtt_GetCodepointHMetrics(&font, pageCounterStr[i], &advance, &lsb);
            totalWidth += (int)(advance * scale);
        }
        DrawTextSTB(screen, font, scale, baselineY, pageCounterStr, iX + iWidth - totalWidth - 8, iTheme.titleTextColor);
    }*/

}

void SdlViewPager::DrawTextSTB(SDL_Surface* dest, stbtt_fontinfo& font, float scale, int baseline, const char* text, int x, Uint32 color) {
    if (!text) return;
    Uint8 r, g, b; 
    SDL_GetRGB(color, dest->format, &r, &g, &b);
    int len = strlen(text); 
    int currentX = x;
    
    for (int i = 0; i < len; ++i) {
        int advance, lsb; 
        stbtt_GetCodepointHMetrics(&font, text[i], &advance, &lsb);
        int bitmapW, bitmapH, xOffset, yOffset;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, scale, scale, text[i], &bitmapW, &bitmapH, &xOffset, &yOffset);
        
        if (bitmap != NULL) {
            if (SDL_MUSTLOCK(dest)) SDL_LockSurface(dest);
            int targetYStart = baseline + yOffset;
            
            for (int srcY = 0; srcY < bitmapH; ++srcY) {
                int targetY = targetYStart + srcY; 
                if (targetY < 0 || targetY >= dest->h) continue;
                
                Uint8* pixelLinePtr = (Uint8*)dest->pixels + targetY * dest->pitch;
                int baseTargetX = currentX + (int)(lsb * scale) + xOffset;
                
                for (int srcX = 0; srcX < bitmapW; ++srcX) {
                    int targetX = baseTargetX + srcX; 
                    if (targetX < 0 || targetX >= dest->w) continue;
                    
                    unsigned char alpha = bitmap[srcY * bitmapW + srcX]; 
                    if (alpha == 0) continue;
                    
                    Uint8* pixelPtr = pixelLinePtr + targetX * dest->format->BytesPerPixel;
                    Uint32 destPixel = (dest->format->BytesPerPixel == 2) ? *(Uint16*)pixelPtr : *(Uint32*)pixelPtr;
                    
                    Uint8 destR, destG, destB; 
                    SDL_GetRGB(destPixel, dest->format, &destR, &destG, &destB);
                    
                    Uint32 finalPixel = SDL_MapRGB(dest->format, 
                        (Uint8)(((r - destR) * alpha) / 255 + destR), 
                        (Uint8)(((g - destG) * alpha) / 255 + destG), 
                        (Uint8)(((b - destB) * alpha) / 255 + destB));
                        
                    if (dest->format->BytesPerPixel == 2) {
                        *(Uint16*)pixelPtr = (Uint16)finalPixel;
                    } else {
                        *(Uint32*)pixelPtr = finalPixel;
                    }
                }
            }
            if (SDL_MUSTLOCK(dest)) SDL_UnlockSurface(dest);
            stbtt_FreeBitmap(bitmap, NULL);
        }
        currentX += (int)(advance * scale);
    }
}
#endif


