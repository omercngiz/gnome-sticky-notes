/* MIT License
 *
 * Copyright (c) 2026 omer
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

/*
 * Plain data record for a single note as stored in the database.
 * Returned by gnome_sticky_notes_database_load_all() inside a GPtrArray
 * whose free-func is gnome_sticky_notes_note_data_free.
 */
typedef struct
{
	gint64  id;
	char   *content;   /* note body (plain text for now) */
	char   *color;     /* color/theme key, may be NULL */
	int     pos_x;     /* stored for the future; -1 = unset */
	int     pos_y;     /* stored for the future; -1 = unset */
	int     width;
	int     height;
	char   *monitor;   /* monitor connector name, may be NULL */
} GnomeStickyNotesNoteData;

void gnome_sticky_notes_note_data_free (GnomeStickyNotesNoteData *data);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GnomeStickyNotesNoteData, gnome_sticky_notes_note_data_free)

#define GNOME_STICKY_NOTES_TYPE_DATABASE (gnome_sticky_notes_database_get_type())

G_DECLARE_FINAL_TYPE (GnomeStickyNotesDatabase, gnome_sticky_notes_database, GNOME_STICKY_NOTES, DATABASE, GObject)

GnomeStickyNotesDatabase *gnome_sticky_notes_database_new        (GError                    **error);

GPtrArray                *gnome_sticky_notes_database_load_all   (GnomeStickyNotesDatabase   *self,
                                                                  GError                    **error);

gint64                    gnome_sticky_notes_database_create_note (GnomeStickyNotesDatabase  *self,
                                                                   GError                   **error);

gboolean                  gnome_sticky_notes_database_save_note   (GnomeStickyNotesDatabase        *self,
                                                                   const GnomeStickyNotesNoteData  *data,
                                                                   GError                         **error);

gboolean                  gnome_sticky_notes_database_delete_note (GnomeStickyNotesDatabase  *self,
                                                                   gint64                     id,
                                                                   GError                   **error);

G_END_DECLS
