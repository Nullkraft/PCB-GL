/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of pcb.
 *
 *   Copyright (C) 2000-2003 Stefan Petersen (spe@stacken.kth.se)
 *   Copyright (C) 2012      PCB Contributors (See Changelog for details.)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

/** \file draw-pcb.c
    \brief Render a gerbv image to PCB native objects
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h> /* M_PI */

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "gerbv.h"
#include "gerber_import.h"

#ifdef DEBUG
#   define dprintf printf
#else
#   define dprintf if (0) printf
#endif

void
draw_pcb_line_to (gdouble x, gdouble y) {
//	gdouble x1 = x, y1 = y;
//	cairo_line_to (cairoTarget, x1, y1);
}

void
draw_pcb_move_to (gdouble x, gdouble y) {
//	gdouble x1 = x, y1 = y;
//	cairo_move_to (cairoTarget, x1, y1);
}

void
draw_pcb_translate_adjust (gdouble x, gdouble y) {
//	gdouble x1 = x, y1 = y;
//	cairo_translate (cairoTarget, x1, y1);
}

static void
draw_fill (gchar drawMode, gerbv_selection_info_t *selectionInfo,
           gerbv_image_t *image, struct gerbv_net *net){
//	if (drawMode == DRAW_IMAGE)
//		cairo_fill (cairoTarget);
}

static void
draw_stroke (gchar drawMode, gerbv_selection_info_t *selectionInfo,
             gerbv_image_t *image, struct gerbv_net *net){
//	if (drawMode == DRAW_IMAGE)
//		cairo_stroke (cairoTarget);
}

/*
 * Draws a circle _centered_ at x,y with diameter dia
 */
static void 
gerbv_draw_circle(gdouble diameter)
{
//    cairo_arc (cairoTarget, 0.0, 0.0, diameter/2.0, 0, 2.0*M_PI);
    return;
}


/*
 * Draws a rectangle _centered_ at x,y with sides x_side, y_side
 */
static void
gerbv_draw_rectangle(gdouble width1, gdouble height1)
{
//	gdouble width = width1, height = height1;
//	cairo_rectangle (cairoTarget, - width / 2.0, - height / 2.0, width, height);
	return;
}


/*
 * Draws an oblong _centered_ at x,y with x axis x_axis and y axis y_axis
 */ 
static void
gerbv_draw_oblong(gdouble width, gdouble height)
{
#if 0
    /* --- This stuff produces a line + rounded ends --- */
    gdouble circleDiameter, strokeDistance;

    cairo_new_path (cairoTarget);
    if (width < height) {
    	circleDiameter = width;
    	strokeDistance = (height - width)/2.0;
    	cairo_arc (cairoTarget, 0.0, strokeDistance, circleDiameter/2.0, 0, -M_PI);
    	cairo_line_to (cairoTarget, -circleDiameter/2.0, -strokeDistance);
    	cairo_arc (cairoTarget, 0.0, -strokeDistance, circleDiameter/2.0, -M_PI, 0);
    	cairo_line_to (cairoTarget, circleDiameter/2.0, strokeDistance);
    }
    else {
    	circleDiameter = height;
    	strokeDistance = (width - height)/2.0;
    	cairo_arc (cairoTarget, -strokeDistance, 0.0, circleDiameter/2.0, M_PI/2.0, -M_PI/2.0);
    	cairo_line_to (cairoTarget, strokeDistance, -circleDiameter/2.0);
    	cairo_arc (cairoTarget, strokeDistance, 0.0, circleDiameter/2.0, -M_PI/2.0, M_PI/2.0);
    	cairo_line_to (cairoTarget, -strokeDistance, circleDiameter/2.0);
    }
#endif
    /*  --- This stuff produces an oval pad --- */
    /* cairo doesn't have a function to draw ovals, so we must
     * draw an arc and stretch it by scaling different x and y values
    cairo_save (cairoTarget);
    cairo_scale (cairoTarget, width, height);
    gerbv_draw_circle (cairoTarget, 1);
    cairo_restore (cairoTarget);
    */
    return;
}


static void
gerbv_draw_polygon(gdouble outsideDiameter, gdouble numberOfSides, gdouble degreesOfRotation)
{
#if 0
    int i, numberOfSidesInteger = (int) numberOfSides;

    cairo_rotate(cairoTarget, degreesOfRotation * M_PI/180);
    cairo_move_to(cairoTarget, outsideDiameter / 2.0, 0);
    /* skip first point, since we've moved there already */
    /* include last point, since we may be drawing an aperture hole next
       and cairo may not correctly close the path itself */
    for (i = 1; i <= (int)numberOfSidesInteger; i++){
	gdouble angle = (double) i / numberOfSidesInteger * M_PI * 2.0;
	cairo_line_to (cairoTarget, cos(angle) * outsideDiameter / 2.0,
		       sin(angle) * outsideDiameter / 2.0);
    }
#endif
    return;
}


static void 
gerbv_draw_aperature_hole(gdouble dimensionX, gdouble dimensionY)
{
#if 0
    if (dimensionX) {
	if (dimensionY) {
	    gerbv_draw_rectangle (cairoTarget, dimensionX, dimensionY);
	} else {
	    gerbv_draw_circle (cairoTarget, dimensionX);
	}
    }
    return;
#endif
}

static gboolean
draw_update_macro_exposure (cairo_operator_t clearOperator,
                            cairo_operator_t darkOperator, gdouble exposureSetting){

#if 0
	if (exposureSetting == 0.0) {
		cairo_set_operator (cairoTarget, clearOperator);
	}
	else if (exposureSetting == 1.0) {
		cairo_set_operator (cairoTarget, darkOperator);
	}
	else if (exposureSetting == 2.0) {
		/* reverse current exposure setting */
		cairo_operator_t currentOperator = cairo_get_operator (cairoTarget);
		if (currentOperator == clearOperator) {
			cairo_set_operator (cairoTarget, darkOperator);
		}
		else {
			cairo_set_operator (cairoTarget, clearOperator);
		}
	}
#endif
	return TRUE;
}

	    
static int
gerbv_draw_amacro(cairo_operator_t clearOperator,
	cairo_operator_t darkOperator, gerbv_simplified_amacro_t *s,
	gint usesClearPrimative, gdouble pixelWidth, gchar drawMode,
	gerbv_selection_info_t *selectionInfo,
	gerbv_image_t *image, struct gerbv_net *net)
{
    int handled = 1;  
    gerbv_simplified_amacro_t *ls = s;

    dprintf("Drawing simplified aperture macros:\n");
//    if (usesClearPrimative)
//    	cairo_push_group (cairoTarget);
    while (ls != NULL) {
	    /* 
	     * This handles the exposure thing in the aperture macro
	     * The exposure is always the first element on stack independent
	     * of aperture macro.
	     */
#if 0
	    cairo_save (cairoTarget);
	    cairo_new_path(cairoTarget);
	    cairo_operator_t oldOperator = cairo_get_operator (cairoTarget);
#endif

	    if (ls->type == GERBV_APTYPE_MACRO_CIRCLE) {
	    	
	      if (draw_update_macro_exposure (clearOperator, 
	      		darkOperator, ls->parameter[CIRCLE_EXPOSURE])){
//			cairo_translate (cairoTarget, ls->parameter[CIRCLE_CENTER_X],
//			                 ls->parameter[CIRCLE_CENTER_Y]);

			gerbv_draw_circle (ls->parameter[CIRCLE_DIAMETER]);
			draw_fill (drawMode, selectionInfo, image, net);
		}
	    } else if (ls->type == GERBV_APTYPE_MACRO_OUTLINE) {
		int pointCounter,numberOfPoints;
		/* Number of points parameter seems to not include the start point,
		 * so we add one to include the start point.
		 */
		numberOfPoints = (int) ls->parameter[OUTLINE_NUMBER_OF_POINTS] + 1;
		
		if (draw_update_macro_exposure (clearOperator, 
					darkOperator, ls->parameter[OUTLINE_EXPOSURE])){
//			cairo_rotate (cairoTarget, ls->parameter[(numberOfPoints - 1) * 2 + OUTLINE_ROTATION] * M_PI/180.0);
//			cairo_move_to (cairoTarget, ls->parameter[OUTLINE_FIRST_X], ls->parameter[OUTLINE_FIRST_Y]);
			
			for (pointCounter=0; pointCounter < numberOfPoints; pointCounter++) {
//			    cairo_line_to (cairoTarget, ls->parameter[pointCounter * 2 + OUTLINE_FIRST_X],
//					   ls->parameter[pointCounter * 2 + OUTLINE_FIRST_Y]);
			}
			/* although the gerber specs allow for an open outline,
			   I interpret it to mean the outline should be closed by the
			   rendering softare automatically, since there is no dimension
			   for line thickness.
			*/
			draw_fill (drawMode, selectionInfo, image, net);
		}
	    } else if (ls->type == GERBV_APTYPE_MACRO_POLYGON) {
	      if (draw_update_macro_exposure (clearOperator, 
	      			darkOperator, ls->parameter[POLYGON_EXPOSURE])){
//			cairo_translate (cairoTarget, ls->parameter[POLYGON_CENTER_X],
//				       ls->parameter[POLYGON_CENTER_Y]);
			gerbv_draw_polygon(ls->parameter[POLYGON_DIAMETER],
					   ls->parameter[POLYGON_NUMBER_OF_POINTS], ls->parameter[POLYGON_ROTATION]);
			draw_fill (drawMode, selectionInfo, image, net);
		}
	    } else if (ls->type == GERBV_APTYPE_MACRO_MOIRE) {
		gdouble diameter, diameterDifference;
		int circleIndex;
		
//		cairo_translate (cairoTarget, ls->parameter[MOIRE_CENTER_X],
//			       ls->parameter[MOIRE_CENTER_Y]);
//		cairo_rotate (cairoTarget, ls->parameter[MOIRE_ROTATION] * M_PI/180);
		diameter = ls->parameter[MOIRE_OUTSIDE_DIAMETER] -  ls->parameter[MOIRE_CIRCLE_THICKNESS];
		diameterDifference = 2*(ls->parameter[MOIRE_GAP_WIDTH] +
			ls->parameter[MOIRE_CIRCLE_THICKNESS]);
//		cairo_set_line_width (cairoTarget, ls->parameter[MOIRE_CIRCLE_THICKNESS]);
		
		for (circleIndex = 0; circleIndex < (int)ls->parameter[MOIRE_NUMBER_OF_CIRCLES];  circleIndex++) {
		    gdouble currentDiameter = (diameter - diameterDifference * (float) circleIndex);
		    if (currentDiameter >= 0){
				gerbv_draw_circle (currentDiameter);
				draw_stroke (drawMode, selectionInfo, image, net);
		    }
		}
		
//		gdouble crosshairRadius = (ls->parameter[MOIRE_CROSSHAIR_LENGTH] / 2.0);
		
//		cairo_set_line_width (cairoTarget, ls->parameter[MOIRE_CROSSHAIR_THICKNESS]);
//		cairo_move_to (cairoTarget, -crosshairRadius, 0);
//		cairo_line_to (cairoTarget, crosshairRadius, 0);    	
//		cairo_move_to (cairoTarget, 0, -crosshairRadius);
//		cairo_line_to (cairoTarget, 0, crosshairRadius);
		draw_stroke (drawMode, selectionInfo, image, net);
	    } else if (ls->type == GERBV_APTYPE_MACRO_THERMAL) {
		gint i;
//		gdouble startAngle1, startAngle2, endAngle1, endAngle2;

//		cairo_translate (cairoTarget, ls->parameter[THERMAL_CENTER_X],
//			       ls->parameter[THERMAL_CENTER_Y]);
//		cairo_rotate (cairoTarget, ls->parameter[THERMAL_ROTATION] * M_PI/180.0);
//		startAngle1 = atan (ls->parameter[THERMAL_CROSSHAIR_THICKNESS]/ls->parameter[THERMAL_INSIDE_DIAMETER]);
//		endAngle1 = M_PI/2 - startAngle1;
//		endAngle2 = atan (ls->parameter[THERMAL_CROSSHAIR_THICKNESS]/ls->parameter[THERMAL_OUTSIDE_DIAMETER]);
//		startAngle2 = M_PI/2 - endAngle2;
		for (i = 0; i < 4; i++) {
//			cairo_arc (cairoTarget, 0, 0, ls->parameter[THERMAL_INSIDE_DIAMETER]/2.0, startAngle1, endAngle1);
//			cairo_rel_line_to (cairoTarget, 0, ls->parameter[THERMAL_CROSSHAIR_THICKNESS]);
//			cairo_arc_negative (cairoTarget, 0, 0, ls->parameter[THERMAL_OUTSIDE_DIAMETER]/2.0,
//				startAngle2, endAngle2);
//			cairo_rel_line_to (cairoTarget, -ls->parameter[THERMAL_CROSSHAIR_THICKNESS],0);
//			draw_fill (cairoTarget, drawMode, selectionInfo, image, net);
//			cairo_rotate (cairoTarget, 90 * M_PI/180);
		}
	    } else if (ls->type == GERBV_APTYPE_MACRO_LINE20) {
	      if (draw_update_macro_exposure (clearOperator, 
	      			darkOperator, ls->parameter[LINE20_EXPOSURE])){
	      	gdouble cParameter = ls->parameter[LINE20_LINE_WIDTH];
			if (cParameter < pixelWidth)
				cParameter = pixelWidth;
				
//			cairo_set_line_width (cairoTarget, cParameter);
//			cairo_set_line_cap (cairoTarget, CAIRO_LINE_CAP_BUTT);
//			cairo_rotate (cairoTarget, ls->parameter[LINE20_ROTATION] * M_PI/180.0);
//			cairo_move_to (cairoTarget, ls->parameter[LINE20_START_X], ls->parameter[LINE20_START_Y]);
//			cairo_line_to (cairoTarget, ls->parameter[LINE20_END_X], ls->parameter[LINE20_END_Y]);
			draw_stroke (drawMode, selectionInfo, image, net);
		}
	    } else if (ls->type == GERBV_APTYPE_MACRO_LINE21) {
		gdouble halfWidth, halfHeight;
		
		if (draw_update_macro_exposure (clearOperator,
						darkOperator, ls->parameter[LINE22_EXPOSURE])){
			halfWidth = ls->parameter[LINE21_WIDTH] / 2.0;
			halfHeight = ls->parameter[LINE21_HEIGHT] / 2.0;
			if (halfWidth < pixelWidth)
				halfWidth = pixelWidth;
			if (halfHeight < pixelWidth)
				halfHeight = pixelWidth;		
//			cairo_translate (cairoTarget, ls->parameter[LINE21_CENTER_X], ls->parameter[LINE21_CENTER_Y]);
//			cairo_rotate (cairoTarget, ls->parameter[LINE21_ROTATION] * M_PI/180.0);
//			cairo_rectangle (cairoTarget, -halfWidth, -halfHeight,
//					 ls->parameter[LINE21_WIDTH], ls->parameter[LINE21_HEIGHT]);
			draw_fill (drawMode, selectionInfo, image, net);
		}	
	    } else if (ls->type == GERBV_APTYPE_MACRO_LINE22) {
	    	gdouble halfWidth, halfHeight;
		
		if (draw_update_macro_exposure (clearOperator,
					darkOperator, ls->parameter[LINE22_EXPOSURE])){
			halfWidth = ls->parameter[LINE22_WIDTH] / 2.0;
			halfHeight = ls->parameter[LINE22_HEIGHT] / 2.0;
			if (halfWidth < pixelWidth)
				halfWidth = pixelWidth;
			if (halfHeight < pixelWidth)
				halfHeight = pixelWidth;
//			cairo_translate (cairoTarget, ls->parameter[LINE22_LOWER_LEFT_X],
//					ls->parameter[LINE22_LOWER_LEFT_Y]);
//			cairo_rotate (cairoTarget, ls->parameter[LINE22_ROTATION] * M_PI/180.0);
//			cairo_rectangle (cairoTarget, 0, 0,
//					ls->parameter[LINE22_WIDTH], ls->parameter[LINE22_HEIGHT]);
			draw_fill (drawMode, selectionInfo, image, net);
		}
	    } else {
		handled = 0;
	    }
//	    cairo_set_operator (cairoTarget, oldOperator);
//	    cairo_restore (cairoTarget);
	    ls = ls->next;
    }
    if (usesClearPrimative) {
//        cairo_pop_group_to_source (cairoTarget);
//      cairo_paint (cairoTarget);
    }
    return handled;
} /* gerbv_draw_amacro */


static void
draw_apply_netstate_transformation (gerbv_netstate_t *state) 
{
	/* apply scale factor */
//	cairo_scale (cairoTarget, state->scaleA, state->scaleB);
	/* apply offset */
//	cairo_translate (cairoTarget, state->offsetA, state->offsetB);
	/* apply mirror */
	switch (state->mirrorState) {
		case GERBV_MIRROR_STATE_FLIPA:
//			cairo_scale (cairoTarget, -1, 1);
			break;
		case GERBV_MIRROR_STATE_FLIPB:
//			cairo_scale (cairoTarget, 1, -1);
			break;
		case GERBV_MIRROR_STATE_FLIPAB:
//			cairo_scale (cairoTarget, -1, -1);
			break;
		default:
			break;
	}
	/* finally, apply axis select */
	if (state->axisSelect == GERBV_AXIS_SELECT_SWAPAB) {
		/* we do this by rotating 270 (counterclockwise, then mirroring
		   the Y axis */
//		cairo_rotate (cairoTarget, 3 * M_PI / 2);
//		cairo_scale (cairoTarget, 1, -1);
	}
}

static void
draw_render_polygon_object (gerbv_net_t *oldNet, gdouble sr_x, gdouble sr_y,
		gerbv_image_t *image, gchar drawMode, gerbv_selection_info_t *selectionInfo) {
	gerbv_net_t *currentNet, *polygonStartNet;
	int haveDrawnFirstFillPoint = 0;
	gdouble x2,y2;
//	gdouble cp_x=0,cp_y=0;
	
	haveDrawnFirstFillPoint = FALSE;
	/* save the first net in the polygon as the "ID" net pointer
	   in case we are saving this net to the selection array */
	polygonStartNet = oldNet;
//	cairo_new_path(cairoTarget);
		
	for (currentNet = oldNet->next; currentNet!=NULL; currentNet = currentNet->next){
		x2 = currentNet->stop_x + sr_x;
		y2 = currentNet->stop_y + sr_y;
           
		/* translate circular x,y data as well */
#if 0
		if (currentNet->cirseg) {
			cp_x = currentNet->cirseg->cp_x + sr_x;
			cp_y = currentNet->cirseg->cp_y + sr_y;
		}
#endif
		if (!haveDrawnFirstFillPoint) {
			draw_pcb_move_to (x2, y2);
			haveDrawnFirstFillPoint=TRUE;
			continue;
		}
		switch (currentNet->interpolation) {
			case GERBV_INTERPOLATION_x10 :
			case GERBV_INTERPOLATION_LINEARx01 :
			case GERBV_INTERPOLATION_LINEARx001 :
			case GERBV_INTERPOLATION_LINEARx1 :
				draw_pcb_line_to (x2, y2);
				break;
			case GERBV_INTERPOLATION_CW_CIRCULAR :
			case GERBV_INTERPOLATION_CCW_CIRCULAR :
				if (currentNet->cirseg->angle2 > currentNet->cirseg->angle1) {
//					cairo_arc (cairoTarget, cp_x, cp_y, currentNet->cirseg->width/2.0,
//						currentNet->cirseg->angle1 * M_PI/180,currentNet->cirseg->angle2 * M_PI/180);
				}
				else {
//					cairo_arc_negative (cairoTarget, cp_x, cp_y, currentNet->cirseg->width/2.0,
//						currentNet->cirseg->angle1 * M_PI/180,currentNet->cirseg->angle2 * M_PI/180);
				}
				break;
			case GERBV_INTERPOLATION_PAREA_END :
//				cairo_close_path(cairoTarget);
				/* turn off anti-aliasing for polygons, since it shows seams
				   with adjacent polygons (usually on PCB ground planes) */
//				cairo_antialias_t oldAlias = cairo_get_antialias (cairoTarget);
//				cairo_set_antialias (cairoTarget, CAIRO_ANTIALIAS_NONE);
				draw_fill (drawMode, selectionInfo, image, polygonStartNet);
//				cairo_set_antialias (cairoTarget, oldAlias);
				return;
			default :
				break;
		}
	}
}



int
draw_image_to_pcb_target (gerbv_image_t *image,
                          gdouble pixelWidth,
                          gchar drawMode, gerbv_selection_info_t *selectionInfo,
                          gerbv_render_info_t *renderInfo, gboolean allowOptimization,
                          gerbv_user_transformation_t transform) {
	struct gerbv_net *net;
	double x1, y1, x2, y2;
//	double cp_x=0, cp_y=0;
	gdouble p1, p2, p3, p4, p5, dx, dy; //, lineWidth;
	gerbv_netstate_t *oldState;
	gerbv_layer_t *oldLayer;
	int repeat_X=1, repeat_Y=1;
	double repeat_dist_X = 0, repeat_dist_Y = 0;
	int repeat_i, repeat_j;
	cairo_operator_t drawOperatorClear, drawOperatorDark;
	gboolean invertPolarity = FALSE;
//	gdouble criticalRadius;
	gdouble scaleX = transform.scaleX;
	gdouble scaleY = transform.scaleY;
	
	if (transform.mirrorAroundX)
		scaleY *= -1;
	if (transform.mirrorAroundY)
		scaleX *= -1;
//	cairo_translate (cairoTarget, transform.translateX, transform.translateY);
//	cairo_scale (cairoTarget, scaleX, scaleY);
//	cairo_rotate (cairoTarget, transform.rotation);

    /* do initial justify */
//	cairo_translate (cairoTarget, image->info->imageJustifyOffsetActualA,
//		 image->info->imageJustifyOffsetActualB);

    /* set the fill rule so aperture holes are cleared correctly */	 
//    cairo_set_fill_rule (cairoTarget, CAIRO_FILL_RULE_EVEN_ODD);
    /* offset image */
//    cairo_translate (cairoTarget, image->info->offsetA, image->info->offsetB);
    /* do image rotation */
//    cairo_rotate (cairoTarget, image->info->imageRotation);
    /* load in polarity operators depending on the image polarity */
    invertPolarity = transform.inverted;
    if (image->info->polarity == GERBV_POLARITY_NEGATIVE)
    	invertPolarity = !invertPolarity;

    if (invertPolarity) {
		drawOperatorClear = CAIRO_OPERATOR_OVER;
		drawOperatorDark = CAIRO_OPERATOR_CLEAR;
//		cairo_set_operator (cairoTarget, CAIRO_OPERATOR_OVER);
//		cairo_paint (cairoTarget);
//		cairo_set_operator (cairoTarget, CAIRO_OPERATOR_CLEAR);
    }
    else {
		drawOperatorClear = CAIRO_OPERATOR_CLEAR;
		drawOperatorDark = CAIRO_OPERATOR_OVER;
    }
    /* next, push two cairo states to simulate the first layer and netstate
       translations (these will be popped when another layer or netstate is
       started */

//    cairo_save (cairoTarget);
//    cairo_save (cairoTarget);
    /* store the current layer and netstate so we know when they change */
    oldLayer = image->layers;
    oldState = image->states;

    for (net = image->netlist->next ; net != NULL; net = gerbv_image_return_next_renderable_object(net)) {

	/* check if this is a new layer */
	if (net->layer != oldLayer){
		/* it's a new layer, so recalculate the new transformation matrix
		   for it */
//		cairo_restore (cairoTarget);
//		cairo_restore (cairoTarget);
//		cairo_save (cairoTarget);
		/* do any rotations */
//		cairo_rotate (cairoTarget, net->layer->rotation);
		/* handle the layer polarity */
		if ((net->layer->polarity == GERBV_POLARITY_CLEAR)^invertPolarity) {
//			cairo_set_operator (cairoTarget, CAIRO_OPERATOR_CLEAR);
			drawOperatorClear = CAIRO_OPERATOR_OVER;
    			drawOperatorDark = CAIRO_OPERATOR_CLEAR;
		}
		else {
//			cairo_set_operator (cairoTarget, CAIRO_OPERATOR_OVER);
			drawOperatorClear = CAIRO_OPERATOR_CLEAR;
    			drawOperatorDark = CAIRO_OPERATOR_OVER;
		}
		/* check for changes to step and repeat */
		repeat_X = net->layer->stepAndRepeat.X;
		repeat_Y = net->layer->stepAndRepeat.Y;
		repeat_dist_X = net->layer->stepAndRepeat.dist_X;
		repeat_dist_Y = net->layer->stepAndRepeat.dist_Y;
		/* draw any knockout areas */
		if (net->layer->knockout.firstInstance == TRUE) {
//			cairo_operator_t oldOperator = cairo_get_operator (cairoTarget);
			if (net->layer->knockout.polarity == GERBV_POLARITY_CLEAR) {
//				cairo_set_operator (cairoTarget, drawOperatorClear);
			}
			else {
//				cairo_set_operator (cairoTarget, drawOperatorDark);
			}
//			cairo_new_path (cairoTarget);
//			cairo_rectangle (cairoTarget, net->layer->knockout.lowerLeftX - net->layer->knockout.border,
//					net->layer->knockout.lowerLeftY - net->layer->knockout.border,
//					net->layer->knockout.width + (net->layer->knockout.border*2),
//					net->layer->knockout.height + (net->layer->knockout.border*2));
			draw_fill (drawMode, selectionInfo, image, net);
//			cairo_set_operator (cairoTarget, oldOperator);
		}
		/* finally, reapply old netstate transformation */
//		cairo_save (cairoTarget);
		draw_apply_netstate_transformation (net->state);
		oldLayer = net->layer;
	}
	/* check if this is a new netstate */
	if (net->state != oldState){
		/* pop the transformation matrix back to the "pre-state" state and
		   resave it */
//		cairo_restore (cairoTarget);
//		cairo_save (cairoTarget);
		/* it's a new state, so recalculate the new transformation matrix
		   for it */
		draw_apply_netstate_transformation (net->state);
		oldState = net->state;	
	}
	for(repeat_i = 0; repeat_i < repeat_X; repeat_i++) {
	    for(repeat_j = 0; repeat_j < repeat_Y; repeat_j++) {
		double sr_x = repeat_i * repeat_dist_X;
		double sr_y = repeat_j * repeat_dist_Y;
		
		x1 = net->start_x + sr_x;
		y1 = net->start_y + sr_y;
		x2 = net->stop_x + sr_x;
		y2 = net->stop_y + sr_y;
           
		/* translate circular x,y data as well */
#if 0
		if (net->cirseg) {
			cp_x = net->cirseg->cp_x + sr_x;
			cp_y = net->cirseg->cp_y + sr_y;
		}
#endif
		
		/* render any labels attached to this net */
		/* NOTE: this is currently only used on PNP files, so we may
		   make some assumptions here... */
		if (net->label) {
//			cairo_set_font_size (cairoTarget, 0.05);
//			cairo_save (cairoTarget);
			
//			cairo_move_to (cairoTarget, x1, y1);
//			cairo_scale (cairoTarget, 1, -1);
//			cairo_show_text (cairoTarget, net->label->str);
//			cairo_restore (cairoTarget);
		}
		/*
		* Polygon Area Fill routines
		*/
		switch (net->interpolation) {
			case GERBV_INTERPOLATION_PAREA_START :
				draw_render_polygon_object (net, sr_x, sr_y, image, drawMode, selectionInfo);
				continue;
			case GERBV_INTERPOLATION_DELETED:
				continue;
			default :
				break;
		}
	
		/*
		 * If aperture state is off we allow use of undefined apertures.
		 * This happens when gerber files starts, but hasn't decided on 
		 * which aperture to use.
		 */
		if (image->aperture[net->aperture] == NULL) {
		  /* Commenting this out since it gets emitted every time you click on the screen 
		  if (net->aperture_state != GERBV_APERTURE_STATE_OFF)
		    GERB_MESSAGE("Aperture D%d is not defined\n", net->aperture);
		  */
		  continue;
		}
		switch (net->aperture_state) {
			case GERBV_APERTURE_STATE_ON :
				/* if the aperture width is truly 0, then render as a 1 pixel width
				   line.  0 diameter apertures are used by some programs to draw labels,
				   etc, and they are rendered by other programs as 1 pixel wide */
				/* NOTE: also, make sure all lines are at least 1 pixel wide, so they
				   always show up at low zoom levels */
				
//				criticalRadius = image->aperture[net->aperture]->parameter[0]/2.0;
//				lineWidth = criticalRadius*2.0;
				// convert to a pixel integer
//				cairo_set_line_width (cairoTarget, lineWidth);
				switch (net->interpolation) {
					case GERBV_INTERPOLATION_x10 :
					case GERBV_INTERPOLATION_LINEARx01 :
					case GERBV_INTERPOLATION_LINEARx001 :
					case GERBV_INTERPOLATION_LINEARx1 :
//						cairo_set_line_cap (cairoTarget, CAIRO_LINE_CAP_ROUND);
						// weed out any lines that are obviously not going to render on the
						//   visible screen

						switch (image->aperture[net->aperture]->type) {
							case GERBV_APTYPE_CIRCLE :
								draw_pcb_move_to (x1, y1);
								draw_pcb_line_to (x2, y2);
								draw_stroke (drawMode, selectionInfo, image, net);
								break;
							case GERBV_APTYPE_RECTANGLE :				
								dx = (image->aperture[net->aperture]->parameter[0]/ 2);
								dy = (image->aperture[net->aperture]->parameter[1]/ 2);
								if(x1 > x2)
									dx = -dx;
								if(y1 > y2)
									dy = -dy;
//								cairo_new_path(cairoTarget);
								draw_pcb_move_to (x1 - dx, y1 - dy);
								draw_pcb_line_to (x1 - dx, y1 + dy);
								draw_pcb_line_to (x2 - dx, y2 + dy);
								draw_pcb_line_to (x2 + dx, y2 + dy);
								draw_pcb_line_to (x2 + dx, y2 - dy);
								draw_pcb_line_to (x1 + dx, y1 - dy);
								draw_fill (drawMode, selectionInfo, image, net);
								break;
							/* for now, just render ovals or polygons like a circle */
							case GERBV_APTYPE_OVAL :
							case GERBV_APTYPE_POLYGON :
								draw_pcb_move_to (x1,y1);
								draw_pcb_line_to (x2,y2);
								draw_stroke (drawMode, selectionInfo, image, net);
								break;
							/* macros can only be flashed, so ignore any that might be here */
							default :
								break;
						}
						break;
					case GERBV_INTERPOLATION_CW_CIRCULAR :
					case GERBV_INTERPOLATION_CCW_CIRCULAR :
						/* cairo doesn't have a function to draw oval arcs, so we must
						 * draw an arc and stretch it by scaling different x and y values
						 */
//						cairo_new_path(cairoTarget);
						if (image->aperture[net->aperture]->type == GERBV_APTYPE_RECTANGLE) {
//							cairo_set_line_cap (cairoTarget, CAIRO_LINE_CAP_SQUARE);
						}
						else {
//							cairo_set_line_cap (cairoTarget, CAIRO_LINE_CAP_ROUND);
						}
//						cairo_save (cairoTarget);
//						cairo_translate(cairoTarget, cp_x, cp_y);
//						cairo_scale (cairoTarget, net->cirseg->width, net->cirseg->height);
						if (net->cirseg->angle2 > net->cirseg->angle1) {
//							cairo_arc (cairoTarget, 0.0, 0.0, 0.5, net->cirseg->angle1 * M_PI/180,
//								net->cirseg->angle2 * M_PI/180);
						}
						else {
//							cairo_arc_negative (cairoTarget, 0.0, 0.0, 0.5, net->cirseg->angle1 * M_PI/180,
//								net->cirseg->angle2 * M_PI/180);
						}
//						cairo_restore (cairoTarget);
						draw_stroke (drawMode, selectionInfo, image, net);
						break;
					default :
						break;
				}
				break;
			case GERBV_APERTURE_STATE_OFF :
				break;
			case GERBV_APERTURE_STATE_FLASH :
				p1 = image->aperture[net->aperture]->parameter[0];
				p2 = image->aperture[net->aperture]->parameter[1];
				p3 = image->aperture[net->aperture]->parameter[2];
				p4 = image->aperture[net->aperture]->parameter[3];
				p5 = image->aperture[net->aperture]->parameter[4];

//				cairo_save (cairoTarget);
				draw_pcb_translate_adjust(x2, y2);
				
				switch (image->aperture[net->aperture]->type) {
					case GERBV_APTYPE_CIRCLE :
						gerbv_draw_circle(p1);
						gerbv_draw_aperature_hole (p2, p3);
						break;
					case GERBV_APTYPE_RECTANGLE :
						// some CAD programs use very thin flashed rectangles to compose
						//	logos/images, so we must make sure those display here
						gerbv_draw_rectangle(p1, p2);
						gerbv_draw_aperature_hole (p3, p4);
						break;
					case GERBV_APTYPE_OVAL :
						gerbv_draw_oblong(p1, p2);
						gerbv_draw_aperature_hole (p3, p4);
						break;
					case GERBV_APTYPE_POLYGON :
						gerbv_draw_polygon(p1, p2, p3);
						gerbv_draw_aperature_hole (p4, p5);
						break;
					case GERBV_APTYPE_MACRO :
						gerbv_draw_amacro(drawOperatorClear, drawOperatorDark,
								  image->aperture[net->aperture]->simplified,
								  (int) image->aperture[net->aperture]->parameter[0], pixelWidth,
								  drawMode, selectionInfo, image, net);
						break;   
					default :
						GERB_MESSAGE("Unknown aperture type\n");
						return 0;
				}
				/* and finally fill the path */
				draw_fill (drawMode, selectionInfo, image, net);
//				cairo_restore (cairoTarget);
				break;
			default:
				GERB_MESSAGE("Unknown aperture state\n");
				return 0;
		}
	    }
	}
    }

    /* restore the initial two state saves (one for layer, one for netstate)*/
//    cairo_restore (cairoTarget);
//    cairo_restore (cairoTarget);

    return 1;
}

