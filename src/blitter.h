
#ifndef _BLITTER_H_
#define _BLITTER_H_

extern int blitter_x;
extern int blitter_y;
extern int blitter_w;
extern int blitter_h;

#if defined(USE_8BPP_RENDERING)
#define BITMAP_BPP  1
#elif defined(USE_15BPP_RENDERING) || defined(USE_16BPP_RENDERING)
#define BITMAP_BPP 2
#elif defined(USE_32BPP_RENDERING)
#define BITMAP_BPP 4
#endif

#define CUSTOM_BLITTER(line, width, pixel, src) \
  PIXEL_OUT_T *dst = ((PIXEL_OUT_T *)&bitmap.data[( (line + blitter_y) * bitmap.pitch) + (blitter_x * BITMAP_BPP)]); \
  do { \
    *dst++ = pixel[*src++]; \
  } while (--width);


#endif
