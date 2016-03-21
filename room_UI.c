#include <gtk/gtk.h>
#include <string.h>
#include <gio/gio.h>
#include "room_UI.h"
#include "game_UI.h"
#define MAX_DIM_ROOM_NAME 25
#define MAX_BUFFER 1024
#define WINDOW_WIDTH 1136   // window fixed width
#define WINDOW_HEIGHT 640   // window fixed heigth
#define CARD_WIDTH 102      // card fixed width
#define CARD_HEIGHT 162     // card fixed heigth

int inside_room = 0;
int end_game = 0;
int i_am_master = 0;
int status_count = 0;

extern int n_player_cards;
extern int n_dock_cards;
extern int add_to_dock;
extern int my_turn;

/*---------------------------------*/
/*-------- Clears a string --------*/
/*---------------------------------*/
void clear_buffer (char *a, int dimension) {
	for (int i = 0; i < dimension; i++) {
		a[i] = '\0';
	}
}

/* --||||||||-- ROOM UTILITY FUNCTIONS SECTION BELOW --||||||||-- */

/*------------------------------------------------------------------------------------------------*/
/*-------- Informs the server that the master of the room just decided to begin the match --------*/
/*------------------------------------------------------------------------------------------------*/
void match_up (GtkWidget *start, gpointer user_data){
	struct UI_ref *UI = (struct UI_ref*) user_data;
	g_socket_send(UI->socket,"#match#",strlen("#match#"),NULL,NULL);
	i_am_master = 1;
	return;
}

/*-------------------------------------------------------------------------------*/
/*-------- Send to the server the client's will to leave the joined room --------*/
/*-------------------------------------------------------------------------------*/
void send_leave (GtkWidget *leave, gpointer user_data){
	struct UI_ref *UI = (struct UI_ref*) user_data;
	g_socket_send(UI->socket,"L",strlen("L"),NULL,NULL);
	return;
}

/*------------------------------------------------------------------------------------------------------*/
/*-------- If there already is a room with the same name... the client won't be able to create! --------*/
/*------------------------------------------------------------------------------------------------------*/
gboolean notice_created(gchar *name, gpointer user_data){

	struct UI_ref *UI = (struct UI_ref*) user_data;
	gint room_number;
	gint i;
	GList *children = gtk_container_get_children(GTK_CONTAINER(UI->box));

	room_number = g_list_length(children);
	for(i=0;i<room_number;i++)
		if(g_strcmp0(name,gtk_label_get_text(g_list_nth_data(children,i)))==0){
			return FALSE;
		}

	return TRUE;
}

/*-----------------------------------------------------------------------*/
/*-------- Send to the server the client's will to create a room --------*/
/*-----------------------------------------------------------------------*/
void send_create_room (GtkWidget *entry, GdkEventKey *event, gpointer user_data){

	if((int)g_utf8_strlen(gtk_entry_get_text(GTK_ENTRY(entry)),-1)!=0){
		if(event->keyval == 65293){
			if(inside_room == 0){
				struct UI_ref *UI = (struct UI_ref*) user_data;
				gchar *sender;
				sender = g_strconcat("C_",gtk_entry_get_text(GTK_ENTRY(entry)),NULL);
				if(notice_created(&sender[2],UI)){
					g_socket_send(UI->socket,sender,strlen(sender),NULL,NULL);
					gtk_entry_set_text(GTK_ENTRY(entry),"");
				}
				else
					gtk_entry_set_text(GTK_ENTRY(entry),"Room already existing!");
			}
			else
				gtk_entry_set_text(GTK_ENTRY(entry),"Already in a room!");
		}
	}
}

/*-----------------------------------------------------------------------------------------------------*/
/*-------- If the client wants to join a room that doesn't exist... it won't be able to do so! --------*/
/*-----------------------------------------------------------------------------------------------------*/
gboolean notice_joined(gchar *name, gpointer user_data){

	struct UI_ref *UI = (struct UI_ref*) user_data;
	gint room_number;
	gint i;
	GList *children = gtk_container_get_children(GTK_CONTAINER(UI->box));

	room_number = g_list_length(children);
	for(i=0;i<room_number;i++)
		if(g_strcmp0(name,gtk_label_get_text(g_list_nth_data(children,i)))==0){
			return TRUE;
		}

	return FALSE;
}

/*---------------------------------------------------------------------*/
/*-------- Send to the server the client's will to join a room --------*/
/*---------------------------------------------------------------------*/
void send_join_room (GtkWidget *entry, GdkEventKey *event, gpointer user_data){

	if((int)g_utf8_strlen(gtk_entry_get_text(GTK_ENTRY(entry)),-1)!=0){
		if(event->keyval == 65293){
			if(inside_room == 0){
				struct UI_ref *UI = (struct UI_ref*) user_data;
				gchar *sender;
				sender = g_strconcat("J_",gtk_entry_get_text(GTK_ENTRY(entry)),NULL);
				if(notice_joined(&sender[2],UI)){
					g_socket_send(UI->socket,sender,strlen(sender),NULL,NULL);
					gtk_entry_set_text(GTK_ENTRY(entry),"");
				}
				else
					gtk_entry_set_text(GTK_ENTRY(entry),"No Room with such name!");
				
			}
			else
				gtk_entry_set_text(GTK_ENTRY(entry),"Already in a room!");	
		}
	}
}

/*---------------------------------------------------------------------------------------------------------------------*/
/*-------- Creates a new room, visible to the actual player; called whenever a player decides to create a room --------*/
/*---------------------------------------------------------------------------------------------------------------------*/
void create_room(char *string, gpointer user_data, char ident){

	GtkWidget *text;
	GtkWidget **box;
	struct UI_ref *UI = (struct UI_ref*) user_data;
	char *label = (char *) g_malloc (MAX_DIM_ROOM_NAME*sizeof(char));
	clear_buffer(label,MAX_DIM_ROOM_NAME);
	gint i;

	box = &(UI->box);

	sprintf(label,"%s",string);

	text = gtk_label_new(label);
	gtk_widget_set_margin_start(text,15);
	gtk_widget_set_margin_end(text,15);

	if(ident == 'C')	// Created by the actual player or by someone else? Used to "select" the room
		gtk_widget_set_name(text,"mylabel");
	else
		gtk_widget_set_name(text,"selection");

	gtk_box_pack_start(GTK_BOX(*box),text,FALSE,FALSE,5);
	gtk_widget_show_all(*box);

	g_free(label);	
	return;
}

/*---------------------------------------------------------------------------------*/
/*-------- Called when a client leaves a room previously joined or created --------*/
/*---------------------------------------------------------------------------------*/
void deselect (gint value, gpointer user_data){
	struct UI_ref *UI = (struct UI_ref*) user_data;
	GList *children = gtk_container_get_children(GTK_CONTAINER(UI->box));
	GtkWidget *label = g_list_nth_data(children,value);
	gtk_widget_set_name(label,"mylabel");
	gtk_widget_set_sensitive(UI->leave,FALSE);
	return;
}

/*--------------------------------------------------------------------------------*/
/*-------- Destroys a room called whenever a room is left with no players --------*/
/*--------------------------------------------------------------------------------*/
void destroy_room (gint value, gpointer user_data){

	struct UI_ref *UI = (struct UI_ref*) user_data;
	GList *children = gtk_container_get_children(GTK_CONTAINER(UI->box));
	GtkWidget *to_destroy = g_list_nth_data(children,value);
	gtk_widget_destroy(to_destroy);

	return;
}

/* --||||||||-- GAME INTERFACE CREATION SECTION BELOW --||||||||-- */

/*------------------------------------------------------------------------------------*/
/*-------- Start the match, sending the server a code to begin the first turn --------*/
/*------------------------------------------------------------------------------------*/
void begin_first_turn (GtkWidget *button, gpointer user_data){		// Beware, only the room-master should be able to reach this function

    change_alow* data = (change_alow *) user_data;
    char *stringa = (char *) g_malloc (MAX_BUFFER*sizeof(char));
    clear_buffer(stringa,MAX_BUFFER);

    stringa = g_strconcat("#","first_turn","#",NULL);	// Could have used a sprintf...
    g_socket_send (data->UI->socket,stringa, strlen(stringa), NULL, NULL);
    gtk_widget_destroy(button);	
	
	g_free(stringa);
    return;
}

/*----------------------------------------------------------*/
/*-------- Function to send data to the main server --------*/
/*----------------------------------------------------------*/
void send_data_entry(GtkWidget *entry, GdkEventKey *event, gpointer user_data){

	/*-------- Check if there is something written in the entry (the only key pressed is not 'return' --------*/
    if((int)g_utf8_strlen(gtk_entry_get_text(GTK_ENTRY(entry)),-1)!=0){

		/*-------- Checks if the key pressed is 'enter' --------*/
        if(event->keyval==65293){

			/*-------- Declaration and casting of utility variables --------*/
            GSocket *socket = (GSocket *) user_data;
            gchar* sender;  //Dummy variable to save string

			/*-------- Send a string to the main server. The string contains: the name of the machine this program is running --------*/
			/*-------- on (user name), the data the user wants to send and a separator ": " between those two sub-strings ------------*/
            sender=g_strconcat("T_",g_ascii_strup(g_get_user_name(),strlen(g_get_user_name())),": ",gtk_entry_get_text(GTK_ENTRY(entry)),NULL);
            g_socket_send (socket,sender, strlen(sender), NULL, NULL);

			/*-------- Clear the entry: the data has been already sent --------*/
            gtk_entry_set_text(GTK_ENTRY(entry),"");
        }
    }
}

/*------------------------------------------------------------------------------------------------------------------------*/
/*-------- Function to receive data and write it into the text_view of the main window or do a specific UI action --------*/
/*------------------------------------------------------------------------------------------------------------------------*/
gboolean new_handler ( GSocket *socket, GIOCondition condition, gpointer user_data){

    if (g_socket_condition_check(socket,G_IO_IN)){  // Not really necessary, condition should already be G_IO_IN TRUE

		/*-------- Declaration and casting of utility variables --------*/
        change_alow *data = (change_alow *) user_data;
        GtkTextBuffer* buffer;  // Create a buffer to keep trace of the text_view content
        GtkTextMark *mark;
        GtkTextIter iter;
        gchar stringa[MAX_BUFFER];
        clear_buffer(stringa,MAX_BUFFER);
        gint value,i;
        gint bytes_read;

		// Receive data and check if the program behavior will be focused on the chat or on the UI (first turn, other turns, ecc...)
        bytes_read = g_socket_receive(socket, stringa, MAX_BUFFER, NULL, NULL);
        g_print("\nRec: '%s'",stringa); 

        // The server is closed... all the clients shoult shut down  
        if (bytes_read == 0) {
            g_socket_close (G_SOCKET(socket), NULL);  // Close the socket
            end_game = 0;
            g_application_quit(G_APPLICATION(data->app));
            return TRUE;
        }

        // The last turn has ended: the player who captured cards last will get the remaining dock cards...
        // ...or let the other player know that it is not the last who captured something!
        if(g_strcmp0(stringa,"#match_over#") == 0){

        	if(data->match_data->last_player == 1){
        		for(i=3;i<13;i++){
        			if(data->player_hand[i]->event_box != NULL)
        				data->position[i]->clicked = 1;		// Useful for the D_value - #end_destro# cycle
        		}
        		for(i=3;i<13;i++)
        			if(data->player_hand[i]->event_box != NULL)		// Check for the first one
        				break;

        		// Luckiest case: the last "capturer" got all the cards of the dock, nothing more to check!
        		if(i == 13){
        			g_socket_send (data->UI->socket,"#give_hand#", strlen("#give_hand#"), NULL, NULL);
        			return TRUE;
        		}

        		clear_buffer(stringa,MAX_BUFFER);

        		// Usual score settings
        		data->match_data->cards_number++;
        		if(calculate_primiera(data->player_hand[i]->value) > 
                            data->match_data->higher_per_seed[data->player_hand[i]->seed]){
                    data->match_data->higher_per_seed[data->player_hand[i]->seed] = calculate_primiera(data->player_hand[i]->value);
                }
        		if(data->position[i]->value >= 21 && data->position[i]->value <= 30){
        			data->match_data->gold_number++;
        			if(data->position[i]->value == 27)
        				data->match_data->settebello = TRUE;
        		}

        		// And usual widget destruction
        		gtk_widget_destroy(data->player_hand[i]->event_box);
                value = data->position[i]->value;
                reset_values(data,i);    
                n_dock_cards--; 
                initialize_reset_position(data);
        		sprintf(stringa,"D_%d",value);
        		my_turn = 10;	// Setted to disable the sweep event, not allowed in this case
        		g_socket_send (data->UI->socket,stringa, strlen(stringa), NULL, NULL);
        	}
        	else
        		g_socket_send (data->UI->socket,"#match_failed#", strlen("#match_failed#"), NULL, NULL);

        	return TRUE;
        }

        // The other player added a card to the dock, so the actual player's dock should be completed
        if(stringa[0] == 'A'){
        	value = atoi(&stringa[2]);
        	add_to_dock = 1;
        	add_card(NULL,data,value);
			my_turn = 1;	// The card has been added, it's now the receiving player turn
            if(n_dock_cards == 0)
			    gtk_label_set_text(GTK_LABEL(data->status_bar),"@@@     MY TURN, SWEEP FOR THE OPPONENT!!     @@@");
            else
                gtk_label_set_text(GTK_LABEL(data->status_bar),"@@@     MY TURN     @@@");

        	if(n_player_cards==0)  // No cards means turn ended, the other client must be informed
        		g_socket_send (data->UI->socket,"#phase_end#", strlen("#phase_end#"), NULL, NULL);
        	return TRUE;
        }

        // The other player captured some of the dock cards: the actual player's dock must be the same of the other
        if(stringa[0] == 'D'){	// Delete all the already captured cards, by value
        	value = atoi(&stringa[2]);
        	for(i=3;i<13;i++){
        		if(data->player_hand[i]->event_box != NULL){
        			if(value == data->position[i]->value){
        				gtk_widget_destroy(data->player_hand[i]->event_box);
        				reset_values(data,i);
        				n_dock_cards--;
        				initialize_reset_position(data);
        				g_socket_send(data->UI->socket,"#end_destro#",strlen("#end_destro#"),NULL,NULL);
        				return TRUE;
        			}
        		}
        	}	
        	return TRUE;
        }

        // The other player has ended the destruction of the dock card, the program now checks if there are other cards to get and destroy
        if(g_strcmp0(stringa,"#end_destro#") == 0){

        	clear_buffer(stringa,MAX_BUFFER);
        	for(i=3;i<13;i++){	
        		if(data->player_hand[i]->event_box != NULL && data->position[i]->clicked == 1){		// All the clicked ones

        			// Setting the player score, such as cards number, gold cards or other things
        			data->match_data->cards_number++;
        			if(calculate_primiera(data->player_hand[i]->value) > 
                            	data->match_data->higher_per_seed[data->player_hand[i]->seed]){
                    	data->match_data->higher_per_seed[data->player_hand[i]->seed] = calculate_primiera(data->player_hand[i]->value);
               	 	}
        			if(data->position[i]->value >= 21 && data->position[i]->value <= 30){
        				data->match_data->gold_number++;
        				if(data->position[i]->value == 27)
        					data->match_data->settebello = TRUE;
        			}

        			// Destruction and UI setting, the same card must be deleted from the other player's dock
                	gtk_widget_destroy(data->player_hand[i]->event_box);
                	value = data->position[i]->value;
                	reset_values(data,i);    
                   	n_dock_cards--; 
                   	initialize_reset_position(data);
                    sprintf(stringa,"D_%d",value);	// All the strings that begin with D_ are used to destroy a card with a certain value
                   	g_socket_send (data->UI->socket,stringa, strlen(stringa), NULL, NULL);
                   	
                   	return TRUE;
                }
        	}

        	data->match_data->last_player = 1;	// That means the client which is "taking" the deck cards is the player... who played last!
        	if(n_dock_cards == 0 && my_turn != 10){		// Check for sweep event, except for the last turn (my_turn setted to 10)
        		data->match_data->sweep_number++;
        	}

        	// If the program is here, all the deletable cards have already been deleted: the other player must know it's now its turn
            if(n_dock_cards == 0)
        	   gtk_label_set_text(GTK_LABEL(data->status_bar),"@@@     OPPONENT'S TURN, MY SWEEP!!     @@@");
            else
                gtk_label_set_text(GTK_LABEL(data->status_bar),"@@@     OPPONENT'S TURN     @@@");
        	g_socket_send (data->UI->socket,"#all_destro#", strlen("#all_destro#"), NULL, NULL);
        	return TRUE;
        }

        // The other client ended its turn, so the actual player can play, knowing that he isn't the player who played last..
        if(g_strcmp0(stringa,"#all_destro#") == 0){

        	my_turn = 1;
        	data->match_data->last_player = 0;	// The other player just captured something!
            if(n_dock_cards == 0)
                gtk_label_set_text(GTK_LABEL(data->status_bar),"@@@     MY TURN, SWEEP FOR THE OPPONENT!!     @@@");
            else
                gtk_label_set_text(GTK_LABEL(data->status_bar),"@@@     MY TURN     @@@");

    		if(n_player_cards==0)   // The player full-turn has reached an end, the other player must be informed
        		g_socket_send (data->UI->socket,"#phase_end#", strlen("#phase_end#"), NULL, NULL);
        	return TRUE;
        }

        // The program reaches this cond. branch only if one of the player has no cards in its hand
        // If also the actual player has no cards, that means a new set of cards has to be given by the server
        if(g_strcmp0(stringa,"#phase_end#") == 0){
        	if(n_player_cards == 0)
        		g_socket_send (data->UI->socket,"#give_hand#", strlen("#give_hand#"), NULL, NULL);
        	return TRUE;
        }

        // The server-master is giving a new set of hands to the players
        if(stringa[0] == '@'){
        	g_print("\nAggiungendo alla mano");
        	value = atoi(&stringa[2]);
        	add_to_dock = 0;	// Added to the player's hand
            add_card(NULL,data,value);
            g_socket_send (data->UI->socket,"#give_hand#", strlen("#give_hand#"), NULL, NULL);
            return TRUE;
        }

        // First turn strings, sent only 7 times per client, at the beginning of the match
        if(stringa[0] == '*'){
            value = atoi(&stringa[2]);
            add_to_dock = 0;	// Should be zero already, just to be sure
            add_card(NULL,data,value);
            g_socket_send (data->UI->socket,"#first_turn#", strlen("#first_turn#"), NULL, NULL);
            status_count++;
            if(status_count == 7){
            	if(i_am_master == 1){
            		my_turn = 1;
            		gtk_label_set_text(GTK_LABEL(data->status_bar),"@@@     MY TURN     @@@");
            	}
            	else{
            		my_turn = 0;
            		gtk_label_set_text(GTK_LABEL(data->status_bar),"@@@     OPPONENT'S TURN     @@@");
            	}
			}
            return TRUE;
        }

        // First player to calculate Primiera
        if(g_strcmp0(stringa,"#primiera#") == 0){
        	value = 0;
        	for(i=0;i<4;i++){
        		if(data->match_data->higher_per_seed[i] == 0){	// No card for a certain seed
        			data->match_data->primiera = FALSE;
        			g_socket_send(data->UI->socket,"P_0",strlen("P_0"),NULL,NULL);    			
        			return TRUE;
        		}
        		value = value + data->match_data->higher_per_seed[i];
        	}
        	clear_buffer(stringa,MAX_BUFFER);
        	sprintf(stringa,"P_%d",value);
        	g_socket_send(data->UI->socket,stringa,strlen(stringa),NULL,NULL);
        	return TRUE;
        }

        // Second player to calculate Primiera
        if(stringa[0] == 'P'){
        	gint sum = 0;
        	value = atoi(&stringa[2]);
        	for(i=0;i<4;i++){
        		if(data->match_data->higher_per_seed[i] == 0){
        			data->match_data->primiera = FALSE;
        			if(value == 0)	// It means that non of the players will access the Primiera
        				g_socket_send(data->UI->socket,"#end_primiera#",strlen("#end_primiera#"),NULL,NULL);
        			else
						g_socket_send(data->UI->socket,"#lost_primiera#",strlen("#lost_primiera#"),NULL,NULL);

        			return TRUE;
        		}
        		sum = sum + data->match_data->higher_per_seed[i];
        	}
        	if(sum > value){
        		data->match_data->primiera = TRUE;
        		g_socket_send(data->UI->socket,"#win_primiera#",strlen("#win_primiera#"),NULL,NULL);
        		return TRUE;
        	}
        	else{
        		data->match_data->primiera = FALSE;
        		if(sum == value){
        			g_socket_send(data->UI->socket,"#draw_primiera#",strlen("#draw_primiera#"),NULL,NULL);
        		}
        		else{
        			g_socket_send(data->UI->socket,"#lost_primiera#",strlen("#lost_primiera#"),NULL,NULL);
        		}
        		return TRUE;
        	}
        }

        // The other player won, lost or didn't draw the primiera
		if(g_strcmp0(stringa,"#win_primiera#") == 0 || g_strcmp0(stringa,"#draw_primiera#") == 0 || g_strcmp0(stringa,"#lost_primiera#") == 0){
			if(g_strcmp0(stringa,"#win_primiera#") == 0){
				data->match_data->primiera = FALSE;		// The other won, so the actual lost
			}
			else{
				data->match_data->primiera = TRUE;
			}
			g_socket_send(data->UI->socket,"#end_primiera#",strlen("#end_primiera#"),NULL,NULL);
			return TRUE;
		}

        // The other player closed the window or simply didn't want a rematch
        if(g_strcmp0(stringa,"#recreated#") == 0){
        	end_game = 1;
        	g_application_quit(G_APPLICATION(data->app));
        	return TRUE;
        }
        
        // Calculates the final score
        if(g_strcmp0(stringa,"#calc_results#") == 0){
			if(data->match_data->cards_number > 20)
				data->match_data->score++;
			if(data->match_data->gold_number > 5)
				data->match_data->score++;
			if(data->match_data->settebello == TRUE)
				data->match_data->score++;
			data->match_data->score = data->match_data->score + data->match_data->sweep_number;
			if(data->match_data->primiera == TRUE)
				data->match_data->score++;
			clear_buffer(stringa,MAX_BUFFER);
			sprintf(stringa,"score_%d",data->match_data->score);
			g_socket_send(data->UI->socket,stringa,strlen(stringa),NULL,NULL);
			return TRUE;
        }

        // Receive the opponent's score and setup the results widget pack
        if(strncmp(stringa,"score",5) == 0){
        	value = atoi(&stringa[6]);
        	gtk_label_set_text(GTK_LABEL(data->status_bar),"@@@     GAME RESULTS     @@@");
        	create_results(data,value);
        	return TRUE;
        }

        // The new match is about to start
        if(g_strcmp0(stringa,"#anew#") == 0){
        	gtk_label_set_text(GTK_LABEL(data->status_bar),"@@@     BEGINNING MATCH     @@@");
        	reset_match(data->match_data);
        	gtk_widget_destroy(data->result_box);
        	status_count = 0;
        	if(i_am_master == 1)
        		g_socket_send(data->UI->socket,"#first_turn#",strlen("#first_turn#"),NULL,NULL);
        	return TRUE;
        }

        // Chat handling, the client enters in his conditional branch many times
        if(stringa[0] == 'T'){
            buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->UI->text_view));  // Get the content (all of it) of the text_view widget
            mark = gtk_text_buffer_get_insert(buffer);  // Get the mark of the buffer, which is like a bookmark that keeps memory of the text position
            gtk_text_buffer_get_iter_at_mark(buffer,&iter,mark);    // Get the iterator associated to a specific mark. It's used to alter the content of the buffer
        
            /* The next branch allows the program to check if there is already something in the text_view. If that's true, then it means
            that the client is receiving data, and that data ha to be written in the text. To distinct between previous data and the data
            the client is getting, i append the "return" character to my buffer. If there is nothing in my text_view, i can skip all that,
            because the received data, is the first since the run of the program */
            if(gtk_text_buffer_get_char_count(buffer))
                gtk_text_buffer_insert(buffer,&iter,"\n",1);    

            gtk_text_buffer_insert(buffer,&iter,&stringa[2],-1);

            /* With the next function the viewed screen is forced to be the one that contains the mark, so that when the
             client receives new data, the older messagges will scroll up */       
            gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(data->UI->text_view),mark);
        }
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------------------------*/
/*-------- Instead of just destroying the window, it's good to let the other player know --------*/
/*-------- something happened... both application should end their work and restart -------------*/
/*-------- from the room lobby (self-execl in main) ---------------------------------------------*/
/*-----------------------------------------------------------------------------------------------*/
void reenter_rooms (GtkWidget *window, gpointer user_data){
	struct UI_ref *UI = (struct UI_ref*) user_data;
	g_socket_send(UI->socket,"#recreated#",strlen("#recreated#"),NULL,NULL);
	end_game = 1;
	g_application_quit(G_APPLICATION(UI->app));
	return; 
}

/*---------------------------------------------------------------------*/
/*-------- Creates the Game Interface and sets the new signals --------*/
/*---------------------------------------------------------------------*/
void re_create_UI(gpointer user_data){
	struct UI_ref *UI = (struct UI_ref*) user_data;

	/*-------- Widget declaration, divided by use --------*/
    GtkWidget *window;
    GdkGeometry hints;    
    GtkWidget *overlay;

    GtkWidget *addcard;
    GtkWidget *delcard;
    GtkWidget *status_bar; 

    GtkWidget *entry_chat;
    GtkWidget *scrolled_win;
    GtkWidget *text_view;    
    GtkWidget *vbox;

    GtkWidget *rules_scrolled;
    GtkWidget *rules_text;
    char *rules = (char*) g_malloc (4*MAX_BUFFER*sizeof(char));
    clear_buffer(rules,4*MAX_BUFFER);
    set_rules(rules);

    GtkTextBuffer* buffer;  // Create a buffer to keep trace of the text_view content
    GtkTextMark *mark;
    GtkTextIter iter;

    change_alow *alow = UI->game_interface;

    my_turn = 0;

	/*-------- Window creation and position --------*/   
    window = gtk_application_window_new(GTK_APPLICATION(UI->app));
    gtk_window_set_position (GTK_WINDOW(window), GTK_WIN_POS_CENTER);   
    gtk_window_set_title (GTK_WINDOW(window), "Sweep!!!"); 
    gtk_window_set_resizable (GTK_WINDOW(window), FALSE); 
    hints.min_width=WINDOW_WIDTH;
    hints.max_width=WINDOW_WIDTH;
    hints.min_height=WINDOW_HEIGHT;
    hints.max_height=WINDOW_HEIGHT;
    gtk_window_set_geometry_hints(GTK_WINDOW(window),window,&hints,(GdkWindowHints)(GDK_HINT_MIN_SIZE|GDK_HINT_MAX_SIZE));
    
	/*-------- Overlay creation, it will contain all the other widgets --------*/
    overlay = gtk_overlay_new ();

	/*-------- Start, capture and status bar widget --------*/
	if(i_am_master == 1){	// Only the master of the room, who started the game, can also start the match
    	addcard=gtk_button_new_with_label ("START MATCH!");
    	gtk_widget_set_margin_start (addcard, 2*WINDOW_WIDTH/3 - 120);
    	gtk_widget_set_margin_end (addcard, WINDOW_WIDTH/3 - 120);
    	gtk_widget_set_margin_top (addcard, WINDOW_HEIGHT/2 - 50);
    	gtk_widget_set_margin_bottom (addcard, WINDOW_HEIGHT/2 - 50); 
    	gtk_widget_set_name (addcard, "mybuttonfirst");
	}

    delcard = gtk_button_new_with_label ("MOSSA");
    gtk_widget_set_margin_start (delcard, WINDOW_WIDTH - 150);
    gtk_widget_set_margin_end (delcard, 30);
    gtk_widget_set_margin_top (delcard, 20);
    gtk_widget_set_margin_bottom (delcard, WINDOW_HEIGHT - 50);
    gtk_widget_set_sensitive(delcard,FALSE);
    gtk_widget_set_name (delcard, "mycapture");

    status_bar = gtk_label_new("@@@ GAME WAITING TO BE STARTED @@@");
    gtk_widget_set_margin_start(status_bar,WINDOW_WIDTH/3+30);
    gtk_widget_set_margin_end(status_bar,200);
    gtk_widget_set_margin_top(status_bar,20);
    gtk_widget_set_margin_bottom(status_bar,WINDOW_HEIGHT-50);
    gtk_widget_set_name(status_bar, "mystatusbar");

	/*-------- Chat widgets creation and settings --------*/
    entry_chat=gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry_chat), TRUE);
    gtk_entry_set_max_length(GTK_ENTRY(entry_chat),256);
    gtk_widget_set_margin_start(entry_chat,5);
    gtk_widget_set_margin_end(entry_chat,5);
    gtk_widget_set_margin_bottom(entry_chat,5);

    scrolled_win = gtk_scrolled_window_new(NULL,NULL);
    gtk_widget_set_margin_start(scrolled_win,4);
    gtk_widget_set_margin_top(scrolled_win,4);
    gtk_widget_set_margin_end(scrolled_win,4);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled_win),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);

    text_view=gtk_text_view_new();
    gtk_text_view_set_editable (GTK_TEXT_VIEW(text_view),FALSE);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW(text_view),15);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW(text_view),15);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW(text_view),GTK_WRAP_WORD_CHAR);  
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));  // Get the content (all of it) of the text_view widget
    mark = gtk_text_buffer_get_insert(buffer);  // Get the mark of the buffer, which is like a bookmark that keeps memory of the text position
    gtk_text_buffer_get_iter_at_mark(buffer,&iter,mark);
    gtk_text_buffer_insert(buffer,&iter," ",-1);   
    gtk_text_view_set_pixels_above_lines (GTK_TEXT_VIEW(text_view),2);
    gtk_text_view_set_pixels_below_lines (GTK_TEXT_VIEW(text_view),2);
    gtk_text_view_set_pixels_inside_wrap (GTK_TEXT_VIEW(text_view),1);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,5);
    gtk_box_set_homogeneous(GTK_BOX(vbox),FALSE);
    gtk_widget_set_margin_start (vbox, 15);
    gtk_widget_set_margin_end (vbox, (2*WINDOW_WIDTH)/3+15);
    gtk_widget_set_margin_top (vbox, WINDOW_HEIGHT/2+15);
    gtk_widget_set_margin_bottom (vbox, 15);
    gtk_box_pack_start(GTK_BOX(vbox),scrolled_win,TRUE,TRUE,0);
    gtk_box_pack_end(GTK_BOX(vbox),entry_chat,FALSE,TRUE,0);

	/*-------- Rules and game explanation widgets ---------*/
    rules_scrolled = gtk_scrolled_window_new(NULL,NULL);
    gtk_widget_set_margin_start(rules_scrolled,15);
    gtk_widget_set_margin_top(rules_scrolled,15);
    gtk_widget_set_margin_end(rules_scrolled,(2*WINDOW_WIDTH)/3+15);
    gtk_widget_set_margin_bottom(rules_scrolled, WINDOW_HEIGHT/2 + 15);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled_win),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);

    rules_text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(rules_text),FALSE);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW(rules_text),5);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW(rules_text),5);
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(rules_text));  // Get the content (all of it) of the text_view widget
    mark = gtk_text_buffer_get_insert(buffer);  // Get the mark of the buffer, which is like a bookmark that keeps memory of the text position
    gtk_text_buffer_get_iter_at_mark(buffer,&iter,mark);
    gtk_text_buffer_insert(buffer,&iter,rules,-1);  

	/*-------- Container Widget Tree, including the overlay --------*/
    gtk_container_add (GTK_CONTAINER(window),overlay);    
    gtk_container_add (GTK_CONTAINER(scrolled_win),text_view); 
    gtk_container_add (GTK_CONTAINER(rules_scrolled),rules_text);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay),vbox); 
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay),rules_scrolled);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay),status_bar);
    if(i_am_master == 1)
    	gtk_overlay_add_overlay (GTK_OVERLAY(overlay), addcard);
    gtk_overlay_add_overlay (GTK_OVERLAY(overlay), delcard);

	/*-------- Connections between widgets and functions, when an event occurs --------*/
	if(i_am_master == 1)
    	g_signal_connect (G_OBJECT(addcard),"clicked",G_CALLBACK(begin_first_turn),alow);
    g_signal_connect (G_OBJECT(delcard),"clicked",G_CALLBACK(delete_card),alow);
    g_signal_connect (G_OBJECT(entry_chat), "key_release_event",G_CALLBACK(send_data_entry),UI->socket);
    g_signal_connect (G_OBJECT(window),"destroy",G_CALLBACK(reenter_rooms),UI);

	/*-------- Playground and Card data, used through all the program --------*/
    alow->UI->entry_chat=entry_chat;
    alow->UI->text_view=text_view;  
    alow->overlay = overlay; 
    alow->UI->socket = UI->socket;
    alow->UI->end_turn = delcard;
    alow->app = UI->app;
    alow->status_bar = status_bar;

    reset_match(alow->match_data);

	/*-------- Widget style definition, loading an external .css file --------*/
    gtk_widget_set_name(GTK_WIDGET(vbox),"mytextchat");
    gtk_widget_set_name(GTK_WIDGET(window),"mywindow");
    gtk_style_context_add_class (gtk_widget_get_style_context (text_view), "myview");
    gtk_style_context_add_class (gtk_widget_get_style_context(rules_text), "rulesdata");

    g_source_set_callback(UI->source, (GSourceFunc)new_handler,alow,NULL); 

	/*-------- Initialization of the window and its children, focusing the cursor in the chat --------*/
    gtk_window_set_focus (GTK_WINDOW(window),entry_chat);
    gtk_widget_show_all(window);
	gtk_widget_destroy(UI->window_room);
	free(rules);
	return;
}