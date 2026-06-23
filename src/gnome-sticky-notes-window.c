/* MIT License
 *
 * Copyright (c) 2026 omer
 *
 * SPDX-License-Identifier: MIT
 */

#include "config.h"

#include "gnome-sticky-notes-window.h"

/* Debounce delay (ms) for autosaving note text while typing. */
#define AUTOSAVE_DELAY_MS 800

struct _GnomeStickyNotesWindow
{
	AdwApplicationWindow      parent_instance;

	GnomeStickyNotesDatabase *database;   /* owned */
	gint64                    note_id;
	char                     *color;      /* pass-through until theming lands */
	char                     *monitor;    /* pass-through until positioning lands */
	guint                     save_timeout_id;
	int                       cur_width;   /* latest real allocation */
	int                       cur_height;

	/* Template widgets */
	GtkTextView              *text_view;
};

G_DEFINE_FINAL_TYPE (GnomeStickyNotesWindow, gnome_sticky_notes_window, ADW_TYPE_APPLICATION_WINDOW)

/* Reads the live note state into a stack record (no allocation of the record
 * itself; caller frees the returned content string). Position is left unset
 * (-1) because GTK4 exposes no client window position on Wayland. */
static void
gnome_sticky_notes_window_collect (GnomeStickyNotesWindow   *self,
                                   GnomeStickyNotesNoteData *out,
                                   char                    **out_content)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer (self->text_view);
	GtkTextIter start, end;
	int width = self->cur_width;
	int height = self->cur_height;

	gtk_text_buffer_get_bounds (buffer, &start, &end);
	*out_content = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	/* Fall back to the live allocation if size_allocate hasn't run yet. */
	if (width <= 0)
		width = gtk_widget_get_width (GTK_WIDGET (self));
	if (height <= 0)
		height = gtk_widget_get_height (GTK_WIDGET (self));

	out->id      = self->note_id;
	out->content = *out_content;
	out->color   = self->color;
	out->pos_x   = -1;
	out->pos_y   = -1;
	out->width   = width;
	out->height  = height;
	out->monitor = self->monitor;
}

static void
gnome_sticky_notes_window_save_now (GnomeStickyNotesWindow *self)
{
	GnomeStickyNotesNoteData data = { 0 };
	g_autofree char *content = NULL;
	g_autoptr(GError) error = NULL;

	if (self->database == NULL || self->note_id <= 0)
		return;

	gnome_sticky_notes_window_collect (self, &data, &content);

	if (!gnome_sticky_notes_database_save_note (self->database, &data, &error))
		g_warning ("Failed to save note %" G_GINT64_FORMAT ": %s",
		           self->note_id, error->message);
}

static gboolean
autosave_timeout_cb (gpointer user_data)
{
	GnomeStickyNotesWindow *self = user_data;

	self->save_timeout_id = 0;
	gnome_sticky_notes_window_save_now (self);

	return G_SOURCE_REMOVE;
}

/* Coalesce rapid edits/resizes into a single delayed save. */
static void
gnome_sticky_notes_window_schedule_save (GnomeStickyNotesWindow *self)
{
	g_clear_handle_id (&self->save_timeout_id, g_source_remove);
	self->save_timeout_id = g_timeout_add (AUTOSAVE_DELAY_MS,
	                                        autosave_timeout_cb,
	                                        self);
}

static void
on_buffer_changed (GtkTextBuffer          *buffer,
                   GnomeStickyNotesWindow *self)
{
	gnome_sticky_notes_window_schedule_save (self);
}

/* Track the real toplevel size as the user resizes, and persist it. Reading
 * the allocation here is reliable, unlike gtk_window_get_default_size(). */
static void
gnome_sticky_notes_window_size_allocate (GtkWidget *widget,
                                         int        width,
                                         int        height,
                                         int        baseline)
{
	GnomeStickyNotesWindow *self = GNOME_STICKY_NOTES_WINDOW (widget);

	GTK_WIDGET_CLASS (gnome_sticky_notes_window_parent_class)->size_allocate (widget, width, height, baseline);

	if (width <= 0 || height <= 0)
		return;

	if (width != self->cur_width || height != self->cur_height)
		{
			self->cur_width = width;
			self->cur_height = height;
			gnome_sticky_notes_window_schedule_save (self);
		}
}

static gboolean
on_close_request (GtkWindow *window,
                  gpointer   user_data)
{
	GnomeStickyNotesWindow *self = GNOME_STICKY_NOTES_WINDOW (window);

	/* Flush any pending edit and persist final geometry before the window
	 * goes away. The note is kept in the database so it reopens next launch. */
	g_clear_handle_id (&self->save_timeout_id, g_source_remove);
	gnome_sticky_notes_window_save_now (self);

	return GDK_EVENT_PROPAGATE; /* allow the close to proceed */
}

static void
delete_action (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
	GnomeStickyNotesWindow *self = user_data;
	g_autoptr(GError) error = NULL;

	g_clear_handle_id (&self->save_timeout_id, g_source_remove);

	if (self->database != NULL && self->note_id > 0)
		{
			if (!gnome_sticky_notes_database_delete_note (self->database, self->note_id, &error))
				g_warning ("Failed to delete note %" G_GINT64_FORMAT ": %s",
				           self->note_id, error->message);
		}

	/* Avoid the close handler re-saving a row we just deleted. */
	self->note_id = -1;
	gtk_window_destroy (GTK_WINDOW (self));
}

static const GActionEntry win_actions[] = {
	{ "delete", delete_action },
};

static void
gnome_sticky_notes_window_dispose (GObject *object)
{
	GnomeStickyNotesWindow *self = GNOME_STICKY_NOTES_WINDOW (object);

	g_clear_handle_id (&self->save_timeout_id, g_source_remove);
	g_clear_object (&self->database);
	g_clear_pointer (&self->color, g_free);
	g_clear_pointer (&self->monitor, g_free);

	G_OBJECT_CLASS (gnome_sticky_notes_window_parent_class)->dispose (object);
}

static void
gnome_sticky_notes_window_class_init (GnomeStickyNotesWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = gnome_sticky_notes_window_dispose;
	widget_class->size_allocate = gnome_sticky_notes_window_size_allocate;

	gtk_widget_class_set_template_from_resource (widget_class, "/io/omercngiz/GnomeStickyNotes/gnome-sticky-notes-window.ui");
	gtk_widget_class_bind_template_child (widget_class, GnomeStickyNotesWindow, text_view);
}

static void
gnome_sticky_notes_window_init (GnomeStickyNotesWindow *self)
{
	self->note_id = -1;

	gtk_widget_init_template (GTK_WIDGET (self));

	g_action_map_add_action_entries (G_ACTION_MAP (self),
	                                 win_actions,
	                                 G_N_ELEMENTS (win_actions),
	                                 self);

	g_signal_connect (self, "close-request",
	                  G_CALLBACK (on_close_request), NULL);
	g_signal_connect (gtk_text_view_get_buffer (self->text_view), "changed",
	                  G_CALLBACK (on_buffer_changed), self);
}

GnomeStickyNotesWindow *
gnome_sticky_notes_window_new (GtkApplication                 *application,
                               GnomeStickyNotesDatabase       *database,
                               const GnomeStickyNotesNoteData *data)
{
	GnomeStickyNotesWindow *self;
	GtkTextBuffer *buffer;

	g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);
	g_return_val_if_fail (GNOME_STICKY_NOTES_IS_DATABASE (database), NULL);
	g_return_val_if_fail (data != NULL, NULL);

	self = g_object_new (GNOME_STICKY_NOTES_TYPE_WINDOW,
	                     "application", application,
	                     NULL);

	self->database = g_object_ref (database);
	self->note_id  = data->id;
	self->color    = g_strdup (data->color);
	self->monitor  = g_strdup (data->monitor);

	/* Restore size. Position is intentionally not applied: on Wayland a
	 * client cannot place its own toplevel; the value is persisted for a
	 * future X11 backend. */
	gtk_window_set_default_size (GTK_WINDOW (self),
	                             data->width  > 0 ? data->width  : 280,
	                             data->height > 0 ? data->height : 320);

	/* Load content without triggering an immediate autosave. */
	buffer = gtk_text_view_get_buffer (self->text_view);
	g_signal_handlers_block_by_func (buffer, on_buffer_changed, self);
	gtk_text_buffer_set_text (buffer, data->content ? data->content : "", -1);
	g_signal_handlers_unblock_by_func (buffer, on_buffer_changed, self);

	return self;
}

gint64
gnome_sticky_notes_window_get_note_id (GnomeStickyNotesWindow *self)
{
	g_return_val_if_fail (GNOME_STICKY_NOTES_IS_WINDOW (self), -1);

	return self->note_id;
}
