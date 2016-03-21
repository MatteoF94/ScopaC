#include <gtk/gtk.h>
#include <string.h>
#include "room_UI.h"

#define MAX_DIM_ROOM_NAME 25
#define MAX_BUFFER 1024

extern int inside_room;
extern int end_game;

/*---------------------------------------------------------------------------*/
/*-------- Handles the room-about messagges incoming from the server --------*/
/*---------------------------------------------------------------------------*/
gboolean connection_handler (GSocket *socket, GIOCondition condition, gpointer user_data){

	/*-------- Declaration and casting of variables --------*/
	struct UI_ref *UI = (struct UI_ref*) user_data;
	gchar *string = (char *) g_malloc (MAX_BUFFER*sizeof(char));
	clear_buffer(string,MAX_BUFFER);
	gint value, i, bytes_read;

	/*-------- Data reception and closed server handling --------*/
	bytes_read = g_socket_receive(socket,string,MAX_BUFFER,NULL,NULL);
	g_print("\nRec: %s",string);

	if(bytes_read == 0){
		g_application_quit(G_APPLICATION(UI->app));
		return FALSE;
	}

	// Some client (C) or the actual one (R) created a room
	if(string[0] == 'C' || string[0]== 'R'){
		create_room(&string[2],UI, string[0]);
		if(string[0] == 'R'){
			inside_room = 1;	
			gtk_widget_set_sensitive(UI->leave,TRUE);	// Now he is able to leave
		}
		g_socket_send(socket,"#@##",strlen("#@##"),NULL,NULL);	// Used only for the SET event 

		g_free(string);
		return TRUE;
	}

	// The client succesfully joined a room, which must be selected
	if(string[0] == 'J'){

		gtk_widget_set_sensitive(UI->leave,TRUE);	// Now he is able to leave
		gint room_number;
		GList *children = gtk_container_get_children(GTK_CONTAINER(UI->box));

		room_number = g_list_length(children);
		for(i=0;i<room_number;i++)
			if(g_strcmp0(&string[2],gtk_label_get_text(g_list_nth_data(children,i)))==0){
				inside_room = 1;
				gtk_widget_set_name(g_list_nth_data(children,i),"selection");
				break;
		}
		g_socket_send(socket,"###",strlen("###"),NULL,NULL);

		g_free(string);
		return TRUE;
	}

	// The client succesfully left a room, which must be deselected
	if(string[0] == 'L'){

		inside_room = 0;
		value = atoi(&string[2]);
		deselect(value,UI);
		gtk_widget_set_sensitive(UI->start,FALSE);
		g_socket_send(socket,"###",strlen("###"),NULL,NULL);
		
		g_free(string);
		return TRUE;
	}

	// A room has no players in it, so it has to be destroyed
	if(string[0] == 'D' && string[1] == '#'){

		value = atoi(&string[3]);
		destroy_room(value,UI);

		g_free(string);
		return TRUE;
	}

	// The room to be joined is already full, maybe a match has already started!
	if(g_strcmp0(string,"#Full#") == 0){

		gtk_entry_set_text(GTK_ENTRY(UI->join_entry),"Full Room");
		inside_room = 0;

		g_free(string);
		return TRUE;
	}

	// A room has two players in it, the room-master can now start the game
	if(g_strcmp0(string,"#starter#") == 0){
		gtk_widget_set_sensitive(UI->start,TRUE);
		g_free(string);
		return TRUE;
	}

	// The status of a room changed and now it's no more full
	if(g_strcmp0(string,"#unstart#") == 0){
		gtk_widget_set_sensitive(UI->start,FALSE);
		g_socket_send(socket,"###",strlen("###"),NULL,NULL);
		g_free(string);
		return TRUE;
	}

	// The room-master choose to start the game, the Game Interface is now initialized
	if(g_strcmp0(string,"#match#") == 0){
		re_create_UI(UI);
	}

	g_free(string);
	return TRUE;
}

/*---------------------------------------------------------------------------------------------------*/
/*-------- Network loop thread, connects client to server and set the incoming data handlers --------*/
/*---------------------------------------------------------------------------------------------------*/
gpointer network_loop (gpointer user_data){

	/*-------- Declaration and casting of the utility variables --------*/
    GError *error = NULL;
    struct UI_ref *UI = (struct UI_ref*) user_data;

	/*-------- Connect the client to an host by an address and a gate --------*/
    GSocketClient *client = g_socket_client_new();
    GSocketConnection *connection = g_socket_client_connect_to_host(client, (gchar*)"localhost",3389,NULL,&error);
    if (error != NULL) {
        g_error("%s\n", error->message);
        g_clear_error(&error);
        return NULL;
    }
    GSocket *socket = g_socket_connection_get_socket (connection);
    g_socket_send(socket,"S",strlen("S"),NULL,NULL);	// Set, the client has to know all the already existing rooms!
    UI->socket = socket;

    /*-------- Creates a source, watching for incoming data --------*/
    GSource *socket_source = g_socket_create_source (socket, G_IO_IN, NULL);
    UI->source = socket_source;
    g_source_set_callback (socket_source, (GSourceFunc) connection_handler, UI, NULL);

	/*-------- Create a new context and a new loop associated --------*/
    GMainContext* context;
    context=g_main_context_default();
    g_source_attach (socket_source, context);

    return NULL;
}

/*------------------------------------------------------------------------------------------*/
/*-------- Called when the application starts to run, it creates the Room Interface --------*/
/*------------------------------------------------------------------------------------------*/
void activate_call(GtkApplication *app, gpointer user_data){
	
	/*-------- Variables declaration --------*/
	GtkWidget *window;
	GdkGeometry hints;

    GtkWidget *label_create;
    GtkWidget *entry_create;
	GtkWidget *create_box;

	GtkWidget *label_join;
	GtkWidget *entry_join;
	GtkWidget *join_box;

	GtkWidget *leave;
	GtkWidget *start;
	GtkWidget *button_box;

	GtkWidget *scrolled_win;
	GtkWidget *collector;
	GtkWidget *container;

    GError *error = NULL;
    GtkCssProvider *provider;
    GdkDisplay *display;
    GdkScreen *screen;  
    gchar* style_css = "data.css";
    gsize bytes_written, bytes_read;

    GThread *mainT;

    struct UI_ref *UI = (struct UI_ref*) user_data;

    inside_room = 0;	// Just to be sure, at the creation, the client is in no room

	/*-------- Room selection window creation and definition --------*/
	window = gtk_application_window_new(GTK_APPLICATION(app));
	gtk_window_set_position(GTK_WINDOW(window),GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(GTK_WINDOW(window),FALSE);
	hints.min_width = 400;
	hints.max_width = 400;
	hints.min_height = 500;
	hints.max_height = 500;
	gtk_window_set_geometry_hints(GTK_WINDOW(window),window,&hints,(GdkWindowHints)(GDK_HINT_MIN_SIZE|GDK_HINT_MAX_SIZE));

	/*-------- Section about the widget used to create a room --------*/
	entry_create = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry_create), TRUE);
    gtk_entry_set_max_length(GTK_ENTRY(entry_create),MAX_DIM_ROOM_NAME);
    gtk_widget_set_margin_top(entry_create,5);
    gtk_widget_set_margin_bottom(entry_create,5);
    gtk_widget_set_margin_end(entry_create,5);

    label_create = gtk_label_new("CREATE ROOM:");
    gtk_widget_set_margin_end(label_create,10);
    gtk_widget_set_margin_start(label_create,10);
    gtk_widget_set_margin_top(label_create,2);
    gtk_widget_set_name(label_create,"create_label");

    create_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    gtk_widget_set_name(create_box,"creation");
    gtk_box_set_homogeneous(GTK_BOX(create_box),FALSE);
    gtk_box_pack_start(GTK_BOX(create_box),label_create,FALSE,FALSE,0);    
    gtk_box_pack_start(GTK_BOX(create_box),entry_create,TRUE,TRUE,0);

	/*-------- Section about the widget used to join a room --------*/
	entry_join = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry_join), TRUE);
    gtk_entry_set_max_length(GTK_ENTRY(entry_join),MAX_DIM_ROOM_NAME);
    gtk_widget_set_margin_top(entry_join,5);
    gtk_widget_set_margin_bottom(entry_join,5);
    gtk_widget_set_margin_end(entry_join,5);

    label_join = gtk_label_new("JOIN ROOM:");
    gtk_widget_set_margin_end(label_join,10);
    gtk_widget_set_margin_start(label_join,10);
    gtk_widget_set_margin_top(label_join,2);
    gtk_widget_set_name(label_join,"create_label");

    join_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    gtk_widget_set_name(join_box,"creation");
    gtk_box_set_homogeneous(GTK_BOX(join_box),FALSE);
    gtk_box_pack_start(GTK_BOX(join_box),label_join,FALSE,FALSE,0);    
    gtk_box_pack_start(GTK_BOX(join_box),entry_join,TRUE,TRUE,0);

	/*-------- Packing of the widget to leave a room or start a match, unclickable for now --------*/
    leave = gtk_button_new_with_label("LEAVE ROOM");
    gtk_widget_set_sensitive(leave,FALSE);

    start = gtk_button_new_with_label("START MATCH");
    gtk_widget_set_sensitive(start,FALSE);

    button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_box_set_homogeneous(GTK_BOX(button_box),TRUE);
    gtk_box_pack_start(GTK_BOX(button_box),leave,TRUE,TRUE,20);
    gtk_box_pack_start(GTK_BOX(button_box),start,TRUE,TRUE,20);

	/*-------- Section about the widget that will "collect" the rooms --------*/
	scrolled_win = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled_win),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    gtk_widget_set_name(scrolled_win,"my_scroll");

    collector = gtk_box_new(GTK_ORIENTATION_VERTICAL,5);
    gtk_box_set_homogeneous(GTK_BOX(collector),FALSE);

    gtk_container_add(GTK_CONTAINER(scrolled_win),collector);

	/*-------- Define the main container and pack it into the main window --------*/
    container = gtk_box_new(GTK_ORIENTATION_VERTICAL,1);
    gtk_widget_set_margin_start(container,15);
    gtk_widget_set_margin_end(container,15);
    gtk_widget_set_margin_top(container,15);
    gtk_widget_set_margin_bottom(container,15);
    gtk_box_pack_start(GTK_BOX(container),scrolled_win,TRUE,TRUE,0);
    gtk_box_pack_start(GTK_BOX(container),create_box,FALSE,FALSE,5);
    gtk_box_pack_start(GTK_BOX(container),join_box,FALSE,FALSE,5);
    gtk_box_pack_start(GTK_BOX(container),button_box,FALSE,FALSE,5);
    gtk_container_add(GTK_CONTAINER(window),container);

	/*-------- Fill the User Interface structure --------*/
    UI->window_room = window;
    UI->box = collector;
    UI->join_entry = entry_join;
    UI->leave = leave;
    UI->start = start;

	/*-------- Signal and event handlers section --------*/
    g_signal_connect(G_OBJECT(entry_create), "key_release_event",G_CALLBACK(send_create_room),UI); 
    g_signal_connect(G_OBJECT(entry_join), "key_release_event", G_CALLBACK(send_join_room),UI);
    g_signal_connect(G_OBJECT(leave),"clicked",G_CALLBACK(send_leave),UI);
    g_signal_connect(G_OBJECT(start),"clicked",G_CALLBACK(match_up),UI);

    /*-------- CSS section and network creation stuff --------*/
    provider = gtk_css_provider_get_default();
    display = gdk_display_get_default();
    screen = gdk_display_get_default_screen(display);
    gtk_style_context_reset_widgets(screen);
    gtk_style_context_add_provider_for_screen(screen,
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    gtk_css_provider_load_from_path (provider,
                                     g_filename_to_utf8(style_css, strlen(style_css), &bytes_read, &bytes_written, &error),
                                     NULL);
    g_object_unref(provider);

    gtk_widget_show_all(window);
    mainT = g_thread_new("Thread_main", network_loop,UI);	// Not necessary but useful
	
	return;
}

/*-----------------------------------------------------------------------------------------------*/
/*-------- Body of the whole program, here everything is created and memory is allocated --------*/
/*-----------------------------------------------------------------------------------------------*/
int main(int argc, char** argv){

	/*-------- Declaration and casting of variables --------*/
	GtkApplication *app;
	int status, i;
	struct UI_ref *UI;
	change_alow alow;
	struct card_data *player_hand[13];
    struct widget_position *position[13];
    struct UI_game *game;
    struct match_data *match_data;
    
	/*-------- Allocating memory for each pointer of the arrays --------*/
    UI = (struct UI_ref*) malloc (sizeof(struct UI_ref));

    game = (struct UI_game*) malloc (sizeof(struct UI_game));
    match_data = (struct match_data*) malloc (sizeof(struct match_data));
    for(i=0;i<13;i++){
        player_hand[i]= (struct card_data*) malloc (sizeof(struct card_data));
        position[i] = (struct widget_position*) malloc (sizeof(struct widget_position));
    }

	/*-------- Completing the main structure creation and reset of all its fields --------*/
    UI->game_interface = &alow;

    alow.UI = game;
    alow.match_data = match_data;
    for(i=0;i<13;i++){
        alow.player_hand[i]=player_hand[i];
        alow.position[i]=position[i]; 
        reset_values(&alow,i);
    }

    /*-------- Application running stuff --------*/
	app = gtk_application_new("org.gtk.Lobby",G_APPLICATION_FLAGS_NONE);
	UI->app = app;
	g_signal_connect(app,"activate",G_CALLBACK(activate_call), UI);
	status = g_application_run(G_APPLICATION(app),argc,argv);

	/*-------- Freeing all the previously allocated memory --------*/
	g_object_unref(app);
	free(UI);
	free(game);
	free(match_data);
    for(i=0;i<13;i++){
        free(player_hand[i]);
        free(position[i]);
    }

    g_print("\nStatus %d",status);

    /*-------- And eventually restart the program, sending the client to the room selection --------*/
    if(end_game == 1)
    	execl("room","room",(char *) 0, 0);

	return status;
}