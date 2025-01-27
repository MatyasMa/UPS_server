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
    int is_connected;
    pthread_t thread;
};

extern pthread_mutex_t players_mutex;
extern struct player *players;


#define KEEP_ALIVE_INTERVAL 5       // Interval pro odesílání keep-alive zpráv (v sekundách)
#define KEEP_ALIVE_TIMEOUT 40       // Časový limit pro detekci výpadku (v sekundách)
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
