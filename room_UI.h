#ifndef _ROOM_LIB
#define _ROOM_LIB

#include <gtk/gtk.h>
#include "game_UI.h"

struct UI_ref{
	GtkApplication *app;
	GSocket *socket;
	GSource *source;
	GtkWidget *window_room;
	GtkWidget *box;
	GtkWidget *join_entry;
	GtkWidget *leave;
	GtkWidget *start;
	change_alow *game_interface;
};

void re_create_UI(gpointer user_data);
void clear_buffer (char *a, int dimension);
void match_up (GtkWidget *start, gpointer user_data);
void send_leave (GtkWidget *leave, gpointer user_data);
gboolean notice_created(gchar *name, gpointer user_data);
void send_create_room (GtkWidget *entry, GdkEventKey *event, gpointer user_data);
gboolean notice_joined(gchar *name, gpointer user_data);
void send_join_room (GtkWidget *entry, GdkEventKey *event, gpointer user_data);
void create_room(char *string, gpointer user_data, char ident);
void deselect (gint value, gpointer user_data);
void destroy_room (gint value, gpointer user_data);

#endif