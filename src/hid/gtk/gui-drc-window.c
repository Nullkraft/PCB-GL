/* $Id$ */

/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 1994,1995,1996 Thomas Nau
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
 *  Contact addresses for paper mail and Email:
 *  Thomas Nau, Schlehenweg 15, 88471 Baustetten, Germany
 *  Thomas.Nau@rz.uni-ulm.de
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gui.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

RCSID ("$Id$");

static GtkWidget *drc_window, *drc_list;


/* Remember user window resizes. */
static gint
drc_window_configure_event_cb (GtkWidget * widget,
			       GdkEventConfigure * ev, gpointer data)
{
  ghidgui->drc_window_width = widget->allocation.width;
  ghidgui->drc_window_height = widget->allocation.height;
  ghidgui->config_modified = TRUE;

  return FALSE;
}

static void
drc_close_cb (gpointer data)
{
  gtk_widget_destroy (drc_window);
  drc_window = NULL;
}

static void
drc_destroy_cb (GtkWidget * widget, gpointer data)
{
  drc_window = NULL;
}

void
ghid_drc_window_show (gboolean raise)
{
  GtkWidget *vbox, *hbox, *button;

  if (drc_window)
    {
      if (raise)
	gtk_window_present(GTK_WINDOW(drc_window));
      return;
    }

  drc_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (drc_window), "destroy",
		    G_CALLBACK (drc_destroy_cb), NULL);
  g_signal_connect (G_OBJECT (drc_window), "configure_event",
		    G_CALLBACK (drc_window_configure_event_cb), NULL);
  gtk_window_set_title (GTK_WINDOW (drc_window), _("PCB DRC"));
  gtk_window_set_wmclass (GTK_WINDOW (drc_window), "PCB_DRC", "PCB");
  gtk_window_set_default_size (GTK_WINDOW (drc_window),
			       ghidgui->drc_window_width,
			       ghidgui->drc_window_height);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  gtk_container_add (GTK_CONTAINER (drc_window), vbox);

  drc_list = ghid_scrolled_text_view (vbox, NULL,
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);

  hbox = gtk_hbutton_box_new ();
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  g_signal_connect (G_OBJECT (button), "clicked",
		    G_CALLBACK (drc_close_cb), NULL);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  gtk_widget_realize (drc_window);
  if (Settings.AutoPlace)
    gtk_widget_set_uposition (GTK_WIDGET (drc_window), 10, 10);
  gtk_widget_show_all (drc_window);
}

static void
ghid_drc_append_violation (gchar * s)
{
  ghid_drc_window_show (FALSE);
}

void ghid_drc_window_reset_message (void)
{
  printf ("RESET DRC WINDOW\n");
}

#if 0
void ghid_drc_window_append_message (const char *fmt, ...)
{
  va_list va;
  va_start (va, fmt);
  vprintf (fmt, va);
  va_end (va);
}
#endif

void ghid_drc_window_append_messagev (const char *fmt, va_list va)
{
  vprintf (fmt, va);
}

int ghid_drc_window_throw_dialog (void)
{
  printf ("THROW DRC WINDOW\n");
  ghid_drc_window_show (TRUE);
}

