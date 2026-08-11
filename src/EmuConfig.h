
#ifndef EMUCONFIG_H
#define EMUCONFIG_H

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
    EMU_INPUT_LOADSTATE,
    EMU_INPUT_TAKESS
};


#define CONFIG_VERSION "GPGX_CFG1"
struct EmuConfig {
    char version[10];
    bool audioEnabled;
    int audioVolume; // Range: 0 to 10
    bool fullscreen;
    int skipFrames;
    int keys[28];  
};

/*struct EmuConfig {
    char version[10];
    bool audioEnabled;
    int audioVolume;       // Range: 0 to 10
    int up_btn;
    int down_btn;
    int left_btn;
    int right_btn;
    int a_btn;
    int b_btn;
    int c_btn;
    int start_btn;
    int x_btn;
    int y_btn;
int z_btn;
    int ss_btn;
    int ls_btn;
    bool fullscreen;
};
*/
#endif // EMUCONFIG_H
