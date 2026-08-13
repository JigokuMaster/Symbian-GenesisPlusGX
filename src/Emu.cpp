/*
 * File: Emu.cpp
 * Initial boilerplate/template generated via Gemini.
 * Adapted, refactored, and maintained by JigokuMaster.
 */

#include "SDL.h"
#include "SDL_thread.h"

extern "C" {

#include "shared.h"
#include "sms_ntsc.h"
#include "md_ntsc.h"
}

#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_SDL_TTF
#include <SDL_ttf.h>
#endif

#ifndef STB_TRUETYPE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#endif
#include "SdlUiToolkit.h"
#include "EmuUtils.h"
#include "EmuConfig.h"

#ifdef __SYMBIAN32__
#include <e32std.h>
#include "DirectAudioStream.h"
#define PATH_MAX FILENAME_MAX
#define SOUND_FREQUENCY 22050
#define SOUND_SAMPLES_SIZE  1024
//#define SOUND_FREQUENCY 48000
//#define SOUND_SAMPLES_SIZE  2048

extern "C" void EPOC_SetAudioVolume(int);
#define EMU_FONT "pixelfont-7.ttf"
#define ROMS_PATH_PREFIX "\\Data\\GenesisPlusGX\\"
#else
#define SOUND_FREQUENCY 22050
#define SOUND_SAMPLES_SIZE  1024//2048
#define EMU_FONT "pixelfont-7.ttf"
#endif

#define VIDEO_WIDTH  320
#define VIDEO_HEIGHT 240
#define SMS_SCREEN_WIDTH  256
#define SMS_SCREEN_HEIGHT 192
#define GG_SCREEN_WIDTH 160
#define GG_SCREEN_HEIGHT 144

#define MD_SCREEN_WIDTH  256
#define MD_SCREEN_HEIGHT 224

#define FPS 1000 / 50
 
// =========================================================================
// STATIC SEGA CD FORMAT PATTERN BLOCK
// =========================================================================
static const uint8 K_BRM_FORMAT[0x40] = {
    0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x00,0x00,0x00,0x00,0x40,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x53,0x45,0x47,0x41,0x5f,0x43,0x44,0x5f,0x52,0x4f,0x4d,0x00,0x01,0x00,0x00,0x00,
    0x52,0x41,0x4d,0x5f,0x43,0x41,0x52,0x54,0x52,0x4a,0x44,0x47,0x45,0x5f,0x5f,0x5f
};

md_ntsc_t *md_ntsc;
sms_ntsc_t *sms_ntsc;


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
    INPUT_SETTINGS, 
    OUTPUT_SETTINGS,        
    EXIT_EMU
};
}

namespace MainMenuAction {
    enum  {
    LOAD_ROM = 0, 
    INPUT_SETTINGS, 
    OUTPUT_SETTINGS,
    ABOUT_EMU,        
    EXIT_EMU
};
}



static int gTimerActive = 0;
#ifdef USE_ESDL
static bool gDirectScreenBlit =  false;
#else 
static bool gDirectScreenBlit =  true;
#endif


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
#ifdef ENABLE_SEGACD
		!ext.CompareF(_L(".md")) ||
#endif
		!ext.CompareF(_L(".sg"))
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



#ifdef DEBUG
#define DBG_MSG(args) printf("DEBUG: %s:%d (%s) - ", __FILE__, __LINE__, __FUNCTION__); printf args; printf("\n")
#endif

#define PRINT_MSG(...) do {         \
    fprintf(stderr, __VA_ARGS__);   \
    fputc('\n', stderr);            \
} while (0)

#define PRINT_ERRMSG(...) do {         \
    fprintf(stderr, "[ERROR] ");   \
    fprintf(stderr, __VA_ARGS__);   \
    fputc('\n', stderr);            \
} while (0)



// =========================================================================
// GenesisPlusGX SDL 1.2 front-end
// =========================================================================


class Emu {
private:

    EmuConfig iConfig;
    int iEmuScreenWidth;
    int iEmuScreenHeight;
    int iSysScreenWidth;
    int iSysScreenHeight;
    bool iRunning;
    int iJoyNum;
    int iUseSemSync;
    int iUseSound;
    int iFullscreen;

    // Audio Subsystem Components
    struct SdlSoundState {
        char* current_pos;
        char* buffer;
        int current_emulated_samples;
    } iSdlSound;
    short iSoundFrame[SOUND_SAMPLES_SIZE];

    // Video Subsystem Components
    //md_ntsc_t* iMdNtsc;
    //sms_ntsc_t* iSmsNtsc;
    struct SdlVideoState {
        SDL_Surface* surf_screen;
        SDL_Surface* surf_bitmap;
        SDL_Rect srect;
        SDL_Rect drect;
        Uint32 frames_rendered;
    } iSdlVideo;


    // Synchronization Elements
    struct SdlSyncState {
        SDL_sem* sem_sync;
        unsigned ticks;
    } iSdlSync;
    
    Uint32 iFrameMS;
    Uint32 iNextFrameTicks;
    Uint32 iFrameSkipCount;



    // UI & Typography Assets
    SDL_Surface* iBMPFont;
#ifdef HAVE_SDL_TTF
    TTF_Font* iTTFFont;
#endif
#ifdef HAVE_STB_TRUETYPE
    unsigned char* iSTBFontBuffer;
#endif
    int iFontSize;

    char* iRomsPath;
    char iRomPathBuf[PATH_MAX+1];
    char iStatePath[PATH_MAX+1];
    char iStateFilePath[PATH_MAX+1];
    char iRomName[FILENAME_MAX+1];
    StateManager* iStateManager;

    // VDP Video Timing Mapping Array
    uint16 iVcTable[4][2];


public:
    Emu()
        : iEmuScreenWidth(0), iEmuScreenHeight(0),
	  iSysScreenWidth(SMS_SCREEN_WIDTH), iSysScreenHeight(SMS_SCREEN_HEIGHT),
	  iRunning(false),iJoyNum(0),
	  iUseSemSync(true), iUseSound(true), iFullscreen(false),
          iBMPFont(NULL), iFontSize(34), iRomsPath(NULL), iStateManager(NULL), iFrameSkipCount(0)
    {
        memset(&iSdlSound, 0, sizeof(iSdlSound));
        memset(&iSdlVideo, 0, sizeof(iSdlVideo));
        memset(&iSdlSync, 0, sizeof(iSdlSync));
        memset(iSoundFrame, 0, sizeof(iSoundFrame));
        memset(iRomPathBuf, 0, sizeof(iRomPathBuf));
	memset(&bitmap, 0, sizeof(t_bitmap));


#ifdef HAVE_SDL_TTF
        iTTFFont = NULL;
#endif
#ifdef HAVE_STB_TRUETYPE
        iSTBFontBuffer = NULL;
#endif

        // Populate VC Frame Timing Lookups
        iVcTable[0][0] = 0xDA;  iVcTable[0][1] = 0xF2;   // Mode 4 (192 lines)
        iVcTable[1][0] = 0xEA;  iVcTable[1][1] = 0x102;  // Mode 5 (224 lines)
        iVcTable[2][0] = 0xDA;  iVcTable[2][1] = 0xF2;   // Mode 4 (192 lines)
        iVcTable[3][0] = 0x106; iVcTable[3][1] = 0x10A;  // Mode 5 (240 lines)
    }

    ~Emu() {}

    // Global audio callback router
    static void AudioCallbackWrapper(void* userdata, Uint8* stream, int len) {
        if (userdata) {
            static_cast<Emu*>(userdata)->HandleAudioCallback(stream, len);
        }
    }

    // Global timer sync callback router
    static Uint32 TimerCallbackWrapper(Uint32 interval, void* param) {
	if (!gTimerActive) return interval;
        if (param) {
            return static_cast<Emu*>(param)->HandleTimerCallback(interval);
        }
        return interval;
    }

public:
    void HandleAudioCallback(Uint8* stream, int len);
    Uint32 HandleTimerCallback(Uint32 interval);
private:

    void SetTimerState(int state);
    void UpdateAudioStream();
    void UpdateVideoFrameDirect();
    void UpdateVideoFrame();
    void UpdateVideoFrameFull();
    int InitSync();
    int InitAudio();
    void InitSysBitmap();
    void ConfigureVideoBlitRect();
    int InitVideo();
    void ClearEmuScreen();
    const char* GetROMFilePath(const char* romFileName);
    unsigned char* LoadRawFont(const char* filename);
    bool InitMenuFont();
    void FreeMenuFont();

#ifdef HAVE_STB_TRUETYPE
    void DrawOverlayText(const char* text, Uint32 color);
#endif

    void DrawMenu(SdlListbox* menu);
    void DrawViewPager(SdlViewPager* p);
    void DrawMessageBox(SdlMessageBox* box);
    void ShowMessageBox(const char* title, const char* message);


#ifdef ENABLE_SEGACD
    void SaveSegaCdBackupRam();
    void LoadSegaCdBackupRam(); 
#endif

    bool ReadConfig();
    void SaveConfig();
    void ApplyConfig();
    void InitStateManager();
    char* GetStateFilePath();
    SDL_Surface*  PrepareStateBitmap();
    void SaveStateBMP(char* fp, SDL_Surface* bmp=NULL);
    void AutoSaveState();
    void AutoLoadState();
    void SaveState(SDL_Surface* bmp=NULL);
    void LoadState();
    Uint32 ShowSavedStates();
    void ShowInputSettings();
    void DrawOutputSettings(SdlListbox* listbox, int itemIndex);
    bool UpdateOutputSettings(SdlListbox* listbox, SDL_Event& event, bool* configChanged);
    void ShowOutputSettings();
    bool LoadROM(char* romFilePath = NULL, bool reset = true);
    bool ShowOptionsMenu();
    void ShowAboutMenu();
    const char* ShowROMList();
    bool ShowMainMenu();
    void CloseAudio();
    void CloseVideo();
    void CloseSync();
    void Shutdown();
    bool HandleResizeEvent(SDL_Event* event);
    void HandleKeyEvent(SDLKey key);
    bool DoFrameSync();
public:

    int UpdateInputDevice();
    int Run(int argc, char** argv);
};



// =========================================================================
// INPUT & OUTPUT UPDATE HANDLERS
// =========================================================================


void Emu::SetTimerState(int state)
{
    gTimerActive = state;
    if (state == SDL_DISABLE)
    {
	iNextFrameTicks = SDL_GetTicks();
	iFrameSkipCount = 0;
    }
}


Uint32 Emu::HandleTimerCallback(Uint32 interval) {
        SDL_SemPost(iSdlSync.sem_sync);
        iSdlSync.ticks++;
#ifndef __SYMBIAN32__
        if (iSdlSync.ticks == (vdp_pal ? 50 : 20)) {
            SDL_Event event;
            SDL_UserEvent userevent;

            userevent.type = SDL_USEREVENT;
            userevent.code = vdp_pal ? (iSdlVideo.frames_rendered / 3) : iSdlVideo.frames_rendered;
            userevent.data1 = NULL;
            userevent.data2 = NULL;
            iSdlSync.ticks = iSdlVideo.frames_rendered = 0;

            event.type = SDL_USEREVENT;
            event.user = userevent;
            SDL_PushEvent(&event);
        }
#endif
        return interval;
    }

#ifdef SYMBIAN_DIRECT_AUDIO
inline void Emu::UpdateAudioStream()
{
    // 1. Generate core audio frame samples into iSoundFrame
    int totalSamples = audio_update(iSoundFrame) * 2;

    if (totalSamples > 0)
    {
        // 2. Queue raw PCM 16-bit samples directly to the ring buffer
        DirectAudio_Queue(iSoundFrame, totalSamples);
    }

    // 3. Pump active signals and handle underflow recovery
    DirectAudio_Service();
}

#else
void Emu::HandleAudioCallback(Uint8* stream, int len) {
        if (iSdlSound.current_emulated_samples < len) {
            memset(stream, 0, len);
        } else {
            memcpy(stream, iSdlSound.buffer, len);
            do {
                iSdlSound.current_emulated_samples -= len;
            } while (iSdlSound.current_emulated_samples > 2 * len);
            memcpy(iSdlSound.buffer, iSdlSound.current_pos - iSdlSound.current_emulated_samples, iSdlSound.current_emulated_samples);
            iSdlSound.current_pos = iSdlSound.buffer + iSdlSound.current_emulated_samples;
        }
    }

inline void Emu::UpdateAudioStream() 
{
	int size = audio_update(iSoundFrame) * 2;
        if (SDL_GetAudioStatus() != SDL_AUDIO_PLAYING) return;
        SDL_LockAudio();
        short* out = (short*)iSdlSound.current_pos;
        //memcpy(out, iSoundFrame, size * 2);
        //out += size;
	for(int i = 0; i < size; i++) {
	    *out++ = iSoundFrame[i];
	}

        iSdlSound.current_pos = (char*)out;
        iSdlSound.current_emulated_samples += size * sizeof(short);
        SDL_UnlockAudio();
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


inline void Emu::UpdateVideoFrameDirect() 
{

	int skipFrame = iFrameSkipCount > 0;
	if(iUseSemSync && iConfig.skipFrames)
	{
	    skipFrame = (iSdlVideo.frames_rendered % iConfig.skipFrames == 0) ? 0 : 1;
	}
 
	//SDL_LockSurface(iSdlVideo.surf_screen);
#ifdef  ENABLE_SEGACD
	if (system_hw == SYSTEM_MCD) {
            system_frame_scd(skipFrame);
        } else if ((system_hw & SYSTEM_PBC) == SYSTEM_MD) {
            system_frame_gen(skipFrame);
        } else 
#endif
	{
            system_frame_sms(skipFrame);
        }
	//SDL_UnlockSurface(iSdlVideo.surf_screen);
	 
        if (bitmap.viewport.changed & 1 && !skipFrame)
	{

	    SDL_UpdateRect(iSdlVideo.surf_screen, blitter_x, blitter_y, blitter_w, blitter_h);
        }
        ++iSdlVideo.frames_rendered;
    }


inline void Emu::UpdateVideoFrameFull() {
  
	int skipFrame = iFrameSkipCount > 0;
	if(iUseSemSync && iConfig.skipFrames)
	{
	    skipFrame = (iSdlVideo.frames_rendered % iConfig.skipFrames == 0) ? 0 : 1;
	}
 
        system_frame_sms(skipFrame);
        
        if (!skipFrame) {
       
	    //SDL_SoftStretch(iSdlVideo.surf_bitmap, &iSdlVideo.srect, iSdlVideo.surf_screen, &iSdlVideo.drect);
	    SoftStretchWithScanlines(iSdlVideo.surf_bitmap,
		    &iSdlVideo.srect,
		    iSdlVideo.surf_screen,
		    &iSdlVideo.drect, iConfig.scanlines);
    
	    SDL_UpdateRect(iSdlVideo.surf_screen,
		    iSdlVideo.drect.x,
		    iSdlVideo.drect.y,
		    iSdlVideo.drect.w,
		    iSdlVideo.drect.h);
        }
        ++iSdlVideo.frames_rendered;
	
    }

inline void Emu::UpdateVideoFrame()
{
	if ( iFullscreen )
	{
	    UpdateVideoFrameFull();
	    return;
	}

	else if (gDirectScreenBlit)
	{
	    UpdateVideoFrameDirect();
	    return;
	}

	int skipFrame = iFrameSkipCount > 0;
	if(iUseSemSync && iConfig.skipFrames)
	{
	    skipFrame = (iSdlVideo.frames_rendered % iConfig.skipFrames == 0) ? 0 : 1;
	}
 

#ifdef  ENABLE_SEGACD
	if (system_hw == SYSTEM_MCD) {
            system_frame_scd(skipFrame);
        } else if ((system_hw & SYSTEM_PBC) == SYSTEM_MD) {
            system_frame_gen(skipFrame);
        } else 
#endif
	{
            system_frame_sms(skipFrame);
        }
        
        if (bitmap.viewport.changed & 1 && !skipFrame)
	{
            iSdlVideo.srect.w = bitmap.viewport.w;
            iSdlVideo.srect.h = bitmap.viewport.h;

            iSdlVideo.srect.x = bitmap.viewport.x;
            iSdlVideo.srect.y = bitmap.viewport.y;

            iSdlVideo.drect.x = (iEmuScreenWidth - bitmap.viewport.w ) >> 1;
            iSdlVideo.drect.y = (iEmuScreenHeight - bitmap.viewport.h) >> 1;

            iSdlVideo.drect.w = bitmap.viewport.w;
            iSdlVideo.drect.h = bitmap.viewport.h;
	    if (iConfig.scanlines)
	    {
		SoftStretchWithScanlines(iSdlVideo.surf_bitmap,
		    &iSdlVideo.srect,
		    iSdlVideo.surf_screen,
		    &iSdlVideo.drect, 1);
	    }
	    else{
		SDL_BlitSurface(iSdlVideo.surf_bitmap,
			&iSdlVideo.srect,
			iSdlVideo.surf_screen,
			&iSdlVideo.drect);
	    }

	    SDL_UpdateRect(iSdlVideo.surf_screen, iSdlVideo.drect.x, iSdlVideo.drect.y, iSdlVideo.drect.w, iSdlVideo.drect.h);

        }
        ++iSdlVideo.frames_rendered;
    }

inline int Emu::UpdateInputDevice() {
        uint8* keystate = SDL_GetKeyState(NULL);
        input.pad[iJoyNum] = 0;
        switch (input.dev[iJoyNum]) {
            case DEVICE_LIGHTGUN: {
                int x, y; int state = SDL_GetMouseState(&x, &y);
		//char msg[64] = {0};
		//sprintf(msg, "mouse %d,%d", x, y);
		//DrawOverlayText(msg, 0xFFFF);
                input.analog[iJoyNum][0] = x - (VIDEO_WIDTH - bitmap.viewport.w) / 2;
                input.analog[iJoyNum][1] = y - (VIDEO_HEIGHT - bitmap.viewport.h) / 2;
                if (state & SDL_BUTTON_LMASK) input.pad[iJoyNum] |= INPUT_A;
                if (state & SDL_BUTTON_RMASK) input.pad[iJoyNum] |= INPUT_B;
                if (state & SDL_BUTTON_MMASK) input.pad[iJoyNum] |= INPUT_C;
                if (keystate[iConfig.keys[EMU_INPUT_START]]) input.pad[iJoyNum] |= INPUT_START;
                break;
            }
            case DEVICE_PADDLE: {
                int x; int state = SDL_GetMouseState(&x, NULL);
                input.analog[iJoyNum][0] = x * 256 / VIDEO_WIDTH;
                if (state & SDL_BUTTON_LMASK) input.pad[iJoyNum] |= INPUT_B;
                break;
            }
            case DEVICE_SPORTSPAD: {
                int x, y; int state = SDL_GetRelativeMouseState(&x, &y);
                input.analog[iJoyNum][0] = (unsigned char)(-x & 0xFF);
                input.analog[iJoyNum][1] = (unsigned char)(-y & 0xFF);
                if (state & SDL_BUTTON_LMASK) input.pad[iJoyNum] |= INPUT_B;
                if (state & SDL_BUTTON_RMASK) input.pad[iJoyNum] |= INPUT_C;
                break;
            }
            case DEVICE_MOUSE: {
                int x, y; int state = SDL_GetRelativeMouseState(&x, &y);
                input.analog[iJoyNum][0] = x * 2; input.analog[iJoyNum][1] = y * 2;
                if (!config.invert_mouse)     input.analog[iJoyNum][1] = 0 - input.analog[iJoyNum][1];
                if (state & SDL_BUTTON_LMASK) input.pad[iJoyNum] |= INPUT_B;
                if (state & SDL_BUTTON_RMASK) input.pad[iJoyNum] |= INPUT_C;
                if (state & SDL_BUTTON_MMASK) input.pad[iJoyNum] |= INPUT_A;
                if (keystate[SDLK_f])         input.pad[iJoyNum] |= INPUT_START;
                break;
            }
            case DEVICE_XE_1AP:
                if (keystate[SDLK_a])  input.pad[iJoyNum] |= INPUT_START;
                if (keystate[SDLK_s])  input.pad[iJoyNum] |= INPUT_A;
                if (keystate[SDLK_d])  input.pad[iJoyNum] |= INPUT_C;
                if (keystate[SDLK_f])  input.pad[iJoyNum] |= INPUT_Y;
                if (keystate[SDLK_z])  input.pad[iJoyNum] |= INPUT_B;
                if (keystate[SDLK_x])  input.pad[iJoyNum] |= INPUT_X;
                if (keystate[SDLK_c])  input.pad[iJoyNum] |= INPUT_MODE;
                if (keystate[SDLK_v])  input.pad[iJoyNum] |= INPUT_Z;
                if (keystate[SDLK_UP]) input.analog[iJoyNum][1] -= 2; else if (keystate[SDLK_DOWN]) input.analog[iJoyNum][1] += 2; else input.analog[iJoyNum][1] = 128;
                if (keystate[SDLK_LEFT]) input.analog[iJoyNum][0] -= 2; else if (keystate[SDLK_RIGHT]) input.analog[iJoyNum][0] += 2; else input.analog[iJoyNum][0] = 128;
                if (keystate[SDLK_KP8]) input.analog[iJoyNum + 1][0] -= 2; else if (keystate[SDLK_KP2]) input.analog[iJoyNum + 1][0] += 2; else if (keystate[SDLK_KP4]) input.analog[iJoyNum + 1][0] -= 2; else if (keystate[SDLK_KP6]) input.analog[iJoyNum + 1][0] += 2; else input.analog[iJoyNum + 1][0] = 128;
                if (input.analog[iJoyNum][0] > 0xFF) input.analog[iJoyNum][0] = 0xFF; else if (input.analog[iJoyNum][0] < 0) input.analog[iJoyNum][0] = 0;
                if (input.analog[iJoyNum][1] > 0xFF) input.analog[iJoyNum][1] = 0xFF; else if (input.analog[iJoyNum][1] < 0) input.analog[iJoyNum][1] = 0;
                break;
            case DEVICE_PICO: {
                int x, y; int state = SDL_GetMouseState(&x, &y);
                input.analog[0][0] = 0x3c + (x * (0x17c - 0x03c + 1)) / VIDEO_WIDTH;
                input.analog[0][1] = 0x1fc + (y * (0x2f7 - 0x1fc + 1)) / VIDEO_HEIGHT;
                if (state & SDL_BUTTON_MMASK) pico_current = (pico_current + 1) & 7;
                if (state & SDL_BUTTON_RMASK) input.pad[0] |= INPUT_PICO_RED;
                if (state & SDL_BUTTON_LMASK) input.pad[0] |= INPUT_PICO_PEN;
                break;
            }
            case DEVICE_TEREBI: {
                int x, y; int state = SDL_GetMouseState(&x, &y);
                input.analog[0][0] = (x * 250) / VIDEO_WIDTH; input.analog[0][1] = (y * 250) / VIDEO_HEIGHT;
                if (state & SDL_BUTTON_RMASK) input.pad[0] |= INPUT_B;
                break;
            }
            case DEVICE_GRAPHIC_BOARD: {
                int x, y; int state = SDL_GetMouseState(&x, &y);
                input.analog[0][0] = (x * 255) / VIDEO_WIDTH; input.analog[0][1] = (y * 255) / VIDEO_HEIGHT;
                if (state & SDL_BUTTON_LMASK) input.pad[0] |= INPUT_GRAPHIC_PEN;
                if (state & SDL_BUTTON_RMASK) input.pad[0] |= INPUT_GRAPHIC_MENU;
                if (state & SDL_BUTTON_MMASK) input.pad[0] |= INPUT_GRAPHIC_DO;
                break;
            }
            case DEVICE_SMASH:
                if (keystate[SDLK_KP9]) input.pad[iJoyNum] |= INPUT_SMASH_UP_RIGHT;
                if (keystate[SDLK_KP8]) input.pad[iJoyNum] |= INPUT_SMASH_UP;
                if (keystate[SDLK_KP7]) input.pad[iJoyNum] |= INPUT_SMASH_UP_LEFT;
                if (keystate[SDLK_KP6]) input.pad[iJoyNum] |= INPUT_SMASH_RIGHT;
                if (keystate[SDLK_KP5]) input.pad[iJoyNum] |= INPUT_SMASH_CENTER;
                if (keystate[SDLK_KP4]) input.pad[iJoyNum] |= INPUT_SMASH_LEFT;
                if (keystate[SDLK_KP3]) input.pad[iJoyNum] |= INPUT_SMASH_DOWN_RIGHT;
                if (keystate[SDLK_KP2]) input.pad[iJoyNum] |= INPUT_SMASH_DOWN;
                if (keystate[SDLK_KP1]) input.pad[iJoyNum] |= INPUT_SMASH_DOWN_LEFT;
                break;
            case DEVICE_ACTIVATOR:
                if (keystate[SDLK_g]) input.pad[iJoyNum] |= INPUT_ACTIVATOR_7L;
                if (keystate[SDLK_h]) input.pad[iJoyNum] |= INPUT_ACTIVATOR_7U;
                if (keystate[SDLK_j]) input.pad[iJoyNum] |= INPUT_ACTIVATOR_8L;
                if (keystate[SDLK_k]) input.pad[iJoyNum] |= INPUT_ACTIVATOR_8U;
                break;
            default:
                if (keystate[iConfig.keys[EMU_INPUT_A]])     input.pad[iJoyNum] |= INPUT_A;
                if (keystate[iConfig.keys[EMU_INPUT_B]])     input.pad[iJoyNum] |= INPUT_B;
                if (keystate[iConfig.keys[EMU_INPUT_C]])     input.pad[iJoyNum] |= INPUT_C;
                if (keystate[iConfig.keys[EMU_INPUT_START]]) input.pad[iJoyNum] |= INPUT_START;
                if (keystate[iConfig.keys[EMU_INPUT_X]])     input.pad[iJoyNum] |= INPUT_X;
                if (keystate[iConfig.keys[EMU_INPUT_Y]])     input.pad[iJoyNum] |= INPUT_Y;
                if (keystate[iConfig.keys[EMU_INPUT_Z]])     input.pad[iJoyNum] |= INPUT_Z;
                if (keystate[iConfig.keys[EMU_INPUT_LEFT]])     input.pad[iJoyNum] |= INPUT_LEFT;
                if (keystate[iConfig.keys[EMU_INPUT_UP]])     input.pad[iJoyNum] |= INPUT_UP;
                if (keystate[iConfig.keys[EMU_INPUT_DOWN]])     input.pad[iJoyNum] |= INPUT_DOWN;
                if (keystate[iConfig.keys[EMU_INPUT_RIGHT]])     input.pad[iJoyNum] |= INPUT_RIGHT;
                break;
        }
        return 1;
    }



// =========================================================================
// INIT EMULATOR INPUT & OUTPUT SUB-SYSTEMS
// =========================================================================

#ifdef SYMBIAN_DIRECT_AUDIO
int Emu::InitAudio()
{

    // DirectAudio_Init returns 0 on KErrNone success
    int err = DirectAudio_Init(SOUND_FREQUENCY, 2); 
    if (err != 0) {
        PRINT_ERRMSG("DirectAudio_Init failed with Symbian error code: %d", err);
        iUseSound = false;
        return 0;
    }

    int vol = ((iConfig.audioVolume/10.0) * 256);
    DirectAudio_SetVolume(vol);
    return 1;
}

#else

int Emu::InitAudio() 
{
        SDL_AudioSpec as_desired;
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            PRINT_ERRMSG("SDL Audio initialization failed: %s", SDL_GetError());
            return 0;
        }

	as_desired.freq     = SOUND_FREQUENCY;
        as_desired.format   = AUDIO_S16SYS;
        as_desired.channels = 2;
        as_desired.samples  = SOUND_SAMPLES_SIZE;
        as_desired.callback = AudioCallbackWrapper;
        as_desired.userdata = this;

        if (SDL_OpenAudio(&as_desired, NULL) == -1) {
            PRINT_ERRMSG("SDL_OpenAudio failed: %s", SDL_GetError());
            return 0;
        }

        iSdlSound.current_emulated_samples = 0;
        int n = SOUND_SAMPLES_SIZE * 2 * sizeof(short) * 20;
        iSdlSound.buffer = (char*)malloc(n);
        if (!iSdlSound.buffer) {
            PRINT_ERRMSG("Failed to allocate memory buffer for audio tracking streams (%d bytes requested)", n);
            return 0;
        }

	SDL_PauseAudio(0);
        memset(iSdlSound.buffer, 0, n);
        iSdlSound.current_pos = iSdlSound.buffer;
	ApplyConfig(); // set audio volume.
        return 1;
    }
#endif

void Emu::InitSysBitmap() {

	ClearEmuScreen();
        if (!iSdlVideo.surf_bitmap)
	{
	    iSdlVideo.surf_bitmap = SDL_CreateRGBSurface(SDL_SWSURFACE, SMS_SCREEN_WIDTH, SMS_SCREEN_HEIGHT, 16, 0, 0, 0, 0);
	}

        if (!iSdlVideo.surf_bitmap) {
            PRINT_ERRMSG("SDL_CreateRGBSurface error: %s", SDL_GetError());
            return;
        }

	if (gDirectScreenBlit)
	{
	    blitter_x = (iEmuScreenWidth - iSysScreenWidth) >> 1;
	    blitter_y = (iEmuScreenHeight - iSysScreenHeight) >> 1;
	    bitmap.width  = iEmuScreenWidth;
	    bitmap.height = iEmuScreenHeight;
	}
	else
	{
	    blitter_x = 0;
	    blitter_y = 0;
	    bitmap.width  = SMS_SCREEN_WIDTH;
	    bitmap.height = SMS_SCREEN_HEIGHT;
	}

	blitter_w = iSysScreenWidth;
	blitter_h = iSysScreenHeight;


	/*blitter_w = SDL_max(SMS_SCREEN_WIDTH, iSysScreenWidth);
	blitter_h = SDL_max(SMS_SCREEN_HEIGHT, iSysScreenHeight);
	*/

	PRINT_MSG("BLITTER RECT %d, %d %d,%d", blitter_x, blitter_y, blitter_w, blitter_h);
	

#if defined(USE_8BPP_RENDERING)
        bitmap.pitch  = (bitmap.width * 1);
#elif defined(USE_15BPP_RENDERING) || defined(USE_16BPP_RENDERING)
        bitmap.pitch  = (bitmap.width * 2);
#elif defined(USE_32BPP_RENDERING)
        bitmap.pitch  = (bitmap.width * 4);
#endif
	if (gDirectScreenBlit)
	{
	    SDL_LockSurface(iSdlVideo.surf_screen);
	    bitmap.data = (unsigned char*)iSdlVideo.surf_screen->pixels;
	    SDL_UnlockSurface(iSdlVideo.surf_screen);
	}
	else
	{	   
	    SDL_LockSurface(iSdlVideo.surf_bitmap);
	    bitmap.data = (unsigned char*)iSdlVideo.surf_bitmap->pixels;
	    SDL_UnlockSurface(iSdlVideo.surf_bitmap);
	}

        bitmap.viewport.changed = 3;
	ConfigureVideoBlitRect();
    }

void Emu::ConfigureVideoBlitRect()
    {
	iSdlVideo.drect.x = 0;
	iSdlVideo.drect.y = 0;
	iSdlVideo.drect.w = 0;
	iSdlVideo.drect.h = 0;

	iSdlVideo.srect.x = 0;
	iSdlVideo.srect.y = 0;
	iSdlVideo.srect.w = iSysScreenWidth;
	iSdlVideo.srect.h = iSysScreenHeight;

	if (iFullscreen && iSdlVideo.surf_screen)
	{
	    CalculateAspectRatioRect(iSysScreenWidth, iSysScreenHeight, iEmuScreenWidth, iEmuScreenHeight, &iSdlVideo.drect);
	}


	//PRINT_MSG("BLIT_DEST_RECT %d,%d %d, %d", iSdlVideo.drect.x, iSdlVideo.drect.y, iSdlVideo.drect.w, iSdlVideo.drect.h);

	//PRINT_MSG("BLIT_SRC_RECT %d,%d %d, %d", iSdlVideo.srect.x, iSdlVideo.srect.y, iSdlVideo.srect.w, iSdlVideo.srect.h);
    }


int Emu::InitVideo() {

        if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
            PRINT_ERRMSG("SDL Video subsystem initialization failed: %s", SDL_GetError());
            return 0;
        }
 #ifdef __SYMBIAN32__
	SDL_Rect** modes = SDL_ListModes(NULL, SDL_FULLSCREEN);
	if(modes == NULL)
	{
	    PRINT_ERRMSG("No available video modes");
	    return 0;
	}

	iEmuScreenWidth = modes[0]->w;
	iEmuScreenHeight = modes[0]->h;
#else 
	iEmuScreenWidth = VIDEO_WIDTH;
	iEmuScreenHeight = VIDEO_HEIGHT;
	//iEmuScreenWidth = VIDEO_HEIGHT;
	//iEmuScreenHeight = VIDEO_WIDTH;
#endif
	iSdlVideo.surf_screen = SDL_SetVideoMode(iEmuScreenWidth, iEmuScreenHeight, 16, SDL_SWSURFACE);
        if (!iSdlVideo.surf_screen) {
            PRINT_ERRMSG("SDL_SetVideoMode setup configuration failed: %s", SDL_GetError());
            return 0;
        }


	PRINT_MSG("SCREEN_SIZE %dx%d\n", iEmuScreenWidth, iEmuScreenHeight);

        iSdlVideo.frames_rendered = 0;
        SDL_ShowCursor(SDL_DISABLE);
        return 1;
    }

int Emu::InitSync() {
        if (SDL_InitSubSystem(SDL_INIT_TIMER) < 0) {
            PRINT_ERRMSG("SDL Timing subsystem failed to start up: %s", SDL_GetError());
            return 0;
        }
        iSdlSync.sem_sync = SDL_CreateSemaphore(0);
        if (iSdlSync.sem_sync)
	{
            SDL_AddTimer(vdp_pal ? 60 : 50, reinterpret_cast<SDL_NewTimerCallback>(TimerCallbackWrapper), this);
	    SetTimerState(SDL_ENABLE);
        }

        iSdlSync.ticks = 0;
        return 1;
    }



// =========================================================================
// EXTERNAL DATA
// =========================================================================


#ifdef ENABLE_SEGACD

void Emu::LoadSegaCdBackupRam() 
{
        if (system_hw != SYSTEM_MCD) return;
        FILE* fp = fopen("./scd.brm", "rb");
        if (fp != NULL) { fread(scd.bram, 0x2000, 1, fp); fclose(fp); }

        if (memcmp(scd.bram + 0x2000 - 0x20, K_BRM_FORMAT + 0x20, 0x20)) {
            memset(scd.bram, 0x00, 0x200);
            scd.bram[0x10] = scd.bram[0x12] = scd.bram[0x14] = scd.bram[0x16] = 0x00;
            scd.bram[0x11] = scd.bram[0x13] = scd.bram[0x15] = scd.bram[0x17] = (sizeof(scd.bram) / 64) - 3;
            memcpy(scd.bram + 0x2000 - 0x40, K_BRM_FORMAT, 0x40);
        }

        if (scd.cartridge.id) {
            fp = fopen("./cart.brm", "rb");
            if (fp != NULL) { fread(scd.cartridge.area, scd.cartridge.mask + 1, 1, fp); fclose(fp); }
            if (memcmp(scd.cartridge.area + scd.cartridge.mask + 1 - 0x20, K_BRM_FORMAT + 0x20, 0x20)) {
                memset(scd.cartridge.area, 0x00, scd.cartridge.mask + 1);
                scd.cartridge.area[0x10] = scd.cartridge.area[0x12] = scd.cartridge.area[0x14] = scd.cartridge.area[0x16] = (((scd.cartridge.mask + 1) / 64) - 3) >> 8;
                scd.cartridge.area[0x11] = scd.cartridge.area[0x13] = scd.cartridge.area[0x15] = scd.cartridge.area[0x17] = (((scd.cartridge.mask + 1) / 64) - 3) & 0xff;
                memcpy(scd.cartridge.area + scd.cartridge.mask + 1 - sizeof(K_BRM_FORMAT), K_BRM_FORMAT, sizeof(K_BRM_FORMAT));
            }
        }
    }

void Emu::SaveSegaCdBackupRam() 
{
        if (system_hw != SYSTEM_MCD) return;
        if (!memcmp(scd.bram + 0x2000 - 0x20, K_BRM_FORMAT + 0x20, 0x20)) {
            FILE* fp = fopen("./scd.brm", "wb");
            if (fp != NULL) { fwrite(scd.bram, 0x2000, 1, fp); fclose(fp); }
        }
        if (scd.cartridge.id && !memcmp(scd.cartridge.area + scd.cartridge.mask + 1 - 0x20, K_BRM_FORMAT + 0x20, 0x20)) {
            FILE* fp = fopen("./cart.brm", "wb");
            if (fp != NULL) { fwrite(scd.cartridge.area, scd.cartridge.mask + 1, 1, fp); fclose(fp); }
        }
    }
#endif


bool Emu::ReadConfig()
    {

	memset(&iConfig, 0, sizeof(EmuConfig));
	
	iConfig.audioEnabled = 1;
	iConfig.audioVolume = 5;
	iConfig.fullscreen = 0;

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
	if (memcmp(version,CONFIG_VERSION, 10))
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


void Emu::SaveConfig()
    {
	memcpy(iConfig.version,CONFIG_VERSION, 10);
	FILE *f = fopen("cfg.bin", "wb");
	if (f)
	{
	    /* dump config to the file */
	    fwrite(&iConfig, sizeof(EmuConfig), 1, f);
	    fclose(f);
	}
    }

void Emu::ApplyConfig()
    {
	iUseSound = iConfig.audioEnabled;
	if (iFullscreen != iConfig.fullscreen)
	{
	    iFullscreen = iConfig.fullscreen;
#ifndef USE_ESDL
	    gDirectScreenBlit = !iFullscreen;
#endif
	    InitSysBitmap();
	}

#ifdef __SYMBIAN32__
	int vol = ((iConfig.audioVolume/10.0) * 256);

#ifdef SYMBIAN_DIRECT_AUDIO
	DirectAudio_SetVolume(vol);
#else
	EPOC_SetAudioVolume(vol);
#endif
#endif // __SYMBIAN32__
    }


// =========================================================================
// STATE MANAGEMENT
// =========================================================================


void Emu::InitStateManager()
    {

	    
	memset(iStatePath, 0, sizeof(iStatePath));
	char sep = '/';
#ifdef __SYMBIAN32__
	sep = '\\';
#endif
	
	sprintf(iStatePath, "%s%csav", rompath, sep);
    
	mkdir(iStatePath, 0777);
	
	if ( iStateManager ) delete iStateManager;

	char* romFilePath = (char*) iRomPathBuf;
	memset(iRomName, 0, sizeof(iRomName));
	ExtractBaseFilename(romFilePath, iRomName, sizeof(iRomName));


	//PRINT_MSG("STATEPATH %s", iStatePath);
	//PRINT_MSG("ROMNAME %s", iRomName);

	iStateManager = new StateManager(iStatePath, iRomName);

    }




char* Emu::GetStateFilePath()
    {
	memset(iStateFilePath, 0, sizeof(iStateFilePath));
/*
	char sep = '/';
#ifdef __SYMBIAN32__
	sep = '\\',
#endif
	sprintf(iStateFilePath, "%s%c%s", iStatePath, sep, iRomName);
*/
	return (char*)iStateFilePath;
    }


SDL_Surface*  Emu::PrepareStateBitmap()
    {
	SDL_Surface* surface = gDirectScreenBlit ? iSdlVideo.surf_screen : iSdlVideo.surf_bitmap;


	SDL_PixelFormat* fmt = surface->format;
	SDL_Surface* bm = SDL_CreateRGBSurface(
		surface->flags,
		iSysScreenWidth,
		iSysScreenHeight,
		fmt->BitsPerPixel,
		fmt->Rmask,
		fmt->Gmask,
		fmt->Bmask,
		fmt->Amask);

	SDL_Rect srect = {blitter_x,blitter_y, iSysScreenWidth, iSysScreenHeight};
	SDL_Rect drect = {0,0, bm->w, bm->h};
	SDL_BlitSurface(surface, &srect, bm, &drect);
	return bm;
    }


void Emu::SaveStateBMP(char* fp, SDL_Surface* bmp)
    {
	int status;
	if ( !fp ) return;
	strncat(fp, ".bmp", PATH_MAX);

	if ( !bmp ) bmp = PrepareStateBitmap();

	SDL_LockSurface(bmp);
	status = SDL_SaveBMP(bmp, fp);
	if(status != 0)
	{	
	    PRINT_ERRMSG("failed to save bmp %s", SDL_GetError());
	}

	SDL_UnlockSurface(bmp);	
	SDL_FreeSurface(bmp);
    }


void Emu::AutoSaveState()
    {


	if (!iStateManager) return;
	char* fp = GetStateFilePath();
	if (!iStateManager->getAutoSaveFilename(fp, PATH_MAX))
	{
	    return;
	}

	FILE *f = fopen(fp, "wb");
	uint8 buf[STATE_SIZE];
	if (f)
	{
	    int len = state_save(buf);
	    fwrite(&buf, len, 1, f);
	    fclose(f);
	    SaveStateBMP(fp);
	}
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

	    //PRINT_MSG("SlotCount %u", slotNum);
	    if ( slotNum < 1 ) return;

	    iStateManager->getSlotFilename(slotNum, fp, PATH_MAX);
	}


	FILE *f = fopen(fp, "rb");
	uint8 buf[STATE_SIZE];
	if (f)
	{
	    fread(&buf, STATE_SIZE, 1, f);
	    state_load(buf);
	    fclose(f);
	}
}



void Emu::SaveState(SDL_Surface* bmp)
    {

	Uint32 slotNum = ShowSavedStates();
	if ( !slotNum ) return;

	char* fp = GetStateFilePath();

	if (!iStateManager->getSlotFilename(slotNum, fp, PATH_MAX)) return; 


	DrawOverlayText("saving state ...", 0xFFF00);
	FILE *f = fopen(fp, "wb");
	uint8 buf[STATE_SIZE];
	if (f)
	{
	    int len = state_save(buf);
	    fwrite(&buf, len, 1, f);
	    fclose(f);
	    iStateManager->notifySlotSaved(slotNum);
	    SaveStateBMP(fp, bmp);
	}
}



void Emu::LoadState()
    {

	Uint32 slotNum = ShowSavedStates();
	if ( !slotNum ) return;

	char* fp = GetStateFilePath();
	if (!fp) return;

	if (!iStateManager->getSlotFilename(slotNum, fp, PATH_MAX)) return; 


	DrawOverlayText("loading state ...", 0xFFF00);

	FILE *f = fopen(fp, "rb");
	uint8 buf[STATE_SIZE];
	if (f)
	{
	    fread(&buf, STATE_SIZE, 1, f);
	    state_load(buf);
	    fclose(f);
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



void Emu::DrawMenu(SdlListbox* menu) {
	SDL_Surface* screen = iSdlVideo.surf_screen;
        SDL_FillRect(screen, NULL, 0x0000);
        /*if (iBMPFont) {
            menu->RenderBMP(screen, iBMPFont);
        }*/
	
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

	SDL_Surface* dest = iSdlVideo.surf_screen;
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



void Emu::DrawViewPager(SdlViewPager* p) 
    {

	SDL_Surface* screen = iSdlVideo.surf_screen;
        SDL_FillRect(screen, NULL, 0x0000);
        /*if (iBMPFont) {
            p->RenderBMP(screen, iBMPFont);
        }*/
	
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

	SDL_Surface* screen = iSdlVideo.surf_screen;
        SDL_FillRect(screen, NULL, 0x0000);
        /*if (iBMPFont) {
            box->RenderBMP(iBMPFont);
        }*/
	
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
	SdlMessageBox* box = new SdlMessageBox(iSdlVideo.surf_screen);
	box->SetTheme(msgBoxTheme);
	box->SetTitle(title);
	box->SetConfirmText(""); 
	box->AddLine(message);
#ifdef HAVE_STB_TRUETYPE
	box->ShowSTB(iSTBFontBuffer, 15);
#endif
    }

void Emu::ClearEmuScreen()
    {
	SDL_FillRect(iSdlVideo.surf_screen, NULL, 0x0000);
	SDL_Flip(iSdlVideo.surf_screen);
    }


Uint32 Emu::ShowSavedStates()
    {

	char* fp = GetStateFilePath();

	Uint32 maxSlots = iStateManager->getSlotCount() + 1;
	Uint32 slotNum = SDL_max(1, iStateManager->getLastSavedSlot());

	char titleBuf[MAX_UI_STR_LEN]={0};
 	SdlViewPager* pager = new SdlViewPager(iSdlVideo.surf_screen, iFontSize);
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
	    if ( stateThumbnail )
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
	
            if ( event.type == SDL_KEYDOWN )
	    {
		switch(event.key.keysym.sym)
		{
		    case SDLK_LEFT:
		    {
			if ( slotNum > 1) slotNum--;
			else {slotNum = maxSlots;};
		    }
			break;
		    case SDLK_RIGHT:
		    {
			if (slotNum < maxSlots) slotNum++;
			else {slotNum = 1;}
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
	size_t bufLen = sizeof(titleBuf)-1;
	const int keymapCount = 13; 
	const char* buttonNames[keymapCount] = {
	    "up",
	    "down",
	    "left",
	    "right",
	    "button A",
	    "button B",
	    "button C",
	    "button START",
	    "button X",
	    "button Y",
	    "button Z",
	    "save state",
	    "load state"
	};
	
	SdlMessageBox* box = new SdlMessageBox(iSdlVideo.surf_screen);
	box->SetTheme(msgBoxTheme);

        SDL_Event event;
	SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL); // disable
        while (keymapIndex < keymapCount) 
	{

	    box->Clear();
	    box->SetConfirmText("< press [back] to close >"); 
	    if (keymapIndex == keymapCount)
	    {
		box->AddLine("key mappings saved!");
	    }
	    else
	    {
		snprintf(titleBuf, bufLen, "press [%s] key", buttonNames[keymapIndex]);
		box->AddLine(titleBuf);
		box->AddLine("");
		SDLKey currentBindKey = (SDLKey)(iConfig.keys[keymapIndex]);
		snprintf(titleBuf, bufLen, "current key [%s]", SDL_GetKeyName(currentBindKey));
		box->AddLine(titleBuf);
		box->AddLine("");
	    }

	    DrawMessageBox(box);
	    SDL_WaitEvent(&event);
	    if (HandleResizeEvent(&event))
	    {
		continue;
	    }
	
            if (event.type != SDL_KEYDOWN ) continue;
	    SDLKey key = event.key.keysym.sym;
#ifdef __SYMBIAN32__
	    if ( key == SDLK_HOME) continue;
#endif
	    if ( key == SDLK_ESCAPE) break;
	    iConfig.keys[keymapIndex] = key;
	    keymapIndex++;
        }
	
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL); // enable
	delete box;
    }



void Emu::DrawOutputSettings(SdlListbox* listbox, int itemIndex)
    {
	listbox->Clear();

	char buffer[100] = {0,};  
	sprintf(buffer, "Audio: %s", iConfig.audioEnabled ? "on" : "off");
	listbox->AddItem(buffer);

	sprintf(buffer,"Audio volume: %d%%", iConfig.audioVolume);
	listbox->AddItem(buffer);
    
	sprintf(buffer, "Full screen: %s", iConfig.fullscreen ? "on" : "off");
	listbox->AddItem(buffer);
    
	sprintf(buffer, "Skip frames: %d", iConfig.skipFrames);
	listbox->AddItem(buffer);

	sprintf(buffer, "Scanlines filter: %s", iConfig.scanlines ? "on" : "off");
	listbox->AddItem(buffer);
	listbox->SetSelectedIndex(itemIndex);
	DrawMenu(listbox);
    }



bool Emu::UpdateOutputSettings(SdlListbox* listbox, SDL_Event& event, bool* configChanged)
    {
    

	listbox->HandleInput(event);
	int activeRow = listbox->GetSelectedIndex();
    
	SDLKey key = event.key.keysym.sym;
  
	bool redrawNeeded = (key == SDLK_UP || key == SDLK_DOWN);

	if (key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_RETURN ) 
    
	{
	    *configChanged = true;
	    redrawNeeded = true;
	    switch (activeRow) {
            
		case 0: // Toggle Audio State
		    {
			iConfig.audioEnabled = !iConfig.audioEnabled;
		    }

		    break;
            
		case 1: 
		    {
			int v = iConfig.audioVolume + ((key == SDLK_RIGHT) ? 1:-1 );
		
			if ( v < 0 ) v = 0;
			if ( v > 10 ) v = 10;
			iConfig.audioVolume = v;
		
		    }
		    break;		
		case 2:
		    {
		    iConfig.fullscreen = !iConfig.fullscreen;
		    }
                break;
            case 3:
		{
		    int n = iConfig.skipFrames + ((key == SDLK_RIGHT) ? 1:-1 );
		    if ( n < 0 || n > 60 ) n = 0;
		    iConfig.skipFrames = n;
		}
                break;
            case 4:
                iConfig.scanlines = !iConfig.scanlines;
                break;
	    }
	}

	if (redrawNeeded) DrawOutputSettings(listbox, activeRow);

	return ( key != SDLK_ESCAPE); // close the listbox
    }


void Emu::ShowOutputSettings()
    {
	SdlListbox* listbox = new SdlListbox(iSdlVideo.surf_screen, LAYOUT_FULLSCREEN, iFontSize);
	listbox->SetTheme(darkTheme);       
	DrawOutputSettings(listbox,0);	
	bool configChanged = false;
        SDL_Event event;
        while (SDL_WaitEvent(&event)) 
	{

	
	    if (HandleResizeEvent(&event))
	    {
		continue;
	    }
	
	    if (event.type != SDL_KEYDOWN) continue;
	    if (!UpdateOutputSettings(listbox, event, &configChanged)) break;
	
	}


	if (configChanged) ApplyConfig(); 
	delete listbox;
    }


bool Emu::ShowOptionsMenu() 
    {
	SDL_Surface* stateBmp = PrepareStateBitmap();
	//SaveStateBMP(NULL, stateBmp);

	SdlListbox* options = new SdlListbox(iSdlVideo.surf_screen, LAYOUT_CENTER_MODAL, iFontSize);
	options->SetTheme(darkTheme);
	options->AddItem("1 - Load ROM");
	options->AddItem("2 - Reset");
	options->AddItem("3 - Save state");
	options->AddItem("4 - Load state");
	options->AddItem("5 - Input settings");
	options->AddItem("6 - Output settings");
	options->AddItem("7 - Exit");

        SDL_Event event;
	bool ret = true;

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
	    if ( key == SDLK_ESCAPE) break;
 	    if ( key != SDLK_RETURN) continue;

	    int action = options->GetSelectedIndex();

	    options->Clear();
	    delete options;

	    if (action ==  OptionMenuAction::LOAD_ROM) LoadROM(NULL);
	    else if (action == OptionMenuAction::RESET_EMU) system_reset();	    
	    else if (action == OptionMenuAction::SAVE_STATE) SaveState(stateBmp);	    
	    else if (action == OptionMenuAction::LOAD_STATE) LoadState();	    
	    else if (action == OptionMenuAction::INPUT_SETTINGS) ShowInputSettings(); 
	    else if (action == OptionMenuAction::OUTPUT_SETTINGS) ShowOutputSettings();         
	    else if (action == OptionMenuAction::EXIT_EMU ) ret = false;
	    break;
        }

	ClearEmuScreen();
        return ret;
    }

void Emu::ShowAboutMenu()
    {
	SdlListbox* menu = new SdlListbox(iSdlVideo.surf_screen, LAYOUT_FULLSCREEN, 28);
	menu->SetTheme(darkTheme);
	menu->AddItem("GenesisPlusGX for Symbian");
	menu->AddItem("Version: 1.0 (2026)");
	menu->AddItem("Developer: JigokuMaster");
	menu->AddItem("Testers: ACER7, Dante");
	menu->AddItem("Emulator Core: Eke-Eke");
	menu->AddItem("Emulator Icon: ACER7");
	menu->AddItem("Emulator Font: Style-7");

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

	    if ( key == SDLK_ESCAPE) break;
 	    
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
	    return strncpy(iRomPathBuf, (char*)fpPtr.PtrZ(), sizeof(iRomPathBuf));
	}
#else
        if (iRomsPath) {
            snprintf(iRomPathBuf, sizeof(iRomPathBuf)-1, "%s/%s", iRomsPath, romFileName);
            return iRomPathBuf;
        }
#endif
        return NULL;

    }


bool Emu::LoadROM(char* romFilePath, bool reset) {
        if (!romFilePath) {
            romFilePath =(char*)ShowROMList();
        }

        if (romFilePath) {
            if (!load_rom(romFilePath)) {
                PRINT_ERRMSG("failed to load ROM");
            }
	    else
	    {

		if (reset){
		    system_init();
		    system_reset();
		    audio_reset();
		}

		switch ( system_hw )
		{
		    case SYSTEM_SMS:
		    case SYSTEM_SMS2:
			iSysScreenWidth = SMS_SCREEN_WIDTH;
			iSysScreenHeight = SMS_SCREEN_HEIGHT;
			break;
		    case SYSTEM_GG:
			iSysScreenWidth = GG_SCREEN_WIDTH;
			iSysScreenHeight = GG_SCREEN_HEIGHT;
			break;
		    case SYSTEM_MD:
			iSysScreenWidth = MD_SCREEN_WIDTH;
			iSysScreenHeight = MD_SCREEN_HEIGHT;
		    default:
			break;
		}

		iFrameMS = 1000 / (vdp_pal ? 50 : 60);
		PRINT_MSG("vdp_pal = %d, FRAME_MS %u", vdp_pal, iFrameMS);

		InitStateManager();
		InitSysBitmap();
                return true;
            }
        }
        return false;
    }


const char* Emu::ShowROMList()
{
    SdlListbox* listbox = new SdlListbox(iSdlVideo.surf_screen, LAYOUT_FULLSCREEN, iFontSize);
    listbox->SetTheme(darkTheme);
    listbox->Clear();
#ifdef __SYMBIAN32__
    PopulateRomList(listbox, _L("C:"ROMS_PATH_PREFIX));
    PopulateRomList(listbox, _L("E:"ROMS_PATH_PREFIX));
    PopulateRomList(listbox, _L("F:"ROMS_PATH_PREFIX));
#else
    iRomsPath = (char*)"ROMs";
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

bool Emu::ShowMainMenu() {

	SdlListbox* menu = new SdlListbox(iSdlVideo.surf_screen, LAYOUT_CENTER_MODAL, 28);
	menu->SetTheme(darkTheme);
	menu->AddItem("1 - Load ROM");
	menu->AddItem("2 - Input settings");
	menu->AddItem("3 - Output settings");
	menu->AddItem("4 - About");
	menu->AddItem("5 - Exit");
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
	    if (!ok ) continue;
	    int action = menu->GetSelectedIndex();
	    if (action ==  MainMenuAction::LOAD_ROM)
	    {
		if (LoadROM(NULL,false))
		    break;
	    }

	    else if (action ==  MainMenuAction::INPUT_SETTINGS)
		ShowInputSettings();
	    else if (action ==  MainMenuAction::OUTPUT_SETTINGS)
		ShowOutputSettings();


	    else if (action ==  MainMenuAction::ABOUT_EMU)
		ShowAboutMenu();

	    else if (action ==  MainMenuAction::EXIT_EMU)
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



#ifdef SYMBIAN_DIRECT_AUDIO
void Emu::CloseAudio()
{
    DirectAudio_Quit();
}

#else
void Emu::CloseAudio() {
        SDL_PauseAudio(1);
        SDL_CloseAudio();
        if (iSdlSound.buffer) { free(iSdlSound.buffer); iSdlSound.buffer = NULL; }
    }
#endif 

void Emu::CloseVideo() {
        if (iSdlVideo.surf_bitmap) 
	{
	    SDL_FreeSurface(iSdlVideo.surf_bitmap);
	    iSdlVideo.surf_bitmap = NULL; 
	}
    }



void Emu::CloseSync() {

    if (iSdlSync.sem_sync) {
	SDL_DestroySemaphore(iSdlSync.sem_sync);
	iSdlSync.sem_sync = NULL;
    }
}

void Emu::Shutdown() {
	SaveConfig();
        audio_shutdown();
        error_shutdown();
        CloseVideo();
        CloseAudio();
        CloseSync();
        FreeMenuFont();
        SDL_Quit();
	if (iStateManager) delete iStateManager;

    }



// =========================================================================
// EVENT HANDLING
// =========================================================================


bool Emu::HandleResizeEvent(SDL_Event* event)
{
	if (event->type != SDL_VIDEORESIZE) return false;

	PRINT_MSG("SCREEN MODE CHANGED");
	iEmuScreenWidth = event->resize.w;
	iEmuScreenHeight = event->resize.h;

	PRINT_MSG("SCREEN_SIZE %dx%d", iEmuScreenWidth, iEmuScreenHeight);
    
        iSdlVideo.surf_screen = SDL_SetVideoMode(iEmuScreenWidth, iEmuScreenHeight, 16, SDL_SWSURFACE); 
        if (!iSdlVideo.surf_screen) {
            PRINT_ERRMSG("SDL_SetVideoMode failed: %s", SDL_GetError());

            iRunning = false;
            return false;
        }

	InitSysBitmap();
	return true;
}



void Emu::HandleKeyEvent(SDLKey key) 
{
	if ( key == iConfig.keys[EMU_INPUT_SAVESTATE])
	{

	
	    SetTimerState(SDL_DISABLE);
	    DrawOverlayText("saving state ...", 0xFFF00);
	    AutoSaveState();
	    ClearEmuScreen(),
	    SetTimerState(SDL_ENABLE);
	}

	else if ( key == iConfig.keys[EMU_INPUT_LOADSTATE]){
	    SetTimerState(SDL_DISABLE);
	    DrawOverlayText("loading state ...", 0xFFF00);
	    AutoLoadState();
	    ClearEmuScreen();
	    SetTimerState(SDL_ENABLE);
	}
#ifdef __SYMBIAN32__
	else if ( key == SDLK_HOME ) 
	{
	    static bool mousemode = false;
	    mousemode = !mousemode;
	    SDL_ShowCursor(mousemode);
	}
#endif
	else if ( key == SDLK_ESCAPE ) 
	{
	    SetTimerState(SDL_DISABLE);
            iRunning = ShowOptionsMenu();
	    SetTimerState(SDL_ENABLE);
        }
}




inline bool Emu::DoFrameSync()
{
    iNextFrameTicks += iFrameMS;

    Uint32 current_ticks = SDL_GetTicks();

    // 1. Throttle timing via SDL_Delay + spin-wait
    if (current_ticks < iNextFrameTicks) {
        Uint32 remaining = iNextFrameTicks - current_ticks;

        if (remaining > 2) {
            SDL_Delay(remaining - 2); 
        }

        while (SDL_GetTicks() < iNextFrameTicks) {
            // Spin loop
        }
    } 
    else if (current_ticks - iNextFrameTicks > 100) {
        // Reset timing baseline if emulation stalled heavily
        iNextFrameTicks = current_ticks;
    }

    if (iConfig.skipFrames > 0) {
        if (iFrameSkipCount < iConfig.skipFrames) {
            iFrameSkipCount++;
            return true; // Skip rendering on the next frame
        }
        
        // Rendered frame reached -> reset counter
        iFrameSkipCount = 0;
    }

    return false; // Do NOT skip rendering on the next frame
}



int Emu::Run(int argc, char** argv){
#ifdef __SYMBIAN32__

        const char* logFile = "D:\\GenesisPlusGX.log";
        int fd = open(logFile, O_WRONLY | O_CREAT | O_TRUNC);

	dup2(fd, STDOUT_FILENO); 
	dup2(fd, STDERR_FILENO);
	//close(fd);
	char* privDir = getenv("EPOC_PRIVATE_DIR");
	if (privDir) chdir(privDir);
	PRINT_MSG("EPOC_PRIVATE_DIR %s", privDir);
	mkdir("C:"ROMS_PATH_PREFIX, 0755); 
	mkdir("E:"ROMS_PATH_PREFIX, 0755);
	mkdir("F:"ROMS_PATH_PREFIX, 0755);

#endif

        error_init();
        set_config_defaults();
        system_bios = 0;
	FILE* fp;

#ifdef ENABLE_SEGACD
        // Load Optional Genesis Master Boot ROM System
        memset(boot_rom, 0xFF, 0x800);
        fp = fopen(MD_BIOS, "rb");
        if (fp != NULL) {
            fread(boot_rom, 1, 0x800, fp);
            fclose(fp);

            if (!memcmp((char*)(boot_rom + 0x120), "GENESIS OS", 10)) {
                system_bios = SYSTEM_MD;
            }
            for (int i = 0; i < 0x800; i += 2) {
                uint8 temp = boot_rom[i];
                boot_rom[i] = boot_rom[i + 1];
                boot_rom[i + 1] = temp;
            }
        }
#endif

        if (SDL_Init(0) < 0) {
            PRINT_ERRMSG("SDL initialization failed: %s", SDL_GetError());
            return 1;
        }
	
        if (!InitVideo()) return 1;
	
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	ReadConfig();

        if (!InitMenuFont()) {
            PRINT_ERRMSG("failed to load font");
            return 1;
        }

        if (argc > 1) 
	{
	    if (!LoadROM(argv[1], false))
	    {
		Shutdown();
		return 1;
	    }
        }

	else
	{
	    if (!ShowMainMenu()) 
	    {
		Shutdown();
		return 1;
	    }
	}


	InitAudio();
        if (iUseSemSync) 
	{
	    if (!InitSync()) return 1;
	}
        
        audio_init(SOUND_FREQUENCY, 0);
        system_init();

#ifdef ENABLE_SEGACD

        LoadSegaCdBackupRam();
#endif
        if (sram.on) {
            fp = fopen("./game.srm", "rb");
            if (fp != NULL) {
                fread(sram.sram, 0x10000, 1, fp);
                fclose(fp);
            }
        }

        system_reset();
        iRunning = true;
	SDL_Event event;
	iNextFrameTicks = SDL_GetTicks();
        while (iRunning)
	{

           if (SDL_PollEvent(&event))
	   {
	       
		HandleResizeEvent(&event);
                switch (event.type) {
#ifndef __SYMBIAN32__
		    
                    case SDL_USEREVENT: {
                        char caption[10] = {0,};
                        /*sprintf(caption, "Genesis Plus GX - %d fps - %s", event.user.code, 
                                (rominfo.international[0] != 0x20) ? rominfo.international : rominfo.domestic);
                        SDL_WM_SetCaption(caption, NULL);
			*/
                        if ( !iFullscreen )
			{
			    sprintf(caption, "FPS %d", event.user.code);
			    DrawOverlayText(caption, 0xFFF00);
			}
                        break;
                    }
#endif
                    case SDL_QUIT:
                        iRunning = false;
                        break;

                    case SDL_KEYDOWN:
                        HandleKeyEvent(event.key.keysym.sym);
                        break;
                }
            }



	    UpdateInputDevice();
	    UpdateVideoFrame();
            if ( iUseSound ) UpdateAudioStream();	
	    if (!iUseSemSync) DoFrameSync();
	    else if (iUseSemSync && iSdlSync.sem_sync && iSdlVideo.frames_rendered % 3 == 0)
	    {
                SDL_SemWait(iSdlSync.sem_sync);
            }

        }


#ifdef ENABLE_SEGACD
        SaveSegaCdBackupRam();
#endif
        if (sram.on) {
            fp = fopen("./game.srm", "wb");
            if (fp != NULL) {
                fwrite(sram.sram, 0x10000, 1, fp);
                fclose(fp);
            }
        }

        Shutdown();
        return 0;
    }


Emu* gEmu;

int sdl_input_update()
{
    if (gEmu) return gEmu->UpdateInputDevice();
    return 1;
}

int main(int argc, char** argv) {
    gEmu = new Emu();
    int ret = gEmu->Run(argc, argv);
    delete gEmu;
    return ret;
}

