/****************************************************************************/
/*                                                                          */
/*  The FreeType project -- a free and portable quality TrueType renderer.  */
/*                                                                          */
/*  Copyright (C) 1996-2025 by                                              */
/*  D. Turner, R.Wilhelm, and W. Lemberg                                    */
/*                                                                          */
/*                                                                          */
/*  FTMulti - a simple variable font viewer                                 */
/*                                                                          */
/*  Press ? when running this program to have a list of key-bindings        */
/*                                                                          */
/****************************************************************************/

#include <ft2build.h>
#include <freetype/freetype.h>

#include <freetype/ftdriver.h>
#include <freetype/ftfntfmt.h>
#include <freetype/ftmm.h>
#include <freetype/ftmodapi.h>

#include "common.h"
#include "mlgetopt.h"
#include "strbuf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "graph.h"
#include "grfont.h"

#define  DIM_X   640
#define  DIM_Y   480

#define  HEADER_HEIGHT  12

#define  MAXPTSIZE    500               /* dtp */
#define  MAX_MM_AXES   16


  static char         Header[256];
  static const char*  new_header = NULL;

  static const char*  Text =
    "The quick brown fox jumps over the lazy dog 0123456789 "
    "\342\352\356\373\364\344\353\357\366\374\377\340\371\351\350\347 "
    "&#~\"\'(-`_^@)=+\260 ABCDEFGHIJKLMNOPQRSTUVWXYZ "
    "$\243^\250*\265\371%!\247:/;.,?<>";

  static FT_Library    library;      /* the FreeType library        */
  static FT_Face       face;         /* the font face               */
  static FT_Size       size;         /* the font size               */
  static FT_GlyphSlot  glyph;        /* the glyph slot              */

  static unsigned long  encoding = ULONG_MAX;

  static FT_Error      error;        /* error returned by FreeType? */

  static grSurface*  surface;        /* current display surface     */
  static grBitmap*   bit;            /* current display bitmap      */
  static grColor     fore_color;     /* foreground on black back    */
  static grColor     step_color;     /* step color                  */

  static unsigned short  width  = DIM_X;     /* window width        */
  static unsigned short  height = DIM_Y;     /* window height       */

  static int  ptsize;                /* current point size          */
  static int  res       = 72;        /* resolution, dpi             */

  static int  hinted    = 1;         /* is glyph hinting active?    */
  static int  autohint  = 0;         /* is auto hinting enforced?   */
  static int  grouping  = 1;         /* is axis grouping active?    */
  static int  antialias = 1;         /* is anti-aliasing active?    */
  static int  fillrule  = 0x0;       /* flip fill flags or not?     */
  static int  overlaps  = 0x0;       /* flip overlap flags or not?  */

  static int  num_glyphs;            /* number of glyphs            */
  static int  Num;                   /* current first glyph index   */
  static int  Fail;                  /* number of failed glyphs     */

  static int  render_mode = 1;

  static FT_MM_Var    *multimaster   = NULL;
  static FT_Fixed      design_pos   [MAX_MM_AXES];
  static FT_Fixed      requested_pos[MAX_MM_AXES];
  static unsigned int  requested_cnt =  0;
  static unsigned int  used_num_axis =  0;
  static FT_Long       frac          = 40;  /* axes resolution */

  /*
   * We use the following arrays to support both the display of all axes and
   * the grouping of axes.  If grouping is active, hidden axes that have the
   * same tag as a non-hidden axis are not displayed; instead, they receive
   * the same axis value as the non-hidden one.
   */
  static unsigned int  hidden[MAX_MM_AXES];
  static unsigned int  shown_axes[MAX_MM_AXES];  /* array of axis indices */
  static unsigned int  num_shown_axes;

  static FT_UInt  num_named;
  static FT_UInt  idx_named;


#define DEBUGxxx

#ifdef DEBUG
#define LOG( x )  LogMessage x
#else
#define LOG( x )  /* empty */
#endif


#ifdef DEBUG
  static void
  LogMessage( const char*  fmt,
              ... )
  {
    va_list  ap;


    va_start( ap, fmt );
    vfprintf( stderr, fmt, ap );
    va_end( ap );
  }
#endif


  /* PanicZ */
  static void
  PanicZ( const char*  message )
  {
    fprintf( stderr, "%s\n  error = 0x%04x\n", message, error );
    exit( 1 );
  }


  static unsigned long
  make_tag( char  *s )
  {
    int            i;
    unsigned long  l = ULONG_MAX;


    for ( i = 0; i < 4 && s[i]; i++ )
      l = ( l << 8 ) | (unsigned char)s[i];

    /* interpret numerically if too short for a tag */
    if ( i < 4 && !sscanf( s, "%lu", &l ) )
      l = ULONG_MAX;

    return l;
  }


  static int
  get_last_char( void )
  {
    FT_ULong  nxt, max, mid, min = 0;
    FT_UInt   gidx;


    switch ( face->charmap->encoding )
    {
    case FT_ENCODING_ADOBE_LATIN_1:
    case FT_ENCODING_ADOBE_STANDARD:
    case FT_ENCODING_ADOBE_EXPERT:
    case FT_ENCODING_ADOBE_CUSTOM:
    case FT_ENCODING_APPLE_ROMAN:
      return 0xFF;

    case FT_ENCODING_UNICODE:
      max = 0x110000;
      break;

    /* some fonts use range 0x00-0x100, others have 0xF000-0xF0FF */
    case FT_ENCODING_MS_SYMBOL:
    default:
      max = 0x10000;
      break;
    }

    /* binary search for the last charcode */
    do
    {
      mid = ( min + max ) >> 1;
      nxt = FT_Get_Next_Char( face, mid, &gidx );

      if ( gidx )
        min = nxt;
      else
      {
        max = mid;

        /* once moved, it helps to advance min through sparse regions */
        if ( min )
        {
          nxt = FT_Get_Next_Char( face, min, &gidx );

          if ( gidx )
            min = nxt;
          else
            max = min;  /* found it */
        }
      }
    } while ( max > min );

    return (int)max;
  }


  static void
  parse_design_coords( char  *s )
  {
    for ( requested_cnt = 0;
          requested_cnt < MAX_MM_AXES && *s;
          requested_cnt++ )
    {
      requested_pos[requested_cnt] = (FT_Fixed)( strtod( s, &s ) * 65536.0 );

      if ( *s == ',' )
        ++s;
    }
  }


  static unsigned int
  set_up_axes( void )
  {
    unsigned int i, j, idx;


    for ( i = 0; i < used_num_axis; i++ )
    {
      unsigned int  flags;


      (void)FT_Get_Var_Axis_Flags( multimaster, i, &flags );
      hidden[i] = flags & FT_VAR_AXIS_FLAG_HIDDEN;
    }

    if ( grouping )
    {
      /*
       * `ftmulti' is a diagnostic tool that should also be able to
       * handle pathological situations; for this reason the looping
       * code below is a bit more complicated in comparison to normal
       * applications.
       *
       * In particular, the loop handles the following cases gracefully,
       * avoiding grouping.
       *
       * . multiple non-hidden axes that have the same tag
       *
       * . multiple hidden axes that have the same tag without a
       *   corresponding non-hidden axis.
       */

      for ( idx = 0, i = 0; i < used_num_axis; i++ )
      {
        int            do_skip = 0;
        unsigned long  tag = multimaster->axis[i].tag;


        if ( hidden[i] )
        {
          /* if axis is hidden, check whether an already assigned */
          /* non-hidden axis has the same tag; if yes, skip it    */
          for ( j = 0; j < idx; j++ )
            if ( !hidden[shown_axes[j]]                      &&
                 multimaster->axis[shown_axes[j]].tag == tag )
            {
              do_skip = 1;
              break;
            }
        }
        else
        {
          /* otherwise, check whether we have already assigned this axis */
          for ( j = 0; j < idx; j++ )
            if ( shown_axes[j] == i )
            {
              do_skip = 1;
              break;
            }
        }
        if ( do_skip )
          continue;

        /* we have a new axis to display */
        shown_axes[idx] = i;

        /* if axis is hidden, use a non-hidden axis */
        /* with the same tag instead if available   */
        if ( hidden[i] )
        {
          for ( j = i + 1; j < used_num_axis; j++ )
            if ( !hidden[j]                      &&
                 multimaster->axis[j].tag == tag )
              shown_axes[idx] = j;
        }

        idx++;
      }
    }
    else
    {
      /* show all axes */
      for ( idx = 0; idx < used_num_axis; idx++ )
        shown_axes[idx] = idx;
    }

    return idx;
  }


  /* Clear `bit' bitmap/pixmap */
  static void
  Clear_Display( void )
  {
    /* fast black background */
    memset( bit->buffer, 0, (size_t)bit->rows *
                            (size_t)( bit->pitch < 0 ? -bit->pitch
                                                     : bit->pitch ) );
  }


  /* Initialize the display bitmap named `bit' */
  static void
  Init_Display( void )
  {
    grBitmap  bitmap = { (int)height, (int)width, 0,
                         gr_pixel_mode_none, 256, NULL };


    grInitDevices();

    surface = grNewSurface( 0, &bitmap );
    if ( !surface )
      PanicZ( "could not allocate display surface\n" );

    bit = (grBitmap*)surface;

    fore_color = grFindColor( bit, 255, 255, 255, 255 );  /* white */
    step_color = grFindColor( bit,   0, 100,   0, 255 );  /* green */

    grSetTitle( surface, "FreeType Variations Viewer - press ? for help" );
  }


  /* Render a single glyph with the `grays' component */
  static FT_Error
  Render_Glyph( int  x_offset,
                int  y_offset )
  {
    grBitmap  bit3;
    FT_Pos    x_top, y_top;


    /* first, render the glyph image into a bitmap */
    if ( glyph->format != FT_GLYPH_FORMAT_BITMAP )
    {
      /* overlap flags mitigate AA rendering artifacts in overlaps */
      /* by oversampling; even-odd fill rule reveals the overlaps; */
      /* toggle these flag to test the effects                     */
      glyph->outline.flags ^= overlaps | fillrule;

      error = FT_Render_Glyph( glyph, antialias ? FT_RENDER_MODE_NORMAL
                                                : FT_RENDER_MODE_MONO );
      if ( error )
        return error;
    }

    /* now blit it to our display screen */
    bit3.rows   = (int)glyph->bitmap.rows;
    bit3.width  = (int)glyph->bitmap.width;
    bit3.pitch  = glyph->bitmap.pitch;
    bit3.buffer = glyph->bitmap.buffer;

    switch ( glyph->bitmap.pixel_mode )
    {
    case FT_PIXEL_MODE_MONO:
      bit3.mode  = gr_pixel_mode_mono;
      bit3.grays = 0;
      break;

    case FT_PIXEL_MODE_GRAY:
      bit3.mode  = gr_pixel_mode_gray;
      bit3.grays = glyph->bitmap.num_grays;
    }

    /* Then, blit the image to the target surface */
    x_top = x_offset + glyph->bitmap_left;
    y_top = y_offset - glyph->bitmap_top;

    grBlitGlyphToSurface( surface, &bit3,
                          x_top, y_top, fore_color );

    return 0;
  }


  static FT_Error
  Reset_Scale( int  pointSize )
  {
    return FT_Set_Char_Size( face,
                             pointSize << 6, pointSize << 6,
                             (FT_UInt)res, (FT_UInt)res );
  }


  static FT_Error
  LoadChar( unsigned int  idx,
            int           hint )
  {
    int  flags = FT_LOAD_NO_BITMAP;


    if ( !antialias )
      flags |= FT_LOAD_TARGET_MONO;

    if ( !hint )
      flags |= FT_LOAD_NO_HINTING;

    else if ( autohint )
      flags |= FT_LOAD_FORCE_AUTOHINT;

    return FT_Load_Glyph( face, idx, flags );
  }


  static int
  Render_All( int  first_glyph )
  {
    int  start_x = num_shown_axes ? 18 * 8 : 12;
    int  start_y = size->metrics.y_ppem + HEADER_HEIGHT * 4;
    int  step_y  = size->metrics.height / 64 + 1;
    int  x, y, w, i;

    unsigned int  idx;


    x = start_x;
    y = start_y;

    for ( i = first_glyph; i < num_glyphs; i++ )
    {
      idx = encoding == ULONG_MAX ? (unsigned int)i
                                  : FT_Get_Char_Index( face, (FT_ULong)i );

      error = LoadChar( idx, hinted );

      if ( error )
      {
        Fail++;
        continue;
      }

      w = glyph->metrics.horiAdvance ? glyph->metrics.horiAdvance >> 6
                                     : size->metrics.x_ppem / 2;

      if ( x + w > bit->width - 4 )
      {
        x  = start_x;
        y += step_y;

        if ( y >= bit->rows - size->metrics.y_ppem / 5 )
          break;
      }

      /* half-step for zero-width glyphs */
      if ( glyph->metrics.horiAdvance == 0 )
      {
         w /= 2;
         x += w;
         grFillRect( bit, x, y - w, w, w, step_color );
      }

      Render_Glyph( x, y );
      x += w + 1;
    }

    return i - first_glyph;
  }


  static int
  Render_Text( int  offset )
  {
    int  start_x = num_shown_axes ? 18 * 8 : 12;
    int  start_y = size->metrics.y_ppem + HEADER_HEIGHT * 4;
    int  step_y  = size->metrics.height / 64 + 1;
    int  x, y, w;

    const char*  p = Text;
    int          ch;


    x = start_x;
    y = start_y;

    while ( 1 )
    {
      ch = utf8_next( &p );
      if ( ch == 0 )
      {
        if ( p == Text )
          break;

        /* check for progress in pen position */
        if ( offset < 0 && x + y <= start_x + start_y )
          break;

        p  = Text;
        ch = utf8_next( &p );
      }

      if ( offset-- > 0 )
        continue;

      error = LoadChar( FT_Get_Char_Index( face, (FT_ULong)ch ), hinted );

      if ( error )
      {
        Fail++;
        continue;
      }

      w = ( glyph->metrics.horiAdvance + 48 ) >> 6;
      if ( x + w > bit->width - 4 )
      {
        x  = start_x;
        y += step_y;

        if ( y >= bit->rows - size->metrics.y_ppem / 5 )
          break;
      }

      Render_Glyph( x, y );
      x += w;
    }

    return 0;
  }


  static void
  Help( void )
  {
    char  buf[256];
    char  version[64];

    FT_Int  major, minor, patch;

    grEvent  dummy_event;


    FT_Library_Version( library, &major, &minor, &patch );

    if ( patch )
      snprintf( version, sizeof ( version ),
                "%d.%d.%d", major, minor, patch );
    else
      snprintf( version, sizeof ( version ),
                "%d.%d", major, minor );

    Clear_Display();
    grSetLineHeight( 10 );
    grGotoxy( 0, 0 );
    grSetMargin( 2, 1 );
    grGotobitmap( bit );

    snprintf( buf, sizeof ( buf ),
              "FreeType Glyph Variations Viewer -"
                " part of the FreeType %s test suite",
              version );

    grWriteln( buf );
    grLn();
    grWriteln( "This program displays all glyphs from one or several" );
    grWriteln( "Multiple Masters, GX, or OpenType Variation font files." );
    grLn();
    grWriteln( "Use the following keys:");
    grLn();
    grWriteln( "F1, ?       display this help screen" );
    grWriteln( "q, ESC      quit ftmulti" );
    grLn();
    grWriteln( "F2          toggle axis grouping" );
    grWriteln( "F3          toggle fill rule flags" );
    grWriteln( "F4          toggle overlap flags" );
    grWriteln( "F5          toggle outline hinting" );
    grWriteln( "F6          cycle through hinting engines (incl. auto)" );
    grLn();
    grWriteln( "Tab         toggle anti-aliasing" );
    grWriteln( "Space       toggle rendering mode" );
    grLn();
    grWriteln( ", .         previous/next font" );
    grLn();
    grWriteln( "Up, Down    change pointsize by 1 unit" );
    grWriteln( "PgUp, PgDn  change pointsize by 10 units" );
    grLn();
    grWriteln( "Left, Right adjust index by 1" );
    grWriteln( "F7, F8      adjust index by 16" );
    grWriteln( "F9, F10     adjust index by 256" );
    grWriteln( "F11, F12    adjust index by 4096" );
    grLn();
    grWriteln( "[, ]        previous/next named instance" );
    grLn();
    grWriteln( "a, A        adjust axis 0" );
    grWriteln( "b, B        adjust axis 1" );
    grWriteln( "..." );
    grWriteln( "p, P        adjust axis 16" );
    grLn();
    grWriteln( "-, +        adjust axis range increment" );
    grLn();
    grWriteln( "Axes marked with an asterisk are hidden." );
    grLn();
    grLn();
    grWriteln( "press any key to exit this help screen" );

    grRefreshSurface( surface );
    grListenSurface( surface, gr_event_key, &dummy_event );
  }


  static void
  hinting_engine_change( void )
  {
    const FT_String*  module_name = FT_FACE_DRIVER_NAME( face );
    FT_UInt           prop = 0;


    if ( !FT_Property_Get( library, module_name,
                                    "interpreter-version", &prop ) )
    {
      switch ( prop )
      {
      S1:
      case TT_INTERPRETER_VERSION_35:
        prop = TT_INTERPRETER_VERSION_40;
        if ( !FT_Property_Set( library, module_name,
                                        "interpreter-version", &prop ) )
          break;
        /* fall through */
      case TT_INTERPRETER_VERSION_40:
        autohint = !autohint;
        if ( autohint )
          break;
        prop = TT_INTERPRETER_VERSION_35;
        if ( !FT_Property_Set( library, module_name,
                                        "interpreter-version", &prop ) )
          break;
        goto S1;
      }
    }

    else if ( !FT_Property_Get( library, module_name,
                                         "hinting-engine", &prop ) )
    {
      switch ( prop )
      {
      S2:
      case FT_HINTING_FREETYPE:
        prop = FT_HINTING_ADOBE;
        if ( !FT_Property_Set( library, module_name,
                                        "hinting-engine", &prop ) )
          break;
        /* fall through */
      case FT_HINTING_ADOBE:
        autohint = !autohint;
        if ( autohint )
          break;
        prop = FT_HINTING_FREETYPE;
        if ( !FT_Property_Set( library, module_name,
                                        "hinting-engine", &prop ) )
          break;
        goto S2;
      }
    }
  }


  static int
  Process_Event( void )
  {
    grEvent       event;
    FT_Long       i;
    unsigned int  axis;


    grRefreshSurface( surface );
    grListenSurface( surface, 0, &event );

    if ( event.type == gr_event_resize )
      return 1;

    switch ( event.key )
    {
    case grKeyEsc:            /* ESC or q */
    case grKEY( 'q' ):
      return 0;

    case grKeyF1:
    case grKEY( '?' ):
      Help();
      break;

    case grKEY( ',' ):
    case grKEY( '.' ):
      return (int)event.key;

    /* mode keys */

    case grKeyF2:
      grouping = !grouping;
      new_header = grouping ? "axis grouping is now on"
                            : "axis grouping is now off";
      num_shown_axes = set_up_axes();
      break;

    case grKeyF3:
      fillrule  ^= FT_OUTLINE_EVEN_ODD_FILL;
      new_header = fillrule
                     ? "fill rule flags are flipped"
                     : "fill rule flags are unchanged";
      break;

    case grKeyF4:
      overlaps  ^= FT_OUTLINE_OVERLAP;
      new_header = overlaps
                     ? "overlap flags are flipped"
                     : "overlap flags are unchanged";
      break;

    case grKeyF5:
      hinted     = !hinted;
      break;

    case grKeyF6:
      hinting_engine_change();
      return (int)event.key;

    case grKeyTab:
      antialias  = !antialias;
      new_header = antialias ? "anti-aliasing is now on"
                             : "anti-aliasing is now off";
      break;

    case grKEY( ' ' ):
      render_mode ^= 1;
      new_header   = render_mode ? "rendering all glyphs in font"
                                 : "rendering test text string";
      break;

    /* MM-related keys */

    case grKEY( '+' ):
      if ( frac > 10 )
        frac /= 2;
      break;

    case grKEY( '-' ):
      if ( frac < 80 )
        frac *= 2;
      break;

    case grKEY( '[' ):
      if ( num_named )
      {
        if ( --idx_named == 0 )
          idx_named = num_named;
        FT_Set_Named_Instance( face, idx_named );
        FT_Get_Var_Design_Coordinates( face, used_num_axis, design_pos );
      }
      break;

    case grKEY( ']' ):
      if ( num_named )
      {
        if ( ++idx_named > num_named )
          idx_named = 1;
        FT_Set_Named_Instance( face, idx_named );
        FT_Get_Var_Design_Coordinates( face, used_num_axis, design_pos );
      }
      break;

    case grKEY( 'a' ):
    case grKEY( 'b' ):
    case grKEY( 'c' ):
    case grKEY( 'd' ):
    case grKEY( 'e' ):
    case grKEY( 'f' ):
    case grKEY( 'g' ):
    case grKEY( 'h' ):
    case grKEY( 'i' ):
    case grKEY( 'j' ):
    case grKEY( 'k' ):
    case grKEY( 'l' ):
    case grKEY( 'm' ):
    case grKEY( 'n' ):
    case grKEY( 'o' ):
    case grKEY( 'p' ):
      i = -1;
      axis = event.key - 'a';
      goto Do_Axis;

    case grKEY( 'A' ):
    case grKEY( 'B' ):
    case grKEY( 'C' ):
    case grKEY( 'D' ):
    case grKEY( 'E' ):
    case grKEY( 'F' ):
    case grKEY( 'G' ):
    case grKEY( 'H' ):
    case grKEY( 'I' ):
    case grKEY( 'J' ):
    case grKEY( 'K' ):
    case grKEY( 'L' ):
    case grKEY( 'M' ):
    case grKEY( 'N' ):
    case grKEY( 'O' ):
    case grKEY( 'P' ):
      i = 1;
      axis = event.key - 'A';
      goto Do_Axis;

    /* scaling related keys */

    case grKeyPageUp:
      ptsize += 10;
      goto Do_Scale;

    case grKeyPageDown:
      ptsize -= 10;
      goto Do_Scale;

    case grKeyUp:
      ptsize++;
      goto Do_Scale;

    case grKeyDown:
      ptsize--;
      goto Do_Scale;

    /* glyph index related keys */

    case grKeyLeft:
      Num--;
      goto Do_Glyph;

    case grKeyRight:
      Num++;
      goto Do_Glyph;

    case grKeyF7:
      Num -= 16;
      goto Do_Glyph;

    case grKeyF8:
      Num += 16;
      goto Do_Glyph;

    case grKeyF9:
      Num -= 256;
      goto Do_Glyph;

    case grKeyF10:
      Num += 256;
      goto Do_Glyph;

    case grKeyF11:
      Num -= 4096;
      goto Do_Glyph;

    case grKeyF12:
      Num += 4096;
      goto Do_Glyph;

    default:
      ;
    }
    return 1;

  Do_Axis:
    if ( axis < num_shown_axes )
    {
      FT_Var_Axis*  a;
      FT_Fixed      pos, rng;
      unsigned int  n;


      /* convert to real axis index */
      axis = shown_axes[axis];
      a    = multimaster->axis + axis;

      rng = a->maximum       - a->minimum;
      pos = design_pos[axis] - a->minimum;
      i  += FT_MulDiv( frac, pos, rng );
      pos = a->minimum + FT_MulDiv( rng, i, frac );

      if ( pos < a->minimum )
        pos = a->maximum;
      if ( pos > a->maximum )
        pos = a->minimum;

      /* for MM fonts or large ranges, round the design coordinates      */
      /* otherwise round to two decimal digits to make the PS name short */
      if ( !FT_IS_SFNT( face ) || rng > 0x200000 )
        pos = FT_RoundFix( pos );
      else
      {
        FT_Fixed  x = pos & 0xFFFF;


        x   -= ( ( ( x * 100 + 0x8000 ) & ~0xFFFF ) + 50 ) / 100;
        pos -= x;
      }

      design_pos[axis] = pos;

      if ( grouping )
      {
        /* synchronize hidden axes with visible axis */
        for ( n = 0; n < used_num_axis; n++ )
          if ( hidden[n]                          &&
               multimaster->axis[n].tag == a->tag )
            design_pos[n] = pos;
      }

      FT_Set_Var_Design_Coordinates( face, used_num_axis, design_pos );
    }
    return 1;

  Do_Scale:
    if ( ptsize < 1 )
      ptsize = 1;
    if ( ptsize > MAXPTSIZE )
      ptsize = MAXPTSIZE;
    Reset_Scale( ptsize );
    return 1;

  Do_Glyph:
    if ( Num < 0 )
      Num = 0;
    if ( Num >= num_glyphs )
      Num = num_glyphs - 1;
    return 1;
  }


  static void
  usage( const char*  execname )
  {
    fprintf( stderr,
      "\n"
      "ftmulti: multiple masters font viewer - part of FreeType\n"
      "--------------------------------------------------------\n"
      "\n" );
    fprintf( stderr,
      "Usage: %s [options] [pt] font ...\n"
      "\n",
             execname );
    fprintf( stderr,
      "  pt           The point size for the given resolution.\n"
      "               If resolution is 72dpi, this directly gives the\n"
      "               ppem value (pixels per EM).\n" );
    fprintf( stderr,
      "  font         The font file(s) to display.\n"
      "\n" );
    fprintf( stderr,
      "  -d WxH       Set window dimensions (default: %ux%u).\n",
             DIM_X, DIM_Y );
    fprintf( stderr,
      "  -r R         Use resolution R dpi (default: 72dpi).\n"
      "  -f index     Specify first glyph index to display.\n"
      "  -e encoding  Specify encoding tag (default: no encoding).\n"
      "               Common values: `unic' (Unicode), `symb' (symbol),\n"
      "               `ADOB' (Adobe standard), `ADBC' (Adobe custom),\n"
      "               or a numeric charmap index.\n"
      "  -m text      Use `text' for rendering.\n"
      "  -a axis1,axis2,...\n"
      "               Specify the design coordinates for each\n"
      "               variation axis at start-up.\n"
      "\n"
      "  -v           Show version."
      "\n" );

    exit( 1 );
  }


  int
  main( int    argc,
        char*  argv[] )
  {
    FT_Long  face_index;
    int      orig_ptsize, file;
    int      first_glyph = 0;
    int      option;

    unsigned int  n;
    const char*   execname = ft_basename( argv[0] );


    /* Initialize engine */
    error = FT_Init_FreeType( &library );
    if ( error )
      PanicZ( "Could not initialize FreeType library" );

    while ( 1 )
    {
      option = getopt( argc, argv, "a:d:e:f:m:r:v" );

      if ( option == -1 )
        break;

      switch ( option )
      {
      case 'a':
        parse_design_coords( optarg );
        break;

      case 'd':
        if ( sscanf( optarg, "%hux%hu", &width, &height ) != 2 )
          usage( execname );
        break;

      case 'e':
        encoding = make_tag( optarg );
        break;

      case 'f':
        sscanf( optarg, "%i", &first_glyph );
        break;

      case 'm':
        Text = optarg;
        render_mode = 0;
        break;

      case 'r':
        res = atoi( optarg );
        if ( res < 1 )
          usage( execname );
        break;

      case 'v':
        {
          FT_Int  major, minor, patch;


          FT_Library_Version( library, &major, &minor, &patch );

          printf( "ftmulti (FreeType) %d.%d", major, minor );
          if ( patch )
            printf( ".%d", patch );
          printf( "\n" );
          exit( 0 );
        }
        /* break; */

      default:
        usage( execname );
        break;
      }
    }

    argc -= optind;
    argv += optind;

    if ( argc > 1 && sscanf( argv[0], "%d", &orig_ptsize ) == 1 )
    {
      argc--;
      argv++;
    }
    else
      orig_ptsize = 64;

    if ( argc == 0 )
      usage( execname );

    Init_Display();

    file       = 0;
    face_index = 0;

  NewFace:
    ptsize     = orig_ptsize;
    hinted     = 1;
    num_glyphs = 0;

    /* Load face */
    error = FT_New_Face( library, argv[file], face_index, &face );
    if ( error )
    {
      face = NULL;
      goto Display_Font;
    }

    if ( face_index < 0 )
    {
      face_index = face->num_faces - 1;

      FT_Done_Face( face );
      goto NewFace;
    }

    error = Reset_Scale( ptsize );
    if ( error )
      goto Display_Font;

    if ( encoding != ULONG_MAX )
    {
      if ( encoding < (unsigned long)face->num_charmaps )
        error = FT_Set_Charmap( face, face->charmaps[encoding] );
      else
        error = FT_Select_Charmap( face, (FT_Encoding)encoding );

      if ( error )
        goto Display_Font;

      num_glyphs = get_last_char() + 1;
    }
    else
      num_glyphs = face->num_glyphs;

    glyph       = face->glyph;
    size        = face->size;

    /* retrieve multiple master information */
    FT_Done_MM_Var( library, multimaster );
    error = FT_Get_MM_Var( face, &multimaster );
    if ( error )
    {
      num_named      = 0;
      num_shown_axes = 0;
      multimaster    = NULL;
      goto Display_Font;
    }

    if ( FT_IS_SFNT( face ) )
    {
      num_named = (FT_UInt)( face->style_flags >> 16 );
      FT_Get_Default_Named_Instance( face, &idx_named );
    }
    else
    {
      num_named = 1;
      idx_named = 1;
    }

    /* if the user specified a position, use it, otherwise  */
    /* set the current position to the default of each axis */
    if ( multimaster->num_axis > MAX_MM_AXES )
    {
      fprintf( stderr, "only handling first %u variation axes (of %u)\n",
                       MAX_MM_AXES, multimaster->num_axis );
      used_num_axis = MAX_MM_AXES;
    }
    else
      used_num_axis = multimaster->num_axis;

    num_shown_axes = set_up_axes();

    for ( n = 0; n < used_num_axis; n++ )
    {
      FT_Var_Axis*  a = multimaster->axis + n;


      design_pos[n] = n < requested_cnt ? requested_pos[n]
                                        : a->def;
      if ( design_pos[n] < a->minimum )
        design_pos[n] = a->minimum;
      else if ( design_pos[n] > a->maximum )
        design_pos[n] = a->maximum;

      /* for MM fonts, round the design coordinates to integers */
      if ( !FT_IS_SFNT( face ) )
        design_pos[n] = FT_RoundFix( design_pos[n] );
    }

    error = FT_Set_Var_Design_Coordinates( face, used_num_axis, design_pos );
    if ( error )
      goto Display_Font;

  Display_Font:
    if ( num_glyphs )
    {
      Fail = 0;
      Num  = first_glyph % num_glyphs;

      if ( Num < 0 )
        Num += num_glyphs;
    }

    for ( ;; )
    {
      int     key;
      StrBuf  header[1];


      Clear_Display();

      strbuf_init( header, Header, sizeof ( Header ) );
      strbuf_reset( header );

      if ( num_glyphs )
      {
        int  count;


        switch ( render_mode )
        {
        case 0:
          count = Render_Text( Num );
          break;

        default:
          count = Render_All( Num );
        }

        strbuf_format( header, "%.50s %.50s | %.100s",
                       face->family_name,
                       face->style_name,
                       ft_basename( argv[file] ) );
        if ( face->num_faces > 1 )
          strbuf_format( header, ":%ld", face_index );
        grWriteCellString( bit, 0, 0, Header, fore_color );

        if ( !new_header )
        {
          strbuf_reset( header );
          strbuf_format( header, "PS name: %s",
                         FT_Get_Postscript_Name( face ) );
          new_header = Header;
        }

        grWriteCellString( bit, 0, 2 * HEADER_HEIGHT, new_header, fore_color );
        new_header = NULL;

        if ( num_shown_axes > 0 )
        {
          strbuf_reset( header );
          strbuf_format( header, "axes (\361 1/%ld):", frac );
          grWriteCellString( bit, 0, 4 * HEADER_HEIGHT, Header, fore_color );
        }

        for ( n = 0; n < num_shown_axes; n++ )
        {
          unsigned int  axis = shown_axes[n];


          strbuf_reset( header );
          strbuf_format( header, "%c %.50s%s:",
                         n + 'A',
                         multimaster->axis[axis].name,
                         hidden[axis] ? "*" : "" );
          if ( design_pos[axis] & 0xFFFF )
            strbuf_format( header, "% .2f", design_pos[axis] / 65536.0 );
          else
            strbuf_format( header,   "% d", design_pos[axis] / 65536 );
          grWriteCellString( bit, 0, (int)( n + 5 ) * HEADER_HEIGHT,
                             Header, fore_color );
        }

        {
          const FT_String*  module_name = FT_FACE_DRIVER_NAME( face );
          const char*       hinting_engine = "";
          FT_UInt           prop = 0;


          if ( !FT_IS_SCALABLE( face ) )
            hinting_engine = " bitmap";

          else if ( !hinted )
            hinting_engine = " unhinted";

          else if ( autohint )
            hinting_engine = " auto";

          else if ( !FT_Property_Get( library, module_name,
                                      "interpreter-version", &prop ) )
          {
            switch ( prop )
            {
            case TT_INTERPRETER_VERSION_35:
              hinting_engine = "\372v35";
              break;
            case TT_INTERPRETER_VERSION_40:
              hinting_engine = "\372v40";
              break;
            }
          }

          else if ( !FT_Property_Get( library, module_name,
                                      "hinting-engine", &prop ) )
          {
            switch ( prop )
            {
            case FT_HINTING_FREETYPE:
              hinting_engine = "\372FT";
              break;
            case FT_HINTING_ADOBE:
              hinting_engine = "\372Adobe";
              break;
            }
          }

          strbuf_reset( header );
          strbuf_format( header, "%s%s, size: %d %s",
                         module_name,
                         hinting_engine,
                         ptsize,
                         res == 72 ? "ppem" : "pt" );
          if ( count )
            strbuf_format( header,
                           encoding == ULONG_MAX ? ", glyphs: %d-%d"
                                                 : ", chars: 0x%X-0x%X",
                           Num, Num + count - 1 );
        }
      }
      else
        strbuf_format( header,
                       "%.100s: no glyphs found at %d %s",
                       ft_basename( argv[file] ), ptsize,
                       res == 72 ? "ppem" : "pt" );

      grWriteCellString( bit, 0, HEADER_HEIGHT, Header, fore_color );

      if ( !( key = Process_Event() ) )
        goto End;

      if ( key == grKEY( '.' ) )
      {
        if ( face && face_index < face->num_faces - 1 )
          face_index++;
        else if ( file < argc - 1 )
        {
          face_index = 0;
          file++;
        }

        FT_Done_Face( face );
        goto NewFace;
      }

      if ( key == grKEY( ',' ) )
      {
        if ( face_index > 0 )
          face_index--;
        else if ( file > 0 )
        {
          face_index = -1;
          file--;
        }

        FT_Done_Face( face );
        goto NewFace;
      }

      if ( key == grKeyF6 )
      {
        /* enforce reloading */
        FT_Done_Face( face );
        goto NewFace;
      }
    }

  End:
    grDoneSurface( surface );
    grDoneDevices();

    FT_Done_MM_Var  ( library, multimaster );
    FT_Done_Face    ( face        );
    FT_Done_FreeType( library     );

    printf( "Execution completed successfully.\n" );
    printf( "Fails = %d\n", Fail );
    fflush( stdout );  /* clean mintty pipes */

    exit( 0 );      /* for safety reasons */
    /* return 0; */ /* never reached */
  }


/* End */
