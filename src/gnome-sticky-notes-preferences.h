/* MIT License
 *
 * Copyright (c) 2026 omer
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define GNOME_STICKY_NOTES_TYPE_PREFERENCES (gnome_sticky_notes_preferences_get_type())

G_DECLARE_FINAL_TYPE (GnomeStickyNotesPreferences, gnome_sticky_notes_preferences, GNOME_STICKY_NOTES, PREFERENCES, AdwPreferencesDialog)

GnomeStickyNotesPreferences *gnome_sticky_notes_preferences_new (void);

G_END_DECLS
