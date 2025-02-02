/* gb-plugins.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "plugins"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libpeas/peas.h>

#include "gb-application.h"
#include "gb-document.h"
#include "gb-plugins.h"
#include "gb-tree-builder.h"
#include "gb-view.h"
#include "gb-workbench.h"
#include "gb-workspace.h"

void
gb_plugins_init (void)
{
  PeasEngine *engine;
  const GList *list;

  /*
   * Ensure plugin-extensible types are registered.
   * This allows libgnome-builder.la to be linked and not drop
   * important symbols.
   */
  g_type_ensure (GB_TYPE_APPLICATION);
  g_type_ensure (GB_TYPE_DOCUMENT);
  g_type_ensure (GB_TYPE_TREE_BUILDER);
  g_type_ensure (GB_TYPE_VIEW);
  g_type_ensure (GB_TYPE_WORKBENCH);
  g_type_ensure (GB_TYPE_WORKSPACE);

  engine = peas_engine_get_default ();

  peas_engine_enable_loader (engine, "python3");

  if (g_getenv ("GB_IN_TREE_PLUGINS") != NULL)
    {
      GDir *dir;

      if ((dir = g_dir_open ("plugins", 0, NULL)))
        {
          const gchar *name;

          while ((name = g_dir_read_name (dir)))
            {
              gchar *path;

              path = g_build_filename ("plugins", name, NULL);
              peas_engine_prepend_search_path (engine, path, path);
              g_free (path);
            }

          g_dir_close (dir);
        }
    }
  else
    {
      peas_engine_prepend_search_path (engine,
                                       PACKAGE_LIBDIR"/gnome-builder/plugins",
                                       PACKAGE_DATADIR"/gnome-builder/plugins");
    }

  list = peas_engine_get_plugin_list (engine);

  for (; list; list = list->next)
    {
      if (peas_plugin_info_is_builtin (list->data))
        peas_engine_load_plugin (engine, list->data);
    }
}

