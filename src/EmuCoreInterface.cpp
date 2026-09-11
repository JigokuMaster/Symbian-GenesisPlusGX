#include "EmuCoreInterface.h"
#include "EmuConfig.h"

extern "C" {
#include "shared.h"
#include "sms_ntsc.h"
#include "md_ntsc.h"
}

#include <stdio.h>


#define SMS_SCREEN_WIDTH  256
#define SMS_SCREEN_HEIGHT 192
#define GG_SCREEN_WIDTH 160
#define GG_SCREEN_HEIGHT 144
#define MD_SCREEN_WIDTH  256
#define MD_SCREEN_HEIGHT 224


/* =============================================================================
 * Implementation of INIT/SHUTDOWN/LOAD Functions
 * ========================================================================== */

md_ntsc_t *md_ntsc;
sms_ntsc_t *sms_ntsc;

void emucore_init(int p1) 
{
    set_config_defaults();
    system_bios = 0;
    FILE* fp = NULL;

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

    
    audio_init(p1/*audio frequency*/, 0);
    system_init();
    if (sram.on) {
        fp = fopen("./game.srm", "rb");
        if (fp) {
            fread(sram.sram, 0x10000, 1, fp);
            fclose(fp);
        }
    }

    if (fp) fclose(fp);
    system_reset();

}

void emucore_shutdown()
{
    audio_shutdown();
}
void emucore_reset()
{
    system_reset();
}

bool emucore_load_rom(const char* fp)
{
    static bool firstload = true;
    int ok = load_rom((char*)fp);
    if ( ok && !firstload) {
	system_init();
	//system_reset();
	audio_reset();
    }

    if (firstload) firstload = false;
    return ok;
}


/* =============================================================================
* Implementation of VIDEO/FRAME Functions
 * ========================================================================== */

void emucore_update_frame(int p1 /*skip frame*/)
{

#ifdef  ENABLE_SEGACD
    if (system_hw == SYSTEM_MCD) {
        system_frame_scd(p1);
    } 
    else if ((system_hw & SYSTEM_PBC) == SYSTEM_MD) {
	system_frame_gen(p1);
    } else 
#endif
    {
	system_frame_sms(p1);
    }

    /*if (bitmap.viewport.changed & 1)
    {
	bitmap.viewport.changed &= ~1;
	printf("screen rect %dx%d %dx%d\n",
	    bitmap.viewport.x,
	    bitmap.viewport.y,
	    bitmap.viewport.w,
	    bitmap.viewport.h);
    
	return true;
    }
    return false;*/
}


void emucore_init_video(unsigned char* data, int width, int height, int depth, int pitch)
{

    /* 
     * setup bitmap rect for the current hw system.
     * setup the dimensions for our custom blitter.
     * */
    switch ( system_hw )
    {
	case SYSTEM_SG:
	case SYSTEM_SGII:
	case SYSTEM_SGII_RAM_EXT:
	case SYSTEM_MARKIII:
	case SYSTEM_SMS:
	case SYSTEM_SMS2:
	    width = SMS_SCREEN_WIDTH;
	    height = SMS_SCREEN_HEIGHT;
	    break;
	case SYSTEM_GG:	
	case SYSTEM_GGMS:
	    width = GG_SCREEN_WIDTH;
	    height = GG_SCREEN_HEIGHT;
	    break;

	case SYSTEM_PICO:
	case SYSTEM_PBC:
	case SYSTEM_MCD:
	case SYSTEM_MD:
	    width = MD_SCREEN_WIDTH;
	    height = MD_SCREEN_HEIGHT;
	    break;
	default:
	    break;
    }

    bitmap.width  = width;
    bitmap.height = height;
    bitmap.pitch  = pitch;
    bitmap.data   = data;
    bitmap.viewport.changed = 3;
    blitter_x = 0;
    blitter_y = 0;  
    blitter_w = width;
    blitter_h = height;
}


void emucore_get_videorect(EMUVIDEO_RECT* rect)
{
    // when the custom blitter is used
    /**/
    rect->x = blitter_x;
    rect->y = blitter_y;
    rect->w = blitter_w;
    rect->h = blitter_h;

    /**/
    //  when the custom blitter is not used
    /*
    rect->x = bitmap.viewport.x;
    rect->y = bitmap.viewport.y;
    rect->w = bitmap.viewport.w;
    rect->h = bitmap.viewport.h;
    */
}

// set video blit rect area
void emucore_set_videorect(EMUVIDEO_RECT* rect)
{
    // setup these only when the custom blitter is used to draw on the screen directly
    blitter_x = rect->x;
    blitter_y = rect->y;
    blitter_w = rect->w;
    blitter_h = rect->h;
}


/* =============================================================================
* Implementation of AUDIO Functions
 * ========================================================================== */


bool emucore_reinit_audio(int p1) 
{
    return audio_init(p1, 0) == 0;
}

void emucore_cleanup_audio() 
{

}


int emucore_fetch_audio(unsigned char* data)
{
    return audio_update((int16*)data);
}

int emucore_audio_numsamples(int frequency)
{
    return ( frequency / 60) + 1;
}

/* =============================================================================
* Implementation of INPUT Functions
 * ========================================================================== */



const char* const emucore_button_names[] =
{
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

void emucore_update_input(int buttons[], int k, int p) 
{
    const int joynum = 0;
    input.pad[joynum] = 0;
    
    if ( !buttons) return;

    uint8* keystate = SDL_GetKeyState(NULL);
    switch (input.dev[joynum]) 
    {
            case DEVICE_LIGHTGUN: 
	    {
                
		int videoWidth, videoHeight, x, y; 

		videoWidth = SDL_GetVideoSurface()->w;

		videoHeight = SDL_GetVideoSurface()->h;
		int state = SDL_GetMouseState(&x, &y);

                input.analog[joynum][0] = x - (videoWidth - bitmap.viewport.w) / 2;
                input.analog[joynum][1] = y - (videoHeight - bitmap.viewport.h) / 2;
                if (state & SDL_BUTTON_LMASK) input.pad[joynum] |= INPUT_A;
                if (state & SDL_BUTTON_RMASK) input.pad[joynum] |= INPUT_B;
                if (state & SDL_BUTTON_MMASK) input.pad[joynum] |= INPUT_C;
		if (keystate[buttons[EMU_INPUT_START]]) input.pad[joynum] |= INPUT_START;
                break;
            }

            case DEVICE_PADDLE:
	    case DEVICE_SPORTSPAD:
            case DEVICE_MOUSE:			  
            case DEVICE_XE_1AP:
            case DEVICE_PICO:
	    case DEVICE_TEREBI:
	    case DEVICE_GRAPHIC_BOARD:
		break;
 
            default:
                if (keystate[buttons[EMU_INPUT_A]]) input.pad[joynum] |= INPUT_A;

                if (keystate[buttons[EMU_INPUT_B]]) input.pad[joynum] |= INPUT_B;
   

		if (keystate[buttons[EMU_INPUT_C]]) input.pad[joynum] |= INPUT_C;
		

                if (keystate[buttons[EMU_INPUT_START]]) input.pad[joynum] |= INPUT_START;

                if (keystate[buttons[EMU_INPUT_X]]) input.pad[joynum] |= INPUT_X;
                
		if (keystate[buttons[EMU_INPUT_Y]]) input.pad[joynum] |= INPUT_Y;
                
		if (keystate[buttons[EMU_INPUT_Z]]) input.pad[joynum] |= INPUT_Z;
                
		if (keystate[buttons[EMU_INPUT_LEFT]]) input.pad[joynum] |= INPUT_LEFT;

                
		if (keystate[buttons[EMU_INPUT_RIGHT]]) input.pad[joynum] |= INPUT_RIGHT;

		if (keystate[buttons[EMU_INPUT_UP]]) input.pad[joynum] |= INPUT_UP;


		if (keystate[buttons[EMU_INPUT_DOWN]]) input.pad[joynum] |= INPUT_DOWN;
		break;
        }
}




/* =============================================================================
* Implementation of STATE Functions
 * ========================================================================== */



static uint8 stateBuf[STATE_SIZE];
bool emucore_save_state(const char* fp)
{
    bool ok;
    FILE *f = fopen(fp, "wb");
    ok = f != NULL;
    if (ok)
    {
	int len = state_save(stateBuf);
	ok = fwrite(&stateBuf, 1, len, f) == len;
	fclose(f);
    }
    return ok;
}

bool emucore_load_state(const char* fp)
{
    bool ok;
    FILE *f = fopen(fp, "rb");
    ok = f != NULL;
    if (ok)
    {
	ok = fread(&stateBuf,1, STATE_SIZE, f) >= 0;
	if (ok) state_load(stateBuf);
	fclose(f);
    }
    return ok;
}









