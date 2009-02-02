/* $Id: gui-output-events.c,v 1.29 2008-11-29 13:20:36 danmc Exp $ */

/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 1994,1995,1996,1997,1998,1999 Thomas Nau
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
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Contact addresses for paper mail and Email:
 *  Thomas Nau, Schlehenweg 15, 88471 Baustetten, Germany
 *  Thomas.Nau@rz.uni-ulm.de
 *
 */

/* This file written by Bill Wilson for the PCB Gtk port */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gui.h"
#include "gtkhid.h"

#include <gdk/gdkkeysyms.h>

#include "action.h"
#include "crosshair.h"
#include "draw.h"
#include "error.h"
#include "misc.h"
#include "set.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

RCSID ("$Id: gui-output-events.c,v 1.29 2008-11-29 13:20:36 danmc Exp $");

static gint x_pan_speed, y_pan_speed;

/* Set to true if cursor is currently in viewport. This is a hack to prevent
 * Crosshair stack corruption due to unmatching window enter / leave events */
gboolean cursor_in_viewport = False;

void
ghid_port_ranges_changed (void)
{
  GtkAdjustment *h_adj, *v_adj;

  if (!ghidgui->combine_adjustments)
    HideCrosshair (FALSE);
  if (ghidgui->combine_adjustments)
    {
      ghidgui->combine_adjustments = FALSE;
      return;
    }

  ghidgui->need_restore_crosshair = TRUE;

  h_adj = gtk_range_get_adjustment (GTK_RANGE (ghidgui->h_range));
  v_adj = gtk_range_get_adjustment (GTK_RANGE (ghidgui->v_range));
  gport->view_x0 = h_adj->value;
  gport->view_y0 = v_adj->value;

  ghid_invalidate_all ();
}

gboolean
ghid_port_ranges_pan (gdouble x, gdouble y, gboolean relative)
{
  GtkAdjustment *h_adj, *v_adj;
  gdouble x0, y0, x1, y1;

  h_adj = gtk_range_get_adjustment (GTK_RANGE (ghidgui->h_range));
  v_adj = gtk_range_get_adjustment (GTK_RANGE (ghidgui->v_range));
  x0 = h_adj->value;
  y0 = v_adj->value;

  if (relative)
    {
      x1 = x0 + x;
      y1 = y0 + y;
    }
  else
    {
      x1 = x;
      y1 = y;
    }

  if (x1 < h_adj->lower)
    x1 = h_adj->lower;
  if (x1 > h_adj->upper - h_adj->page_size)
    x1 = h_adj->upper - h_adj->page_size;

  if (y1 < v_adj->lower)
    y1 = v_adj->lower;
  if (y1 > v_adj->upper - v_adj->page_size)
    y1 = v_adj->upper - v_adj->page_size;

  if (x0 != x1 && y0 != y1)
    ghidgui->combine_adjustments = TRUE;
  if (x0 != x1)
    gtk_range_set_value (GTK_RANGE (ghidgui->h_range), x1);
  if (y0 != y1)
    gtk_range_set_value (GTK_RANGE (ghidgui->v_range), y1);

  ghid_note_event_location (NULL);
  return ((x0 != x1) || (y0 != y1));
}

  /* Do scrollbar scaling based on current port drawing area size and
     |  overall PCB board size.
   */
void
ghid_port_ranges_scale (gboolean emit_changed)
{
  GtkAdjustment *adj;

  /* Update the scrollbars with PCB units.  So Scale the current
     |  drawing area size in pixels to PCB units and that will be
     |  the page size for the Gtk adjustment.
   */
  gport->view_width = gport->width * gport->zoom;
  gport->view_height = gport->height * gport->zoom;

  if (gport->view_width >= PCB->MaxWidth)
    gport->view_width = PCB->MaxWidth;
  if (gport->view_height >= PCB->MaxHeight)
    gport->view_height = PCB->MaxHeight;

  adj = gtk_range_get_adjustment (GTK_RANGE (ghidgui->h_range));
  adj->page_size = gport->view_width;
  adj->page_increment = adj->page_size/10.0;
  adj->step_increment = adj->page_size/100.0;
  adj->upper = PCB->MaxWidth;
  if (emit_changed)
    gtk_signal_emit_by_name (GTK_OBJECT (adj), "changed");

  adj = gtk_range_get_adjustment (GTK_RANGE (ghidgui->v_range));
  adj->page_size = gport->view_height;
  adj->page_increment = adj->page_size/10.0;
  adj->step_increment = adj->page_size/100.0;
  adj->upper = PCB->MaxHeight;
  if (emit_changed)
    gtk_signal_emit_by_name (GTK_OBJECT (adj), "changed");
}

void
ghid_port_ranges_zoom (gdouble zoom)
{
  gdouble xtmp, ytmp;
  gint		x0, y0;

  /* figure out zoom values in that would just make the width fit and
   * that would just make the height fit
   */
  xtmp = (gdouble) PCB->MaxWidth / gport->width;
  ytmp = (gdouble) PCB->MaxHeight / gport->height;

  /* if we've tried to zoom further out than what would make the
   * entire board fit or we passed 0, then pick a zoom that just makes
   * the board fit.
   */
  if ((zoom > xtmp && zoom > ytmp) || zoom == 0.0)
    zoom = (xtmp > ytmp) ? xtmp : ytmp;

  xtmp = (gport->view_x - gport->view_x0) / (gdouble) gport->view_width;
  ytmp = (gport->view_y - gport->view_y0) / (gdouble) gport->view_height;

  gport->zoom = zoom;
  pixel_slop = zoom;
  ghid_port_ranges_scale(FALSE);

  x0 = gport->view_x - xtmp * gport->view_width;
  if (x0 < 0)
    x0 = 0;
  gport->view_x0 = x0;

  y0 = gport->view_y - ytmp * gport->view_height;
  if (y0 < 0)
    y0 = 0;
  gport->view_y0 = y0;

  ghidgui->adjustment_changed_holdoff = TRUE;
  gtk_range_set_value (GTK_RANGE (ghidgui->h_range), gport->view_x0);
  gtk_range_set_value (GTK_RANGE (ghidgui->v_range), gport->view_y0);
  ghidgui->adjustment_changed_holdoff = FALSE;

  ghid_port_ranges_changed();
}


/* ---------------------------------------------------------------------- 
 * handles all events from PCB drawing area
 */

static gint event_x, event_y;

void
ghid_get_coords (const char *msg, int *x, int *y)
{
  if (!ghid_port.has_entered)
    ghid_get_user_xy (msg);
  *x = SIDE_X (gport->view_x);
  *y = SIDE_Y (gport->view_y);
}

gboolean
ghid_note_event_location (GdkEventButton * ev)
{
  gint x, y;
  gboolean moved;

  if (!ev)
    {
      gdk_window_get_pointer (ghid_port.drawing_area->window, &x, &y, NULL);
      event_x = x;
      event_y = y;
    }
  else
    {
      event_x = ev->x;
      event_y = ev->y;
    }
  gport->view_x = event_x * gport->zoom + gport->view_x0;
  gport->view_y = event_y * gport->zoom + gport->view_y0;

  moved = MoveCrosshairAbsolute (SIDE_X (gport->view_x), 
				 SIDE_Y (gport->view_y));
  if (moved)
    {
      AdjustAttachedObjects ();
      RestoreCrosshair (False);
    }
  ghid_set_cursor_position_labels ();
  return moved;
}

static gboolean
have_crosshair_attachments (void)
{
  gboolean result = FALSE;

  switch (Settings.Mode)
    {
    case COPY_MODE:
    case MOVE_MODE:
    case INSERTPOINT_MODE:
      if (Crosshair.AttachedObject.Type != NO_TYPE)
	result = TRUE;
      break;
    case PASTEBUFFER_MODE:
    case VIA_MODE:
      result = TRUE;
      break;
    case POUR_MODE:
      if (Crosshair.AttachedLine.State != STATE_FIRST)
	result = TRUE;
      break;
    case ARC_MODE:
      if (Crosshair.AttachedBox.State != STATE_FIRST)
	result = TRUE;
      break;
    case LINE_MODE:
      if (Crosshair.AttachedLine.State != STATE_FIRST)
	result = TRUE;
      break;
    default:
      if (Crosshair.AttachedBox.State == STATE_SECOND
	  || Crosshair.AttachedBox.State == STATE_THIRD)
	result = TRUE;
      break;
    }
  return result;
}


#define	VCW		16
#define VCD		8

static void
draw_right_cross (gint x, gint y)
{
  glVertex2i (x, 0);
  glVertex2i (x, gport->height);
  glVertex2i (0, y);
  glVertex2i (gport->width, y);
}

static void
draw_slanted_cross (gint x, gint y)
{
  gint x0, y0, x1, y1;

  x0 = x + (gport->height - y);
  x0 = MAX(0, MIN (x0, gport->width));
  x1 = x - y;
  x1 = MAX(0, MIN (x1, gport->width));
  y0 = y + (gport->width - x);
  y0 = MAX(0, MIN (y0, gport->height));
  y1 = y - x;
  y1 = MAX(0, MIN (y1, gport->height));
  glVertex2i (x0, y0);
  glVertex2i (x1, y1);

  x0 = x - (gport->height - y);
  x0 = MAX(0, MIN (x0, gport->width));
  x1 = x + y;
  x1 = MAX(0, MIN (x1, gport->width));
  y0 = y + x;
  y0 = MAX(0, MIN (y0, gport->height));
  y1 = y - (gport->width - x);
  y1 = MAX(0, MIN (y1, gport->height));
  glVertex2i (x0, y0);
  glVertex2i (x1, y1);
}

static void
draw_dozen_cross (gint x, gint y)
{
  gint x0, y0, x1, y1;
  gdouble tan60 = sqrt (3);

  x0 = x + (gport->height - y) / tan60;
  x0 = MAX(0, MIN (x0, gport->width));
  x1 = x - y / tan60;
  x1 = MAX(0, MIN (x1, gport->width));
  y0 = y + (gport->width - x) * tan60;
  y0 = MAX(0, MIN (y0, gport->height));
  y1 = y - x * tan60;
  y1 = MAX(0, MIN (y1, gport->height));
  glVertex2i (x0, y0);
  glVertex2i (x1, y1);

  x0 = x + (gport->height - y) * tan60;
  x0 = MAX(0, MIN (x0, gport->width));
  x1 = x - y * tan60;
  x1 = MAX(0, MIN (x1, gport->width));
  y0 = y + (gport->width - x) / tan60;
  y0 = MAX(0, MIN (y0, gport->height));
  y1 = y - x / tan60;
  y1 = MAX(0, MIN (y1, gport->height));
  glVertex2i (x0, y0);
  glVertex2i (x1, y1);

  x0 = x - (gport->height - y) / tan60;
  x0 = MAX(0, MIN (x0, gport->width));
  x1 = x + y / tan60;
  x1 = MAX(0, MIN (x1, gport->width));
  y0 = y + x * tan60;
  y0 = MAX(0, MIN (y0, gport->height));
  y1 = y - (gport->width - x) * tan60;
  y1 = MAX(0, MIN (y1, gport->height));
  glVertex2i (x0, y0);
  glVertex2i (x1, y1);

  x0 = x - (gport->height - y) * tan60;
  x0 = MAX(0, MIN (x0, gport->width));
  x1 = x + y * tan60;
  x1 = MAX(0, MIN (x1, gport->width));
  y0 = y + x / tan60;
  y0 = MAX(0, MIN (y0, gport->height));
  y1 = y - (gport->width - x) / tan60;
  y1 = MAX(0, MIN (y1, gport->height));
  glVertex2i (x0, y0);
  glVertex2i (x1, y1);
}

static void
draw_crosshair (gint x, gint y)
{
  static enum crosshair_shape prev = Basic_Crosshair_Shape;

  draw_right_cross (x, y);
  if (prev == Union_Jack_Crosshair_Shape)
    draw_slanted_cross (x, y);
  if (prev == Dozen_Crosshair_Shape)
    draw_dozen_cross (x, y);
  prev = Crosshair.shape;
}

void
ghid_show_crosshair (gboolean show)
{
  gint x, y;
  static gint x_prev = -1, y_prev = -1;
  static int done_once = 0;
  static GdkColor cross_color;

  if (gport->x_crosshair < 0 || ghidgui->creating) {// || !gport->has_entered) {
    printf ("Returning\n");
    return;
  }

  if (!done_once)
    {
      done_once = 1;
      /* FIXME: when CrossColor changed from config */
      ghid_map_color_string (Settings.CrossColor, &cross_color);
    }
  x = DRAW_X (gport->x_crosshair);
  y = DRAW_Y (gport->y_crosshair);

  glEnable (GL_COLOR_LOGIC_OP);
  glLogicOp (GL_XOR);

  hidgl_flush_triangles (&buffer);

  glColor3f (cross_color.red / 65535.,
             cross_color.green / 65535.,
             cross_color.blue / 65535.);

  glBegin (GL_LINES);

#if 1
  if (x_prev >= 0)
    {
      draw_crosshair (x_prev, y_prev);
    }
#endif

  if (x >= 0 && show)
    {
      draw_crosshair (x, y);
    }

  glEnd ();

  if (ghidgui->auto_pan_on && have_crosshair_attachments ())
    {
      glBegin (GL_QUADS);

#if 1
      if (x_prev >= 0)
        {
          glVertex2i (0,                  y_prev - VCD);
          glVertex2i (0,                  y_prev - VCD + VCW);
          glVertex2i (VCD,                y_prev - VCD + VCW);
          glVertex2i (VCD,                y_prev - VCD);
          glVertex2i (gport->width,       y_prev - VCD);
          glVertex2i (gport->width,       y_prev - VCD + VCW);
          glVertex2i (gport->width - VCD, y_prev - VCD + VCW);
          glVertex2i (gport->width - VCD, y_prev - VCD);
          glVertex2i (x_prev - VCD,       0);
          glVertex2i (x_prev - VCD,       VCD);
          glVertex2i (x_prev - VCD + VCW, VCD);
          glVertex2i (x_prev - VCD + VCW, 0);
          glVertex2i (x_prev - VCD,       gport->height - VCD);
          glVertex2i (x_prev - VCD,       gport->height);
          glVertex2i (x_prev - VCD + VCW, gport->height);
          glVertex2i (x_prev - VCD + VCW, gport->height - VCD);
        }
#endif

      if (x >= 0 && show)
        {
          glVertex2i (0,                  y - VCD);
          glVertex2i (0,                  y - VCD + VCW);
          glVertex2i (VCD,                y - VCD + VCW);
          glVertex2i (VCD,                y - VCD);
          glVertex2i (gport->width,       y - VCD);
          glVertex2i (gport->width,       y - VCD + VCW);
          glVertex2i (gport->width - VCD, y - VCD + VCW);
          glVertex2i (gport->width - VCD, y - VCD);
          glVertex2i (x - VCD,            0);
          glVertex2i (x - VCD,            VCD);
          glVertex2i (x - VCD + VCW,      VCD);
          glVertex2i (x - VCD + VCW,      0);
          glVertex2i (x - VCD,            gport->height - VCD);
          glVertex2i (x - VCD,            gport->height);
          glVertex2i (x - VCD + VCW,      gport->height);
          glVertex2i (x - VCD + VCW,      gport->height - VCD);
        }

      glEnd ();
    }

  if (x >= 0 && show)
    {
      x_prev = x;
      y_prev = y;
    }
  else
    x_prev = y_prev = -1;

  glDisable (GL_COLOR_LOGIC_OP);
}

static gboolean
ghid_idle_cb (gpointer data)
{
  if (Settings.Mode == NO_MODE)
    SetMode (ARROW_MODE);
  ghid_mode_cursor (Settings.Mode);
  if (ghidgui->settings_mode != Settings.Mode)
    {
      ghid_mode_buttons_update ();
    }
  ghidgui->settings_mode = Settings.Mode;

  ghid_update_toggle_flags ();
  return FALSE;
}

gboolean
ghid_port_key_release_cb (GtkWidget * drawing_area, GdkEventKey * kev,
			  GtkUIManager * ui)
{
  gint ksym = kev->keyval;

  if (ghid_is_modifier_key_sym (ksym))
    ghid_note_event_location (NULL);

  HideCrosshair (TRUE);
  AdjustAttachedObjects ();
  ghid_invalidate_all ();
  RestoreCrosshair (TRUE);
  ghid_screen_update ();
  g_idle_add (ghid_idle_cb, NULL);
  return FALSE;
}

/* Handle user keys in the output drawing area.
 * Note that the default is for all hotkeys to be handled by the
 * menu accelerators.
 *
 * Key presses not handled by the menus will show up here.  This means
 * the key press was either not defined in the menu resource file or
 * that the key press is special in that gtk doesn't allow the normal
 * menu code to ever see it.  We capture those here (like Tab and the
 * arrow keys) and feed it back to the normal menu callback.
 */

gboolean
ghid_port_key_press_cb (GtkWidget * drawing_area,
			GdkEventKey * kev, GtkUIManager * ui)
{
  ModifierKeysState mk;
  gint  ksym = kev->keyval;
  gboolean handled;
  extern  void ghid_hotkey_cb (int);
  GdkModifierType state;

  if (ghid_is_modifier_key_sym (ksym))
    ghid_note_event_location (NULL);

  state = (GdkModifierType) (kev->state);
  mk = ghid_modifier_keys_state (&state);

  ghid_show_crosshair (FALSE);

  handled = TRUE;		/* Start off assuming we handle it */
  switch (ksym)
    {
    case GDK_Alt_L:
    case GDK_Alt_R:
    case GDK_Control_L:
    case GDK_Control_R:
    case GDK_Shift_L:
    case GDK_Shift_R:
    case GDK_Shift_Lock:
      break;

    case GDK_Up:
      ghid_hotkey_cb (GHID_KEY_UP);
      break;
      
    case GDK_Down:
      ghid_hotkey_cb (GHID_KEY_DOWN);
      break;
    case GDK_Left:
      ghid_hotkey_cb (GHID_KEY_LEFT);
      break;
    case GDK_Right:
      ghid_hotkey_cb (GHID_KEY_RIGHT);
      break;

    case GDK_ISO_Left_Tab: 
    case GDK_3270_BackTab: 
      switch (mk) 
	{
	case NONE_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	case CONTROL_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_CONTROL | GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	case MOD1_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_ALT | GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	case SHIFT_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	case SHIFT_CONTROL_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_CONTROL | GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	case SHIFT_MOD1_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_ALT | GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	  
	default:
	  handled = FALSE;
	  break;
	}
      break;

    case GDK_Tab: 
      switch (mk) 
	{
	case NONE_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_TAB);
	  break;
	case CONTROL_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_CONTROL | GHID_KEY_TAB);
	  break;
	case MOD1_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_ALT | GHID_KEY_TAB);
	  break;
	case SHIFT_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	case SHIFT_CONTROL_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_CONTROL | GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	case SHIFT_MOD1_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_ALT | GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	  
	default:
	  handled = FALSE;
	  break;
	}
      break;

    default:
      handled = FALSE;
    }

  return handled;
}

static gboolean
in_draw_state (void)
{
  if ((Settings.Mode == LINE_MODE
       && Crosshair.AttachedLine.State != STATE_FIRST)
      || (Settings.Mode == ARC_MODE
	  && Crosshair.AttachedBox.State != STATE_FIRST)
      || (Settings.Mode == RECTANGLE_MODE
	  && Crosshair.AttachedBox.State != STATE_FIRST)
      || (Settings.Mode == POUR_MODE
	  && Crosshair.AttachedLine.State != STATE_FIRST))
    return TRUE;
  return FALSE;
}

static gboolean draw_state_reset;
static gint x_press, y_press;

gboolean
ghid_port_button_press_cb (GtkWidget * drawing_area,
			   GdkEventButton * ev, GtkUIManager * ui)
{
  GtkWidget *menu = gtk_ui_manager_get_widget (ui, "/Popup1");
  ModifierKeysState mk;
  gboolean drag, start_pan = FALSE;
  GdkModifierType state;

  /* Reject double and triple click events */
  if (ev->type != GDK_BUTTON_PRESS) return TRUE;

  x_press = ev->x;
  y_press = ev->y;

  ghid_note_event_location (ev);
  state = (GdkModifierType) (ev->state);
  mk = ghid_modifier_keys_state (&state);
  ghid_show_crosshair (FALSE);
  HideCrosshair (TRUE);
  drag = have_crosshair_attachments ();
  draw_state_reset = FALSE;

  switch (ev->button)
    {
    case 1:
      if (mk == NONE_PRESSED || mk == SHIFT_PRESSED)
	hid_actionl ("Mode", "Notify", NULL);
      else if (mk == CONTROL_PRESSED)
	{
	  hid_actionl ("Mode", "Save", NULL);
	  hid_actionl ("Mode", "None", NULL);
	  hid_actionl ("Mode", "Restore", NULL);
	  hid_actionl ("Mode", "Notify", NULL);
	}
      else if (mk == SHIFT_CONTROL_PRESSED)
	{
	  hid_actionl ("Mode", "Save", NULL);
	  hid_actionl ("Mode", "Remove", NULL);
	  hid_actionl ("Mode", "Notify", NULL);
	  hid_actionl ("Mode", "Restore", NULL);
	}
      break;

    case 2:
      if (mk == NONE_PRESSED && in_draw_state ())
	{
	  if (Settings.Mode == LINE_MODE)
	    hid_actionl ("Mode", "Line", NULL);
	  else if (Settings.Mode == ARC_MODE)
	    hid_actionl ("Mode", "Arc", NULL);
	  else if (Settings.Mode == RECTANGLE_MODE)
	    hid_actionl ("Mode", "Rectangle", NULL);
	  else if (Settings.Mode == POUR_MODE)
	    hid_actionl ("Mode", "Pour", NULL);
//	    hid_actionl ("Mode", "Polygon", NULL);

	  hid_actionl ("Mode", "Notify", NULL);
	  draw_state_reset = TRUE;
	}
      else if (mk == NONE_PRESSED)
	{
	  hid_actionl ("Mode", "Save", NULL);
	  hid_actionl ("Mode", "Stroke", NULL);
	}
      else if (mk == CONTROL_PRESSED)
	{
	  hid_actionl ("Mode", "Save", NULL);
	  hid_actionl ("Mode", "Copy", NULL);
	  hid_actionl ("Mode", "Notify", NULL);
	}
      else if (mk == SHIFT_CONTROL_PRESSED)
	{
	  hid_actionl ("Display", "ToggleRubberbandMode", NULL);
	  hid_actionl ("Mode", "Save", NULL);
	  hid_actionl ("Mode", "Move", NULL);
	  hid_actionl ("Mode", "Notify", NULL);
	}
      break;

    case 3:
      if (mk == NONE_PRESSED)
	{
	  ghid_mode_cursor (PAN_MODE);
	  start_pan = TRUE;
	}
      else if (mk == SHIFT_PRESSED
	       && !ghidgui->command_entry_status_line_active)
	{
	  ghidgui->in_popup = TRUE;
	  gtk_widget_grab_focus (drawing_area);
	  if (GTK_IS_MENU (menu))
	    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL,
			    drawing_area, 3, ev->time);
	}

      break;
    }
  ghid_invalidate_all ();
  RestoreCrosshair (TRUE);
  ghid_set_status_line_label ();
  ghid_show_crosshair (TRUE);
  if (!start_pan)
    g_idle_add (ghid_idle_cb, NULL);
  return TRUE;
}


gboolean
ghid_port_button_release_cb (GtkWidget * drawing_area,
			     GdkEventButton * ev, GtkUIManager * ui)
{
  ModifierKeysState mk;
  gboolean drag;
  GdkModifierType state;

  ghid_note_event_location (ev);
  state = (GdkModifierType) (ev->state);
  mk = ghid_modifier_keys_state (&state);

  drag = have_crosshair_attachments ();
  if (drag)
    HideCrosshair (TRUE);

  switch (ev->button)
    {
    case 1:
      hid_actionl ("Mode", "Release", NULL);	/* For all modifier states */
      break;

    case 2:
      if (mk == NONE_PRESSED && !draw_state_reset)
	{
	  hid_actionl ("Mode", "Release", NULL);
	  hid_actionl ("Mode", "Restore", NULL);
	}
      else if (mk == CONTROL_PRESSED)
	{
	  hid_actionl ("Mode", "Notify", NULL);
	  hid_actionl ("Mode", "Restore", NULL);
	}
      else if (mk == SHIFT_CONTROL_PRESSED)
	{
	  hid_actionl ("Mode", "Notify", NULL);
	  hid_actionl ("Mode", "Restore", NULL);
	  hid_actionl ("Display", "ToggleRubberbandMode", NULL);
	}
      break;

    case 3:
      if (mk == SHIFT_PRESSED)
	{
	  hid_actionl ("Display", "Center", NULL);
	  hid_actionl ("Display", "Restore", NULL);
	}
      else if (mk == CONTROL_PRESSED)
	  hid_actionl ("Display", "CycleCrosshair", NULL);
      else if (ev->x == x_press && ev->y == y_press)
	{
	  ghid_show_crosshair (FALSE);
	  ghidgui->auto_pan_on = !ghidgui->auto_pan_on;
	  ghid_show_crosshair (TRUE);
	}
      break;
    }
  if (drag)
    {
      AdjustAttachedObjects ();
      ghid_invalidate_all ();
      RestoreCrosshair (TRUE);
      ghid_screen_update ();
    }
  ghid_set_status_line_label ();
  g_idle_add (ghid_idle_cb, NULL);
  return TRUE;
}


gboolean
ghid_port_drawing_area_configure_event_cb (GtkWidget * widget,
					   GdkEventConfigure * ev,
					   GHidPort * out)
{
  static gboolean first_time_done;

  HideCrosshair (TRUE);
  gport->width = ev->width;
  gport->height = ev->height;

  if (gport->pixmap)
    gdk_pixmap_unref (gport->pixmap);

  gport->pixmap = gdk_pixmap_new (widget->window,
				  gport->width, gport->height, -1);
  gport->drawable = gport->pixmap;

  if (!first_time_done)
    {
      gport->colormap = gtk_widget_get_colormap (gport->top_window);
      if (gdk_color_parse (Settings.BackgroundColor, &gport->bg_color))
	gdk_color_alloc (gport->colormap, &gport->bg_color);
      else
	gdk_color_white (gport->colormap, &gport->bg_color);

      if (gdk_color_parse (Settings.OffLimitColor, &gport->offlimits_color))
	gdk_color_alloc (gport->colormap, &gport->offlimits_color);
      else
	gdk_color_white (gport->colormap, &gport->offlimits_color);
      first_time_done = TRUE;
      PCBChanged (0, NULL, 0, 0);
    }
//  if (gport->mask)
//    {
//      gdk_pixmap_unref (gport->mask);
//      gport->mask = gdk_pixmap_new (0, gport->width, gport->height, 1);
//    }
  ghid_port_ranges_scale (FALSE);
  ghid_invalidate_all ();
  RestoreCrosshair (TRUE);
  return 0;
}

static inline int
Px (int x)
{
  int rv = x * gport->zoom + gport->view_x0;
  if (ghid_flip_x)
    rv = PCB->MaxWidth - (x * gport->zoom + gport->view_x0);
  return  rv;
}

static inline int
Py (int y)
{
  int rv = y * gport->zoom + gport->view_y0;
  if (ghid_flip_y)
    rv = PCB->MaxHeight - (y * gport->zoom + gport->view_y0);
  return  rv;
}

static inline int
Vx (int x)
{
  int rv;
  if (ghid_flip_x) 
    rv = (PCB->MaxWidth - x - gport->view_x0) / gport->zoom + 0.5;
  else
    rv = (x - gport->view_x0) / gport->zoom + 0.5;
  return rv;
}

static inline int
Vy (int y)
{
  int rv;
  if (ghid_flip_y)
    rv = (PCB->MaxHeight - y - gport->view_y0) / gport->zoom + 0.5;
  else
    rv = (y - gport->view_y0) / gport->zoom + 0.5;
  return rv;
}

void
ghid_screen_update (void)
{
#if 0
  ghid_show_crosshair (FALSE);
  gdk_draw_drawable (gport->drawing_area->window, gport->bg_gc, gport->pixmap,
		     0, 0, 0, 0, gport->width, gport->height);
  ghid_show_crosshair (TRUE);
#endif
}

void DrawAttached (Boolean);
void draw_grid ();

#define Z_NEAR 3.0
gboolean
ghid_port_drawing_area_expose_event_cb (GtkWidget * widget,
					GdkEventExpose * ev, GHidPort * port)
{
  GdkRectangle *rectangles;
  int n_rectangles;
  int i;
  BoxType region;
  int eleft, eright, etop, ebottom;
  extern HID ghid_hid;
  GdkGLContext* pGlContext = gtk_widget_get_gl_context (widget);
  GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (widget);

  /* make GL-context "current" */
  if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext)) {
    return FALSE;
  }

  ghid_show_crosshair (FALSE);

  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//  glEnable(GL_POLYGON_SMOOTH);
//  glHint(GL_POLYGON_SMOOTH_HINT, [GL_FASTEST, GL_NICEST, or GL_DONT_CARE]);

  glViewport (ev->area.x,
              widget->allocation.height - ev->area.height - ev->area.y,
              ev->area.width, ev->area.height);

  glEnable (GL_SCISSOR_TEST);
  glScissor (ev->area.x,
             widget->allocation.height - ev->area.height - ev->area.y,
             ev->area.width, ev->area.height);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glOrtho (ev->area.x, ev->area.x + ev->area.width, ev->area.y + ev->area.height, ev->area.y, 0, 100);
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
  glTranslatef (0.0f, 0.0f, -Z_NEAR);

  glClearColor (gport->bg_color.red / 65535.,
                gport->bg_color.green / 65535.,
                gport->bg_color.blue / 65535.,
                1.);

  glClear (GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  region.X1 = MIN (Px (ev->area.x), Px (ev->area.x + ev->area.width + 1));
  region.X2 = MAX (Px (ev->area.x), Px (ev->area.x + ev->area.width + 1));
  region.Y1 = MIN (Py (ev->area.y), Py (ev->area.y + ev->area.height + 1));
  region.Y2 = MAX (Py (ev->area.y), Py (ev->area.y + ev->area.height + 1));

  eleft = Vx (0);
  eright = Vx (PCB->MaxWidth);
  etop = Vy (0);
  ebottom = Vy (PCB->MaxHeight);
  if (eleft > eright)
    {
      int tmp = eleft;
      eleft = eright;
      eright = tmp;
    }
  if (etop > ebottom)
    {
      int tmp = etop;
      etop = ebottom;
      ebottom = tmp;
    }

  glColor3f (gport->offlimits_color.red / 65535.,
             gport->offlimits_color.green / 65535.,
             gport->offlimits_color.blue / 65535.);

  if (eleft > 0)
    {
      glBegin (GL_QUADS);
      glVertex2i (0, 0);
      glVertex2i (eleft, 0);
      glVertex2i (eleft, gport->height);
      glVertex2i (0, gport->height);
      glEnd ();
    }
  else
    eleft = 0;

  if (eright < gport->width)
    {
      glBegin (GL_QUADS);
      glVertex2i (eright, 0);
      glVertex2i (gport->width, 0);
      glVertex2i (gport->width, gport->height);
      glVertex2i (eright, gport->width);
      glEnd ();
    }
  else
    eright = gport->width;
  if (etop > 0)
    {
      glBegin (GL_QUADS);
      glVertex2i (eleft, 0);
      glVertex2i (eright, 0);
      glVertex2i (eright, etop);
      glVertex2i (eleft, etop);
      glEnd ();
    }
  if (ebottom < gport->height)
    {
      glBegin (GL_QUADS);
      glVertex2i (eleft, ebottom);
      glVertex2i (eright + 1, ebottom);
      glVertex2i (eright + 1, gport->height);
      glVertex2i (eleft, gport->height);
      glEnd ();
    }

  /* TODO: Background image */

  hidgl_init_triangle_array (&buffer);
  ghid_invalidate_current_gc ();

  glPushMatrix ();
  glScalef ((ghid_flip_x ? -1. : 1.) / gport->zoom,
            (ghid_flip_y ? -1. : 1.) / gport->zoom, 1);
  glTranslatef (ghid_flip_x ? gport->view_x0 - PCB->MaxWidth  :
                             -gport->view_x0,
                ghid_flip_y ? gport->view_y0 - PCB->MaxHeight :
                             -gport->view_y0, 0);

  gdk_region_get_rectangles (ev->region, &rectangles, &n_rectangles);
  for (i = 0; i < n_rectangles; i++) {
    int x, y, width, height;

    x = rectangles[i].x;
    y = rectangles[i].y;
    width = rectangles[i].width;
    height = rectangles[i].height;

    region.X1 = MIN (Px (x), Px (x + width + 1));
    region.X2 = MAX (Px (x), Px (x + width + 1));
    region.Y1 = MIN (Py (y), Py (y + height + 1));
    region.Y2 = MAX (Py (y), Py (y + height + 1));

    hid_expose_callback (&ghid_hid, &region, 0);
  }
  g_free (rectangles);


  hidgl_flush_triangles (&buffer);
  glPopMatrix ();


  glUnmapBuffer (GL_ARRAY_BUFFER);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDeleteBuffers (1, &buffer.vbo_name);

  draw_grid ();

  hidgl_init_triangle_array (&buffer);
  ghid_invalidate_current_gc ();
  glPushMatrix ();
  glScalef ((ghid_flip_x ? -1. : 1.) / gport->zoom,
            (ghid_flip_y ? -1. : 1.) / gport->zoom, 1);
  glTranslatef (ghid_flip_x ? gport->view_x0 - PCB->MaxWidth  :
                             -gport->view_x0,
                ghid_flip_y ? gport->view_y0 - PCB->MaxHeight :
                             -gport->view_y0, 0);
  DrawAttached (TRUE);
  DrawMark (TRUE);
  hidgl_flush_triangles (&buffer);
  glPopMatrix ();

  ghid_show_crosshair (TRUE);

  hidgl_flush_triangles (&buffer);

  glUnmapBuffer (GL_ARRAY_BUFFER);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDeleteBuffers (1, &buffer.vbo_name);

  if (gdk_gl_drawable_is_double_buffered (pGlDrawable))
    gdk_gl_drawable_swap_buffers (pGlDrawable);
  else
    glFlush ();

  /* end drawing to current GL-context */
  gdk_gl_drawable_gl_end (pGlDrawable);

  return FALSE;
}

gint
ghid_port_window_motion_cb (GtkWidget * widget,
			    GdkEventButton * ev, GHidPort * out)
{
  ModifierKeysState mk;
  gdouble dx, dy;
  static gint x_prev = -1, y_prev = -1;
  gboolean moved;
  GdkModifierType state;
#if 0
  GdkGLContext* pGlContext = gtk_widget_get_gl_context (widget);
  GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (widget);

  /* make GL-context "current" */
  if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext)) {
    printf ("GL THingy returned\n");
    return FALSE;
  }
#endif

  state = (GdkModifierType) (ev->state);
  mk = ghid_modifier_keys_state (&state);

  if ((ev->state & GDK_BUTTON3_MASK) == GDK_BUTTON3_MASK
      && mk == NONE_PRESSED)
    {
/* Not sure this really matters, since we're using invalidate to get a redraw */
#if 1
      if (gtk_events_pending ())
	return FALSE;
#endif
      dx = gport->zoom * (x_prev - ev->x);
      dy = gport->zoom * (y_prev - ev->y);
      if (x_prev > 0)
	ghid_port_ranges_pan (dx, dy, TRUE);
      x_prev = ev->x;
      y_prev = ev->y;
      return FALSE;
    }
  x_prev = y_prev = -1;
  moved = ghid_note_event_location (ev);

//  ghid_show_crosshair (TRUE);
  if (moved) // && have_crosshair_attachments ())
    ghid_draw_area_update (gport, NULL);

#if 0
  if (gdk_gl_drawable_is_double_buffered (pGlDrawable))
    gdk_gl_drawable_swap_buffers (pGlDrawable);
  else
    glFlush ();

  /* end drawing to current GL-context */
  gdk_gl_drawable_gl_end (pGlDrawable);
#endif

  return FALSE;
}

gint
ghid_port_window_enter_cb (GtkWidget * widget,
			   GdkEventCrossing * ev, GHidPort * out)
{
  /* printf("enter: mode: %d detail: %d\n", ev->mode, ev->detail); */

  /* See comment in ghid_port_window_leave_cb() */

  if(ev->mode != GDK_CROSSING_NORMAL && ev->detail != GDK_NOTIFY_NONLINEAR) 
    {
      return FALSE;
    }

  if (!ghidgui->command_entry_status_line_active)
    {
      out->has_entered = TRUE;
      /* Make sure drawing area has keyboard focus when we are in it.
       */
      gtk_widget_grab_focus (out->drawing_area);
    }
  ghidgui->in_popup = FALSE;

  /* Following expression is true if a you open a menu from the menu bar, 
   * move the mouse to the viewport and click on it. This closes the menu 
   * and moves the pointer to the viewport without the pointer going over 
   * the edge of the viewport */
  if(ev->mode == GDK_CROSSING_UNGRAB && ev->detail == GDK_NOTIFY_NONLINEAR)
    {
      ghid_screen_update ();
    }

  if(! cursor_in_viewport)
    {
      RestoreCrosshair (TRUE);
      cursor_in_viewport = TRUE;
    }

  return FALSE;
}

static gboolean
ghid_pan_idle_cb (gpointer data)
{
  gdouble dx = 0, dy = 0;

  if (gport->has_entered)
    return FALSE;
  dy = gport->zoom * y_pan_speed;
  dx = gport->zoom * x_pan_speed;
  return (ghid_port_ranges_pan (dx, dy, TRUE));
}

gint
ghid_port_window_leave_cb (GtkWidget * widget, 
                           GdkEventCrossing * ev, GHidPort * out)
{
  gint x0, y0, x, y, dx, dy, w, h;
  
  /* printf("leave mode: %d detail: %d\n", ev->mode, ev->detail); */

  /* Window leave events can also be triggered because of focus grabs. Some
   * X applications occasionally grab the focus and so trigger this function.
   * At least GNOME's window manager is known to do this on every mouse click.
   *
   * See http://bugzilla.gnome.org/show_bug.cgi?id=102209 
   */

  if(ev->mode != GDK_CROSSING_NORMAL)
    {
      return FALSE;
    }

  if(out->has_entered && !ghidgui->in_popup)
    {
      /* if actively drawing, start scrolling */

      if (have_crosshair_attachments () && ghidgui->auto_pan_on)
	{
	  /* GdkEvent coords are set to 0,0 at leave events, so must figure
	     |  out edge the cursor left.
	   */
	  w = ghid_port.width * gport->zoom;
	  h = ghid_port.height * gport->zoom;

	  x0 = VIEW_X (0);
	  y0 = VIEW_Y (0);
	  ghid_get_coords (NULL, &x, &y);
	  x -= x0;
	  y -= y0;

	  if (ghid_flip_x )
	      x = -x;
	  if (ghid_flip_y )
	      y = -y;

	  dx = w - x;
	  dy = h - y;

	  x_pan_speed = y_pan_speed = 2 * ghidgui->auto_pan_speed;

	  if (x < dx)
	    {
	      x_pan_speed = -x_pan_speed;
	      dx = x;
	    }
	  if (y < dy)
	    {
	      y_pan_speed = -y_pan_speed;
	      dy = y;
	    }
	  if (dx < dy)
	    {
	      if (dy < h / 3)
		y_pan_speed = y_pan_speed - (3 * dy * y_pan_speed) / h;
	      else
		y_pan_speed = 0;
	    }
	  else
	    {
	      if (dx < w / 3)
		x_pan_speed = x_pan_speed - (3 * dx * x_pan_speed) / w;
	      else
		x_pan_speed = 0;
	    }
	  g_idle_add (ghid_pan_idle_cb, NULL);
	}
    }

  if(cursor_in_viewport)
    {
      HideCrosshair (TRUE);
      cursor_in_viewport = FALSE;
    }

  ghid_show_crosshair (FALSE);
  out->has_entered = FALSE;

  ghid_screen_update ();

  return FALSE;
}


  /* Mouse scroll wheel events
   */
gint
ghid_port_window_mouse_scroll_cb (GtkWidget * widget,
				  GdkEventScroll * ev, GHidPort * out)
{
  ModifierKeysState mk;
  gdouble dx = 0.0, dy = 0.0, zoom_factor;
  GdkModifierType state;

  state = (GdkModifierType) (ev->state);
  mk = ghid_modifier_keys_state (&state);
  if (mk == NONE_PRESSED)
    {
      zoom_factor = (ev->direction == GDK_SCROLL_UP) ? 0.8 : 1.25;
      ghid_port_ranges_zoom (gport->zoom * zoom_factor);
      return TRUE;
    }

  if (mk == SHIFT_PRESSED)
    dy = ghid_port.height * gport->zoom / 40;
  else
    dx = ghid_port.width * gport->zoom / 40;

  if (ev->direction == GDK_SCROLL_UP)
    {
      dx = -dx;
      dy = -dy;
    }

  HideCrosshair (FALSE);
  ghid_port_ranges_pan (dx, dy, TRUE);
  MoveCrosshairRelative (dx, dy);
  AdjustAttachedObjects ();
  RestoreCrosshair (FALSE);

  return TRUE;
}
