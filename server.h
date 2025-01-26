#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <pthread.h>

#define MAX_PLAYERS 2
#define MAX_HANDS_PLAY 1

struct player {
    int id;
    char *nickname;
    int is_ready;
    int is_ready_to_play_hand;
    int can_play;
    int socket_fd;
    int has_first_cards;
    int loses_hand;
    int hands_played;
    int balance;
    pthread_t thread;
};

extern pthread_mutex_t players_mutex;
extern struct player *players;


// int create_shared_memory();
// /**
//  * @brief Ready to play game
//  * 
//  * @return int 
//  */
// int check_ready_of_players();
// int check_ready_to_play_hand_of_players();
// void broadcast_message(const char *message);

// void player_hit(int player_id);
// void croupier_hit();
// char get_random_card();
// void hide_players_buttons(int player_id);
// void show_players_buttons(int player_id);
// void start_croupier_play(int player_id);
// void unready_to_play_hand_players();
// void send_message_to_player(int id_player, const char *message);
// void clear_players_data();

void* handle_client(void* arg);
void start_clients(void);
void handle_sigchld(void);

#endif
