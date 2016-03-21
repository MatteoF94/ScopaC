#ifndef _UI_LIB
#define _UI_LIB

#include <gtk/gtk.h>

struct UI_game{
    GtkWidget *window;
    GtkWidget *end_turn;
    GtkWidget* text_view;
    GtkWidget* entry_chat;
    GSocket* socket;  
};

struct widget_position{
    gint left;
    gint right;
    gint top;
    gint bottom;
    gint clicked;
    gint selected;
    gint value;
};

struct card_data{
    GtkWidget *event_box;
    gint value;
    gint seed;
};

struct match_data{
    gint sweep_number;
    gint cards_number;
    gboolean settebello;
    gint gold_number;
    gint higher_per_seed[4];
    gboolean primiera;
    gint last_player;
    gint score;
    gboolean winner;
};

typedef struct change_al{
    GtkApplication *app;
    struct card_data *player_hand[13];
    struct widget_position *position[13];
    struct UI_game *UI;
    struct match_data *match_data;
    GtkWidget *overlay;
    GtkWidget *status_bar;
    GtkWidget *result_box;
} change_alow;

void reset_values (change_alow* user_data, gint i);
gboolean sensitive (gpointer user_data);
void set_position (GtkWidget *button, GdkEvent *event, gpointer user_data);
void selected (GtkWidget *button, GdkEventCrossing *event, gpointer user_data);
void fix_position_hand (GtkWidget *event_box, GdkEventButton *event, gpointer user_data);
void set_position_dock (GtkWidget *button, GdkEventCrossing *event, gpointer user_data);
void selected_dock (GtkWidget *button, GdkEventCrossing *event, gpointer user_data);
void fix_position_dock (GtkWidget *event_box, GdkEventButton *event, gpointer user_data);
void initialize_reset_position (change_alow *user_data);
void select_image (GtkWidget** widget, gint value, gint seed);
void add_card (GtkWidget* widget,change_alow *user_data, int value);
gint calculate_primiera (gint value);
void delete_card (GtkWidget* widget,change_alow *user_data);
void set_rules(char* rules);
void reset_match (gpointer user_data);
void create_results (gpointer user_data, gint opp_score);

#endif