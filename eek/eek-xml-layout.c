/*
 * Copyright (C) 2011 Daiki Ueno <ueno@unixuser.org>
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:eek-xml-layout
 * @short_description: Layout engine which loads layout information from XML
 */

#include "config.h"

#include <gio/gio.h> /* GResource */
#include <stdlib.h>
#include <string.h>

#include "eek-keyboard.h"
#include "src/keyboard.h"

#include "squeekboard-resources.h"

#include "eek-xml-layout.h"

enum {
    PROP_0,
    PROP_ID,
    PROP_LAST
};

static void initable_iface_init (GInitableIface *initable_iface);

typedef struct _EekXmlLayoutPrivate
{
    gchar *id;
    gchar *keyboards_dir;
    EekXmlKeyboardDesc *desc;
} EekXmlLayoutPrivate;

G_DEFINE_TYPE_EXTENDED (EekXmlLayout, eek_xml_layout, EEK_TYPE_LAYOUT,
            0, /* GTypeFlags */
            G_ADD_PRIVATE(EekXmlLayout)
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                               initable_iface_init))

G_DEFINE_BOXED_TYPE(EekXmlKeyboardDesc, eek_xml_keyboard_desc, eek_xml_keyboard_desc_copy, eek_xml_keyboard_desc_free);

#define BUFSIZE	8192

static GList        *parse_keyboards (const gchar         *path,
                                      GError             **error);

static gboolean      validate        (const gchar        **valid_path_list,
                                      gsize                valid_path_list_len,
                                      const gchar         *element_name,
                                      GSList              *element_stack,
                                      GError             **error);

static gboolean      parse           (GMarkupParseContext *pcontext,
                                      GInputStream        *input,
                                      GError             **error);
static const gchar * get_attribute   (const gchar        **names,
                                      const gchar        **values,
                                      const gchar         *name);

static void
keyboard_desc_free (EekXmlKeyboardDesc *desc)
{
    g_free (desc->id);
    g_free (desc->name);
    g_free (desc->geometry);
    g_free (desc->symbols);
    g_free (desc->longname);
    g_free (desc->language);
    g_slice_free (EekXmlKeyboardDesc, desc);
}

struct _KeyboardsParseData {
    GSList *element_stack;

    GList *keyboards;
};
typedef struct _KeyboardsParseData KeyboardsParseData;

static KeyboardsParseData *
keyboards_parse_data_new (void)
{
    return g_slice_new0 (KeyboardsParseData);
}

static void
keyboards_parse_data_free (KeyboardsParseData *data)
{
    g_list_free_full (data->keyboards, (GDestroyNotify) keyboard_desc_free);
    g_slice_free (KeyboardsParseData, data);
}

static const gchar *keyboards_valid_path_list[] = {
    "keyboards",
    "keyboard/keyboards",
};

static void
keyboards_start_element_callback (GMarkupParseContext *pcontext,
                                  const gchar         *element_name,
                                  const gchar        **attribute_names,
                                  const gchar        **attribute_values,
                                  gpointer             user_data,
                                  GError             **error)
{
    KeyboardsParseData *data = user_data;

    if (!validate (keyboards_valid_path_list,
                   G_N_ELEMENTS (keyboards_valid_path_list),
                   element_name,
                   data->element_stack,
                   error))
        return;

    if (g_strcmp0 (element_name, "keyboard") == 0) {
        EekXmlKeyboardDesc *desc = g_slice_new0 (EekXmlKeyboardDesc);
        const gchar *attribute;

        data->keyboards = g_list_prepend (data->keyboards, desc);

        attribute = get_attribute (attribute_names, attribute_values,
                                   "id");
        if (attribute == NULL) {
            g_set_error (error,
                         G_MARKUP_ERROR,
                         G_MARKUP_ERROR_MISSING_ATTRIBUTE,
                         "no \"id\" attribute for \"keyboard\"");
            return;
        }
        desc->id = g_strdup (attribute);

        attribute = get_attribute (attribute_names, attribute_values,
                                   "name");
        if (attribute)
            desc->name = g_strdup (attribute);

        attribute = get_attribute (attribute_names, attribute_values,
                                   "geometry");
        if (attribute == NULL) {
            g_set_error (error,
                         G_MARKUP_ERROR,
                         G_MARKUP_ERROR_MISSING_ATTRIBUTE,
                         "no \"geometry\" attribute for \"keyboard\"");
            return;
        }
        desc->geometry = g_strdup (attribute);

        attribute = get_attribute (attribute_names, attribute_values,
                                   "symbols");
        if (attribute == NULL) {
            g_set_error (error,
                         G_MARKUP_ERROR,
                         G_MARKUP_ERROR_MISSING_ATTRIBUTE,
                         "no \"symbols\" attribute for \"keyboard\"");
            goto out;
        }
        desc->symbols = g_strdup (attribute);

        attribute = get_attribute (attribute_names, attribute_values,
                                   "longname");
        if (attribute)
            desc->longname = g_strdup (attribute);

        attribute = get_attribute (attribute_names, attribute_values,
                                   "language");
        if (attribute)
            desc->language = g_strdup (attribute);
    }

 out:
    data->element_stack = g_slist_prepend (data->element_stack,
                                           g_strdup (element_name));
}

static void
keyboards_end_element_callback (GMarkupParseContext *pcontext,
                                const gchar         *element_name,
                                gpointer             user_data,
                                GError             **error)
{
    KeyboardsParseData *data = user_data;
    GSList *head = data->element_stack;

    g_free (head->data);
    data->element_stack = g_slist_next (data->element_stack);
    g_slist_free1 (head);
}

static const GMarkupParser keyboards_parser = {
    keyboards_start_element_callback,
    keyboards_end_element_callback,
    0,
    0,
    0
};

struct _GeometryParseData {
    GSList *element_stack;

    EekBounds bounds;
    struct squeek_view **views;
    guint view_idx;
    struct squeek_row *row;
    gint num_rows;
    EekOrientation orientation;
    gdouble corner_radius;
    GSList *points;
    gchar *name;
    EekOutline outline;
    gchar *outline_id;
    guint keycode;

    GString *text;

    GArray *outline_array;

    GHashTable *name_button_hash; // char* -> struct squeek_button*
    GHashTable *keyname_oref_hash; // char* -> guint
    GHashTable *outlineid_oref_hash; // char* -> guint
};
typedef struct _GeometryParseData GeometryParseData;

struct _SymbolsParseData {
    GSList *element_stack;
    GString *text;

    LevelKeyboard *keyboard;
    struct squeek_view *view;

    gchar *label;
    gchar *icon;
    gchar *tooltip;
    guint keyval;
};
typedef struct _SymbolsParseData SymbolsParseData;


struct _PrerequisitesParseData {
    GSList *element_stack;
    GString *text;

    GList *prerequisites;
};
typedef struct _PrerequisitesParseData PrerequisitesParseData;

LevelKeyboard *
eek_xml_layout_real_create_keyboard (const char *keyboard_type,
                                     EekboardContextService *manager)
{
    struct squeek_layout *layout = squeek_load_layout(keyboard_type);
    squeek_layout_place_contents(layout);
    return level_keyboard_new(manager, layout);
}

static void
eek_xml_layout_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
    EekXmlLayout *layout = EEK_XML_LAYOUT (object);
    EekXmlLayoutPrivate *priv = eek_xml_layout_get_instance_private (layout);

    switch (prop_id) {
    case PROP_ID:
        g_free (priv->id);
        priv->id = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eek_xml_layout_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
    EekXmlLayout *layout = EEK_XML_LAYOUT (object);
    EekXmlLayoutPrivate *priv = eek_xml_layout_get_instance_private (layout);

    switch (prop_id) {
    case PROP_ID:
        g_value_set_string (value, priv->id);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eek_xml_layout_finalize (GObject *object)
{
    EekXmlLayoutPrivate *priv = eek_xml_layout_get_instance_private (
            EEK_XML_LAYOUT (object));

    g_free (priv->id);

    if (priv->desc)
        keyboard_desc_free (priv->desc);

    g_free (priv->keyboards_dir);

    G_OBJECT_CLASS (eek_xml_layout_parent_class)->finalize (object);
}

static void
eek_xml_layout_class_init (EekXmlLayoutClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec *pspec;

    gobject_class->set_property = eek_xml_layout_set_property;
    gobject_class->get_property = eek_xml_layout_get_property;
    gobject_class->finalize = eek_xml_layout_finalize;

    pspec = g_param_spec_string ("id",
                 "ID",
                 "ID",
                 NULL,
                 G_PARAM_CONSTRUCT_ONLY |
                                 G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class, PROP_ID, pspec);
}

static void
eek_xml_layout_init (EekXmlLayout *self)
{
    /* void */
}

EekLayout *
eek_xml_layout_new (const gchar *id, GError **error)
{
    return (EekLayout *) g_initable_new (EEK_TYPE_XML_LAYOUT,
                                         NULL,
                                         error,
                                         "id", id,
                                         NULL);
}

static gboolean
initable_init (GInitable    *initable,
               GCancellable *cancellable,
               GError      **error)
{
    EekXmlLayout *layout = EEK_XML_LAYOUT (initable);
    EekXmlLayoutPrivate *priv = eek_xml_layout_get_instance_private (layout);
    GList *keyboards, *p;
    gchar *path;
    EekXmlKeyboardDesc *desc;

    priv->keyboards_dir = g_strdup ((gchar *) g_getenv ("EEKBOARD_KEYBOARDSDIR"));

    if (priv->keyboards_dir == NULL)
        priv->keyboards_dir = g_strdup ("resource:///sm/puri/squeekboard/keyboards/");

    path = g_build_filename (priv->keyboards_dir, "keyboards.xml", NULL);
    keyboards = parse_keyboards (path, error);
    g_free (path);
    if (error && *error)
        return FALSE;

    for (p = keyboards; p; p = p->next) {
        desc = p->data;
        if (g_strcmp0 (desc->id, priv->id) == 0)
            break;
    }
    if (p == NULL) {
        g_set_error (error,
                     EEK_ERROR,
                     EEK_ERROR_LAYOUT_ERROR,
                     "no such keyboard %s",
                     priv->id);
        return FALSE;
    }

    keyboards = g_list_remove_link (keyboards, p);
    priv->desc = p->data;
    g_list_free_1 (p);
    g_list_free_full (keyboards, (GDestroyNotify) keyboard_desc_free);

    return TRUE;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
    initable_iface->init = initable_init;
}

/**
 * eek_xml_list_keyboards:
 *
 * List available keyboards.
 * Returns: (transfer container) (element-type utf8): the list of keyboards
 */
GList *
eek_xml_list_keyboards (void)
{
    const gchar *keyboards_dir;
    gchar *path;
    GList *keyboards;

    keyboards_dir = g_getenv ("EEKBOARD_KEYBOARDSDIR");
    if (keyboards_dir == NULL)
        keyboards_dir = g_strdup ("resource:///sm/puri/squeekboard/keyboards/");
    path = g_build_filename (keyboards_dir, "keyboards.xml", NULL);
    keyboards = parse_keyboards (path, NULL);
    g_free (path);
    return keyboards;
}

EekXmlKeyboardDesc *
eek_xml_keyboard_desc_copy (EekXmlKeyboardDesc *desc)
{
    return g_slice_dup (EekXmlKeyboardDesc, desc);
}

void
eek_xml_keyboard_desc_free (EekXmlKeyboardDesc *desc)
{
    g_free (desc->id);
    g_free (desc->name);
    g_free (desc->geometry);
    g_free (desc->symbols);
    g_free (desc->language);
    g_free (desc->longname);
    g_slice_free (EekXmlKeyboardDesc, desc);
}

static GList *
parse_keyboards (const gchar *path, GError **error)
{
    KeyboardsParseData *data;
    GMarkupParseContext *pcontext;
    GFile *file;
    GFileInputStream *input;
    GList *keyboards;
    gboolean retval;

    file = g_str_has_prefix (path, "resource://")
         ? g_file_new_for_uri  (path)
         : g_file_new_for_path (path);

    input = g_file_read (file, NULL, error);
    g_object_unref (file);
    if (input == NULL)
        return NULL;

    data = keyboards_parse_data_new ();
    pcontext = g_markup_parse_context_new (&keyboards_parser,
                                           0,
                                           data,
                                           NULL);
    retval = parse (pcontext, G_INPUT_STREAM (input), error);
    g_object_unref (input);
    g_markup_parse_context_free (pcontext);
    if (!retval) {
        keyboards_parse_data_free (data);
        return NULL;
    }

    keyboards = data->keyboards;
    data->keyboards = NULL;
    keyboards_parse_data_free (data);
    return keyboards;
}

static gboolean
validate (const gchar **valid_path_list,
          gsize         valid_path_list_len,
          const gchar  *element_name,
          GSList       *element_stack,
          GError      **error)
{
    guint i;
    gchar *element_path;
    GSList *head, *p;
    GString *string;

    head = g_slist_prepend (element_stack, (gchar *)element_name);
    string = g_string_sized_new (64);
    for (p = head; p; p = p->next) {
        g_string_append (string, p->data);
        if (g_slist_next (p))
            g_string_append (string, "/");
    }
    element_path = g_string_free (string, FALSE);
    g_slist_free1 (head);

    for (i = 0; i < valid_path_list_len; i++)
        if (g_strcmp0 (element_path, valid_path_list[i]) == 0)
            break;
    g_free (element_path);

    if (i == valid_path_list_len) {
        gchar *reverse_element_path;

        head = g_slist_reverse (g_slist_copy (element_stack));
        string = g_string_sized_new (64);
        for (p = head; p; p = p->next) {
            g_string_append (string, p->data);
            if (g_slist_next (p))
                g_string_append (string, "/");
        }
        reverse_element_path = g_string_free (string, FALSE);

        abort ();
        g_set_error (error,
                     G_MARKUP_ERROR,
                     G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                     "%s cannot appear as %s",
                     element_name,
                     reverse_element_path);
        g_free (reverse_element_path);

        return FALSE;
    }
    return TRUE;
}

static gboolean
parse (GMarkupParseContext *pcontext,
       GInputStream        *input,
       GError             **error)
{
    gchar buffer[BUFSIZE];

    while (1) {
        gssize nread = g_input_stream_read (input,
                                            buffer,
                                            sizeof (buffer),
                                            NULL,
                                            error);
        if (nread < 0)
            return FALSE;

        if (nread == 0)
            break;

        if (!g_markup_parse_context_parse (pcontext, buffer, nread, error))
            return FALSE;
    }

    return g_markup_parse_context_end_parse (pcontext, error);
}

static const gchar *
get_attribute (const gchar **names, const gchar **values, const gchar *name)
{
    for (; *names && *values; names++, values++) {
        if (g_strcmp0 (*names, name) == 0)
            return *values;
    }
    return NULL;
}
