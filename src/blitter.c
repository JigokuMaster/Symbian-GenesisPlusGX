#include "shared.h"

int blitter_x = 0;
int blitter_y = 0;
int blitter_w = 0;
int blitter_h = 0;

#ifdef CUSTOM_BLITTER

#include "sms_ntsc.h"
#include "md_ntsc.h"

typedef unsigned short sms_ntsc_out_t;
typedef unsigned short md_ntsc_out_t;

void sms_ntsc_blit( sms_ntsc_t const* ntsc, SMS_NTSC_IN_T const* table, unsigned char* input,
                    int in_width, int vline)
{
  int n;
  int const chunk_count = in_width / sms_ntsc_in_chunk;

  /* handle extra 0, 1, or 2 pixels by placing them at beginning of row */
  int const in_extra = in_width - chunk_count * sms_ntsc_in_chunk;
  unsigned const extra2 = (unsigned) -(in_extra >> 1 & 1); /* (unsigned) -1 = ~0 */
  unsigned const extra1 = (unsigned) -(in_extra & 1) | extra2;

  /* use palette entry 0 for unused pixels */
  SMS_NTSC_IN_T border = table[0];

  SMS_NTSC_BEGIN_ROW( ntsc, border,
      (SMS_NTSC_ADJ_IN( table[input[0]] )) & extra2,
      (SMS_NTSC_ADJ_IN( table[input[extra2 & 1]] )) & extra1 );

  sms_ntsc_out_t* line_out  = (sms_ntsc_out_t*)(&bitmap.data[(vline * bitmap.pitch)]);

  input += in_extra;

  for ( n = chunk_count; n; --n )
  {
    /* order of input and output pixels must not be altered */
    SMS_NTSC_COLOR_IN( 0, ntsc, SMS_NTSC_ADJ_IN( table[*input++] ) );
    SMS_NTSC_RGB_OUT( 0, *line_out++ );
    SMS_NTSC_RGB_OUT( 1, *line_out++ );
    
    SMS_NTSC_COLOR_IN( 1, ntsc, SMS_NTSC_ADJ_IN( table[*input++] ) );
    SMS_NTSC_RGB_OUT( 2, *line_out++ );
    SMS_NTSC_RGB_OUT( 3, *line_out++ );
      
    SMS_NTSC_COLOR_IN( 2, ntsc, SMS_NTSC_ADJ_IN( table[*input++] ) );
    SMS_NTSC_RGB_OUT( 4, *line_out++ );
    SMS_NTSC_RGB_OUT( 5, *line_out++ );
    SMS_NTSC_RGB_OUT( 6, *line_out++ );
  }

  /* finish final pixels */
  SMS_NTSC_COLOR_IN( 0, ntsc, border );
  SMS_NTSC_RGB_OUT( 0, *line_out++ );
  SMS_NTSC_RGB_OUT( 1, *line_out++ );

  SMS_NTSC_COLOR_IN( 1, ntsc, border );
  SMS_NTSC_RGB_OUT( 2, *line_out++ );
  SMS_NTSC_RGB_OUT( 3, *line_out++ );

  SMS_NTSC_COLOR_IN( 2, ntsc, border );
  SMS_NTSC_RGB_OUT( 4, *line_out++ );
  SMS_NTSC_RGB_OUT( 5, *line_out++ );
  SMS_NTSC_RGB_OUT( 6, *line_out++ );
}

void md_ntsc_blit( md_ntsc_t const* ntsc, MD_NTSC_IN_T const* table, unsigned char* input,
                   int in_width, int vline)
{
  int const chunk_count = in_width / md_ntsc_in_chunk - 1;

  /* use palette entry 0 for unused pixels */
  MD_NTSC_IN_T border = table[0];

  MD_NTSC_BEGIN_ROW( ntsc, border,
        MD_NTSC_ADJ_IN( table[*input++] ),
        MD_NTSC_ADJ_IN( table[*input++] ),
        MD_NTSC_ADJ_IN( table[*input++] ) );

  md_ntsc_out_t* restrict line_out  = (md_ntsc_out_t*)(&bitmap.data[(vline * bitmap.pitch)]);

  int n;

  for ( n = chunk_count; n; --n )
  {
    /* order of input and output pixels must not be altered */
    MD_NTSC_COLOR_IN( 0, ntsc, MD_NTSC_ADJ_IN( table[*input++] ) );
    MD_NTSC_RGB_OUT( 0, *line_out++ );
    MD_NTSC_RGB_OUT( 1, *line_out++ );

    MD_NTSC_COLOR_IN( 1, ntsc, MD_NTSC_ADJ_IN( table[*input++] ) );
    MD_NTSC_RGB_OUT( 2, *line_out++ );
    MD_NTSC_RGB_OUT( 3, *line_out++ );

    MD_NTSC_COLOR_IN( 2, ntsc, MD_NTSC_ADJ_IN( table[*input++] ) );
    MD_NTSC_RGB_OUT( 4, *line_out++ );
    MD_NTSC_RGB_OUT( 5, *line_out++ );

    MD_NTSC_COLOR_IN( 3, ntsc, MD_NTSC_ADJ_IN( table[*input++] ) );
    MD_NTSC_RGB_OUT( 6, *line_out++ );
    MD_NTSC_RGB_OUT( 7, *line_out++ );
  }

  /* finish final pixels */
  MD_NTSC_COLOR_IN( 0, ntsc, MD_NTSC_ADJ_IN( table[*input++] ) );
  MD_NTSC_RGB_OUT( 0, *line_out++ );
  MD_NTSC_RGB_OUT( 1, *line_out++ );

  MD_NTSC_COLOR_IN( 1, ntsc, border );
  MD_NTSC_RGB_OUT( 2, *line_out++ );
  MD_NTSC_RGB_OUT( 3, *line_out++ );

  MD_NTSC_COLOR_IN( 2, ntsc, border );
  MD_NTSC_RGB_OUT( 4, *line_out++ );
  MD_NTSC_RGB_OUT( 5, *line_out++ );

  MD_NTSC_COLOR_IN( 3, ntsc, border );
  MD_NTSC_RGB_OUT( 6, *line_out++ );
  MD_NTSC_RGB_OUT( 7, *line_out++ );
}
#endif
