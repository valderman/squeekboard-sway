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

/**
 * SECTION:eek-gtk-keyboard
 * @short_description: a #GtkWidget displaying #EekKeyboard
 */

#include "config.h"

#ifdef HAVE_LIBCANBERRA
#include <canberra-gtk.h>
#endif

#include <math.h>
#include <string.h>

#include "eek-renderer.h"
#include "eek-keyboard.h"

#include "eek-gtk-keyboard.h"

#include "eekboard/eekboard-context-service.h"
#include "src/layout.h"

enum {
    PROP_0,
    PROP_LAST
};

/* since 2.91.5 GDK_DRAWABLE was removed and gdk_cairo_create takes
   GdkWindow as the argument */
#ifndef GDK_DRAWABLE
#define GDK_DRAWABLE(x) (x)
#endif

typedef struct _EekGtkKeyboardPrivate
{
    EekRenderer *renderer;
    LevelKeyboard *keyboard;

    GdkEventSequence *sequence; // unowned reference
} EekGtkKeyboardPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EekGtkKeyboard, eek_gtk_keyboard, GTK_TYPE_DRAWING_AREA)

static void       render_pressed_button      (GtkWidget *widget, struct button_place *place);
static void       render_released_button     (GtkWidget *widget,
                                              const struct squeek_button *button);

static void
eek_gtk_keyboard_real_realize (GtkWidget      *self)
{
    gtk_widget_set_events (self,
                           GDK_EXPOSURE_MASK |
                           GDK_KEY_PRESS_MASK |
                           GDK_KEY_RELEASE_MASK |
                           GDK_BUTTON_PRESS_MASK |
                           GDK_BUTTON_RELEASE_MASK |
                           GDK_BUTTON_MOTION_MASK |
                           GDK_TOUCH_MASK);

    GTK_WIDGET_CLASS (eek_gtk_keyboard_parent_class)->realize (self);
}

static gboolean
eek_gtk_keyboard_real_draw (GtkWidget *self,
                            cairo_t   *cr)
{
    EekGtkKeyboardPrivate *priv =
	    eek_gtk_keyboard_get_instance_private (EEK_GTK_KEYBOARD (self));
    GtkAllocation allocation;
    gtk_widget_get_allocation (self, &allocation);

    if (!priv->renderer) {
        PangoContext *pcontext = gtk_widget_get_pango_context (self);

        priv->renderer = eek_renderer_new (priv->keyboard, pcontext);

        eek_renderer_set_allocation_size (priv->renderer,
                                          allocation.width,
                                          allocation.height);
        eek_renderer_set_scale_factor (priv->renderer,
                                       gtk_widget_get_scale_factor (self));
    }

    // render the keyboard without any key activity (TODO: from a cached buffer)
    eek_renderer_render_keyboard (priv->renderer, cr);
    // render only a few remaining changes
    squeek_layout_draw_all_changed(priv->keyboard->layout, EEK_GTK_KEYBOARD(self));
    return FALSE;
}

static void
eek_gtk_keyboard_real_size_allocate (GtkWidget     *self,
                                     GtkAllocation *allocation)
{
    EekGtkKeyboardPrivate *priv =
	    eek_gtk_keyboard_get_instance_private (EEK_GTK_KEYBOARD (self));

    if (priv->renderer)
        eek_renderer_set_allocation_size (priv->renderer,
                                          allocation->width,
                                          allocation->height);

    GTK_WIDGET_CLASS (eek_gtk_keyboard_parent_class)->
        size_allocate (self, allocation);
}

static void depress(EekGtkKeyboard *self,
                    gdouble x, gdouble y, guint32 time)
{
    EekGtkKeyboardPrivate *priv = eek_gtk_keyboard_get_instance_private (self);

    squeek_layout_depress(priv->keyboard->layout, priv->keyboard->manager->virtual_keyboard,
                          x, y, eek_renderer_get_transformation(priv->renderer), time, self);
}

static void drag(EekGtkKeyboard *self,
                 gdouble x, gdouble y, guint32 time)
{
    EekGtkKeyboardPrivate *priv = eek_gtk_keyboard_get_instance_private (self);
    squeek_layout_drag(priv->keyboard->layout, priv->keyboard->manager->virtual_keyboard,
                       x, y, eek_renderer_get_transformation(priv->renderer), time, self);
}

static void release(EekGtkKeyboard *self, guint32 time)
{
    EekGtkKeyboardPrivate *priv = eek_gtk_keyboard_get_instance_private (self);

    squeek_layout_release(priv->keyboard->layout, priv->keyboard->manager->virtual_keyboard, time, self);
}

static gboolean
eek_gtk_keyboard_real_button_press_event (GtkWidget      *self,
                                          GdkEventButton *event)
{
    if (event->type == GDK_BUTTON_PRESS && event->button == 1) {
        depress(EEK_GTK_KEYBOARD(self), event->x, event->y, event->time);
    }
    return TRUE;
}

// TODO: this belongs more in gtk_keyboard, with a way to find out which key to re-render
static gboolean
eek_gtk_keyboard_real_button_release_event (GtkWidget      *self,
                                            GdkEventButton *event)
{
    if (event->type == GDK_BUTTON_RELEASE && event->button == 1) {
        // TODO: can the event have different coords than the previous move event?
        release(EEK_GTK_KEYBOARD(self), event->time);
    }
    return TRUE;
}

static gboolean
eek_gtk_keyboard_real_motion_notify_event (GtkWidget      *self,
                                           GdkEventMotion *event)
{
    if (event->state & GDK_BUTTON1_MASK) {
        drag(EEK_GTK_KEYBOARD(self), event->x, event->y, event->time);
    }
    return TRUE;
}

// Only one touch stream at a time allowed. Others will be completely ignored.
static gboolean
handle_touch_event (GtkWidget     *widget,
                    GdkEventTouch *event)
{
    EekGtkKeyboard        *self = EEK_GTK_KEYBOARD (widget);
    EekGtkKeyboardPrivate *priv = eek_gtk_keyboard_get_instance_private (self);

    /* For each new touch, release the previous one and record the new event
       sequence. */
    if (event->type == GDK_TOUCH_BEGIN) {
        release(self, event->time);
        priv->sequence = event->sequence;
        depress(self, event->x, event->y, event->time);
        return TRUE;
    }

    /* Only allow the latest touch point to be dragged. */
    if (event->type == GDK_TOUCH_UPDATE && event->sequence == priv->sequence) {
        drag(self, event->x, event->y, event->time);
    }
    else if (event->type == GDK_TOUCH_END || event->type == GDK_TOUCH_CANCEL) {
        // TODO: can the event have different coords than the previous update event?
        /* Only respond to the release of the latest touch point. Previous
           touches have already been released. */
        if (event->sequence == priv->sequence) {
            release(self, event->time);
            priv->sequence = NULL;
        }
    }
    return TRUE;
}

static void
eek_gtk_keyboard_real_unmap (GtkWidget *self)
{
    EekGtkKeyboardPrivate *priv =
	    eek_gtk_keyboard_get_instance_private (EEK_GTK_KEYBOARD (self));

    if (priv->keyboard) {
        squeek_layout_release_all_only(
            priv->keyboard->layout, priv->keyboard->manager->virtual_keyboard,
            gdk_event_get_time(NULL));
    }

    GTK_WIDGET_CLASS (eek_gtk_keyboard_parent_class)->unmap (self);
}

static void
eek_gtk_keyboard_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eek_gtk_keyboard_dispose (GObject *object)
{
    EekGtkKeyboard        *self = EEK_GTK_KEYBOARD (object);
    EekGtkKeyboardPrivate *priv = eek_gtk_keyboard_get_instance_private (self);

    if (priv->renderer) {
        g_object_unref (priv->renderer);
        priv->renderer = NULL;
        priv->renderer = NULL;
    }

    if (priv->keyboard) {
        squeek_layout_release_all_only(
            priv->keyboard->layout, priv->keyboard->manager->virtual_keyboard,
            gdk_event_get_time(NULL));
        priv->keyboard = NULL;
    }

    G_OBJECT_CLASS (eek_gtk_keyboard_parent_class)->dispose (object);
}

static void
eek_gtk_keyboard_class_init (EekGtkKeyboardClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    widget_class->realize = eek_gtk_keyboard_real_realize;
    widget_class->unmap = eek_gtk_keyboard_real_unmap;
    widget_class->draw = eek_gtk_keyboard_real_draw;
    widget_class->size_allocate = eek_gtk_keyboard_real_size_allocate;
    widget_class->button_press_event =
        eek_gtk_keyboard_real_button_press_event;
    widget_class->button_release_event =
        eek_gtk_keyboard_real_button_release_event;
    widget_class->motion_notify_event =
        eek_gtk_keyboard_real_motion_notify_event;
    widget_class->touch_event = handle_touch_event;

    gobject_class->set_property = eek_gtk_keyboard_set_property;
    gobject_class->dispose = eek_gtk_keyboard_dispose;
}

static void
eek_gtk_keyboard_init (EekGtkKeyboard *self)
{}

/**
 * eek_gtk_keyboard_new:
 * @keyboard: an #EekKeyboard
 *
 * Create a new #GtkWidget displaying @keyboard.
 * Returns: a #GtkWidget
 */
GtkWidget *
eek_gtk_keyboard_new (LevelKeyboard *keyboard)
{
    EekGtkKeyboard *ret = EEK_GTK_KEYBOARD(g_object_new (EEK_TYPE_GTK_KEYBOARD, NULL));
    EekGtkKeyboardPrivate *priv = (EekGtkKeyboardPrivate*)eek_gtk_keyboard_get_instance_private (ret);
    priv->keyboard = keyboard;
    return GTK_WIDGET(ret);
}

static void
render_pressed_button (GtkWidget *widget,
                       struct button_place *place)
{
    EekGtkKeyboard        *self = EEK_GTK_KEYBOARD (widget);
    EekGtkKeyboardPrivate *priv = eek_gtk_keyboard_get_instance_private (self);

    GdkWindow         *window  = gtk_widget_get_window (widget);
    cairo_region_t    *region  = gdk_window_get_clip_region (window);
    GdkDrawingContext *context = gdk_window_begin_draw_frame (window, region);
    cairo_t           *cr      = gdk_drawing_context_get_cairo_context (context);

    eek_renderer_render_button (priv->renderer, cr, place, 1.0, TRUE);
/*
    eek_renderer_render_key (priv->renderer, cr, key, 1.5, TRUE);
*/
    gdk_window_end_draw_frame (window, context);

    cairo_region_destroy (region);
}

void
eek_gtk_render_locked_button (EekGtkKeyboard *self, struct button_place place)
{
    EekGtkKeyboardPrivate *priv = eek_gtk_keyboard_get_instance_private (self);

    GdkWindow         *window  = gtk_widget_get_window (GTK_WIDGET(self));
    cairo_region_t    *region  = gdk_window_get_clip_region (window);
    GdkDrawingContext *context = gdk_window_begin_draw_frame (window, region);
    cairo_t           *cr      = gdk_drawing_context_get_cairo_context (context);

    eek_renderer_render_button (priv->renderer, cr, &place, 1.0, TRUE);

    gdk_window_end_draw_frame (window, context);

    cairo_region_destroy (region);
}

// TODO: does it really redraw the entire keyboard?
static void
render_released_button (GtkWidget *widget,
                        const struct squeek_button *button)
{
    (void)button;
    EekGtkKeyboard        *self = EEK_GTK_KEYBOARD (widget);
    EekGtkKeyboardPrivate *priv = eek_gtk_keyboard_get_instance_private (self);

    GdkWindow         *window  = gtk_widget_get_window (widget);
    cairo_region_t    *region  = gdk_window_get_clip_region (window);
    GdkDrawingContext *context = gdk_window_begin_draw_frame (window, region);
    cairo_t           *cr      = gdk_drawing_context_get_cairo_context (context);

    eek_renderer_render_keyboard (priv->renderer, cr);

    gdk_window_end_draw_frame (window, context);

    cairo_region_destroy (region);
}

void
eek_gtk_on_button_pressed (struct button_place place,
                   EekGtkKeyboard *self)
{
    EekGtkKeyboardPrivate *priv = eek_gtk_keyboard_get_instance_private (self);

    /* renderer may have not been set yet if the widget is a popup */
    if (!priv->renderer)
        return;

    if (!place.row) {
        return;
    }
    render_pressed_button (GTK_WIDGET(self), &place);
    gtk_widget_queue_draw (GTK_WIDGET(self));

#if HAVE_LIBCANBERRA
    ca_gtk_play_for_widget (widget, 0,
                            CA_PROP_EVENT_ID, "button-pressed",
                            CA_PROP_EVENT_DESCRIPTION, "virtual key pressed",
                            CA_PROP_APPLICATION_ID, "org.fedorahosted.Eekboard",
                            NULL);
#endif
}

void
eek_gtk_on_button_released (const struct squeek_button *button,
                    struct squeek_view *view,
                    EekGtkKeyboard *self)
{
    (void)view;
    EekGtkKeyboardPrivate *priv = eek_gtk_keyboard_get_instance_private (self);

    /* renderer may have not been set yet if the widget is a popup */
    if (!priv->renderer)
        return;

    render_released_button (GTK_WIDGET(self), button);
    gtk_widget_queue_draw (GTK_WIDGET(self));

#if HAVE_LIBCANBERRA
    ca_gtk_play_for_widget (widget, 0,
                            CA_PROP_EVENT_ID, "button-released",
                            CA_PROP_EVENT_DESCRIPTION, "virtual key pressed",
                            CA_PROP_APPLICATION_ID, "org.fedorahosted.Eekboard",
                            NULL);
#endif
}
