/*
 * "$Id$"
 *
 *   Main window code for Print plug-in for the GIMP.
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com),
 *	Robert Krawitz (rlk@alum.mit.edu), Steve Miller (smiller@rni.net)
 *      and Michael Natterer (mitch@gimp.org)
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the Free
 *   Software Foundation; either version 2 of the License, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *   for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "../../lib/libprintut.h"

#define MAX_PREVIEW_PPI        (400)
#define INCH 72
#define FINCH ((gdouble) INCH)
#define ROUNDUP(x, y) (((x) + ((y) - 1)) / (y))
#define SCALE(x, y) (((x) + (1.0 / (2.0 * (y)))) * (y))

#include <gimp-print/gimp-print-intl-internal.h>
#include <gimp-print/gimp-print-ui.h>
#include "gimp-print-ui-internal.h"

#include <string.h>
#include <stdio.h>

#define MAXIMUM_PARAMETER_LEVEL STP_PARAMETER_LEVEL_ADVANCED4

/*
 * Constants for GUI.
 */
static int preview_size_vert = 360;
static int preview_size_horiz = 300;
static const int minimum_image_percent = 5.0;
static const int thumbnail_hintw = 128;
static const int thumbnail_hinth = 128;

#define MOVE_CONSTRAIN	   0
#define MOVE_HORIZONTAL	   1
#define MOVE_VERTICAL      2
#define MOVE_ANY           (MOVE_HORIZONTAL | MOVE_VERTICAL)

/*
 *  Main window widgets
 */

static GtkWidget *main_vbox;
static GtkWidget *main_hbox;
static GtkWidget *right_vbox;
static GtkWidget *notebook;

static GtkWidget *output_color_vbox;
static GtkWidget *cyan_button;
static GtkWidget *magenta_button;
static GtkWidget *yellow_button;
static GtkWidget *black_button;

static GtkWidget *print_dialog;           /* Print dialog window */

static GtkWidget *recenter_button;
static GtkWidget *recenter_vertical_button;
static GtkWidget *recenter_horizontal_button;

static GtkWidget *left_entry;
/*
static GtkWidget *right_entry;
*/
static GtkWidget *right_border_entry;
static GtkWidget *top_entry;
/*
static GtkWidget *bottom_entry;
*/
static GtkWidget *bottom_border_entry;
static GtkWidget *width_entry;
static GtkWidget *height_entry;
static GtkWidget *units_hbox;
static GtkWidget *units_label;

static GtkWidget *custom_size_width;
static GtkWidget *custom_size_height;
static GtkWidget *show_all_paper_sizes_button;

static GtkWidget *orientation_menu;    /* Orientation menu */

static GtkWidget *scaling_percent;     /* Scale by percent */
static GtkWidget *scaling_ppi;         /* Scale by pixels-per-inch */
static GtkWidget *scaling_image;       /* Scale to the image */
static GtkObject *scaling_adjustment;  /* Adjustment object for scaling */

static GtkWidget *setup_dialog;        /* Setup dialog window */
static GtkWidget *printer_driver;      /* Printer driver widget */
static GtkWidget *printer_model_label; /* Printer model name */
static GtkWidget *printer_crawler;     /* Scrolled Window for menu */
static GtkWidget *printer_combo;       /* Combo for menu */
static gint plist_callback_id = -1;
static GtkWidget *ppd_file;            /* PPD file entry */
static GtkWidget *ppd_box;
static GtkWidget *ppd_label;           /* PPD file entry */
static GtkWidget *ppd_button;          /* PPD file browse button */
static GtkWidget *output_cmd;          /* Output command text entry */
static GtkWidget *ppd_browser;         /* File selection dialog for PPDs */
static GtkWidget *new_printer_dialog;  /* New printer dialog window */
static GtkWidget *new_printer_entry;   /* New printer text entry */
static GtkWidget *file_browser;        /* FSD for print files */

static GtkWidget *adjust_color_button;
static GtkWidget *about_dialog;

static GtkWidget *page_size_table;
static GtkWidget *printer_features_table;
static GtkWidget *color_adjustment_table;

static gboolean preview_valid = FALSE;
static gboolean frame_valid = FALSE;
static gboolean need_exposure = FALSE;
static gboolean suppress_scaling_adjustment = FALSE;
static gboolean suppress_scaling_callback   = FALSE;
/*
 * These are semaphores, not true booleans.
 */
static gint suppress_preview_update = 0;
static gint suppress_preview_reset = 0;

static GtkDrawingArea *preview = NULL;	/* Preview drawing area widget */
static GtkDrawingArea *swatch = NULL;
static gint mouse_x, mouse_y;		/* Last mouse position */
static gint orig_top, orig_left;	/* Original mouse position at start */
static gint buttons_pressed = 0;
static gint preview_active = 0;
static gint buttons_mask = 0;
static gint move_constraint = 0;
static gint mouse_button = -1;	/* Button being dragged with */
static gint preview_thumbnail_w, preview_thumbnail_h;
static gint preview_x, preview_y, preview_w, preview_h;

static gint physical_orientation = -2; /* Actual orientation */

static gint paper_width, paper_height; /* Physical width */
static gint printable_width, printable_height;	/* Size of printable area */
static gint print_width, print_height; /* Printed area of image */
static gint left, right, top, bottom; /* Imageable region */
static gint image_width, image_height; /* Image size (possibly rotated) */
static gint image_true_width, image_true_height; /* Original image */
static gdouble image_xres, image_yres; /* Original image resolution */
static gint do_update_thumbnail = 0;
static gint saveme = 0;		/* True if printrc should be saved */
static gint runme = 0;		/* True if print should proceed */



static void scaling_update        (GtkAdjustment *adjustment);
static void scaling_callback      (GtkWidget *widget);
static void plist_callback        (GtkWidget *widget, gpointer data);
static void custom_media_size_callback(GtkWidget *widget, gpointer data);
static void show_all_paper_sizes_callback(GtkWidget *widget, gpointer data);
static void combo_callback        (GtkWidget *widget, gpointer data);
static void output_type_callback  (GtkWidget *widget, gpointer data);
static void unit_callback         (GtkWidget *widget, gpointer data);
static void orientation_callback  (GtkWidget *widget, gpointer data);
static void printandsave_callback (void);
static void about_callback        (void);
static void print_callback        (void);
static void save_callback         (void);

static void setup_update          (void);
static void setup_open_callback   (void);
static void setup_ok_callback     (void);
static void new_printer_open_callback   (void);
static void new_printer_ok_callback     (void);
static void ppd_browse_callback   (void);
static void ppd_ok_callback       (void);
static void print_driver_callback (GtkWidget      *widget,
				   gint            row,
				   gint            column,
				   GdkEventButton *event,
				   gpointer        data);

static void file_ok_callback      (void);
static void file_cancel_callback  (void);
static void do_preview_thumbnail (void);
static void invalidate_preview_thumbnail (void);
static void invalidate_frame (void);

static GtkWidget *color_adjust_dialog;

static void preview_update              (void);
static void preview_expose              (void);
static void preview_button_callback     (GtkWidget      *widget,
					 GdkEventButton *bevent,
					 gpointer        data);
static void preview_motion_callback     (GtkWidget      *widget,
					 GdkEventMotion *mevent,
					 gpointer        data);
static void position_callback           (GtkWidget      *widget);
static void position_button_callback    (GtkWidget      *widget,
					 gpointer        data);
static void plist_build_combo(GtkWidget *combo,
			      GtkWidget *label,
			      stp_string_list_t items,
			      int active,
			      const gchar *cur_item,
			      const gchar *def_value,
			      GtkSignalFunc callback,
			      gint *callback_id,
			      int (*check_func)(const char *string),
			      gpointer data);
static void initialize_thumbnail(void);
static void set_color_defaults (void);
static void redraw_color_swatch (void);
static void color_update (GtkAdjustment *adjustment);
static void set_controls_active (GtkObject *checkbutton, gpointer optno);
static void update_adjusted_thumbnail (void);

static void set_media_size(const gchar *new_media_size);
static stp_printer_t tmp_printer = NULL;

static option_t *current_options = NULL;
static int current_option_count = 0;

static unit_t units[] =
  {
    { N_("Inch"), N_("Set the base unit of measurement to inches"),
      72.0, NULL, "%.2f" },
    { N_("cm"), N_("Set the base unit of measurement to centimetres"),
      72.0 / 2.54, NULL, "%.2f" },
    { N_("Points"), N_("Set the base unit of measurement to points (1/72\")"),
      1.0, NULL, "%.0f" },
    { N_("mm"), N_("Set the base unit of measurement to millimetres"),
      72.0 / 25.4, NULL, "%.1f" },
    { N_("Pica"), N_("Set the base unit of measurement to picas (1/12\")"),
      72.0 / 12.0, NULL, "%.1f" },
  };
static const gint unit_count = sizeof(units) / sizeof(unit_t);

static radio_group_t output_types[] =
  {
    { N_("Color"), N_("Color output"), OUTPUT_COLOR, NULL },
    { N_("Grayscale"),
      N_("Print in shades of gray using black ink"), OUTPUT_GRAY, NULL }
  };

static const gint output_type_count = (sizeof(output_types) /
				       sizeof(radio_group_t));

static gdouble preview_ppi = 10;

static stp_string_list_t printer_list = 0;
static stpui_plist_t *pv;

static gint thumbnail_w, thumbnail_h, thumbnail_bpp;
static guchar *thumbnail_data;
static guchar *adjusted_thumbnail_data;
static guchar *preview_thumbnail_data;

static void
set_gtk_curve_values(GtkWidget *gcurve, stp_curve_t seed)
{
  if (stp_curve_get_gamma(seed))
    {
      gtk_curve_set_gamma(GTK_CURVE(gcurve), stp_curve_get_gamma(seed));
    }
  else
    {
      stp_curve_t copy = stp_curve_create_copy(seed);
      const float *fdata;
      size_t count;
      stp_curve_resample(copy, 256);
      fdata = stp_curve_get_float_data(copy, &count);
      gtk_curve_set_vector(GTK_CURVE(gcurve), count, (float *) fdata);
      stp_curve_free(copy);
    }
}

static void
set_stp_curve_values(GtkWidget *widget, option_t *opt)
{
  int i;
  double lo, hi;
  gfloat vector[256];
  GtkWidget *gcurve = GTK_WIDGET(widget);
  stp_curve_t curve = stp_curve_create_copy(opt->info.curve.deflt);
  gtk_curve_get_vector(GTK_CURVE(gcurve), 256, vector);
  stp_curve_get_bounds(opt->info.curve.deflt, &lo, &hi);
  for (i = 0; i < 256; i++)
    {
      if (vector[i] > hi)
	vector[i] = hi;
      else if (vector[i] < lo)
	vector[i] = lo;
    }
  switch (GTK_CURVE(gcurve)->curve_type)
    {
    case GTK_CURVE_TYPE_SPLINE:
      stp_curve_set_interpolation_type(curve, STP_CURVE_TYPE_SPLINE);
      break;
    default:
      stp_curve_set_interpolation_type(curve, STP_CURVE_TYPE_LINEAR);
      break;
    }
  stp_curve_set_float_data(curve, 256, vector);
  stp_set_curve_parameter(pv->v, opt->fast_desc->name, curve);
  stp_curve_free(curve);
}

static int
open_curve_editor(GtkObject *button, gpointer xopt)
{
  option_t *opt = (option_t *)xopt;
  if (opt->info.curve.is_visible == FALSE)
    {
      GtkWidget *gcurve =
	GTK_WIDGET(GTK_GAMMA_CURVE(opt->info.curve.gamma_curve)->curve);
      stp_curve_t seed = stp_get_curve_parameter(pv->v, opt->fast_desc->name);
      if (!seed)
	seed = opt->info.curve.deflt;
      if (seed)
	seed = stp_curve_create_copy(seed);
      gtk_widget_set_sensitive(GTK_WIDGET(opt->checkbox), FALSE);
      gtk_widget_show(GTK_WIDGET(opt->info.curve.dialog));
      set_gtk_curve_values(gcurve, seed);
      opt->info.curve.is_visible = TRUE;
      opt->info.curve.current = seed;
      invalidate_preview_thumbnail();
      update_adjusted_thumbnail();
    }
/*  gtk_window_activate_focus(GTK_WINDOW(opt->info.curve.dialog)); */
  return 1;
}

static int
set_default_curve_callback(GtkObject *button, gpointer xopt)
{
  option_t *opt = (option_t *)xopt;
  GtkWidget *gcurve =
    GTK_WIDGET(GTK_GAMMA_CURVE(opt->info.curve.gamma_curve)->curve);
  stp_curve_t seed = opt->info.curve.deflt;
  set_gtk_curve_values(gcurve, seed);
  set_stp_curve_values(gcurve, opt);
  invalidate_preview_thumbnail();
  update_adjusted_thumbnail();
  return 1;
}  

static int
set_previous_curve_callback(GtkObject *button, gpointer xopt)
{
  option_t *opt = (option_t *)xopt;
  GtkWidget *gcurve =
    GTK_WIDGET(GTK_GAMMA_CURVE(opt->info.curve.gamma_curve)->curve);
  stp_curve_t seed = opt->info.curve.current;
  if (!seed)
    seed = opt->info.curve.deflt;
  set_gtk_curve_values(gcurve, seed);
  set_stp_curve_values(gcurve, opt);
  invalidate_preview_thumbnail();
  update_adjusted_thumbnail();
  return 1;
}

static int
set_curve_callback(GtkObject *button, gpointer xopt)
{
  option_t *opt = (option_t *)xopt;
  GtkWidget *gcurve =
    GTK_WIDGET(GTK_GAMMA_CURVE(opt->info.curve.gamma_curve)->curve);
  gtk_widget_hide(opt->info.curve.dialog);
  gtk_widget_set_sensitive(GTK_WIDGET(opt->checkbox), TRUE);
  opt->info.curve.is_visible = FALSE;
  set_stp_curve_values(gcurve, opt);
  stp_curve_free(opt->info.curve.current);
  invalidate_preview_thumbnail();
  update_adjusted_thumbnail();
  return 1;
}

static gint
curve_draw_callback(GtkWidget *widget, GdkEvent *event, gpointer xopt)
{
  option_t *opt = (option_t *)xopt;
  switch (event->type)
    {
    case GDK_BUTTON_RELEASE:
      set_stp_curve_values(widget, opt);
      invalidate_preview_thumbnail();
      update_adjusted_thumbnail();
      break;
    default:
      break;
    }
  return 1;
}

static gint
curve_type_changed(GtkWidget *widget, gpointer xopt)
{
  option_t *opt = (option_t *)xopt;
  set_stp_curve_values(widget, opt);
  invalidate_preview_thumbnail();
  update_adjusted_thumbnail();
  return 1;
}

static int
cancel_curve_callback(GtkObject *button, gpointer xopt)
{
  option_t *opt = (option_t *)xopt;
  if (opt->info.curve.is_visible)
    {
      stp_set_curve_parameter(pv->v, opt->fast_desc->name,
			      opt->info.curve.current);
      stp_curve_free(opt->info.curve.current);
      gtk_widget_hide(opt->info.curve.dialog);
      gtk_widget_set_sensitive(GTK_WIDGET(opt->checkbox), TRUE);
      opt->info.curve.is_visible = FALSE;
      invalidate_preview_thumbnail();
      update_adjusted_thumbnail();
    }
  return 1;
}

static void
stpui_create_curve(option_t *opt,
		   GtkTable *table,
		   gint column,
		   gint row,
		   const gchar *text,
		   stp_curve_t deflt,
		   gboolean is_optional)
{
  double lower, upper;
  opt->checkbox = gtk_check_button_new();
  gtk_table_attach_defaults(GTK_TABLE(table), opt->checkbox,
			    column, column + 1, row, row + 1);
  if (is_optional)
    gtk_widget_show(opt->checkbox);
  else
    gtk_widget_hide(opt->checkbox);

  opt->info.curve.label = gtk_label_new(text);
  gtk_misc_set_alignment (GTK_MISC (opt->info.curve.label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), opt->info.curve.label,
                    column + 1, column + 2, row, row + 1,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (opt->info.curve.label);

  opt->info.curve.button = gtk_button_new_with_label(_("Edit"));
  gtk_signal_connect(GTK_OBJECT(opt->info.curve.button), "clicked",
		     GTK_SIGNAL_FUNC(open_curve_editor), opt);
  gtk_table_attach (GTK_TABLE (table), opt->info.curve.button,
                    column + 2, column + 3, row, row + 1,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(opt->info.curve.button);

  opt->info.curve.dialog =
    stpui_dialog_new(_(opt->fast_desc->text), _(opt->fast_desc->text),
		     GTK_WIN_POS_MOUSE, FALSE, TRUE, FALSE,
		     _("Set Default"), set_default_curve_callback,
		     opt, NULL, NULL, FALSE, FALSE,
		     _("Restore Previous"), set_previous_curve_callback,
		     opt, NULL, NULL, FALSE, FALSE,
		     _("OK"), set_curve_callback,
		     opt, NULL, NULL, FALSE, FALSE,
		     _("Cancel"), cancel_curve_callback,
		     opt, NULL, NULL, FALSE, FALSE,
		     NULL);
  gtk_window_set_policy(GTK_WINDOW(opt->info.curve.dialog), 1, 1, 1);
  opt->info.curve.gamma_curve = gtk_gamma_curve_new();
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(opt->info.curve.dialog)->vbox),
		     opt->info.curve.gamma_curve, TRUE, TRUE, 0);
  stp_curve_get_bounds(opt->info.curve.deflt, &lower, &upper);
  gtk_curve_set_range(GTK_CURVE(GTK_GAMMA_CURVE(opt->info.curve.gamma_curve)->curve),
		      0.0, 1.0, lower, upper);
  gtk_widget_set_usize
    (GTK_WIDGET(GTK_GAMMA_CURVE(opt->info.curve.gamma_curve)->curve), 256, 256);
  gtk_widget_show(opt->info.curve.gamma_curve);

  gtk_signal_connect
    (GTK_OBJECT(GTK_GAMMA_CURVE(opt->info.curve.gamma_curve)->curve),
     "curve-type-changed", GTK_SIGNAL_FUNC(curve_type_changed), opt);
  gtk_signal_connect
    (GTK_OBJECT(GTK_GAMMA_CURVE(opt->info.curve.gamma_curve)->curve),
     "event", GTK_SIGNAL_FUNC(curve_draw_callback), opt);

  if (opt->fast_desc->help)
    {
      stpui_set_help_data (opt->info.curve.label, opt->fast_desc->help);
      stpui_set_help_data (opt->info.curve.button, opt->fast_desc->help);
      stpui_set_help_data (opt->info.curve.gamma_curve, opt->fast_desc->help);
    }
}

static int
checkbox_callback(GtkObject *button, gpointer xopt)
{
  option_t *opt = (option_t *)xopt;
  GtkWidget *checkbox = GTK_WIDGET(opt->info.bool.checkbox);
  opt->info.bool.current =
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox));
  stp_set_boolean_parameter(pv->v, opt->fast_desc->name,
			    opt->info.bool.current);
  invalidate_frame();
  invalidate_preview_thumbnail();
  if (opt->fast_desc->p_class == STP_PARAMETER_CLASS_OUTPUT)
    update_adjusted_thumbnail();
  preview_update();
  return 1;
}  

static void
stpui_create_boolean(option_t *opt,
		     GtkTable *table,
		     gint column,
		     gint row,
		     const gchar *text,
		     int deflt,
		     gboolean is_optional)
{
  opt->checkbox = gtk_check_button_new();
  gtk_table_attach_defaults(GTK_TABLE(table), opt->checkbox,
			    column, column + 1, row, row + 1);
  if (is_optional)
    gtk_widget_show(opt->checkbox);
  else
    gtk_widget_hide(opt->checkbox);

  opt->info.bool.checkbox =
    gtk_toggle_button_new_with_label(_(opt->fast_desc->text));
  gtk_table_attach_defaults(GTK_TABLE(table), opt->info.bool.checkbox,
			    column + 1, column + 3, row, row + 1);
  gtk_widget_show(opt->info.bool.checkbox);
  gtk_toggle_button_set_active
    (GTK_TOGGLE_BUTTON(opt->info.bool.checkbox),
     stp_get_boolean_parameter(pv->v, opt->fast_desc->name));
  gtk_signal_connect(GTK_OBJECT(opt->info.bool.checkbox), "toggled",
		     GTK_SIGNAL_FUNC(checkbox_callback), opt);
}

static void
build_printer_combo(void)
{
  int i;
  if (printer_list)
    stp_string_list_free(printer_list);
  printer_list = stp_string_list_create();
  for (i = 0; i < stpui_plist_count; i++)
    {
      if (stpui_plist[i].active)
	stp_string_list_add_string(printer_list, stpui_plist[i].name, stpui_plist[i].name);
      else
	{
	  gchar *name = malloc(strlen(stpui_plist[i].name) + 2);
	  strcpy(name + 1, stpui_plist[i].name);
	  name[0] = '*';
	  stp_string_list_add_string(printer_list, name, name);
	  free(name);
	}
    }
  plist_build_combo(printer_combo,
		    NULL,
		    printer_list,
		    1,
		    stp_string_list_param(printer_list, stpui_plist_current)->name,
		    NULL,
		    plist_callback,
		    &plist_callback_id,
		    NULL,
		    NULL);
}

static int
check_page_size(const char *paper_size)
{
  const stp_papersize_t *ps = stp_get_papersize_by_name(paper_size);
  if (ps && (ps->paper_unit == PAPERSIZE_ENGLISH_STANDARD ||
	     ps->paper_unit == PAPERSIZE_METRIC_STANDARD))
    return 1;
  else
    return 0;
}

static void
build_a_combo(option_t *option)
{
  if (option->fast_desc &&
      option->fast_desc->p_type == STP_PARAMETER_TYPE_STRING_LIST)
    {
      const gchar *val = stp_get_string_parameter(pv->v,
						  option->fast_desc->name);
      if (option->info.list.params == NULL || ! option->is_active ||
	  stp_string_list_count(option->info.list.params) == 0)
	stp_set_string_parameter(pv->v, option->fast_desc->name, NULL);
      else if (!val || strlen(val) == 0 ||
	       ! stp_string_list_is_present(option->info.list.params, val))
	stp_set_string_parameter(pv->v, option->fast_desc->name,
				 option->info.list.default_val);
      if (option->fast_desc->p_class == STP_PARAMETER_CLASS_PAGE_SIZE &&
	  strcmp(option->fast_desc->name, "PageSize") == 0 &&
	  !stpui_show_all_paper_sizes)
	
	plist_build_combo(option->info.list.combo, option->info.list.label,
			  option->info.list.params,
			  option->is_active,
			  stp_get_string_parameter(pv->v,
						   option->fast_desc->name),
			  option->info.list.default_val, combo_callback,
			  &(option->info.list.callback_id),
			  check_page_size, option);
      else
	plist_build_combo(option->info.list.combo, option->info.list.label,
			  option->info.list.params,
			  option->is_active,
			  stp_get_string_parameter(pv->v,
						   option->fast_desc->name),
			  option->info.list.default_val, combo_callback,
			  &(option->info.list.callback_id), NULL, option);
      if (option->fast_desc->p_class == STP_PARAMETER_CLASS_PAGE_SIZE)
	set_media_size
	  (stp_get_string_parameter(pv->v, option->fast_desc->name));
    }
  else
    plist_build_combo(option->info.list.combo, option->info.list.label,
		      NULL, 0, "", "", combo_callback,
		      &(option->info.list.callback_id), NULL, option);
}

static void
populate_options(const stp_vars_t v)
{
  stp_parameter_list_t params = stp_get_parameter_list(v);
  int i;
  if (current_options)
    {
      for (i = 0; i < current_option_count; i++)
	{
	  option_t *opt = &(current_options[i]);
	  switch (opt->fast_desc->p_type)
	    {
	    case STP_PARAMETER_TYPE_STRING_LIST:
	      if (opt->info.list.combo)
		{
		  gtk_widget_destroy(opt->info.list.combo);
		  gtk_widget_destroy(opt->info.list.label);
		  if (opt->info.list.params)
		    stp_string_list_free(opt->info.list.params);
		  free(opt->info.list.default_val);
		}
	      break;
	    case STP_PARAMETER_TYPE_DOUBLE:
	      if (opt->info.flt.adjustment)
		{
		  gtk_widget_destroy
		    (GTK_WIDGET
		     (SCALE_ENTRY_SCALE(opt->info.flt.adjustment)));
		  gtk_widget_destroy
		    (GTK_WIDGET
		     (SCALE_ENTRY_LABEL(opt->info.flt.adjustment)));
		  gtk_widget_destroy
		    (GTK_WIDGET
		     (SCALE_ENTRY_SPINBUTTON(opt->info.flt.adjustment)));
		}
	      break;
	    case STP_PARAMETER_TYPE_CURVE:
	      gtk_widget_destroy(GTK_WIDGET(opt->info.curve.label));
	      gtk_widget_destroy(GTK_WIDGET(opt->info.curve.button));
	      gtk_widget_destroy(GTK_WIDGET(opt->info.curve.dialog));
	      break;
	    case STP_PARAMETER_TYPE_BOOLEAN:
	      gtk_widget_destroy(GTK_WIDGET(opt->info.bool.checkbox));
	      break;
	    default:
	      break;
	    }
	  if (opt->checkbox)
	    gtk_widget_destroy(GTK_WIDGET(opt->checkbox));
	}
      free(current_options);
    }
  current_option_count = stp_parameter_list_count(params);
  current_options = malloc(sizeof(option_t) * current_option_count);

  for (i = 0; i < current_option_count; i++)
    {
      stp_parameter_t desc;
      option_t *opt = &(current_options[i]);
      opt->fast_desc = stp_parameter_list_param(params, i);
      stp_describe_parameter(v, opt->fast_desc->name, &desc);
      opt->checkbox = NULL;
      opt->is_active = 0;
      opt->is_enabled = 0;
      switch (opt->fast_desc->p_type)
	{
	case STP_PARAMETER_TYPE_STRING_LIST:
	  opt->info.list.callback_id = -1;
	  opt->info.list.default_val = g_strdup(desc.deflt.str);
	  if (desc.bounds.str)
	    opt->info.list.params =
	      stp_string_list_create_copy(desc.bounds.str);
	  else
	    opt->info.list.params = NULL;
	  opt->info.list.combo = NULL;
	  opt->info.list.label = NULL;
	  opt->is_active = desc.is_active;
	  break;
	case STP_PARAMETER_TYPE_DOUBLE:
	  opt->info.flt.adjustment = NULL;
	  opt->info.flt.upper = desc.bounds.dbl.upper;
	  opt->info.flt.lower = desc.bounds.dbl.lower;
	  opt->info.flt.deflt = desc.deflt.dbl;
	  opt->info.flt.scale = 1.0;
	  opt->is_active = desc.is_active;
	  break;
	case STP_PARAMETER_TYPE_CURVE:
	  opt->info.curve.label = NULL;
	  opt->info.curve.button = NULL;
	  opt->info.curve.dialog = NULL;
	  opt->info.curve.gamma_curve = NULL;
	  opt->info.curve.current = NULL;
	  opt->info.curve.deflt = desc.deflt.curve;
	  opt->info.curve.is_visible = FALSE;
	  opt->is_active = desc.is_active;
	  break;
	case STP_PARAMETER_TYPE_BOOLEAN:
	  opt->info.bool.checkbox = NULL;
	  opt->info.bool.current = 0;
	  opt->info.bool.deflt = desc.deflt.boolean;
	  opt->is_active = desc.is_active;
	default:
	  break;
	}
      stp_parameter_description_free(&desc);
    }
  stp_parameter_list_free(params);
}

static void
populate_option_table(GtkWidget *table, int p_class)
{
  int i, j;
  int current_pos = 0;
  int counts[STP_PARAMETER_LEVEL_INVALID][STP_PARAMETER_TYPE_INVALID];
  int vpos[STP_PARAMETER_LEVEL_INVALID][STP_PARAMETER_TYPE_INVALID];
  for (i = 0; i < STP_PARAMETER_LEVEL_INVALID; i++)
    for (j = 0; j < STP_PARAMETER_TYPE_INVALID; j++)
      {
	vpos[i][j] = 0;
	counts[i][j] = 0;
      }
	

  /* First scan the options to figure out where to start */
  for (i = 0; i < current_option_count; i++)
    {
      const stp_parameter_t *desc = current_options[i].fast_desc;
      if (desc->p_class == p_class)
	{
	  switch (desc->p_type)
	    {
	    case STP_PARAMETER_TYPE_STRING_LIST:
	    case STP_PARAMETER_TYPE_DOUBLE:
	    case STP_PARAMETER_TYPE_CURVE:
	    case STP_PARAMETER_TYPE_BOOLEAN:
	      counts[desc->p_level][desc->p_type]++;
	      break;
	    default:
	      break;
	    }
	}
    }

  /* Now, figure out where we're going to put the options */
  for (i = 0; i < STP_PARAMETER_LEVEL_INVALID; i++)
    {
      int level_count = 0;
      for (j = 0; j < STP_PARAMETER_TYPE_INVALID; j++)
	level_count += counts[i][j];
      if (level_count > 0 && current_pos > 0)
	{
	  GtkWidget *sep = gtk_hseparator_new();
	  gtk_table_attach_defaults(GTK_TABLE(table), sep, 0, 4,
				    current_pos, current_pos + 1);
	  if (i <= MAXIMUM_PARAMETER_LEVEL)
	    gtk_widget_show(sep);
	  current_pos++;
	}
      for (j = 0; j < STP_PARAMETER_TYPE_INVALID; j++)
	{
	  vpos[i][j] = current_pos;
	  current_pos += counts[i][j];
	}
    }

  for (i = 0; i < current_option_count; i++)
    {
      option_t *opt = &(current_options[i]);
      const stp_parameter_t *desc = opt->fast_desc;
      if (desc->p_class == p_class)
	{
	  switch (desc->p_type)
	    {
	    case STP_PARAMETER_TYPE_STRING_LIST:
	      stpui_create_new_combo(opt, table, 0,
				     vpos[desc->p_level][desc->p_type]++,
				     !(desc->is_mandatory));
	      if (desc->p_level > MAXIMUM_PARAMETER_LEVEL)
		stp_set_string_parameter_active(pv->v, desc->name,
						STP_PARAMETER_INACTIVE);
	      break;
	    case STP_PARAMETER_TYPE_DOUBLE:
	      stpui_create_scale_entry(opt, GTK_TABLE(table), 0,
				       vpos[desc->p_level][desc->p_type]++,
				       _(desc->text), 200, 0,
				       opt->info.flt.deflt,
				       opt->info.flt.lower,
				       opt->info.flt.upper,
				       .001, .01, 3, TRUE, 0, 0, NULL,
				       !(desc->is_mandatory));
	      stpui_set_adjustment_tooltip(opt->info.flt.adjustment,
					   _(desc->help));
	      gtk_signal_connect(GTK_OBJECT(opt->info.flt.adjustment),
				 "value_changed",
				 GTK_SIGNAL_FUNC(color_update), opt);
	      if (desc->p_level > MAXIMUM_PARAMETER_LEVEL)
		stp_set_float_parameter_active(pv->v, desc->name,
					       STP_PARAMETER_INACTIVE);
	      break;
	    case STP_PARAMETER_TYPE_CURVE:
	      opt->info.curve.current =
		stp_get_curve_parameter(pv->v, opt->fast_desc->name);
	      stpui_create_curve(opt, GTK_TABLE(table), 0,
				 vpos[desc->p_level][desc->p_type]++,
				 _(desc->text), opt->info.curve.deflt,
				 !(desc->is_mandatory));
	      if (desc->p_level > MAXIMUM_PARAMETER_LEVEL)
		stp_set_curve_parameter_active(pv->v, desc->name,
					       STP_PARAMETER_INACTIVE);
	      break;
	    case STP_PARAMETER_TYPE_BOOLEAN:
	      opt->info.bool.current =
		stp_get_boolean_parameter(pv->v, opt->fast_desc->name);
	      stpui_create_boolean(opt, GTK_TABLE(table), 0,
				   vpos[desc->p_level][desc->p_type]++,
				   _(desc->text), opt->info.bool.deflt,
				   !(desc->is_mandatory));
	      if (desc->p_level > MAXIMUM_PARAMETER_LEVEL)
		stp_set_boolean_parameter_active(pv->v, desc->name,
						 STP_PARAMETER_INACTIVE);
	      break;
	    case STP_PARAMETER_TYPE_INT:
	      stp_set_int_parameter_active(pv->v, opt->fast_desc->name,
					   STP_PARAMETER_INACTIVE);
	      break;
	    case STP_PARAMETER_TYPE_RAW:
	      stp_set_raw_parameter_active(pv->v, opt->fast_desc->name,
					   STP_PARAMETER_INACTIVE);
	      break;
	    case STP_PARAMETER_TYPE_FILE:
	      if (strcmp(opt->fast_desc->name, "PPDFile") != 0)
		stp_set_file_parameter_active(pv->v, opt->fast_desc->name,
					      STP_PARAMETER_INACTIVE);
	      break;
	    default:
	      break;
	    }
	  if (opt->checkbox)
	    gtk_signal_connect
	      (GTK_OBJECT(opt->checkbox), "toggled",
	       GTK_SIGNAL_FUNC(set_controls_active), opt);
	}
    }
}

static void
set_options_active(void)
{
  int i;
  for (i = 0; i < current_option_count; i++)
    {
      option_t *opt = &(current_options[i]);
      const stp_parameter_t *desc = opt->fast_desc;
      GtkObject *adj;
      switch (desc->p_type)
	{
	case STP_PARAMETER_TYPE_STRING_LIST:
	  build_a_combo(opt);
	  break;
	case STP_PARAMETER_TYPE_DOUBLE:
	  adj = opt->info.flt.adjustment;
	  if (adj)
	    {
	      if (opt->is_active && desc->p_level <= MAXIMUM_PARAMETER_LEVEL)
		{
		  gtk_widget_show(GTK_WIDGET(SCALE_ENTRY_LABEL(adj)));
		  gtk_widget_show(GTK_WIDGET(SCALE_ENTRY_SCALE(adj)));
		  gtk_widget_show(GTK_WIDGET(SCALE_ENTRY_SPINBUTTON(adj)));
		}
	      else
		{
		  gtk_widget_hide(GTK_WIDGET(SCALE_ENTRY_LABEL(adj)));
		  gtk_widget_hide(GTK_WIDGET(SCALE_ENTRY_SCALE(adj)));
		  gtk_widget_hide(GTK_WIDGET(SCALE_ENTRY_SPINBUTTON(adj)));
		}
	    }
	  break;
	case STP_PARAMETER_TYPE_CURVE:
	  if (opt->is_active && desc->p_level <= MAXIMUM_PARAMETER_LEVEL)
	    {
	      gtk_widget_show(GTK_WIDGET(opt->info.curve.label));
	      gtk_widget_show(GTK_WIDGET(opt->info.curve.button));
	    }
	  else
	    {
	      gtk_widget_hide(GTK_WIDGET(opt->info.curve.label));
	      gtk_widget_hide(GTK_WIDGET(opt->info.curve.button));
	      gtk_widget_hide(GTK_WIDGET(opt->info.curve.dialog));
	    }
	case STP_PARAMETER_TYPE_BOOLEAN:
	  if (opt->is_active && desc->p_level <= MAXIMUM_PARAMETER_LEVEL)
	    {
	      gtk_widget_show(GTK_WIDGET(opt->info.bool.checkbox));
	    }
	  else
	    {
	      gtk_widget_hide(GTK_WIDGET(opt->info.bool.checkbox));
	    }
	  break;
	default:
	  break;
	}
      if (opt->checkbox)
	{
	  if (!(opt->is_active) || desc->p_level > MAXIMUM_PARAMETER_LEVEL)
	    gtk_widget_hide(GTK_WIDGET(opt->checkbox));
	  else if (!(desc->is_mandatory))
	    gtk_widget_show(GTK_WIDGET(opt->checkbox));
	}
    }
}

static int
color_button_callback(GtkObject *button)
{
  invalidate_preview_thumbnail();
  update_adjusted_thumbnail();
}

static void
create_top_level_structure(void)
{
  gchar *plug_in_name;
  /*
   * Create the main dialog
   */

  plug_in_name = g_strdup_printf (_("%s -- Print v%s"),
                                  stpui_get_image_filename(),
				  VERSION " - " RELEASE_DATE);

  print_dialog =
    stpui_dialog_new (plug_in_name, "print",
		      GTK_WIN_POS_MOUSE,
		      FALSE, TRUE, FALSE,

		      _("About"), about_callback,
		      NULL, NULL, NULL, FALSE, FALSE,
		      _("Print and\nSave Settings"), printandsave_callback,
		      NULL, NULL, NULL, FALSE, FALSE,
		      _("Save\nSettings"), save_callback,
		      NULL, NULL, NULL, FALSE, FALSE,
		      _("Print"), print_callback,
		      NULL, NULL, NULL, FALSE, FALSE,
		      _("Cancel"), gtk_widget_destroy,
		      NULL, 1, NULL, FALSE, TRUE,

		      NULL);
  gtk_window_set_policy(GTK_WINDOW(print_dialog), 1, 1, 1);

  g_free (plug_in_name);

  gtk_signal_connect (GTK_OBJECT (print_dialog), "destroy",
                      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

  /*
   * Top-level containers
   */

  main_vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (print_dialog)->vbox), main_vbox,
		      TRUE, TRUE, 0);
  gtk_widget_show (main_vbox);

  main_hbox = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (main_vbox), main_hbox, TRUE, TRUE, 0);
  gtk_widget_show (main_hbox);

  right_vbox = gtk_vbox_new (FALSE, 2);
  gtk_box_pack_end (GTK_BOX (main_hbox), right_vbox, FALSE, FALSE, 0);
  gtk_widget_show (right_vbox);

  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (right_vbox), notebook, TRUE, TRUE, 0);
  gtk_widget_show (notebook);
}

static gint
drawing_area_resize_callback(GtkWidget *widget, GdkEventConfigure *event)
{
  preview_size_vert = event->height - 1;
  preview_size_horiz = event->width - 1;
  invalidate_preview_thumbnail();
  invalidate_frame();
  preview_update();
  return 1;
}

static void
create_preview (void)
{
  GtkWidget *frame;
  GtkWidget *event_box;

  frame = gtk_frame_new (_("Preview"));
  gtk_box_pack_start (GTK_BOX (main_hbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  preview = (GtkDrawingArea *) gtk_drawing_area_new ();
  gtk_drawing_area_size(preview, preview_size_horiz + 1, preview_size_vert +1);
  gtk_signal_connect(GTK_OBJECT(preview), "configure_event",
		     GTK_SIGNAL_FUNC(drawing_area_resize_callback), NULL);
  event_box = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (event_box), GTK_WIDGET (preview));
  gtk_container_add (GTK_CONTAINER (frame), event_box);
  gtk_widget_show (event_box);

  gtk_signal_connect (GTK_OBJECT (preview), "expose_event",
		      GTK_SIGNAL_FUNC (preview_expose), NULL);
  gtk_signal_connect (GTK_OBJECT (preview), "button_press_event",
                      GTK_SIGNAL_FUNC (preview_button_callback), NULL);
  gtk_signal_connect (GTK_OBJECT (preview), "button_release_event",
                      GTK_SIGNAL_FUNC (preview_button_callback), NULL);
  gtk_signal_connect (GTK_OBJECT (preview), "motion_notify_event",
                      GTK_SIGNAL_FUNC (preview_motion_callback), NULL);
  gtk_widget_show (GTK_WIDGET (preview));

  stpui_set_help_data
    (event_box,
     _("Position the image on the page.\n"
       "Click and drag with the primary button to position the image.\n"
       "Click and drag with the second button to move the image with finer precision; "
       "each unit of motion moves the image one point (1/72\")\n"
       "Click and drag with the third (middle) button to move the image in units of "
       "the image size.\n"
       "Holding down the shift key while clicking and dragging constrains the image to "
       "only horizontal or vertical motion.\n"
       "If you click another button while dragging the mouse, the image will return "
       "to its original position."));

  gtk_widget_set_events (GTK_WIDGET (preview),
                         GDK_EXPOSURE_MASK | GDK_BUTTON_MOTION_MASK |
                         GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
}

static GtkWidget *
create_positioning_entry(GtkWidget *table, int hpos, int vpos,
			 const char *text, const char *help)
{
  return stpui_create_entry
    (table, hpos, vpos, text, help, GTK_SIGNAL_FUNC(position_callback));
}

static GtkWidget *
create_positioning_button(GtkWidget *box, int invalid,
			  const char *text, const char *help)
{
  GtkWidget *button = gtk_button_new_with_label(_(text));
  gtk_box_pack_start(GTK_BOX(box), button, FALSE, TRUE, 0);
  gtk_widget_show(button);
  stpui_set_help_data(button, help);
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
		     GTK_SIGNAL_FUNC(position_button_callback),
		     (gpointer) invalid);
  return button;
}

static void
create_paper_size_frame(void)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *media_size_table;
  GtkWidget *table;
  int vpos = 0;

  frame = gtk_frame_new (_("Paper Size"));
  gtk_box_pack_start (GTK_BOX (right_vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  table = gtk_table_new (1, 1, FALSE);
  gtk_container_add (GTK_CONTAINER (vbox), table);
  gtk_widget_show (table);

  /*
   * Media size combo box.
   */

  page_size_table = gtk_table_new(1, 1, FALSE);
  gtk_widget_show (page_size_table);
  gtk_table_attach_defaults(GTK_TABLE(table), page_size_table,
			    0, 2, vpos, vpos + 1);
  vpos++;
  show_all_paper_sizes_button =
    gtk_check_button_new_with_label(_("Show All Paper Sizes"));
  gtk_table_attach_defaults
    (GTK_TABLE(table), show_all_paper_sizes_button, 0, 2, vpos, vpos + 1);
  gtk_toggle_button_set_active
    (GTK_TOGGLE_BUTTON(show_all_paper_sizes_button),
     stpui_show_all_paper_sizes);
  gtk_signal_connect(GTK_OBJECT(show_all_paper_sizes_button), "toggled",
		     GTK_SIGNAL_FUNC(show_all_paper_sizes_callback), NULL);
  gtk_widget_show(show_all_paper_sizes_button);
  vpos++;

  /*
   * Custom media size entries
   */

  media_size_table = gtk_table_new (1, 1, FALSE);
  stpui_table_attach_aligned(GTK_TABLE (table), 0, vpos++, _("Dimensions:"),
			     0.0, 0.5, media_size_table, 1, TRUE);
  gtk_table_set_col_spacings (GTK_TABLE (media_size_table), 4);

  custom_size_width = stpui_create_entry
    (media_size_table, 0, 3, _("Width:"),
     _("Width of the paper that you wish to print to"),
     custom_media_size_callback);

  custom_size_height = stpui_create_entry
    (media_size_table, 2, 3, _("Height:"),
     _("Height of the paper that you wish to print to"),
     custom_media_size_callback);
}

static void
create_positioning_frame (void)
{
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *box;
  GtkWidget *sep;

  frame = gtk_frame_new (_("Image Position"));
  gtk_box_pack_start (GTK_BOX (right_vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (1, 1, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  /*
   * Orientation option menu.
   */

  orientation_menu =
    stpui_option_menu_new (FALSE,
			   _("Auto"), orientation_callback,
			   (gpointer) ORIENT_AUTO, NULL, NULL, 0,
			   _("Portrait"), orientation_callback,
			   (gpointer) ORIENT_PORTRAIT, NULL, NULL, 0,
			   _("Landscape"), orientation_callback,
			   (gpointer) ORIENT_LANDSCAPE, NULL, NULL, 0,
			   _("Upside down"), orientation_callback,
			   (gpointer) ORIENT_UPSIDEDOWN, NULL, NULL, 0,
			   _("Seascape"), orientation_callback,
			   (gpointer) ORIENT_SEASCAPE, NULL, NULL, 0,
			   NULL);
  stpui_set_help_data (orientation_menu,
		 _("Select the orientation: portrait, landscape, "
		   "upside down, or seascape (upside down landscape)"));
  stpui_table_attach_aligned (GTK_TABLE (table), 0, 0, _("Orientation:"),
			      1.0, 0.5, orientation_menu, 4, TRUE);
  sep = gtk_hseparator_new ();
  gtk_table_attach_defaults (GTK_TABLE (table), sep, 0, 6, 1, 2);
  gtk_widget_show (sep);

  /*
   * Position entries
   */

  left_entry = create_positioning_entry
    (table, 0, 2, _("Left:"),
     _("Distance from the left of the paper to the image"));
#if 0
  right_entry = create_positioning_entry
    (table, 0, 3, _("Right:"),
     _("Distance from the left of the paper to the right of the image"));
#endif
  right_border_entry = create_positioning_entry
    (table, 0, 4, _("Right:"),
     _("Distance from the right of the paper to the image"));
  top_entry = create_positioning_entry
    (table, 3, 2, _("Top:"),
     _("Distance from the top of the paper to the image"));
#if 0
  bottom_entry = create_positioning_entry
    (table, 3, 3, _("Bottom:"),
     _("Distance from the top of the paper to bottom of the image"));
#endif
  bottom_border_entry = create_positioning_entry
    (table, 3, 4, _("Bottom:"),
     _("Distance from the bottom of the paper to the image"));
  /*
   * Center options
   */

  sep = gtk_hseparator_new ();
  gtk_table_attach_defaults (GTK_TABLE (table), sep, 0, 6, 5, 6);
  gtk_widget_show (sep);

  box = gtk_hbox_new (TRUE, 4);
  stpui_table_attach_aligned (GTK_TABLE (table), 0, 7, _("Center:"), 0.5, 0.5,
			      box, 5, TRUE);
  recenter_vertical_button = create_positioning_button
    (box, INVALID_TOP, _("Vertical"),
     _("Center the image vertically on the paper"));
  recenter_button = create_positioning_button
    (box, INVALID_LEFT | INVALID_TOP, _("Both"),
     _("Center the image on the paper"));
  recenter_horizontal_button = create_positioning_button
    (box, INVALID_LEFT, _("Horizontal"),
     _("Center the image horizontally on the paper"));
}

static void
create_printer_dialog (void)
{
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *event_box;
  gint       i;

  setup_dialog = stpui_dialog_new(_("Setup Printer"), "print",
				  GTK_WIN_POS_MOUSE, FALSE, TRUE, FALSE,
				  _("OK"), setup_ok_callback,
				  NULL, NULL, NULL, TRUE, FALSE,
				  _("Cancel"), gtk_widget_hide,
				  NULL, 1, NULL, FALSE, TRUE,

				  NULL);
  gtk_window_set_policy(GTK_WINDOW(setup_dialog), 1, 1, 1);

  /*
   * Top-level table for dialog.
   */

  table = gtk_table_new (4, 2, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacing (GTK_TABLE (table), 0, 150);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (setup_dialog)->vbox), table,
                      TRUE, TRUE, 0);
  gtk_widget_show (table);

  /*
   * Printer driver option menu.
   */

  label = gtk_label_new (_("Printer Model:"));
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 2,
		    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  event_box = gtk_event_box_new ();
  gtk_table_attach (GTK_TABLE (table), event_box, 1, 2, 0, 2,
                    GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
  gtk_widget_show (event_box);

  stpui_set_help_data (event_box, _("Select your printer model"));

  printer_crawler = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (printer_crawler),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (event_box), printer_crawler);
  gtk_widget_show (printer_crawler);

  printer_driver = gtk_clist_new (1);
  gtk_widget_set_usize (printer_driver, 200, 0);
  gtk_clist_set_selection_mode(GTK_CLIST(printer_driver),GTK_SELECTION_SINGLE);
  gtk_container_add (GTK_CONTAINER (printer_crawler), printer_driver);
  gtk_widget_show (printer_driver);

  gtk_signal_connect (GTK_OBJECT (printer_driver), "select_row",
                      GTK_SIGNAL_FUNC (print_driver_callback), NULL);

  for (i = 0; i < stp_printer_model_count (); i ++)
    {
      stp_printer_t the_printer = stp_get_printer_by_index (i);

      if (strcmp(stp_printer_get_long_name (the_printer), "") != 0 &&
	  strcmp(stp_printer_get_family(the_printer), "raw") != 0)
	{
	  gchar *tmp=g_strdup(gettext(stp_printer_get_long_name(the_printer)));

	  gtk_clist_insert (GTK_CLIST (printer_driver), i, &tmp);
	  gtk_clist_set_row_data (GTK_CLIST (printer_driver), i, (gpointer) i);
	  g_free(tmp);
	}
    }

  /*
   * PPD file.
   */

  ppd_label = gtk_label_new (_("PPD File:"));
  gtk_misc_set_alignment (GTK_MISC (ppd_label), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), ppd_label, 0, 1, 3, 4,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (ppd_label);

  ppd_box = gtk_hbox_new (FALSE, 8);
  gtk_table_attach (GTK_TABLE (table), ppd_box, 1, 2, 3, 4,
                    GTK_FILL, GTK_FILL, 0, 0);

  ppd_file = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (ppd_box), ppd_file, TRUE, TRUE, 0);
  gtk_widget_show (ppd_file);

  stpui_set_help_data(ppd_file,_("Enter the correct PPD filename for your printer"));

  ppd_button = gtk_button_new_with_label (_("Browse"));
  gtk_misc_set_padding (GTK_MISC (GTK_BIN (ppd_button)->child), 2, 0);
  gtk_box_pack_start (GTK_BOX (ppd_box), ppd_button, FALSE, FALSE, 0);
  gtk_widget_show (ppd_button);
  gtk_widget_show (ppd_box);

  stpui_set_help_data(ppd_button,
		_("Choose the correct PPD filename for your printer"));
  gtk_signal_connect (GTK_OBJECT (ppd_button), "clicked",
                      GTK_SIGNAL_FUNC (ppd_browse_callback), NULL);

  /*
   * Print command.
   */

  label = gtk_label_new (_("Command:"));
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  output_cmd = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), output_cmd, 1, 2, 2, 3,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (output_cmd);

  stpui_set_help_data(output_cmd,
		_("Enter the correct command to print to your printer. "
		  "Note: Please do not remove the `-l' or `-oraw' from "
		  "the command string, or printing will probably fail!"));

  /*
   * Output file selection dialog.
   */

  file_browser = gtk_file_selection_new (_("Print To File?"));

  gtk_signal_connect
    (GTK_OBJECT (GTK_FILE_SELECTION (file_browser)->ok_button), "clicked",
     GTK_SIGNAL_FUNC (file_ok_callback), NULL);
  gtk_signal_connect
    (GTK_OBJECT (GTK_FILE_SELECTION (file_browser)->cancel_button), "clicked",
     GTK_SIGNAL_FUNC (file_cancel_callback), NULL);

  /*
   * PPD file selection dialog.
   */

  ppd_browser = gtk_file_selection_new (_("PPD File?"));
  gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION (ppd_browser));

  gtk_signal_connect
    (GTK_OBJECT (GTK_FILE_SELECTION (ppd_browser)->ok_button), "clicked",
     GTK_SIGNAL_FUNC (ppd_ok_callback), NULL);
  gtk_signal_connect_object
    (GTK_OBJECT (GTK_FILE_SELECTION (ppd_browser)->cancel_button), "clicked",
     GTK_SIGNAL_FUNC (gtk_widget_hide), GTK_OBJECT (ppd_browser));
}

static void
create_new_printer_dialog (void)
{
  GtkWidget *table;

  new_printer_dialog =
    stpui_dialog_new (_("Define New Printer"), "print",
                     GTK_WIN_POS_MOUSE, FALSE, TRUE, FALSE,
                     _("OK"), new_printer_ok_callback,
		     NULL, NULL, NULL, TRUE, FALSE,
                     _("Cancel"), gtk_widget_hide,
                     NULL, 1, NULL, FALSE, TRUE,
		     NULL);
  gtk_window_set_policy(GTK_WINDOW(new_printer_dialog), 1, 1, 1);

  table = gtk_table_new (1, 1, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (new_printer_dialog)->vbox), table,
                      FALSE, FALSE, 0);
  gtk_widget_show (table);

  new_printer_entry = gtk_entry_new ();
  gtk_entry_set_max_length (GTK_ENTRY (new_printer_entry), 127);
  stpui_table_attach_aligned(GTK_TABLE (table), 0, 0, _("Printer Name:"), 1.0,
			     0.5, new_printer_entry, 1, TRUE);

  stpui_set_help_data(new_printer_entry,
		_("Enter the name you wish to give this logical printer"));
  gtk_signal_connect (GTK_OBJECT (new_printer_entry), "activate",
                      GTK_SIGNAL_FUNC (new_printer_ok_callback), NULL);
}

static void
create_about_dialog (void)
{
  GtkWidget *label;
  about_dialog =
    stpui_dialog_new (_("About Gimp-Print " PLUG_IN_VERSION), "print",
		      GTK_WIN_POS_MOUSE, FALSE, TRUE, FALSE,
		      _("OK"), gtk_widget_hide,
		      NULL, 1, NULL, TRUE, TRUE,
		      NULL);
  gtk_window_set_policy(GTK_WINDOW(about_dialog), 1, 1, 1);

  label = gtk_label_new
    (_("Gimp-Print Version " PLUG_IN_VERSION "\n"
       "\n"
       "Copyright (C) 1997-2001 Michael Sweet, Robert Krawitz,\n"
       "and the rest of the Gimp-Print Development Team.\n"
       "\n"
       "Please visit our web site at http://gimp-print.sourceforge.net.\n"
       "\n"
       "This program is free software; you can redistribute it and/or modify\n"
       "it under the terms of the GNU General Public License as published by\n"
       "the Free Software Foundation; either version 2 of the License, or\n"
       "(at your option) any later version.\n"
       "\n"
       "This program is distributed in the hope that it will be useful,\n"
       "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
       "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
       "GNU General Public License for more details.\n"
       "\n"
       "You should have received a copy of the GNU General Public License\n"
       "along with this program; if not, write to the Free Software\n"
       "Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  "
       "USA\n"));

  gtk_misc_set_padding (GTK_MISC (label), 12, 4);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (about_dialog)->vbox), label,
                      FALSE, FALSE, 0);
  gtk_widget_show (label);
}

static void
create_printer_settings_frame (void)
{
  GtkWidget *table;
  GtkWidget *sep;
  GtkWidget *printer_hbox;
  GtkWidget *button;
  GtkWidget *event_box;
  gint vpos = 0;

  create_printer_dialog ();
  create_about_dialog ();
  create_new_printer_dialog ();

  table = gtk_table_new (1, 1, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table,
                            gtk_label_new (_("Printer Settings")));
  gtk_widget_show (table);

  /*
   * Printer option menu.
   */

  printer_combo = gtk_combo_new ();
  event_box = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (event_box), printer_combo);
  gtk_widget_show (printer_combo);

  stpui_set_help_data(event_box,
		_("Select the name of the printer (not the type, "
		  "or model, of printer) that you wish to print to"));
  stpui_table_attach_aligned(GTK_TABLE (table), 0, vpos++, _("Printer Name:"),
			     0.0, 0.5, event_box, 1, TRUE);
  printer_model_label = gtk_label_new ("");
  stpui_table_attach_aligned(GTK_TABLE (table), 0, vpos++, _("Printer Model:"),
			     0.0, 0.0, printer_model_label, 1, TRUE);
  printer_hbox = gtk_hbox_new (TRUE, 4);
  gtk_table_attach_defaults (GTK_TABLE (table), printer_hbox,
			     1, 4, vpos, vpos + 1);
  vpos += 2;
  gtk_widget_show (printer_hbox);

  /*
   * Setup printer button
   */

  button = gtk_button_new_with_label (_("Setup Printer..."));
  stpui_set_help_data(button,
		      _("Choose the printer model, PPD file, and command "
			"that is used to print to this printer"));
  gtk_misc_set_padding (GTK_MISC (GTK_BIN (button)->child), 2, 0);
  gtk_box_pack_start (GTK_BOX (printer_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  gtk_signal_connect (GTK_OBJECT (button), "clicked",
                      GTK_SIGNAL_FUNC (setup_open_callback), NULL);

  /*
   * New printer button
   */

  button = gtk_button_new_with_label (_("New Printer..."));
  stpui_set_help_data (button, _("Define a new logical printer. This can be used to "
			   "name a collection of settings that you wish to "
			   "remember for future use."));
  gtk_box_pack_start (GTK_BOX (printer_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  gtk_signal_connect (GTK_OBJECT (button), "clicked",
                      GTK_SIGNAL_FUNC (new_printer_open_callback), NULL);

  sep = gtk_hseparator_new ();
  gtk_table_attach_defaults (GTK_TABLE (table), sep, 0, 5, vpos, vpos + 1);
  gtk_widget_show (sep);
  vpos++;

  printer_features_table = gtk_table_new(1, 1, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (printer_features_table), 2);
  gtk_table_set_row_spacings (GTK_TABLE (printer_features_table), 0);
  gtk_widget_show (printer_features_table);
  gtk_table_attach_defaults(GTK_TABLE(table), printer_features_table,
			    0, 6, vpos, vpos + 1);
}

static void
create_scaling_frame (void)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *table;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *event_box;
  GtkWidget *sep;
  GSList    *group;

  frame = gtk_frame_new (_("Image Size"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_widget_show (hbox);

  vbox = gtk_vbox_new (FALSE, 2);
/*  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4); */
  gtk_container_add (GTK_CONTAINER (hbox), vbox);
  gtk_widget_show (vbox);

  table = gtk_table_new (1, 1, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  /*
   * Create the scaling adjustment using percent.  It doesn't really matter,
   * since as soon as we call plist_callback at the end of initialization
   * everything will be put right.
   */
  scaling_adjustment =
    stpui_scale_entry_new (GTK_TABLE (table), 0, 0, _("Scaling:"), 100, 75,
			   100.0, minimum_image_percent, 100.0,
			   1.0, 10.0, 1, TRUE, 0, 0, NULL);
  stpui_set_adjustment_tooltip(scaling_adjustment,
			       _("Set the scale (size) of the image"));
  gtk_signal_connect (GTK_OBJECT (scaling_adjustment), "value_changed",
                      GTK_SIGNAL_FUNC (scaling_update), NULL);

  box = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);
  gtk_widget_show (box);

  /*
   * The scale by percent/ppi toggles
   */

  table = gtk_table_new (1, 1, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_box_pack_start (GTK_BOX (box), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  event_box = gtk_event_box_new ();
  gtk_table_attach (GTK_TABLE (table), event_box, 0, 1, 0, 1,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (event_box);

  label = gtk_label_new ("Scale by:");
  gtk_container_add (GTK_CONTAINER (event_box), label);
  gtk_widget_show (label);

  stpui_set_help_data(event_box,
		_("Select whether scaling is measured as percent of "
		  "available page size or number of output dots per inch"));

  scaling_percent = gtk_radio_button_new_with_label (NULL, _("Percent"));
  group = gtk_radio_button_group (GTK_RADIO_BUTTON (scaling_percent));
  stpui_table_attach_aligned(GTK_TABLE (table), 0, 0, NULL, 0.5, 0.5,
			     scaling_percent, 2, TRUE);

  stpui_set_help_data(scaling_percent, _("Scale the print to the size of the page"));
  gtk_signal_connect (GTK_OBJECT (scaling_percent), "toggled",
                      GTK_SIGNAL_FUNC (scaling_callback), NULL);

  scaling_ppi = gtk_radio_button_new_with_label (group, _("PPI"));
  stpui_table_attach_aligned(GTK_TABLE (table), 2, 0, NULL, 0.5, 0.5,
			     scaling_ppi, 1, TRUE);

  stpui_set_help_data(scaling_ppi,
		_("Scale the print to the number of dots per inch"));
  gtk_signal_connect (GTK_OBJECT (scaling_ppi), "toggled",
                      GTK_SIGNAL_FUNC (scaling_callback), NULL);

  sep = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (hbox), sep, FALSE, FALSE, 8);
  gtk_widget_show (sep);

  /*
   * The width/height enries
   */

  table = gtk_table_new (1, 1, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);
  gtk_box_pack_start (GTK_BOX (hbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  width_entry = create_positioning_entry
    (table, 0, 0, _("Width:"), _("Set the width of the print"));
  height_entry = create_positioning_entry
    (table, 0, 1, _("Height:"), _("Set the height of the print"));

  /*
   * The "image size" button
   */

  scaling_image = gtk_button_new_with_label (_("Use Original\nImage Size"));
  gtk_misc_set_padding (GTK_MISC (GTK_BIN (scaling_image)->child), 2, 2);
  gtk_box_pack_end (GTK_BOX (hbox), scaling_image, FALSE, FALSE, 0);
  gtk_widget_show (scaling_image);

  stpui_set_help_data(scaling_image,
		_("Set the print size to the size of the image"));
  gtk_signal_connect (GTK_OBJECT (scaling_image), "clicked",
                      GTK_SIGNAL_FUNC (scaling_callback), NULL);
}

/*
 * create_color_adjust_window (void)
 *
 * NOTES:
 *   creates the color adjuster popup, allowing the user to adjust brightness,
 *   contrast, saturation, etc.
 */
static void
create_color_adjust_window (void)
{
  GtkWidget *table;
  GtkWidget *event_box;

  initialize_thumbnail();

  color_adjust_dialog =
    stpui_dialog_new(_("Print Color Adjust"), "print",
		     GTK_WIN_POS_MOUSE, FALSE, TRUE, FALSE,

		     _("Set Defaults"), set_color_defaults,
		     NULL, NULL, NULL, FALSE, FALSE,
		     _("Close"), gtk_widget_hide,
		     NULL, 1, NULL, TRUE, TRUE,

		     NULL);
  gtk_window_set_policy(GTK_WINDOW(color_adjust_dialog), 1, 1, 1);

  table = gtk_table_new (1, 1, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 0);
/*  gtk_table_set_row_spacing (GTK_TABLE (table), 8, 6); */

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (color_adjust_dialog)->vbox),
		      table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  /*
   * Drawing area for color swatch feedback display...
   */

  event_box = gtk_event_box_new ();
  gtk_widget_show (GTK_WIDGET (event_box));
  gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (event_box),
                    0, 1, 0, 1, 0, 0, 0, 0);

  swatch = (GtkDrawingArea *) gtk_drawing_area_new ();
  gtk_widget_set_events (GTK_WIDGET (swatch), GDK_EXPOSURE_MASK);
  gtk_drawing_area_size (swatch, thumbnail_w, thumbnail_h);
  gtk_container_add (GTK_CONTAINER (event_box), GTK_WIDGET (swatch));
  gtk_widget_show (GTK_WIDGET (swatch));

  stpui_set_help_data (GTK_WIDGET (event_box), _("Image preview"));
  gtk_signal_connect (GTK_OBJECT (swatch), "expose_event",
                      GTK_SIGNAL_FUNC (redraw_color_swatch),
                      NULL);

  event_box = gtk_event_box_new ();
  gtk_widget_show (GTK_WIDGET (event_box));
  gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (event_box),
                    1, 2, 0, 1, 0, 0, 0, 0);

  output_color_vbox = gtk_vbox_new(TRUE, 0);
  gtk_container_add(GTK_CONTAINER(event_box), output_color_vbox);
  gtk_widget_show(GTK_WIDGET(output_color_vbox));

  cyan_button = gtk_toggle_button_new_with_label(_("Cyan"));
  gtk_box_pack_start(GTK_BOX(output_color_vbox), cyan_button, TRUE, TRUE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cyan_button), TRUE);
  gtk_widget_show(GTK_WIDGET(cyan_button));
  gtk_signal_connect (GTK_OBJECT (cyan_button), "toggled",
                      GTK_SIGNAL_FUNC (color_button_callback), NULL);

  magenta_button = gtk_toggle_button_new_with_label(_("Magenta"));
  gtk_box_pack_start(GTK_BOX(output_color_vbox), magenta_button, TRUE, TRUE,0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(magenta_button), TRUE);
  gtk_widget_show(GTK_WIDGET(magenta_button));
  gtk_signal_connect (GTK_OBJECT (magenta_button), "toggled",
                      GTK_SIGNAL_FUNC (color_button_callback), NULL);

  yellow_button = gtk_toggle_button_new_with_label(_("Yellow"));
  gtk_box_pack_start(GTK_BOX(output_color_vbox), yellow_button, TRUE, TRUE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(yellow_button), TRUE);
  gtk_widget_show(GTK_WIDGET(yellow_button));
  gtk_signal_connect (GTK_OBJECT (yellow_button), "toggled",
                      GTK_SIGNAL_FUNC (color_button_callback), NULL);

  black_button = gtk_toggle_button_new_with_label(_("Black"));
  gtk_box_pack_start(GTK_BOX(output_color_vbox), black_button, TRUE, TRUE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(black_button), TRUE);
  gtk_widget_show(GTK_WIDGET(black_button));
  gtk_signal_connect (GTK_OBJECT (black_button), "toggled",
                      GTK_SIGNAL_FUNC (color_button_callback), NULL);

  color_adjustment_table = gtk_table_new(1, 1, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (color_adjustment_table), 2);
  gtk_table_set_row_spacings (GTK_TABLE (color_adjustment_table), 0);
  gtk_container_set_border_width (GTK_CONTAINER (color_adjustment_table), 4);
  gtk_widget_show (color_adjustment_table);
  gtk_table_attach_defaults(GTK_TABLE(table), color_adjustment_table,
			    0, 2, 1, 2);
}

static void
create_image_settings_frame (void)
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *event_box;
  GtkWidget *sep;
  GSList    *group;
  gint i;

  create_color_adjust_window ();

  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox,
                            gtk_label_new (_("Output")));
  gtk_widget_show (vbox);

  table = gtk_table_new (1, 1, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  event_box = gtk_event_box_new ();
  gtk_table_attach (GTK_TABLE (table), event_box, 0, 1, 0, 1,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (event_box);

  /*
   * Output type toggles.
   */

  table = gtk_table_new (1, 1, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
/*  gtk_table_set_row_spacing (GTK_TABLE (table), 2, 4); */
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  event_box = gtk_event_box_new ();
  gtk_table_attach (GTK_TABLE (table), event_box, 0, 1, 0, 1,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (event_box);

  label = gtk_label_new (_("Output Type:"));
  gtk_container_add (GTK_CONTAINER (event_box), label);
  gtk_widget_show (label);

  stpui_set_help_data(event_box, _("Select the desired output type"));

  group = NULL;
  for (i = 0; i < output_type_count; i++)
    group = stpui_create_radio_button(&(output_types[i]), group, table, 0, i,
				      output_type_callback);

  sep = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), sep, FALSE, FALSE, 0);
  gtk_widget_show (sep);

  /*
   *  Color adjust button
   */
  hbox = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show(hbox);
  label = gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
  gtk_widget_show(label);

  adjust_color_button = gtk_button_new_with_label (_("Adjust Output..."));
  gtk_misc_set_padding (GTK_MISC (GTK_BIN (adjust_color_button)->child), 4, 0);
  gtk_box_pack_start (GTK_BOX (hbox), adjust_color_button, FALSE, FALSE, 0);
  gtk_widget_show(adjust_color_button);
  label = gtk_label_new("");
  gtk_box_pack_end(GTK_BOX(hbox), label, TRUE, TRUE, 0);
  gtk_widget_show(label);

  stpui_set_help_data(adjust_color_button,
		_("Adjust color balance, brightness, contrast, "
		  "saturation, and dither algorithm"));
  gtk_signal_connect_object (GTK_OBJECT (adjust_color_button), "clicked",
			     GTK_SIGNAL_FUNC (gtk_widget_show),
			     GTK_OBJECT (color_adjust_dialog));
}

static void
create_units_frame (void)
{
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *event_box;
  GSList    *group;
  gint i;

  units_hbox = gtk_hbox_new(FALSE, 0);
  label = gtk_label_new(_("Size Units:"));
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(units_hbox), label, TRUE, TRUE, 0);
  units_label = gtk_label_new(_(" "));
  gtk_widget_show(units_label);
  gtk_box_pack_start(GTK_BOX(units_hbox), units_label, TRUE, TRUE, 0);
  gtk_widget_show(units_hbox);

  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, units_hbox);
  gtk_widget_show (vbox);

  /*
   * The units toggles
   */

  table = gtk_table_new (1, 1, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  event_box = gtk_event_box_new ();
  gtk_table_attach (GTK_TABLE (table), event_box, 0, 1, 0, 1,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (event_box);

  label = gtk_label_new (_("Units:"));
  gtk_container_add (GTK_CONTAINER (event_box), label);
  gtk_widget_show (label);

  stpui_set_help_data(event_box,
		_("Select the base unit of measurement for printing"));

  group = NULL;
  for (i = 0; i < unit_count; i++)
    {
      unit_t *unit = &(units[i]);
      unit->checkbox = gtk_radio_button_new_with_label(group, _(unit->name));
      group = gtk_radio_button_group(GTK_RADIO_BUTTON(unit->checkbox));
      stpui_table_attach_aligned(GTK_TABLE(table), i / 2, i % 2, NULL, 0.5,
				 0.5, unit->checkbox, 1, TRUE);
      stpui_set_help_data(unit->checkbox, _(unit->help));
      gtk_signal_connect(GTK_OBJECT(unit->checkbox), "toggled",
			 GTK_SIGNAL_FUNC(unit_callback), (gpointer) i);
    }
}

/*
 *  create_main_window()
 */
static void
create_main_window (void)
{

  pv = &(stpui_plist[stpui_plist_current]);
  /*
   * Create the various dialog components.  Note that we're not
   * actually initializing the values at this point; that will be done after
   * the UI is fully created.
   */

  stpui_help_init();

  create_top_level_structure ();

  create_preview ();
  create_printer_settings_frame ();
  create_units_frame();
  create_paper_size_frame();
  create_positioning_frame ();
  create_scaling_frame ();
  create_image_settings_frame ();

  /*
   * Now actually set up the correct values in the dialog
   */

  do_update_thumbnail = 1;
  build_printer_combo ();
  plist_callback (NULL, (gpointer) stpui_plist_current);
  update_adjusted_thumbnail ();

  gtk_widget_show (print_dialog);
}

static void
set_entry_value(GtkWidget *entry, double value, int block)
{
  gchar s[255];
  gdouble unit_scaler = units[pv->unit].scale;
  const gchar *format = units[pv->unit].format;

  g_snprintf(s, sizeof(s), format, value / unit_scaler);
  if (block)
    gtk_signal_handler_block_by_data (GTK_OBJECT (entry), NULL);
  gtk_entry_set_text (GTK_ENTRY (entry), s);
  if (block)
    gtk_signal_handler_unblock_by_data (GTK_OBJECT (entry), NULL);
}

static void
reset_preview(void)
{
  if (!suppress_preview_reset)
    {
      stpui_enable_help();
      buttons_pressed = preview_active = 0;
    }
}

static void
invalidate_preview_thumbnail (void)
{
  preview_valid = FALSE;
}

static void
invalidate_frame (void)
{
  frame_valid = FALSE;
}

static void
compute_scaling_limits(gdouble *min_ppi_scaling, gdouble *max_ppi_scaling)
{
  gdouble min_ppi_scaling1, min_ppi_scaling2;
  min_ppi_scaling1 = FINCH * (gdouble) image_width / (gdouble) printable_width;
  min_ppi_scaling2 = FINCH * (gdouble)image_height / (gdouble)printable_height;

  if (min_ppi_scaling1 > min_ppi_scaling2)
    *min_ppi_scaling = min_ppi_scaling1;
  else
    *min_ppi_scaling = min_ppi_scaling2;

  *max_ppi_scaling = *min_ppi_scaling * 100 / minimum_image_percent;
}

/*
 *  scaling_update() - Update the scaling scale using the slider.
 */
static void
scaling_update (GtkAdjustment *adjustment)
{
  reset_preview ();

  if (pv->scaling != adjustment->value)
    {
      invalidate_preview_thumbnail ();
      if (GTK_TOGGLE_BUTTON (scaling_ppi)->active)
	pv->scaling = -adjustment->value;
      else
	pv->scaling = adjustment->value;

      suppress_scaling_adjustment = TRUE;
      preview_update ();
      suppress_scaling_adjustment = FALSE;
    }
}

/*
 *  scaling_callback() - Update the scaling scale using radio buttons.
 */
static void
scaling_callback (GtkWidget *widget)
{
  gdouble max_ppi_scaling;
  gdouble min_ppi_scaling;
  gdouble current_scale;

  reset_preview ();

  if (suppress_scaling_callback)
    return;

  compute_scaling_limits(&min_ppi_scaling, &max_ppi_scaling);

  if (widget == scaling_ppi)
    {
      if (! GTK_TOGGLE_BUTTON (scaling_ppi)->active)
	return;

      GTK_ADJUSTMENT (scaling_adjustment)->lower = min_ppi_scaling;
      GTK_ADJUSTMENT (scaling_adjustment)->upper = max_ppi_scaling;

      /*
       * Compute the correct PPI to create an image of the same size
       * as the one measured in percent
       */
      current_scale = GTK_ADJUSTMENT (scaling_adjustment)->value;
      GTK_ADJUSTMENT (scaling_adjustment)->value =
	min_ppi_scaling / (current_scale / 100);
      pv->scaling = 0.0;
    }
  else if (widget == scaling_percent)
    {
      gdouble new_percent;

      if (! GTK_TOGGLE_BUTTON (scaling_percent)->active)
	return;

      current_scale = GTK_ADJUSTMENT (scaling_adjustment)->value;
      GTK_ADJUSTMENT (scaling_adjustment)->lower = minimum_image_percent;
      GTK_ADJUSTMENT (scaling_adjustment)->upper = 100.0;

      new_percent = 100 * min_ppi_scaling / current_scale;

      if (new_percent > 100)
	new_percent = 100;
      if (new_percent < minimum_image_percent)
	new_percent = minimum_image_percent;

      GTK_ADJUSTMENT (scaling_adjustment)->value = new_percent;
      pv->scaling = 0.0;
    }
  else if (widget == scaling_image)
    {
      gdouble yres = image_yres;

      invalidate_preview_thumbnail ();

      GTK_ADJUSTMENT (scaling_adjustment)->lower = min_ppi_scaling;
      GTK_ADJUSTMENT (scaling_adjustment)->upper = max_ppi_scaling;

      if (yres < min_ppi_scaling)
	yres = min_ppi_scaling;
      if (yres > max_ppi_scaling)
	yres = max_ppi_scaling;

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scaling_ppi), TRUE);
      GTK_ADJUSTMENT (scaling_adjustment)->value = yres;
      pv->scaling = 0.0;
    }

  if (widget == scaling_ppi || widget == scaling_percent)
    suppress_preview_update++;
  gtk_adjustment_changed (GTK_ADJUSTMENT (scaling_adjustment));
  gtk_adjustment_value_changed (GTK_ADJUSTMENT (scaling_adjustment));
  if (widget == scaling_ppi || widget == scaling_percent)
    suppress_preview_update--;
}

/****************************************************************************
 *
 * plist_build_combo
 *
 ****************************************************************************/
static void
plist_build_combo (GtkWidget      *combo,       /* I - Combo widget */
		   GtkWidget      *label,
		   stp_string_list_t items,      /* I - Menu items */
		   int		  active,
		   const gchar    *cur_item,    /* I - Current item */
		   const gchar    *def_value,   /* I - default item */
		   GtkSignalFunc   callback,    /* I - Callback */
		   gint           *callback_id, /* IO - Callback ID (init to -1) */
		   int (*check_func)(const char *string),
		   gpointer        data)
{
  gint      i; /* Looping var */
  GList    *list = 0;
  gint num_items = 0;
  GtkEntry *entry = GTK_ENTRY (GTK_COMBO (combo)->entry);

  if (check_func && items)
    {
      stp_string_list_t new_items = stp_string_list_create();
      num_items = stp_string_list_count(items);
      for (i = 0; i < num_items; i++)
	{
	  stp_param_string_t *param = stp_string_list_param(items, i);
	  if ((*check_func)(param->name))
	    stp_string_list_add_string(new_items, param->name, param->text);
	}
      items = new_items;
    }

  if (items)
    num_items = stp_string_list_count(items);

  if (*callback_id != -1)
    gtk_signal_disconnect (GTK_OBJECT (entry), *callback_id);
  gtk_entry_set_editable (entry, FALSE);

  if (!active || num_items == 0)
    {
      list = g_list_append (list, _("Standard"));
      gtk_combo_set_popdown_strings (GTK_COMBO (combo), list);
      *callback_id = -1;
      gtk_widget_set_sensitive (combo, FALSE);
      gtk_widget_hide (combo);
      if (label)
	gtk_widget_hide(label);
      if (check_func && items)
	stp_string_list_free(items);
      return;
    }

  for (i = 0; i < num_items; i ++)
    list = g_list_append(list, g_strdup(stp_string_list_param(items, i)->text));

  gtk_combo_set_popdown_strings (GTK_COMBO (combo), list);

  for (i = 0; i < num_items; i ++)
    if (strcmp(stp_string_list_param(items, i)->name, cur_item) == 0)
      break;

  if (i >= num_items && def_value)
    for (i = 0; i < num_items; i ++)
      if (strcmp(stp_string_list_param(items, i)->name, def_value) == 0)
	break;

  if (i >= num_items)
    i = 0;

  gtk_entry_set_text (entry, stp_string_list_param(items, i)->text);

  gtk_combo_set_value_in_list (GTK_COMBO (combo), TRUE, FALSE);
  gtk_widget_set_sensitive (combo, TRUE);
  gtk_widget_show (combo);
  if (label)
    gtk_widget_show(label);

  *callback_id = gtk_signal_connect (GTK_OBJECT (entry), "changed", callback,
				     data);
  if (check_func && items)
    stp_string_list_free(items);
}

void
stpui_set_image_dimensions(gint width, gint height)
{
  image_true_width = width;
  image_true_height = height;
}

void
stpui_set_image_resolution(gdouble xres, gdouble yres)
{
  image_xres = xres;
  image_yres = yres;
}

gint
stpui_compute_orientation(void)
{
  if ((printable_width >= printable_height &&
       image_true_width >= image_true_height) ||
      (printable_height >= printable_width &&
       image_true_height >= image_true_width))
    return ORIENT_PORTRAIT;
  else
    return ORIENT_LANDSCAPE;
}

static void
set_orientation(int orientation)
{
  pv->orientation = orientation;
  if (orientation == ORIENT_AUTO)
    orientation = stpui_compute_orientation();
  physical_orientation = orientation;
  switch (orientation)
    {
    case ORIENT_PORTRAIT:
    case ORIENT_UPSIDEDOWN:
      image_height = image_true_height;
      image_width = image_true_width;
      preview_thumbnail_h = thumbnail_h;
      preview_thumbnail_w = thumbnail_w;
      break;
    case ORIENT_LANDSCAPE:
    case ORIENT_SEASCAPE:
      image_height = image_true_width;
      image_width = image_true_height;
      preview_thumbnail_h = thumbnail_w;
      preview_thumbnail_w = thumbnail_h;
      break;
    }
  update_adjusted_thumbnail();
}

static void
position_button_callback(GtkWidget *widget, gpointer data)
{
  reset_preview();
  pv->invalid_mask |= (gint) data;
  preview_update ();
}

/*
 * position_callback() - callback for position entry widgets
 */
static void
position_callback (GtkWidget *widget)
{
  gdouble new_printed_value = atof (gtk_entry_get_text (GTK_ENTRY (widget)));
  gint new_value = SCALE(new_printed_value, units[pv->unit].scale);

  reset_preview ();
  suppress_preview_update++;

  if (widget == top_entry)
    stp_set_top(pv->v, new_value);
/*
  else if (widget == bottom_entry)
    stp_set_top(pv->v, new_value - print_height);
*/
  else if (widget == bottom_border_entry)
    stp_set_top (pv->v, paper_height - print_height - new_value);
  else if (widget == left_entry)
    stp_set_left (pv->v, new_value);
/*
  else if (widget == right_entry)
    stp_set_left(pv->v, new_value - print_width);
*/
  else if (widget == right_border_entry)
    stp_set_left (pv->v, paper_width - print_width - new_value);
  else if (widget == width_entry || widget == height_entry)
    {
      gboolean was_percent = (pv->scaling >= 0);
      if (pv->scaling >= 0)
	{
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scaling_ppi), TRUE);
	  scaling_callback (scaling_ppi);
	}
      if  (widget == width_entry)
	GTK_ADJUSTMENT (scaling_adjustment)->value =
	  ((gdouble) image_width) / (new_value / FINCH);
      else
	GTK_ADJUSTMENT (scaling_adjustment)->value =
	  ((gdouble) image_height) / (new_value / FINCH);
      gtk_adjustment_value_changed (GTK_ADJUSTMENT (scaling_adjustment));
      if (was_percent)
	{
	  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(scaling_percent),TRUE);
	  gtk_adjustment_value_changed(GTK_ADJUSTMENT (scaling_adjustment));
	}
    }

  suppress_preview_update--;
  preview_update ();
}

static void
set_adjustment_active(option_t *opt, gboolean active, gboolean do_toggle)
{
  GtkObject *adj = opt->info.flt.adjustment;
  if (do_toggle)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(opt->checkbox), active);
  gtk_widget_set_sensitive
    (GTK_WIDGET (SCALE_ENTRY_LABEL (adj)), active);
  gtk_widget_set_sensitive
    (GTK_WIDGET (SCALE_ENTRY_SCALE (adj)), active);
  gtk_widget_set_sensitive
    (GTK_WIDGET (SCALE_ENTRY_SPINBUTTON (adj)), active);
}

static void
set_combo_active(option_t *opt, gboolean active, gboolean do_toggle)
{
  if (do_toggle)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(opt->checkbox), active);
  gtk_widget_set_sensitive(GTK_WIDGET(opt->info.list.combo), active);
  gtk_widget_set_sensitive(GTK_WIDGET(opt->info.list.label), active);
}

static void
set_curve_active(option_t *opt, gboolean active, gboolean do_toggle)
{
  if (do_toggle)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(opt->checkbox), active);
  gtk_widget_set_sensitive(GTK_WIDGET(opt->info.curve.button), active);
  gtk_widget_set_sensitive(GTK_WIDGET(opt->info.curve.label), active);
  if (active)
    {
      if (opt->info.curve.is_visible)
	gtk_widget_show(GTK_WIDGET(opt->info.curve.dialog));
    }
  else
    gtk_widget_hide(GTK_WIDGET(opt->info.curve.dialog));
}

static void
set_bool_active(option_t *opt, gboolean active, gboolean do_toggle)
{
  if (do_toggle)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(opt->checkbox), active);
  gtk_widget_set_sensitive(GTK_WIDGET(opt->info.bool.checkbox), active);
}

static void
do_color_updates (void)
{
  int i;
  for (i = 0; i < current_option_count; i++)
    {
      option_t *opt = &(current_options[i]);
      if (opt->fast_desc->p_level <= MAXIMUM_PARAMETER_LEVEL)
	{
	  switch (opt->fast_desc->p_type)
	    {
	    case STP_PARAMETER_TYPE_DOUBLE:
	      gtk_adjustment_set_value
		(GTK_ADJUSTMENT(opt->info.flt.adjustment),
		 stp_get_float_parameter(pv->v, opt->fast_desc->name));
	      if (stp_check_float_parameter(pv->v, opt->fast_desc->name,
					    STP_PARAMETER_ACTIVE) ||
		  opt->fast_desc->is_mandatory)
		set_adjustment_active(opt, TRUE, TRUE);
	      else
		set_adjustment_active(opt, FALSE, TRUE);
	      break;
	    case STP_PARAMETER_TYPE_CURVE:
	      if (stp_check_curve_parameter(pv->v, opt->fast_desc->name,
					    STP_PARAMETER_ACTIVE) ||
		  opt->fast_desc->is_mandatory)
		set_curve_active(opt, TRUE, TRUE);
	      else
		set_curve_active(opt, FALSE, TRUE);
	      break;
	    case STP_PARAMETER_TYPE_STRING_LIST:
	      if (stp_check_string_parameter(pv->v, opt->fast_desc->name,
					     STP_PARAMETER_ACTIVE) ||
		  opt->fast_desc->is_mandatory)
		set_combo_active(opt, TRUE, TRUE);
	      else
		set_combo_active(opt, FALSE, TRUE);
	      break;
	    case STP_PARAMETER_TYPE_BOOLEAN:
	      if (stp_check_boolean_parameter(pv->v, opt->fast_desc->name,
					      STP_PARAMETER_ACTIVE) ||
		  opt->fast_desc->is_mandatory)
		set_bool_active(opt, TRUE, TRUE);
	      else
		set_bool_active(opt, FALSE, TRUE);
	      break;
	    default:
	      break;
	    }
	}
    }
  update_adjusted_thumbnail ();
}

static void
update_options(void)
{
  gtk_widget_hide(page_size_table);
  gtk_widget_hide(printer_features_table);
  gtk_widget_hide(color_adjustment_table);
  populate_options(pv->v);
  populate_option_table(page_size_table, STP_PARAMETER_CLASS_PAGE_SIZE);
  populate_option_table(printer_features_table, STP_PARAMETER_CLASS_FEATURE);
  populate_option_table(color_adjustment_table, STP_PARAMETER_CLASS_OUTPUT);
  gtk_widget_show(page_size_table);
  gtk_widget_show(printer_features_table);
  gtk_widget_show(color_adjustment_table);
  set_options_active();
}

static void
do_all_updates(void)
{
  gint i;
  suppress_preview_update++;
  set_orientation(pv->orientation);
  invalidate_preview_thumbnail ();
  preview_update ();

  if (pv->scaling < 0)
    {
      gdouble tmp = -pv->scaling;
      gdouble max_ppi_scaling;
      gdouble min_ppi_scaling;

      compute_scaling_limits(&min_ppi_scaling, &max_ppi_scaling);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scaling_ppi), TRUE);
      GTK_ADJUSTMENT (scaling_adjustment)->lower = min_ppi_scaling;
      GTK_ADJUSTMENT (scaling_adjustment)->upper = max_ppi_scaling;
      GTK_ADJUSTMENT (scaling_adjustment)->value = tmp;
      gtk_adjustment_changed (GTK_ADJUSTMENT (scaling_adjustment));
      gtk_adjustment_value_changed (GTK_ADJUSTMENT (scaling_adjustment));
    }
  else
    {
      gdouble tmp = pv->scaling;

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scaling_percent), TRUE);
      GTK_ADJUSTMENT (scaling_adjustment)->lower = minimum_image_percent;
      GTK_ADJUSTMENT (scaling_adjustment)->upper = 100.0;
      GTK_ADJUSTMENT (scaling_adjustment)->value = tmp;
      gtk_signal_emit_by_name (scaling_adjustment, "changed");
      gtk_signal_emit_by_name (scaling_adjustment, "value_changed");
    }

  for (i = 0; i < output_type_count; i++)
    {
      if (output_types[i].value == stp_get_output_type(pv->v))
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(output_types[i].button),
				     TRUE);
    }

  /*
   * Now get option parameters.
   */

  update_options();
  do_color_updates ();

  gtk_option_menu_set_history (GTK_OPTION_MENU (orientation_menu),
			       pv->orientation + 1);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(units[pv->unit].checkbox),
			       TRUE);
  gtk_label_set_text(GTK_LABEL(units_label), units[pv->unit].name);
  suppress_preview_update--;
  preview_update ();
}

/*
 *  plist_callback() - Update the current system printer.
 */
static void
plist_callback (GtkWidget *widget,
		gpointer   data)
{
  gint         i;

  suppress_preview_update++;
  invalidate_frame ();
  invalidate_preview_thumbnail ();
  reset_preview ();

  if (widget)
    {
      const gchar *result =
	gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(printer_combo)->entry));

      for (i = 0; i < stpui_plist_count; i++)
	{
	  if (! strcmp (result, stp_string_list_param(printer_list, i)->text))
	    {
	      stpui_plist_current = i;
	      break;
	    }
	}
    }
  else
    {
      stpui_plist_current = (gint) data;
    }

  pv = &(stpui_plist[stpui_plist_current]);

  if (strcmp(stp_get_driver(pv->v), ""))
    tmp_printer = stp_get_printer(pv->v);

  if (stp_get_output_type (stp_printer_get_defaults(tmp_printer)) ==
      OUTPUT_COLOR)
    {
      gtk_widget_set_sensitive (output_types[0].button, TRUE);
    }
  else
    {
      if (gtk_toggle_button_get_active
	  (GTK_TOGGLE_BUTTON (output_types[0].button)) == TRUE)
	gtk_toggle_button_set_active
	  (GTK_TOGGLE_BUTTON (output_types[1].button), TRUE);
      gtk_widget_set_sensitive (output_types[0].button, FALSE);
    }
  do_all_updates();

  setup_update ();
  do_all_updates();
  suppress_preview_update--;
  update_adjusted_thumbnail();
  preview_update ();
}

static void
show_all_paper_sizes_callback(GtkWidget *widget, gpointer data)
{
  int i;
  stpui_show_all_paper_sizes =
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  for (i = 0; i < current_option_count; i++)
    {
      option_t *option = &(current_options[i]);
      if (option->fast_desc &&
	  strcmp(option->fast_desc->name, "PageSize") == 0)
	{
	  build_a_combo(option);
	  break;
	}
    }
}

static void
custom_media_size_callback(GtkWidget *widget,
			   gpointer data)
{
  gint width_limit, height_limit;
  gint min_width_limit, min_height_limit;
  gdouble new_printed_value = atof(gtk_entry_get_text(GTK_ENTRY(widget)));
  gint new_value = SCALE(new_printed_value, units[pv->unit].scale);
  invalidate_frame ();
  invalidate_preview_thumbnail ();
  reset_preview ();

  stp_get_size_limit(pv->v, &width_limit, &height_limit,
		     &min_width_limit, &min_height_limit);
  if (widget == custom_size_width)
    {
      if (new_value < min_width_limit)
	new_value = min_width_limit;
      else if (new_value > width_limit)
	new_value = width_limit;
      stp_set_page_width (pv->v, new_value);
    }
  else
    {
      if (new_value < min_height_limit)
	new_value = min_height_limit;
      else if (new_value > height_limit)
	new_value = height_limit;
      stp_set_page_height (pv->v, new_value);
    }
  set_entry_value (widget, new_value, 0);
  preview_update ();
}


/*
 *  media_size_callback() - Update the current media size.
 */
static void
set_media_size(const gchar *new_media_size)
{
  static int setting_media_size = 0;
  const stp_papersize_t *pap = stp_get_papersize_by_name (new_media_size);

  if (setting_media_size)
    return;
  setting_media_size++;

  if (pap)
    {
      gint default_width, default_height;
      gint size;
      int old_width = stp_get_page_width(pv->v);
      int old_height = stp_get_page_height(pv->v);
      int need_preview_update = 0;

      if (! stpui_show_all_paper_sizes &&
	  (pap->paper_unit == PAPERSIZE_METRIC_EXTENDED ||
	   pap->paper_unit == PAPERSIZE_ENGLISH_EXTENDED))
	{
	  int i;
	  stp_parameter_t desc;
	  stp_describe_parameter(pv->v, "PageSize", &desc);
	  stp_set_string_parameter(pv->v, "PageSize", desc.deflt.str);
	  pap = stp_get_papersize_by_name(desc.deflt.str);
	  stp_parameter_description_free(&desc);
	  for (i = 0; i < current_option_count; i++)
	    {
	      option_t *option = &(current_options[i]);
	      if (option->fast_desc &&
		  strcmp(option->fast_desc->name, "PageSize") == 0)
		{
		  build_a_combo(option);
		  break;
		}
	    }
	}
	  
      if (pap->width == 0)
	{
	  stp_get_media_size(pv->v, &default_width, &default_height);
	  gtk_widget_set_sensitive (GTK_WIDGET (custom_size_width), TRUE);
	  gtk_entry_set_editable (GTK_ENTRY (custom_size_width), TRUE);
	  size = default_width;
	}
      else
	{
	  size = pap->width;
	  gtk_widget_set_sensitive (GTK_WIDGET (custom_size_width), FALSE);
	  gtk_entry_set_editable (GTK_ENTRY (custom_size_width), FALSE);
	}
      if (size != old_width)
	{
	  need_preview_update = 1;
	  set_entry_value (custom_size_width, size, 0);
	  stp_set_page_width (pv->v, size);
	}

      if (pap->height == 0)
	{
	  stp_get_media_size(pv->v, &default_height, &default_height);
	  gtk_widget_set_sensitive (GTK_WIDGET (custom_size_height), TRUE);
	  gtk_entry_set_editable (GTK_ENTRY (custom_size_height), TRUE);
	  size = default_height;
	}
      else
	{
	  size = pap->height;
	  gtk_widget_set_sensitive(GTK_WIDGET (custom_size_height), FALSE);
	  gtk_entry_set_editable (GTK_ENTRY (custom_size_height), FALSE);
	}
      if (size != old_height)
	{
	  need_preview_update = 1;
	  set_entry_value (custom_size_height, size, 0);
	  stp_set_page_height (pv->v, size);
	}
      if (need_preview_update)
	{
	  invalidate_preview_thumbnail();
	  invalidate_frame();
	  preview_update();
	}
    }
  setting_media_size--;
}

static void
combo_callback(GtkWidget *widget, gpointer data)
{
  option_t *option = (option_t *)data;
  const gchar *new_value =
    stpui_combo_get_name(option->info.list.combo, option->info.list.params);
  const gchar *value =
    stp_get_string_parameter(pv->v, option->fast_desc->name);
  reset_preview();
  if (!value || strcmp(value, new_value) != 0)
    {
      invalidate_frame();
      invalidate_preview_thumbnail();
      stp_set_string_parameter(pv->v, option->fast_desc->name, new_value);
      if (option->fast_desc->p_class == STP_PARAMETER_CLASS_PAGE_SIZE)
	set_media_size(new_value);
      if (option->fast_desc->p_class == STP_PARAMETER_CLASS_OUTPUT)
	update_adjusted_thumbnail();
      preview_update();
    }
}

/*
 *  orientation_callback() - Update the current media size.
 */
static void
orientation_callback (GtkWidget *widget,
		      gpointer   data)
{
  reset_preview ();

  if (pv->orientation != (gint) data)
    {
      invalidate_preview_thumbnail ();
      set_orientation((gint) data);
      preview_update ();
    }
}

/*
 *  output_type_callback() - Update the current output type.
 */
static void
output_type_callback (GtkWidget *widget,
		      gpointer   data)
{
  reset_preview ();

  if (GTK_TOGGLE_BUTTON (widget)->active)
    {
      if ((gint) data == OUTPUT_GRAY)
	gtk_widget_hide(output_color_vbox);
      else
	gtk_widget_show(output_color_vbox);
      stp_set_output_type (pv->v, (gint) data);
      invalidate_preview_thumbnail ();
      update_adjusted_thumbnail ();
      set_options_active();
      preview_update ();
      do_all_updates();
    }
}

static void
set_all_entry_values(void)
{
  set_entry_value (top_entry, (stp_get_top (pv->v)), 1);
  set_entry_value (left_entry, (stp_get_left (pv->v)), 1);
/*
  set_entry_value (bottom_entry, (top + stp_get_top(pv->v) + print_height), 1);
*/
  set_entry_value (bottom_border_entry,
                   (paper_height - (stp_get_top (pv->v) + print_height)), 1);
/*
  set_entry_value (right_entry, (stp_get_left(pv->v) + print_width), 1);
*/
  set_entry_value (right_border_entry,
                   (paper_width - (stp_get_left (pv->v) + print_width)), 1);
  set_entry_value (width_entry, print_width, 1);
  set_entry_value (height_entry, print_height, 1);
  set_entry_value (custom_size_width, stp_get_page_width (pv->v), 1);
  set_entry_value (custom_size_height, stp_get_page_height (pv->v), 1);
}

/*
 *  unit_callback() - Update the current unit.
 */
static void
unit_callback (GtkWidget *widget,
	       gpointer   data)
{
  reset_preview ();

  if (GTK_TOGGLE_BUTTON (widget)->active)
    {
      pv->unit = (gint) data;
      gtk_label_set_text(GTK_LABEL(units_label), units[pv->unit].name);
      set_all_entry_values();
    }
}

static void
destroy_dialogs (void)
{
  int i;
  gtk_widget_destroy (color_adjust_dialog);
  gtk_widget_destroy (setup_dialog);
  gtk_widget_destroy (print_dialog);
  gtk_widget_destroy (new_printer_dialog);
  gtk_widget_destroy (about_dialog);
  for (i = 0; i < current_option_count; i++)
    {
      if (current_options[i].fast_desc->p_type == STP_PARAMETER_TYPE_CURVE &&
	  current_options[i].info.curve.dialog)
	gtk_widget_destroy(current_options[i].info.curve.dialog);
    }
}

static void
dialogs_set_sensitive (gboolean sensitive)
{
  int i;
  gtk_widget_set_sensitive (color_adjust_dialog, sensitive);
  gtk_widget_set_sensitive (setup_dialog, sensitive);
  gtk_widget_set_sensitive (print_dialog, sensitive);
  gtk_widget_set_sensitive (new_printer_dialog, sensitive);
  gtk_widget_set_sensitive (about_dialog, sensitive);
  for (i = 0; i < current_option_count; i++)
    {
      if (current_options[i].fast_desc->p_type == STP_PARAMETER_TYPE_CURVE &&
	  current_options[i].info.curve.dialog)
	gtk_widget_set_sensitive(current_options[i].info.curve.dialog,
				 sensitive);
    }
}

/*
 * 'print_callback()' - Start the print.
 */
static void
print_callback (void)
{
  if (stpui_plist_current > 0)
    {
      runme = TRUE;
      destroy_dialogs ();
    }
  else
    {
      dialogs_set_sensitive (FALSE);
      gtk_widget_show (file_browser);
    }
}

/*
 *  printandsave_callback() -
 */
static void
printandsave_callback (void)
{
  saveme = TRUE;
  print_callback();
}

static void
about_callback (void)
{
  gtk_widget_show (about_dialog);
}

/*
 *  save_callback() - save settings, don't destroy dialog
 */
static void
save_callback (void)
{
  reset_preview ();
  stpui_printrc_save ();
}

/*
 *  setup_update() - update widgets in the setup dialog
 */
static void
setup_update (void)
{
  GtkAdjustment *adjustment;
  gint           idx;
  const char *ppd_file_name = stp_get_file_parameter(pv->v, "PPDFile");

  idx = stp_get_printer_index_by_driver (stp_get_driver (pv->v));

  gtk_clist_select_row (GTK_CLIST (printer_driver), idx, 0);
  gtk_label_set_text (GTK_LABEL (printer_model_label),
                      gettext (stp_printer_get_long_name (tmp_printer)));

  if (ppd_file_name)
    gtk_entry_set_text (GTK_ENTRY (ppd_file), ppd_file_name);
  else
    gtk_entry_set_text (GTK_ENTRY (ppd_file), "");

  if (stp_parameter_find_in_settings(pv->v, "PPDFile"))
    {
      gtk_widget_show (ppd_box);
      gtk_widget_show (ppd_label);
    }
  else
    {
      gtk_widget_hide (ppd_box);
      gtk_widget_hide (ppd_label);
    }

  gtk_entry_set_text (GTK_ENTRY (output_cmd), stpui_plist_get_output_to (pv));

  if (stpui_plist_current == 0)
    gtk_widget_hide (output_cmd);
  else
    gtk_widget_show (output_cmd);

  adjustment = GTK_CLIST (printer_driver)->vadjustment;
  gtk_adjustment_set_value
    (adjustment,
     adjustment->lower + idx * (adjustment->upper - adjustment->lower) /
     GTK_CLIST (printer_driver)->rows);
}

/*
 *  setup_open_callback() -
 */
static void
setup_open_callback (void)
{
  static gboolean first_time = TRUE;

  reset_preview ();
  setup_update ();

  gtk_widget_show (setup_dialog);

  if (first_time)
    {
      /* Make sure the driver scroller gets positioned correctly. */
      setup_update ();
      first_time = FALSE;
    }
}

/*
 *  new_printer_open_callback() -
 */
static void
new_printer_open_callback (void)
{
  reset_preview ();
  gtk_entry_set_text (GTK_ENTRY (new_printer_entry), "");
  gtk_widget_show (new_printer_dialog);
}

static void
set_printer(void)
{
  stp_set_driver (pv->v, stp_printer_get_driver (tmp_printer));
  stpui_plist_set_output_to (pv, gtk_entry_get_text (GTK_ENTRY (output_cmd)));
  stp_set_file_parameter (pv->v, "PPDFile",
			  gtk_entry_get_text (GTK_ENTRY (ppd_file)));
  gtk_label_set_text (GTK_LABEL (printer_model_label),
                      gettext (stp_printer_get_long_name (tmp_printer)));

  plist_callback (NULL, (gpointer) stpui_plist_current);
}

/*
 *  setup_ok_callback() -
 */
static void
setup_ok_callback (void)
{
  set_printer();
  gtk_widget_hide (setup_dialog);
}

/*
 *  setup_ok_callback() -
 */
static void
new_printer_ok_callback (void)
{
  const gchar *data = gtk_entry_get_text (GTK_ENTRY (new_printer_entry));
  stpui_plist_t   key;

  if (strlen(data))
    {
      memset(&key, 0, sizeof(key));
      stpui_printer_initialize (&key);
      stpui_plist_copy(&key, pv);
      stpui_plist_set_name(&key, data);

      key.active = 0;

      if (stpui_plist_add (&key, 1))
	{
	  stp_vars_free(key.v);
	  free(key.name);
	  free(key.output_to);
	  stpui_plist_current = stpui_plist_count - 1;
	  build_printer_combo ();
	  set_printer();
	}
    }

  gtk_widget_hide (new_printer_dialog);
}

/*
 *  print_driver_callback() - Update the current printer driver.
 */
static void
print_driver_callback (GtkWidget      *widget, /* I - Driver list */
		       gint            row,
		       gint            column,
		       GdkEventButton *event,
		       gpointer        data)   /* I - Data */
{
  static int calling_print_driver_callback = 0;
  if (calling_print_driver_callback)
    return;
  calling_print_driver_callback++;
  invalidate_frame ();
  invalidate_preview_thumbnail ();
  reset_preview ();
  data = gtk_clist_get_row_data (GTK_CLIST (widget), row);
  tmp_printer = stp_get_printer_by_index ((gint) data);

  { const stp_vars_t v = stp_printer_get_defaults(tmp_printer);
    if (stp_parameter_find_in_settings(v, "PPDFile")) {
      gtk_widget_show (ppd_label);
      gtk_widget_show (ppd_box);
    } else {
      gtk_widget_hide (ppd_label);
      gtk_widget_hide (ppd_box);
    }
  }
  calling_print_driver_callback--;
}

/*
 *  ppd_browse_callback() -
 */
static void
ppd_browse_callback (void)
{
  reset_preview ();
  gtk_file_selection_set_filename (GTK_FILE_SELECTION (ppd_browser),
				   gtk_entry_get_text (GTK_ENTRY (ppd_file)));
  gtk_widget_show (ppd_browser);
}

/*
 *  ppd_ok_callback() -
 */
static void
ppd_ok_callback (void)
{
  reset_preview ();
  gtk_widget_hide (ppd_browser);
  gtk_entry_set_text
    (GTK_ENTRY (ppd_file),
     gtk_file_selection_get_filename (GTK_FILE_SELECTION (ppd_browser)));
}

/*
 *  file_ok_callback() - print to file and go away
 */
static void
file_ok_callback (void)
{
  gtk_widget_hide (file_browser);
  stpui_plist_set_output_to
    (pv, gtk_file_selection_get_filename (GTK_FILE_SELECTION (file_browser)));

  runme = TRUE;
  destroy_dialogs ();
}

/*
 *  file_cancel_callback() -
 */
static void
file_cancel_callback (void)
{
  gtk_widget_hide (file_browser);
  dialogs_set_sensitive (TRUE);
}

static void
fill_buffer_writefunc(void *priv, const char *buffer, size_t bytes)
{
  int mask = 0;
  int i;
  int pixels = bytes / 4;
  priv_t *p = (priv_t *) priv;
  unsigned char *where = p->base_addr + p->offset;
  const unsigned char *xbuffer = (const unsigned char *)buffer;

  if (p->bpp == 1)
    {
      memcpy(where, buffer, bytes);
      p->offset += bytes;
    }
  else
    {
      if (bytes + p->offset > p->limit)
	bytes = p->limit - p->offset;
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cyan_button)))
	mask |= 1;
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(magenta_button)))
	mask |= 2;
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(yellow_button)))
	mask |= 4;
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(black_button)))
	mask |= 8;

      memset(where, 0xff, pixels * 3);
      for (i = 0; i < pixels; i++)
	{
	  if (mask & 8)
	    {
	      where[0] -= xbuffer[3];
	      where[1] -= xbuffer[3];
	      where[2] -= xbuffer[3];
	    }
	  if (mask & 1)
	    {
	      if (where[0] < xbuffer[0])
		where[0] = 0;
	      else
		where[0] -= xbuffer[0];
	    }
	  if (mask & 2)
	    {
	      if (where[1] < xbuffer[1])
		where[1] = 0;
	      else
		where[1] -= xbuffer[1];
	    }
	  if (mask & 4)
	    {
	      if (where[2] < xbuffer[2])
		where[2] = 0;
	      else
		where[2] -= xbuffer[2];
	    }
	  where += 3;
	  xbuffer += 4;
	}
      p->offset += pixels * 3;
    }
}

/*
 * update_adjusted_thumbnail()
 */

static void
redraw_color_swatch (void)
{
  static GdkGC *gc = NULL;
  static GdkColormap *cmap;

  if (adjusted_thumbnail_data && swatch && swatch->widget.window)
    {
      if (gc == NULL)
	{
	  gc = gdk_gc_new (swatch->widget.window);
	  cmap = gtk_widget_get_colormap (GTK_WIDGET(swatch));
	}

      if (stp_get_output_type(pv->v) == OUTPUT_GRAY)
	gdk_draw_gray_image(swatch->widget.window, gc, 0, 0,
			    thumbnail_w, thumbnail_h, GDK_RGB_DITHER_NORMAL,
			    adjusted_thumbnail_data, thumbnail_w);
      else
	gdk_draw_rgb_image(swatch->widget.window, gc, 0, 0,
			   thumbnail_w, thumbnail_h, GDK_RGB_DITHER_NORMAL,
			   adjusted_thumbnail_data, 3 * thumbnail_w);
    }
}

static void
initialize_thumbnail(void)
{
  int i;
  if (stpui_get_thumbnail_func())
    {
      const guchar *internal_thumbnail_data;
      /*
       * Fetch a thumbnail of the image we're to print from the Gimp.
       */

      thumbnail_w = thumbnail_hintw;
      thumbnail_h = thumbnail_hinth;
      internal_thumbnail_data =
	(stpui_get_thumbnail_func()) (stpui_get_thumbnail_data(), &thumbnail_w,
				      &thumbnail_h, &thumbnail_bpp, 0);
      if (adjusted_thumbnail_data)
	g_free(adjusted_thumbnail_data);
      if (preview_thumbnail_data)
	g_free(preview_thumbnail_data);
      if (thumbnail_data)
	g_free(thumbnail_data);

      if (internal_thumbnail_data)
	{
	  /*
	   * thumbnail_w and thumbnail_h have now been adjusted to the actual
	   * thumbnail dimensions.  Now initialize a color-adjusted version of
	   * the thumbnail.
	   */

	  adjusted_thumbnail_data = g_malloc (3 * thumbnail_w * thumbnail_h);
	  preview_thumbnail_data = g_malloc (3 * thumbnail_w * thumbnail_h);
	  thumbnail_data = g_malloc (3 * thumbnail_w * thumbnail_h);

	  switch (thumbnail_bpp)
	    {
	    case 1:
	      for (i = 0; i < thumbnail_w * thumbnail_h; i++)
		{
		  gint val = internal_thumbnail_data[i];
		  thumbnail_data[(3 * i) + 0] = val;
		  thumbnail_data[(3 * i) + 1] = val;
		  thumbnail_data[(3 * i) + 2] = val;
		}
	      break;
	    case 3:
	      memcpy(thumbnail_data, internal_thumbnail_data,
		     3 * thumbnail_w * thumbnail_h);
	      break;
	    case 2:
	      for (i = 0; i < thumbnail_w * thumbnail_h; i++)
		{
		  gint val = internal_thumbnail_data[2 * i];
		  gint alpha = internal_thumbnail_data[(2 * i) + 1];
		  thumbnail_data[(3 * i) +0] = val * alpha / 255 + 255 - alpha;
		  thumbnail_data[(3 * i) +1] = val * alpha / 255 + 255 - alpha;
		  thumbnail_data[(3 * i) +2] = val * alpha / 255 + 255 - alpha;
		}
	      break;
	    case 4:
	      for (i = 0; i < thumbnail_w * thumbnail_h; i++)
		{
		  gint r = internal_thumbnail_data[(4 * i)];
		  gint g = internal_thumbnail_data[(4 * i) + 1];
		  gint b = internal_thumbnail_data[(4 * i) + 2];
		  gint alpha = internal_thumbnail_data[(4 * i) + 3];
		  thumbnail_data[(3 * i) + 0] = r * alpha / 255 + 255 - alpha;
		  thumbnail_data[(3 * i) + 1] = g * alpha / 255 + 255 - alpha;
		  thumbnail_data[(3 * i) + 2] = b * alpha / 255 + 255 - alpha;
		}
	      break;
	    default:
	      break;
	      /* Whatever */
	    }
	  thumbnail_bpp = 3;
	}
      else
	{
	  thumbnail_h = 0;
	  thumbnail_w = 0;
	}
    }
  else
    {
      thumbnail_h = 0;
      thumbnail_w = 0;
    }
}

static int
compute_thumbnail(const stp_vars_t v)
{
  priv_t priv;
  int answer = 1;
  stp_image_t *im = stpui_image_thumbnail_new(thumbnail_data, thumbnail_w,
					      thumbnail_h, thumbnail_bpp);
  const stp_vars_t nv = stp_vars_create_copy(v);
  stp_set_printer_defaults(nv, stp_get_printer_by_driver("raw-data-8"));
  stp_set_top(nv, 0);
  stp_set_left(nv, 0);
  stp_set_width(nv, thumbnail_w);
  stp_set_height(nv, thumbnail_h);
  stp_set_outfunc(nv, fill_buffer_writefunc);
  stp_set_outdata(nv, &priv);
  stp_set_errfunc(nv, stpui_get_errfunc());
  stp_set_errdata(nv, stpui_get_errdata());
  if (stp_get_output_type(nv) == OUTPUT_COLOR)
    stp_set_output_type(nv, OUTPUT_RAW_CMYK);
  if (stp_get_output_type(nv) == OUTPUT_GRAY)
    {
      priv.bpp = 1;
      stp_set_string_parameter(nv, "InkType", "RGBGray");
    }
  else
    {
      priv.bpp = 4;
      stp_set_string_parameter(nv, "InkType", "CMYK");
    }
  stp_set_page_height(nv, thumbnail_h);
  stp_set_page_width(nv, thumbnail_w);
  stp_set_float_parameter (nv, "Density", 1.0);

  priv.base_addr = adjusted_thumbnail_data;
  priv.offset = 0;
  priv.limit = thumbnail_bpp * thumbnail_h * thumbnail_w;

  if (stp_verify(nv) != 1 || stp_print(nv, im) != 1)
    {
      answer = 0;
      fprintf(stderr, "Could not print thumbnail!\n");
    }
  stp_vars_free(nv);
  return answer;
}  

static void
set_thumbnail_orientation(void)
{
  gint           x, y;
  gint preview_limit = (thumbnail_h * thumbnail_w) - 1;
  gint bpp = stp_get_output_type(pv->v) == OUTPUT_GRAY ? 1 : 3;
  switch (physical_orientation)
    {
    case ORIENT_PORTRAIT:
      memcpy(preview_thumbnail_data, adjusted_thumbnail_data,
	     bpp * thumbnail_h * thumbnail_w);
      break;
    case ORIENT_SEASCAPE:
      for (x = 0; x < thumbnail_w; x++)
	for (y = 0; y < thumbnail_h; y++)
	  memcpy((preview_thumbnail_data + bpp * (x * thumbnail_h + y)),
		 (adjusted_thumbnail_data + bpp * (y * thumbnail_w + x)), bpp);
      break;

    case ORIENT_UPSIDEDOWN:
      for (x = 0; x < thumbnail_h * thumbnail_w; x++)
	memcpy((preview_thumbnail_data + bpp * (preview_limit - x)),
	       adjusted_thumbnail_data + bpp * x, bpp);
      break;
    case ORIENT_LANDSCAPE:
      for (x = 0; x < thumbnail_w; x++)
	for (y = 0; y < thumbnail_h; y++)
	  memcpy((preview_thumbnail_data +
		  bpp * (preview_limit - (x * thumbnail_h + y))),
		 (adjusted_thumbnail_data + bpp * (y * thumbnail_w + x)), bpp);
      break;
    }
}  

static void
update_adjusted_thumbnail (void)
{
  if (thumbnail_data && adjusted_thumbnail_data && do_update_thumbnail &&
      suppress_preview_update == 0)
    {
      if (compute_thumbnail(pv->v))
	{
	  set_thumbnail_orientation();
	  redraw_color_swatch ();
	  preview_update ();
	}
    }
}

static void
draw_arrow (GdkWindow *w,
            GdkGC     *gc,
            gint       paper_left,
            gint       paper_top)
{
  gint u  = preview_ppi/2;
  gint ox = paper_left + preview_ppi * paper_width / INCH / 2;
  gint oy = paper_top + preview_ppi * paper_height / INCH / 2;

  oy -= preview_ppi * paper_height / INCH / 4;
  if (oy < paper_top + u)
    oy = paper_top + u;
  gdk_draw_line (w, gc, ox, oy - u, ox - u, oy);
  gdk_draw_line (w, gc, ox, oy - u, ox + u, oy);
  gdk_draw_line (w, gc, ox, oy - u, ox, oy + u);
}

static void
create_valid_preview(guchar **preview_data)
{
  if (adjusted_thumbnail_data)
    {
      gint bpp = stp_get_output_type(pv->v) == OUTPUT_GRAY ? 1 : 3;
      gint v_denominator = preview_h > 1 ? preview_h - 1 : 1;
      gint v_numerator = (preview_thumbnail_h - 1) % v_denominator;
      gint v_whole = (preview_thumbnail_h - 1) / v_denominator;
      gint h_denominator = preview_w > 1 ? preview_w - 1 : 1;
      gint h_numerator = (preview_thumbnail_w - 1) % h_denominator;
      gint h_whole = (preview_thumbnail_w - 1) / h_denominator;
      gint adjusted_preview_width = bpp * preview_w;
      gint adjusted_thumbnail_width = bpp * preview_thumbnail_w;
      gint v_cur = 0;
      gint v_last = -1;
      gint v_error = v_denominator / 2;
      gint y;
      gint i;

      if (*preview_data)
	free (*preview_data);
      *preview_data = g_malloc (bpp * preview_h * preview_w);

      for (y = 0; y < preview_h; y++)
	{
	  guchar *outbuf = *preview_data + adjusted_preview_width * y;

	  if (v_cur == v_last)
	    memcpy (outbuf, outbuf-adjusted_preview_width,
		    adjusted_preview_width);
	  else
	    {
	      guchar *inbuf = preview_thumbnail_data - bpp
		+ adjusted_thumbnail_width * v_cur;

	      gint h_cur = 0;
	      gint h_last = -1;
	      gint h_error = h_denominator / 2;
	      gint x;

	      v_last = v_cur;
	      for (x = 0; x < preview_w; x++)
		{
		  if (h_cur == h_last)
		    {
		      for (i = 0; i < bpp; i++)
			outbuf[i] = outbuf[i - bpp];
		    }
		  else
		    {
		      inbuf += bpp * (h_cur - h_last);
		      h_last = h_cur;
		      for (i = 0; i < bpp; i++)
			outbuf[i] = inbuf[i];
		    }
		  outbuf += bpp;
		  h_cur += h_whole;
		  h_error += h_numerator;
		  if (h_error >= h_denominator)
		    {
		      h_error -= h_denominator;
		      h_cur++;
		    }
		}
	    }
	  v_cur += v_whole;
	  v_error += v_numerator;
	  if (v_error >= v_denominator)
	    {
	      v_error -= v_denominator;
	      v_cur++;
	    }
	}
      preview_valid = TRUE;
    }
}

/*
 *  preview_update_callback() -
 */
static void
do_preview_thumbnail (void)
{
  static GdkGC	*gc    = NULL;
  static GdkGC  *gcinv = NULL;
  static GdkGC  *gcset = NULL;
  static guchar *preview_data = NULL;
  gint    opx = preview_x;
  gint    opy = preview_y;
  gint    oph = preview_h;
  gint    opw = preview_w;
  gint paper_display_left, paper_display_top;
  gint printable_display_left, printable_display_top;
  gint paper_display_width, paper_display_height;
  gint printable_display_width, printable_display_height;
  int bottom = stp_get_top(pv->v) + stp_get_height(pv->v);
  int right = stp_get_left(pv->v) + stp_get_width(pv->v);

  preview_ppi = preview_size_horiz * FINCH / (gdouble) paper_width;

  if (preview_ppi > preview_size_vert * FINCH / (gdouble) paper_height)
    preview_ppi = preview_size_vert * FINCH / (gdouble) paper_height;
  if (preview_ppi > MAX_PREVIEW_PPI)
    preview_ppi = MAX_PREVIEW_PPI;

  if (preview == NULL || preview->widget.window == NULL)
    return;
  /*
   * Center the page on the preview
   */
  paper_display_width = MAX(3, ROUNDUP(preview_ppi * paper_width, INCH));
  paper_display_height = MAX(3, ROUNDUP(preview_ppi * paper_height, INCH));

  paper_display_left = (preview_size_horiz - paper_display_width) / 2;
  paper_display_top = (preview_size_vert - paper_display_height) / 2;

  printable_display_width =
    MAX(3, ROUNDUP(preview_ppi * printable_width, INCH));
  printable_display_height =
    MAX(3, ROUNDUP(preview_ppi * printable_height, INCH));

  printable_display_left = paper_display_left + preview_ppi * left / INCH;
  printable_display_top  = paper_display_top + preview_ppi * top / INCH ;

  preview_x =
    1 + paper_display_left + preview_ppi * stp_get_left (pv->v) / INCH;
  preview_y =
    1 + paper_display_top + preview_ppi * stp_get_top (pv->v) / INCH;

  if (!preview_valid)
    {
      gint preview_r = 1 + paper_display_left + preview_ppi * right / INCH;
      gint preview_b = 1 + paper_display_top + preview_ppi * bottom / INCH;
      preview_w = preview_r - preview_x;
      preview_h = preview_b - preview_y;
      if (preview_w >= printable_display_width)
	preview_w = printable_display_width - 1;
      if (preview_h >= printable_display_height)
	preview_h = printable_display_height - 1;
    }

  if (preview_w + preview_x > printable_display_left + printable_display_width)
    preview_x--;
  if (preview_h + preview_y > printable_display_top + printable_display_height)
    preview_y--;

  if (gc == NULL)
    {
      gc = gdk_gc_new (preview->widget.window);
      gcinv = gdk_gc_new (preview->widget.window);
      gdk_gc_set_function (gcinv, GDK_INVERT);
      gcset = gdk_gc_new (preview->widget.window);
      gdk_gc_set_function (gcset, GDK_SET);
    }

  if (!preview_valid)
    create_valid_preview(&preview_data);

  if (printable_display_left < paper_display_left)
    printable_display_left = paper_display_left;
  if (printable_display_top < paper_display_top)
    printable_display_top = paper_display_top;
  if (printable_display_left + printable_display_width >
      paper_display_left + paper_display_width)
    printable_display_width =
      paper_display_width - (paper_display_left - printable_display_left);
  if (printable_display_top + printable_display_height >
      paper_display_top + paper_display_height)
    printable_display_height =
      paper_display_height - (paper_display_top - printable_display_top);
  if (need_exposure)
    {
      /* draw paper frame */
      gdk_draw_rectangle (preview->widget.window, gc, 0,
			  paper_display_left, paper_display_top,
			  paper_display_width, paper_display_height);

      /* draw printable frame */
      gdk_draw_rectangle (preview->widget.window, gc, 0,
			  printable_display_left, printable_display_top,
			  printable_display_width, printable_display_height);
      need_exposure = FALSE;
    }
  else if (!frame_valid)
    {
      gdk_window_clear (preview->widget.window);
      /* draw paper frame */
      gdk_draw_rectangle (preview->widget.window, gc, 0,
			  paper_display_left, paper_display_top,
			  paper_display_width, paper_display_height);

      /* draw printable frame */
      gdk_draw_rectangle (preview->widget.window, gc, 0,
			  printable_display_left, printable_display_top,
			  printable_display_width, printable_display_height);
      frame_valid = TRUE;
    }
  else
    {
      if (opx + opw <= preview_x || opy + oph <= preview_y ||
	  preview_x + preview_w <= opx || preview_y + preview_h <= opy)
        {
          gdk_window_clear_area (preview->widget.window, opx, opy, opw, oph);
        }
      else
	{
	  if (opx < preview_x)
	    gdk_window_clear_area (preview->widget.window,
                                   opx, opy, preview_x - opx, oph);
	  if (opy < preview_y)
	    gdk_window_clear_area (preview->widget.window,
                                   opx, opy, opw, preview_y - opy);
	  if (opx + opw > preview_x + preview_w)
	    gdk_window_clear_area (preview->widget.window,
                                   preview_x + preview_w, opy,
                                   (opx + opw) - (preview_x + preview_w), oph);
	  if (opy + oph > preview_y + preview_h)
	    gdk_window_clear_area (preview->widget.window,
                                   opx, preview_y + preview_h,
                                   opw, (opy + oph) - (preview_y + preview_h));
	}
    }

  draw_arrow (preview->widget.window, gcset, paper_display_left,
	      paper_display_top);

  if (!preview_valid)
    gdk_draw_rectangle (preview->widget.window, gc, 1,
			preview_x, preview_y, preview_w, preview_h);
  else if (stp_get_output_type(pv->v) == OUTPUT_GRAY)
    gdk_draw_gray_image (preview->widget.window, gc,
			 preview_x, preview_y, preview_w, preview_h,
			 GDK_RGB_DITHER_NORMAL, preview_data, preview_w);
  else
    gdk_draw_rgb_image (preview->widget.window, gc,
			preview_x, preview_y, preview_w, preview_h,
			GDK_RGB_DITHER_NORMAL, preview_data, 3 * preview_w);

  /* If we're printing full bleed, redisplay the paper frame */
  if (preview_x <= paper_display_left || opx <= paper_display_left ||
      preview_y <= paper_display_top || opy <= paper_display_top ||
      preview_x + preview_w >= paper_display_left + paper_display_width ||
      opx + opw >= paper_display_left + paper_display_width ||
      preview_y + preview_h >= paper_display_top + paper_display_height ||
      opy + oph >= paper_display_top + paper_display_height)
    gdk_draw_rectangle (preview->widget.window, gc, 0,
			paper_display_left, paper_display_top,
			paper_display_width, paper_display_height);
    

  /* draw orientation arrow pointing to top-of-paper */
  draw_arrow (preview->widget.window, gcinv, paper_display_left,
	      paper_display_top);
  gdk_flush();
}

static void
preview_expose (void)
{
  need_exposure = TRUE;
  preview_update ();
}

static void
preview_update (void)
{
  gdouble max_ppi_scaling;   /* Maximum PPI for current page size */
  gdouble min_ppi_scaling;   /* Minimum PPI for current page size */

  suppress_preview_update++;
  stp_get_media_size(pv->v, &paper_width, &paper_height);

  stp_get_imageable_area(pv->v, &left, &right, &bottom, &top);

  printable_width  = right - left;
  printable_height = bottom - top;

  if (pv->scaling < 0)
    {
      gdouble twidth;

      compute_scaling_limits(&min_ppi_scaling, &max_ppi_scaling);

      if (pv->scaling < 0 && pv->scaling > -min_ppi_scaling)
	pv->scaling = -min_ppi_scaling;

      twidth = (FINCH * (gdouble) image_width / -pv->scaling);
      print_width = twidth + .5;
      print_height = (twidth * (gdouble) image_height / image_width) + .5;
      GTK_ADJUSTMENT (scaling_adjustment)->lower = min_ppi_scaling;
      GTK_ADJUSTMENT (scaling_adjustment)->upper = max_ppi_scaling;
      GTK_ADJUSTMENT (scaling_adjustment)->value = -pv->scaling;

      if (!suppress_scaling_adjustment)
	{
	  suppress_preview_reset++;
	  gtk_adjustment_changed (GTK_ADJUSTMENT (scaling_adjustment));
	  suppress_scaling_callback = TRUE;
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scaling_ppi), TRUE);
	  suppress_scaling_callback = FALSE;
	  gtk_adjustment_value_changed (GTK_ADJUSTMENT (scaling_adjustment));
	  suppress_preview_reset--;
	}
    }
  else
    {
      /* we do pv->scaling % of height or width, whatever is less */
      /* this is relative to printable size */
      if (image_width * printable_height > printable_width * image_height)
	/* if image_width/image_height > printable_width/printable_height */
	/* i.e. if image is wider relative to its height than the width
	   of the printable area relative to its height */
	{
	  gdouble twidth = .5 + printable_width * pv->scaling / 100;

	  print_width = twidth;
	  print_height = twidth * (gdouble) image_height /
	    (gdouble) image_width;
	}
      else
	{
	  gdouble theight = .5 + printable_height * pv->scaling /100;

	  print_height = theight;
	  print_width = theight * (gdouble) image_width /
	    (gdouble) image_height;
	}
    }
  stp_set_width(pv->v, print_width);
  stp_set_height(pv->v, print_height);

  if (pv->invalid_mask & INVALID_LEFT)
    stp_set_left (pv->v, (paper_width - print_width) / 2);

  if (stp_get_left(pv->v) < left)
    stp_set_left(pv->v, left);

  if (stp_get_left (pv->v) > right - print_width)
    stp_set_left (pv->v, right - print_width);

  if (pv->invalid_mask & INVALID_TOP)
    stp_set_top (pv->v, ((paper_height - print_height) / 2));
  if (stp_get_top(pv->v) < top)
    stp_set_top(pv->v, top);

  if (stp_get_top (pv->v) > bottom - print_height)
    stp_set_top (pv->v, bottom - print_height);

  pv->invalid_mask = 0;

  set_all_entry_values();
  suppress_preview_update--;

  /* draw image */
  if (! suppress_preview_update)
    do_preview_thumbnail ();
}

/*
 *  preview_button_callback() -
 */
static void
preview_button_callback (GtkWidget      *widget,
			 GdkEventButton *event,
			 gpointer        data)
{
  if (event->type == GDK_BUTTON_PRESS)
    {
      if (preview_active == 0)
	{
	  mouse_x = event->x;
	  mouse_y = event->y;
	  orig_left = stp_get_left (pv->v);
	  orig_top = stp_get_top (pv->v);
	  mouse_button = event->button;
	  buttons_mask = 1 << event->button;
	  buttons_pressed++;
	  preview_active = 1;
	  stpui_disable_help();
	  move_constraint =
	    (event->state & GDK_SHIFT_MASK) ? MOVE_CONSTRAIN : MOVE_ANY;
	}
      else if ((buttons_mask & (1 << event->button)) == 0)
	{
	  if (preview_active == 1)
	    {
	      stpui_enable_help();
	      preview_active = -1;
	      stp_set_left (pv->v, orig_left);
	      stp_set_top (pv->v, orig_top);
	      preview_update ();
	    }
	  buttons_mask |= 1 << event->button;
	  buttons_pressed++;
	}
    }
  else if (event->type == GDK_BUTTON_RELEASE)
    {
      buttons_pressed--;
      buttons_mask &= ~(1 << event->button);
      if (buttons_pressed == 0)
	{
	  stpui_enable_help ();
	  preview_active = 0;
	}
    }
}

/*
 *  preview_motion_callback() -
 */
static void
preview_motion_callback (GtkWidget      *widget,
			 GdkEventMotion *event,
			 gpointer        data)
{

  gint old_top  = stp_get_top (pv->v);
  gint old_left = stp_get_left (pv->v);
  gint new_top  = old_top;
  gint new_left = old_left;
  gint steps;
  if (preview_active != 1 || event->type != GDK_MOTION_NOTIFY)
    return;
  if (move_constraint == MOVE_CONSTRAIN)
    {
      int dx = abs(event->x - mouse_x);
      int dy = abs(event->y - mouse_y);
      if (dx > dy && dx > 3)
	move_constraint = MOVE_HORIZONTAL;
      else if (dy > dx && dy > 3)
	move_constraint = MOVE_VERTICAL;
      else
	return;
    }

  switch (mouse_button)
    {
    case 1:
      if (move_constraint & MOVE_VERTICAL)
	new_top = orig_top + INCH * (event->y - mouse_y) / preview_ppi;
      if (move_constraint & MOVE_HORIZONTAL)
	new_left = orig_left + INCH * (event->x - mouse_x) / preview_ppi;
      break;
    case 3:
      if (move_constraint & MOVE_VERTICAL)
	new_top = orig_top + event->y - mouse_y;
      if (move_constraint & MOVE_HORIZONTAL)
	new_left = orig_left + event->x - mouse_x;
      break;
    case 2:
      if (move_constraint & MOVE_HORIZONTAL)
	{
	  gint x_threshold = MAX (1, (preview_ppi * print_width) / INCH);
	  if (event->x > mouse_x)
	    steps = MIN((event->x - mouse_x) / x_threshold,
			((right - orig_left) / print_width) - 1);
	  else
	    steps = -(MIN((mouse_x - event->x) / x_threshold,
			  (orig_left - left) / print_width));
	  new_left = orig_left + steps * print_width;
	}
      if (move_constraint & MOVE_VERTICAL)
	{
	  gint y_threshold = MAX (1, (preview_ppi * print_height) / INCH);
	  if (event->y > mouse_y)
	    steps = MIN((event->y - mouse_y) / y_threshold,
			((bottom - orig_top) / print_height) - 1);
	  else
	    steps = -(MIN((mouse_y - event->y) / y_threshold,
			  (orig_top - top) / print_height));
	  new_top = orig_top + steps * print_height;
	}
      break;
    }

  if (new_top < top)
    new_top = top;
  if (new_top > bottom - print_height)
    new_top = bottom - print_height;
  if (new_left < left)
    new_left = left;
  if (new_left > right - print_width)
    new_left = right - print_width;

  if (new_top != old_top || new_left != old_left)
    {
      stp_set_top (pv->v, new_top);
      stp_set_left (pv->v, new_left);
      preview_update ();
    }
}

static void
color_update (GtkAdjustment *adjustment)
{
  int i;
  for (i = 0; i < current_option_count; i++)
    {
      option_t *opt = &(current_options[i]);
      if (opt->fast_desc->p_type == STP_PARAMETER_TYPE_DOUBLE &&
	  opt->fast_desc->p_level <= MAXIMUM_PARAMETER_LEVEL &&
	  opt->info.flt.adjustment &&
	  adjustment == GTK_ADJUSTMENT(opt->info.flt.adjustment))
	{
	  invalidate_preview_thumbnail ();
	  if (stp_get_float_parameter(pv->v, opt->fast_desc->name) !=
	      adjustment->value)
	    {
	      stp_set_float_parameter(pv->v, opt->fast_desc->name,
				      adjustment->value);
	      update_adjusted_thumbnail();
	    }
	}
    }
}

static void
set_controls_active (GtkObject *checkbutton, gpointer xopt)
{
  option_t *opt = (option_t *) xopt;
  stp_parameter_t desc;
  gboolean setting =
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbutton));
  if (setting && opt->fast_desc->p_level <= MAXIMUM_PARAMETER_LEVEL)
    {
      switch (opt->fast_desc->p_type)
	{
	case STP_PARAMETER_TYPE_DOUBLE:
	  set_adjustment_active(opt, TRUE, FALSE);
	  if (! stp_check_float_parameter(pv->v, opt->fast_desc->name,
					  STP_PARAMETER_INACTIVE))
	    {
	      stp_describe_parameter(pv->v, opt->fast_desc->name, &desc);
	      stp_set_float_parameter(pv->v, opt->fast_desc->name,
				      desc.deflt.dbl);
	      stp_parameter_description_free(&desc);
	    }
	  stp_set_float_parameter_active(pv->v, opt->fast_desc->name,
					 STP_PARAMETER_ACTIVE);
	  break;
	case STP_PARAMETER_TYPE_CURVE:
	  set_curve_active(opt, TRUE, FALSE);
	  if (! stp_check_curve_parameter(pv->v, opt->fast_desc->name,
					  STP_PARAMETER_INACTIVE))
	    {
	      stp_describe_parameter(pv->v, opt->fast_desc->name, &desc);
	      stp_set_curve_parameter(pv->v, opt->fast_desc->name,
				      desc.deflt.curve);
	      stp_parameter_description_free(&desc);
	    }
	  stp_set_curve_parameter_active(pv->v, opt->fast_desc->name,
					 STP_PARAMETER_ACTIVE);
	  break;
	case STP_PARAMETER_TYPE_STRING_LIST:
	  set_combo_active(opt, TRUE, FALSE);
	  if (! stp_check_string_parameter(pv->v, opt->fast_desc->name,
					   STP_PARAMETER_INACTIVE))
	    {
	      stp_describe_parameter(pv->v, opt->fast_desc->name, &desc);
	      stp_set_string_parameter(pv->v, opt->fast_desc->name,
				       desc.deflt.str);
	      stp_parameter_description_free(&desc);
	    }
	  stp_set_string_parameter_active(pv->v, opt->fast_desc->name,
					  STP_PARAMETER_ACTIVE);
	  break;
	case STP_PARAMETER_TYPE_BOOLEAN:
	  set_bool_active(opt, TRUE, FALSE);
	  if (! stp_check_boolean_parameter(pv->v, opt->fast_desc->name,
					    STP_PARAMETER_INACTIVE))
	    {
	      stp_describe_parameter(pv->v, opt->fast_desc->name, &desc);
	      stp_set_boolean_parameter(pv->v, opt->fast_desc->name,
					desc.deflt.boolean);
	      stp_parameter_description_free(&desc);
	    }
	  stp_set_boolean_parameter_active(pv->v, opt->fast_desc->name,
					   STP_PARAMETER_ACTIVE);
	  break;
	default:
	  break;
	}
    }
  else
    {
      switch (opt->fast_desc->p_type)
	{
	case STP_PARAMETER_TYPE_DOUBLE:
	  set_adjustment_active(opt, FALSE, FALSE);
	  stp_set_float_parameter_active(pv->v, opt->fast_desc->name,
					 STP_PARAMETER_INACTIVE);
	  break;
	case STP_PARAMETER_TYPE_CURVE:
	  set_curve_active(opt, FALSE, FALSE);
	  stp_set_curve_parameter_active(pv->v, opt->fast_desc->name,
					 STP_PARAMETER_INACTIVE);
	  break;
	case STP_PARAMETER_TYPE_STRING_LIST:
	  set_combo_active(opt, FALSE, FALSE);
	  stp_set_string_parameter_active(pv->v, opt->fast_desc->name,
					  STP_PARAMETER_INACTIVE);
	  break;
	case STP_PARAMETER_TYPE_BOOLEAN: /* ??? */
	  set_bool_active(opt, FALSE, FALSE);
	  stp_set_boolean_parameter_active(pv->v, opt->fast_desc->name,
					   STP_PARAMETER_INACTIVE);
	  break;
	default:
	  break;
	}
    }   
  invalidate_preview_thumbnail();
  update_adjusted_thumbnail();
}

static void
set_color_defaults (void)
{
  int i;
  for (i = 0; i < current_option_count; i++)
    {
      option_t *opt = &(current_options[i]);
      if (opt->fast_desc->p_type == STP_PARAMETER_TYPE_DOUBLE &&
	  opt->fast_desc->p_level <= MAXIMUM_PARAMETER_LEVEL &&
	  opt->fast_desc->p_class == STP_PARAMETER_CLASS_OUTPUT)
	{
	  stp_parameter_activity_t active =
	    stp_get_float_parameter_active(pv->v, opt->fast_desc->name);
	  stp_set_float_parameter(pv->v, opt->fast_desc->name,
				  opt->info.flt.deflt);
	  stp_set_float_parameter_active(pv->v, opt->fast_desc->name, active);
	}
    }

  do_color_updates ();
}

gint
stpui_do_print_dialog(void)
{
  /*
   * Get printrc options...
   */
  stpui_printrc_load ();

  /*
   * Print dialog window...
   */
  create_main_window();

  gtk_main ();
  gdk_flush ();

  /*
   * Set printrc options...
   */
  if (saveme)
    stpui_printrc_save ();

  /*
   * Return ok/cancel...
   */
  return (runme);
}
