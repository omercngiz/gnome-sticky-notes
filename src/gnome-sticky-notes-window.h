/* MIT License
 *
 * Copyright (c) 2026 omer
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <adwaita.h>

#include "gnome-sticky-notes-database.h"

G_BEGIN_DECLS

#define GNOME_STICKY_NOTES_TYPE_WINDOW (gnome_sticky_notes_window_get_type())

G_DECLARE_FINAL_TYPE (GnomeStickyNotesWindow, gnome_sticky_notes_window, GNOME_STICKY_NOTES, WINDOW, AdwApplicationWindow)

GnomeStickyNotesWindow *gnome_sticky_notes_window_new (GtkApplication                 *application,
                                                       GnomeStickyNotesDatabase       *database,
                                                       const GnomeStickyNotesNoteData *data);

gint64                  gnome_sticky_notes_window_get_note_id (GnomeStickyNotesWindow *self);

G_END_DECLS
