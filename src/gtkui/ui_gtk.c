/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2010  Audacious development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>

#include "config.h"
#include "gtkui_cfg.h"
#include "ui_gtk.h"
#include "ui_playlist_notebook.h"
#include "ui_manager.h"
#include "ui_infoarea.h"
#include "playlist_util.h"
#include "actions-mainwin.h"
#include "actions-playlist.h"

gboolean multi_column_view;

static GtkWidget *label_time;
static GtkWidget *slider;
static GtkWidget *volume;
static GtkWidget *visualizer = NULL;
GtkWidget *playlist_box;
GtkWidget *window;       /* the main window */
GtkWidget *vbox;         /* the main vertical box */
GtkWidget *menu;
GtkWidget *infoarea = NULL;

static gulong slider_change_handler_id;
static gboolean slider_is_moving = FALSE;
static gboolean volume_slider_is_moving = FALSE;
static gint slider_position;
static guint update_song_timeout_source = 0;
static guint update_volume_timeout_source = 0;
static gulong volume_change_handler_id;

extern GtkWidget *ui_playlist_notebook_tab_title_editing;

static gboolean _ui_initialize(InterfaceCbs * cbs);
static gboolean _ui_finalize(void);

Interface gtkui_interface = {
    .id = "gtkui",
    .desc = N_("GTK Foobar-like Interface"),
    .init = _ui_initialize,
    .fini = _ui_finalize,
};

SIMPLE_INTERFACE_PLUGIN("gtkui", &gtkui_interface);

static void save_window_size (void)
{
    gtk_window_get_position ((GtkWindow *) window, & config.player_x,
     & config.player_y);

    if (gtk_window_get_resizable ((GtkWindow *) window))
        gtk_window_get_size ((GtkWindow *) window, & config.player_width,
         & config.player_height);
}

static void shrink_window (void)
{
    GtkRequisition r;

    gtk_widget_size_request (window, & r);
    gtk_window_resize ((GtkWindow *) window, r.width, r.height);
    gtk_window_set_resizable ((GtkWindow *) window, FALSE);
}

static void unshrink_window (void)
{
    gtk_window_resize ((GtkWindow *) window, config.player_width,
     config.player_height);
    gtk_window_set_resizable ((GtkWindow *) window, TRUE);
}

static void container_remove_reversed (GtkWidget * w, GtkWidget * c)
{
    gtk_container_remove ((GtkContainer *) c, w);
}

void setup_panes (void)
{
    static GtkWidget * panes = NULL;
    GtkWidget * a, * b;

    save_window_size ();

    if (panes)
    {
        gtk_container_foreach ((GtkContainer *) panes, (GtkCallback)
         container_remove_reversed, panes);
        gtk_widget_destroy (panes);
    }

    gtk_container_foreach ((GtkContainer *) playlist_box, (GtkCallback)
     container_remove_reversed, playlist_box);

    if (config.vis_position == VIS_ON_LEFT || config.vis_position == VIS_ON_TOP)
    {
        a = (config.vis_position == VIS_IN_TABS) ? NULL : visualizer;
        b = config.playlist_visible ? (GtkWidget *) UI_PLAYLIST_NOTEBOOK : NULL;
    }
    else
    {
        a = config.playlist_visible ? (GtkWidget *) UI_PLAYLIST_NOTEBOOK : NULL;
        b = (config.vis_position == VIS_IN_TABS) ? NULL : visualizer;
    }

    if (! a)
    {
        a = b;
        b = NULL;
    }

    if (! a)
    {
        shrink_window ();
        return;
    }

    unshrink_window ();

    if (! b)
    {
        gtk_box_pack_start ((GtkBox *) playlist_box, a, TRUE, TRUE, 0);
        gtk_widget_show (a);
        return;
    }

    panes = (config.vis_position == VIS_ON_LEFT || config.vis_position ==
     VIS_ON_RIGHT) ? gtk_hpaned_new () : gtk_vpaned_new ();
    gtk_box_pack_start ((GtkBox *) playlist_box, panes, TRUE, TRUE, 0);
    g_signal_connect ((GObject *) panes, "destroy", (GCallback)
     gtk_widget_destroyed, & panes);

    gtk_paned_add1 ((GtkPaned *) panes, a);
    gtk_paned_add2 ((GtkPaned *) panes, b);
    gtk_widget_show (panes);
    gtk_widget_show (a);
    gtk_widget_show (b);
}

static void ui_run_gtk_plugin(GtkWidget *parent, const gchar *name)
{
    GtkWidget *label;

    g_return_if_fail(parent != NULL);
    g_return_if_fail(name != NULL);

    if (visualizer) /* only one at a time */
        return;

    visualizer = parent;
    g_object_ref ((GObject *) visualizer);

    if (config.vis_position == VIS_IN_TABS)
    {
        label = gtk_label_new(name);
        gtk_notebook_append_page(UI_PLAYLIST_NOTEBOOK, parent, label);
    }
    else
        setup_panes ();
}

static void ui_stop_gtk_plugin(GtkWidget *parent)
{
    if (parent != visualizer) /* only one at a time */
        return;

    g_object_unref ((GObject *) visualizer);
    visualizer = NULL;

    if (config.vis_position == VIS_IN_TABS)
        gtk_notebook_remove_page(UI_PLAYLIST_NOTEBOOK, gtk_notebook_page_num
         (UI_PLAYLIST_NOTEBOOK, parent));
    else
        setup_panes ();
}

static gboolean window_delete()
{
    audacious_drct_quit ();
    return TRUE;
}

void show_preferences_window(gboolean show)
{
    /* static GtkWidget * * prefswin = NULL; */
    static void * * prefswin = NULL;

    if (show)
    {
        if ((prefswin != NULL) && (*prefswin != NULL))
        {
            gtk_window_present(GTK_WINDOW(*prefswin));
            return;
        }

        prefswin = gtkui_interface.ops->create_prefs_window();

        gtk_widget_show_all(*prefswin);
    }
    else
    {
        if ((prefswin != NULL) && (*prefswin != NULL))
        {
            gtkui_interface.ops->destroy_prefs_window();
        }
    }
}

static void button_open_pressed()
{
    audgui_run_filebrowser(TRUE);
}

static void button_add_pressed()
{
    audgui_run_filebrowser(FALSE);
}

static void button_play_pressed()
{
    audacious_drct_play();
}

static void button_pause_pressed()
{
    audacious_drct_pause();
}

static void button_stop_pressed()
{
    audacious_drct_stop();
}

static void button_previous_pressed()
{
    audacious_drct_pl_prev();
}

static void button_next_pressed()
{
    audacious_drct_pl_next();
}

static void ui_set_song_info(void *unused, void *another)
{
    gchar *title = aud_playback_get_title();
    gchar *title_s = g_strdup_printf("%s - Audacious", title);

    gtk_window_set_title(GTK_WINDOW(window), title_s);
    g_free(title_s);

    ui_playlist_notebook_add_tab_label_markup(aud_playlist_get_playing(), FALSE);
}

static void ui_playlist_created(void *data, void *unused)
{
    ui_playlist_notebook_create_tab(GPOINTER_TO_INT(data));
}

static void ui_playlist_destroyed(void *data, void *unused)
{
    ui_playlist_notebook_destroy_tab(GPOINTER_TO_INT(data));
}

static void ui_mainwin_show()
{
    if (config.save_window_position)
        gtk_window_move(GTK_WINDOW(window), config.player_x, config.player_y);

    gtk_widget_show(window);
    gtk_window_present(GTK_WINDOW(window));
}

static void ui_mainwin_hide()
{
    if (config.save_window_position)
        gtk_window_get_position(GTK_WINDOW(window), &config.player_x, &config.player_y);

    gtk_widget_hide(window);
}

static void ui_mainwin_toggle_visibility(gpointer hook_data, gpointer user_data)
{
    gboolean show = GPOINTER_TO_INT(hook_data);

    config.player_visible = show;
    aud_cfg->player_visible = show;

    if (show)
    {
        ui_mainwin_show();
    }
    else
    {
        ui_mainwin_hide();
    }
}

static void ui_toggle_visibility(void)
{
    ui_mainwin_toggle_visibility(GINT_TO_POINTER(!config.player_visible), NULL);
}

static void ui_show_error(const gchar * markup)
{
    GtkWidget *dialog =
        gtk_message_dialog_new_with_markup(GTK_WINDOW(window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_OK,
                                           "%s",_(markup));

    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_widget_show(GTK_WIDGET(dialog));

    g_signal_connect_swapped(dialog, "response",
                             G_CALLBACK(gtk_widget_destroy),
                             dialog);
}

static void ui_update_time_info(gint time)
{
    gchar text[128];
    gint length = audacious_drct_get_length();

    time /= 1000;
    length /= 1000;

    g_snprintf(text, sizeof(text) / sizeof(gchar), "<tt><b>%.2d:%.2d/%.2d:%.2d</b></tt>", time / 60, time % 60, length / 60, length % 60);
    gtk_label_set_markup(GTK_LABEL(label_time), text);
}

static gboolean ui_update_song_info(gpointer hook_data, gpointer user_data)
{
    if (!audacious_drct_get_playing())
    {
        if (GTK_IS_WIDGET(slider))
            gtk_range_set_value(GTK_RANGE(slider), 0.0);
        return FALSE;
    }

    if (slider_is_moving)
        return TRUE;

    gint time = audacious_drct_get_time();

    if (!g_signal_handler_is_connected(slider, slider_change_handler_id))
        return TRUE;

    g_signal_handler_block(slider, slider_change_handler_id);
    gtk_range_set_value(GTK_RANGE(slider), (gdouble) time);
    g_signal_handler_unblock(slider, slider_change_handler_id);

    ui_update_time_info(time);

    return TRUE;
}

static void ui_clear_song_info()
{
    if (!GTK_IS_WINDOW(window)) return;

    gtk_window_set_title(GTK_WINDOW(window), "Audacious");
    gtk_label_set_markup(GTK_LABEL(label_time), "<tt><b>00:00/00:00</b></tt>");
}

static gboolean ui_slider_value_changed_cb(GtkRange * range, gpointer user_data)
{
    gint seek_;

    seek_ = gtk_range_get_value(range);

    /* XXX: work around a horrible bug in playback_seek(), also
       we should do mseek here. --nenolod */
    audacious_drct_seek(seek_ != 0 ? seek_ : 1);

    slider_is_moving = FALSE;

    return TRUE;
}

static gboolean ui_slider_change_value_cb(GtkRange * range, GtkScrollType scroll)
{
    ui_update_time_info(gtk_range_get_value(range));
    return FALSE;
}

static gboolean ui_slider_button_press_cb(GtkWidget * widget, GdkEventButton * event, gpointer user_data)
{
    slider_is_moving = TRUE;
    slider_position = gtk_range_get_value(GTK_RANGE(widget));

    /* HACK: clicking with the left mouse button moves the slider
       to the location of the click. */
    if (event->button == 1)
        event->button = 2;

    return FALSE;
}

static gboolean ui_slider_button_release_cb(GtkWidget * widget, GdkEventButton * event, gpointer user_data)
{
    /* HACK: see ui_slider_button_press_cb */
    if (event->button == 1)
        event->button = 2;

    if (slider_position == (gint) gtk_range_get_value(GTK_RANGE(widget)))
        slider_is_moving = FALSE;

    return FALSE;
}

static gboolean ui_volume_value_changed_cb(GtkButton * button, gdouble volume, gpointer user_data)
{
    audacious_drct_set_volume((gint) volume, (gint) volume);

    return TRUE;
}

static void ui_volume_pressed_cb(GtkButton *button, gpointer user_data)
{
    volume_slider_is_moving = TRUE;
}

static void ui_volume_released_cb(GtkButton *button, gpointer user_data)
{
    volume_slider_is_moving = FALSE;
}

static gboolean ui_volume_slider_update(gpointer data)
{
    gint volume;
    static gint last_volume = -1;

    if (volume_slider_is_moving || data == NULL)
        return TRUE;

    audacious_drct_get_volume_main(&volume);

    if (last_volume == volume)
        return TRUE;

    last_volume = volume;

    if (volume == (gint) gtk_scale_button_get_value(GTK_SCALE_BUTTON(data)))
        return TRUE;

    g_signal_handler_block(data, volume_change_handler_id);
    gtk_scale_button_set_value(GTK_SCALE_BUTTON(data), volume);
    g_signal_handler_unblock(data, volume_change_handler_id);

    return TRUE;
}

static void set_slider_length (gint length)
{
    if (g_signal_handler_is_connected (slider, slider_change_handler_id))
        g_signal_handler_block (slider, slider_change_handler_id);

    if (length > 0)
    {
        gtk_range_set_range ((GtkRange *) slider, 0, length);
        gtk_widget_set_sensitive (slider, TRUE);
    }
    else
    {
        gtk_range_set_value(GTK_RANGE(slider), 0.0);
        gtk_widget_set_sensitive (slider, FALSE);
    }

    if (g_signal_handler_is_connected (slider, slider_change_handler_id))
        g_signal_handler_unblock (slider, slider_change_handler_id);
}

static void ui_playback_begin(gpointer hook_data, gpointer user_data)
{
    ui_update_song_info(NULL, NULL);

    /* update song info 4 times a second */
    update_song_timeout_source = g_timeout_add(250, (GSourceFunc) ui_update_song_info, NULL);

    set_slider_length (audacious_drct_get_length ());
}

static void ui_playback_stop(gpointer hook_data, gpointer user_data)
{
    if (update_song_timeout_source)
    {
        g_source_remove(update_song_timeout_source);
        update_song_timeout_source = 0;
    }

    ui_clear_song_info();
    set_slider_length (0);
}

static void ui_playback_end(gpointer hook_data, gpointer user_data)
{
    ui_update_song_info(NULL, NULL);
}

static GtkWidget *gtk_toolbar_button_add(GtkWidget * toolbar, void (*callback) (), const gchar * stock_id)
{
    GtkWidget *icon;
    GtkWidget *button = gtk_button_new();

    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(button, FALSE);

    icon = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
    gtk_container_add(GTK_CONTAINER(button), icon);

    gtk_box_pack_start(GTK_BOX(toolbar), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(callback), NULL);

    return button;
}

static GtkWidget *gtk_markup_label_new(const gchar * str)
{
    GtkWidget *label = gtk_label_new(str);
    g_object_set(G_OBJECT(label), "use-markup", TRUE, NULL);
    return label;
}

void set_volume_diff(gint diff)
{
    gint vol = gtk_scale_button_get_value(GTK_SCALE_BUTTON(volume));
    gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume), CLAMP(vol + diff, 0, 100));
}

static gboolean ui_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    if (ui_playlist_notebook_tab_title_editing != NULL &&
        event->keyval != GDK_KP_Enter && event->keyval != GDK_Escape)
    {
        GtkWidget *entry = g_object_get_data(G_OBJECT(ui_playlist_notebook_tab_title_editing), "entry");
        gtk_widget_event(entry, (GdkEvent*) event);
        return TRUE;
    }

    switch (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK))
    {
        case 0:
            switch (event->keyval)
            {
                case GDK_F2:
                    ui_playlist_notebook_edit_tab_title(NULL);
                    break;

                case GDK_minus: //FIXME
                    set_volume_diff(-5);
                    break;

                case GDK_plus: //FIXME
                    set_volume_diff(5);
                    break;

                case GDK_Left:
                case GDK_KP_Left:
                case GDK_KP_7:
                    if (audacious_drct_get_playing ())
                        audacious_drct_seek (audacious_drct_get_time () - 5000);
                    break;

                case GDK_Right:
                case GDK_KP_Right:
                case GDK_KP_9:
                    if (audacious_drct_get_playing ())
                        audacious_drct_seek (audacious_drct_get_time () + 5000);
                    break;

                case GDK_KP_4:
                    audacious_drct_pl_prev();
                    break;

                case GDK_KP_6:
                    audacious_drct_pl_next();
                    break;

                case GDK_KP_Insert:
                    action_jump_to_file();
                    break;

                case GDK_space:
                    if (audacious_drct_get_playing())
                        audacious_drct_pause();
                    else
                        audacious_drct_play();
                    break;

                case GDK_Escape:
                    if (ui_playlist_notebook_tab_title_editing == NULL)
                    {
                        gint playing = aud_playlist_get_playing();
                        gtk_notebook_set_current_page(UI_PLAYLIST_NOTEBOOK, playing);
                        playlist_scroll_to_row(playlist_get_playing_treeview(),
                            aud_playlist_get_position(playing));
                        break;
                    }

                case GDK_Tab:
                    action_playlist_next();
                    break;

                default:
                    return FALSE;
            }
            break;

        case GDK_SHIFT_MASK:
        {
            switch (event->keyval)
            {
                case GDK_ISO_Left_Tab:
                case GDK_Tab:
                    action_playlist_prev();
                    break;

                default:
                    return FALSE;
            }
            break;
        }
        default:
            return FALSE;
    }

    return TRUE;
}

static void stop_after_song_toggled (void * data, void * user)
{
    check_set (toggleaction_group_others, "stop after current song",
     aud_cfg->stopaftersong);
}

static void ui_hooks_associate(void)
{
    aud_hook_associate("title change", ui_set_song_info, NULL);
    aud_hook_associate("playback seek", (HookFunction) ui_update_song_info, NULL);
    aud_hook_associate("playback begin", ui_playback_begin, NULL);
    aud_hook_associate("playback stop", ui_playback_stop, NULL);
    aud_hook_associate("playback end", ui_playback_end, NULL);
    aud_hook_associate("playlist insert", ui_playlist_created, NULL);
    aud_hook_associate("playlist delete", ui_playlist_destroyed, NULL);
    aud_hook_associate("mainwin show", ui_mainwin_toggle_visibility, NULL);
    aud_hook_associate("playlist update", ui_playlist_notebook_update, NULL);
    aud_hook_associate("toggle stop after song", stop_after_song_toggled, NULL);
}

static void ui_hooks_disassociate(void)
{
    aud_hook_dissociate("title change", ui_set_song_info);
    aud_hook_dissociate("playback seek", (HookFunction) ui_update_song_info);
    aud_hook_dissociate("playback begin", ui_playback_begin);
    aud_hook_dissociate("playback stop", ui_playback_stop);
    aud_hook_dissociate("playback end", ui_playback_end);
    aud_hook_dissociate("playlist insert", ui_playlist_created);
    aud_hook_dissociate("playlist delete", ui_playlist_destroyed);
    aud_hook_dissociate("mainwin show", ui_mainwin_toggle_visibility);
    aud_hook_dissociate("playlist update", ui_playlist_notebook_update);
    aud_hook_dissociate("toggle stop after song", stop_after_song_toggled);
}

static gboolean _ui_initialize(InterfaceCbs * cbs)
{
    GtkWidget *tophbox;         /* box to contain toolbar and shbox */
    GtkWidget *buttonbox;       /* contains buttons like "open", "next" */
    GtkWidget *shbox;           /* box for volume control + slider + time combo --nenolod */
    GtkWidget *button_open, *button_add, *button_play, *button_pause, *button_stop, *button_previous, *button_next;
    GtkWidget *evbox;
    GtkAccelGroup *accel;

    gint lvol = 0, rvol = 0;    /* Left and Right for the volume control */

    gtkui_cfg_load();

    multi_column_view = config.multi_column_view;

    audgui_set_default_icon();
    audgui_register_stock_icons();

    ui_manager_init();
    ui_manager_create_menus();

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), MAINWIN_DEFAULT_WIDTH, MAINWIN_DEFAULT_HEIGHT);

    if (config.save_window_position && config.player_width && config.player_height)
        gtk_window_resize(GTK_WINDOW(window), config.player_width, config.player_height);

    if (config.save_window_position && config.player_x != -1)
        gtk_window_move(GTK_WINDOW(window), config.player_x, config.player_y);
    else
        gtk_window_move(GTK_WINDOW(window), MAINWIN_DEFAULT_POS_X, MAINWIN_DEFAULT_POS_Y);

    g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(window_delete), NULL);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    menu = ui_manager_get_menus();
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, TRUE, 0);

    accel = ui_manager_get_accel_group();
    gtk_window_add_accel_group(GTK_WINDOW(window), accel);

    tophbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), tophbox, FALSE, TRUE, 0);

    buttonbox = gtk_hbox_new(FALSE, 0);
    button_open = gtk_toolbar_button_add(buttonbox, button_open_pressed, GTK_STOCK_OPEN);
    button_add = gtk_toolbar_button_add(buttonbox, button_add_pressed, GTK_STOCK_ADD);
    button_play = gtk_toolbar_button_add(buttonbox, button_play_pressed, GTK_STOCK_MEDIA_PLAY);
    button_pause = gtk_toolbar_button_add(buttonbox, button_pause_pressed, GTK_STOCK_MEDIA_PAUSE);
    button_stop = gtk_toolbar_button_add(buttonbox, button_stop_pressed, GTK_STOCK_MEDIA_STOP);
    button_previous = gtk_toolbar_button_add(buttonbox, button_previous_pressed, GTK_STOCK_MEDIA_PREVIOUS);
    button_next = gtk_toolbar_button_add(buttonbox, button_next_pressed, GTK_STOCK_MEDIA_NEXT);

    gtk_box_pack_start(GTK_BOX(tophbox), buttonbox, FALSE, FALSE, 0);

    /* The slider and time display are packed in an event box so that they can
     * be redrawn separately as they are updated. */
    evbox = gtk_event_box_new ();
    gtk_box_pack_start ((GtkBox *) tophbox, evbox, TRUE, TRUE, 0);

    shbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add ((GtkContainer *) evbox, shbox);

    slider = gtk_hscale_new(NULL);
    gtk_scale_set_draw_value(GTK_SCALE(slider), FALSE);
    /* TODO: make this configureable */
    gtk_range_set_update_policy(GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS);
    gtk_widget_set_size_request(slider, 120, -1);
    gtk_widget_set_can_focus(slider, FALSE);
    gtk_box_pack_start(GTK_BOX(shbox), slider, TRUE, TRUE, 0);

    label_time = gtk_markup_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(shbox), label_time, FALSE, FALSE, 5);

    volume = gtk_volume_button_new();
    gtk_button_set_relief(GTK_BUTTON(volume), GTK_RELIEF_NONE);
    gtk_scale_button_set_adjustment(GTK_SCALE_BUTTON(volume), GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 100, 1, 5, 0)));
    gtk_widget_set_can_focus(volume, FALSE);
    /* Set the default volume to the balance average.
       (I'll add balance control later) -Ryan */
    audacious_drct_get_volume(&lvol, &rvol);
    gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume), (lvol + rvol) / 2);
    gtk_box_pack_start(GTK_BOX(shbox), volume, FALSE, FALSE, 0);

    playlist_box = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), playlist_box, TRUE, TRUE, 0);

    /* Create playlist notebook */
    ui_playlist_notebook_new ();
    g_object_ref ((GObject *) UI_PLAYLIST_NOTEBOOK);

    if (config.vis_position == VIS_IN_TABS)
    {
        AUDDBG("vis in tabs\n");
        gtk_box_pack_end(GTK_BOX(playlist_box), (GtkWidget *)
         UI_PLAYLIST_NOTEBOOK, TRUE, TRUE, 0);
    }

    if (config.infoarea_visible)
    {
        AUDDBG ("infoarea setup\n");
        infoarea = ui_infoarea_new ();
        gtk_box_pack_end ((GtkBox *) vbox, infoarea, FALSE, FALSE, 0);
    }

    AUDDBG("hooks associate\n");
    ui_hooks_associate();

    AUDDBG("playlist associate\n");
    ui_playlist_notebook_populate();

    slider_change_handler_id = g_signal_connect(slider, "value-changed", G_CALLBACK(ui_slider_value_changed_cb), NULL);

    g_signal_connect(slider, "change-value", G_CALLBACK(ui_slider_change_value_cb), NULL);
    g_signal_connect(slider, "button-press-event", G_CALLBACK(ui_slider_button_press_cb), NULL);
    g_signal_connect(slider, "button-release-event", G_CALLBACK(ui_slider_button_release_cb), NULL);

    volume_change_handler_id = g_signal_connect(volume, "value-changed", G_CALLBACK(ui_volume_value_changed_cb), NULL);
    g_signal_connect(volume, "pressed", G_CALLBACK(ui_volume_pressed_cb), NULL);
    g_signal_connect(volume, "released", G_CALLBACK(ui_volume_released_cb), NULL);
    update_volume_timeout_source = g_timeout_add(250, (GSourceFunc) ui_volume_slider_update, volume);

    g_signal_connect(window, "key-press-event", G_CALLBACK(ui_key_press_cb), NULL);

    gtk_widget_show_all(vbox);

    if (!config.menu_visible)
        gtk_widget_hide(menu);

    setup_panes ();

    if (config.player_visible)
        ui_mainwin_toggle_visibility(GINT_TO_POINTER(config.player_visible), NULL);

    if (audacious_drct_get_playing())
    {
        ui_set_song_info(NULL, NULL);
        ui_playback_begin(NULL, NULL);
    }
    else
        ui_playback_stop (NULL, NULL);

    AUDDBG("check menu settings\n");
    check_set(toggleaction_group_others, "view menu", config.menu_visible);
    check_set(toggleaction_group_others, "view playlists", config.playlist_visible);
    check_set(toggleaction_group_others, "view infoarea", config.infoarea_visible);
    check_set(toggleaction_group_others, "playback repeat", aud_cfg->repeat);
    check_set(toggleaction_group_others, "playback shuffle", aud_cfg->shuffle);
    check_set(toggleaction_group_others, "playback no playlist advance", aud_cfg->no_playlist_advance);
    check_set(toggleaction_group_others, "stop after current song", aud_cfg->stopaftersong);

    AUDDBG("callback setup\n");

    /* Register interface callbacks */
    cbs->show_prefs_window = show_preferences_window;
    cbs->run_filebrowser = audgui_run_filebrowser;
    cbs->hide_filebrowser = audgui_hide_filebrowser;
    cbs->toggle_visibility = ui_toggle_visibility;
    cbs->show_error = ui_show_error;
    cbs->show_jump_to_track = audgui_jump_to_track;
    cbs->hide_jump_to_track = audgui_jump_to_track_hide;
    cbs->show_about_window = audgui_show_about_window;
    cbs->hide_about_window = audgui_hide_about_window;
    cbs->run_gtk_plugin = (void *) ui_run_gtk_plugin;
    cbs->stop_gtk_plugin = (void *) ui_stop_gtk_plugin;

    AUDDBG("launch\n");
    gtk_main();

    return TRUE;
}

static gboolean _ui_finalize(void)
{
    if (update_song_timeout_source)
    {
        g_source_remove(update_song_timeout_source);
        update_song_timeout_source = 0;
    }

    if (update_volume_timeout_source)
    {
        g_source_remove(update_volume_timeout_source);
        update_volume_timeout_source = 0;
    }

    save_window_size ();
    gtkui_cfg_save();
    gtkui_cfg_free();
    ui_hooks_disassociate();
    g_object_unref ((GObject *) UI_PLAYLIST_NOTEBOOK);
    gtk_widget_destroy (window);
    return TRUE;
}
