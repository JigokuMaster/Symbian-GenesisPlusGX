#ifndef EMUCORE_INTERFACE_H
#define EMUCORE_INTERFACE_H


#define EMUCORE_NAME "GenesisPlusGX"
#define EMUCORE_AUTHOR "Eke-Eke"



#include <SDL.h> // for SDL_Rect
#define EMUVIDEO_RECT SDL_Rect
#define EMUCORE_FRAMERATE 60//(vdp_pal ? 50 : 60)


#ifdef __cplusplus
extern "C" {
#endif


// INIT/SHUTDOWN/LOAD Functions
void emucore_init(int p1);
void emucore_shutdown();
void emucore_reset();
bool emucore_load_rom(const char* fp);

// VIDEO Functions 
void emucore_update_frame(int p1);
void emucore_init_video(unsigned char* data, int width, int height, int depth, int pitch);
void emucore_get_videorect(EMUVIDEO_RECT* rect);
void emucore_set_videorect(EMUVIDEO_RECT* rect);
int emucore_get_framerate();

// AUDIO Functions
bool emucore_reinit_audio(int p1);
void emucore_cleanup_audio();
int emucore_fetch_audio(unsigned char* data);
int emucore_audio_numsamples(int frequency);

// INPUT Functions
extern const char* const emucore_button_names[];
void emucore_update_input(int buttons[], int k, int p);

// STATE Functions
bool emucore_save_state(const char* fp);
bool emucore_load_state(const char* fp);

#ifdef __cplusplus
}
#endif

#endif /* EMUCORE_INTERFACE_H */
