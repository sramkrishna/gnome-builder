/* gb-terminal-view-private.h
 *
 * Copyright (C) 2015 Sebastien Lafargue <slafargue@gnome.org>
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

#ifndef GB_TERMINAL_VIEW_PRIVATE_H
#define GB_TERMINAL_VIEW_PRIVATE_H

#include <vte/vte.h>

#include "gb-terminal-document.h"
#include "gb-view.h"

G_BEGIN_DECLS

struct _GbTerminalView
{
  GbView               parent_instance;

  GbTerminalDocument  *document;

  VteTerminal         *terminal_top;
  VteTerminal         *terminal_bottom;

  GFile               *save_as_file_top;
  GFile               *save_as_file_bottom;

  gchar               *selection_buffer;

  GtkWidget           *scrolled_window_bottom;

  guint                top_has_spawned : 1;
  guint                bottom_has_spawned : 1;
  guint                bottom_has_focus : 1;

  guint                top_has_needs_attention : 1;
  guint                bottom_has_needs_attention : 1;
};

G_END_DECLS

#endif /* GB_TERMINAL_VIEW_PRIVATE_H */
