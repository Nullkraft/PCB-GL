/* $Id$ */

/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 2006 Dan McMahill
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/*
 *  Heavily based on the ps HID written by DJ Delorie
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "global.h"
#include "data.h"
#include "misc.h"

#include "hid.h"
#include "../hidint.h"
#include "png.h"

/* the gd library which makes this all so easy */
#include <gd.h>

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

RCSID("$Id$");

#define CRASH fprintf(stderr, "HID error: pcb called unimplemented PNG function %s.\n", __FUNCTION__); abort()

int scale = 1;
int x_shift = 0;
int y_shift = 0;
#define SCALE(x)   ((x)/scale)
#define SCALE_X(x) (((x) - x_shift)/scale)
#define SCALE_Y(x) (((x) - y_shift)/scale)

typedef struct color_struct {
  /* the descriptor used by the gd library */
  int c;

 /* so I can figure out what rgb value c refers to */
  unsigned int r, g, b;

} color_struct;

typedef struct hid_gc_struct {
  HID *me_pointer;
  EndCapStyle cap;
  int width;
  unsigned char r, g, b;
  int erase;
  int faded;
  color_struct *color;
  gdImagePtr brush;
} hid_gc_struct;

static color_struct *black, *white;
static gdImagePtr im = NULL;
static FILE *f = 0;
static int linewidth = -1;
static int lastgroup = -1;
static gdImagePtr lastbrush = (void *) -1;
static int lastcap = -1;
static int lastcolor = -1;
static int print_group[MAX_LAYER];
static int print_layer[MAX_LAYER];

static char *filetypes[] = {
  "GIF", 
#define FMT_gif 0

  "JPEG",
#define FMT_jpg 1

  "PNG",
#define FMT_png 2

 0};

HID_Attribute png_attribute_list[] = {
  /* other HIDs expect this to be first.  */
  {"outfile", "Graphics output file",
   HID_String, 0, 0, {0, 0, 0}, 0, 0},
#define HA_pngfile 0

  {"dpi", "Scale factor (pixels/inch). 0 to scale to fix specified size",
   HID_Integer, 0, 1000, {0, 0, 0}, 0, 0},
#define HA_dpi 1

  {"x-max", "Maximum width (pixels).  0 to not constrain.",
   HID_Integer, 0, 10000, {0, 0, 0}, 0, 0},
#define HA_xmax 2

  {"y-max", "Maximum height (pixels).  0 to not constrain.",
   HID_Integer, 0, 10000, {0, 0, 0}, 0, 0},
#define HA_ymax 3

  {"xy-max", "Maximum width and height (pixels).  0 to not constrain.",
   HID_Integer, 0, 10000, {800, 0, 0}, 0, 0},
#define HA_xymax 4

  {"as-shown", "Export layers as shown on screen",
   HID_Boolean, 0, 0, {0, 0, 0}, 0, 0},
#define HA_as_shown 5

  {"monochrome", "Convert to monochrome",
   HID_Boolean, 0, 0, {0, 0, 0}, 0, 0},
#define HA_mono 6

  {"only-visible", "Limit the bounds of the PNG file to the visible items",
   HID_Boolean, 0, 0, {0, 0, 0}, 0, 0},
#define HA_only_visible 7

  {"format", "Graphics file format",
   HID_Enum, 0, 0, {2, 0, 0}, filetypes, 0},
#define HA_filetype 8
};
#define NUM_OPTIONS (sizeof(png_attribute_list)/sizeof(png_attribute_list[0]))

REGISTER_ATTRIBUTES(png_attribute_list)

static HID_Attr_Val png_values[NUM_OPTIONS];

static HID_Attribute *
png_get_export_options (int *n)
{
  char *buf = 0;

  if (PCB && PCB->Filename)
    {
      buf = (char *)malloc(strlen(PCB->Filename) + 4);
      if (buf)
	{
	  strcpy(buf, PCB->Filename);
	  if (strcmp (buf + strlen(buf) - 4, ".pcb") == 0)
	    buf[strlen(buf)-4] = 0;

	  switch (png_attribute_list[HA_filetype].default_val.int_value)
	    {
	    case FMT_gif:
	      strcat(buf, ".gif");
	      break;
	      
	    case FMT_jpg:
	      strcat(buf, ".jpg");
	      break;
	      
	    case FMT_png:
	      strcat(buf, ".png");
	      break;
	      
	    default:
	      fprintf (stderr, "Error:  Invalid graphic file format\n");
	      strcat(buf, ".???");
	      break;
	    }

	  if (png_attribute_list[HA_pngfile].default_val.str_value)
	    free (png_attribute_list[HA_pngfile].default_val.str_value);
	  png_attribute_list[HA_pngfile].default_val.str_value = buf;
	}
    }

  if (n)
    *n = NUM_OPTIONS;
  return png_attribute_list;
}

static int comp_layer, solder_layer;

static int
group_for_layer (int l)
{
  if (l < MAX_LAYER+2 && l >= 0)
    return GetLayerGroupNumberByNumber(l);
  /* else something unique */
  return MAX_LAYER + 3 + l;
}

static int
layer_sort(const void *va, const void *vb)
{
  int a = *(int *)va;
  int b = *(int *)vb;
  int al = group_for_layer(a);
  int bl = group_for_layer(b);
  int d = bl - al;

  if (a >= 0 && a <= MAX_LAYER+1)
    {
      int aside = (al == solder_layer ? 0 : al == comp_layer ? 2 : 1);
      int bside = (bl == solder_layer ? 0 : bl == comp_layer ? 2 : 1);
      if (bside != aside)
	return bside - aside;
    }
  if (d)
    return d;
  return b - a;
}

static char *filename;
static BoxType *bounds;
static int in_mono, as_shown;

void
png_hid_export_to_file(FILE *the_file, HID_Attr_Val *options)
{
  int i;
  static int saved_layer_stack[MAX_LAYER];
  BoxType region;

  f = the_file;

  region.X1 = 0;
  region.Y1 = 0;
  region.X2 = PCB->MaxWidth;
  region.Y2 = PCB->MaxHeight;

  if (options[HA_only_visible].int_value)
    bounds = GetDataBoundingBox(PCB->Data);
  else
    bounds = &region;

  memset(print_group, 0, sizeof(print_group));
  memset(print_layer, 0, sizeof(print_layer));

  for (i=0; i<MAX_LAYER; i++)
    {
      LayerType *layer = PCB->Data->Layer+i;
      if (layer->LineN || layer->TextN || layer->ArcN
	  || layer->PolygonN)
	print_group[GetLayerGroupNumberByNumber(i)] = 1;
    }
  print_group[GetLayerGroupNumberByNumber(MAX_LAYER)] = 1;
  print_group[GetLayerGroupNumberByNumber(MAX_LAYER+1)] = 1;
  for (i=0; i<MAX_LAYER; i++)
    if (print_group[GetLayerGroupNumberByNumber(i)])
      print_layer[i] = 1;

  memcpy (saved_layer_stack, LayerStack, sizeof(LayerStack));
  as_shown = options[HA_as_shown].int_value;
  if (! options[HA_as_shown].int_value)
    {
      comp_layer = GetLayerGroupNumberByNumber(MAX_LAYER+COMPONENT_LAYER);
      solder_layer = GetLayerGroupNumberByNumber(MAX_LAYER+SOLDER_LAYER);
      qsort (LayerStack, MAX_LAYER, sizeof(LayerStack[0]), layer_sort);
    }
  linewidth = -1;
  lastbrush = (void *) -1;
  lastcap = -1;
  lastgroup = -1;
  lastcolor = -1;
  lastgroup = -1;

  in_mono = options[HA_mono].int_value;

  hid_expose_callback(&png_hid, bounds, 0);

  memcpy (LayerStack, saved_layer_stack, sizeof(LayerStack));
}

static void
png_do_export (HID_Attr_Val *options)
{
  int save_ons[MAX_LAYER+2];
  int i;
  BoxType *bbox;
  int w, h;
  int xmax, ymax, dpi;
  if (!options)
    {
      png_get_export_options (0);
      for (i=0; i<NUM_OPTIONS; i++)
	png_values[i] = png_attribute_list[i].default_val;
      options = png_values;
    }

  filename = options[HA_pngfile].str_value;
  if (!filename)
    filename = "pcb-out.png";

  /* figure out width and height of the board */
  if (options[HA_only_visible].int_value) 
    {
      bbox = GetDataBoundingBox(PCB->Data);
      x_shift = bbox->X1;
      y_shift = bbox->Y1;
      h = bbox->Y2 - bbox->Y1;
      w = bbox->X2 - bbox->X1;
    }
  else
    {
      x_shift = 0;
      y_shift = 0;
      h = PCB->MaxHeight;
      w = PCB->MaxWidth;
    }

  /*
   * figure out the scale factor we need to make the image
   * fit in our specified PNG file size
   */
  xmax = ymax = dpi = 0;
  if (options[HA_dpi].int_value != 0)
    {
      dpi = options[HA_dpi].int_value;
      if (dpi < 0)
	{
	  fprintf (stderr, "ERROR:  dpi may not be < 0\n");
	  return;
	}
    } 
  else 
    {
      if (options[HA_xmax].int_value > 0)
	xmax = options[HA_xmax].int_value;
      if (options[HA_xymax].int_value > 0)
	{
	  if (options[HA_xymax].int_value < xmax
	      || xmax == 0)
	    xmax = options[HA_xymax].int_value;
	}

      if (options[HA_ymax].int_value > 0)
	ymax = options[HA_ymax].int_value;
      if (options[HA_xymax].int_value > 0)
	{
	  if (options[HA_xymax].int_value < ymax
	      || ymax == 0)
	    ymax = options[HA_xymax].int_value;
	}

      if (xmax <= 0 || ymax <= 0)
	{
	  fprintf (stderr, "ERROR:  xmax and ymax may not be <= 0\n");
	  return;
	}
    }

  if (dpi > 0)
    {
      /*
       * a scale of 1 means 1 pixel is 1/100 mil 
       * a scale of 100,000 means 1 pixel is 1 inch
       */
      scale = 100000 / dpi;
      w = w / scale;
      h = h / scale;
    }
  else 
    {
      if ( (w/xmax) > (h/ymax) )
	{
	  h = (h * xmax) / w;
	  scale = w / xmax;
	  w = xmax;
	}
      else 
	{
	  w = (w * ymax) / h;
	  scale = h / ymax;
	  h = ymax;
	}
    }
  
  im = gdImageCreate (w, h);

  /* 
   * Allocate white and black -- the first color allocated
   * becomes the background color
   */

  white = (color_struct *) malloc (sizeof (color_struct));
  white->r = white->g = white->b = 255; 
  white->c = gdImageColorAllocate (im, white->r, white->g, white->b);
  
  black = (color_struct *) malloc (sizeof (color_struct));
  black->r = black->g = black->b = 0; 
  black->c = gdImageColorAllocate (im, black->r, black->g, black->b);

  f = fopen(filename, "wb");
  if (!f)
    {
      perror (filename);
      return;
    }

  if (! options[HA_as_shown].int_value)
    hid_save_and_show_layer_ons (save_ons);

  png_hid_export_to_file (f, options);

  if (! options[HA_as_shown].int_value)
    hid_restore_layer_ons (save_ons);

  /* actually write out the image */
  switch (options[HA_filetype].int_value)
    {
    case FMT_gif:
      gdImageGif (im, f);
      break;

    case FMT_jpg:
      /* JPEG with default JPEG quality */
      gdImageJpeg (im, f, -1);
      break;
      
    case FMT_png:
      gdImagePng (im, f);
      break;

    default:
      fprintf (stderr, "Error:  Invalid graphic file format\n");
      return;
      break;
    }
  
  fclose (f);

  gdImageDestroy (im);
}

extern void hid_parse_command_line (int *argc, char ***argv);

static void
png_parse_arguments (int *argc, char ***argv)
{
  hid_register_attributes (png_attribute_list, sizeof(png_attribute_list)/sizeof(png_attribute_list[0]));
  hid_parse_command_line (argc, argv);
}


static int is_mask;
static int is_drill;

static int
png_set_layer (const char *name, int group)
{
  int idx = (group >= 0 && group < MAX_LAYER) ? PCB->LayerGroups.Entries[group][0] : group;
  if (name == 0)
    name = PCB->Data->Layer[idx].Name;

  if (idx >= 0 && idx < MAX_LAYER && !print_layer[idx])
    return 0;
  if (SL_TYPE(idx) == SL_ASSY
      || SL_TYPE(idx) == SL_FAB)
    return 0;

  if (strcmp (name, "invisible") == 0)
    return 0;

  is_drill = (SL_TYPE(idx) == SL_DRILL);
  is_mask = (SL_TYPE(idx) == SL_MASK);

  if (is_mask)
    return 0;

  if (as_shown)
    {
      switch (idx)
	{
	case SL(SILK,TOP):
	case SL(SILK,BOTTOM):
	  if (SL_MYSIDE(idx))
	    return PCB->ElementOn;
	}
    }
  else
    {
      switch (idx)
	{
	case SL(SILK,TOP):
	  return 1;
	case SL(SILK,BOTTOM):
	  return 0;
	}
    }


  return 1;
}


static hidGC
png_make_gc (void)
{
  hidGC rv = (hidGC) malloc (sizeof(hid_gc_struct));
  rv->me_pointer = &png_hid;
  rv->cap = Trace_Cap;
  rv->width = 1;
  rv->color = (color_struct *) malloc (sizeof (color_struct));
  rv->color->r = rv->color->g = rv->color->b = 0;
  rv->color->c = 0;
  return rv;
}

static void
png_destroy_gc (hidGC gc)
{
  free(gc);
}

static void
png_use_mask (int use_it)
{
  /* does nothing */
}

static void
png_set_color (hidGC gc, const char *name)
{
  static void *cache = 0;
  hidval cval;

  if (im == NULL)
      return;

  if (name == NULL)
      name = "#ff0000";

  if (strcmp (name, "erase") == 0
      || strcmp (name, "drill") == 0)
    {
      /* FIXME -- should be background, not white */
      gc->color = white;
      gc->erase = 1;
      return;
    }
  gc->erase = 0;

  if (in_mono || (strcmp (name, "#000000") == 0 ) )
    {
      gc->color = black;
      return;
    }

  if (hid_cache_color(0, name, &cval, &cache))
    {
      gc->color = cval.ptr;
    }
  else if (name[0] == '#')
    {
      gc->color = (color_struct *) malloc (sizeof (color_struct));
      sscanf (name+1, "%2x%2x%2x", &(gc->color->r), &(gc->color->g), &(gc->color->b));
      gc->color->c = gdImageColorAllocate (im, gc->color->r, gc->color->g, gc->color->b);
      cval.ptr = gc->color;
      hid_cache_color (1, name, &cval, &cache);
    }
  else
    {
      printf ("WE SHOULD NOT BE HERE!!!\n");
      gc->color = black;
    }

}

static void
png_set_line_cap (hidGC gc, EndCapStyle style)
{
  gc->cap = style;
}

static void
png_set_line_width (hidGC gc, int width)
{
  gc->width = width;
}

static void
png_set_draw_xor (hidGC gc, int xor)
{
  ;
}

static void
png_set_draw_faded (hidGC gc, int faded)
{
  gc->faded = faded;
}

static void
png_set_line_cap_angle (hidGC gc, int x1, int y1, int x2, int y2)
{
  CRASH;
}

static void
use_gc (hidGC gc)
{
  int need_brush = 0;

  if (gc->me_pointer != &png_hid)
    {
      fprintf(stderr, "Fatal: GC from another HID passed to png HID\n");
      abort();
    }

  if (linewidth != gc->width)
    {
      /* Make sure the scaling doesn't erase lines completely */
      if (SCALE (gc->width) == 0 && gc->width > 0) 
	gdImageSetThickness (im, 1);
      else
	gdImageSetThickness (im, SCALE (gc->width));
	  linewidth = gc->width;
      need_brush = 1;
    }

  if (lastbrush != gc->brush || need_brush)
    {
      static void *bcache = 0;
      hidval bval;
      char name[256];
      char type;
      int r;

      switch (gc->cap)
	{
	case Round_Cap:
	case Trace_Cap: 
	  type = 'C'; 
	  r = SCALE (0.5 * gc->width);
	  break;
	default:
	case Square_Cap: 
	  r = SCALE (gc->width);
	  type = 'S';
	  break;
	}
      sprintf (name, "#%.2x%.2x%.2x_%c_%d", gc->color->r, gc->color->g, gc->color->b, type, r);
      
      if (hid_cache_color(0, name, &bval, &bcache))
	{
	  gc->brush = bval.ptr;
	}
      else
	{
	  int bg, fg;
	  if (type == 'C')
	    gc->brush = gdImageCreate (2*r+1, 2*r+1);
	  else
	    gc->brush = gdImageCreate (r+1, r+1);
	  bg = gdImageColorAllocate (gc->brush, 255, 255, 255);
	  fg = gdImageColorAllocate (gc->brush, gc->color->r, gc->color->g, gc->color->b);
	  gdImageColorTransparent (gc->brush, bg);

	  /*
	   * if we shrunk to a radius/box width of zero, then just use
	   * a single pixel to draw with.
	   */
	  if (r == 0)
	    gdImageFilledRectangle (gc->brush, 0, 0, 0, 0, fg);
	  else
	    {
	      if (type == 'C')
		gdImageFilledEllipse (gc->brush, r, r, 2*r, 2*r, fg);
	      else
		gdImageFilledRectangle (gc->brush, 0, 0, r, r, fg);
	    }
	  bval.ptr = gc->brush;
	  hid_cache_color (1, name, &bval, &bcache);
	}

      gdImageSetBrush (im, gc->brush);
      lastbrush = gc->brush;

    }

#define CBLEND(gc) (((gc->r)<<24)|((gc->g)<<16)|((gc->b)<<8)|(gc->faded))
  if (lastcolor != CBLEND(gc))
    {
      if (is_drill || is_mask)
	{
#ifdef FIXME
	  fprintf(f, "%d gray\n", gc->erase ? 0 : 1);
#endif
	  lastcolor = 0;
	}
      else
	{
	  double r, g, b;
	  r = gc->r;
	  g = gc->g;
	  b = gc->b;
	  if (gc->faded)
	    {
	      r = 0.8 * 255 + 0.2 * r;
	      g = 0.8 * 255 + 0.2 * g;
	      b = 0.8 * 255 + 0.2 * b;
	    }
#ifdef FIXME
	  if (gc->r == gc->g && gc->g == gc->b)
	    fprintf(f, "%g gray\n", r/255.0);
	  else
	    fprintf(f, "%g %g %g rgb\n", r/255.0, g/255.0, b/255.0);
#endif
	  lastcolor = CBLEND(gc);
	}
    }
}

static void
png_draw_rect (hidGC gc, int x1, int y1, int x2, int y2)
{
  use_gc (gc);
  gdImageRectangle (im, 
		    SCALE_X (x1), SCALE_Y (y1), 
		    SCALE_X (x2), SCALE_Y (y2),
		    gc->color->c);
}

static void
png_fill_rect (hidGC gc, int x1, int y1, int x2, int y2)
{
  use_gc (gc);
  gdImageSetThickness (im, 0);
  linewidth = 0;
  gdImageFilledRectangle (im, SCALE_X (x1), SCALE_Y (y1), 
			  SCALE_X (x2), SCALE_Y (y2), gc->color->c);
}

static void
png_draw_line (hidGC gc, int x1, int y1, int x2, int y2)
{
  if (x1 == x2 && y1 == y2)
    {
      int w = gc->width / 2;
      png_fill_rect (gc, x1-w, y1-w, x1+w, y1+w);
      return;
    }
  use_gc (gc);

  gdImageSetThickness (im, 0);
  linewidth = 0;
  gdImageLine (im, SCALE_X (x1), SCALE_Y (y1), 
	       SCALE_X (x2), SCALE_Y (y2), gdBrushed);
}

static void
png_draw_arc (hidGC gc, int cx, int cy, int width, int height,
		int start_angle, int delta_angle)
{
  int sa, ea;

  /* 
   * in gdImageArc, 0 degrees is to the right and +90 degrees is down
   * in pcb, 0 degrees is to the left and +90 degrees is down
   */
  start_angle = 180 - start_angle;
  delta_angle = -delta_angle;
  if (delta_angle > 0)
    {
      sa = start_angle;
      ea = start_angle + delta_angle;
    }
  else
    {
      sa = start_angle + delta_angle;
      ea = start_angle;
    }

  /* 
   * make sure we start between 0 and 360 otherwise gd does
   * strange things
   */
  while (sa < 0)
    {
      sa += 360;
      ea += 360;
    }
  while (sa >= 360)
    {
      sa -= 360;
      ea -= 360;
    }

#if 0
  printf("draw_arc %d,%d %dx%d %d..%d %d..%d\n",
	 cx, cy, width, height, start_angle, delta_angle, sa, ea);
  printf ("gdImageArc (%p, %d, %d, %d, %d, %d, %d, %d)\n",
	  im, SCALE_X (cx), SCALE_Y (cy), 
	  SCALE (width), SCALE (height), 
	  sa, ea, gc->color->c);
#endif
  use_gc (gc);
  gdImageSetThickness (im, 0);
  linewidth = 0;
  gdImageArc (im, SCALE_X (cx), SCALE_Y (cy), 
	      SCALE (2*width), SCALE (2*height), 
	      sa, ea, gdBrushed);
}

static void
png_fill_circle (hidGC gc, int cx, int cy, int radius)
{
  use_gc (gc);

  gdImageSetThickness (im, 0);
  linewidth = 0;
  gdImageFilledEllipse (im, SCALE_X (cx), SCALE_Y (cy), 
			SCALE (2*radius), SCALE (2*radius), 
			gc->color->c);

}

static void
png_fill_polygon (hidGC gc, int n_coords, int *x, int *y)
{
  int i;
  gdPoint *points;

  points = (gdPoint *) malloc (n_coords * sizeof (gdPoint));
  if (points == NULL) 
    {
      fprintf (stderr, "ERROR:  png_fill_polygon():  malloc failed\n");
      exit (1);
    }

  use_gc (gc);
  for (i=0; i<n_coords; i++)
    {
      points[i].x = SCALE_X (x[i]);
      points[i].y = SCALE_Y (y[i]);
    }
  gdImageSetThickness (im, 0);
  linewidth = 0;
  gdImageFilledPolygon (im, points, n_coords, gc->color->c);
  free (points);
}

static void
png_calibrate (double xval, double yval)
{
  CRASH;
}

static void
png_set_crosshair (int x, int y)
{
}

HID png_hid = {
  "png",
  "GIF/JPEG/PNG export.",
  0, /* gui */
  0, /* printer */
  1, /* exporter */
  1, /* poly before */
  0, /* poly after */
  png_get_export_options,
  png_do_export,
  png_parse_arguments,
  0 /* png_invalidate_wh */,
  0 /* png_invalidate_lr */,
  0 /* png_invalidate_all */,
  png_set_layer,
  png_make_gc,
  png_destroy_gc,
  png_use_mask,
  png_set_color,
  png_set_line_cap,
  png_set_line_width,
  png_set_draw_xor,
  png_set_draw_faded,
  png_set_line_cap_angle,
  png_draw_line,
  png_draw_arc,
  png_draw_rect,
  png_fill_circle,
  png_fill_polygon,
  png_fill_rect,
  png_calibrate,
  0 /* png_shift_is_pressed */,
  0 /* png_control_is_pressed */,
  0 /* png_get_coords */,
  png_set_crosshair,
  0 /* png_add_timer */,
  0 /* png_stop_timer */,
  0 /* png_log */,
  0 /* png_logv */,
  0 /* png_confirm_dialog */,
  0 /* png_report_dialog */,
  0 /* png_prompt_for */,
  0 /* png_attribute_dialog */,
  0 /* png_show_item */,
  0 /* png_beep */
};

#include "dolists.h"

void
hid_png_init()
{
  apply_default_hid(&png_hid, 0);
  hid_register_hid (&png_hid);

#include "png_lists.h"
}
