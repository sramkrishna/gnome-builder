/* test-ide-indenter.c
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

#include <libpeas/peas.h>
#include <girepository.h>
#include <ide.h>
#include <string.h>

typedef void (*IndentTestFunc) (IdeContext *context,
                                GtkWidget  *widget);

typedef struct
{
  IndentTestFunc  func;
  gchar          *path;
} IndentTest;

static void
load_plugins (void)
{
  PeasEngine *engine;
  const GList *list;
  GDir *dir;

  engine = peas_engine_get_default ();

  if ((dir = g_dir_open ("../plugins", 0, NULL)))
    {
      const gchar *name;

      while ((name = g_dir_read_name (dir)))
        {
          gchar *path;

          path = g_build_filename ("..", "plugins", name, NULL);
          peas_engine_prepend_search_path (engine, path, path);
          g_free (path);
        }

      g_dir_close (dir);
    }

  peas_engine_rescan_plugins (engine);
  list = peas_engine_get_plugin_list (engine);

  for (; list; list = list->next)
    {
      PeasPluginInfo *info = list->data;

      if (g_strcmp0 (peas_plugin_info_get_module_name (info), "c-pack-plugin") == 0)
        {
          if (!peas_plugin_info_is_loaded (info))
            peas_engine_load_plugin (engine, info);
        }
    }
}

static void
new_context_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  IndentTest *test = user_data;
  GtkWidget *window;
  GtkWidget *widget;
  IdeBuffer *buffer;
  GtkSourceCompletion *completion;
  IdeContext *context;
  IdeProject *project;
  IdeFile *file;
  GError *error = NULL;

  g_assert (test != NULL);
  g_assert (test->func != NULL);
  g_assert (test->path != NULL);

  context = ide_context_new_finish (result, &error);
  g_assert_no_error (error);
  g_assert (context != NULL);
  g_assert (IDE_IS_CONTEXT (context));

  project = ide_context_get_project (context);
  file = ide_project_get_file_for_path (project, test->path);

  buffer = g_object_new (IDE_TYPE_BUFFER,
                         "context", context,
                         "file", file,
                         NULL);

  window = gtk_offscreen_window_new ();
  widget = g_object_new (IDE_TYPE_SOURCE_VIEW,
                         "auto-indent", TRUE,
                         "buffer", buffer,
                         "visible", TRUE,
                         NULL);
  gtk_container_add (GTK_CONTAINER (window), widget);

  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (widget));
  gtk_source_completion_block_interactive (completion);

  gtk_window_present (GTK_WINDOW (window));

  while (gtk_events_pending ())
    gtk_main_iteration ();

  test->func (context, widget);

#if 0
  ide_context_unload_async (context,
                            NULL,
                            (GAsyncReadyCallback)gtk_main_quit,
                            NULL);
#else
  gtk_main_quit ();
#endif

  g_object_unref (buffer);
  g_object_unref (file);
  g_free (test->path);
  g_free (test);
}

static void
run_test (const gchar    *path,
          IndentTestFunc  func)
{
  g_autoptr(GFile) project_file = NULL;
  IndentTest *test;

  test = g_new0 (IndentTest, 1);
  test->path = g_strdup (path);
  test->func = func;

  project_file = g_file_new_for_path (TEST_DATA_DIR"/project1/configure.ac");
  ide_context_new_async (project_file,
                         NULL,
                         new_context_cb,
                         test);

  gtk_main ();
}

static GdkEventKey *
synthesize_event (GtkTextView *text_view,
                  gunichar     ch)
{
  GdkDisplay *display;
  GdkDeviceManager *device_manager;
  GdkDevice *client_pointer;
  GdkWindow *window;
  GdkEvent *ev;
  GdkKeymapKey *keys = NULL;
  gint n_keys = 0;
  gchar str[8] = { 0 };

  window = gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT);
  g_assert (window != NULL);
  g_assert (GDK_IS_WINDOW (window));

  g_unichar_to_utf8 (ch, str);

  ev = gdk_event_new (GDK_KEY_PRESS);
  ev->key.window = g_object_ref (window);
  ev->key.send_event = TRUE;
  ev->key.time = gtk_get_current_event_time ();
  ev->key.state = 0;
  ev->key.hardware_keycode = 0;
  ev->key.group = 0;
  ev->key.is_modifier = 0;

  switch (ch)
    {
    case '\n':
      ev->key.keyval = GDK_KEY_Return;
      ev->key.string = g_strdup ("\n");
      ev->key.length = 1;
      break;

    case '\e':
      ev->key.keyval = GDK_KEY_Escape;
      ev->key.string = g_strdup ("");
      ev->key.length = 0;
      break;

    default:
      ev->key.keyval = gdk_unicode_to_keyval (ch);
      ev->key.length = strlen (str);
      ev->key.string = g_strdup (str);
      break;
    }

  gdk_keymap_get_entries_for_keyval (gdk_keymap_get_default (),
                                     ev->key.keyval,
                                     &keys,
                                     &n_keys);

  if (n_keys > 0)
    {
      ev->key.hardware_keycode = keys [0].keycode;
      ev->key.group = keys [0].group;
      if (keys [0].level == 1)
        ev->key.state |= GDK_SHIFT_MASK;
      g_free (keys);
    }

  display = gdk_window_get_display (ev->any.window);
  device_manager = gdk_display_get_device_manager (display);
  client_pointer = gdk_device_manager_get_client_pointer (device_manager);
  gdk_event_set_device (ev, gdk_device_get_associated_device (client_pointer));

  return &ev->key;
}

/*
 * Converts the input_chars into GdkEventKeys and synthesizes them to
 * the widget. Then ensures that we get the proper string back out.
 */
static void
assert_keypress_equal (GtkWidget   *widget,
                       const gchar *input_chars,
                       const gchar *output_str)
{
  g_autofree gchar *result = NULL;
  GtkTextView *text_view = (GtkTextView *)widget;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (GTK_IS_TEXT_VIEW (widget));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  for (; *input_chars; input_chars = g_utf8_next_char (input_chars))
    {
      gunichar ch = g_utf8_get_char (input_chars);
      GdkEventKey *event;

      while (gtk_events_pending ())
        gtk_main_iteration ();

      event = synthesize_event (text_view, ch);
      gtk_main_do_event ((GdkEvent *)event);
      gdk_event_free ((GdkEvent *)event);
    }

  gtk_text_buffer_get_bounds (buffer, &begin, &end);
  result = gtk_text_buffer_get_text (buffer, &begin, &end, TRUE);

  g_assert_cmpstr (result, ==, output_str);

  gtk_text_buffer_set_text (buffer, "", 0);
}

static void
test_cindenter_basic_cb (IdeContext *context,
                         GtkWidget  *widget)
{
  g_object_set (widget,
                "insert-matching-brace", TRUE,
                "overwrite-braces", TRUE,
                NULL);

  assert_keypress_equal (widget, "  #include <glib.h>", "#include <glib.h>");
  assert_keypress_equal (widget, "\n  #include <glib.h>", "\n#include <glib.h>");
  assert_keypress_equal (widget, "if (abcd)\n{\n", "if (abcd)\n  {\n    \n  }");
  assert_keypress_equal (widget,
                         "static void\nfoo (GtkWidget *widget,\nGError **error)",
                         "static void\nfoo (GtkWidget  *widget,\n     GError    **error)");
}

static void
test_cindenter_basic (void)
{
  load_plugins ();

  run_test ("test.c", test_cindenter_basic_cb);
}

gint
main (gint argc,
      gchar *argv[])
{
  g_irepository_prepend_search_path (BUILDDIR"/../libide");
  gtk_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/CIndenter/basic", test_cindenter_basic);
  return g_test_run ();
}
