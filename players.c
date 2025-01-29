#include "server.h"
#include "game_control.h"
#include "players.h"
#include "sender.h"

int shmid; 
pthread_mutex_t players_mutex = PTHREAD_MUTEX_INITIALIZER;
struct player *players;
struct session *games;

int create_shared_memory(void) {
    shmid = shmget(IPC_PRIVATE, sizeof(struct player) * (MAX_PLAYERS), IPC_CREAT | 0666); 
    if (shmid == -1) {
        perror("shmget failed");
        return -1;
    }

    // Attach the shared memory segment
    players = (struct player *)shmat(shmid, NULL, 0); 
    if (players == (void *)-1) {
        perror("shmat failed");
        return -1;
    }


    // Inicializace hráčských dat
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        players[i].id = 0;
        players[i].is_ready = 0;
        players[i].is_ready_to_play_hand = 0;
        players[i].can_play = 0;
        players[i].nickname = malloc(50 * sizeof(char));
        if (!players[i].nickname) {
            free(players);  // Uvolnění již alokované paměti
            return -1;
        }
        players[i].hands_played = 0;
        players[i].balance = -1;
    }

    games = malloc(MAX_GAMES * sizeof(struct session));
    if (!games) {
        exit(EXIT_FAILURE);
    }
    for (int j = 0; j < MAX_GAMES; ++j) {
        for (int k = 0; k < MAX_PLAYERS_IN_GAME; ++k) {
            games[j].players[k] = NULL;
        }        
        games[j].is_active = 0;
        games[j].is_full = 0;        
    }

    return 0;
}


int check_ready_of_players(struct session* curr_sess) {
    int all_ready = 1;
    // pthread_mutex_lock(&players_mutex);
    //     for (int i = 0; i < MAX_PLAYERS; ++i) {
    //         if (!players[i].id || !players[i].is_ready) {
    //             all_ready = 0;
    //         }
    //     }
    // pthread_mutex_unlock(&players_mutex);

    pthread_mutex_lock(&players_mutex);
        for (int i = 0; i < MAX_PLAYERS_IN_GAME; ++i) {
            if (curr_sess->players[i]) {
                if (!curr_sess->players[i]->is_ready) {
                    all_ready = 0;
                }
            } else {
                all_ready = 0;
            }            
        }
    pthread_mutex_unlock(&players_mutex);
    return all_ready;
}

int check_ready_to_play_hand_of_players(struct session* curr_sess) {
    int all_ready_to_play_hand = 1;
    // pthread_mutex_lock(&players_mutex);

    //     for (int i = 0; i < MAX_PLAYERS; ++i) {
    //         if (!players[i].id || !players[i].is_ready_to_play_hand) {
    //             all_ready_to_play_hand = 0;
    //         }
    //     }

    // pthread_mutex_unlock(&players_mutex);
    // return all_ready_to_play_hand;
    pthread_mutex_lock(&players_mutex);

        for (int i = 0; i < MAX_PLAYERS_IN_GAME; ++i) {
            if (curr_sess->players[i]) {
                if (!curr_sess->players[i]->is_ready_to_play_hand) {
                    all_ready_to_play_hand = 0;
                }
            } else {
                all_ready_to_play_hand = 0;
            }
            
        }

    pthread_mutex_unlock(&players_mutex);
    return all_ready_to_play_hand;
}



void player_hit(int player_id, struct session* curr_sess) {
    pthread_mutex_lock(&players_mutex);
    
    sleep(1);
    char mess[20];
    char random_card = get_random_card();
    // sprintf(mess, "player_hit:%c_%c;", player_id + 1, random_card);
    sprintf(mess, "player_hit:%d_%c;", (player_id + 1), random_card);
    // sprintf(mess, mess, random_card);
    printf("odesílám zprávu hitnutí: %s\n", mess);

    broadcast_message(mess, curr_sess);
    // if (send(players[player_id].socket_fd, mess, strlen(mess), 0) < 0) {
    //     perror("send failed");
    // }

    pthread_mutex_unlock(&players_mutex);
}


void hide_players_buttons(int player_position, struct session* curr_sess) {
    char *message = "hide_play_buttons;";
    // if (send(players[player_id].socket_fd, message, strlen(message), 0) < 0) {
    //     perror("send failed");
    // }
    if (send(curr_sess->players[player_position]->socket_fd, message, strlen(message), 0) < 0) {
        perror("send failed");
    }
}

void show_players_buttons(int player_position, struct session* curr_sess) {
    char *message = "show_play_buttons;";
    // if (send(players[player_id].socket_fd, message, strlen(message), 0) < 0) {
    //     perror("send failed");
    // }
    
    if (send(curr_sess->players[player_position]->socket_fd, message, strlen(message), 0) < 0) {
        perror("send failed");
    }
}

void unready_to_play_hand_players(void) {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        players[i].has_first_cards = 0;
        players[i].is_ready_to_play_hand = 0;
        players[i].loses_hand = 0;
    }
}

// kromě id
void clear_players_data(void) {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        players[i].is_ready = 0;
        players[i].is_ready_to_play_hand = 0;
        players[i].can_play = 0;
        players[i].hands_played = 0;
        players[i].balance = -1;
    }
}

int is_some_player_disconnected(void) {
    int i;
    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (!players[i].is_connected && players[i].is_created) {
            return i;
        }
    }
    return -1;
}
