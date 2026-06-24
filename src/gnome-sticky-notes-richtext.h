/* MIT License
 *
 * Copyright (c) 2026 omer
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Character-level toggle attributes. Family, size, colour and paragraph
 * alignment are value attributes and have their own setters. */
typedef enum
{
	GNOME_STICKY_NOTES_RICH_TEXT_BOLD,
	GNOME_STICKY_NOTES_RICH_TEXT_ITALIC,
	GNOME_STICKY_NOTES_RICH_TEXT_UNDERLINE,
	GNOME_STICKY_NOTES_RICH_TEXT_STRIKETHROUGH,
} GnomeStickyNotesRichTextToggle;

#define GNOME_STICKY_NOTES_TYPE_RICH_TEXT (gnome_sticky_notes_rich_text_get_type ())

G_DECLARE_FINAL_TYPE (GnomeStickyNotesRichText, gnome_sticky_notes_rich_text, GNOME_STICKY_NOTES, RICH_TEXT, GObject)

/* Binds a rich-text engine to an existing text view. The engine installs the
 * tag table, watches the buffer for cursor moves and insertions, and emits
 * "state-changed" whenever the formatting that applies at the cursor changes
 * (so a toolbar can be kept in sync). */
GnomeStickyNotesRichText *gnome_sticky_notes_rich_text_new (GtkTextView *view);

/* Persistence. serialize() returns a self-describing string (with a format
 * marker) suitable for storing in the database; deserialize() accepts that
 * string and also tolerates legacy plain text. */
char *gnome_sticky_notes_rich_text_serialize   (GnomeStickyNotesRichText *self);
void  gnome_sticky_notes_rich_text_deserialize (GnomeStickyNotesRichText *self,
                                                 const char               *data);

/* Formatting commands. With a selection they act on the selection; with no
 * selection they update the "active" formatting used for the next typed text
 * (Word-like behaviour). */
void gnome_sticky_notes_rich_text_toggle        (GnomeStickyNotesRichText       *self,
                                                  GnomeStickyNotesRichTextToggle  which);
void gnome_sticky_notes_rich_text_set_family    (GnomeStickyNotesRichText       *self,
                                                  const char                     *family);
void gnome_sticky_notes_rich_text_set_size      (GnomeStickyNotesRichText       *self,
                                                  double                          points);
void gnome_sticky_notes_rich_text_set_color     (GnomeStickyNotesRichText       *self,
                                                  const GdkRGBA                  *rgba);
void gnome_sticky_notes_rich_text_set_alignment (GnomeStickyNotesRichText       *self,
                                                  GtkJustification                justification);

/* State getters reflecting the formatting at the cursor / selection. Valid to
 * call at any time, intended for use from a "state-changed" handler. */
gboolean         gnome_sticky_notes_rich_text_get_toggle    (GnomeStickyNotesRichText       *self,
                                                             GnomeStickyNotesRichTextToggle  which);
const char      *gnome_sticky_notes_rich_text_get_family    (GnomeStickyNotesRichText *self); /* NULL = default */
double           gnome_sticky_notes_rich_text_get_size      (GnomeStickyNotesRichText *self); /* 0 = default */
gboolean         gnome_sticky_notes_rich_text_get_color     (GnomeStickyNotesRichText *self,
                                                             GdkRGBA                  *out);  /* FALSE = default */
GtkJustification gnome_sticky_notes_rich_text_get_alignment (GnomeStickyNotesRichText *self);

G_END_DECLS
