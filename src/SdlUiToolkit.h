#ifndef SDL_UI_TOOLKIT_H
#define SDL_UI_TOOLKIT_H

#include <SDL.h>

#ifdef HAVE_SDL_TTF
#include <SDL_ttf.h>
#endif

#ifdef HAVE_STB_TRUETYPE
#include "stb_truetype.h"
#endif

// Master Shared System Limits
#define MAX_UI_ITEMS 128
#define MAX_UI_STR_LEN 64
#define MAX_PAGER_SLOTS 10

// System Layout Configurations
enum UiLayoutProfile {
    LAYOUT_FULLSCREEN,
    LAYOUT_CENTER_MODAL,
    LAYOUT_TITLEBAR_FULLSCREEN
};

// Unified Global Theme Profile Layout
struct UiTheme {
    Uint32 bgColor;
    Uint32 textColor;
    Uint32 selectedBgColor;
    Uint32 selectedTextColor;
    Uint32 borderColor;
    Uint32 titleBgColor;    
    Uint32 titleTextColor;  
};

// Pager Slot Meta Blueprint
struct PagerSlot {
    char description[MAX_UI_STR_LEN];
    SDL_Surface* visualThumbnail; 
};



// =========================================================================
// 1. VERTICAL LISTBOX WIDGET
// =========================================================================
class SdlListbox {
public:
    SdlListbox(int x, int y, int width, int height, int itemHeight);
    SdlListbox(SDL_Surface* screen, UiLayoutProfile profile, int itemHeight);
    ~SdlListbox();

    void SetTheme(const UiTheme& theme);
    void SetTitle(const char* title);
    
    // Feature 1: Toggle text alignment
    void SetCenteredText(bool centerText) { iCenterText = centerText; }

    // Feature 2: Auto-resize container height to match item count
    void SetAutoSizeToItems(bool autoSize) { iAutoSizeToItems = autoSize; RecalculateHeight(); }

    bool AddItem(const char* item);
    bool AddItemW(const Uint16* wideItem);
    void Clear();
    bool HandleInput(SDL_Event& event); // Returns true ONLY on SDLK_RETURN

    void RenderBMP(SDL_Surface* screen, SDL_Surface* fontSurface);
#ifdef HAVE_SDL_TTF
    void RenderTTF(SDL_Surface* screen, TTF_Font* ttfFont);
#endif
#ifdef HAVE_STB_TRUETYPE
    void RenderSTB(SDL_Surface* screen, const unsigned char* ttfBuffer, float fontHeight);
#endif

    const char* GetSelectedText() const { return iItems[iSelectedIndex]; }
    const Uint16* GetSelectedTextW() const { return iWideItems[iSelectedIndex]; }

    int GetSelectedIndex() const { return iSelectedIndex; }
    void SetSelectedIndex(int index);
    int GetItemCount() const { return iItemCount; }

    // Feature 3: Geometry & Size Accessors
    int GetX() const { return iX; }
    int GetY() const { return iY; }
    int GetWidth() const { return iWidth; }
    int GetHeight() const { return iHeight; }
    int GetItemHeight() const { return iItemHeight; }
    int GetMaxVisibleItems() const { return iMaxVisibleItems; }

private:
    void EnsureVisible();
    void RecalculateHeight();
    void DrawBackground(SDL_Surface* screen);
    void DrawSelectionBanner(SDL_Surface* screen, int itemY);
    
    void DrawTextBMP(SDL_Surface* dest, SDL_Surface* font, const char* text, int x, int y, bool center);

#ifdef HAVE_SDL_TTF
    void DrawTextTTF(SDL_Surface* dest, TTF_Font* font, const char* text, int x, int y, Uint32 color, bool center);
    void DrawTextTTFW(SDL_Surface* dest, TTF_Font* font, const Uint16* text, int x, int y, Uint32 color, bool center);
#endif

#ifdef HAVE_STB_TRUETYPE
    void DrawTextSTB(SDL_Surface* dest, stbtt_fontinfo& font, float scale, int baseline, const char* text, int x, Uint32 color, bool center);
    void DrawTextSTBW(SDL_Surface* dest, stbtt_fontinfo& font, float scale, int baseline, const Uint16* text, int x, Uint32 color, bool center);
#endif

private:
    int iX; int iY; int iWidth; int iHeight;
    int iInitialMaxHeight;
    int iItemHeight; int iMaxVisibleItems;
    int iSelectedIndex; int iScrollOffset; 
    int iTextYOffset; 

    bool iCenterText;
    bool iAutoSizeToItems;

    char iTitle[MAX_UI_STR_LEN]; 
    char iItems[MAX_UI_ITEMS][MAX_UI_STR_LEN];
    Uint16 iWideItems[MAX_UI_ITEMS][MAX_UI_STR_LEN];
    bool iIsItemWide[MAX_UI_ITEMS];
    int iItemCount;
    UiTheme iTheme;
};


#if 0 // cmnt
// =========================================================================
// 1. VERTICAL LISTBOX WIDGET
// =========================================================================
class SdlListbox {
public:
    SdlListbox(int x, int y, int width, int height, int itemHeight);
    SdlListbox(SDL_Surface* screen, UiLayoutProfile profile, int itemHeight);
    ~SdlListbox();

    void SetTheme(const UiTheme& theme);
    void SetTitle(const char* title);
    
    bool AddItem(const char* item);
    bool AddItemW(const Uint16* wideItem);
    void Clear();
    bool HandleInput(SDL_Event& event); // Returns true ONLY on SDLK_RETURN

    void RenderBMP(SDL_Surface* screen, SDL_Surface* fontSurface);
#ifdef HAVE_SDL_TTF
    void RenderTTF(SDL_Surface* screen, TTF_Font* ttfFont);
#endif
#ifdef HAVE_STB_TRUETYPE
    void RenderSTB(SDL_Surface* screen, const unsigned char* ttfBuffer, float fontHeight);
#endif

    const char* GetSelectedText() const { return iItems[iSelectedIndex]; }
    const Uint16* GetSelectedTextW() const { return iWideItems[iSelectedIndex]; }

    int GetSelectedIndex() const { return iSelectedIndex; }
    void SetSelectedIndex(int index);
    int GetItemCount() const { return iItemCount; }

private:
    void EnsureVisible();
    void DrawBackground(SDL_Surface* screen);
    void DrawSelectionBanner(SDL_Surface* screen, int itemY);
    void DrawTextBMP(SDL_Surface* dest, SDL_Surface* font, const char* text, int x, int y);

#ifdef HAVE_SDL_TTF
    void DrawTextTTF(SDL_Surface* dest, TTF_Font* font, const char* text, int x, int y, Uint32 color);
    void DrawTextTTFW(SDL_Surface* dest, TTF_Font* font, const Uint16* text, int x, int y, Uint32 color);
#endif

#ifdef HAVE_STB_TRUETYPE
    void DrawTextSTB(SDL_Surface* dest, stbtt_fontinfo& font, float scale, int baseline, const char* text, int x, Uint32 color);
    void DrawTextSTBW(SDL_Surface* dest, stbtt_fontinfo& font, float scale, int baseline, const Uint16* text, int x, Uint32 color);
#endif

private:
    int iX; int iY; int iWidth; int iHeight;
    int iItemHeight; int iMaxVisibleItems;
    int iSelectedIndex; int iScrollOffset; 
    int iTextYOffset; 

    char iTitle[MAX_UI_STR_LEN]; 
    char iItems[MAX_UI_ITEMS][MAX_UI_STR_LEN];
    Uint16 iWideItems[MAX_UI_ITEMS][MAX_UI_STR_LEN];
    bool iIsItemWide[MAX_UI_ITEMS];
    int iItemCount;
    UiTheme iTheme;
};

#endif

// =========================================================================
// 2. MODAL MESSAGEBOX COMPONENT
// =========================================================================


#define MAX_MSG_BOX_STR 64
#define MAX_MSG_LINES 6

struct MsgBoxTheme {
    Uint32 bgColor;
    Uint32 textColor;
    Uint32 borderColor;
    Uint32 titleBgColor;
    Uint32 titleTextColor;
    Uint32 footerTextColor; // Subdued indicator text color
};

class SdlMessageBox {
public:
    SdlMessageBox(SDL_Surface* screen, int itemHeight = 16);
    ~SdlMessageBox();

    void SetTheme(const MsgBoxTheme& theme);
    void SetTitle(const char* title);
    
    bool AddLine(const char* messageLine);
    void SetConfirmText(const char* confirmText);
    void SetConfirmKey(const SDLKey k);
    void Clear();

    void RenderBMP(SDL_Surface* fontSurface);
#ifdef HAVE_SDL_TTF
    void RenderTTF(TTF_Font* ttfFont);
#endif
#ifdef HAVE_STB_TRUETYPE
    void RenderSTB(const unsigned char* ttfBuffer, float fontHeight);
#endif


    bool ShowBMP(SDL_Surface* fontSurface);
#ifdef HAVE_STB_TRUETYPE
    bool ShowSTB(const unsigned char* ttfBuffer, float fontHeight);
#endif

private:
    void CalculateGeometry(SDL_Surface* screen);
    void DrawFrameCommon(SDL_Surface* screen, int& innerYStart);
    void DrawTextBMP(SDL_Surface* dest, SDL_Surface* font, const char* text, int x, int y, bool center);
#ifdef HAVE_STB_TRUETYPE
    void DrawTextSTB(SDL_Surface* dest, stbtt_fontinfo& font, float scale, int baseline, const char* text, int x, Uint32 color, bool center);
#endif

private:
    int iX; int iY; int iWidth; int iHeight;
    int iItemHeight;
    
    char iTitle[MAX_MSG_BOX_STR];
    char iLines[MAX_MSG_LINES][MAX_MSG_BOX_STR];
    char iConfirmText[MAX_MSG_BOX_STR];
    int iLineCount;
    SDL_Surface* iScreen;
    SDLKey iConfirmKey;
    MsgBoxTheme iTheme;
};



// =========================================================================
// 3. HORIZONTAL VIEWPAGER WIDGET (WITH BITMAP STATE CAROUSEL PREVIEWS)
// =========================================================================


class SdlViewPager {
public:
    // C++98 Constructor matching Symbian baseline conventions
    SdlViewPager(SDL_Surface* screen, int titleBarHeight);
    ~SdlViewPager();

    void SetTheme(const UiTheme& theme);
    void SetTitle(const char* title);
    
    // Core single-page bitmap dynamic binding rule
    void SetBitmap(SDL_Surface* externalBitmap);

    void SetTotalPages(int total) { iTotalPages = total; }
    void SetCurrentPage(int page) { iCurrentPage = page; }

    void RenderBMP(SDL_Surface* screen, SDL_Surface* fontSurface);
#ifdef HAVE_STB_TRUETYPE
    void RenderSTB(SDL_Surface* screen, const unsigned char* ttfBuffer, float fontHeight);
#endif

private:
    void AllocateOrResizeInternalSurface(SDL_Surface* src, int targetW, int targetH);
    void DrawFrameCommon(SDL_Surface* screen, SDL_Rect& viewableImageBounds);
    void DrawTextBMP(SDL_Surface* dest, SDL_Surface* font, const char* text, int x, int y);
#ifdef HAVE_STB_TRUETYPE
    void DrawTextSTB(SDL_Surface* dest, stbtt_fontinfo& font, float scale, int baseline, const char* text, int x, Uint32 color);
#endif

private:
    int iX; 
    int iY; 
    int iWidth; 
    int iHeight;
    int iTitleBarHeight;
    int iCurrentPage; 
    int iTotalPages;
    char iTitle[64];
    UiTheme iTheme;
    // Internal surface handled seamlessly inside components
    SDL_Surface* iInternalSurface; 
    SDL_Surface* iScreen;
};


#if 0
class SdlViewPager {
public:
    SdlViewPager(SDL_Surface* screen, int itemHeight);
    ~SdlViewPager();

    void SetTheme(const UiTheme& theme);
    void SetTitle(const char* title);
    bool AddSlot(const char* slotDesc, SDL_Surface* stateImageThumbnail = NULL);
    void Clear();
    
    bool HandleInput(SDL_Event& event); // Returns true ONLY on SDLK_RETURN
    void RenderBMP(SDL_Surface* screen, SDL_Surface* fontSurface);
#ifdef HAVE_STB_TRUETYPE
    void RenderSTB(SDL_Surface* screen, const unsigned char* ttfBuffer, float fontHeight);
#endif

    int GetCurrentIndex() const { return iCurrentIndex; }
    void SetCurrentIndex(int idx) { if (idx >= 0 && idx < iSlotCount) iCurrentIndex = idx; }

private:
    void DrawTextBMP(SDL_Surface* dest, SDL_Surface* font, const char* text, int x, int y);
    void DrawComponentsCommon(SDL_Surface* screen, int& imageFrameY, int& imageFrameW, int& imageFrameH);
#ifdef HAVE_STB_TRUETYPE
    void DrawTextSTB(SDL_Surface* dest, stbtt_fontinfo& font, float scale, int baseline, const char* text, int x, Uint32 color);
#endif

private:
    int iX; int iY; int iWidth; int iHeight; int iItemHeight;
    char iTitle[MAX_UI_STR_LEN];
    PagerSlot iSlots[MAX_PAGER_SLOTS];
    int iSlotCount; int iCurrentIndex;
    UiTheme iTheme;
    SDL_Surface* iThumbSurface;
};
#endif

#endif

