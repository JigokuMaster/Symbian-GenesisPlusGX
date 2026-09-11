/*
 * File: Emu.cpp
 * Initial boilerplate/template generated via Gemini.
 * Adapted, refactored, and maintained by JigokuMaster.
 */


#include "SDL.h"
#include "SDL_thread.h"

#ifdef HAVE_SDL_TTF
#include <SDL_ttf.h>
#endif

#ifndef STB_TRUETYPE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#endif

#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "SdlUiToolkit.h"
#include "EmuCoreInterface.h"
#include "EmuUtils.h"
#include "EmuConfig.h"

#ifdef __SYMBIAN32__
#include <e32std.h>
#include "DirectAudioStream.h"

#define PATH_MAX FILENAME_MAX
extern "C" void EPOC_SetAudioVolume(int);
#define EMU_FONT "pixelfont-7.ttf"
#define ROMS_PATH_PREFIX "\\Data\\"EMUCORE_NAME"\\"
#else
#define EMU_FONT "pixelfont-7.ttf"
#endif

#define VIDEO_WIDTH  320
#define VIDEO_HEIGHT 240
#define DEFAULT_SOUND_FREQUENCY 22050

#ifdef __SYMBIAN32__
#include <f32file.h> // RFs
#include <bautils.h> // BaflUtils
#include <coemain.h> // CCoeEnv

TInt PopulateRomList(SdlListbox* aListbox, const TDesC& aPath)
{
    TInt err(KErrNone);
    CDir* fileList;

    RFs& rfs = CCoeEnv::Static()->FsSession();
    if (!BaflUtils::FolderExists(rfs, aPath)) return err;
    err = rfs.GetDir(aPath, KEntryAttNormal, ESortByName, fileList);

    if (err == KErrNone && fileList != NULL)
    {
	TBuf<KMaxFileName+1> itemText;
	for( TInt i = 0; i < fileList->Count(); ++i)
	{
	    TEntry e = (*fileList)[i];
	    if (e.IsDir()) continue;
	    
	    TParse p;
	    p.Set(e.iName, NULL, NULL);
	    TPtrC ext = p.Ext();
	    if (
		!ext.CompareF(_L(".sms")) ||
		!ext.CompareF(_L(".gg")) ||
		!ext.CompareF(_L(".sg")) ||
		!ext.CompareF(_L(".bin")) 
	    )
	    {

		TPtrC d = aPath.Left(1);
		itemText.Format(_L("%S:%S"), &d,  &e.iName);
		const Uint16* itemTextPtr = reinterpret_cast<const Uint16*>(itemText.PtrZ());
		aListbox->AddItemW(itemTextPtr);
	    }
	}
    }

    if (err == KErrNone) delete fileList;
    return err;
}


#else
#include <dirent.h> // Standard Linux POSIX directory parsing utilities

/**
 * Scans a local directory path on your Linux laptop for rapid debugging.
 * Maps standard 8-bit characters safely into the exact same listbox memory layout.
 */
void PopulateRomList(SdlListbox* listbox, const char* directoryPath) {
    
    DIR* dir = opendir(directoryPath);
    if (!dir) return;
    struct dirent* entry;
    int count = 0;
    
    while ((entry = readdir(dir)) != NULL && count < MAX_UI_ITEMS) {
        // Filter out navigation directory markers '.' and '..'
        if (entry->d_name[0] == '.') continue;
        
        // Deep copy standard Linux char string directly into the internal layout
        listbox->AddItem(entry->d_name);
        count++;
    }
    
    closedir(dir);
}

#endif


UiTheme darkTheme = {
    0x0000, // bgColor    
    0xFFFF, // textColor    
    0x0000, // selectedBgColor
    0xF800, // selectedTextColor
    0xFFFF, // borderColor  
    0x0000, //titleBgColor;    
    0xFFFF // titleTextColor; 
};

MsgBoxTheme msgBoxTheme = {
    0x0000, // bgColor
    0xFFFF, // textColor
    0x8410, // borderColor  
    0x0000, // titleBgColor
    0xFFFF, // titleTextColor
    0xFFFF //footerTextColor
};


namespace OptionMenuAction {
enum  {
    LOAD_ROM = 0,
    RESET_EMU,
    SAVE_STATE,	    
    LOAD_STATE,  
    SETTINGS_MENU,
    EXIT_EMU
};
}

namespace MainMenuAction {
enum  {
    LOAD_ROM = 0, 
    SETTINGS_MENU, 
    ABOUT_EMU,        
    EXIT_EMU
};
}

namespace SettingsMenuAction {
enum  {
    INPUT_SETTINGS = 0,
    VIDEO_SETTINGS,
    AUDIO_SETTINGS,
};
}

namespace VideoSettingsAction {
enum  {
    SET_FULLSCREEN = 0,
    SET_FRAMESKIP,
    SET_FILTER,
    SET_DELAY
};
}


namespace AudioSettingsAction {
enum  {
    TOGGLE_AUDIO = 0, 
    SET_VOLUME, 
    SET_STREAM,
    SET_FREQ
};
}



#ifdef DEBUG
#define DBG_MSG(args) printf("DEBUG: %s:%d (%s) - ", __FILE__, __LINE__, __FUNCTION__); printf args; printf("\n")
#endif

#define PRINT_MSG(args) printf args; printf("\n")
#define PRINT_ERRMSG(args) printf("[ERROR] "); printf args; printf("\n")

#define DIRECT_SCREEN_BLIT 1

class Emu {
private:
    // Core Engine Parameters
    EmuConfig iConfig;
    bool iRunning;
    bool iUseSound;
    bool iFullscreen;

    // Audio Subsystem Components
    struct SoundState {
        char* buffer;
	int buffersize;
	int freq;
	int streammode;
        SDL_sem* sem;
    } iSoundState;

    struct SdlVideoState {
        SDL_Surface* surf_screen;
        SDL_Surface* surf_bitmap;
        SDL_Rect srect;
        SDL_Rect drect;
        Uint32 frame_count;
    } iSdlVideo;

    Uint32 iNextFrameTicks;

    // UI & Typography Assets
    SDL_Surface* iBMPFont;
#ifdef HAVE_SDL_TTF
    TTF_Font* iTTFFont;
#endif
#ifdef HAVE_STB_TRUETYPE
    unsigned char* iSTBFontBuffer;
#endif
    int iFontSize;

    char iRomPathBuf[PATH_MAX+1];
    char iRomsPath[PATH_MAX+1];
    char iStatePath[PATH_MAX+1];
    char iStateFilePath[PATH_MAX+1];
    char iRomName[FILENAME_MAX+1];
    StateManager* iStateManager;
#ifdef __SYMBIAN32__
    CSymbianAudioStream* iAudioStreamer;
#endif
public:
    Emu();
    ~Emu();

    // Global audio callback router
    static void AudioCallbackWrapper(void* userdata, Uint8* stream, int len);

private:
    void HandleAudioCallback(Uint8* stream, int len);
#ifdef __SYMBIAN32__
    int InitAudio(bool reinit=false);
    void UpdateAudioStream();
    void UpdateAudioVolume();
#endif
    int InitSDLAudio(bool reinit=false);
    void UpdateSDLAudioStream();
    void InitSysBitmap();
    void ConfigureVideoBlitRect();
    int InitVideo();
    void ClearEmuScreen();
    void UpdateVideoFrame(bool skipFrame);
    void UpdateVideoFrameFull(bool skipFrame);
    const char* GetROMFilePath(const char* romFileName);
    unsigned char* LoadRawFont(const char* filename);
    bool InitMenuFont();
    void FreeMenuFont();

#ifdef HAVE_STB_TRUETYPE
    void DrawOverlayText(const char* text, Uint32 color);
#endif

    inline SDL_Surface* Screen()
    {
	return iSdlVideo.surf_screen;
    }

    void DrawMenu(SdlListbox* menu);
    void DrawViewPager(SdlViewPager* p);
    void DrawMessageBox(SdlMessageBox* box);
    void ShowMessageBox(const char* title, const char* message);

    bool ReadConfig();
    void ApplyConfig();
    void SaveConfig();
    void InitStateManager();
    char* GetStateFilePath();
    SDL_Surface* PrepareStateBitmap();
    void SaveStateBMP(char* fp, SDL_Surface* bmp = NULL);

    void AutoSaveState();
    void AutoLoadState();
    void SaveState(SDL_Surface* bmp = NULL);
    void LoadState();
    Uint32 ShowSavedStates();

    void ShowInputSettings();
    void SetupDefaultInputKeys();
    void ShowSettingsMenu();
    void DrawVideoSettings(SdlListbox* listbox, int itemIndex);
    bool UpdateVideoSettings(SdlListbox* listbox, SDL_Event& event, bool* configChanged);
    void ShowVideoSettings();
    void DrawAudioSettings(SdlListbox* listbox, int itemIndex);
    bool UpdateAudioSettings(SdlListbox* listbox, SDL_Event& event, bool* configChanged);
    void ShowAudioSettings();
    void ShowOptionsMenu();
    void ShowAboutMenu();
    bool LoadROM(char* romFilePath = NULL, bool reset = true);
    const char* ShowROMList();
    bool ShowMainMenu();

    // =========================================================================
    // INPUT DEVICE HANDLING
    // =========================================================================
public:     
    void UpdateEmuCoreInput();

private:
    void UpdateControlInput(SDLKey k);

    // =========================================================================
    // SYSTEM LIFECYCLE DESTRUCTION DESTRUCTORS
    // =========================================================================
    void CloseAudio();
    void CloseVideo();
    void Shutdown();
    bool HandleResizeEvent(SDL_Event* event);
    bool HandleFrameSkip();
    void HandleFramePacing();
    void DoFrameSync();

public:
    int Run(int argc, char** argv);
};

Emu::Emu()
      : iRunning(false), iUseSound(true), iFullscreen(false),
      iBMPFont(NULL), iFontSize(34), iStateManager(NULL) {
    memset(&iSoundState, 0, sizeof(iSoundState));
    memset(&iSdlVideo, 0, sizeof(iSdlVideo));
    memset(iRomPathBuf, 0, sizeof(iRomPathBuf));
    memset(iRomsPath, 0, sizeof(iRomsPath));
#ifdef __SYMBIAN32__
    iAudioStreamer = NULL;
#endif

#ifdef HAVE_SDL_TTF
    iTTFFont = NULL;
#endif
#ifdef HAVE_STB_TRUETYPE
    iSTBFontBuffer = NULL;
#endif
}

Emu::~Emu()
{
    Shutdown();
}


// =========================================================================
// INPUT & OUTPUT UPDATE HANDLERS
// =========================================================================


void Emu::AudioCallbackWrapper(void* userdata, Uint8* stream, int len) {
    if (userdata) {
        static_cast<Emu*>(userdata)->HandleAudioCallback(stream, len);
    }
}

void Emu::HandleAudioCallback(Uint8* stream, int len) {

    if (iSoundState.sem)
    {
	memcpy(stream, iSoundState.buffer, len);
	SDL_SemPost(iSoundState.sem);
    }

    //else if (iSoundState.active) {
    else {
	memset(stream, 0, len);
    }
}


#ifdef __SYMBIAN32__
inline void Emu::UpdateAudioStream()
{
    if (!iUseSound) return;

    if (iConfig.audioStream == SDL_AUDIOSTREAM)
    {
	UpdateSDLAudioStream();
	return;
    }
 
    int samples = emucore_fetch_audio((Uint8*)iSoundState.buffer)*2;

    if (iConfig.audioStream == AUDIOSTREAM_CUSTOMQUEUE)
    {
	DirectAudio_Queue((short*)iSoundState.buffer, samples);
	DirectAudio_Service(); 
    }
    
    else if (iConfig.audioStream == AUDIOSTREAM_SYSQUEUE)
    {
	iUseSound = iAudioStreamer->DirectWriteData((TUint8*)iSoundState.buffer, samples*2);
    }
}
#endif

inline void Emu::UpdateSDLAudioStream() {
    if (SDL_GetAudioStatus() != SDL_AUDIO_PLAYING) return;
  
    //SDL_LockAudio();
    emucore_fetch_audio((Uint8*)iSoundState.buffer);
    //SDL_UnlockAudio();
}




#if 0
/*
 * Safe 1:1 RGB565 Software Blitter (Drop-in replacement for SDL_BlitSurface)
 * 
 * - Never modifies the input 'dstrect' or 'srcrect' structs in-place.
 * - Safely clips both source and destination rectangles to prevent crashes.
 * - Uses hardware-optimized memcpy for scanline transfers.
 */
inline int DirectBlitRGB565(SDL_Surface* src, const SDL_Rect* srcrect,
                     SDL_Surface* dst, const SDL_Rect* dstrect)
{
    int sX, sY, sW, sH;
    int dX, dY;
    int y;
    int srcStride, dstStride;
    size_t copyBytes;
    const unsigned short* srcRow;
    unsigned short* dstRow;

    /* 1. Null pointer safety checks */
    if (!src || !dst || !src->pixels || !dst->pixels) {
        return -1;
    }

    /* 2. Resolve source rectangle (full surface if NULL) */
    sX = srcrect ? srcrect->x : 0;
    sY = srcrect ? srcrect->y : 0;
    sW = srcrect ? srcrect->w : src->w;
    sH = srcrect ? srcrect->h : src->h;

    /* 3. Resolve destination position (0,0 if NULL) */
    dX = dstrect ? dstrect->x : 0;
    dY = dstrect ? dstrect->y : 0;

    /* 4. Clip against source surface boundaries */
    if (sX < 0) { sW += sX; dX -= sX; sX = 0; }
    if (sY < 0) { sH += sY; dY -= sY; sY = 0; }
    if (sX + sW > src->w) { sW = src->w - sX; }
    if (sY + sH > src->h) { sH = src->h - sY; }

    /* 5. Clip against destination surface boundaries */
    if (dX < 0) { sW += dX; sX -= dX; dX = 0; }
    if (dY < 0) { sH += dY; sY -= dY; dY = 0; }
    if (dX + sW > dst->w) { sW = dst->w - dX; }
    if (dY + sH > dst->h) { sH = dst->h - dY; }

    /* 6. Return early if rect is completely off-screen */
    if (sW <= 0 || sH <= 0) {
        return 0;
    }

    /* 7. Safely lock surfaces if required */
    if (SDL_MUSTLOCK(src)) {
        if (SDL_LockSurface(src) < 0) return -1;
    }
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            if (SDL_MUSTLOCK(src)) {
                SDL_UnlockSurface(src);
            }
            return -1;
        }
    }

    /* 8. Calculate 16-bit word strides and line pointers */
    srcStride = src->pitch / 2;
    dstStride = dst->pitch / 2;

    srcRow = ((const unsigned short*)src->pixels) + (sY * srcStride) + sX;
    dstRow = ((unsigned short*)dst->pixels) + (dY * dstStride) + dX;
    copyBytes = (size_t)sW * sizeof(unsigned short);

    /* 9. Direct line-by-line copy loop */
    for (y = 0; y < sH; ++y) {
        memcpy(dstRow, srcRow, copyBytes);
        srcRow += srcStride;
        dstRow += dstStride;
    }

    /* 10. Unlock surfaces */
    if (SDL_MUSTLOCK(dst)) {
        SDL_UnlockSurface(dst);
    }
    if (SDL_MUSTLOCK(src)) {
        SDL_UnlockSurface(src);
    }

    return 0;
}
#endif

/*
 * SDL_SoftStretch replacement.
 */

inline void SoftStretchWithScanlines(SDL_Surface* src, const SDL_Rect* srcrect,
                                     SDL_Surface* dst, const SDL_Rect* dstrect,
                                     int enableScanlines) {
    int sX, sY, sW, sH;
    int dX, dY, dW, dH;

    if (!src || !dst || !src->pixels || !dst->pixels) return;

    /* Resolve source rect (full surface if NULL) */
    sX = srcrect ? srcrect->x : 0;
    sY = srcrect ? srcrect->y : 0;
    sW = srcrect ? srcrect->w : src->w;
    sH = srcrect ? srcrect->h : src->h;

    /* Resolve destination rect (full surface if NULL) */
    dX = dstrect ? dstrect->x : 0;
    dY = dstrect ? dstrect->y : 0;
    dW = dstrect ? dstrect->w : dst->w;
    dH = dstrect ? dstrect->h : dst->h;

    /* 1. Prevent Black Screen / Crash: Clip destination to physical surface boundaries */
    if (dX < 0) { dW += dX; sX -= dX; dX = 0; }
    if (dY < 0) { dH += dY; sY -= dY; dY = 0; }
    if (dX + dW > dst->w) dW = dst->w - dX;
    if (dY + dH > dst->h) dH = dst->h - dY;

    if (sW <= 0 || sH <= 0 || dW <= 0 || dH <= 0) return;

    /* 2. Lock surfaces safely */
    if (SDL_MUSTLOCK(src)) {
        if (SDL_LockSurface(src) < 0) return;
    }
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            if (SDL_MUSTLOCK(src)) SDL_UnlockSurface(src);
            return;
        }
    }

    FastStretchRectRGB565(
        (const unsigned short*)src->pixels, src->pitch / 2,
        sX, sY, sW, sH,
        (unsigned short*)dst->pixels, dst->pitch / 2,
        dX, dY, dW, dH,
        enableScanlines
    );

    if (SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
    if (SDL_MUSTLOCK(src)) SDL_UnlockSurface(src);
}


#if 0
inline void SoftStretchWithScanlines(SDL_Surface* src, const SDL_Rect* srcrect,
                              SDL_Surface* dst, const SDL_Rect* dstrect,
                              int enableScanlines) {
    int sX, sY, sW, sH;
    int dX, dY, dW, dH;

    if (!src || !dst) return;

    /* Resolve source rect (full surface if NULL) */
    sX = srcrect ? srcrect->x : 0;
    sY = srcrect ? srcrect->y : 0;
    sW = srcrect ? srcrect->w : src->w;
    sH = srcrect ? srcrect->h : src->h;

    /* Resolve destination rect (full surface if NULL) */
    dX = dstrect ? dstrect->x : 0;
    dY = dstrect ? dstrect->y : 0;
    dW = dstrect ? dstrect->w : dst->w;
    dH = dstrect ? dstrect->h : dst->h;

    if (SDL_MUSTLOCK(src)) SDL_LockSurface(src);
    if (SDL_MUSTLOCK(dst)) SDL_LockSurface(dst);

    FastStretchRectRGB565(
        (const unsigned short*)src->pixels, src->pitch / 2,
        sX, sY, sW, sH,
        (unsigned short*)dst->pixels, dst->pitch / 2,
        dX, dY, dW, dH,
        enableScanlines
    );

    if (SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
    if (SDL_MUSTLOCK(src)) SDL_UnlockSurface(src);
}
#endif


/*
 * Bilinear SDL_SoftStretch replacement with safety clipping and scanlines support.
 */
inline void SoftBilinearWithScanlines(SDL_Surface* src, const SDL_Rect* srcrect,
                                      SDL_Surface* dst, const SDL_Rect* dstrect,
                                      int enableScanlines) {
    int sX, sY, sW, sH;
    int dX, dY, dW, dH;

    if (!src || !dst || !src->pixels || !dst->pixels) return;

    /* Resolve source rect (full surface if NULL) */
    sX = srcrect ? srcrect->x : 0;
    sY = srcrect ? srcrect->y : 0;
    sW = srcrect ? srcrect->w : src->w;
    sH = srcrect ? srcrect->h : src->h;

    /* Resolve destination rect (full surface if NULL) */
    dX = dstrect ? dstrect->x : 0;
    dY = dstrect ? dstrect->y : 0;
    dW = dstrect ? dstrect->w : dst->w;
    dH = dstrect ? dstrect->h : dst->h;

    /* Clip destination to surface boundaries */
    if (dX < 0) { dW += dX; sX -= dX; dX = 0; }
    if (dY < 0) { dH += dY; sY -= dY; dY = 0; }
    if (dX + dW > dst->w) dW = dst->w - dX;
    if (dY + dH > dst->h) dH = dst->h - dY;

    if (sW <= 0 || sH <= 0 || dW <= 0 || dH <= 0) return;

    if (SDL_MUSTLOCK(src)) {
        if (SDL_LockSurface(src) < 0) return;
    }
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            if (SDL_MUSTLOCK(src)) SDL_UnlockSurface(src);
            return;
        }
    }

    FastBilinearRectRGB565(
        (const unsigned short*)src->pixels, src->pitch / 2,
        sX, sY, sW, sH,
        (unsigned short*)dst->pixels, dst->pitch / 2,
        dX, dY, dW, dH,
        enableScanlines
    );

    if (SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
    if (SDL_MUSTLOCK(src)) SDL_UnlockSurface(src);
}


inline void Emu::UpdateVideoFrameFull(bool skipFrame) 
{

    if (!skipFrame) 
    {

	if (iConfig.filter == SCANLINES_FILTER)
	{
	    SoftStretchWithScanlines(
		    iSdlVideo.surf_bitmap,
		    &iSdlVideo.srect,
		    Screen(),
		    &iSdlVideo.drect, 1);
	}


	else if (iConfig.filter == BILINEAR_FILTER)
	{
	    SoftBilinearWithScanlines(
		    iSdlVideo.surf_bitmap,
		    &iSdlVideo.srect,
		    Screen(),
		    &iSdlVideo.drect, 0);
	}

	else {
	    
	    //SDL_SoftStretch(iSdlVideo.surf_bitmap, &iSdlVideo.srect, Screen(), &iSdlVideo.drect);	    
	    SoftStretchWithScanlines(
		    iSdlVideo.surf_bitmap,
		    &iSdlVideo.srect,
		    iSdlVideo.surf_screen,
		    &iSdlVideo.drect, 0);
	}

	SDL_UpdateRect(iSdlVideo.surf_screen, iSdlVideo.drect.x, iSdlVideo.drect.y, iSdlVideo.drect.w , iSdlVideo.drect.h);
    }
    ++iSdlVideo.frame_count;
}


#ifdef DIRECT_SCREEN_BLIT

inline void Emu::UpdateVideoFrame(bool skipFrame) 
{
 
    if (!skipFrame) 
    {

	if (iConfig.filter == SCANLINES_FILTER)
	{
	    SoftStretchWithScanlines(Screen(),
		    &iSdlVideo.srect,
		    Screen(),
		    &iSdlVideo.drect, 1);
	}


	else if (iConfig.filter == BILINEAR_FILTER)
	{
	    SoftBilinearWithScanlines(Screen(),
		    &iSdlVideo.srect,
		    Screen(),
		    &iSdlVideo.drect, 0);
	}
       
	SDL_UpdateRect(Screen(), iSdlVideo.drect.x, iSdlVideo.drect.y, iSdlVideo.drect.w , iSdlVideo.drect.h);

    }

    ++iSdlVideo.frame_count;
}

#else
inline void Emu::UpdateVideoFrame(bool skipFrame) 
{
 
    if (!skipFrame) {

	if (iConfig.filter == SCANLINES_FILTER)
	{
	    SoftStretchWithScanlines(iSdlVideo.surf_bitmap,
		    &iSdlVideo.srect,
		    Screen(),
		    &iSdlVideo.drect, 1);
	}


	else if (iConfig.filter == BILINEAR_FILTER)
	{
	    SoftBilinearWithScanlines(iSdlVideo.surf_bitmap,
		    &iSdlVideo.srect,
		    Screen(),
		    &iSdlVideo.drect, 0);
	}

	else{
	    SDL_BlitSurface(iSdlVideo.surf_bitmap, &iSdlVideo.srect, Screen(), &iSdlVideo.drect);

	}
        
	SDL_UpdateRect(Screen(), iSdlVideo.drect.x, iSdlVideo.drect.y, iSdlVideo.drect.w , iSdlVideo.drect.h);

    }

    ++iSdlVideo.frame_count;
}
#endif

void Emu::UpdateEmuCoreInput()
{
    emucore_update_input(iConfig.keys, 0, 0);
}

void Emu::UpdateControlInput(SDLKey k) {
    

    if (k == iConfig.keys[EMU_INPUT_SAVESTATE])
    {
        AutoSaveState();
    }
    else if (k == iConfig.keys[EMU_INPUT_LOADSTATE])
    {
        AutoLoadState();
    }
    else if (k == SDLK_ESCAPE)
    {
        SDL_PauseAudio(1);
        ShowOptionsMenu();
        SDL_PauseAudio(0);
    }
#ifdef __SYMBIAN32__

    else if ( k == SDLK_HOME ) 
    {
	SDL_ShowCursor(!SDL_ShowCursor(-1));
    }
#endif

}


#ifdef __SYMBIAN32__
int Emu::InitAudio(bool reinit)
{

    PRINT_MSG(("AUDIOSTREAM_MODE %d", iConfig.audioStream));

    PRINT_MSG(("AUDIO_FREQ %d", iConfig.audioFreq));

    if ( reinit ) CloseAudio();

    int err;
    if (iConfig.audioStream == SDL_AUDIOSTREAM) 
    {
	return InitSDLAudio();
    }


    int samples = emucore_audio_numsamples(iConfig.audioFreq);
    iSoundState.buffersize = samples * 2 * sizeof(short); 
    iSoundState.buffer = (char*)malloc(iSoundState.buffersize);

    if (!iSoundState.buffer) 
    {
	PRINT_ERRMSG(("failed to allocate audio buffer  (%d bytes requested)", iSoundState.buffersize));
	return 0;
    }

    memset(iSoundState.buffer, 0, iSoundState.buffersize);

    if (iConfig.audioStream == AUDIOSTREAM_CUSTOMQUEUE)
    {
	// DirectAudio_Init returns 0 on KErrNone success
	err = DirectAudio_Init(iConfig.audioFreq, 2); 
	if (err != 0) {
	    PRINT_ERRMSG(("DirectAudio_Init failed with Symbian error code: %d", err));
	    iUseSound = false;
	    return 0;
	}
    }

    else 
    {
	// set default stream mode here.
	iConfig.audioStream = AUDIOSTREAM_SYSQUEUE;

	TRAP(err, iAudioStreamer = CSymbianAudioStream::NewL(iConfig.audioFreq, 0));
	if (err != KErrNone) 
	{
	    PRINT_ERRMSG(("CSymbianAudioStream failed with Symbian error code: %d", err));
	    return 0;
	}
    }

    UpdateAudioVolume();
    return 1;
}

void Emu::UpdateAudioVolume()
{

    int vol = ((iConfig.audioVolume/10.0) * 256);
    if (iConfig.audioStream == SDL_AUDIOSTREAM) 
    {
	EPOC_SetAudioVolume(vol);
    }   
    else if (iConfig.audioStream == AUDIOSTREAM_CUSTOMQUEUE)
    {
	DirectAudio_SetVolume(vol);
    }
    else if ((iConfig.audioStream == AUDIOSTREAM_SYSQUEUE) && iAudioStreamer)
    {
	iAudioStreamer->SetVolume(iConfig.audioVolume);
    }

}

#endif

int Emu::InitSDLAudio(bool reinit)
{

    if ( reinit ) CloseAudio();

    SDL_AudioSpec as_desired;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        PRINT_ERRMSG(("SDL Audio initialization failed: %s", SDL_GetError()));
        return 0;
    }

    int samples = emucore_audio_numsamples(iConfig.audioFreq);

    as_desired.freq     = iConfig.audioFreq;
    as_desired.format   = AUDIO_S16SYS;
    as_desired.channels = 2;
    as_desired.samples  = samples;
    as_desired.callback = AudioCallbackWrapper;
    as_desired.userdata = this;

    if (SDL_OpenAudio(&as_desired, NULL) == -1) {
        PRINT_ERRMSG(("SDL_OpenAudio failed: %s", SDL_GetError()));
        return 0;
    }

    PRINT_MSG(("SDL_AUDIO SAMPLES %d BUFSIZE %d", as_desired.samples, as_desired.size));
    iSoundState.buffersize = samples * 2 * sizeof(short); 
    
    if (as_desired.size < iSoundState.buffersize)
    {
	SDL_CloseAudio();
	as_desired.samples  = samples*2;
	if (SDL_OpenAudio(&as_desired, NULL) == -1) {
	    PRINT_ERRMSG(("SDL_OpenAudio failed: %s", SDL_GetError()));
	    return 0;
	}
	PRINT_MSG(("SDL_AUDIO BUFSIZE now is %d", as_desired.size));
    } 
    
    iSoundState.buffer = (char*)malloc(iSoundState.buffersize);
    if (!iSoundState.buffer) 
    {
	PRINT_ERRMSG(("failed to allocate audio buffer  (%d bytes requested)", iSoundState.buffersize));
	return 0;
    }

    memset(iSoundState.buffer, 0, iSoundState.buffersize);

    iSoundState.sem = SDL_CreateSemaphore(0);
    if ( !iSoundState.sem ) {
	PRINT_ERRMSG(("SDL_CreateSemaphore error: %s", SDL_GetError()));
	return 0;
    }

#ifdef __SYMBIAN32__
    UpdateAudioVolume();
#endif // __SYMBIAN32__

    SDL_PauseAudio(0);
    return 1;
}

#ifdef DIRECT_SCREEN_BLIT

void Emu::InitSysBitmap()
{

    if (!iSdlVideo.surf_bitmap)
    {
        iSdlVideo.surf_bitmap = SDL_CreateRGBSurface(SDL_SWSURFACE, VIDEO_WIDTH, VIDEO_HEIGHT, 16, 0, 0, 0, 0);
    }


    if (!iSdlVideo.surf_bitmap) {
        PRINT_ERRMSG(("SDL_CreateRGBSurface video context buffer allocation failure: %s", SDL_GetError()));
	iRunning = false;
        return;
    }

    SDL_Surface* surface = iFullscreen ? iSdlVideo.surf_bitmap : Screen();


    if (SDL_MUSTLOCK(surface))
    {
	SDL_LockSurface(surface);
    }
   
    emucore_init_video(
	    (Uint8*)surface->pixels, 
	    VIDEO_WIDTH,
	    VIDEO_HEIGHT,
	    16,
	    surface->pitch);

    if (SDL_MUSTLOCK(surface)) 
    {
	SDL_UnlockSurface(surface);
    } 

    ConfigureVideoBlitRect();
}

#else

void Emu::InitSysBitmap()
{
    if (!iSdlVideo.surf_bitmap)
    {
        iSdlVideo.surf_bitmap = SDL_CreateRGBSurface(SDL_SWSURFACE, VIDEO_WIDTH, VIDEO_HEIGHT, 16, 0, 0, 0, 0);
    }

    if (!iSdlVideo.surf_bitmap) {
        PRINT_ERRMSG(("SDL_CreateRGBSurface video context buffer allocation failure: %s", SDL_GetError()));
	iRunning = false;
        return;
    }

    emucore_init_video((Uint8*)iSdlVideo.surf_bitmap->pixels, 
	    VIDEO_WIDTH,
	    VIDEO_HEIGHT,
	    16,
	    iSdlVideo.surf_bitmap->pitch);

    ConfigureVideoBlitRect();
}
#endif


int Emu::InitVideo() 
{

    int scrWidth, scrHeight;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        PRINT_ERRMSG(("SDL Video subsystem initialization failed: %s", SDL_GetError()));
        return 0;
    }
#ifdef __SYMBIAN32__
    SDL_Rect** modes = SDL_ListModes(NULL, SDL_FULLSCREEN);
    if(modes == NULL)
    {
        PRINT_ERRMSG(("No available video modes"));
        return 0;
    }

    scrWidth = modes[0]->w;
    scrHeight = modes[0]->h;

    PRINT_MSG(("SCREEN_SIZE %dx%d\n", scrWidth, scrHeight));
#else 
    scrWidth = VIDEO_WIDTH;
    scrHeight = VIDEO_HEIGHT;
#endif

    iSdlVideo.surf_screen = SDL_SetVideoMode(scrWidth, scrHeight, 16, SDL_SWSURFACE); 
    if (!Screen()) {
        PRINT_ERRMSG(("SDL_SetVideoMode failed: %s", SDL_GetError()));
        return 0;
    }
    
    SDL_ShowCursor(SDL_DISABLE);
    return 1;
}


void Emu::ConfigureVideoBlitRect()
{
    iSdlVideo.drect.x = 0;
    iSdlVideo.drect.y = 0;
    iSdlVideo.drect.w = VIDEO_WIDTH;
    iSdlVideo.drect.h = VIDEO_HEIGHT;

    // get emulator bitmap rect
    emucore_get_videorect(&iSdlVideo.srect);

    if (iFullscreen)
    {
	// reset those because surface bitmap is used now.
	iSdlVideo.srect.x = 0;
	iSdlVideo.srect.y = 0;
    }

    if (Screen())
    {
	// calculate rect to fit into the screen.
        int scrWidth = Screen()->w;
        int scrHeight = Screen()->h;
        CalculateAspectRatioRect(iSdlVideo.srect.w, iSdlVideo.srect.h, scrWidth, scrHeight, &iSdlVideo.drect);
    }


    if (iFullscreen)
    {

	emucore_set_videorect(&iSdlVideo.srect);
    }

    else 
    {
	// blit in the center of screen
        iSdlVideo.drect.x = (Screen()->w - iSdlVideo.srect.w) >> 1;
        iSdlVideo.drect.y = (Screen()->h - iSdlVideo.srect.h) >> 1;
	iSdlVideo.drect.w = iSdlVideo.srect.w;
	iSdlVideo.drect.h = iSdlVideo.srect.h;
#ifdef DIRECT_SCREEN_BLIT
	/// set  blitter rect area 
	emucore_set_videorect(&iSdlVideo.drect);
#endif
    }


    PRINT_MSG(("FULLSCREEN %d", iFullscreen));
    PRINT_MSG(("BLIT_DEST_RECT %d,%d %d, %d", iSdlVideo.drect.x, iSdlVideo.drect.y, iSdlVideo.drect.w, iSdlVideo.drect.h));

    PRINT_MSG(("BLIT_SRC_RECT %d,%d %d, %d", iSdlVideo.srect.x, iSdlVideo.srect.y, iSdlVideo.srect.w, iSdlVideo.srect.h));
}


// =========================================================================
// EXTERNAL DATA
// =========================================================================



bool Emu::ReadConfig()
{
    memset(&iConfig, 0, sizeof(EmuConfig));  
    iConfig.audioEnabled = true;
    iConfig.audioVolume = 5;
    iConfig.audioFreq = DEFAULT_SOUND_FREQUENCY;
    iConfig.fullscreen = false;

#ifdef __SYMBIAN32__
    // set the default streammode
    iConfig.audioStream  = AUDIOSTREAM_CUSTOMQUEUE;
#endif


    FILE *f = fopen("cfg.bin", "rb");
    if (!f) return false;

    /* check file size */
    fseek(f, 0, SEEK_END);
    if (ftell(f) != sizeof(EmuConfig))
    {
        fclose(f);
        return false;
    }
    
    /* check version */
    char version[10];
    fseek(f, 0, SEEK_SET);
    fread(version, 10, 1, f);
    if (memcmp(version, CONFIG_VERSION, 10))
    {
        fclose(f);
        return false;
    }

    /* read file */
    fseek(f, 0, SEEK_SET);
    fread(&iConfig, sizeof(iConfig), 1, f);
    fclose(f);

    ApplyConfig();
    return true;
}

void Emu::ApplyConfig()
{
    bool screenModeChanged = iFullscreen != iConfig.fullscreen;
    iFullscreen = iConfig.fullscreen;

    iUseSound = iConfig.audioEnabled;
    bool audioFreqChanged = iSoundState.freq != iConfig.audioFreq;
    bool reInitAudio = audioFreqChanged; 

    if (iConfig.audioFreq != 22050 && iConfig.audioFreq != 44100) 
    {
	iConfig.audioFreq = 22050;
    }

    iSoundState.freq = iConfig.audioFreq;

#ifdef __SYMBIAN32__
   
   if ( iSoundState.streammode  != iConfig.audioStream )
    {
       iSoundState.streammode = iConfig.audioStream; 
       reInitAudio = true;
    }
#endif

    if ( !iRunning ) return;


    if ( screenModeChanged )
    {
	iFullscreen = iConfig.fullscreen;
	InitSysBitmap();
    }

    // reinitialize emulator core audio
    if ( audioFreqChanged )
    {
	if (!emucore_reinit_audio(iSoundState.freq))
	{
	    //iRunning = false;
	    iUseSound = false;
	    PRINT_ERRMSG(("couldn't reinitialize audio."));
	    return;
	}
    }

    // reinitialize host audio
    if ( reInitAudio )
    {

#ifdef __SYMBIAN32__
	iRunning = InitAudio(true);
#else 
	iRunning = InitSDLAudio(true);
#endif
    }

    ConfigureVideoBlitRect();
#ifdef __SYMBIAN32__
   UpdateAudioVolume();
#endif
}


// =========================================================================
// STATE MANAGEMENT
// =========================================================================


void Emu::SaveConfig()
{

    memcpy(iConfig.version, CONFIG_VERSION, 10);
    FILE *f = fopen("cfg.bin", "wb");
    if (f)
    {
        /* dump the config to the file */
        fwrite(&iConfig, sizeof(EmuConfig), 1, f);
        fclose(f);
    }
}

void Emu::InitStateManager()
{
    memset(iStatePath, 0, sizeof(iStatePath));
    char sep = '/';
#ifdef __SYMBIAN32__
    sep = '\\';
#endif
    
    sprintf(iStatePath, "%s%csav", iRomsPath, sep);
    PRINT_MSG(("SAVED_STATE_PATH %s", iStatePath));

    mkdir(iStatePath, 0755);

    char* romFilePath = (char*) iRomPathBuf;    
    memset(iRomName, 0, sizeof(iRomName));
    ExtractBaseFilename(romFilePath, iRomName, sizeof(iRomName));

    iStateManager = new StateManager(iStatePath, iRomName);
}

char* Emu::GetStateFilePath()
{
    return (char*)iStateFilePath;
}

SDL_Surface* Emu::PrepareStateBitmap()
{


    SDL_Rect srect;
    emucore_get_videorect(&srect);

    SDL_Surface* surface = iFullscreen ? iSdlVideo.surf_bitmap : Screen();

    SDL_PixelFormat* fmt = surface->format;
    SDL_Surface* bm = SDL_CreateRGBSurface(
        surface->flags,
        srect.w,
	srect.h,
        fmt->BitsPerPixel,
        fmt->Rmask,
        fmt->Gmask,
        fmt->Bmask,
        fmt->Amask);
   

    SDL_Rect drect = {0, 0, bm->w, bm->h};
    SDL_BlitSurface(surface, &srect, bm, &drect);
    return bm;
}

void Emu::SaveStateBMP(char* fp, SDL_Surface* bmp)
{
    int status;
    if (!fp) return;
    strncat(fp, ".bmp", PATH_MAX);

    if (!bmp) bmp = PrepareStateBitmap();

    SDL_LockSurface(bmp);
    status = SDL_SaveBMP(bmp, fp);
    if(status != 0)
    {   
        PRINT_ERRMSG(("%s", SDL_GetError()));
    }

    SDL_UnlockSurface(bmp); 
    SDL_FreeSurface(bmp);
}

void Emu::AutoSaveState()
{
    if (!iStateManager) return;

    char* fp = GetStateFilePath();

    if (iStateManager->autoSaveExists())
    {
        if (!iStateManager->getAutoSaveFilename(fp, PATH_MAX)) return; 
    }
    else
    {
        Uint32 slotNum = iStateManager->getSlotCount();

        if (slotNum < 1) return;

        iStateManager->getSlotFilename(slotNum, fp, PATH_MAX);
    }

    if(emucore_save_state(fp))
    {
        SaveStateBMP(fp);
    }

    ClearEmuScreen();
}

void Emu::AutoLoadState()
{
    if (!iStateManager) return;

    char* fp = GetStateFilePath();

    if (iStateManager->autoSaveExists())
    {
        if (!iStateManager->getAutoSaveFilename(fp, PATH_MAX)) return; 
    }
    else
    {
        Uint32 slotNum = iStateManager->getSlotCount();

        if (slotNum < 1) return;

        iStateManager->getSlotFilename(slotNum, fp, PATH_MAX);
    }


    if(!emucore_load_state(fp))
    {
	// handle failure
    }

    ClearEmuScreen();
}

void Emu::SaveState(SDL_Surface* bmp)
{
    Uint32 slotNum = ShowSavedStates();
    if (!slotNum) return;

    char* fp = GetStateFilePath();

    if (!iStateManager->getSlotFilename(slotNum, fp, PATH_MAX)) return; 

    DrawOverlayText("saving state ...", 0xFFF00);

    if(emucore_save_state(fp))
    {
        SaveStateBMP(fp, bmp);
        iStateManager->notifySlotSaved(slotNum);
    }
    else
    {
        PRINT_ERRMSG(("failed to save state %d", slotNum));
    }   
}

void Emu::LoadState()
{
    Uint32 slotNum = ShowSavedStates();
    if (!slotNum) return;

    char* fp = GetStateFilePath();
    if (!fp) return;

    if (!iStateManager->getSlotFilename(slotNum, fp, PATH_MAX)) return; 

    DrawOverlayText("loading state ...", 0xFFF00);

    if(!emucore_load_state(fp))
    {
	// handle failure
    }
}


// =========================================================================
// EMULATOR UI FUNCTIONS
// =========================================================================


unsigned char* Emu::LoadRawFont(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char* buf = (unsigned char*)malloc(size);
    if (buf) fread(buf, 1, size, f);
    fclose(f);
    return buf;
}

bool Emu::InitMenuFont() {
#ifdef HAVE_SDL_TTF
    TTF_Init();
    iTTFFont = TTF_OpenFont(EMU_FONT, 14);
    return (iTTFFont != NULL);
#endif
#ifdef HAVE_STB_TRUETYPE
    iSTBFontBuffer = LoadRawFont(EMU_FONT);
    return (iSTBFontBuffer != NULL);
#endif
}

void Emu::FreeMenuFont() {
    if (iBMPFont) SDL_FreeSurface(iBMPFont);
#ifdef HAVE_SDL_TTF
    if (iTTFFont) TTF_CloseFont(iTTFFont);
    TTF_Quit();
#endif
#ifdef HAVE_STB_TRUETYPE
    if (iSTBFontBuffer) free(iSTBFontBuffer);
#endif
}

#ifdef HAVE_STB_TRUETYPE
void Emu::DrawOverlayText(const char* text, Uint32 color)
{
    if (!text) return;

    SDL_Surface* dest = Screen();
    float fontHeight = 16;
    int ascent, descent, lineGap; 
    stbtt_fontinfo font;

    if (!stbtt_InitFont(&font, iSTBFontBuffer, 0)) return;

    float scale = stbtt_ScaleForPixelHeight(&font, fontHeight);
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    int baseline = (int)(ascent * scale) + 1;
    Uint8 r, g, b; 
    SDL_GetRGB(color, dest->format, &r, &g, &b);
    int len = strlen(text); 
    int currentX = 5;
    
    SDL_Rect rect = {0, 0, dest->w, baseline + fontHeight};
    SDL_FillRect(dest, &rect, 0x0000);

    for (int i = 0; i < len; ++i) {
        int advance, lsb; 
        stbtt_GetCodepointHMetrics(&font, text[i], &advance, &lsb);
        int bitmapW, bitmapH, xOffset, yOffset;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, scale, scale, text[i], &bitmapW, &bitmapH, &xOffset, &yOffset);
    
        if (bitmap != NULL) {
            if (SDL_MUSTLOCK(dest)) SDL_LockSurface(dest);
            int targetYStart = baseline + yOffset;
            
            for (int srcY = 0; srcY < bitmapH; ++srcY)
            {
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

    SDL_Flip(dest);
}
#endif

void Emu::DrawMenu(SdlListbox* menu) {
    SDL_Surface* screen = Screen();
    SDL_FillRect(screen, NULL, 0x0000);
    
#ifdef HAVE_SDL_TTF
    if (iTTFFont) {
        menu->RenderTTF(screen, iTTFFont);
    }
#endif
#ifdef HAVE_STB_TRUETYPE
    if (iSTBFontBuffer) {
        menu->RenderSTB(screen, iSTBFontBuffer, 14.0f);
    }
#endif
    SDL_Flip(screen);
}


void Emu::ClearEmuScreen()
{
    SDL_FillRect(Screen(), NULL, 0x0000);
    SDL_Flip(Screen());
}


void Emu::DrawViewPager(SdlViewPager* p) 
{
    SDL_Surface* screen = Screen();
    SDL_FillRect(screen, NULL, 0x0000);

#ifdef HAVE_SDL_TTF
    if (iTTFFont) {
        p->RenderTTF(screen, iTTFFont);
    }
#endif
#ifdef HAVE_STB_TRUETYPE
    if (iSTBFontBuffer) {
        p->RenderSTB(screen, iSTBFontBuffer, 16.0f);
    }
#endif
    SDL_Flip(screen);
}

void Emu::DrawMessageBox(SdlMessageBox* box) 
{
    SDL_Surface* screen = Screen();
    SDL_FillRect(screen, NULL, 0x0000);

#ifdef HAVE_SDL_TTF
    if (iTTFFont) {
        box->RenderTTF(iTTFFont);
    }
#endif
#ifdef HAVE_STB_TRUETYPE
    if (iSTBFontBuffer) {
        box->RenderSTB(iSTBFontBuffer, 16.0f);
    }
#endif
    SDL_Flip(screen);
}

void Emu::ShowMessageBox(const char* title, const char* message)
{
    SdlMessageBox* box = new SdlMessageBox(Screen());
    box->SetTheme(msgBoxTheme);
    box->SetTitle(title);
    box->SetConfirmText(""); 
    box->AddLine(message);
    box->ShowSTB(iSTBFontBuffer, 15);
    return;
}



Uint32 Emu::ShowSavedStates()
{
    char* fp = GetStateFilePath();

    Uint32 maxSlots = iStateManager->getSlotCount() + 1;
    Uint32 slotNum = SDL_max(1, iStateManager->getLastSavedSlot());

    char titleBuf[MAX_UI_STR_LEN]={0};
    SdlViewPager* pager = new SdlViewPager(Screen(), iFontSize);
    pager->SetTheme(darkTheme);
    
    bool waiting = true;    
    while (waiting) 
    {
        if (iStateManager->slotExists(slotNum)) 
        {  
            sprintf(titleBuf, "Slot %u", slotNum);
        }
        else 
        { 
            sprintf(titleBuf, "Slot %u: [ Empty]", slotNum); 
        }
                   
        pager->SetTitle(titleBuf);
    
        if (!iStateManager->getSlotFilename(slotNum, fp, PATH_MAX)) return 0; 

        strncat(fp, ".bmp", PATH_MAX);

        SDL_Surface* stateThumbnail = SDL_LoadBMP(fp);

        pager->SetBitmap(stateThumbnail);
        if (stateThumbnail)
        {
            SDL_FreeSurface(stateThumbnail);
        }       
        DrawViewPager(pager);

        SDL_Event event;
        SDL_WaitEvent(&event);
        if (HandleResizeEvent(&event))
        {
            continue;
        }
        
        if (event.type == SDL_KEYDOWN)
        {
            switch(event.key.keysym.sym)
            {
                case SDLK_LEFT:
                {
                    if (slotNum > 1) slotNum--;
                    else { slotNum = maxSlots; }
                }
                    break;
                case SDLK_RIGHT:
                {
                    if (slotNum < maxSlots) slotNum++;
                    else { slotNum = 1; }
                }
                    break;
                case SDLK_RETURN:   
                    waiting = false;
                    break;
                case SDLK_ESCAPE:   
                    slotNum = 0; 
                    waiting = false;
                    break;
                default:
                    break;
            }
        }
    }

    delete pager;
    return slotNum;
}

void Emu::ShowInputSettings()
{
    int keymapIndex = 0;
    char titleBuf[100] = {0,};


    SdlMessageBox* box = new SdlMessageBox(Screen());
    box->SetTheme(msgBoxTheme);
   
    SDL_Event event;
    SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL); // disable
    
    while (keymapIndex <= EMU_NUM_BUTTONS) 
    {
        box->Clear();
        box->SetConfirmText("< press [back] to close >"); 
        if (keymapIndex == EMU_NUM_BUTTONS)
        {
            box->AddLine("key mappings done!");
        }
        else
        {
            sprintf(titleBuf, "press [%s] key", emucore_button_names[keymapIndex]);
            box->AddLine(titleBuf);
            box->AddLine("");
            SDLKey currentBindKey = (SDLKey)(iConfig.keys[keymapIndex]);
            
	    sprintf(titleBuf, "current key [%s]", SDL_GetKeyName(currentBindKey));
            box->AddLine(titleBuf);
            box->AddLine("");
        }

        DrawMessageBox(box);
        SDL_WaitEvent(&event);
        if (HandleResizeEvent(&event))
        {
            continue;
        }

        if (event.type != SDL_KEYDOWN)
	    continue;
        
	SDLKey key = event.key.keysym.sym;
        if (key == SDLK_ESCAPE) break;
     
#ifdef __SYMBIAN32__
	if ( key == SDLK_HOME) continue;
#endif
	iConfig.keys[keymapIndex] = key;
        keymapIndex++;
    }
    
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL); // enable
    delete box;
}


void Emu::SetupDefaultInputKeys()
{
    int bondedkeys = 0;

    for (int i = 0; i < EMU_NUM_BUTTONS; i++ )
    {
	if ( iConfig.keys[i] >= 1 )
	    bondedkeys++;
    }
    
    // prompt the user to setup controls
    if ( bondedkeys == 0 ) {
	ShowInputSettings();
    }
}



/////////////////////////////////////////////
///  VIDEO SETTINGS
///
////////////////////////////////////////////



void Emu::DrawVideoSettings(SdlListbox* listbox, int itemIndex)
{
    listbox->Clear();

    char buffer[100] = {0,};  

    sprintf(buffer, "Full screen: %s", iConfig.fullscreen ? "on" : "off");
    listbox->AddItem(buffer);

    sprintf(buffer, "Skip frames: %d", iConfig.skipFrames);
    listbox->AddItem(buffer);


    const char* const filters[] = {
	"none",
	"scanlines",
	"bilinear"
    };

    sprintf(buffer, "Filter: %s", filters[iConfig.filter]);
    listbox->AddItem(buffer);

    if ( iConfig.delay ) {
	sprintf(buffer, "Delay: %u ms", iConfig.delay );
    }
    else {
	sprintf(buffer, "Delay: auto");
    }
    listbox->AddItem(buffer);

    listbox->SetSelectedIndex(itemIndex);
    DrawMenu(listbox);
}



bool Emu::UpdateVideoSettings(SdlListbox* listbox, SDL_Event& event, bool* configChanged)
{
    using namespace VideoSettingsAction;

    listbox->HandleInput(event);
    int activeRow = listbox->GetSelectedIndex();
    
    SDLKey key = event.key.keysym.sym;
    bool redrawNeeded = (key == SDLK_UP || key == SDLK_DOWN);

    if (key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_RETURN) 
    {
        *configChanged = true;
        redrawNeeded = true;
        switch (activeRow) {
            case SET_FULLSCREEN:
                iConfig.fullscreen ^= 1;
                break;

            case SET_FRAMESKIP: {
                int n = iConfig.skipFrames + ((key == SDLK_RIGHT) ? 1 : -1);
                iConfig.skipFrames = SDL_max(0, SDL_min(n, 60));
                }
		break;

            case SET_FILTER: {
		iConfig.filter += (key == SDLK_RIGHT) ? 1 : -1;
		iConfig.filter  = SDL_max(0, SDL_min(iConfig.filter, 2));
		}
                break;
            case SET_DELAY: {

		iConfig.delay += (key == SDLK_RIGHT) ? 1 : -1;
		iConfig.delay  = SDL_max(0, SDL_min(iConfig.delay, 16));
		}
                break;
        }

    }

    if (redrawNeeded) DrawVideoSettings(listbox, activeRow);

    return (key != SDLK_ESCAPE); // close the listbox
}

void Emu::ShowVideoSettings()
{
    SdlListbox* listbox = new SdlListbox(Screen(), LAYOUT_FULLSCREEN, iFontSize);
    listbox->SetTheme(darkTheme);       
    DrawVideoSettings(listbox, 0); 
    
    bool configChanged = false;
    SDL_Event event;
    while (SDL_WaitEvent(&event)) 
    {
        if (HandleResizeEvent(&event))
        {
            continue;
        }

        if (event.type != SDL_KEYDOWN) continue;
        if (!UpdateVideoSettings(listbox, event, &configChanged)) break;
    }

    if (configChanged) ApplyConfig(); 
    delete listbox;
}


/////////////////////////////////////////////
///  AUDIO SETTINGS
///
////////////////////////////////////////////



void Emu::DrawAudioSettings(SdlListbox* listbox, int itemIndex)
{
    listbox->Clear();

    char buffer[100] = {0,};  
    sprintf(buffer, "Audio: %s", iConfig.audioEnabled ? "on" : "off");
    listbox->AddItem(buffer);

    sprintf(buffer, "Volume: %d%%", iConfig.audioVolume);
    listbox->AddItem(buffer);
#ifdef __SYMBIAN32__
    const char* streamer[] = {"SDL", "default", "system queue"};
    sprintf(buffer, "Stream mode: %s", streamer[iConfig.audioStream]);
    listbox->AddItem(buffer);
#endif

    const char* freq = (iConfig.audioFreq == 44100) ? "44100" : "22050";  
    sprintf(buffer, "Frequency: %sHz", freq);
    listbox->AddItem(buffer);

    listbox->SetSelectedIndex(itemIndex);
    DrawMenu(listbox);
}



#ifdef __SYMBIAN32__
bool Emu::UpdateAudioSettings(SdlListbox* listbox, SDL_Event& event, bool* configChanged)
{
    using namespace AudioSettingsAction;

    listbox->HandleInput(event);
    int activeRow = listbox->GetSelectedIndex();
    
    SDLKey key = event.key.keysym.sym;
    bool redrawNeeded = (key == SDLK_UP || key == SDLK_DOWN);

    if (key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_RETURN) 
    {
        *configChanged = true;
        redrawNeeded = true;
        switch (activeRow) {
	    case TOGGLE_AUDIO: // Toggle Audio State
                iConfig.audioEnabled ^= 1;
                break;

            case SET_VOLUME: {
                int v = iConfig.audioVolume + ((key == SDLK_RIGHT) ? 1 : -1);
                iConfig.audioVolume = SDL_max(0, SDL_min(v, 10));
                }
                break;

            case SET_STREAM:
		iConfig.audioStream = (iConfig.audioStream  + 1) % 3;
                break;	
	    case SET_FREQ:
                iConfig.audioFreq = ((key == SDLK_RIGHT) ? 44100 : 22050);
		break;

        }

    }

    if (redrawNeeded) DrawAudioSettings(listbox, activeRow);
    return (key != SDLK_ESCAPE); // close the listbox
}
#else
bool Emu::UpdateAudioSettings(SdlListbox* listbox, SDL_Event& event, bool* configChanged)
{

    using namespace AudioSettingsAction;

    listbox->HandleInput(event);
    int activeRow = listbox->GetSelectedIndex();
    
    SDLKey key = event.key.keysym.sym;
    bool redrawNeeded = (key == SDLK_UP || key == SDLK_DOWN);

    if (key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_RETURN) 
    {
        *configChanged = true;
        redrawNeeded = true;
        switch (activeRow) {
            case TOGGLE_AUDIO: // Toggle Audio State
                iConfig.audioEnabled ^= 1;
                break;

            case SET_VOLUME: {
                int v = iConfig.audioVolume + ((key == SDLK_RIGHT) ? 1 : -1);
                iConfig.audioVolume = SDL_max(0, SDL_min(v, 10));
                }
                break;
	    case SET_FREQ:
                iConfig.audioFreq = ((key == SDLK_RIGHT) ? 44100 : 22050);
		break;
        }

    }

    if (redrawNeeded) DrawAudioSettings(listbox, activeRow);

    return (key != SDLK_ESCAPE); // close the listbox
}
#endif

void Emu::ShowAudioSettings()
{
    SdlListbox* listbox = new SdlListbox(Screen(), LAYOUT_FULLSCREEN, iFontSize);
    listbox->SetTheme(darkTheme);       
    DrawAudioSettings(listbox, 0); 
    bool configChanged = false;
    SDL_Event event;
    while (SDL_WaitEvent(&event)) 
    {
        if (HandleResizeEvent(&event))
        {
            continue;
        }

        if (event.type != SDL_KEYDOWN) continue;
        if (!UpdateAudioSettings(listbox, event, &configChanged)) break;
    }

    if (configChanged) ApplyConfig(); 
    delete listbox;
}



void Emu::ShowSettingsMenu()
{
    using namespace SettingsMenuAction;
    SdlListbox* options = new SdlListbox(Screen(), LAYOUT_FULLSCREEN, iFontSize);
    options->SetTheme(darkTheme);

    options->AddItem("1 - Input settings");
    options->AddItem("2 - Video settings");
    options->AddItem("3 - Audio settings");

    SDL_Event event;

    while (true) 
    {
        DrawMenu(options);
        SDL_WaitEvent(&event);
        if (HandleResizeEvent(&event))
        {
            continue;
        }
        options->HandleInput(event);

        SDLKey key = event.key.keysym.sym;

        if (event.type != SDL_KEYDOWN) continue;

        if (key == SDLK_ESCAPE) break;
        if (key != SDLK_RETURN) continue;

        int action = options->GetSelectedIndex();

        if (action == INPUT_SETTINGS) ShowInputSettings();
	else if (action == VIDEO_SETTINGS) ShowVideoSettings();         
        else if (action == AUDIO_SETTINGS) ShowAudioSettings();         
    }
    

    delete options;
    ClearEmuScreen();
}



void Emu::ShowOptionsMenu() 
{

    using namespace OptionMenuAction;
    SDL_Surface* stateBmp = PrepareStateBitmap();
    SdlListbox* options = new SdlListbox(Screen(), LAYOUT_CENTER_MODAL, iFontSize);
    options->SetTheme(darkTheme);
    options->AddItem("1 - Load ROM");
    options->AddItem("2 - Reset");
    options->AddItem("3 - Save state");
    options->AddItem("4 - Load state");
    options->AddItem("5 - Settings");
    options->AddItem("6 - Exit");


    SDL_Event event;
    while (true) 
    {
        DrawMenu(options);
        SDL_WaitEvent(&event);
        if (HandleResizeEvent(&event)) continue;
    
        options->HandleInput(event);

        SDLKey key = event.key.keysym.sym;

        if (event.type != SDL_KEYDOWN) continue;

        if (key == SDLK_ESCAPE) break;
        if (key != SDLK_RETURN) continue;

        int action = options->GetSelectedIndex();

        if (action == SETTINGS_MENU) {
	    ShowSettingsMenu();
	    continue; 
	}

	else if (action == LOAD_ROM) LoadROM(NULL);     
        else if (action == RESET_EMU ) emucore_reset();       
        else if (action == SAVE_STATE) SaveState(stateBmp);       
        else if (action == LOAD_STATE) LoadState();       

        iRunning = (action != EXIT_EMU);
	break;	
    }

    delete options;
    ClearEmuScreen();
}

void Emu::ShowAboutMenu()
{
    SdlListbox* menu = new SdlListbox(Screen(), LAYOUT_FULLSCREEN, 20);
    menu->SetTheme(darkTheme);
    menu->AddItem(EMUCORE_NAME " for symbian");
    menu->AddItem("Version: "EMU_VERSION);
    menu->AddItem("Developer: JigokuMaster");
    menu->AddItem("Testers: ACER7, Dante");
    menu->AddItem("Emulator core: "EMUCORE_AUTHOR);
    menu->AddItem("Emulator icon: ACER7");
    menu->AddItem("Emulator font: Style-7");
    SDL_Event event;
    while (true) 
    {
        DrawMenu(menu);
        SDL_WaitEvent(&event);
        if (HandleResizeEvent(&event))
        {
            continue;
        }

        SDLKey key = event.key.keysym.sym;
        if (event.type != SDL_KEYDOWN) continue;

        if (key == SDLK_ESCAPE) break;
        
        menu->HandleInput(event);
    }

    delete menu;
}

const char* Emu::GetROMFilePath(const char* romFileName) {
    memset(iRomPathBuf, 0, sizeof(iRomPathBuf));
#ifdef __SYMBIAN32__
    const wchar_t* fp = (wchar_t*)romFileName;
    size_t nBytes = wcstombs(iRomPathBuf, fp, sizeof(iRomPathBuf));
    if ( nBytes != (size_t)-1 ){
        TBuf8<(PATH_MAX+1)> fpPtr((Uint8*)iRomPathBuf);
        fpPtr.Insert(2, _L8(ROMS_PATH_PREFIX));
        sprintf(iRomsPath, "%c:"ROMS_PATH_PREFIX, fpPtr[0]);
        
        //PRINT_MSG(("ROMS_PATH %s", iRomsPath));
        return strncpy(iRomPathBuf, (char*)fpPtr.PtrZ(), sizeof(iRomPathBuf));
    }
#else
    if (iRomsPath) {
        sprintf(iRomPathBuf , "%s/%s", iRomsPath, romFileName);
        return iRomPathBuf;
    }
#endif
    return NULL;
}

bool Emu::LoadROM(char* romFilePath, bool reset)
{
    if (!romFilePath) {
        romFilePath = (char*)ShowROMList();
        if (!romFilePath) return false;
    }

    if (romFilePath) {
        if (!emucore_load_rom(romFilePath))
        {
            PRINT_ERRMSG(("failed to load ROM"));
            return false;
        }
        
        InitStateManager();
        if (reset) emucore_reset();
        ClearEmuScreen();
        InitSysBitmap();
	iNextFrameTicks = SDL_GetTicks() + ( EMUCORE_FRAMERATE == 50 ? 20 : 50);

    }
    return true;
}


const char* Emu::ShowROMList()
{
    SdlListbox* listbox = new SdlListbox(Screen(), LAYOUT_FULLSCREEN, iFontSize);
    listbox->SetTheme(darkTheme);
    listbox->Clear();
#ifdef __SYMBIAN32__
    PopulateRomList(listbox, _L("C:"ROMS_PATH_PREFIX));
    PopulateRomList(listbox, _L("E:"ROMS_PATH_PREFIX));
    PopulateRomList(listbox, _L("F:"ROMS_PATH_PREFIX));
#else
    memcpy(iRomsPath, "ROMs", 4);
    PopulateRomList(listbox, iRomsPath);
#endif
 
    bool romsCount = listbox->GetItemCount();
    if (romsCount < 1)
    {
#ifdef __SYMBIAN32__
	listbox->AddItem("No ROMs found in:");
	listbox->AddItem("E:"ROMS_PATH_PREFIX);
	listbox->AddItem("C:"ROMS_PATH_PREFIX);
	listbox->AddItem("F:"ROMS_PATH_PREFIX);
#endif
    }

    SDL_Event event;
    const char* fp = NULL;
    while (true)
    {
    
	DrawMenu(listbox);
	SDL_WaitEvent(&event);
        if (HandleResizeEvent(&event))
        {
            continue;
        }

        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            break;
        }
        bool ok = listbox->HandleInput(event);
	// avoid crash if ROMList is empty, thanks to Dante.
        if (ok && (romsCount > 0))
	{
#ifdef __SYMBIAN32__
            fp = GetROMFilePath((const char*)listbox->GetSelectedTextW());
#else
            fp = GetROMFilePath(listbox->GetSelectedText());
#endif
            break;
        }
    }
    
    delete listbox;
    return fp;
}

bool Emu::ShowMainMenu()
{
    using namespace MainMenuAction;

    SdlListbox* menu = new SdlListbox(Screen(), LAYOUT_CENTER_MODAL, 28);
    menu->SetTheme(darkTheme);
    menu->AddItem("1 - Load ROM");
    menu->AddItem("2 - Settings");
    menu->AddItem("3 - About");
    menu->AddItem("4 - Exit");
    DrawMenu(menu);

    bool ret = true;
    SDL_Event event;
    while (SDL_WaitEvent(&event)) 
    {
        if (HandleResizeEvent(&event))
        {
            continue;
        }

        bool ok = menu->HandleInput(event);
        DrawMenu(menu);
        if (!ok) continue;
        int action = menu->GetSelectedIndex();
        if (action == LOAD_ROM)
        {
            if (LoadROM(NULL, false))
                break;
        }
        else if (action == SETTINGS_MENU)
            ShowSettingsMenu();
        else if (action == ABOUT_EMU)
            ShowAboutMenu();
        else if (action == EXIT_EMU)
        {
            ret = false;
            break;
        }
    }

    delete menu;
    return ret;
}



// =========================================================================
// CLEANUP
// =========================================================================


void Emu::CloseAudio()
{

#ifdef __SYMBIAN32__
    DirectAudio_Quit();
    if ( iAudioStreamer ) {
	delete iAudioStreamer;
	iAudioStreamer = NULL;
    }
#endif
    SDL_PauseAudio(1);
    SDL_CloseAudio();

    if ( iSoundState.buffer ) { 
	free(iSoundState.buffer);
	iSoundState.buffer = NULL; 
    }
    if (iSoundState.sem)
    {
	SDL_SemPost(iSoundState.sem);
	SDL_DestroySemaphore(iSoundState.sem);
        iSoundState.sem = NULL;
    }
}

void Emu::CloseVideo() {
    if (iSdlVideo.surf_bitmap) 
    {
        SDL_FreeSurface(iSdlVideo.surf_bitmap);
        iSdlVideo.surf_bitmap = NULL; 
    }
}

void Emu::Shutdown()
{

    SaveConfig();
    emucore_shutdown();
    CloseVideo();
    CloseAudio();
    FreeMenuFont();
    SDL_Quit();
    if (iStateManager) delete iStateManager;
}

bool Emu::HandleResizeEvent(SDL_Event* event)
{
    if (event->type != SDL_VIDEORESIZE) return false;

    PRINT_MSG(("SCREEN MODE CHANGED"));
    int scrWidth = event->resize.w;
    int scrHeight = event->resize.h;

    PRINT_MSG(("SCREEN_SIZE %dx%d", scrWidth, scrHeight));

    iSdlVideo.surf_screen = SDL_SetVideoMode(scrWidth, scrHeight, 16, SDL_SWSURFACE); 
    if (!Screen()) {
        PRINT_ERRMSG(("SDL_SetVideoMode failed: %s", SDL_GetError()));
	iRunning = false;
        return false;
    }

    InitSysBitmap();
    return true;
}

inline bool Emu::HandleFrameSkip() 
{

    bool skipFrame = false;
    if( iConfig.skipFrames > 0 )
    {
	skipFrame = (iSdlVideo.frame_count % iConfig.skipFrames == 0) ? false : true;
    }
    return skipFrame;
}


// Handle frame pacing using simple integer arithmetic, bypassing floating-point overhead and precision issues
inline void Emu::HandleFramePacing()
{

    Uint32 now, waitMs;

    // NTSC (60Hz) 3-Frame Pacing (idea from sdlsms by Gregory Montoir )
     
    // Only throttle the main loop once every 3 frames (3 / 60 sec = 50 ms) 
    
    if ( EMUCORE_FRAMERATE == 60)
    {

	// 3-Frame batching
	if (iSdlVideo.frame_count % 3 == 0) 
	{
	    now = SDL_GetTicks();

	    // 1. If we finished 3 frames early, yield remaining time to the OS
	    if (now < iNextFrameTicks) 
	    {
		waitMs = iNextFrameTicks - now;
		SDL_Delay(waitMs);
	    }

	    // 2. Lag protection: If we fell severely behind (>100 ms), reset target
	    // to prevent "fast-forwarding" catch-up behavior
	    else if (now > iNextFrameTicks + 100) 
	    {
		iNextFrameTicks = now;
	    }

	    // 3. Advance target cumulatively by exactly 50 ms for the next batch
	    iNextFrameTicks += 50;
	}
    }

    // PAL (50Hz) 1-Frame Pacing
    else if ( EMUCORE_FRAMERATE == 50)
    {
	now = SDL_GetTicks();
	if (now < iNextFrameTicks) 
	{
	    SDL_Delay(iNextFrameTicks - now);
	}
	iNextFrameTicks+= 20; // Exact 50 FPS timing
    }
}



inline void Emu::DoFrameSync()
{
    if (

#ifdef __SYMBIAN32__ 
	    (iConfig.audioStream == SDL_AUDIOSTREAM) && 
#endif
	    iSoundState.sem)
    {
	while (SDL_SemTryWait(iSoundState.sem) == 0) {}

	SDL_SemWait(iSoundState.sem);
    }


    if ( iConfig.delay ) {
	SDL_Delay(iConfig.delay);
    }
    else {

	HandleFramePacing();
    }
}


int Emu::Run(int argc, char** argv) {
#ifdef __SYMBIAN32__

    const char* logFile = "D:\\"EMUCORE_NAME".log";

    freopen(logFile, "w+", stdout);
    freopen(logFile, "w+", stderr);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    // another annoying bug in OpenC ...
    /*int fd = open(logFile, O_WRONLY | O_CREAT | O_TRUNC);
    close(1);
    dup2(fd, 1);
    close(2);
    dup2(fd, 2);
    close(fd);*/

    char* privDir = getenv("EPOC_PRIVATE_DIR");
    if (privDir) chdir(privDir);

    PRINT_MSG(("EPOC_PRIVATE_DIR %s", privDir));
    mkdir("C:"ROMS_PATH_PREFIX, 0755); 
    mkdir("E:"ROMS_PATH_PREFIX, 0755);
    mkdir("F:"ROMS_PATH_PREFIX, 0755);
#endif

    ReadConfig();
    
    if (SDL_Init(0) < 0) {
        PRINT_ERRMSG(("SDL initialization failed: %s", SDL_GetError()));
        return 1;
    }
    
    if (!InitVideo()) return 1;
    
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
    
    if (!InitMenuFont()) {
        PRINT_ERRMSG(("failed to load font"));
        return 1;
    }
     
    if (!ShowMainMenu()) return 0;
#ifdef __SYMBIAN32__
    if (!InitAudio()) return 1;
#else
    if (!InitSDLAudio()) return 1;
#endif


    // prompt user to setup input keys if needed.
    SetupDefaultInputKeys();
    emucore_init(iConfig.audioFreq);

    iRunning = true;
    SDL_Event event;
    while (true)
    {
	if (SDL_PollEvent(&event))
        {
	    HandleResizeEvent(&event);
            switch (event.type) {
#ifndef __SYMBIAN32__
                case SDL_USEREVENT: {
                    char caption[10] = {0,};
                    sprintf(caption, "fps %d", event.user.code);
                    DrawOverlayText(caption, 0xFFF00);
                    break;
                }
#endif

                case SDL_QUIT:
                    iRunning = false;
                    break;

                case SDL_KEYDOWN:
                //case SDL_KEYUP:
                    UpdateControlInput(event.key.keysym.sym);
                    break;
            }
        }

        if (!iRunning) break;

        //UpdateEmuCoreInput();

	bool skipFrame = HandleFrameSkip();
	emucore_update_frame(skipFrame);
	if ( iUseSound ) 
	{
#ifdef __SYMBIAN32__
	    UpdateAudioStream();
#else 
	    UpdateSDLAudioStream();
#endif
	}

	if ( iFullscreen ) {
	    UpdateVideoFrameFull(skipFrame); 
	}
	else {
	    UpdateVideoFrame(skipFrame);
	    }
	DoFrameSync();
    }

    return 0;
}


    
static Emu* gEmu = NULL;
extern "C" int sdl_input_update()
{
    if (gEmu) gEmu->UpdateEmuCoreInput();
    return 1;
}

int main(int argc, char** argv)
{
    gEmu = new Emu();
    int ret = gEmu->Run(argc, argv);
    delete gEmu;
    return ret;
}
