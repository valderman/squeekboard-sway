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

#include "eek-keyboard.h"
#include "eek-renderer.h"
#include "src/style.h"

enum {
    PROP_0,
    PROP_PCONTEXT,
    PROP_LAST
};

typedef struct _EekRendererPrivate
{
    LevelKeyboard *keyboard; // unowned
    PangoContext *pcontext; // owned
    GtkCssProvider *css_provider; // owned
    GtkStyleContext *view_context; // owned
    GtkStyleContext *button_context; // TODO: maybe move a copy to each button

    gdouble border_width; // FIXME: border of what?

    gdouble allocation_width;
    gdouble allocation_height;
    gint scale_factor; /* the outputs scale factor */
    struct transformation widget_to_layout;

    PangoFontDescription *font; // owned reference

} EekRendererPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EekRenderer, eek_renderer, G_TYPE_OBJECT)

/* eek-keyboard-drawing.c */
static void eek_renderer_render_button_label (EekRenderer *self, cairo_t *cr, GtkStyleContext *ctx,
                                                const struct squeek_button *button);

void eek_render_button                         (EekRenderer *self,
                                                cairo_t     *cr, const struct squeek_button *button,
                                                gboolean     pressed, gboolean locked);

static void
render_outline (cairo_t     *cr,
                GtkStyleContext *ctx,
                EekBounds bounds)
{
    GtkBorder margin, border;
    gtk_style_context_get_margin(ctx, GTK_STATE_FLAG_NORMAL, &margin);
    gtk_style_context_get_border(ctx, GTK_STATE_FLAG_NORMAL, &border);

    gdouble x = margin.left + border.left;
    gdouble y = margin.top + border.top;
    EekBounds position = {
        .x = x,
        .y = y,
        .width = bounds.width - x - (margin.right + border.right),
        .height = bounds.height - y - (margin.bottom + border.bottom),
    };
    gtk_render_background (ctx, cr,
        position.x, position.y, position.width, position.height);
    gtk_render_frame (ctx, cr,
        position.x, position.y, position.width, position.height);
}

static void render_button_in_context(EekRenderer *self,
                                     gint scale_factor,
                                     cairo_t     *cr,
                                     GtkStyleContext *ctx,
                                     const struct squeek_button *button) {
    /* blank background */
    cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint (cr);

    EekBounds bounds = squeek_button_get_bounds(button);
    render_outline (cr, ctx, bounds);
    cairo_paint (cr);

    /* render icon (if any) */
    const char *icon_name = squeek_button_get_icon_name(button);

    if (icon_name) {
        cairo_surface_t *icon_surface =
            eek_renderer_get_icon_surface (icon_name, 16, scale_factor);
        if (icon_surface) {
            gint width = cairo_image_surface_get_width (icon_surface);
            gint height = cairo_image_surface_get_height (icon_surface);

            cairo_save (cr);
            cairo_translate (cr,
                             (bounds.width - (double)width / scale_factor) / 2,
                             (bounds.height - (double)height / scale_factor) / 2);
            cairo_rectangle (cr, 0, 0, width, height);
            cairo_clip (cr);
            /* Draw the shape of the icon using the foreground color */
            GdkRGBA color = {0};
            gtk_style_context_get_color (ctx, GTK_STATE_FLAG_NORMAL, &color);

            cairo_set_source_rgba (cr, color.red,
                                       color.green,
                                       color.blue,
                                       color.alpha);
            cairo_mask_surface (cr, icon_surface, 0.0, 0.0);
            cairo_surface_destroy(icon_surface);
            cairo_fill (cr);
            cairo_restore (cr);
            return;
        }
    }
    eek_renderer_render_button_label (self, cr, ctx, button);
}

void
eek_render_button (EekRenderer *self,
            cairo_t     *cr,
            const struct squeek_button *button,
               gboolean     pressed,
               gboolean     locked)
{
    EekRendererPrivate *priv = eek_renderer_get_instance_private (self);

    GtkStyleContext *ctx = priv->button_context;
    /* Set the name of the button on the widget path, using the name obtained
       from the button's symbol. */
    g_autoptr (GtkWidgetPath) path = NULL;
    path = gtk_widget_path_copy (gtk_style_context_get_path (ctx));
    const char *name = squeek_button_get_name(button);
    gtk_widget_path_iter_set_name (path, -1, name);

    /* Update the style context with the updated widget path. */
    gtk_style_context_set_path (ctx, path);
    /* Set the state to take into account whether the button is active
       (pressed) or normal. */
    gtk_style_context_set_state(ctx,
        pressed ? GTK_STATE_FLAG_ACTIVE : GTK_STATE_FLAG_NORMAL);
    const char *outline_name = squeek_button_get_outline_name(button);
    if (locked) {
        gtk_style_context_add_class(ctx, "locked");
    }
    gtk_style_context_add_class(ctx, outline_name);

    render_button_in_context(self, priv->scale_factor, cr, ctx, button);

    // Save and restore functions don't work if gtk_render_* was used in between
    gtk_style_context_set_state(ctx, GTK_STATE_FLAG_NORMAL);
    gtk_style_context_remove_class(ctx, outline_name);
    if (locked) {
        gtk_style_context_remove_class(ctx, "locked");
    }
}

static void
eek_renderer_render_button_label (EekRenderer *self,
                                  cairo_t     *cr,
                                  GtkStyleContext *ctx,
                                  const struct squeek_button *button)
{
    EekRendererPrivate *priv = eek_renderer_get_instance_private (self);

    const gchar *label = squeek_button_get_label(button);

    if (!label) {
        return;
    }

    PangoFontDescription *font;
    gdouble scale;


    if (!priv->font) {
        const PangoFontDescription *base_font;
        gdouble size;

        base_font = pango_context_get_font_description (priv->pcontext);
        // FIXME: Base font size on the same size unit used for button sizing,
        // and make the default about 1/3 of the current row height
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

    PangoLayout *layout = pango_cairo_create_layout (cr);
    pango_layout_set_font_description (layout, font);
    pango_font_description_free (font);

    pango_layout_set_text (layout, label, -1);
    PangoLayoutLine *line = pango_layout_get_line_readonly(layout, 0);
    if (line->resolved_dir == PANGO_DIRECTION_RTL) {
        pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);
    }
    pango_layout_set_width (layout,
                            PANGO_SCALE * bounds.width * scale);

    PangoRectangle extents = { 0, };
    pango_layout_get_extents (layout, NULL, &extents);

    cairo_save (cr);
    cairo_move_to
        (cr,
         (bounds.width - (double)extents.width / PANGO_SCALE) / 2,
         (bounds.height - (double)extents.height / PANGO_SCALE) / 2);

    GdkRGBA color = {0};
    gtk_style_context_get_color (ctx, GTK_STATE_FLAG_NORMAL, &color);

    cairo_set_source_rgba (cr,
                           color.red,
                           color.green,
                           color.blue,
                           color.alpha);
    pango_cairo_show_layout (cr, layout);
    cairo_restore (cr);
    g_object_unref (layout);
}

void
eek_renderer_render_keyboard (EekRenderer *self,
                                   cairo_t     *cr)
{
    EekRendererPrivate *priv = eek_renderer_get_instance_private (self);

    g_return_if_fail (priv->keyboard);
    g_return_if_fail (priv->allocation_width > 0.0);
    g_return_if_fail (priv->allocation_height > 0.0);

    /* Paint the background covering the entire widget area */
    gtk_render_background (priv->view_context,
                           cr,
                           0, 0,
                           priv->allocation_width, priv->allocation_height);

    cairo_save(cr);
    cairo_translate (cr, priv->widget_to_layout.origin_x, priv->widget_to_layout.origin_y);
    cairo_scale (cr, priv->widget_to_layout.scale, priv->widget_to_layout.scale);

    squeek_draw_layout_base_view(priv->keyboard->layout, self, cr);
    squeek_layout_draw_all_changed(priv->keyboard->layout, self, cr);
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
    (void)value;
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

    // this is where renderer-specific surfaces would be released

    G_OBJECT_CLASS (eek_renderer_parent_class)->dispose (object);
}

static void
eek_renderer_finalize (GObject *object)
{
    EekRenderer        *self = EEK_RENDERER(object);
    EekRendererPrivate *priv = eek_renderer_get_instance_private (self);

    g_object_unref(priv->css_provider);
    g_object_unref(priv->view_context);
    g_object_unref(priv->button_context);
    pango_font_description_free (priv->font);
    G_OBJECT_CLASS (eek_renderer_parent_class)->finalize (object);
}

static void
eek_renderer_class_init (EekRendererClass *klass)
{
    GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec        *pspec;

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

static GType view_type() {
    static GType type = 0;
    if (!type) {
        type = new_type("sq_view");
    }
    return type;
}

static GType button_type() {
    static GType type = 0;
    if (!type) {
        type = new_type("sq_button");
    }
    return type;
}

static void
eek_renderer_init (EekRenderer *self)
{
    EekRendererPrivate *priv = eek_renderer_get_instance_private (self);

    priv->keyboard = NULL;
    priv->pcontext = NULL;
    priv->border_width = 1.0;
    priv->allocation_width = 0.0;
    priv->allocation_height = 0.0;
    priv->scale_factor = 1;
    priv->font = NULL;

    GtkIconTheme *theme = gtk_icon_theme_get_default ();

    gtk_icon_theme_add_resource_path (theme, "/sm/puri/squeekboard/icons");

    priv->css_provider = squeek_load_style();
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

    /* Create a style context for the layout */
    GtkWidgetPath *path = gtk_widget_path_new();
    gtk_widget_path_append_type(path, view_type());

    priv->view_context = gtk_style_context_new();
    gtk_style_context_set_path(priv->view_context, path);
    gtk_widget_path_unref(path);
    if (squeek_layout_get_kind(priv->keyboard->layout) == ARRANGEMENT_KIND_WIDE) {
        gtk_style_context_add_class(priv->view_context, "wide");
    }
    gtk_style_context_add_provider (priv->view_context,
        GTK_STYLE_PROVIDER(priv->css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);

    /* Create a style context for the buttons */
    path = gtk_widget_path_new();
    gtk_widget_path_append_type(path, view_type());
    if (squeek_layout_get_kind(priv->keyboard->layout) == ARRANGEMENT_KIND_WIDE) {
        gtk_widget_path_iter_add_class(path, -1, "wide");
    }
    gtk_widget_path_append_type(path, button_type());
    priv->button_context = gtk_style_context_new ();
    gtk_style_context_set_path(priv->button_context, path);
    gtk_widget_path_unref(path);
    gtk_style_context_set_parent(priv->button_context, priv->view_context);
    gtk_style_context_set_state (priv->button_context, GTK_STATE_FLAG_NORMAL);
    gtk_style_context_add_provider (priv->button_context,
        GTK_STYLE_PROVIDER(priv->css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
    return renderer;
}

void
eek_renderer_set_allocation_size (EekRenderer *renderer,
                                  gdouble      width,
                                  gdouble      height)
{
    g_return_if_fail (EEK_IS_RENDERER(renderer));
    g_return_if_fail (width > 0.0 && height > 0.0);

    EekRendererPrivate *priv = eek_renderer_get_instance_private (renderer);

    priv->allocation_width = width;
    priv->allocation_height = height;

    priv->widget_to_layout = squeek_layout_calculate_transformation(
                priv->keyboard->layout,
                priv->allocation_width, priv->allocation_height);

    // This is where size-dependent surfaces would be released
}

void
eek_renderer_set_scale_factor (EekRenderer *renderer, gint scale)
{
    g_return_if_fail (EEK_IS_RENDERER(renderer));

    EekRendererPrivate *priv = eek_renderer_get_instance_private (renderer);
    priv->scale_factor = scale;
}

cairo_surface_t *
eek_renderer_get_icon_surface (const gchar *icon_name,
                               gint size,
                               gint scale)
{
    GError *error = NULL;
    cairo_surface_t *surface = gtk_icon_theme_load_surface (gtk_icon_theme_get_default (),
                                                            icon_name,
                                                            size,
                                                            scale,
                                                            NULL,
                                                            0,
                                                            &error);

    if (surface == NULL) {
        g_warning ("can't get icon surface for %s: %s",
                   icon_name,
                   error->message);
        g_error_free (error);
        return NULL;
    }
    return surface;
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

struct transformation
eek_renderer_get_transformation (EekRenderer *renderer) {
    struct transformation failed = {0};
    g_return_val_if_fail (EEK_IS_RENDERER(renderer), failed);

    EekRendererPrivate *priv = eek_renderer_get_instance_private (renderer);
    return priv->widget_to_layout;
}
