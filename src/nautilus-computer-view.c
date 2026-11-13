/* nautilus-computer-view.c
 *
 * Native "This PC / My Computer" view for computer:///.
 * UI layout inspired by nautilus-my-computer (https://github.com/yannmasoch/nautilus-my-computer),
 * Copyright (c) 2026 Yann Masoch (MIT License).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include "nautilus-computer-view.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include "nautilus-scheme.h"
#include "nautilus-toolbar-menu-sections.h"
#include "nautilus-view.h"
#include "nautilus-view-info.h"

struct _NautilusComputerView
{
    AdwBin parent_instance;

    NautilusWindowSlot *slot; /* unowned */
    GFile *location;
    NautilusToolbarMenuSections *menu_sections;

    GtkFlowBox *folders_flow;
    GtkFlowBox *drives_flow;
    GVolumeMonitor *volume_monitor;
};

static void nautilus_computer_view_iface_init (NautilusViewInterface *iface);

G_DEFINE_TYPE_WITH_CODE (NautilusComputerView, nautilus_computer_view, ADW_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_VIEW,
                                                nautilus_computer_view_iface_init))

enum
{
    PROP_0,
    PROP_LOCATION,
    PROP_SEARCH_QUERY,
    PROP_LOADING,
    PROP_SEARCHING,
    PROP_SELECTION,
    PROP_EXTENSIONS_BACKGROUND_MENU,
    PROP_TEMPLATES_MENU,
    LAST_PROP
};

static void
open_target_uri (NautilusComputerView *self,
                 const char           *uri)
{
    if (self->slot == NULL || uri == NULL)
    {
        return;
    }

    g_autoptr (GFile) target = g_file_new_for_uri (uri);
    nautilus_window_slot_open_location_full (self->slot, target, 0, NULL);
}

static void
on_card_clicked (GtkButton *button,
                 gpointer   user_data)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (user_data);
    const char *uri = g_object_get_data (G_OBJECT (button), "target-uri");

    open_target_uri (self, uri);
}

static GtkWidget *
create_folder_card (NautilusComputerView *self,
                    const char           *title,
                    const char           *icon_name,
                    const char           *uri)
{
    GtkWidget *button = gtk_button_new ();
    gtk_widget_add_css_class (button, "card");
    gtk_widget_set_size_request (button, 200, -1);
    g_object_set_data_full (G_OBJECT (button), "target-uri", g_strdup (uri), g_free);
    g_signal_connect (button, "clicked", G_CALLBACK (on_card_clicked), self);

    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top (box, 12);
    gtk_widget_set_margin_bottom (box, 12);
    gtk_widget_set_margin_start (box, 14);
    gtk_widget_set_margin_end (box, 14);

    GtkWidget *icon = gtk_image_new_from_icon_name (icon_name);
    gtk_image_set_pixel_size (GTK_IMAGE (icon), 32);

    GtkWidget *label = gtk_label_new (title);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand (label, TRUE);
    gtk_widget_add_css_class (label, "heading");

    gtk_box_append (GTK_BOX (box), icon);
    gtk_box_append (GTK_BOX (box), label);
    gtk_button_set_child (GTK_BUTTON (button), box);

    return button;
}

static GtkWidget *
create_drive_card (NautilusComputerView *self,
                   const char           *title,
                   const char           *icon_name,
                   const char           *uri,
                   guint64               total_bytes,
                   guint64               free_bytes)
{
    GtkWidget *button = gtk_button_new ();
    gtk_widget_add_css_class (button, "card");
    gtk_widget_set_size_request (button, 260, -1);
    g_object_set_data_full (G_OBJECT (button), "target-uri", g_strdup (uri), g_free);
    g_signal_connect (button, "clicked", G_CALLBACK (on_card_clicked), self);

    GtkWidget *outer = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top (outer, 12);
    gtk_widget_set_margin_bottom (outer, 12);
    gtk_widget_set_margin_start (outer, 14);
    gtk_widget_set_margin_end (outer, 14);

    GtkWidget *icon = gtk_image_new_from_icon_name (icon_name);
    gtk_image_set_pixel_size (GTK_IMAGE (icon), 36);
    gtk_widget_set_valign (icon, GTK_ALIGN_CENTER);

    GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_hexpand (vbox, TRUE);

    GtkWidget *label = gtk_label_new (title);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class (label, "heading");
    gtk_box_append (GTK_BOX (vbox), label);

    if (total_bytes > 0)
    {
        double used_fraction = (double) (total_bytes - MIN (free_bytes, total_bytes)) / (double) total_bytes;

        GtkWidget *level = gtk_level_bar_new_for_interval (0.0, 1.0);
        gtk_level_bar_set_value (GTK_LEVEL_BAR (level), CLAMP (used_fraction, 0.0, 1.0));
        gtk_box_append (GTK_BOX (vbox), level);

        g_autofree char *free_str = g_format_size (free_bytes);
        g_autofree char *total_str = g_format_size (total_bytes);
        g_autofree char *sub_text = g_strdup_printf (_("%s free of %s"), free_str, total_str);
        GtkWidget *sub = gtk_label_new (sub_text);
        gtk_label_set_xalign (GTK_LABEL (sub), 0.0f);
        gtk_label_set_ellipsize (GTK_LABEL (sub), PANGO_ELLIPSIZE_END);
        gtk_widget_add_css_class (sub, "dim-label");
        gtk_widget_add_css_class (sub, "caption");
        gtk_box_append (GTK_BOX (vbox), sub);
    }
    else
    {
        GtkWidget *sub = gtk_label_new (_("Ready"));
        gtk_label_set_xalign (GTK_LABEL (sub), 0.0f);
        gtk_widget_add_css_class (sub, "dim-label");
        gtk_widget_add_css_class (sub, "caption");
        gtk_box_append (GTK_BOX (vbox), sub);
    }

    gtk_box_append (GTK_BOX (outer), icon);
    gtk_box_append (GTK_BOX (outer), vbox);
    gtk_button_set_child (GTK_BUTTON (button), outer);

    return button;
}

static void
clear_flow_box (GtkFlowBox *flow)
{
    GtkWidget *child;

    while ((child = gtk_widget_get_first_child (GTK_WIDGET (flow))) != NULL)
    {
        gtk_flow_box_remove (flow, child);
    }
}

static void
populate_folders (NautilusComputerView *self)
{
    struct
    {
        const char *label;
        const char *icon_name;
        const char *path;
    } entries[] = {
        { _("Home"), "user-home-symbolic", g_get_home_dir () },
        { _("Desktop"), "user-desktop-symbolic", g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP) },
        { _("Documents"), "folder-documents-symbolic", g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS) },
        { _("Downloads"), "folder-download-symbolic", g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD) },
        { _("Music"), "folder-music-symbolic", g_get_user_special_dir (G_USER_DIRECTORY_MUSIC) },
        { _("Pictures"), "folder-pictures-symbolic", g_get_user_special_dir (G_USER_DIRECTORY_PICTURES) },
        { _("Videos"), "folder-videos-symbolic", g_get_user_special_dir (G_USER_DIRECTORY_VIDEOS) },
    };

    clear_flow_box (self->folders_flow);

    for (gsize i = 0; i < G_N_ELEMENTS (entries); i++)
    {
        if (entries[i].path == NULL || !g_file_test (entries[i].path, G_FILE_TEST_IS_DIR))
        {
            continue;
        }

        g_autofree char *uri = g_filename_to_uri (entries[i].path, NULL, NULL);
        if (uri == NULL)
        {
            continue;
        }

        GtkWidget *card = create_folder_card (self, entries[i].label, entries[i].icon_name, uri);
        gtk_flow_box_append (self->folders_flow, card);
    }
}

static void
populate_drives (NautilusComputerView *self)
{
    clear_flow_box (self->drives_flow);

#ifdef G_OS_WIN32
    wchar_t buffer[512];
    DWORD len = GetLogicalDriveStringsW (G_N_ELEMENTS (buffer) - 1, buffer);

    if (len == 0 || len >= G_N_ELEMENTS (buffer))
    {
        return;
    }

    for (const wchar_t *drive = buffer; *drive != L'\0'; drive += wcslen (drive) + 1)
    {
        UINT drive_type = GetDriveTypeW (drive);
        const char *icon_name = "drive-harddisk-symbolic";
        const char *default_name = _("Local Disk");

        if (drive_type == DRIVE_NO_ROOT_DIR)
        {
            continue;
        }
        else if (drive_type == DRIVE_REMOVABLE)
        {
            icon_name = "drive-removable-media-symbolic";
            default_name = _("Removable Disk");
        }
        else if (drive_type == DRIVE_CDROM)
        {
            icon_name = "drive-optical-symbolic";
            default_name = _("CD/DVD Drive");
        }
        else if (drive_type == DRIVE_REMOTE)
        {
            icon_name = "network-server-symbolic";
            default_name = _("Network Drive");
        }

        wchar_t vol_name[MAX_PATH + 1] = { 0 };
        GetVolumeInformationW (drive, vol_name, G_N_ELEMENTS (vol_name),
                               NULL, NULL, NULL, NULL, 0);

        g_autofree char *drive_utf8 = g_utf16_to_utf8 ((const gunichar2 *) drive, -1, NULL, NULL, NULL);
        if (drive_utf8 == NULL)
        {
            continue;
        }

        /* Strip trailing backslash for display "(C:)" */
        char drive_letter[8] = { 0 };
        g_snprintf (drive_letter, sizeof (drive_letter), "%c:", drive_utf8[0]);

        g_autofree char *vol_utf8 = (vol_name[0] != L'\0')
            ? g_utf16_to_utf8 ((const gunichar2 *) vol_name, -1, NULL, NULL, NULL)
            : NULL;

        g_autofree char *title = g_strdup_printf ("%s (%s)",
                                                  (vol_utf8 != NULL && vol_utf8[0] != '\0') ? vol_utf8 : default_name,
                                                  drive_letter);

        ULARGE_INTEGER free_avail = { 0 };
        ULARGE_INTEGER total_bytes = { 0 };
        guint64 total = 0;
        guint64 free_b = 0;

        if (GetDiskFreeSpaceExW (drive, &free_avail, &total_bytes, NULL))
        {
            total = total_bytes.QuadPart;
            free_b = free_avail.QuadPart;
        }

        g_autofree char *uri = g_filename_to_uri (drive_utf8, NULL, NULL);
        if (uri != NULL)
        {
            GtkWidget *card = create_drive_card (self, title, icon_name, uri, total, free_b);
            gtk_flow_box_append (self->drives_flow, card);
        }
    }
#else
    g_autoptr (GFile) root = g_file_new_for_path ("/");
    g_autofree char *uri = g_file_get_uri (root);
    GtkWidget *card = create_drive_card (self, _("File System"), "drive-harddisk-symbolic", uri, 0, 0);
    gtk_flow_box_append (self->drives_flow, card);
#endif
}

static void
on_volumes_changed (GVolumeMonitor *monitor,
                    gpointer        item,
                    gpointer        user_data)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (user_data);

    populate_drives (self);
}

static guint
real_get_view_id (NautilusView *view)
{
    return NAUTILUS_VIEW_COMPUTER_ID;
}

static NautilusToolbarMenuSections *
real_get_toolbar_menu_sections (NautilusView *view)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (view);

    return self->menu_sections;
}

static GFile *
real_get_location (NautilusView *view)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (view);

    return self->location;
}

static void
real_set_location (NautilusView *view,
                   GFile        *location)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (view);

    if (g_set_object (&self->location, location))
    {
        g_object_notify (G_OBJECT (self), "location");
    }

    populate_folders (self);
    populate_drives (self);
}

static GList *
real_get_selection (NautilusView *view)
{
    return NULL;
}

static void
real_set_selection (NautilusView *view,
                    GList        *selection)
{
}

static NautilusQuery *
real_get_search_query (NautilusView *view)
{
    return NULL;
}

static void
real_set_search_query (NautilusView  *view,
                       NautilusQuery *query)
{
}

static gboolean
real_is_loading (NautilusView *view)
{
    return FALSE;
}

static gboolean
real_is_searching (NautilusView *view)
{
    return FALSE;
}

static GMenuModel *
real_get_extensions_background_menu (NautilusView *view)
{
    return NULL;
}

static void
real_set_extensions_background_menu (NautilusView *view,
                                     GMenuModel   *menu)
{
}

static GMenuModel *
real_get_templates_menu (NautilusView *view)
{
    return NULL;
}

static void
real_set_templates_menu (NautilusView *view,
                         GMenuModel   *menu)
{
}

static void
nautilus_computer_view_iface_init (NautilusViewInterface *iface)
{
    iface->get_view_id = real_get_view_id;
    iface->get_toolbar_menu_sections = real_get_toolbar_menu_sections;
    iface->get_location = real_get_location;
    iface->set_location = real_set_location;
    iface->get_selection = real_get_selection;
    iface->set_selection = real_set_selection;
    iface->get_search_query = real_get_search_query;
    iface->set_search_query = real_set_search_query;
    iface->is_loading = real_is_loading;
    iface->is_searching = real_is_searching;
    iface->get_extensions_background_menu = real_get_extensions_background_menu;
    iface->set_extensions_background_menu = real_set_extensions_background_menu;
    iface->get_templates_menu = real_get_templates_menu;
    iface->set_templates_menu = real_set_templates_menu;
}

static void
nautilus_computer_view_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (object);

    switch (prop_id)
    {
        case PROP_LOCATION:
            g_value_set_object (value, self->location);
            break;
        case PROP_SEARCH_QUERY:
            g_value_set_object (value, NULL);
            break;
        case PROP_LOADING:
        case PROP_SEARCHING:
            g_value_set_boolean (value, FALSE);
            break;
        case PROP_SELECTION:
            g_value_set_pointer (value, NULL);
            break;
        case PROP_EXTENSIONS_BACKGROUND_MENU:
        case PROP_TEMPLATES_MENU:
            g_value_set_object (value, NULL);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nautilus_computer_view_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (object);

    switch (prop_id)
    {
        case PROP_LOCATION:
            real_set_location (NAUTILUS_VIEW (self), g_value_get_object (value));
            break;
        case PROP_SEARCH_QUERY:
        case PROP_SELECTION:
        case PROP_EXTENSIONS_BACKGROUND_MENU:
        case PROP_TEMPLATES_MENU:
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nautilus_computer_view_dispose (GObject *object)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (object);

    if (self->volume_monitor != NULL)
    {
        g_signal_handlers_disconnect_by_data (self->volume_monitor, self);
        g_clear_object (&self->volume_monitor);
    }

    g_clear_object (&self->location);
    g_clear_pointer (&self->menu_sections, g_free);

    G_OBJECT_CLASS (nautilus_computer_view_parent_class)->dispose (object);
}

static GtkWidget *
create_section_header (const char *title)
{
    GtkWidget *label = gtk_label_new (title);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
    gtk_widget_add_css_class (label, "title-4");
    return label;
}

static void
nautilus_computer_view_init (NautilusComputerView *self)
{
    self->location = g_file_new_for_uri (SCHEME_COMPUTER ":///");
    self->menu_sections = g_new0 (NautilusToolbarMenuSections, 1);

    GtkWidget *scrolled = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand (scrolled, TRUE);
    gtk_widget_set_hexpand (scrolled, TRUE);

    GtkWidget *clamp = adw_clamp_new ();
    adw_clamp_set_maximum_size (ADW_CLAMP (clamp), 1100);
    adw_clamp_set_tightening_threshold (ADW_CLAMP (clamp), 700);

    GtkWidget *content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_top (content, 24);
    gtk_widget_set_margin_bottom (content, 24);
    gtk_widget_set_margin_start (content, 24);
    gtk_widget_set_margin_end (content, 24);

    gtk_box_append (GTK_BOX (content), create_section_header (_("Folders")));

    self->folders_flow = GTK_FLOW_BOX (gtk_flow_box_new ());
    gtk_flow_box_set_selection_mode (self->folders_flow, GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous (self->folders_flow, TRUE);
    gtk_flow_box_set_column_spacing (self->folders_flow, 12);
    gtk_flow_box_set_row_spacing (self->folders_flow, 12);
    gtk_flow_box_set_min_children_per_line (self->folders_flow, 1);
    gtk_flow_box_set_max_children_per_line (self->folders_flow, 4);
    gtk_box_append (GTK_BOX (content), GTK_WIDGET (self->folders_flow));

    gtk_box_append (GTK_BOX (content), create_section_header (_("Devices and drives")));

    self->drives_flow = GTK_FLOW_BOX (gtk_flow_box_new ());
    gtk_flow_box_set_selection_mode (self->drives_flow, GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous (self->drives_flow, TRUE);
    gtk_flow_box_set_column_spacing (self->drives_flow, 12);
    gtk_flow_box_set_row_spacing (self->drives_flow, 12);
    gtk_flow_box_set_min_children_per_line (self->drives_flow, 1);
    gtk_flow_box_set_max_children_per_line (self->drives_flow, 3);
    gtk_box_append (GTK_BOX (content), GTK_WIDGET (self->drives_flow));

    adw_clamp_set_child (ADW_CLAMP (clamp), content);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), clamp);
    adw_bin_set_child (ADW_BIN (self), scrolled);

    self->volume_monitor = g_volume_monitor_get ();
    g_signal_connect (self->volume_monitor, "volume-added", G_CALLBACK (on_volumes_changed), self);
    g_signal_connect (self->volume_monitor, "volume-removed", G_CALLBACK (on_volumes_changed), self);
    g_signal_connect (self->volume_monitor, "mount-added", G_CALLBACK (on_volumes_changed), self);
    g_signal_connect (self->volume_monitor, "mount-removed", G_CALLBACK (on_volumes_changed), self);

    populate_folders (self);
    populate_drives (self);
}

static void
nautilus_computer_view_class_init (NautilusComputerViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->get_property = nautilus_computer_view_get_property;
    object_class->set_property = nautilus_computer_view_set_property;
    object_class->dispose = nautilus_computer_view_dispose;

    g_object_class_override_property (object_class, PROP_LOCATION, "location");
    g_object_class_override_property (object_class, PROP_SEARCH_QUERY, "search-query");
    g_object_class_override_property (object_class, PROP_LOADING, "loading");
    g_object_class_override_property (object_class, PROP_SEARCHING, "searching");
    g_object_class_override_property (object_class, PROP_SELECTION, "selection");
    g_object_class_override_property (object_class, PROP_EXTENSIONS_BACKGROUND_MENU, "extensions-background-menu");
    g_object_class_override_property (object_class, PROP_TEMPLATES_MENU, "templates-menu");
}

NautilusComputerView *
nautilus_computer_view_new (NautilusWindowSlot *slot)
{
    NautilusComputerView *self = g_object_new (NAUTILUS_TYPE_COMPUTER_VIEW, NULL);
    self->slot = slot;
    return self;
}
