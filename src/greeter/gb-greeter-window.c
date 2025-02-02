/* gb-greeter-window.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gb-greeter-window"

#include <glib/gi18n.h>
#include <ide.h>

#include "egg-search-bar.h"
#include "egg-signal-group.h"
#include "egg-state-machine.h"

#include "gb-application.h"
#include "gb-greeter-project-row.h"
#include "gb-greeter-window.h"
#include "gb-gtk.h"
#include "gb-scrolled-window.h"
#include "gb-settings.h"

struct _GbGreeterWindow
{
  GtkApplicationWindow  parent_instance;

  EggSignalGroup       *signal_group;
  IdeRecentProjects    *recent_projects;
  IdePatternSpec       *pattern_spec;

  GtkViewport          *viewport;
  GtkWidget            *header_bar;
  GtkBox               *my_projects_container;
  GtkListBox           *my_projects_list_box;
  GtkBox               *other_projects_container;
  GtkListBox           *other_projects_list_box;
  GtkButton            *remove_button;
  GtkSearchEntry       *search_entry;
  EggStateMachine      *state_machine;

  gint                  selected_count;
};

G_DEFINE_TYPE (GbGreeterWindow, gb_greeter_window, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_RECENT_PROJECTS,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gb_greeter_window_first_visible_cb (GtkWidget *widget,
                                    gpointer   user_data)
{
  GtkWidget **row = user_data;

  if ((*row == NULL) && gtk_widget_get_child_visible (widget))
    *row = widget;
}

static void
gb_greeter_window__search_entry_activate (GbGreeterWindow *self,
                                          GtkSearchEntry  *search_entry)
{
  GtkWidget *row = NULL;

  g_assert (GB_IS_GREETER_WINDOW (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search_entry));

  gtk_container_foreach (GTK_CONTAINER (self->my_projects_list_box),
                         gb_greeter_window_first_visible_cb,
                         &row);
  if (row == NULL)
    gtk_container_foreach (GTK_CONTAINER (self->other_projects_list_box),
                           gb_greeter_window_first_visible_cb,
                           &row);

  if (row != NULL)
    g_signal_emit_by_name (row, "activate");
}

IdeRecentProjects *
gb_greeter_window_get_recent_projects (GbGreeterWindow *self)
{
  g_return_val_if_fail (GB_IS_GREETER_WINDOW (self), NULL);

  return self->recent_projects;
}

static void
gb_greeter_window_apply_filter_cb (GtkWidget *widget,
                                   gpointer   user_data)
{
  gboolean *visible = user_data;

  g_assert (GB_IS_GREETER_PROJECT_ROW (widget));

  if (gtk_widget_get_child_visible (widget))
    *visible = TRUE;
}

static void
gb_greeter_window_apply_filter (GbGreeterWindow *self,
                                GtkListBox      *list_box,
                                GtkWidget       *container)
{
  gboolean visible = FALSE;

  g_assert (GB_IS_GREETER_WINDOW (self));
  g_assert (GTK_IS_LIST_BOX (list_box));
  g_assert (GTK_IS_CONTAINER (container));

  gtk_list_box_invalidate_filter (list_box);
  gtk_container_foreach (GTK_CONTAINER (list_box), gb_greeter_window_apply_filter_cb, &visible);
  gtk_widget_set_visible (GTK_WIDGET (container), visible);
}

static void
gb_greeter_window_apply_filter_all (GbGreeterWindow *self)
{
  const gchar *text;

  g_assert (GB_IS_GREETER_WINDOW (self));

  g_clear_pointer (&self->pattern_spec, ide_pattern_spec_unref);
  if ((text = gtk_entry_get_text (GTK_ENTRY (self->search_entry))))
    self->pattern_spec = ide_pattern_spec_new (text);

  gb_greeter_window_apply_filter (self,
                                  self->my_projects_list_box,
                                  GTK_WIDGET (self->my_projects_container));
  gb_greeter_window_apply_filter (self,
                                  self->other_projects_list_box,
                                  GTK_WIDGET (self->other_projects_container));
}

static void
gb_greeter_window__search_entry_changed (GbGreeterWindow *self,
                                         GtkSearchEntry  *search_entry)
{
  g_assert (GB_IS_GREETER_WINDOW (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search_entry));

  gb_greeter_window_apply_filter_all (self);
}

static gboolean
row_focus_in_event (GbGreeterWindow     *self,
                    GdkEventFocus       *focus,
                    GbGreeterProjectRow *row)
{
  GtkAllocation alloc;
  GtkAllocation row_alloc;
  gint dest_x;
  gint dest_y;

  g_assert (GB_IS_GREETER_WINDOW (self));

  gtk_widget_get_allocation (GTK_WIDGET (self->viewport), &alloc);
  gtk_widget_get_allocation (GTK_WIDGET (row), &row_alloc);

  if (gtk_widget_translate_coordinates (GTK_WIDGET (row), GTK_WIDGET (self->viewport), 0, 0, &dest_x, &dest_y))
    {
      gint distance = 0;

      if (dest_y < 0)
        {
          distance = dest_y;
        }
      else if ((dest_y + row_alloc.height) > alloc.height)
        {
          distance = dest_y + row_alloc.height - alloc.height;
        }

      if (distance != 0)
        {
          GtkAdjustment *vadj;
          gdouble value;

          vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->viewport));
          value = gtk_adjustment_get_value (vadj);
          gtk_adjustment_set_value (vadj, value + distance);
        }
    }

  return GDK_EVENT_PROPAGATE;
}

static gboolean
selection_to_true (GBinding     *binding,
                   const GValue *from_value,
                   GValue       *to_value,
                   gpointer      user_data)
{
  if (G_VALUE_HOLDS_STRING (from_value) && G_VALUE_HOLDS_BOOLEAN (to_value))
    {
      const gchar *str;

      str = g_value_get_string (from_value);
      g_value_set_boolean (to_value, ide_str_equal0 (str, "selection"));

      return TRUE;
    }

  return FALSE;
}

static void
gb_greeter_window__row_notify_selected (GbGreeterWindow     *self,
                                        GParamSpec          *pspec,
                                        GbGreeterProjectRow *row)
{
  gboolean selected = FALSE;
  GAction *action;

  g_assert (GB_IS_GREETER_WINDOW (self));
  g_assert (pspec != NULL);
  g_assert (GB_IS_GREETER_PROJECT_ROW (row));

  g_object_get (row, "selected", &selected, NULL);
  self->selected_count += selected ? 1 : -1;

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "delete-selected-rows");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), (self->selected_count > 0));
}

static void
gb_greeter_window__recent_projects_items_changed (GbGreeterWindow *self,
                                                  guint            position,
                                                  guint            removed,
                                                  guint            added,
                                                  GListModel      *list_model)
{
  GbGreeterProjectRow *row;
  gsize i;

  /*
   * TODO: We ignore removed out of simplicity for now.
   *       But IdeRecentProjects doesn't currently remove anything anyway.
   */

  g_assert (GB_IS_GREETER_WINDOW (self));
  g_assert (G_IS_LIST_MODEL (list_model));
  g_assert (IDE_IS_RECENT_PROJECTS (list_model));

  for (i = 0; i < added; i++)
    {
      IdeProjectInfo *project_info;
      GtkListBox *list_box;

      project_info = g_list_model_get_item (list_model, position + i);

      row = g_object_new (GB_TYPE_GREETER_PROJECT_ROW,
                          "visible", TRUE,
                          "project-info", project_info,
                          NULL);
      g_signal_connect_object (row,
                               "focus-in-event",
                               G_CALLBACK (row_focus_in_event),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (row,
                               "notify::selected",
                               G_CALLBACK (gb_greeter_window__row_notify_selected),
                               self,
                               G_CONNECT_SWAPPED);

      if (ide_project_info_get_is_recent (project_info))
        {
          list_box = self->my_projects_list_box;
          g_object_bind_property_full (self->state_machine, "state",
                                       row, "selection-mode",
                                       G_BINDING_SYNC_CREATE,
                                       selection_to_true, NULL,
                                       NULL, NULL);
        }
      else
        {
          list_box = self->other_projects_list_box;
        }

      gtk_container_add (GTK_CONTAINER (list_box), GTK_WIDGET (row));
    }

  gb_greeter_window_apply_filter_all (self);
}

static gint
gb_greeter_window_sort_rows (GtkListBoxRow *row1,
                             GtkListBoxRow *row2,
                             gpointer       user_data)
{
  IdeProjectInfo *info1;
  IdeProjectInfo *info2;

  info1 = gb_greeter_project_row_get_project_info (GB_GREETER_PROJECT_ROW (row1));
  info2 = gb_greeter_project_row_get_project_info (GB_GREETER_PROJECT_ROW (row2));

  return ide_project_info_compare (info1, info2);
}

void
gb_greeter_window_set_recent_projects (GbGreeterWindow   *self,
                                       IdeRecentProjects *recent_projects)
{
  g_return_if_fail (GB_IS_GREETER_WINDOW (self));
  g_return_if_fail (!recent_projects || IDE_IS_RECENT_PROJECTS (recent_projects));

  if (g_set_object (&self->recent_projects, recent_projects))
    {
      egg_signal_group_set_target (self->signal_group, recent_projects);

      if (recent_projects != NULL)
        {
          GListModel *list_model;
          guint n_items;

          list_model = G_LIST_MODEL (recent_projects);
          n_items = g_list_model_get_n_items (list_model);
          gb_greeter_window__recent_projects_items_changed (self, 0, 0, n_items, list_model);
        }

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_RECENT_PROJECTS]);
    }
}

static gboolean
gb_greeter_window_filter_row (GtkListBoxRow *row,
                              gpointer       user_data)
{
  GbGreeterWindow *self = user_data;
  GbGreeterProjectRow *project_row = (GbGreeterProjectRow *)row;
  const gchar *search_text;
  gboolean ret;

  g_assert (GB_IS_GREETER_WINDOW (self));
  g_assert (GB_IS_GREETER_PROJECT_ROW (project_row));

  if (self->pattern_spec == NULL)
    return TRUE;

  search_text = gb_greeter_project_row_get_search_text (project_row);
  ret = ide_pattern_spec_match (self->pattern_spec, search_text);

  return ret;
}

static void
gb_greeter_window__open_project_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  GbApplication *app = (GbApplication *)object;
  g_autoptr(GbGreeterWindow) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GB_IS_APPLICATION (app));

  if (!gb_application_open_project_finish (app, result, &error))
    {
      g_warning ("%s\n", error->message);
    }

  gtk_widget_destroy (GTK_WIDGET (self));
}

static void
gb_greeter_window__row_activated (GbGreeterWindow     *self,
                                  GbGreeterProjectRow *row,
                                  GtkListBox          *list_box)
{
  IdeProjectInfo *project_info;
  GApplication *app;
  GFile *project_file;

  g_assert (GB_IS_GREETER_WINDOW (self));
  g_assert (GB_IS_GREETER_PROJECT_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if (ide_str_equal0 (egg_state_machine_get_state (self->state_machine), "selection"))
    {
      gboolean selected = FALSE;

      g_object_get (row, "selected", &selected, NULL);
      g_object_set (row, "selected", !selected, NULL);
      return;
    }

  app = g_application_get_default ();
  project_info = gb_greeter_project_row_get_project_info (row);
  project_file = ide_project_info_get_file (project_info);

  gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);

  gb_application_open_project_async (GB_APPLICATION (app),
                                     project_file,
                                     NULL,
                                     NULL,
                                     gb_greeter_window__open_project_cb,
                                     g_object_ref (self));

  ide_project_info_set_is_recent (project_info, TRUE);
}

static gboolean
gb_greeter_window__keynav_failed (GbGreeterWindow  *self,
                                  GtkDirectionType  dir,
                                  GtkListBox       *list_box)
{
  g_assert (GB_IS_GREETER_WINDOW (self));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if ((list_box == self->my_projects_list_box) && (dir == GTK_DIR_DOWN))
    {
      gtk_widget_child_focus (GTK_WIDGET (self->other_projects_list_box), GTK_DIR_DOWN);
      return GDK_EVENT_STOP;
    }
  else if ((list_box == self->other_projects_list_box) && (dir == GTK_DIR_UP))
    {
      gtk_widget_child_focus (GTK_WIDGET (self->my_projects_list_box), GTK_DIR_UP);
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
delete_selected_rows (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  GbGreeterWindow *self = user_data;
  GList *rows;
  GList *iter;
  GList *projects = NULL;

  g_assert (GB_IS_GREETER_WINDOW (self));
  g_assert (G_IS_SIMPLE_ACTION (action));

  rows = gtk_container_get_children (GTK_CONTAINER (self->my_projects_list_box));

  for (iter = rows; iter; iter = iter->next)
    {
      GbGreeterProjectRow *row = iter->data;
      gboolean selected = FALSE;

      g_object_get (row, "selected", &selected, NULL);

      if (selected)
        {
          IdeProjectInfo *info;

          info = gb_greeter_project_row_get_project_info (row);
          projects = g_list_prepend (projects, g_object_ref (info));
          gtk_container_remove (GTK_CONTAINER (self->my_projects_list_box), iter->data);
        }
    }

  g_list_free (rows);

  ide_recent_projects_remove (self->recent_projects, projects);
  g_list_free_full (projects, g_object_unref);

  self->selected_count = 0;
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

  egg_state_machine_set_state (self->state_machine, "browse");

  gb_greeter_window_apply_filter_all (self);
}

static void
gb_greeter_window_constructed (GObject *object)
{
  GbGreeterWindow *self = (GbGreeterWindow *)object;

  G_OBJECT_CLASS (gb_greeter_window_parent_class)->constructed (object);

  gb_settings_init_window (GTK_WINDOW (self));
}

static void
gb_greeter_window_finalize (GObject *object)
{
  GbGreeterWindow *self = (GbGreeterWindow *)object;

  g_clear_pointer (&self->pattern_spec, ide_pattern_spec_unref);
  g_clear_object (&self->signal_group);
  g_clear_object (&self->recent_projects);

  G_OBJECT_CLASS (gb_greeter_window_parent_class)->finalize (object);
}

static void
gb_greeter_window_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GbGreeterWindow *self = GB_GREETER_WINDOW (object);

  switch (prop_id)
    {
    case PROP_RECENT_PROJECTS:
      g_value_set_object (value, gb_greeter_window_get_recent_projects (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_greeter_window_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbGreeterWindow *self = GB_GREETER_WINDOW (object);

  switch (prop_id)
    {
    case PROP_RECENT_PROJECTS:
      gb_greeter_window_set_recent_projects (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_greeter_window_class_init (GbGreeterWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_greeter_window_constructed;
  object_class->finalize = gb_greeter_window_finalize;
  object_class->get_property = gb_greeter_window_get_property;
  object_class->set_property = gb_greeter_window_set_property;

  gParamSpecs [PROP_RECENT_PROJECTS] =
    g_param_spec_object ("recent-projects",
                         _("Recent Projects"),
                         _("The recent projects that have been mined."),
                         IDE_TYPE_RECENT_PROJECTS,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/gb-greeter-window.ui");
  gtk_widget_class_bind_template_child (widget_class, GbGreeterWindow, header_bar);
  gtk_widget_class_bind_template_child (widget_class, GbGreeterWindow, my_projects_container);
  gtk_widget_class_bind_template_child (widget_class, GbGreeterWindow, my_projects_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbGreeterWindow, other_projects_container);
  gtk_widget_class_bind_template_child (widget_class, GbGreeterWindow, other_projects_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbGreeterWindow, remove_button);
  gtk_widget_class_bind_template_child (widget_class, GbGreeterWindow, search_entry);
  gtk_widget_class_bind_template_child (widget_class, GbGreeterWindow, state_machine);
  gtk_widget_class_bind_template_child (widget_class, GbGreeterWindow, viewport);

  g_type_ensure (EGG_TYPE_SEARCH_BAR);
  g_type_ensure (EGG_TYPE_STATE_MACHINE);
  g_type_ensure (GB_TYPE_GREETER_PROJECT_ROW);
  g_type_ensure (GB_TYPE_SCROLLED_WINDOW);
}

static void
gb_greeter_window_init (GbGreeterWindow *self)
{
  GActionEntry actions[] = {
    { "delete-selected-rows", delete_selected_rows },
  };
  GAction *action;

  self->signal_group = egg_signal_group_new (IDE_TYPE_RECENT_PROJECTS);
  egg_signal_group_connect_object (self->signal_group,
                                   "items-changed",
                                   G_CALLBACK (gb_greeter_window__recent_projects_items_changed),
                                   self,
                                   G_CONNECT_SWAPPED);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->search_entry,
                           "activate",
                           G_CALLBACK (gb_greeter_window__search_entry_activate),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->search_entry,
                           "changed",
                           G_CALLBACK (gb_greeter_window__search_entry_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->my_projects_list_box,
                           "row-activated",
                           G_CALLBACK (gb_greeter_window__row_activated),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->my_projects_list_box,
                           "keynav-failed",
                           G_CALLBACK (gb_greeter_window__keynav_failed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->other_projects_list_box,
                           "row-activated",
                           G_CALLBACK (gb_greeter_window__row_activated),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->other_projects_list_box,
                           "keynav-failed",
                           G_CALLBACK (gb_greeter_window__keynav_failed),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_list_box_set_sort_func (self->my_projects_list_box,
                              gb_greeter_window_sort_rows,
                              NULL, NULL);
  gtk_list_box_set_sort_func (self->other_projects_list_box,
                              gb_greeter_window_sort_rows,
                              NULL, NULL);

  gtk_list_box_set_filter_func (self->my_projects_list_box,
                                gb_greeter_window_filter_row,
                                self, NULL);
  gtk_list_box_set_filter_func (self->other_projects_list_box,
                                gb_greeter_window_filter_row,
                                self, NULL);

  action = egg_state_machine_create_action (self->state_machine, "state");
  g_action_map_add_action (G_ACTION_MAP (self), action);
  g_object_unref (action);

  g_action_map_add_action_entries (G_ACTION_MAP (self), actions, G_N_ELEMENTS (actions), self);

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "delete-selected-rows");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
}
