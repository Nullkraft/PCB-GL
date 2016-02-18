/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 2011 PCB Contributors (See ChangeLog for details)
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

#include "global.h"

#include "gui.h"

#include "gui-pinout-preview.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

static void
set_default_page_attributes (GtkAssistant *assistant, GtkWidget *page)
{
  gtk_assistant_set_page_title (assistant, page, _("BGA Footprint Wizard"));
}

static GtkWidget *
create_intro_page (GtkAssistant *assistant)
{
  GtkWidget *page;
  page = gtk_label_new ("Hello world");

  gtk_assistant_append_page (assistant, page);
  gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_INTRO);
  set_default_page_attributes (assistant, page);
  gtk_assistant_set_page_complete (assistant, page, TRUE);

  return page;
}

static GtkWidget *
create_step1_page (GtkAssistant *assistant)
{
  GtkWidget *page;
  GtkWidget *hbox;
  GtkWidget *preview_frame;
  GtkWidget *preview;
  GtkWidget *vbox;
  GtkWidget *label;

  hbox = gtk_hbox_new (FALSE, 0);

  preview_frame = gtk_frame_new (_("Preview"));
  preview = ghid_pinout_preview_new (NULL);
  gtk_container_add (GTK_CONTAINER (preview_frame), preview);

  vbox = gtk_vbox_new (FALSE, 8);

  label = gtk_label_new (_("Define the parameters for this BGA"));
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
  label = gtk_label_new (_("Parameter 1"));
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
  label = gtk_label_new (_("Parameter 2"));
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
  label = gtk_label_new (_("Parameter 3"));
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);


  gtk_box_pack_start (GTK_BOX (hbox), preview_frame, TRUE, TRUE, 8);
  gtk_box_pack_start (GTK_BOX (hbox),          vbox, TRUE, TRUE, 8);

  page = hbox;

  gtk_assistant_append_page (assistant, page);
  set_default_page_attributes (assistant, page);
  gtk_assistant_set_page_complete (assistant, page, TRUE);

  return page;
}

static void
assistant_close_cb (GtkAssistant *assistant, gpointer userdata)
{
  gtk_widget_destroy (GTK_WIDGET (assistant));
}

static void
assistant_cancel_cb (GtkAssistant *assistant, gpointer userdata)
{
  gtk_widget_destroy (GTK_WIDGET (assistant));
}

static gboolean
assistant_delete_event_cb (GtkWidget *assistant, gpointer userdata)
{
  gtk_widget_destroy (assistant);
  return TRUE;
}

void
demo_assistant (void)
{
  GtkWidget *assistant;

  assistant = gtk_assistant_new ();
  create_intro_page (GTK_ASSISTANT (assistant));
  create_step1_page (GTK_ASSISTANT (assistant));

  g_signal_connect (assistant, "close", G_CALLBACK (assistant_close_cb), NULL);
  g_signal_connect (assistant, "cancel", G_CALLBACK (assistant_cancel_cb), NULL);
  g_signal_connect (assistant, "delete-event", G_CALLBACK (assistant_delete_event_cb), NULL);

  gtk_widget_show_all (assistant);
}


static const char bga_wizard_syntax[] = "BgaWizard()";

static const char bga_wizard_help[] =
"Activate the BGA footprint wizard.";

int
bga_wizard_action (int argc, char **argv, int px, int pz)
{
  if (!ghidgui || !ghidgui->menu_bar)
    return 0;

  demo_assistant ();
  return 0;
}

HID_Action bga_wizard_action_list[] = {
  {"BgaWizard", 0, bga_wizard_action,
   bga_wizard_help, bga_wizard_syntax},
};

REGISTER_ACTIONS (bga_wizard_action_list)


