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

#define MAX_PLAYERS 10
#define MAX_PLAYERS_IN_GAME 2
#define MAX_GAMES 5
#define MAX_HANDS_PLAY 2

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
    int is_connected;
    int is_created;
    pthread_t thread;
};

struct session {
    struct player *players[MAX_PLAYERS_IN_GAME];
    int is_active;
    int is_full;
};

struct session* find_first_useful_game();
struct session* find_clients_session(int player_id);

extern pthread_mutex_t players_mutex;
extern struct player *players;
extern struct session *games;



#define KEEP_ALIVE_INTERVAL 1       // Interval pro odesílání keep-alive zpráv (v sekundách)
#define KEEP_ALIVE_TIMEOUT 10       // Časový limit pro detekci výpadku (v sekundách)
#define TRY_RECONNECT_TIME 120
struct client_status {
    int last_response_time;
    int is_connected;
};
extern struct client_status client_status[];



void* handle_client(void* arg);
void start_clients(void);
void handle_sigchld(void);

void handle_disconnect(int player_id);

void get_first_cards(int player_id);

#endif
