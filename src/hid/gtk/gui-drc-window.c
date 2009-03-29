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

#include "error.h"
#include "search.h"
#include "draw.h"
#include "undo.h"
#include "gui.h"
#include "gui-drc-window.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

RCSID ("$Id$");

static GtkWidget *drc_window, *drc_list;
static GtkListStore *drc_list_model = NULL;
static int num_violations = 0;

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

enum {
  DRC_VIOLATION_NUM_COL = 0,
  DRC_TITLE_COL,
  DRC_EXPLANATION_COL,
  DRC_X_COORD_COL,
  DRC_Y_COORD_COL,
  DRC_ANGLE_COL,
  DRC_HAVE_MEASURED_COL,
  DRC_MEASURED_VALUE_COL,
  DRC_REQUIRED_VALUE_COL,
  DRC_VALUE_DIGITS_COL,
  DRC_VALUE_UNITS_COL,
  DRC_OBJECT_COUNT_COL,
  DRC_OBJECT_ID_LIST_COL,
  DRC_OBJECT_TYPE_LIST_COL,
  NUM_DRC_COLUMNS
};


#warning NASTY, DOESNT USE UNDO PROPERLY
static void
unset_found_flags (int AndDraw)
{
  int flag = FOUNDFLAG;
  int change = 0;

  VIA_LOOP (PCB->Data);
  {
    if (TEST_FLAG (flag, via))
      {
        if (AndDraw)
          AddObjectToFlagUndoList (VIA_TYPE, via, via, via);
        CLEAR_FLAG (flag, via);
        if (AndDraw)
          DrawVia (via, 0);
        change = True;
      }
  }
  END_LOOP;
  ELEMENT_LOOP (PCB->Data);
  {
    PIN_LOOP (element);
    {
      if (TEST_FLAG (flag, pin))
        {
          if (AndDraw)
            AddObjectToFlagUndoList (PIN_TYPE, element, pin, pin);
          CLEAR_FLAG (flag, pin);
          if (AndDraw)
            DrawPin (pin, 0);
          change = True;
        }
    }
    END_LOOP;
    PAD_LOOP (element);
    {
      if (TEST_FLAG (flag, pad))
        {
          if (AndDraw)
            AddObjectToFlagUndoList (PAD_TYPE, element, pad, pad);
          CLEAR_FLAG (flag, pad);
          if (AndDraw)
            DrawPad (pad, 0);
          change = True;
        }
    }
    END_LOOP;
  }
  END_LOOP;
  RAT_LOOP (PCB->Data);
  {
    if (TEST_FLAG (flag, line))
      {
        if (AndDraw)
          AddObjectToFlagUndoList (RATLINE_TYPE, line, line, line);
        CLEAR_FLAG (flag, line);
        if (AndDraw)
          DrawRat (line, 0);
        change = True;
      }
  }
  END_LOOP;
  COPPERLINE_LOOP (PCB->Data);
  {
    if (TEST_FLAG (flag, line))
      {
        if (AndDraw)
          AddObjectToFlagUndoList (LINE_TYPE, layer, line, line);
        CLEAR_FLAG (flag, line);
        if (AndDraw)
          DrawLine (layer, line, 0);
        change = True;
      }
  }
  ENDALL_LOOP;
  COPPERARC_LOOP (PCB->Data);
  {
    if (TEST_FLAG (flag, arc))
      {
        if (AndDraw)
          AddObjectToFlagUndoList (ARC_TYPE, layer, arc, arc);
        CLEAR_FLAG (flag, arc);
        if (AndDraw)
          DrawArc (layer, arc, 0);
        change = True;
      }
  }
  ENDALL_LOOP;
  COPPERPOLYGON_LOOP (PCB->Data);
  {
    if (TEST_FLAG (flag, polygon))
      {
        if (AndDraw)
          AddObjectToFlagUndoList (POLYGON_TYPE, layer, polygon, polygon);
        CLEAR_FLAG (flag, polygon);
        if (AndDraw)
          DrawPolygon (layer, polygon, 0);
        change = True;
      }
  }
  ENDALL_LOOP;
  if (change)
    {
      SetChangedFlag (True);
      if (AndDraw)
        {
          IncrementUndoSerialNumber ();
          Draw ();
        }
    }
}

static void
selection_changed_cb (GtkTreeSelection *selection, gpointer user_data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  int object_count;
#if 0
  long int *object_id_list;
  int *object_type_list;
#else
  long int object_id;
  int object_type;
#endif
  int i;
  int x_coord, y_coord;

#warning CHEATING HERE!
  /* I happen to know I'm only putting 0 or 1 items in the lists of objects.
     To do this properly requires some memory allocation and tracking to
     avoid leaks! */

  /* Check there is anything selected, if not; return */
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* Unflag objects */
      unset_found_flags (True);
      return;
    }

  /* Check the selected node has children, if so; return. */
  if (gtk_tree_model_iter_has_child (model, &iter))
    return;

  gtk_tree_model_get (model, &iter,
                      DRC_X_COORD_COL,          &x_coord,
                      DRC_Y_COORD_COL,          &y_coord,
                      DRC_OBJECT_COUNT_COL,     &object_count,
#if 0
                      DRC_OBJECT_ID_LIST_COL,   &object_id_list,
                      DRC_OBJECT_TYPE_LIST_COL, &object_type_list,
#else
                      DRC_OBJECT_ID_LIST_COL,   &object_id,
                      DRC_OBJECT_TYPE_LIST_COL, &object_type,
#endif
                      -1);

  unset_found_flags (False);

  /* Flag the objects listed against this DRC violation */
  for (i = 0; i < object_count; i++)
    {
      int found_type;
      void *ptr1, *ptr2, *ptr3;

      found_type = SearchObjectByID (PCB->Data, &ptr1, &ptr2, &ptr3,
                                     object_id, object_type);
      if (found_type == NO_TYPE)
        {
          Message (_("Object ID %i identified during DRC was not found. Stale DRC window?\n"),
                   object_id);
          continue;
        }
      SET_FLAG (FOUNDFLAG, (AnyObjectType *)ptr2);
      switch (object_type)
        {
        case LINE_TYPE:
        case ARC_TYPE:
        case POLYGON_TYPE:
          ChangeGroupVisibility (GetLayerNumber (PCB->Data, (LayerTypePtr) ptr1), True, True);
        }
      DrawObject (object_type, ptr1, ptr2, 0);
    }
  Draw();
  CenterDisplay (x_coord, y_coord, False);
}


enum
{
  PROP_TITLE = 1,
  PROP_EXPLANATION,
  PROP_X_COORD,
  PROP_Y_COORD,
  PROP_ANGLE,
  PROP_HAVE_MEASURED,
  PROP_MEASURED_VALUE,
  PROP_REQUIRED_VALUE,
  PROP_VALUE_DIGITS,
  PROP_VALUE_UNITS
};


static GObjectClass *ghid_violation_renderer_parent_class = NULL;


/*! \brief GObject finalise handler
 *
 *  \par Function Description
 *  Just before the GhidViolationRenderer GObject is finalized, free our
 *  allocated data, and then chain up to the parent's finalize handler.
 *
 *  \param [in] widget  The GObject being finalized.
 */
static void
ghid_violation_renderer_finalize (GObject * object)
{
  GhidViolationRenderer *renderer = GHID_VIOLATION_RENDERER (object);

  g_free (renderer->title);
  g_free (renderer->explanation);
  g_free (renderer->value_units);

  G_OBJECT_CLASS (ghid_violation_renderer_parent_class)->finalize (object);
}


/*! \brief GObject property setter function
 *
 *  \par Function Description
 *  Setter function for GhidViolationRenderer's GObject properties,
 *  "settings-name" and "toplevel".
 *
 *  \param [in]  object       The GObject whose properties we are setting
 *  \param [in]  property_id  The numeric id. under which the property was
 *                            registered with g_object_class_install_property()
 *  \param [in]  value        The GValue the property is being set from
 *  \param [in]  pspec        A GParamSpec describing the property being set
 */
static void
ghid_violation_renderer_set_property (GObject * object, guint property_id,
				  const GValue * value, GParamSpec * pspec)
{
  GhidViolationRenderer *renderer = GHID_VIOLATION_RENDERER (object);
  char *markup;

  switch (property_id)
    {
    case PROP_TITLE:
      g_free (renderer->title);
      renderer->title = g_value_dup_string (value);
      break;
    case PROP_EXPLANATION:
      g_free (renderer->explanation);
      renderer->explanation = g_value_dup_string (value);
      break;
    case PROP_X_COORD:
      renderer->x_coord = g_value_get_int (value);
      break;
    case PROP_Y_COORD:
      renderer->y_coord = g_value_get_int (value);
      break;
    case PROP_ANGLE:
      renderer->angle = g_value_get_int (value);
      break;
    case PROP_HAVE_MEASURED:
      renderer->have_measured = g_value_get_int (value);
      break;
    case PROP_MEASURED_VALUE:
      renderer->measured_value = g_value_get_double (value);
      break;
    case PROP_REQUIRED_VALUE:
      renderer->required_value = g_value_get_double (value);
      break;
    case PROP_VALUE_DIGITS:
      renderer->value_digits = g_value_get_int (value);
      break;
    case PROP_VALUE_UNITS:
      g_free (renderer->value_units);
      renderer->value_units = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      return;
    }

  if (renderer->have_measured)
    {
      markup = g_strdup_printf ("<b>%s (%.*f %s)</b>\n"
                                "<span font_size='1024'> </span>\n"
                                "<small>"
                                  "<i>%s</i>\n"
                                  "<span font_size='5120'> </span>\n"
                                  "Required: %.*f %s"
                                "</small>",
                                renderer->title,
                                renderer->value_digits,
                                renderer->measured_value,
                                renderer->value_units,

                                renderer->explanation,

                                renderer->value_digits,
                                renderer->required_value,
                                renderer->value_units);
    }
  else
    {
      markup = g_strdup_printf ("<b>%s</b>\n"
                                "<span font_size='1024'> </span>\n"
                                "<small>"
                                  "<i>%s</i>\n"
                                  "<span font_size='5120'> </span>\n"
                                  "Required: %.*f %s"
                                "</small>",
                                renderer->title,

                                renderer->explanation,

                                renderer->value_digits,
                                renderer->required_value,
                                renderer->value_units);
    }

  g_object_set (object, "markup", markup, NULL);
  g_free (markup);
}


/*! \brief GObject property getter function
 *
 *  \par Function Description
 *  Getter function for GhidViolationRenderer's GObject properties,
 *  "settings-name" and "toplevel".
 *
 *  \param [in]  object       The GObject whose properties we are getting
 *  \param [in]  property_id  The numeric id. under which the property was
 *                            registered with g_object_class_install_property()
 *  \param [out] value        The GValue in which to return the value of the property
 *  \param [in]  pspec        A GParamSpec describing the property being got
 */
static void
ghid_violation_renderer_get_property (GObject * object, guint property_id,
				  GValue * value, GParamSpec * pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }

}


/*! \brief GType class initialiser for GhidViolationRenderer
 *
 *  \par Function Description
 *  GType class initialiser for GhidViolationRenderer. We override our parent
 *  virtual class methods as needed and register our GObject properties.
 *
 *  \param [in]  klass       The GhidViolationRendererClass we are initialising
 */
static void
ghid_violation_renderer_class_init (GhidViolationRendererClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = ghid_violation_renderer_finalize;
  gobject_class->set_property = ghid_violation_renderer_set_property;
  gobject_class->get_property = ghid_violation_renderer_get_property;

  ghid_violation_renderer_parent_class = g_type_class_peek_parent (klass);

  g_object_class_install_property (gobject_class, PROP_TITLE,
				   g_param_spec_string ("title",
							"",
							"",
							"",
							G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class, PROP_EXPLANATION,
				   g_param_spec_string ("explanation",
							"",
							"",
							"",
							G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class, PROP_X_COORD,
				   g_param_spec_int ("x-coord",
						     "",
						     "",
						     G_MININT,
						     G_MAXINT,
						     0,
						     G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class, PROP_Y_COORD,
				   g_param_spec_int ("y-coord",
						     "",
						     "",
						     G_MININT,
						     G_MAXINT,
						     0,
						     G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class, PROP_ANGLE,
				   g_param_spec_int ("angle",
						     "",
						     "",
						     G_MININT,
						     G_MAXINT,
						     0,
						     G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class, PROP_HAVE_MEASURED,
				   g_param_spec_int ("have-measured",
						     "",
						     "",
						     G_MININT,
						     G_MAXINT,
						     0,
						     G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class, PROP_MEASURED_VALUE,
				   g_param_spec_double ("measured-value",
						     "",
						     "",
						     -G_MAXDOUBLE,
						     G_MAXDOUBLE,
						     0.,
						     G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class, PROP_REQUIRED_VALUE,
				   g_param_spec_double ("required-value",
						     "",
						     "",
						     -G_MINDOUBLE,
						     G_MAXDOUBLE,
						     0.,
						     G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class, PROP_VALUE_DIGITS,
				   g_param_spec_int ("value-digits",
						     "",
						     "",
						     0,
						     G_MAXINT,
						     0,
						     G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class, PROP_VALUE_UNITS,
				   g_param_spec_string ("value-units",
							"",
							"",
							"",
							G_PARAM_WRITABLE));
}


/*! \brief Function to retrieve GhidViolationRenderer's GType identifier.
 *
 *  \par Function Description
 *  Function to retrieve GhidViolationRenderer's GType identifier.
 *  Upon first call, this registers the GhidViolationRenderer in the GType system.
 *  Subsequently it returns the saved value from its first execution.
 *
 *  \return the GType identifier associated with GhidViolationRenderer.
 */
GType
ghid_violation_renderer_get_type ()
{
  static GType ghid_violation_renderer_type = 0;

  if (!ghid_violation_renderer_type)
    {
      static const GTypeInfo ghid_violation_renderer_info = {
	sizeof (GhidViolationRendererClass),
	NULL,			/* base_init */
	NULL,			/* base_finalize */
	(GClassInitFunc) ghid_violation_renderer_class_init,
	NULL,			/* class_finalize */
	NULL,			/* class_data */
	sizeof (GhidViolationRenderer),
	0,			/* n_preallocs */
	NULL,			/* instance_init */
      };

      ghid_violation_renderer_type =
	g_type_register_static (GTK_TYPE_CELL_RENDERER_TEXT, "GhidViolationRenderer",
				&ghid_violation_renderer_info, 0);
    }

  return ghid_violation_renderer_type;
}


/*! \brief Convenience function to create a new violation renderer
 *
 *  \par Function Description
 *  Convenience function which creates a GhidViolationRenderer.
 *
 *  \return  The GhidViolationRenderer created.
 */
GtkCellRenderer *
ghid_violation_renderer_new (void)
{
  GhidViolationRenderer *renderer;

  renderer = g_object_new (GHID_TYPE_VIOLATION_RENDERER,
                           "ypad", 6,
                           NULL);

  return GTK_CELL_RENDERER (renderer);
//  return gtk_cell_renderer_text_new ();
}


void
ghid_drc_window_show (gboolean raise)
{
  GtkWidget *vbox, *hbox, *button, *scrolled_window;
  GtkCellRenderer *violation_renderer;

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

  drc_list_model = gtk_list_store_new (NUM_DRC_COLUMNS,
                                       G_TYPE_INT,      /* DRC_VIOLATION_NUM_COL    */
                                       G_TYPE_STRING,   /* DRC_TITLE_COL            */
                                       G_TYPE_STRING,   /* DRC_EXPLANATION_COL      */
                                       G_TYPE_INT,      /* DRC_X_COORD_COL          */
                                       G_TYPE_INT,      /* DRC_Y_COORD_COL          */
                                       G_TYPE_INT,      /* DRC_ANGLE_COL            */
                                       G_TYPE_INT,      /* DRC_HAVE_MEASURED_COL    */
                                       G_TYPE_DOUBLE,   /* DRC_MEASURED_VALUE_COL   */
                                       G_TYPE_DOUBLE,   /* DRC_REQUIRED_VALUE_COL   */
                                       G_TYPE_INT,      /* DRC_VALUE_DIGITS_COL     */
                                       G_TYPE_STRING,   /* DRC_VALUE_UNITS_COL      */
                                       G_TYPE_INT,      /* DRC_OBJECT_COUNT_COL     */
#warning CHEATING HERE!
  /* I happen to know I'm only putting 0 or 1 items in the lists of objects.
     To do this properly requires some memory allocation and tracking to
     avoid leaks! */
#if 0
                                       G_TYPE_POINTER,  /* DRC_OBJECT_ID_LIST_COL   */
                                       G_TYPE_POINTER); /* DRC_OBJECT_TYPE_LIST_COL */
#else
                                       G_TYPE_LONG,     /* DRC_OBJECT_ID_LIST_COL   */
                                       G_TYPE_INT);     /* DRC_OBJECT_TYPE_LIST_COL */
#endif

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled_window,
                      TRUE /* EXPAND */, TRUE /* FILL */, 0 /* PADDING */);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  drc_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (drc_list_model));
  gtk_container_add (GTK_CONTAINER (scrolled_window), drc_list);

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (drc_list), TRUE);
  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (drc_list)), "changed",
                    G_CALLBACK (selection_changed_cb), NULL);

  violation_renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (drc_list),
                                               -1, /* APPEND */
                                               _("No."), /* TITLE */
                                               violation_renderer,
                                               "text",           DRC_VIOLATION_NUM_COL,
                                                NULL);

  violation_renderer = ghid_violation_renderer_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (drc_list),
                                               -1, /* APPEND */
                                               _("Violation details"), /* TITLE */
                                               violation_renderer,
                                               "title",          DRC_TITLE_COL,
                                               "explanation",    DRC_EXPLANATION_COL,
                                               "x-coord",        DRC_X_COORD_COL,
                                               "y-coord",        DRC_Y_COORD_COL,
                                               "angle",          DRC_ANGLE_COL,
                                               "have-measured",  DRC_HAVE_MEASURED_COL,
                                               "measured-value", DRC_MEASURED_VALUE_COL,
                                               "required-value", DRC_REQUIRED_VALUE_COL,
                                               "value-digits",   DRC_VALUE_DIGITS_COL,
                                               "value-units",    DRC_VALUE_UNITS_COL,
                                                NULL);

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

#define UNIT1(value) (Settings.grid_units_mm ? ((value) / 100000.0 * 25.4) : ((value) / 100.0))
#define UNIT(value) UNIT1(value) , (Settings.grid_units_mm ? "mm" : "mils")

void ghid_drc_window_append_violation (DRC_VIOLATION *violation)
{
  GtkTreeIter iter;
  long int ID = 0;
  int type = NO_TYPE;

  /* Ensure the required structures are setup */
  ghid_drc_window_show (FALSE);

  num_violations++;

  /* Copy the violating object lists */
#warning CHEATING HERE!
  /* I happen to know I'm only putting 0 or 1 items in the lists of objects.
     To do this properly requires some memory allocation and tracking to
     avoid leaks! */
  if (violation->object_count > 0)
    {
      ID = violation->object_id_list[0];
      type = violation->object_type_list[0];
    }

  gtk_list_store_append (drc_list_model, &iter);
  gtk_list_store_set (drc_list_model, &iter,
                      DRC_VIOLATION_NUM_COL,    num_violations,
                      DRC_TITLE_COL,            violation->title,
                      DRC_EXPLANATION_COL,      violation->explanation,
                      DRC_X_COORD_COL,          violation->x,
                      DRC_Y_COORD_COL,          violation->y,
                      DRC_ANGLE_COL,            violation->angle,
                      DRC_HAVE_MEASURED_COL,    violation->have_measured,
                      DRC_MEASURED_VALUE_COL,   violation->measured_value,
                      DRC_REQUIRED_VALUE_COL,   violation->required_value,
                      DRC_VALUE_DIGITS_COL,     violation->value_digits,
                      DRC_VALUE_UNITS_COL,      violation->value_units,
                      DRC_OBJECT_COUNT_COL,     violation->object_count,
                      DRC_OBJECT_ID_LIST_COL,   ID, /* violation->object_id_list, */
                      DRC_OBJECT_TYPE_LIST_COL, type, /* violation->object_type_list, */
                      -1);
}

void ghid_drc_window_reset_message (void)
{
  // printf ("RESET DRC WINDOW\n");
  if (drc_list_model != NULL)
    gtk_list_store_clear (drc_list_model);
  num_violations = 0;
}

int ghid_drc_window_throw_dialog (void)
{
  // printf ("THROW DRC WINDOW\n");
  ghid_drc_window_show (TRUE);
  return 1;
}

