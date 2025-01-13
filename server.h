#ifndef SERVER_H
#define SERVER_H

#define MAX_PLAYERS 2

typedef struct player {
    int id;
    char *nickname;
    int is_ready;
    int is_ready_to_play_hand;
    int can_play;
    int socket_fd;
    int has_first_cards;
    int loses_hand;
    int win_game;
    int loses_game;
    pthread_t thread;
};

int create_shared_memory();
int check_ready_of_players();
int check_ready_to_play_hand_of_players();
void broadcast_message(const char *message);

void player_hit(int player_id);
void croupier_hit();
char get_random_card();
void hide_players_buttons(int player_id);
void show_players_buttons(int player_id);
void start_croupier_play(int player_id);
void unready_to_play_hand_players();
void start_clients();

#endif