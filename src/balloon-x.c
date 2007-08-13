#include <config.h>
#include "lisp.h"

#include "device.h"
#include "console-x.h"

#include "balloon_help.h"

/* ### start of hack */

static unsigned long alloc_color( Display* dpy, const char* colorname, int light )
{
  Colormap cmap = DefaultColormap( dpy, DefaultScreen(dpy) );
  unsigned long pixel = 0;
  XColor color;

  if( XParseColor(dpy, cmap, colorname, &color) && XAllocColor(dpy, cmap, &color) )
  {
    pixel = color.pixel;
  }
  else
  {
    if( light )
    {
      printf("Warning: could not allocate color \"%s\", using \"white\"\n", colorname);
      pixel = alloc_color( dpy, "white", True );
    }
    else
    {
      printf("Warning: could not allocate color \"%s\", using \"black\"\n", colorname);
      pixel = alloc_color( dpy, "black", True );
    }
  }
  return pixel;
}

static XFontStruct* open_font( Display* dpy, const char* font_name )
{
  XFontStruct* fontStruct = NULL;

  fontStruct = XLoadQueryFont( dpy, font_name ? font_name : "fixed" );
  if( fontStruct == NULL )
  {
    printf("Warning: could not load font \"%s\", using \"fixed\".\n", font_name);
    fontStruct = XLoadQueryFont( dpy, "fixed" );
    assert( fontStruct != NULL );
  }
  return fontStruct;
}

static void init( void )
{
  static int init;

  if( !init )
  {
    Pixel fg, bg, shine, shadow;
    XFontStruct* font;
    Display *dpy = DEVICE_X_DISPLAY (XDEVICE (Vdefault_x_device));

    fg = alloc_color( dpy, "grey60",  1 );
    bg = alloc_color( dpy, "black", 0 );
    
    shine  = alloc_color( dpy, "grey80", 1 );
    shadow = alloc_color( dpy, "grey40", 0 );

    font = open_font( dpy, "-adobe-helvetica-medium-r-normal--12-*" );

    balloon_help_create( dpy, bg, fg, shine, shadow, font );
    init = 1;
  }
}

/* ### end of hack */

DEFUN( "show-balloon-help", Fshow_balloon_help, 1, 1, 0, /*
Show balloon help.
*/
       (string))
{
  char *p;
  CHECK_STRING (string);

  p = (char *) XSTRING_DATA (string);

  init();

  balloon_help_show( p );

  return Qnil;
}

DEFUN( "hide-balloon-help", Fhide_balloon_help, 0, 0, 0, /*
Hide balloon help.
*/
      ())
{
  init();

  balloon_help_hide();

  return Qnil;
}

DEFUN( "balloon-help-move-to-pointer", Fballoon_help_move_to_pointer, 0, 0, 0, /*
Hide balloon help.
*/
      ())
{
  init();

  balloon_help_move_to_pointer();

  return Qnil;
}



/************************************************************************/
/*				initialization				*/
/************************************************************************/

void
syms_of_balloon_x (void)
{
  DEFSUBR( Fshow_balloon_help );
  DEFSUBR( Fhide_balloon_help );
  DEFSUBR( Fballoon_help_move_to_pointer );
}

void
vars_of_balloon_x (void)
{
  Fprovide (intern ("balloon-help"));
}
