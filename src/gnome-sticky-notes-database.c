/* MIT License
 *
 * Copyright (c) 2026 omer
 *
 * SPDX-License-Identifier: MIT
 */

#include "config.h"

#include "gnome-sticky-notes-database.h"

#include <errno.h>
#include <sqlite3.h>

/* Default geometry used for freshly created notes. */
#define DEFAULT_NOTE_WIDTH  280
#define DEFAULT_NOTE_HEIGHT 320

struct _GnomeStickyNotesDatabase
{
	GObject  parent_instance;

	sqlite3 *db;
};

G_DEFINE_FINAL_TYPE (GnomeStickyNotesDatabase, gnome_sticky_notes_database, G_TYPE_OBJECT)

/* Quark for reporting sqlite errors as GError. */
#define GNOME_STICKY_NOTES_DATABASE_ERROR (gnome_sticky_notes_database_error_quark ())
static GQuark
gnome_sticky_notes_database_error_quark (void)
{
	return g_quark_from_static_string ("gnome-sticky-notes-database-error");
}

/* Adapter so g_clear_pointer gets a clean void(*)(void*) signature. */
static void
close_sqlite (sqlite3 *db)
{
	sqlite3_close (db);
}

void
gnome_sticky_notes_note_data_free (GnomeStickyNotesNoteData *data)
{
	if (data == NULL)
		return;

	g_free (data->content);
	g_free (data->color);
	g_free (data->monitor);
	g_free (data);
}

static void
gnome_sticky_notes_database_finalize (GObject *object)
{
	GnomeStickyNotesDatabase *self = GNOME_STICKY_NOTES_DATABASE (object);

	g_clear_pointer (&self->db, close_sqlite);

	G_OBJECT_CLASS (gnome_sticky_notes_database_parent_class)->finalize (object);
}

static void
gnome_sticky_notes_database_class_init (GnomeStickyNotesDatabaseClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gnome_sticky_notes_database_finalize;
}

static void
gnome_sticky_notes_database_init (GnomeStickyNotesDatabase *self)
{
}

/* Builds the on-disk database path: $XDG_DATA_HOME/gnome-sticky-notes/notes.db
 * and ensures the parent directory exists. */
static char *
build_database_path (GError **error)
{
	g_autofree char *dir = g_build_filename (g_get_user_data_dir (),
	                                         "gnome-sticky-notes",
	                                         NULL);

	if (g_mkdir_with_parents (dir, 0755) != 0)
		{
			g_set_error (error,
			             G_FILE_ERROR,
			             g_file_error_from_errno (errno),
			             "Failed to create data directory “%s”: %s",
			             dir, g_strerror (errno));
			return NULL;
		}

	return g_build_filename (dir, "notes.db", NULL);
}

static gboolean
ensure_schema (GnomeStickyNotesDatabase  *self,
               GError                   **error)
{
	static const char *schema =
		"CREATE TABLE IF NOT EXISTS notes ("
		"  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  content    TEXT    NOT NULL DEFAULT '',"
		"  color      TEXT,"
		"  pos_x      INTEGER NOT NULL DEFAULT -1,"
		"  pos_y      INTEGER NOT NULL DEFAULT -1,"
		"  width      INTEGER NOT NULL DEFAULT 280,"
		"  height     INTEGER NOT NULL DEFAULT 320,"
		"  monitor    TEXT,"
		"  created_at INTEGER NOT NULL DEFAULT 0,"
		"  updated_at INTEGER NOT NULL DEFAULT 0"
		");";
	char *errmsg = NULL;

	if (sqlite3_exec (self->db, schema, NULL, NULL, &errmsg) != SQLITE_OK)
		{
			g_set_error (error,
			             GNOME_STICKY_NOTES_DATABASE_ERROR, 0,
			             "Failed to create schema: %s",
			             errmsg ? errmsg : "unknown error");
			sqlite3_free (errmsg);
			return FALSE;
		}

	return TRUE;
}

GnomeStickyNotesDatabase *
gnome_sticky_notes_database_new (GError **error)
{
	g_autoptr(GnomeStickyNotesDatabase) self = NULL;
	g_autofree char *path = NULL;

	path = build_database_path (error);
	if (path == NULL)
		return NULL;

	self = g_object_new (GNOME_STICKY_NOTES_TYPE_DATABASE, NULL);

	if (sqlite3_open (path, &self->db) != SQLITE_OK)
		{
			g_set_error (error,
			             GNOME_STICKY_NOTES_DATABASE_ERROR, 0,
			             "Failed to open database “%s”: %s",
			             path, sqlite3_errmsg (self->db));
			return NULL;
		}

	/* Pragmas: WAL keeps reads fast and writes durable for our use. */
	sqlite3_exec (self->db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
	sqlite3_exec (self->db, "PRAGMA foreign_keys=ON;", NULL, NULL, NULL);

	if (!ensure_schema (self, error))
		return NULL;

	return g_steal_pointer (&self);
}

GPtrArray *
gnome_sticky_notes_database_load_all (GnomeStickyNotesDatabase  *self,
                                      GError                   **error)
{
	static const char *sql =
		"SELECT id, content, color, pos_x, pos_y, width, height, monitor "
		"FROM notes ORDER BY id;";
	sqlite3_stmt *stmt = NULL;
	GPtrArray *notes;
	int rc;

	g_return_val_if_fail (GNOME_STICKY_NOTES_IS_DATABASE (self), NULL);

	if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
		{
			g_set_error (error,
			             GNOME_STICKY_NOTES_DATABASE_ERROR, 0,
			             "Failed to query notes: %s",
			             sqlite3_errmsg (self->db));
			return NULL;
		}

	notes = g_ptr_array_new_with_free_func ((GDestroyNotify) gnome_sticky_notes_note_data_free);

	while ((rc = sqlite3_step (stmt)) == SQLITE_ROW)
		{
			GnomeStickyNotesNoteData *data = g_new0 (GnomeStickyNotesNoteData, 1);
			const unsigned char *text;

			data->id      = sqlite3_column_int64 (stmt, 0);
			text          = sqlite3_column_text (stmt, 1);
			data->content = g_strdup (text ? (const char *) text : "");
			text          = sqlite3_column_text (stmt, 2);
			data->color   = text ? g_strdup ((const char *) text) : NULL;
			data->pos_x   = sqlite3_column_int (stmt, 3);
			data->pos_y   = sqlite3_column_int (stmt, 4);
			data->width   = sqlite3_column_int (stmt, 5);
			data->height  = sqlite3_column_int (stmt, 6);
			text          = sqlite3_column_text (stmt, 7);
			data->monitor = text ? g_strdup ((const char *) text) : NULL;

			g_ptr_array_add (notes, data);
		}

	sqlite3_finalize (stmt);

	if (rc != SQLITE_DONE)
		{
			g_set_error (error,
			             GNOME_STICKY_NOTES_DATABASE_ERROR, 0,
			             "Failed while reading notes: %s",
			             sqlite3_errmsg (self->db));
			g_ptr_array_unref (notes);
			return NULL;
		}

	return notes;
}

gint64
gnome_sticky_notes_database_create_note (GnomeStickyNotesDatabase  *self,
                                         GError                   **error)
{
	static const char *sql =
		"INSERT INTO notes (content, width, height, created_at, updated_at) "
		"VALUES ('', ?, ?, ?, ?);";
	sqlite3_stmt *stmt = NULL;
	gint64 now = g_get_real_time () / G_USEC_PER_SEC;
	gint64 id;

	g_return_val_if_fail (GNOME_STICKY_NOTES_IS_DATABASE (self), -1);

	if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
		{
			g_set_error (error,
			             GNOME_STICKY_NOTES_DATABASE_ERROR, 0,
			             "Failed to prepare insert: %s",
			             sqlite3_errmsg (self->db));
			return -1;
		}

	sqlite3_bind_int  (stmt, 1, DEFAULT_NOTE_WIDTH);
	sqlite3_bind_int  (stmt, 2, DEFAULT_NOTE_HEIGHT);
	sqlite3_bind_int64 (stmt, 3, now);
	sqlite3_bind_int64 (stmt, 4, now);

	if (sqlite3_step (stmt) != SQLITE_DONE)
		{
			g_set_error (error,
			             GNOME_STICKY_NOTES_DATABASE_ERROR, 0,
			             "Failed to create note: %s",
			             sqlite3_errmsg (self->db));
			sqlite3_finalize (stmt);
			return -1;
		}

	id = sqlite3_last_insert_rowid (self->db);
	sqlite3_finalize (stmt);

	return id;
}

gboolean
gnome_sticky_notes_database_save_note (GnomeStickyNotesDatabase        *self,
                                       const GnomeStickyNotesNoteData  *data,
                                       GError                         **error)
{
	static const char *sql =
		"UPDATE notes SET content=?, color=?, pos_x=?, pos_y=?, "
		"width=?, height=?, monitor=?, updated_at=? WHERE id=?;";
	sqlite3_stmt *stmt = NULL;
	gint64 now = g_get_real_time () / G_USEC_PER_SEC;

	g_return_val_if_fail (GNOME_STICKY_NOTES_IS_DATABASE (self), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
		{
			g_set_error (error,
			             GNOME_STICKY_NOTES_DATABASE_ERROR, 0,
			             "Failed to prepare update: %s",
			             sqlite3_errmsg (self->db));
			return FALSE;
		}

	sqlite3_bind_text  (stmt, 1, data->content ? data->content : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text  (stmt, 2, data->color, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int   (stmt, 3, data->pos_x);
	sqlite3_bind_int   (stmt, 4, data->pos_y);
	sqlite3_bind_int   (stmt, 5, data->width);
	sqlite3_bind_int   (stmt, 6, data->height);
	sqlite3_bind_text  (stmt, 7, data->monitor, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64 (stmt, 8, now);
	sqlite3_bind_int64 (stmt, 9, data->id);

	if (sqlite3_step (stmt) != SQLITE_DONE)
		{
			g_set_error (error,
			             GNOME_STICKY_NOTES_DATABASE_ERROR, 0,
			             "Failed to save note: %s",
			             sqlite3_errmsg (self->db));
			sqlite3_finalize (stmt);
			return FALSE;
		}

	sqlite3_finalize (stmt);
	return TRUE;
}

gboolean
gnome_sticky_notes_database_delete_note (GnomeStickyNotesDatabase  *self,
                                         gint64                     id,
                                         GError                   **error)
{
	static const char *sql = "DELETE FROM notes WHERE id=?;";
	sqlite3_stmt *stmt = NULL;

	g_return_val_if_fail (GNOME_STICKY_NOTES_IS_DATABASE (self), FALSE);

	if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
		{
			g_set_error (error,
			             GNOME_STICKY_NOTES_DATABASE_ERROR, 0,
			             "Failed to prepare delete: %s",
			             sqlite3_errmsg (self->db));
			return FALSE;
		}

	sqlite3_bind_int64 (stmt, 1, id);

	if (sqlite3_step (stmt) != SQLITE_DONE)
		{
			g_set_error (error,
			             GNOME_STICKY_NOTES_DATABASE_ERROR, 0,
			             "Failed to delete note: %s",
			             sqlite3_errmsg (self->db));
			sqlite3_finalize (stmt);
			return FALSE;
		}

	sqlite3_finalize (stmt);
	return TRUE;
}
