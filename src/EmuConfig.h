
#ifndef EMUCONFIG_H
#define EMUCONFIG_H

#define EMU_VERSION "1.2 (2026)"
#define EMU_NUM_BUTTONS 13


enum EmuInputKey {
    EMU_INPUT_UP = 0,
    EMU_INPUT_DOWN,
    EMU_INPUT_LEFT,
    EMU_INPUT_RIGHT,
    EMU_INPUT_A,
    EMU_INPUT_B,
    EMU_INPUT_C,
    EMU_INPUT_START,
    EMU_INPUT_X,
    EMU_INPUT_Y,
    EMU_INPUT_Z,
    EMU_INPUT_SAVESTATE,
    EMU_INPUT_LOADSTATE
};



enum EmuAudioStream {
    SDL_AUDIOSTREAM = 0,
    AUDIOSTREAM_CUSTOMQUEUE,
    AUDIOSTREAM_SYSQUEUE,
};

enum EmuVideoFilter {
    SCANLINES_FILTER = 1,
    BILINEAR_FILTER,
};


#define CONFIG_VERSION "GPGX_CFG1"
struct EmuConfig {
    char version[10];
    bool audioEnabled;
    int audioVolume; // Range: 0 to 10
 #ifdef __SYMBIAN32__ 
    int audioStream;
#endif
    int audioFreq;
    bool fullscreen;
    int skipFrames;
    int keys[28];
    int filter;
    unsigned int delay;
};

#endif // EMUCONFIG_H
