#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "game_UI.h"
#include "room_UI.h"

#define WINDOW_WIDTH 1136   // window fixed width
#define WINDOW_HEIGHT 640   // window fixed heigth
#define CARD_WIDTH 102      // card fixed width
#define CARD_HEIGHT 162     // card fixed heigth
#define MAX_BUFFER 1024

/*------------------------------------------------------------*/
/*-------- Global variables, used to check some stuff --------*/
/*------------------------------------------------------------*/
gint clicked_hand = 0;  
gint n_player_cards = 0;
gint n_dock_cards = 0;
gint add_to_dock = 0;
gint my_turn = 1;

/*-----------------------------------------------------------------*/
/*-------- Function to reset the main structure components --------*/
/*-----------------------------------------------------------------*/
void reset_values (change_alow* user_data, gint i){
    user_data->player_hand[i]->event_box = NULL;
    user_data->player_hand[i]->value = 0;
    user_data->player_hand[i]->seed = 0;
    user_data->position[i]->left = 0;
    user_data->position[i]->right = 0;
    user_data->position[i]->top = 0;
    user_data->position[i]->bottom = 0;
    user_data->position[i]->clicked = 0;
    user_data->position[i]->selected = 0;
    user_data->position[i]->value = 0;

    return;
}

/**/
gboolean sensitive (gpointer user_data){
    change_alow* data = (change_alow*) user_data;
    int i;
    gint sum_hand = 0;
    gint sum_dock = 0;
    gint temp[10]={0,0,0,0,0,0,0,0,0,0};
    int a,b,c,d,e;

    for(i=0;i<13;i++){
        if(data->position[i]->clicked == 1){
            if(i<3){
                sum_hand = sum_hand + data->player_hand[i]->value;
            }
            else{
                sum_dock = sum_dock + data->player_hand[i]->value;
            }
        }
    }

    if(sum_hand == 0){   // Well, the player didn't choose any card of his hand
        add_to_dock = 0;
        return FALSE;
    }

    if(sum_hand == sum_dock){
        add_to_dock = 0;
        return TRUE;
    }

    if(sum_dock == 0){
        for(i=3;i<13;i++){
            if(data->player_hand[i]->value == sum_hand){     // Easiest case: there is a card in the dock with the same value
                add_to_dock = 0;
                return FALSE;
            }
            temp[i-3] = data->player_hand[i]->value;    // Copy the dock value in a new clear array (easier to use)
        }

        for(a=0;a<9;a++){
            for(b=a+1;b<10;b++){
                if(temp[a]+temp[b]== sum_hand){
                    add_to_dock = 0;
                    return FALSE;
                }
                else
                    for(c=b+1;c<10;c++){
                        if(temp[a]+temp[b]+temp[c] == sum_hand){
                            add_to_dock = 0;
                            return FALSE;
                        }
                        else
                            for(d=c+1;d<10;d++){
                                if(temp[a]+temp[b]+temp[c]+temp[d]==sum_hand){
                                    add_to_dock = 0;
                                    return FALSE;
                                }
                                else
                                    for(e=d+1;e<10;e++){
                                        if(temp[a]+temp[b]+temp[c]+temp[d]+temp[e]==sum_hand){
                                            add_to_dock = 0;
                                            return FALSE;
                                        }
                                    }
                            }
                    }
            }
        }
        add_to_dock = 1;
        //g_print("\nDa aggiungere");
        return TRUE;
    }

    return FALSE;
}


/*--------------------------------------------------------------------------------------------------------*/
/*-------- Set the position of a player's card, deciding wheter is just for creation or selection --------*/
/*--------------------------------------------------------------------------------------------------------*/
void set_position (GtkWidget *box, GdkEvent *event, gpointer user_data){

    GtkWidget* image;
    image = gtk_bin_get_child(GTK_BIN(box));
    struct widget_position *position = (struct widget_position*) user_data;

    gtk_widget_set_margin_start (box, position->left);
    gtk_widget_set_margin_end (box, position->right);
    gtk_widget_set_margin_top (box, position->top);
    gtk_widget_set_margin_bottom (box, position->bottom);

    if(position->selected == 0){
        gtk_widget_set_margin_top (image, 20);
        gtk_widget_set_margin_bottom (image, 0);
    }
    else{
        gtk_widget_set_margin_top (image, 0);
        gtk_widget_set_margin_bottom (image, 20);
    }

    return;
}

/*--------------------------------------------------------------------------------------*/
/*-------- Called when the cursor enters or leaves a card (event box, actually) --------*/
/*--------------------------------------------------------------------------------------*/
void selected (GtkWidget *button, GdkEventCrossing *event, gpointer user_data){
    if(my_turn == 0)
        return;
    
    if(event->mode == GDK_CROSSING_NORMAL && clicked_hand == 0){
        struct widget_position *position = (struct widget_position*) user_data;

/*-------- Check if another card is clicked: if positive, disable all the selections --------*/
        if(event->type == GDK_ENTER_NOTIFY)
            position->selected = 1;
        else
            position->selected = 0;

        set_position (button, NULL, position);
    }

    return;
}

/*---------------------------------------------------------------------*/
/*-------- When i click the hand card, this function is called --------*/
/*---------------------------------------------------------------------*/
void fix_position_hand (GtkWidget *event_box, GdkEventButton *event, gpointer user_data){
    if(my_turn == 0)
        return;
    
    if((event->x<0 || event->x>102) || (event->y<0 || event->y > 182))
        return;

    if(event->button == 1){
        gint i;
        change_alow* data = (change_alow*) user_data;

/*-------- Check which of the 3 player's cards has been clicked --------*/
        for(i=0;i<3;i++)
            if(data->player_hand[i]->event_box == event_box)
                break;
        if(i==3)    // Just to avoid error, it should never be TRUE
            return;

/*-------- If the card is clicked and no other one is, then fix its position, otherwise --------*/
/*-------- means that some card has been already clicked. If that card is the one that ---------*/
/*-------- that has just been clicked, then un-click it. Otherwise (the user is trying ---------*/
/*-------- to have more than one card fixed), do nothing ---------------------------------------*/
        if(data->position[i]->clicked == 0 && clicked_hand == 0){
            clicked_hand = 1;
            data->position[i]->clicked = 1;
        } 
        else{
            if(data->position[i]->clicked == 1){
                clicked_hand = 0;
                data->position[i]->clicked = 0;  
            } 
        }
        if(sensitive(data) == TRUE)
            gtk_widget_set_sensitive(data->UI->end_turn,TRUE);
        else
            gtk_widget_set_sensitive(data->UI->end_turn,FALSE);
    }

    return;
}

/*--------------------------------------------------------------------------------------------------------*/
/*-------- Set the position of a player's card, deciding wheter is just for creation or selection --------*/
/*--------------------------------------------------------------------------------------------------------*/
void set_position_dock (GtkWidget *button, GdkEventCrossing *event, gpointer user_data){

    GtkWidget* image;
    image = gtk_bin_get_child(GTK_BIN(button));
    struct widget_position *position = (struct widget_position*) user_data;

    //if(position->clicked == 0){    
        gtk_widget_set_margin_start (button, position->left);
        gtk_widget_set_margin_end (button, position->right);
        gtk_widget_set_margin_top (button, position->top);
        gtk_widget_set_margin_bottom (button, position->bottom);  

        if(position->selected == 0){
            gtk_widget_set_margin_top (image, 10);
            gtk_widget_set_margin_bottom (image, 0);
        }
        else{
            gtk_widget_set_margin_top (image, 0);
            gtk_widget_set_margin_bottom (image, 10);
        }
    //}
    return;
}

/*--------------------------------------------------------------------------------------*/
/*-------- Called when the cursor enters or leaves a card (event box, actually) --------*/
/*--------------------------------------------------------------------------------------*/
void selected_dock (GtkWidget *button, GdkEventCrossing *event, gpointer user_data){

    if(my_turn == 0)
        return;
    struct widget_position *position = (struct widget_position*) user_data;

    if(event->mode == GDK_CROSSING_NORMAL && position->clicked == 0){

/*-------- Check wheter i am entering or leaving the event-box --------*/
        if(event->type == GDK_ENTER_NOTIFY)
            position->selected = 1;
        else
            position->selected = 0;

        //g_print("\nSelected: %d", position->selected);
        //g_print("\nY|X Crossing: %f|%f",event->y, event->x);
        set_position_dock(button,event,position);
    }
    return;
}

/*---------------------------------------------------------------------*/
/*-------- When i click the hand card, this function is called --------*/
/*---------------------------------------------------------------------*/
void fix_position_dock (GtkWidget *event_box, GdkEventButton *event, gpointer user_data){

    if(my_turn == 0)
        return;
    
    if((event->x<0 || event->x>102) || (event->y<0 || event->y > 172))
        return;

    if(event->button==1){

        change_alow *data = (change_alow*) user_data;
        gint i;

        for(i=3;i<13;i++)
            if(data->player_hand[i]->event_box == event_box)
                break;
        
        if(data->position[i]->selected == 1){    // Should always be TRUE, the cursor is surely inside
            data->position[i]->clicked = (data->position[i]->clicked+1)%2;
        }
        //g_print("\nClicked %d!",data->position[i]->clicked);
        if(sensitive(data))
            gtk_widget_set_sensitive(data->UI->end_turn,TRUE);
        else
            gtk_widget_set_sensitive(data->UI->end_turn,FALSE);
    }

    return;
}

/*----------------------------------------------------------------------------------------------*/
/*-------- Most important function: it calculates the card position, ready to be placed --------*/
/*----------------------------------------------------------------------------------------------*/
void initialize_reset_position (change_alow *user_data){
    gint i;
    gint counter_hand = 0;
    gint counter_dock = 0;
    gint temp_position = 0;
    change_alow* data = (change_alow*) user_data;

    for(i=0;i<(13);i++){
        if(data->player_hand[i]->event_box != NULL){

            if(i<3){
                data->position[i]->left = (WINDOW_WIDTH/3)+((2*WINDOW_WIDTH)/3- n_player_cards * CARD_WIDTH - ((n_player_cards-1)*CARD_WIDTH)/5)/2 + counter_hand*CARD_WIDTH + counter_hand*CARD_WIDTH/5;
                data->position[i]->right = WINDOW_WIDTH - data->position[i]->left - CARD_WIDTH;
                data->position[i]->bottom = 15;
                data->position[i]->top = WINDOW_HEIGHT - data->position[i]->bottom - CARD_HEIGHT-20; 
                
                set_position (data->player_hand[i]->event_box, NULL, data->position[i]);             
                counter_hand++;  

                g_signal_connect (G_OBJECT(data->player_hand[i]->event_box), "enter-notify-event", G_CALLBACK(selected), data->position[i]);
                g_signal_connect (G_OBJECT(data->player_hand[i]->event_box), "leave-notify-event", G_CALLBACK(selected), data->position[i]);
                g_signal_connect (G_OBJECT(data->player_hand[i]->event_box), "button-release-event", G_CALLBACK(fix_position_hand), data);
            } 
            else{
                if(counter_dock > 4)
                    temp_position = n_dock_cards - 5;
                else{
                    if(n_dock_cards == 10)
                        temp_position = 5;
                    else
                        temp_position = n_dock_cards - (n_dock_cards*(n_dock_cards/5))%5;
                }
                data->position[i]->left = (WINDOW_WIDTH/3)+((2*WINDOW_WIDTH)/3-(temp_position)*CARD_WIDTH-((temp_position-1)*CARD_WIDTH)/5)/2+(counter_dock%5)*CARD_WIDTH+(counter_dock%5)*CARD_WIDTH/5;
                data->position[i]->right = WINDOW_WIDTH - data->position[i]->left - CARD_WIDTH;
                data->position[i]->top = WINDOW_HEIGHT/10+(6*WINDOW_HEIGHT/10-(1+(n_dock_cards-1)/5)*(CARD_HEIGHT)-20)/2+(counter_dock)/5*(CARD_HEIGHT+20)-10;
                data->position[i]->bottom = WINDOW_HEIGHT - data->position[i]->top - CARD_HEIGHT;     

                set_position_dock (data->player_hand[i]->event_box, NULL, data->position[i]);               
                counter_dock++;

                g_signal_connect (G_OBJECT(data->player_hand[i]->event_box), "enter-notify-event", G_CALLBACK(selected_dock), data->position[i]);
                g_signal_connect (G_OBJECT(data->player_hand[i]->event_box), "leave-notify-event", G_CALLBACK(selected_dock), data->position[i]);
                g_signal_connect (G_OBJECT(data->player_hand[i]->event_box), "button-release-event", G_CALLBACK(fix_position_dock), data);
            }       
            gtk_widget_show_all(data->player_hand[i]->event_box);
        }
    }
    return;
}

/*-----------------------------------------------------------------------------*/
/*-------- Select the image of the card considering its value and seed --------*/
/*-----------------------------------------------------------------------------*/
void select_image (GtkWidget** widget, gint value, gint seed){

    char *im_loader = (char *) g_malloc (30*sizeof(char));
    char *seed_name = (char *) g_malloc (8*sizeof(char));
    gint i;

    for(i=0;i<30;i++)
        im_loader[i]='\0';
    for(i=0;i<8;i++)
        seed_name[i]='\0';

    /*-------- Seed is actually a number, based on the order of the deck seeds. Numbers from 1 to 10 --------*/
    /*-------- are cups, from 11 to 20 swords, from 21 to 30 golds and from 31 to 40 clubs ------------------*/
    switch(seed){
        case 0:
            sprintf(seed_name,"coppe");
            break;
        case 1:
            sprintf(seed_name, "spade");
            break;
        case 2:
            sprintf(seed_name, "denari");
            break;
        case 3:
            sprintf(seed_name, "bastoni");
            break;
    }

    sprintf(im_loader,"Carte/CarteFin/%d_%s.png",value,seed_name);
    *widget = gtk_image_new_from_file(im_loader);

    g_free(im_loader);
    g_free (seed_name);    
    return;
}

/*---------------------------------------------------------------------------------------------------*/
/*-------- Called whenever there is the need to add a card at the player hand or at the dock --------*/
/*---------------------------------------------------------------------------------------------------*/
void add_card (GtkWidget* widget, change_alow *user_data, int value){  

    /*-------- Declaration and casting of variables --------*/
    gint i;
    gint seed;
    gint counter;
    GtkWidget *event_box;
    GtkWidget *image;
    change_alow* data = (change_alow*) user_data;

    /*-------- Create the new box and set its events --------*/
    event_box = gtk_event_box_new();
    gtk_widget_set_events (event_box, GDK_ENTER_NOTIFY_MASK);
    gtk_widget_set_events (event_box, GDK_LEAVE_NOTIFY_MASK);
    gtk_widget_set_events (event_box, GDK_BUTTON_RELEASE_MASK);

    /*-------- Where should the new card be added? This cond. branch controls --------*/
    /*-------- if the card has to be added to the dock or the player's hand ----------*/
    if(add_to_dock == 1){
        counter = 3;
        add_to_dock = 0;
    }
    else
        counter = 0;

    /*-------- Search for the first space available and put the new element in it --------*/
    for(i=counter;i<13;i++){

        if(data->player_hand[i]->event_box == NULL){

            data->player_hand[i]->event_box = event_box;
            data->position[i]->value = value;
            if(value%10 == 0)
                data->player_hand[i]->value = 10;
            else
                data->player_hand[i]->value = value%10;

            seed = (value-1)/10;
            data->player_hand[i]->seed = seed;
            select_image(&image,data->player_hand[i]->value,seed);
            gtk_container_add (GTK_CONTAINER(event_box), image);
            gtk_overlay_add_overlay (GTK_OVERLAY(data->overlay), event_box);
            if(i<3)
                n_player_cards++;
            else
                n_dock_cards++;
            initialize_reset_position(data);
            return;
        }
    }
    return;
}

/*----------------------------------------------------------------------------*/
/*-------- Function that calculates primiera each time a move is done --------*/
/*----------------------------------------------------------------------------*/
gint calculate_primiera (gint value){
    gint primiera;
    switch(value){
        case 1:
            primiera = 16;
            break;
        case 2:
            primiera = 12;
            break;
        case 3:
            primiera = 13;
            break;
        case 4:
            primiera = 14;
            break;
        case 5:
            primiera = 15;
            break;
        case 6:
            primiera = 18;
            break;
        case 7:
            primiera = 21;
            break;
        default:
            primiera = 10;
            break;       
    }
    return primiera;
}

/*-------------------------------------------------------------------------------------------------*/
/*-------- Function called to end the sub-turn: the player has selected and clicked a card --------*/
/*-------------------------------------------------------------------------------------------------*/
void delete_card (GtkWidget* widget, change_alow *user_data){

    /*-------- Usual declaration and casting of utility and support variables --------*/
    gint i, secure;
    gint value;
    change_alow* data = (change_alow*) user_data;
    char *buffer = (char *) g_malloc (20*sizeof(char));
    clear_buffer(buffer,20);

    gtk_widget_set_sensitive(data->UI->end_turn,FALSE);
    my_turn = 0;
    clicked_hand = 0;

    /*-------- Get the value of the clicked cards of the dock (sum) and the value of the clicked card of the player (only one) --------*/
    for(i=0;i<13;i++){
        if(data->position[i]->clicked == 1){
            if(i<3)
                secure = i;
        }
    }

    /*-------- The card has to be added to the dock or the player is capturing other cars? --------*/
    if(add_to_dock == 1){

        add_card(NULL,data,data->position[secure]->value);
        gtk_widget_destroy(data->player_hand[secure]->event_box);
        value = data->position[secure]->value;
        reset_values(data,secure);
        n_player_cards--;
        initialize_reset_position(data);
        sprintf(buffer,"A_%d",value);
        g_print("\nAdd to dock, value %d",value);
        g_socket_send (data->UI->socket,buffer, strlen(buffer), NULL, NULL);
        gtk_label_set_text(GTK_LABEL(data->status_bar),"@@@     OPPONENT'S TURN     @@@");
        add_to_dock = 0;

        free(buffer);
        return;
    }
    else{

        for(i=0;i<13;i++){

            if (data->player_hand[i]->event_box != NULL && data->position[i]->clicked == 1){    // Delete all the clicked cards 

                // Player score of the match
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

                gtk_widget_destroy(data->player_hand[i]->event_box);
                value = data->position[i]->value;
                reset_values(data,i);    
                g_print("\nMy cards: %d",data->match_data->cards_number);
                if(i<3)
                    n_player_cards--;
                else
                    n_dock_cards--; 

                initialize_reset_position(data);

                if(i>2){
                    sprintf(buffer,"D_%d",value);
                    g_socket_send (data->UI->socket,buffer, strlen(buffer), NULL, NULL);
                    break;
                }
            }
        }    
    }

    free(buffer);
    return;
}

/*---------------------------------------------------------------------------*/
/*-------- Function that sets the rules of the game in the text view --------*/
/*---------------------------------------------------------------------------*/
void set_rules(char* rules){
    sprintf(rules,"\n    Il giocatore ricevente per primo dal mazziere dovrà lanciare\n"
                    "    a terra una delle carte in proprio possesso. Calando una\n"
                    "    carta dello stesso valore di una presente a terra, quel \n"
                    "    giocatore prenderà entrambe le carte, riponendole di fronte \n"
                    "    a sé nell'area dedicata alle carte  conquistate. E' anche  \n"
                    "    possibile calare una carta e prendere da terra due o più \n"
                    "    carte il cui valore sommato uguaglia il valore della carta\n"
                    "    lanciata.\n\n"
                    "    Quando a terra non rimane alcuna carta, a seguito di una\n"
                    "    presa, si dice che si è fatto 'scopa'. La scopa non può\n"
                    "    essere eseguita durante l'ultima mano dell'ultimo giocatore\n"
                    "    e quindi prima dell'esaurimento delle carte della smazzata\n"
                    "    in corso. Al termine del lancio delle tre carte dei\n"
                    "    giocatori, il mazziere distribuisce nuovamente tre carte a\n"
                    "    testa, senza riporre nuovamente le quattro carte a terra,\n"
                    "    cosa che avviene solo nella prima mano di gioco.\n\n"
                    "    CALCOLO DEL PUNTEGGIO:\n"
                    "    Quando non rimangono più carte in mano ai giocatori, viene\n"
                    "    assegnato il punteggio secondo le seguenti regole:\n"
                    "    1) Ogni scopa totalizzata vale un punto\n"
                    "    2) Il giocatore che ha conquistato il 7 di denari riceve\n"
                    "       un punto\n"
                    "    3) Il giocatore che ha conquistato più di 20 carte\n"
                    "       guadagna un punto\n"
                    "    4) Il giocatore che ha conquistato più di 5 carte del\n"
                    "       seme di denari guadagna un punto\n"
                    "    5) Primiera: il giocatore che effettua il punteggio\n"
                    "       più alto guadagna un punto. Si calcola nel seguente\n"
                    "       modo: si deve possedere almeno una carta per seme\n"
                    "       (in caso contrario la primiera è data all'avversario).\n"
                    "       Il valore attribuito ad ogni carta è: il 7 vale 21\n"
                    "       punti, il 6 vale 18 punti, l'Asso vale 16 punti, il 5\n"
                    "       vale 15 punti, il 4 vale 14 punti, il 3 vale 13 punti,\n"
                    "       il 2 vale 12 punti e le figure ne valgono 10. Si somma\n"
                    "       il valore più alto per ogni seme.\n"
                    "    6) In caso di parità su ognuno dei precedenti punti, non\n"
                    "       verrà assegnato alcun punteggio.\n\n"
                    "    L'obiettivo del gioco è totalizzare un punteggio maggiore\n"
                    "    dell'avversario.\n");
    return;
}

/*--------------------------------------------------------------------------------*/
/*-------- Reset the match data, used the first time and at every rematch --------*/
/*--------------------------------------------------------------------------------*/
void reset_match (gpointer user_data){
    struct match_data *match_data = (struct match_data *) user_data;
    gint i;

    match_data->sweep_number = 0;
    match_data->cards_number = 0;
    match_data->gold_number = 0;
    match_data->settebello = FALSE;
    for(i=0;i<4;i++)
        match_data->higher_per_seed[i] = 0;
    match_data->last_player = -1;
    match_data->score = 0;
    match_data->winner = FALSE;

    return;
}

void set_up (GtkWidget *check_button, GdkEvent* event, gpointer user_data){
    gtk_widget_set_state_flags(check_button,GTK_STATE_FLAG_CHECKED,TRUE);
    return;
}

void send_rematch (GtkWidget *button, gpointer user_data){
    change_alow *data = (change_alow *) user_data;
    gtk_widget_set_sensitive(button,FALSE);
    g_socket_send(data->UI->socket,"#rematch#",strlen("#rematch#"),NULL,NULL);
    return;
}

/*--------------------------------------------------*/
/*-------- Creates the match results widget --------*/
/*--------------------------------------------------*/
void create_results(gpointer user_data, gint opp_score){

    change_alow *data = (change_alow *) user_data;

    GdkGeometry hints;
    GtkWidget *huge_box;

    GtkWidget *win_status_image;
    GtkWidget *small_box[4];
    GtkWidget *label[5];
    GtkWidget *check_button[4];
    GtkWidget *score;
    GtkWidget *rematch;

    gint i = 0;
    gint prim = 0;
    char *buffer = (char*) g_malloc(40*sizeof(char));

    if(data->match_data->score > opp_score){
        win_status_image = gtk_image_new_from_file("rsz_win.png");
    }
    else{
        win_status_image = gtk_image_new_from_file("rsz_looser.png");
    }
    clear_buffer(buffer,40);
    sprintf(buffer,"TOTAL SCORE: %d", data->match_data->score);
    score = gtk_label_new(buffer);
    gtk_widget_set_name(score,"myscore");
    rematch = gtk_button_new_with_label("Rematch?");

    for(i=0;i<5;i++){
        switch(i){
            case 0: 
                clear_buffer(buffer,40);
                sprintf(buffer,"CAPTURED CARDS: %d",data->match_data->cards_number);
                label[i]= gtk_label_new(buffer);
                check_button[i] = gtk_check_button_new_with_label("WIN (>20)"); 
                if(data->match_data->cards_number > 20){
                    gtk_widget_set_state_flags(check_button[i],GTK_STATE_FLAG_CHECKED,TRUE);
                    g_signal_connect(check_button[i],"toggled",G_CALLBACK(set_up),NULL);
                }
                else
                    gtk_widget_set_state_flags(check_button[i],GTK_STATE_FLAG_INSENSITIVE,TRUE);
                break;
            case 1:
                clear_buffer(buffer,40);
                sprintf(buffer,"GOLDS NUMBER:     %d  ",data->match_data->gold_number);
                label[i] = gtk_label_new(buffer);
                check_button[i] = gtk_check_button_new_with_label("WIN (>5)");
                if(data->match_data->gold_number > 5){
                    gtk_widget_set_state_flags(check_button[i],GTK_STATE_FLAG_CHECKED,TRUE);
                    g_signal_connect(check_button[i],"toggled",G_CALLBACK(set_up),NULL);
                }
                else
                    gtk_widget_set_state_flags(check_button[i],GTK_STATE_FLAG_INSENSITIVE,TRUE);
                break;
            case 2:
                clear_buffer(buffer,40);
                sprintf(buffer,"SETTEBELLO:                ");
                label[i] = gtk_label_new(buffer);
                check_button[i] = gtk_check_button_new_with_label("GOT IT!");
                if(data->match_data->settebello == TRUE){
                    gtk_widget_set_state_flags(check_button[i],GTK_STATE_FLAG_CHECKED,TRUE);
                    g_signal_connect(check_button[i],"toggled",G_CALLBACK(set_up),NULL);
                }
                else
                    gtk_widget_set_state_flags(check_button[i],GTK_STATE_FLAG_INSENSITIVE,TRUE);
                break;
            case 3:
                clear_buffer(buffer,40);
                sprintf(buffer,"SWEEP NUMBER: %d",data->match_data->sweep_number);
                label[i] = gtk_label_new(buffer);
                gtk_widget_set_name(label[i],"score_label");
                break;
            case 4:
                for(i=0;i<4;i++)
                    prim = prim + data->match_data->higher_per_seed[i];
                clear_buffer(buffer,40);
                sprintf(buffer,"PRIMIERA: %d",prim);
                label[i] = gtk_label_new(buffer);       
                check_button[i-1] = gtk_check_button_new_with_label("MAX SCORE");
                if(data->match_data->primiera == TRUE){
                    gtk_widget_set_state_flags(check_button[i-1],GTK_STATE_FLAG_CHECKED,TRUE);
                    g_signal_connect(check_button[i-1],"toggled",G_CALLBACK(set_up),NULL);
                }
                else
                    gtk_widget_set_state_flags(check_button[i-1],GTK_STATE_FLAG_INSENSITIVE,TRUE);
                gtk_widget_set_name(label[i-1],"score_label");
                break;
            default:
                break;
        }

        if(i != 3){
            if(i == 4){
                small_box[i-1] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
                gtk_box_pack_start(GTK_BOX(small_box[i-1]),label[i],FALSE,TRUE,0);
                gtk_box_pack_end(GTK_BOX(small_box[i-1]),check_button[i-1],TRUE,TRUE,0); 
                gtk_box_set_spacing(GTK_BOX(small_box[i-1]),30);
                gtk_widget_set_name(small_box[i-1],"mysmallbox");
                gtk_widget_set_margin_top(check_button[i-1],5);
                gtk_widget_set_margin_bottom(check_button[i-1],5);
                gtk_widget_set_margin_start(small_box[i-1],5);
                gtk_widget_set_margin_end(small_box[i-1],5);
            }
            else{
                small_box[i] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
                gtk_box_pack_start(GTK_BOX(small_box[i]),label[i],FALSE,TRUE,0);
                gtk_box_pack_end(GTK_BOX(small_box[i]),check_button[i],TRUE,TRUE,0); 
                gtk_box_set_spacing(GTK_BOX(small_box[i]),30); 
                gtk_widget_set_name(small_box[i],"mysmallbox");
                gtk_widget_set_margin_top(check_button[i],5);
                gtk_widget_set_margin_bottom(check_button[i],5);
                gtk_widget_set_margin_start(small_box[i],5);
                gtk_widget_set_margin_end(small_box[i],5);
            }

            gtk_widget_set_margin_start(label[i],10);
            gtk_widget_set_margin_top(label[i],5);
            gtk_widget_set_margin_bottom(label[i],5);
        }
    }

    huge_box = gtk_box_new(GTK_ORIENTATION_VERTICAL,5);
    gtk_widget_set_name(huge_box,"result_box");
    data->result_box = huge_box;

    gtk_box_pack_start(GTK_BOX(huge_box),win_status_image,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(huge_box),small_box[0],FALSE,FALSE,0); 
    gtk_box_pack_start(GTK_BOX(huge_box),small_box[1],FALSE,FALSE,0); 
    gtk_box_pack_start(GTK_BOX(huge_box),small_box[2],FALSE,FALSE,0); 
    gtk_box_pack_start(GTK_BOX(huge_box),label[3],FALSE,FALSE,0);
    gtk_widget_set_margin_start(label[3],5);
    gtk_widget_set_margin_end(label[3],5);
    gtk_box_pack_start(GTK_BOX(huge_box),small_box[3],FALSE,FALSE,0); 
    gtk_box_pack_start(GTK_BOX(huge_box),score,FALSE,FALSE,5);
    gtk_box_pack_start(GTK_BOX(huge_box),rematch,TRUE,TRUE,5);
    gtk_widget_set_margin_start(rematch,5);
    gtk_widget_set_margin_end(rematch,5);

    gtk_widget_set_margin_top (huge_box,140);
    gtk_widget_set_margin_bottom (huge_box,100);
    gtk_widget_set_margin_start (huge_box,570);
    gtk_widget_set_margin_end (huge_box,191);
    g_signal_connect(G_OBJECT(rematch),"clicked",G_CALLBACK(send_rematch),data);

    gtk_overlay_add_overlay(GTK_OVERLAY(data->overlay),huge_box);
    gtk_widget_show_all(huge_box);

    return;
}