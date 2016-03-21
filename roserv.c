#include <string.h>
#include <gio/gio.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_DIM_ROOM_NAME 27
#define MAX_BUFFER 1024

struct client_data{		// Useful when the client is in the lobby, it is absorbed by the room_data once the match begins
	GSocketConnection *connection;
	guint g_source_id;
};

struct room_data {		// Contains the rooms whereabouts
	char name[MAX_DIM_ROOM_NAME];
	int cards[40];
	gint deck_index;
	gint id_pl1;	// It actually is the g_source_id of the first client in the room
	GSocketConnection *conn1;
	gint id_pl2;	// It actually is the g_source_id of the second client in the room
	GSocketConnection *conn2;
	gint master_id;	// The master is the client who joined the room since the longest time
	gint wait_for_start;
	gint status;	// Used in the "snake" technique within the match messagges to be handled
	gint rematch_will;	// Keep count of how many players want to have a new match
};

struct LoopData {		// Main structure, that contains rooms, clients and the main loop
	GMainLoop *loop;	// The main context loop, useful when dealing with sources and context
	GList *clients;		// List of client_data structures
	GList *rooms;		// List of room_data structures
};

/*---------------------------------*/
/*-------- Clears a string --------*/
/*---------------------------------*/
void clear_buffer (char *a, int dimension) {
	for (int i = 0; i < dimension; i++) {
		a[i] = '\0';
	}
}

/*----------------------------------------------------------*/
/*-------- Shuffles the deck, randomizing its cards --------*/
/*----------------------------------------------------------*/
void shuffle(int *array, size_t n){

  srand(time(NULL));
  if (n > 1) 
    {
        size_t i;
        for (i = 0; i < n; i++) 
        {
          size_t j = rand()%(i+1); /// (RAND_MAX / (n - i) + 1);
          int t = array[j];
          array[j] = array[i];
          array[i] = t;
        }
    }
    return;
}

/*-------------------------------------------------------------------------------------*/
/*-------- Read the incomng messages, Phase Two: the match has already started --------*/
/*-------------------------------------------------------------------------------------*/
static gboolean read_match(GSocket *socket, GIOCondition condition, gpointer user_data){

	char *buffer = (char*) g_malloc(MAX_BUFFER*sizeof(char));	
	clear_buffer(buffer,MAX_BUFFER);
	struct room_data *room = (struct room_data *) user_data;
	gint bytes_read;
	gint i;
	GSocketConnection *other_conn;

	// Useful way to quickly get who sent the message and who is the other player
	if(g_socket_connection_get_socket(room->conn1) == socket)
		other_conn = room->conn2;
	else
		other_conn = room->conn1;

	// Read and handle if no data is read
	bytes_read = g_socket_receive (socket, buffer, MAX_BUFFER, NULL, NULL);
	g_print("\nBuf: '%s'",buffer);

	if(bytes_read == 0){
		g_socket_close(socket,NULL);
		return FALSE;
	}

	/*-------- Chat identifier, send the message to both players (yes, even the one who sent it) --------*/
	if(buffer[0] == 'T'){
		g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),buffer,strlen(buffer),NULL,NULL);
		g_socket_send(socket,buffer,strlen(buffer),NULL,NULL);
		g_free(buffer);
		return TRUE;
	}

	/*-------- The other player closed the window and the whole application --------*/
	if(g_strcmp0(buffer,"#recreated#") == 0){
		g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),"#recreated#",strlen("#recreated#"),NULL,NULL);
		g_free(buffer);
		return TRUE;
	}

	/*-------- First turn syncro handling (client->server->client...) --------*/
	if(g_strcmp0(buffer,"#first_turn#") == 0){
		clear_buffer(buffer,MAX_BUFFER);
		sprintf(buffer,"*_%d",room->cards[room->deck_index]);
		if(room->deck_index < 3){	// First player, snake technique send_client->do_stuff->send_server->check_condition->repeat
			room->deck_index++;
			g_usleep(150000);
			g_socket_send(socket,buffer,strlen(buffer),NULL,NULL);
			g_free(buffer);
			return TRUE;
		}
		if(room->deck_index > 2 && room->deck_index < 6){	// Second player, same snake technique
			room->deck_index++;
			g_usleep(150000);
			if(room->deck_index == 4)	// Condition verified, it's the second player turn
				g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),buffer,strlen(buffer),NULL,NULL);
			else
				g_socket_send(socket,buffer,strlen(buffer),NULL,NULL);
			g_free(buffer);
			return TRUE;
		}
		if(room->deck_index > 5 && room->deck_index < 10){	// Both players dock, same snake technique
			g_usleep(75000);
			if(room->status == 0){ 
				g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),buffer,strlen(buffer),NULL,NULL);
				room->status = 1;
				g_free(buffer);
				return TRUE;
			}
			else{
				g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),buffer,strlen(buffer),NULL,NULL);
				room->status = 0;
				room->deck_index++;	
				g_free(buffer);
				return TRUE;
			}
		}
		if(room->deck_index == 10)	// Reached when the not-master player receives the fourth dock card
			room->status = 0;	// Just to be sure, it should already be 0 by the previous else branch
	}

	/*-------- Add card to the enemy's dock --------*/
	if(buffer[0] == 'A'){
		g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),buffer,strlen(buffer),NULL,NULL);
		g_free(buffer);
		return TRUE;
	}

	/*-------- Destroy part of the enemy's dock --------*/
	if(buffer[0] == 'D'){
		g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),buffer,strlen(buffer),NULL,NULL);	
		g_free(buffer);
		return TRUE;
	}

	/*-------- Send the calculated primiera to the other client --------*/
	if(buffer[0] == 'P'){
		g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),buffer,strlen(buffer),NULL,NULL);	
		g_free(buffer);
		return TRUE;
	}

	if(g_strcmp0(buffer,"#all_destro#") == 0){
		g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),buffer,strlen(buffer),NULL,NULL);	
		g_free(buffer);
		return TRUE;
	}

	/*-------- The destruction of a card just finished, time to regive the control to the other player --------*/
	if(g_strcmp0(buffer,"#end_destro#") == 0){
		g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),"#end_destro#",strlen("#end_destro#"),NULL,NULL);
		g_free(buffer);
		return TRUE;
	}

	/*-------- Gives a new hand to both players, except for the last turn --------*/
	if(g_strcmp0(buffer,"#give_hand#") == 0){

		if(room->deck_index == 50){
			g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),"#primiera#",strlen("#primiera#"),NULL,NULL);
			g_free(buffer);
			return TRUE;
		}

		if(room->status < 6 && room->deck_index < 40){	// Only six new cards must be taken from the deck
			g_print("\nIm in the correct");
			clear_buffer(buffer,MAX_BUFFER);
			sprintf(buffer,"@_%d",room->cards[room->deck_index]);
			room->deck_index++;
			room->status++;
			g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),buffer,strlen(buffer),NULL,NULL);
			g_print("\nSent %s",buffer);
			g_free(buffer);
			return TRUE;
		}
		else{
			g_print("\nIm in the else :(");
			room->status = 0;
			if(room->deck_index == 40){
				room->deck_index = 41;	// When the last card is taken, the index is forced to a non-valid number to execute the c.b. below
				g_free(buffer);
				return TRUE;
			}
		}

		if(room->deck_index == 41){		// No more cards! Final phase: the remaining dock cards will be auto captured by the last "capturer"
			g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),"#match_over#",strlen("#match_over#"),NULL,NULL);
			room->deck_index = 50;		// Forced again, to execute the upper branch, the next time give hand is called
			free(buffer);
			return TRUE;
		}
		g_print("\nShould not be here...");
	}

	/*-------- The first check of last "capturer" has failed, must check on the other one --------*/
	if(g_strcmp0(buffer,"#match_failed#") == 0){	// Well, the last "capturer" ... was the other player
		g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),"#match_over#",strlen("#match_over#"),NULL,NULL);
		room->deck_index = 50;	// Not necessary but just to be sure...
		g_free(buffer);
		return TRUE;
	}

	/*-------- One client is checking if the other has, as well, no more cards in hand --------*/
	if(g_strcmp0(buffer,"#phase_end#") == 0){
		room->status = 0;
		g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),buffer,strlen(buffer),NULL,NULL);
		free(buffer);
		return TRUE;
	}

	/*-------- Primiera calculum ended --------*/
	if(g_strcmp0(buffer,"#end_primiera#") == 0){
		g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),"#calc_results#",strlen("#calc_results#"),NULL,NULL);
		g_socket_send(socket,"#calc_results#",strlen("#calc_results#"),NULL,NULL);
		g_free(buffer);
		return TRUE;
	}

	/*-------- Primiera info to other player --------*/
	if(g_strcmp0(buffer,"#win_primiera#") == 0 || g_strcmp0(buffer,"#draw_primiera#") == 0 || g_strcmp0(buffer,"#lost_primiera#") == 0){
		g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),buffer,strlen(buffer),NULL,NULL);	
		g_free(buffer);
		return TRUE;
	}

	/*-------- Send the score to the other player --------*/
	if(strncmp(buffer,"score",5) == 0){
		g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),buffer,strlen(buffer),NULL,NULL);	
		g_free(buffer);
		return TRUE;
	}

	/*-------- Rematch Will of the players --------*/
	if(g_strcmp0(buffer,"#rematch#") == 0){
		room->rematch_will++;
		if(room->rematch_will == 2){
			shuffle(room->cards,40);
			room->deck_index = 0;
			room->status = 0;
			room->rematch_will = 0;
			g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(other_conn)),"#anew#",strlen("#anew#"),NULL,NULL);
			g_socket_send(socket,"#anew#",strlen("#anew#"),NULL,NULL);
		}
	}

	g_free(buffer);
	return TRUE;
}

/*--------------------------------------------------------------------------------------------------------------------------------*/
/*-------- Once two clients join the same room, the match begins (and the server makes sure the two players realize that) --------*/
/*--------------------------------------------------------------------------------------------------------------------------------*/
void recreate(struct room_data *room , gpointer user_data){

	/*-------- Declaration and casting of variables --------*/
	struct LoopData * main_loop_data = (struct LoopData* ) user_data;
	GSource *source1;
	GSource *source2;
	GSocket *socket1;
	GSocket *socket2;
	gint i;
	gint client_number;

	// The room is now active and therefore, the deck must be initialized, and shuffled of course
	for(i=0;i<40;i++){
		room->cards[i]=i+1;
	}
	shuffle(room->cards,40);
	room->deck_index = 0;
	room->status = 0;
	room->rematch_will = 0;

	// Delete the first client from the main client list ... he is already in game, no need to be in the lobby!
	client_number = g_list_length(main_loop_data->clients);
	for(i=0;i<client_number;i++){
		if(room->id_pl1 == ((struct client_data*)g_list_nth_data(main_loop_data->clients,i))->g_source_id){
			struct client_data *client = (struct client_data*)g_list_nth_data(main_loop_data->clients,i);
			main_loop_data->clients = g_list_remove_all(main_loop_data->clients,client);
			free(client);
			break;
		}
	}
	
	// Delete the second client from the main client list ... he is already in game, no need to be in the lobby!
	client_number = g_list_length(main_loop_data->clients);
	for(i=0;i<client_number;i++){
		if(room->id_pl2 == ((struct client_data*)g_list_nth_data(main_loop_data->clients,i))->g_source_id){
			struct client_data *client = (struct client_data*)g_list_nth_data(main_loop_data->clients,i);
			main_loop_data->clients = g_list_remove_all(main_loop_data->clients,client);
			free(client);
			break;
		}
	}

	// Set the new sources callback functions and send to both players the message to "realize" the Game Interface
	source1 = g_main_context_find_source_by_id(g_main_loop_get_context (main_loop_data->loop),room->id_pl1);
	g_source_set_callback (source1, (GSourceFunc)read_match, room, NULL);	// New Socket reading function, overrides the previous one
	socket1 = g_socket_connection_get_socket(G_SOCKET_CONNECTION(room->conn1));
	g_socket_send(socket1,"#match#",strlen("#match#"),NULL,NULL);

	source2 = g_main_context_find_source_by_id(g_main_loop_get_context (main_loop_data->loop),room->id_pl2);
	g_source_set_callback (source2, (GSourceFunc)read_match, room, NULL);	// New Socket reading function, overrides the previous one
	socket2 = g_socket_connection_get_socket(G_SOCKET_CONNECTION(room->conn2));
	g_socket_send(socket2,"#match#",strlen("#match#"),NULL,NULL);

	return;
}

/*--------------------------------------------------------------------*/
/*-------- Check for empty rooms, if there is one, destroy it --------*/
/*--------------------------------------------------------------------*/
gboolean check_empty_room ( GList **clients, GList **rooms, gchar *buffer){

	gint client_number, room_number;
	gint j,i;
	room_number = g_list_length(*rooms);
	client_number = g_list_length(*clients);
	
	for(j=0;j<room_number;j++){
		clear_buffer(buffer,MAX_BUFFER);
		struct room_data *room = ((struct room_data*)g_list_nth_data(*rooms,j));

		if(room->id_pl1 == -1 && room->id_pl2 == -1){
			sprintf(buffer,"D#_%d",j);

			for(i=0;i<client_number;i++){
				g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(((struct client_data*)g_list_nth_data(*clients,i))->connection)),
							buffer,strlen(buffer),NULL,NULL);
			}

			*rooms = g_list_remove_all(*rooms,room);
			free(room);
			return TRUE;
		}
	}

	return FALSE;
}

/*------------------------------------------------------------------------*/
/*-------- Read the incoming messagges, Phase One: room selection --------*/
/*------------------------------------------------------------------------*/
gboolean read_socket ( GSocket *socket, GIOCondition condition, gpointer user_data) {

	/*-------- Declaration and casting of utility variables --------*/
	char *buffer = (char*) g_malloc(MAX_BUFFER*sizeof(char));	
	clear_buffer(buffer,MAX_BUFFER);
	gint bytes_read = -1;

	struct LoopData *main_loop_data = (struct LoopData* ) user_data;
	GList **clients = &(main_loop_data->clients);
	GList **rooms = &(main_loop_data->rooms);

	gint client_number;
	gint room_number;

	GSocket *current_socket;
	GSource *current_source;
	gint i,j;

	/*-------- Useful values, such as the number of clients and rooms will be heavily used in this func. --------*/
	client_number = g_list_length(*clients);
	room_number = g_list_length(*rooms);

	/*-------- Check which client of the main list is sending data (number in the list) --------*/
	for (i=0;i<client_number;i++){
		current_socket = g_socket_connection_get_socket(G_SOCKET_CONNECTION(
			((struct client_data*)g_list_nth_data(*clients,i))->connection));
		if(current_socket == socket)
			break;
	}

	bytes_read = g_socket_receive (socket, buffer, MAX_BUFFER, NULL, NULL);
	g_print("\nRec: %s",buffer);

	/*-------- Section to handle the client disconnection --------*/
	if (bytes_read == 0) {

		// If the client disconnects, all his references, such as room belongings must be resetted
		for(j=0;j<room_number;j++){
			struct room_data *room = ((struct room_data*)g_list_nth_data(*rooms,j));
			if(room->id_pl1 == ((struct client_data*)g_list_nth_data(*clients,i))->g_source_id){
				room->id_pl1 = -1;
				room->conn1 = NULL;
				if(room->id_pl2 != -1)
					g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(room->conn2)),
								"#unstart#",strlen("#unstart#"),NULL,NULL);
				else
					check_empty_room(clients,rooms,buffer);	// Should always return TRUE
			}
			if(room->id_pl2 == ((struct client_data*)g_list_nth_data(*clients,i))->g_source_id){
				room->id_pl2 = -1;
				room->conn2 = NULL;
				if(room->id_pl1 != -1)
					g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(room->conn1)),
								"#unstart#",strlen("#unstart#"),NULL,NULL);
				else
					check_empty_room(clients,rooms,buffer);	// Should always return TRUE
			}
		}

		// Usual closing cleanup
		current_source = g_main_context_find_source_by_id(g_main_loop_get_context (main_loop_data->loop),
						 ((struct client_data*)g_list_nth_data(*clients,i))->g_source_id);
		g_print("\nClient closed ID %d",g_source_get_id(current_source));
		g_source_destroy(current_source);	// Deattach the source from the main context
		g_source_unref(current_source);		// Unreference the source to free its memory
		g_object_unref(G_OBJECT(((struct client_data*)g_list_nth_data(*clients,i))->connection));	// Decrement the reference count on the connection closed
		g_socket_close (G_SOCKET(current_socket), NULL);	// Close the socket
		*clients=g_list_remove_all(*clients,g_list_nth_data(*clients,i));	// Remove the element in the list

		return FALSE;
	}

	/*-------- Every time i have a creation (not setting) that message is sent... without utility --------*/
	if(g_strcmp0(buffer,"#@##") == 0){
		free(buffer);
		return TRUE;
	}

	/*-------- Called whenever a client leaves, creates or joins a room to check for empty or ready ones --------*/
	if(g_strcmp0(buffer,"###") == 0){	// Safety check, usually sent when the client receives something
		
		/*-------- Destroy all the rooms with no players in --------*/
		if(check_empty_room(clients,rooms,buffer) == TRUE){
			g_free(buffer);
			return TRUE;
		}

		/*-------- Check for all the full rooms to make the master able to start the match --------*/
		room_number = g_list_length(*rooms);
		for(j=0;j<room_number;j++){
			struct room_data *room = ((struct room_data*)g_list_nth_data(main_loop_data->rooms,j));
			if(room->id_pl1 != -1 && room->id_pl2 != -1 && room->wait_for_start == 0){
				for(i=0;i<client_number;i++){
					if(room->master_id == ((struct client_data*)g_list_nth_data(*clients,i))->g_source_id)
						break;
				}
				room->wait_for_start = 1;
				g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(((struct client_data*)g_list_nth_data(*clients,i))->connection)),
							"#starter#",strlen("#starter#"),NULL,NULL);
				g_free(buffer);
				return TRUE;
			}
		}
		g_free(buffer);
		return TRUE;
	}

	/*-------- The room-master decided to start the game, it's time to recreate the sources of the room's "citizens" --------*/
	if(g_strcmp0(buffer,"#match#")==0){

		for(j=0;j<room_number;j++){
			struct room_data *room = ((struct room_data*)g_list_nth_data(*rooms,j));
			if(room->id_pl1 == ((struct client_data*)g_list_nth_data(*clients,i))->g_source_id 
						|| room->id_pl2 == ((struct client_data*)g_list_nth_data(*clients,i))->g_source_id){

				*rooms = g_list_remove_all(*rooms,room);
				recreate(room,main_loop_data);
				break;
			}
		}

		g_free(buffer);
		return TRUE;
	}

	/*-------- When a client joins the server, it must know all the existing rooms: the server will then "inform" the new client --------*/
	if(buffer[0] == 'S'){	// S for Starter

		for(j=0;j<room_number;j++){
			g_socket_send(socket, ((struct room_data*)g_list_nth_data(*rooms,j))->name,
					strlen(((struct room_data*)g_list_nth_data(*rooms,j))->name),NULL,NULL);
			clear_buffer(buffer,MAX_BUFFER);
			g_socket_receive_with_blocking(socket,buffer,MAX_BUFFER,TRUE,NULL,NULL);	// Because i want to inform the client about one room at time
		}
		g_free(buffer);
		return TRUE;
	}

	/*-------- Create a new room with the name given by the client and "tell" the others about this creation --------*/
	if(buffer[0] == 'C'){	// C for Creation

		struct room_data *room = g_new(struct room_data,1);
		clear_buffer(room->name,MAX_DIM_ROOM_NAME);
		sprintf(room->name,"%s",buffer);	
		room->id_pl1 = ((struct client_data*)g_list_nth_data(*clients,i))->g_source_id;
		room->conn1 = ((struct client_data*)g_list_nth_data(*clients,i))->connection;
		room->master_id = room->id_pl1;
		room->id_pl2 = -1;
		room->conn2 = NULL;
		room->wait_for_start = 0;
		*rooms = g_list_append(*rooms,room);

		for(j=0;j<client_number;j++){	// Send to the other clients the info about the created room
			if(socket == g_socket_connection_get_socket(G_SOCKET_CONNECTION(((struct client_data*)g_list_nth_data(*clients,j))->connection)))
				room->name[0]='R';	// Realize, the creator must acknowledge that it is in the room just created
			else
				room->name[0]='C';	// The other users just have to know that a room has just been created
			g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(
				((struct client_data*)g_list_nth_data(*clients,j))->connection)),room->name,
				strlen(room->name),NULL,NULL);

			room->name[0]='C';	// Reset the standard name used for the rooms, beginning with "C_"	
		}			

		g_free(buffer);
		return TRUE;
	}

	/*-------- Try to join an existing room --------*/
	if(buffer[0] == 'J'){	// J for Joining

		for(j=0;j<room_number;j++){
			if(g_strcmp0(&buffer[2],&(((struct room_data*)g_list_nth_data(main_loop_data->rooms,j))->name[2])) == 0){	// Check the existing names
				
				struct room_data *room = ((struct room_data*)g_list_nth_data(main_loop_data->rooms,j));

				if(room->id_pl1 != -1 && room->id_pl2 != -1){	// Already two players in the room!
					g_socket_send(socket,"#Full#",strlen("#Full#"),NULL,NULL);
					break;
				}
				else
					g_socket_send(socket,buffer,strlen(buffer),NULL,NULL);	// Tell the client he will join for real

				// Add the joining client data to the first free spot in the room
				if(room->id_pl1 == -1){
					room->id_pl1 = ((struct client_data*)g_list_nth_data(*clients,i))->g_source_id;
					room->conn1 = ((struct client_data*)g_list_nth_data(*clients,i))->connection;
					break;
				}

				if(room->id_pl2 == -1){
					room->id_pl2 = ((struct client_data*)g_list_nth_data(*clients,i))->g_source_id;
					room->conn2 = ((struct client_data*)g_list_nth_data(*clients,i))->connection;
					break;
				}
			}
		}

		g_free(buffer);
		return TRUE;
	}

	/*-------- Leave an existing room --------*/
	if(buffer[0]== 'L'){

		clear_buffer(buffer,MAX_BUFFER);
		struct client_data *user = ((struct client_data*)g_list_nth_data(*clients,i));

		for(j=0;j<room_number;j++){

			struct room_data *room = ((struct room_data*)g_list_nth_data(*rooms,j));
			g_print("\nRoom %d, id1 %d, id2 %d",j,room->id_pl1,room->id_pl2);

			if(room->id_pl1 == user->g_source_id || room->id_pl2 == user->g_source_id){
				if(room->id_pl1 == user->g_source_id){
					room->id_pl1 = -1;
					room->master_id = room->id_pl2;
					if(room->id_pl2 != -1)
						g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(room->conn2)),
								"#unstart#",strlen("#unstart#"),NULL,NULL);
				}
				if(room->id_pl2 == user->g_source_id){
					room->id_pl2 = -1;	
					room->master_id = room->id_pl1;
					if(room->id_pl1 != -1)
						g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(room->conn1)),
								"#unstart#",strlen("#unstart#"),NULL,NULL);
				}
				room->wait_for_start = 0;
				sprintf(buffer,"L_%d",j);
				g_print("\nRoom %d, id1 %d, id2 %d",j,room->id_pl1,room->id_pl2);
				g_socket_send(g_socket_connection_get_socket(G_SOCKET_CONNECTION(user->connection)),
								buffer,strlen(buffer),NULL,NULL);
				break;
			}
		}
	}

	free(buffer);
	return TRUE;
}

/*------------------------------------------------------------------------------------------------------------*/
/*-------- Callback function for handling "incoming" signal from GSocketListener (our GSocketService) --------*/
/*------------------------------------------------------------------------------------------------------------*/
static gboolean handle_incoming ( GSocketService *service, GSocketConnection *connection, GObject *source_object, gpointer user_data) {

	/*-------- Casting user_data and initializing some context variables --------*/
	struct LoopData * main_loop_data = (struct LoopData* ) user_data;
	struct client_data *sockcon = g_new(struct client_data,1);
	guint g_source_id = -1;
	gint i;
	char *buffer = (char*) g_malloc(MAX_BUFFER*sizeof(char));	
	clear_buffer(buffer,MAX_BUFFER);

	/*-------- Section that handles the incoming tries of connection from one or more clients --------*/
	g_object_ref (connection); // Reference GSocketConnection as it's automatically dereferenced after the callback returns
	g_message ("Connection incoming from a client\n"); // Debug

	GSocket *socket = g_socket_connection_get_socket (connection); // Get Socket from GSocketConnection
	g_message ("Got socket\n"); // Debug

	GSource *socket_source = g_socket_create_source (socket, G_IO_IN, NULL); // Create a GSource watching for data ready to be read 
	g_message ("Source created for G_IO_IN\n"); // Debug

	g_source_set_callback (socket_source, (GSourceFunc)read_socket, (main_loop_data), NULL);

	g_source_id = g_source_attach (socket_source, g_main_loop_get_context (main_loop_data->loop)); // Attach the GSource to the context of the main loop

	/*-------- Attach all the connection data to the List, appending it --------*/
	sockcon->connection = connection;
	sockcon->g_source_id = g_source_id;

	main_loop_data->clients = g_list_append(main_loop_data->clients,sockcon);	// Isn't g_list_prepend() better? Append is O(n)...

	free(buffer);
	return FALSE;
}

/*-------------------------------*/
/*-------- Main (thread) --------*/
/*-------------------------------*/
int main (int arc, char** argv) {

	/*-------- Declaration of the main structures and variables --------*/
	GError *error = NULL;
	struct LoopData * main_loop_data = g_new (struct LoopData, 1); // Allocate a LoopData struct
	GList *clients = NULL;
	GList *rooms = NULL;
	main_loop_data->clients = clients;
	main_loop_data->rooms = rooms;

	/*-------- Starting the socket service --------*/
	GSocketService *socket_service = g_socket_service_new ();

	/*-------- Listen to a port for incoming connections and checking for errors --------*/
	g_socket_listener_add_inet_port (G_SOCKET_LISTENER(socket_service), 3389, NULL, &error);
	if (error != NULL) {
		g_error("%s\n", error->message);
		g_clear_error(&error);
		return 1;
	}

	/*-------- Register callback for handling incoming event --------*/
	g_signal_connect (socket_service, "incoming", G_CALLBACK(handle_incoming), main_loop_data);

	/*-------- Loop and socket service run section --------*/
	g_socket_service_start(socket_service); // Start socket service
	GMainLoop *main_loop = g_main_loop_new (NULL, FALSE); // Create glib main loop
	main_loop_data->loop = main_loop; // Save it to our data struct
	g_main_loop_run (main_loop); // Run main loop
	g_socket_service_stop (socket_service);

	return 0;
}