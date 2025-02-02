/* gb-terminal-workbench-addin.c
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

#include <glib/gi18n.h>

#include "gb-terminal-view.h"
#include "gb-terminal-document.h"
#include "gb-terminal-workbench-addin.h"
#include "gb-view-grid.h"
#include "gb-workspace.h"

struct _GbTerminalWorkbenchAddin
{
  GObject         parent_instance;

  GbWorkbench    *workbench;
  GbTerminalView *panel_terminal;
};

static void workbench_addin_iface_init (GbWorkbenchAddinInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (GbTerminalWorkbenchAddin,
                                gb_terminal_workbench_addin,
                                G_TYPE_OBJECT,
                                0,
                                G_IMPLEMENT_INTERFACE (GB_TYPE_WORKBENCH_ADDIN,
                                                       workbench_addin_iface_init))

enum {
  PROP_0,
  PROP_WORKBENCH,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
new_terminal_activate_cb (GSimpleAction   *action,
                          GVariant        *param,
                          GbTerminalWorkbenchAddin *self)
{
  GbTerminalDocument *document;
  GbViewGrid *view_grid;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GB_IS_TERMINAL_WORKBENCH_ADDIN (self));

  view_grid = GB_VIEW_GRID (gb_workbench_get_view_grid (self->workbench));

  document = g_object_new (GB_TYPE_TERMINAL_DOCUMENT, NULL);
  gb_view_grid_focus_document (view_grid, GB_DOCUMENT (document));
}

static void
gb_terminal_workbench_addin_load (GbWorkbenchAddin *addin)
{
  GbTerminalWorkbenchAddin *self = (GbTerminalWorkbenchAddin *)addin;
  GbWorkspace *workspace;
  GtkWidget *bottom_pane;
  g_autoptr(GSimpleAction) action = NULL;

  g_assert (GB_IS_TERMINAL_WORKBENCH_ADDIN (self));
  g_assert (GB_IS_WORKBENCH (self->workbench));

  action = g_simple_action_new ("new-terminal", NULL);
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (new_terminal_activate_cb),
                           self,
                           0);
  g_action_map_add_action (G_ACTION_MAP (self->workbench), G_ACTION (action));

  if (self->panel_terminal == NULL)
    {
      self->panel_terminal = g_object_new (GB_TYPE_TERMINAL_VIEW,
                                           "visible", TRUE,
                                           NULL);
      g_object_add_weak_pointer (G_OBJECT (self->panel_terminal),
                                 (gpointer *)&self->panel_terminal);
    }

  workspace = GB_WORKSPACE (gb_workbench_get_workspace (self->workbench));
  bottom_pane = gb_workspace_get_bottom_pane (workspace);
  gb_workspace_pane_add_page (GB_WORKSPACE_PANE (bottom_pane),
                              GTK_WIDGET (self->panel_terminal),
                              _("Terminal"),
                              "utilities-terminal-symbolic");
}

static void
gb_terminal_workbench_addin_unload (GbWorkbenchAddin *addin)
{
  GbTerminalWorkbenchAddin *self = (GbTerminalWorkbenchAddin *)addin;

  g_assert (GB_IS_TERMINAL_WORKBENCH_ADDIN (self));

  g_action_map_remove_action (G_ACTION_MAP (self->workbench), "new-terminal");

  if (self->panel_terminal != NULL)
    {
      GtkWidget *parent;

      parent = gtk_widget_get_parent (GTK_WIDGET (self->panel_terminal));
      gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (self->panel_terminal));
    }
}

static void
gb_terminal_workbench_addin_finalize (GObject *object)
{
  GbTerminalWorkbenchAddin *self = (GbTerminalWorkbenchAddin *)object;

  ide_clear_weak_pointer (&self->workbench);

  G_OBJECT_CLASS (gb_terminal_workbench_addin_parent_class)->finalize (object);
}

static void
gb_terminal_workbench_addin_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbTerminalWorkbenchAddin *self = GB_TERMINAL_WORKBENCH_ADDIN (object);

  switch (prop_id)
    {
    case PROP_WORKBENCH:
      ide_set_weak_pointer (&self->workbench, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_terminal_workbench_addin_class_init (GbTerminalWorkbenchAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_terminal_workbench_addin_finalize;
  object_class->set_property = gb_terminal_workbench_addin_set_property;

  gParamSpecs [PROP_WORKBENCH] =
    g_param_spec_object ("workbench",
                         _("Workbench"),
                         _("The workbench window."),
                         GB_TYPE_WORKBENCH,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_terminal_workbench_addin_class_finalize (GbTerminalWorkbenchAddinClass *klass)
{
}

static void
gb_terminal_workbench_addin_init (GbTerminalWorkbenchAddin *self)
{
}

static void
workbench_addin_iface_init (GbWorkbenchAddinInterface *iface)
{
  iface->load = gb_terminal_workbench_addin_load;
  iface->unload = gb_terminal_workbench_addin_unload;
}

void
_gb_terminal_workbench_addin_register_type (GTypeModule *module)
{
  gb_terminal_workbench_addin_register_type (module);
}
