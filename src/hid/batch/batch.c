/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "global.h"
#include "hid.h"
#include "data.h"
#include "misc.h"
#include "hid.h"
#include "../hidint.h"

#include "hid/common/actions.h"
#include "hid/common/hidinit.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

RCSID ("$Id$");

/* This is a text-line "batch" HID, which exists for scripting and
   non-GUI needs.  */

typedef struct hid_gc_struct
{
  int nothing_interesting_here;
} hid_gc_struct;

static HID_Attribute *
batch_get_export_options (int *n_ret)
{
  return 0;
}

/* ----------------------------------------------------------------------------- */

static char *prompt = "pcb";

static int
nop (int argc, char **argv, int x, int y)
{
  return 0;
}

static int
PCBChanged (int argc, char **argv, int x, int y)
{
  if (PCB && PCB->Filename)
    {
      prompt = strrchr(PCB->Filename, '/');
      if (prompt)
	prompt ++;
      else
	prompt = PCB->Filename;
    }
  else
    prompt = "no-board";
  return 0;
}

static int
help (int argc, char **argv, int x, int y)
{
  print_actions ();
  return 0;
}

static int
info (int argc, char **argv, int x, int y)
{
  int i, j;
  int cg, sg;
  if (!PCB || !PCB->Data || !PCB->Filename)
    {
      printf("No PCB loaded.\n");
      return 0;
    }
  printf("Filename: %s\n", PCB->Filename);
  printf("Size: %g x %g mils, %g x %g mm\n",
	 PCB->MaxWidth / 100.0,
	 PCB->MaxHeight / 100.0,
	 PCB->MaxWidth * COOR_TO_MM,
	 PCB->MaxHeight * COOR_TO_MM);
  cg = GetLayerGroupNumberByNumber (component_silk_layer);
  sg = GetLayerGroupNumberByNumber (solder_silk_layer);
  for (i=0; i<MAX_LAYER; i++)
    {
      
      int lg = GetLayerGroupNumberByNumber (i);
      for (j=0; j<MAX_LAYER; j++)
	putchar(j==lg ? '#' : '-');
      printf(" %c %s\n", lg==cg ? 'c' : lg==sg ? 's' : '-',
	     PCB->Data->Layer[i].Name);
    }
  return 0;
}


HID_Action batch_action_list[] = {
  {"PCBChanged", 0, PCBChanged },
  {"RouteStylesChanged", 0, nop },
  {"NetlistChanged", 0, nop },
  {"LayersChanged", 0, nop },
  {"LibraryChanged", 0, nop },
  {"Busy", 0, nop },
  {"Help", 0, help },
  {"Info", 0, info }
};

REGISTER_ACTIONS (batch_action_list)


/* ----------------------------------------------------------------------------- */

static void
batch_do_export (HID_Attr_Val * options)
{
  int interactive;
  char line[1000];

  if (isatty (0))
    interactive = 1;
  else
    interactive = 0;

  if (interactive)
    {
      printf("Entering %s version %s batch mode.\n", PACKAGE, VERSION);
      printf("See http://pcb.gpleda.org for project information\n");
    }
  while (1)
    {
      if (interactive)
	{
	  printf("%s> ", prompt);
	  fflush(stdout);
	}
      if (fgets(line, sizeof(line)-1, stdin) == NULL)
	return;
      hid_parse_command (line);
    }
}

static void
batch_parse_arguments (int *argc, char ***argv)
{
  hid_parse_command_line (argc, argv);
}

static void
batch_invalidate_lr (int l, int r, int t, int b)
{
}

static void
batch_invalidate_all (void)
{
}

static int
batch_set_layer (const char *name, int idx, int empty)
{
  return 0;
}

static hidGC
batch_make_gc (void)
{
  return 0;
}

static void
batch_destroy_gc (hidGC gc)
{
}

static void
batch_use_mask (int use_it)
{
}

static void
batch_set_color (hidGC gc, const char *name)
{
}

static void
batch_set_line_cap (hidGC gc, EndCapStyle style)
{
}

static void
batch_set_line_width (hidGC gc, int width)
{
}

static void
batch_set_draw_xor (hidGC gc, int xor_set)
{
}

static void
batch_set_draw_faded (hidGC gc, int faded)
{
}

static void
batch_set_line_cap_angle (hidGC gc, int x1, int y1, int x2, int y2)
{
}

static void
batch_draw_line (hidGC gc, int x1, int y1, int x2, int y2)
{
}

static void
batch_draw_arc (hidGC gc, int cx, int cy, int width, int height,
		int start_angle, int end_angle)
{
}

static void
batch_draw_rect (hidGC gc, int x1, int y1, int x2, int y2)
{
}

static void
batch_fill_circle (hidGC gc, int cx, int cy, int radius)
{
}

static void
batch_fill_polygon (hidGC gc, int n_coords, int *x, int *y)
{
}

static void
batch_fill_pcb_polygon (hidGC gc, PolygonType *poly, const BoxType *clip_box)
{
}

static void
batch_thindraw_pcb_polygon (hidGC gc, PolygonType *poly, const BoxType *clip_box)
{
}

static void
batch_fill_rect (hidGC gc, int x1, int y1, int x2, int y2)
{
}

static void
batch_calibrate (double xval, double yval)
{
}

static int
batch_shift_is_pressed (void)
{
  return 0;
}

static int
batch_control_is_pressed (void)
{
  return 0;
}

static int
batch_mod1_is_pressed (void)
{
  return 0;
}

static void
batch_get_coords (const char *msg, int *x, int *y)
{
}

static void
batch_set_crosshair (int x, int y, int action)
{
}

static hidval
batch_add_timer (void (*func) (hidval user_data),
		 unsigned long milliseconds, hidval user_data)
{
  hidval rv;
  rv.lval = 0;
  return rv;
}

static void
batch_stop_timer (hidval timer)
{
}

hidval
batch_watch_file (int fd, unsigned int condition, void (*func) (hidval watch, int fd, unsigned int condition, hidval user_data),
    hidval user_data)
{
  hidval ret;
  ret.ptr = NULL;
  return ret;
}

void
batch_unwatch_file (hidval data)
{
}

static hidval
batch_add_block_hook (void (*func) (hidval data), hidval user_data )
{
  hidval ret;
  ret.ptr = NULL;
  return ret;
}

static void
batch_stop_block_hook (hidval mlpoll)
{
}

static int
batch_attribute_dialog (HID_Attribute * attrs_,
			int n_attrs_, HID_Attr_Val * results_,
			const char *title_, const char *descr_)
{
  return 0;
}

static void
batch_show_item (void *item)
{
}

#include "dolists.h"

HID batch_gui;

void
hid_batch_init ()
{
  memset (&batch_gui, 0, sizeof (HID));

  batch_gui.struct_size           = sizeof (HID);
  batch_gui.name                  = "batch";
  batch_gui.description           = "Batch-mode GUI for non-interactive use.";

  batch_gui.gui                   = 1;
  batch_gui.printer               = 0;
  batch_gui.exporter              = 0;
  batch_gui.poly_before           = 0;
  batch_gui.poly_after            = 0;
  batch_gui.poly_dicer            = 0;

  batch_gui.get_export_options    = batch_get_export_options;
  batch_gui.do_export             = batch_do_export;
  batch_gui.parse_arguments       = batch_parse_arguments;
  batch_gui.invalidate_lr         = batch_invalidate_lr;
  batch_gui.invalidate_all        = batch_invalidate_all;
  batch_gui.set_layer             = batch_set_layer;
  batch_gui.make_gc               = batch_make_gc;
  batch_gui.destroy_gc            = batch_destroy_gc;
  batch_gui.use_mask              = batch_use_mask;
  batch_gui.set_color             = batch_set_color;
  batch_gui.set_line_cap          = batch_set_line_cap;
  batch_gui.set_line_width        = batch_set_line_width;
  batch_gui.set_draw_xor          = batch_set_draw_xor;
  batch_gui.set_draw_faded        = batch_set_draw_faded;
  batch_gui.set_line_cap_angle    = batch_set_line_cap_angle;
  batch_gui.draw_line             = batch_draw_line;
  batch_gui.draw_arc              = batch_draw_arc;
  batch_gui.draw_rect             = batch_draw_rect;
  batch_gui.fill_circle           = batch_fill_circle;
  batch_gui.fill_polygon          = batch_fill_polygon;
  batch_gui.fill_pcb_polygon      = batch_fill_pcb_polygon;
  batch_gui.thindraw_pcb_polygon  = batch_thindraw_pcb_polygon;
  batch_gui.fill_rect             = batch_fill_rect;
  batch_gui.calibrate             = batch_calibrate;
  batch_gui.shift_is_pressed      = batch_shift_is_pressed;
  batch_gui.control_is_pressed    = batch_control_is_pressed;
  batch_gui.mod1_is_pressed       = batch_mod1_is_pressed;
  batch_gui.get_coords            = batch_get_coords;
  batch_gui.set_crosshair         = batch_set_crosshair;
  batch_gui.add_timer             = batch_add_timer;
  batch_gui.stop_timer            = batch_stop_timer;
  batch_gui.watch_file            = batch_watch_file;
  batch_gui.unwatch_file          = batch_unwatch_file;
  batch_gui.add_block_hook        = batch_add_block_hook;
  batch_gui.stop_block_hook       = batch_stop_block_hook;
  batch_gui.attribute_dialog      = batch_attribute_dialog;
  batch_gui.show_item             = batch_show_item;

  apply_default_hid (&batch_gui, 0);
  hid_register_hid (&batch_gui);
#include "batch_lists.h"
}
