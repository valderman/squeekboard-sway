/*
 * Copyright (C) 2010-2011 Daiki Ueno <ueno@unixuser.org>
 * Copyright (C) 2010-2011 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "config.h"

#include <math.h>
#include <string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "eek-renderer.h"

enum {
    PROP_0,
    PROP_PCONTEXT,
    PROP_LAST
};

typedef struct _EekRendererPrivate
{
    LevelKeyboard *keyboard;
    PangoContext *pcontext;
    GtkCssProvider *css_provider;
    GtkStyleContext *layout_context;
    GtkStyleContext *button_context; // TODO: maybe move a copy to each button

    EekColor default_foreground_color;
    EekColor default_background_color;
    gdouble border_width;

    gdouble allocation_width;
    gdouble allocation_height;
    gdouble scale;
    gint scale_factor; /* the outputs scale factor */
    gint origin_x;
    gint origin_y;

    PangoFontDescription *ascii_font;
    PangoFontDescription *font;
    GHashTable *outline_surface_cache;
    GHashTable *active_outline_surface_cache;
    GHashTable *icons;
    cairo_surface_t *keyboard_surface;

} EekRendererPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EekRenderer, eek_renderer, G_TYPE_OBJECT)

static const EekColor DEFAULT_FOREGROUND_COLOR = {0.3, 0.3, 0.3, 1.0};
static const EekColor DEFAULT_BACKGROUND_COLOR = {1.0, 1.0, 1.0, 1.0};

/* eek-keyboard-drawing.c */
extern void _eek_rounded_polygon               (cairo_t     *cr,
                                                gdouble      radius,
                                                EekPoint    *points,
                                                guint         num_points);

static void eek_renderer_real_render_button_label (EekRenderer *self,
                                                PangoLayout *layout,
                                                const struct squeek_button *button);

static void invalidate                         (EekRenderer *renderer);
static void render_button                         (EekRenderer *self,
                                                cairo_t     *cr, struct button_place *place,
                                                gboolean     active);

struct _CreateKeyboardSurfaceCallbackData {
    cairo_t *cr;
    EekRenderer *renderer;
    struct squeek_view *view;
    struct squeek_row *row;
};
typedef struct _CreateKeyboardSurfaceCallbackData CreateKeyboardSurfaceCallbackData;

static void
create_keyboard_surface_button_callback (struct squeek_button *button,
                                      gpointer    user_data)
{
    CreateKeyboardSurfaceCallbackData *data = user_data;
    EekBounds bounds = squeek_button_get_bounds(button);

    cairo_save (data->cr);

    cairo_translate (data->cr, bounds.x, bounds.y);
    cairo_rectangle (data->cr,
                     0.0,
                     0.0,
                     bounds.width + 100,
                     bounds.height + 100);
    cairo_clip (data->cr);
    struct button_place place = {
        .row = data->row,
        .button = button,
    };
    render_button (data->renderer, data->cr, &place, FALSE);

    cairo_restore (data->cr);
}

static void
create_keyboard_surface_row_callback (struct squeek_row *row,
                                          gpointer    user_data)
{
    CreateKeyboardSurfaceCallbackData *data = user_data;

    EekBounds bounds = squeek_row_get_bounds(row);

    cairo_save (data->cr);
    cairo_translate (data->cr, bounds.x, bounds.y);

    gint angle = squeek_row_get_angle (row);
    cairo_rotate (data->cr, angle * G_PI / 180);

    data->row = row;
    squeek_row_foreach(row, create_keyboard_surface_button_callback, data);

    cairo_restore (data->cr);
}

static void
render_keyboard_surface (EekRenderer *renderer, struct squeek_view *view)
{
    EekRendererPrivate *priv = eek_renderer_get_instance_private (renderer);
    EekColor foreground;

    eek_renderer_get_foreground_color (renderer, priv->layout_context, &foreground);

    EekBounds bounds = squeek_view_get_bounds (level_keyboard_current(priv->keyboard));

    CreateKeyboardSurfaceCallbackData data = {
        .cr = cairo_create (priv->keyboard_surface),
        .renderer = renderer,
        .view = view,
    };

    /* Paint the background covering the entire widget area */
    gtk_render_background (priv->layout_context,
                           data.cr,
                           0, 0,
                           priv->allocation_width, priv->allocation_height);
    gtk_render_frame (priv->layout_context,
                      data.cr,
                      0, 0,
                      priv->allocation_width, priv->allocation_height);

    cairo_save (data.cr);
    cairo_scale (data.cr, priv->scale, priv->scale);
    cairo_translate (data.cr, bounds.x, bounds.y);

    cairo_set_source_rgba (data.cr,
                           foreground.red,
                           foreground.green,
                           foreground.blue,
                           foreground.alpha);

    /* draw rows */
    squeek_view_foreach(level_keyboard_current(priv->keyboard),
                        create_keyboard_surface_row_callback,
                        &data);
    cairo_restore (data.cr);

    cairo_destroy (data.cr);
}

static void
render_outline (cairo_t     *cr,
                GtkStyleContext *ctx,
                EekBounds bounds)
{
    gtk_render_background (ctx, cr, 0, 0, bounds.width, bounds.height);
    gtk_render_frame (ctx, cr, 0, 0, bounds.width, bounds.height);
}

static void
render_button (EekRenderer *self,
            cairo_t     *cr,
            struct button_place *place,
            gboolean     active)
{
    EekRendererPrivate *priv = eek_renderer_get_instance_private (self);
    cairo_surface_t *outline_surface;
    GHashTable *outline_surface_cache;
    PangoLayout *layout;
    PangoRectangle extents = { 0, };
    EekColor foreground;

    GtkStyleContext *ctx = priv->button_context;
    /* Set the name of the button on the widget path, using the name obtained
       from the button's symbol. */
    g_autoptr (GtkWidgetPath) path = NULL;
    path = gtk_widget_path_copy (gtk_style_context_get_path (ctx));
    const char *name = squeek_button_get_name(place->button);
    gtk_widget_path_iter_set_name (path, -1, name);

    /* Update the style context with the updated widget path. */
    gtk_style_context_set_path (ctx, path);
    /* Set the state to take into account whether the button is active
       (pressed) or normal. */
    gtk_style_context_set_state(ctx,
        active ? GTK_STATE_FLAG_ACTIVE : GTK_STATE_FLAG_NORMAL);


    /* render outline */
    EekBounds bounds = squeek_button_get_bounds(place->button);

    if (active)
        outline_surface_cache = priv->active_outline_surface_cache;
    else
        outline_surface_cache = priv->outline_surface_cache;

    outline_surface = g_hash_table_lookup (outline_surface_cache, place->button);
    if (!outline_surface) {
        cairo_t *cr;

        // Outline will be drawn on the outside of the button, so the
        // surface needs to be bigger than the button
        outline_surface =
            cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        (int)ceil(bounds.width) + 10,
                                        (int)ceil(bounds.height) + 10);
        cr = cairo_create (outline_surface);

        /* blank background */
        cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
        cairo_paint (cr);

        cairo_save (cr);
        eek_renderer_apply_transformation_for_button (self, cr, place, 1.0, FALSE);
        render_outline (cr, ctx, bounds);
        cairo_restore (cr);

        cairo_destroy (cr);

        g_hash_table_insert (outline_surface_cache,
                             (gpointer)place->button,
                             outline_surface);
    }

    cairo_set_source_surface (cr, outline_surface, 0.0, 0.0);
    cairo_paint (cr);

    eek_renderer_get_foreground_color (self, ctx, &foreground);
    /* render icon (if any) */
    const char *icon_name = squeek_button_get_icon_name(place->button);

    if (icon_name) {
        gint scale = priv->scale_factor;
        cairo_surface_t *icon_surface =
            eek_renderer_get_icon_surface (self, icon_name, 16 / priv->scale,
                                           scale);
        if (icon_surface) {
            gint width = cairo_image_surface_get_width (icon_surface);
            gint height = cairo_image_surface_get_height (icon_surface);

            cairo_save (cr);
            cairo_translate (cr,
                             (bounds.width - (double)width / scale) / 2,
                             (bounds.height - (double)height / scale) / 2);
            cairo_rectangle (cr, 0, 0, width, height);
            cairo_clip (cr);
            /* Draw the shape of the icon using the foreground color */
            cairo_set_source_rgba (cr, foreground.red,
                                       foreground.green,
                                       foreground.blue,
                                       foreground.alpha);
            cairo_mask_surface (cr, icon_surface, 0.0, 0.0);
            cairo_fill (cr);

            cairo_restore (cr);
            return;
        }
    }

    /* render label */
    layout = pango_cairo_create_layout (cr);
    eek_renderer_real_render_button_label (self, layout, place->button);
    pango_layout_get_extents (layout, NULL, &extents);

    cairo_save (cr);
    cairo_move_to
        (cr,
         (bounds.width - (double)extents.width / PANGO_SCALE) / 2,
         (bounds.height - (double)extents.height / PANGO_SCALE) / 2);

    cairo_set_source_rgba (cr,
                           foreground.red,
                           foreground.green,
                           foreground.blue,
                           foreground.alpha);

    gtk_style_context_set_state(ctx, GTK_STATE_FLAG_NORMAL);
    pango_cairo_show_layout (cr, layout);
    cairo_restore (cr);
    g_object_unref (layout);
}

/**
 * eek_renderer_apply_transformation_for_key:
 * @self: The renderer used to render the key
 * @cr: The Cairo rendering context used for rendering
 * @key: The key to be transformed
 * @scale: The factor used to scale the key bounds before rendering
 * @rotate: Whether to rotate the key by the angle defined for the key's
 *   in its section definition
 *
 *  Applies a transformation, consisting of scaling and rotation, to the
 *  current rendering context using the bounds for the given key. The scale
 *  factor is separate to the normal scale factor for the keyboard as a whole
 *  and is applied cumulatively. It is typically used to render larger than
 *  normal keys for popups.
*/
void
eek_renderer_apply_transformation_for_button (EekRenderer *self,
                                           cairo_t     *cr,
                                           struct button_place *place,
                                           gdouble      scale,
                                           gboolean     rotate)
{
    EekBounds bounds, rotated_bounds;
    gdouble s;

    eek_renderer_get_button_bounds (self, place, &bounds, FALSE);
    eek_renderer_get_button_bounds (self, place, &rotated_bounds, TRUE);

    gint angle = squeek_row_get_angle (place->row);

    cairo_scale (cr, scale, scale);
    if (rotate) {
        s = sin (angle * G_PI / 180);
        if (s < 0)
            cairo_translate (cr, 0, - bounds.width * s);
        else
            cairo_translate (cr, bounds.height * s, 0);
        cairo_rotate (cr, angle * G_PI / 180);
    }
}

static void
eek_renderer_real_render_button_label (EekRenderer *self,
                                    PangoLayout *layout,
                                    const struct squeek_button *button)
{
    EekRendererPrivate *priv = eek_renderer_get_instance_private (self);

    const gchar *label = squeek_button_get_label(button);

    if (!label) {
        return;
    }

    PangoFontDescription *font;
    PangoLayoutLine *line;
    gdouble scale;


    if (!priv->font) {
        const PangoFontDescription *base_font;
        gdouble ascii_size, size;

        base_font = pango_context_get_font_description (priv->pcontext);
        // FIXME: Base font size on the same size unit used for button sizing,
        // and make the default about 1/3 of the current row height
        ascii_size = 30000.0;
        priv->ascii_font = pango_font_description_copy (base_font);
        pango_font_description_set_size (priv->ascii_font,
                                         (gint)round(ascii_size));

        size = 30000.0;
        priv->font = pango_font_description_copy (base_font);
        pango_font_description_set_size (priv->font, (gint)round(size * 0.6));
    }

    EekBounds bounds = squeek_button_get_bounds(button);
    scale = MIN((bounds.width - priv->border_width) / bounds.width,
                (bounds.height - priv->border_width) / bounds.height);

    font = pango_font_description_copy (priv->font);
    pango_font_description_set_size (font,
                                     (gint)round(pango_font_description_get_size (font) * scale));
    pango_layout_set_font_description (layout, font);
    pango_font_description_free (font);

    pango_layout_set_text (layout, label, -1);
    line = pango_layout_get_line (layout, 0);
    if (line->resolved_dir == PANGO_DIRECTION_RTL)
        pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);
    pango_layout_set_width (layout,
                            PANGO_SCALE * bounds.width * scale);
}

/*
 * eek_renderer_real_render_key:
 * @self: The renderer used to render the key
 * @cr: The Cairo rendering context used for rendering
 * @key: The key to be transformed
 * @scale: The factor used to scale the key bounds before rendering
 * @rotate: Whether to rotate the key by the angle defined for the key's
 *   in its section definition
 *
 *   Renders a key separately from the normal keyboard rendering.
*/
static void
eek_renderer_real_render_button (EekRenderer *self,
                              cairo_t     *cr,
                              struct button_place *place,
                              gdouble      scale,
                              gboolean     rotate)
{
    EekRendererPrivate *priv = eek_renderer_get_instance_private (self);
    EekBounds bounds;

    eek_renderer_get_button_bounds (self, place, &bounds, rotate);

    cairo_save (cr);
    /* Because this function is called separately from the keyboard rendering
       function, the transformation for the context needs to be set up */
    cairo_translate (cr, priv->origin_x, priv->origin_y);
    cairo_scale (cr, priv->scale, priv->scale);
    cairo_translate (cr, bounds.x, bounds.y);

    eek_renderer_apply_transformation_for_button (self, cr, place, scale, rotate);
    struct squeek_key *key = squeek_button_get_key(place->button);
    render_button (
                self, cr, place,
                squeek_key_is_pressed(key) || squeek_key_is_locked (key)
    );
    cairo_restore (cr);
}

static void
eek_renderer_real_render_keyboard (EekRenderer *self,
                                   cairo_t     *cr)
{
    EekRendererPrivate *priv = eek_renderer_get_instance_private (self);
    cairo_pattern_t *source;

    g_return_if_fail (priv->keyboard);
    g_return_if_fail (priv->allocation_width > 0.0);
    g_return_if_fail (priv->allocation_height > 0.0);

    cairo_save (cr);

    cairo_translate (cr, priv->origin_x, priv->origin_y);

    if (priv->keyboard_surface)
        cairo_surface_destroy (priv->keyboard_surface);

    priv->keyboard_surface = cairo_surface_create_for_rectangle (
        cairo_get_target (cr), 0, 0,
        priv->allocation_width, priv->allocation_height);

    render_keyboard_surface (self, squeek_layout_get_current_view(priv->keyboard->layout));

    cairo_set_source_surface (cr, priv->keyboard_surface, 0.0, 0.0);
    source = cairo_get_source (cr);
    cairo_pattern_set_extend (source, CAIRO_EXTEND_PAD);
    cairo_paint (cr);

    cairo_restore (cr);
}

static void
eek_renderer_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    EekRendererPrivate *priv = eek_renderer_get_instance_private (
		    EEK_RENDERER(object));

    switch (prop_id) {
    case PROP_PCONTEXT:
        priv->pcontext = g_value_get_object (value);
        g_object_ref (priv->pcontext);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eek_renderer_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eek_renderer_dispose (GObject *object)
{
    EekRenderer        *self = EEK_RENDERER (object);
    EekRendererPrivate *priv = eek_renderer_get_instance_private (self);

    if (priv->keyboard) {
        priv->keyboard = NULL;
    }
    if (priv->pcontext) {
        g_object_unref (priv->pcontext);
        priv->pcontext = NULL;
    }

    g_clear_pointer (&priv->icons, g_hash_table_destroy);

    /* this will release all allocated surfaces and font if any */
    invalidate (EEK_RENDERER(object));

    G_OBJECT_CLASS (eek_renderer_parent_class)->dispose (object);
}

static void
eek_renderer_finalize (GObject *object)
{
    EekRenderer        *self = EEK_RENDERER(object);
    EekRendererPrivate *priv = eek_renderer_get_instance_private (self);

    g_hash_table_destroy (priv->outline_surface_cache);
    g_hash_table_destroy (priv->active_outline_surface_cache);
    pango_font_description_free (priv->ascii_font);
    pango_font_description_free (priv->font);
    G_OBJECT_CLASS (eek_renderer_parent_class)->finalize (object);
}

static void
eek_renderer_class_init (EekRendererClass *klass)
{
    GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec        *pspec;

    klass->render_button = eek_renderer_real_render_button;
    klass->render_keyboard = eek_renderer_real_render_keyboard;

    gobject_class->set_property = eek_renderer_set_property;
    gobject_class->get_property = eek_renderer_get_property;
    gobject_class->dispose = eek_renderer_dispose;
    gobject_class->finalize = eek_renderer_finalize;

    pspec = g_param_spec_object ("pango-context",
                                 "Pango Context",
                                 "Pango Context",
                                 PANGO_TYPE_CONTEXT,
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);
    g_object_class_install_property (gobject_class,
                                     PROP_PCONTEXT,
                                     pspec);
}


static GType new_type(char *name) {
    GTypeInfo info = {0};
    info.class_size = sizeof(GtkWidgetClass);
    info.instance_size = sizeof(GtkWidget);

    return g_type_register_static(GTK_TYPE_WIDGET, name, &info,
        G_TYPE_FLAG_ABSTRACT
    );
}

static GType layout_type() {
    static GType type = 0;
    if (!type) {
        type = new_type("layout");
    }
    return type;
}

static GType button_type() {
    static GType type = 0;
    if (!type) {
        type = new_type("button");
    }
    return type;
}

static void
eek_renderer_init (EekRenderer *self)
{
    EekRendererPrivate *priv = eek_renderer_get_instance_private (self);

    priv->keyboard = NULL;
    priv->pcontext = NULL;
    priv->default_foreground_color = DEFAULT_FOREGROUND_COLOR;
    priv->default_background_color = DEFAULT_BACKGROUND_COLOR;
    priv->border_width = 1.0;
    priv->allocation_width = 0.0;
    priv->allocation_height = 0.0;
    priv->scale = 1.0;
    priv->scale_factor = 1;
    priv->font = NULL;
    priv->outline_surface_cache =
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL,
                               (GDestroyNotify)cairo_surface_destroy);
    priv->active_outline_surface_cache =
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL,
                               (GDestroyNotify)cairo_surface_destroy);
    priv->keyboard_surface = NULL;

    GtkIconTheme *theme = gtk_icon_theme_get_default ();

    gtk_icon_theme_add_resource_path (theme, "/sm/puri/squeekboard/icons");
    priv->icons = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         (GDestroyNotify)cairo_surface_destroy);

    /* Create a default CSS provider and load a style sheet */
    priv->css_provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_resource (priv->css_provider,
        "/sm/puri/squeekboard/style.css");

    /* Create a style context for the layout */
    GtkWidgetPath *path = gtk_widget_path_new();
    gtk_widget_path_append_type(path, layout_type());

    priv->layout_context = gtk_style_context_new();
    gtk_style_context_set_path(priv->layout_context, path);
    gtk_widget_path_unref(path);
    gtk_style_context_add_provider (priv->layout_context,
        GTK_STYLE_PROVIDER(priv->css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);

    /* Create a style context for the buttons */
    path = gtk_widget_path_new();
    gtk_widget_path_append_type(path, layout_type());
    gtk_widget_path_append_type(path, button_type());
    priv->button_context = gtk_style_context_new ();
    gtk_style_context_set_path(priv->button_context, path);
    gtk_widget_path_unref(path);

    gtk_style_context_set_parent(priv->button_context, priv->layout_context);
    gtk_style_context_set_state (priv->button_context, GTK_STATE_FLAG_NORMAL);
    gtk_style_context_add_provider (priv->button_context,
        GTK_STYLE_PROVIDER(priv->css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
}

static void
invalidate (EekRenderer *renderer)
{
    EekRendererPrivate *priv = eek_renderer_get_instance_private (renderer);

    if (priv->outline_surface_cache)
        g_hash_table_remove_all (priv->outline_surface_cache);

    if (priv->active_outline_surface_cache)
        g_hash_table_remove_all (priv->active_outline_surface_cache);

    if (priv->keyboard_surface) {
        cairo_surface_destroy (priv->keyboard_surface);
        priv->keyboard_surface = NULL;
    }
}

EekRenderer *
eek_renderer_new (LevelKeyboard  *keyboard,
                  PangoContext *pcontext)
{
    EekRenderer *renderer = g_object_new (EEK_TYPE_RENDERER,
                         "pango-context", pcontext,
                         NULL);
    EekRendererPrivate *priv = eek_renderer_get_instance_private (renderer);
    priv->keyboard = keyboard;
    return renderer;
}

void
eek_renderer_set_allocation_size (EekRenderer *renderer,
                                  gdouble      width,
                                  gdouble      height)
{
    gdouble scale;

    g_return_if_fail (EEK_IS_RENDERER(renderer));
    g_return_if_fail (width > 0.0 && height > 0.0);

    EekRendererPrivate *priv = eek_renderer_get_instance_private (renderer);

    priv->allocation_width = width;
    priv->allocation_height = height;

    /* Calculate a scale factor to use when rendering the keyboard into the
       available space. */
    EekBounds bounds = squeek_view_get_bounds (level_keyboard_current(priv->keyboard));

    gdouble w = (bounds.x * 2) + bounds.width;
    gdouble h = (bounds.y * 2) + bounds.height;

    scale = MIN(width / w, height / h);

    priv->scale = scale;
    /* Set the rendering offset in widget coordinates to center the keyboard */
    priv->origin_x = (gint)floor((width - (scale * w)) / 2);
    priv->origin_y = (gint)floor((height - (scale * h)) / 2);
    invalidate (renderer);
}

void
eek_renderer_get_size (EekRenderer *renderer,
                       gdouble     *width,
                       gdouble     *height)
{
    g_return_if_fail (EEK_IS_RENDERER(renderer));

    EekRendererPrivate *priv = eek_renderer_get_instance_private (renderer);

    EekBounds bounds = squeek_view_get_bounds (level_keyboard_current(priv->keyboard));
    if (width)
        *width = bounds.width;
    if (height)
        *height = bounds.height;
}

void
eek_renderer_get_button_bounds (EekRenderer *renderer,
                                struct button_place *place,
                             EekBounds   *bounds,
                             gboolean     rotate)
{
    gint angle = 0;
    EekPoint points[4], min, max;

    g_return_if_fail (EEK_IS_RENDERER(renderer));
    g_return_if_fail (place);
    g_return_if_fail (bounds != NULL);

    EekRendererPrivate *priv = eek_renderer_get_instance_private (renderer);

    EekBounds button_bounds = squeek_button_get_bounds(place->button);
    EekBounds row_bounds = squeek_row_get_bounds (place->row);
    EekBounds view_bounds = squeek_view_get_bounds (level_keyboard_current(priv->keyboard));

    if (!rotate) {
        button_bounds.x += view_bounds.x + row_bounds.x;
        button_bounds.y += view_bounds.y + row_bounds.y;
        *bounds = button_bounds;
        return;
    }
    points[0].x = button_bounds.x;
    points[0].y = button_bounds.y;
    points[1].x = points[0].x + button_bounds.width;
    points[1].y = points[0].y;
    points[2].x = points[1].x;
    points[2].y = points[1].y + button_bounds.height;
    points[3].x = points[0].x;
    points[3].y = points[2].y;

    if (rotate) {
        angle = squeek_row_get_angle (place->row);
    }

    min = points[2];
    max = points[0];
    for (unsigned i = 0; i < G_N_ELEMENTS(points); i++) {
        eek_point_rotate (&points[i], angle);
        if (points[i].x < min.x)
            min.x = points[i].x;
        if (points[i].x > max.x)
            max.x = points[i].x;
        if (points[i].y < min.y)
            min.y = points[i].y;
        if (points[i].y > max.y)
            max.y = points[i].y;
    }
    bounds->x = view_bounds.x + row_bounds.x + min.x;
    bounds->y = view_bounds.y + row_bounds.y + min.y;
    bounds->width = (max.x - min.x);
    bounds->height = (max.y - min.y);
}

gdouble
eek_renderer_get_scale (EekRenderer *renderer)
{
    g_return_val_if_fail (EEK_IS_RENDERER(renderer), 0);

    EekRendererPrivate *priv = eek_renderer_get_instance_private (renderer);

    return priv->scale;
}

void
eek_renderer_set_scale_factor (EekRenderer *renderer, gint scale)
{
    g_return_if_fail (EEK_IS_RENDERER(renderer));

    EekRendererPrivate *priv = eek_renderer_get_instance_private (renderer);
    priv->scale_factor = scale;
}

PangoLayout *
eek_renderer_create_pango_layout (EekRenderer  *renderer)
{
    g_return_val_if_fail (EEK_IS_RENDERER(renderer), NULL);

    EekRendererPrivate *priv = eek_renderer_get_instance_private (renderer);

    return pango_layout_new (priv->pcontext);
}

cairo_surface_t *
eek_renderer_get_icon_surface (EekRenderer *renderer,
                               const gchar *icon_name,
                               gint size,
                               gint scale)
{
    GError *error = NULL;
    cairo_surface_t *surface;

    g_return_val_if_fail (EEK_IS_RENDERER(renderer), NULL);

    EekRendererPrivate *priv = eek_renderer_get_instance_private (renderer);

    surface = g_hash_table_lookup (priv->icons, icon_name);
    if (!surface) {
        surface = gtk_icon_theme_load_surface (gtk_icon_theme_get_default (),
                                               icon_name,
                                               size,
                                               scale,
                                               NULL,
                                               0,
                                               &error);
        g_hash_table_insert (priv->icons, g_strdup(icon_name), surface);
        if (surface == NULL) {
            g_warning ("can't get icon surface for %s: %s",
                       icon_name,
                       error->message);
            g_error_free (error);
            return NULL;
        }
    }
    return surface;
}

void
eek_renderer_render_button (EekRenderer *renderer,
                         cairo_t     *cr,
                         struct button_place *place,
                         gdouble      scale,
                         gboolean     rotate)
{
    g_return_if_fail (EEK_IS_RENDERER(renderer));
    g_return_if_fail (place);
    g_return_if_fail (scale >= 0.0);

    EEK_RENDERER_GET_CLASS(renderer)->
        render_button (renderer, cr, place, scale, rotate);
}

void
eek_renderer_render_keyboard (EekRenderer *renderer,
                              cairo_t     *cr)
{
    g_return_if_fail (EEK_IS_RENDERER(renderer));
    EEK_RENDERER_GET_CLASS(renderer)->render_keyboard (renderer, cr);
}

void
eek_renderer_set_default_foreground_color (EekRenderer    *renderer,
                                           const EekColor *color)
{
    g_return_if_fail (EEK_IS_RENDERER(renderer));
    g_return_if_fail (color);

    EekRendererPrivate *priv = eek_renderer_get_instance_private (renderer);

    memcpy (&priv->default_foreground_color, color, sizeof(EekColor));
}

void
eek_renderer_set_default_background_color (EekRenderer    *renderer,
                                           const EekColor *color)
{
    g_return_if_fail (EEK_IS_RENDERER(renderer));
    g_return_if_fail (color);

    EekRendererPrivate *priv = eek_renderer_get_instance_private (renderer);

    memcpy (&priv->default_background_color, color, sizeof(EekColor));
}

void
eek_renderer_get_foreground_color (EekRenderer *renderer,
                                   GtkStyleContext *context,
                                   EekColor    *color)
{
    g_return_if_fail (EEK_IS_RENDERER(renderer));
    g_return_if_fail (color);

    GtkStateFlags flags = GTK_STATE_FLAG_NORMAL;
    GdkRGBA gcolor;

    gtk_style_context_get_color (context, flags, &gcolor);
    color->red = gcolor.red;
    color->green = gcolor.green;
    color->blue = gcolor.blue;
    color->alpha = gcolor.alpha;
}

static gboolean
sign (EekPoint *p1, EekPoint *p2, EekPoint *p3)
{
    // FIXME: what is this actually checking?
    return (p1->x - p3->x) * (p2->y - p3->y) -
        (p2->x - p3->x) * (p1->y - p3->y);
}

uint32_t
eek_are_bounds_inside (EekBounds bounds, EekPoint point, EekPoint origin, int32_t angle)
{
    EekPoint points[4];
    gboolean b1, b2, b3;

    points[0].x = bounds.x;
    points[0].y = bounds.y;
    points[1].x = points[0].x + bounds.width;
    points[1].y = points[0].y;
    points[2].x = points[1].x;
    points[2].y = points[1].y + bounds.height;
    points[3].x = points[0].x;
    points[3].y = points[2].y;

    for (unsigned i = 0; i < G_N_ELEMENTS(points); i++) {
        eek_point_rotate (&points[i], angle);
        points[i].x += origin.x;
        points[i].y += origin.y;
    }

    b1 = sign (&point, &points[0], &points[1]) < 0.0;
    b2 = sign (&point, &points[1], &points[2]) < 0.0;
    b3 = sign (&point, &points[2], &points[0]) < 0.0;

    if (b1 == b2 && b2 == b3) {
        return 1;
    }

    b1 = sign (&point, &points[2], &points[3]) < 0.0;
    b2 = sign (&point, &points[3], &points[0]) < 0.0;
    b3 = sign (&point, &points[0], &points[2]) < 0.0;

    if (b1 == b2 && b2 == b3) {
        return 1;
    }
    return 0;
}

/**
 * eek_renderer_find_key_by_position:
 * @renderer: The renderer normally used to render the key
 * @x: The horizontal widget coordinate of the position to test for a key
 * @y: The vertical widget coordinate of the position to test for a key
 *
 * Return value: the key located at the position x, y in widget coordinates, or
 *   NULL if no key can be found at that location
 **/
struct squeek_button *
eek_renderer_find_button_by_position (EekRenderer *renderer,
                                      struct squeek_view *view,
                                      gdouble      x,
                                      gdouble      y)
{
    g_return_val_if_fail (EEK_IS_RENDERER(renderer), NULL);

    EekRendererPrivate *priv = eek_renderer_get_instance_private (renderer);

    /* Transform from widget coordinates to keyboard coordinates */
    EekPoint point = {
        .x = (x - priv->origin_x)/priv->scale,
        .y = (y - priv->origin_y)/priv->scale,
    };
    return squeek_view_find_button_by_position(view, point);
}
