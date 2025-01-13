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

#include "server.h"

#define PORT 8080

// Shared memory segment for player data
struct player *players;
int all_players_ready;
int players_count = 0;
pthread_mutex_t players_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int current_player_id = 0;

// Signal handler to reap zombie processes
void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

// Handle client interaction
void* handle_client(void* arg) {
    int player_id = *(int*)arg;
    char buffer[1024] = {0};
    int bytes_read;
    int i;

    free(arg);  // Free the dynamically allocated player_id memory

    while (1) {
        memset(buffer, 0, sizeof(buffer)); // Clear buffer
        printf("Waiting for message from client %d...\n", players[player_id].id);
        bytes_read = recv(players[player_id].socket_fd, buffer, sizeof(buffer), 0);

        if (bytes_read <= 0) {
            // Client disconnected or error
            printf("Client %d disconnected.\n", players[player_id].id);
            // TODO: vynulovat odhlášeného hráče a aby se na přihlášení nenastavil po přihlášení zpět hráč na stejnou pozici 
            --players_count;
            close(players[player_id].socket_fd);
            pthread_exit(NULL);  // Properly exit the thread
        }

        buffer[bytes_read] = '\0';
        printf("buffer: %s\n",buffer);

        if (!strncmp(buffer, "ready:", 6)) {
            char nick[50];
            if (sscanf(buffer, "ready:%s", &nick) == 1) {
                players[player_id].nickname = nick;
            }
            printf("Client %d - %s is ready\n", players[player_id].id, players[player_id].nickname);
            players[player_id].is_ready = 1;

            // Echo the message back to the client
            char mess[256];
            sprintf(mess, "Ready uživatele %d bylo přijmuto\n", players[player_id].id);
            send(players[player_id].socket_fd, mess, strlen(mess), 0);

            if (check_ready_of_players()) {
                start_clients();
                // broadcast_message("start_game\n");
            }

        } else if (!strcmp(buffer, "exit")) {
            printf("Client %d requested exit.\n", players[player_id].id);
            close(players[player_id].socket_fd);
            pthread_exit(NULL);

        } else if (!strcmp(buffer, "ready_to_play_hand")){
  
            players[player_id].is_ready_to_play_hand = 1;

            if (check_ready_to_play_hand_of_players()) {
                // TODO: send croupier card and players cards
                

            
                // get first card for croupier and send message for players to ask for cards
                broadcast_message("ask_for_first_cards;");
                pthread_mutex_lock(&players_mutex);
                sleep(1);
                croupier_hit();
                players[0].can_play = 1;
                pthread_mutex_unlock(&players_mutex);
            }

        } else if (!strcmp(buffer, "get_first_cards")) {


            hide_players_buttons(player_id);

            // Čekání na správné ID hráče
            while (player_id != current_player_id && !players[player_id-1].has_first_cards) {
                sleep(1);
            }

            // Kritická sekce: hráč provede své akce
            player_hit(player_id);
            player_hit(player_id);

            players[current_player_id].has_first_cards = 1;
            current_player_id++;
            
            if (players[player_id].id == MAX_PLAYERS) {
                current_player_id = 0;
                show_players_buttons(0);
            }
            
        } else if (!strcmp(buffer, "player_get_hit")) {

            if (players[player_id].can_play) {
                player_hit(player_id);
            } else {
                printf("Momentálně nejste na řadě.\n");
            }

            
        } else if (!strncmp(buffer, "player_stand:", 13)) {
            // player_stand:C ... continue
            // player_stand:L ... lost
            char result;
            // Parsování znaku po "player_stand:"
            if (sscanf(buffer, "player_stand:%c", &result) == 1) {
                if (result == 'L') {
                    players[player_id].loses_hand = 1;
                }
                printf("hrac %d prohral: to many\n",player_id);
            } else {
                printf("Failed to extract character.\n");
            }

            if (player_id == 0) {
                // TODO: can_play asi odstranit
                players[0].can_play = 0;
                hide_players_buttons(0);
                players[1].can_play = 1;
                show_players_buttons(1);
            } else if (player_id == 1) {
                players[1].can_play = 0;
                hide_players_buttons(1);

                // TODO: udělané natvrdo, chtělo by obecně
                if (players[0].loses_hand && players[1].loses_hand) {
                    broadcast_message("hand_ended");
                } else {
                    // Posílám libovolném uživateli
                    start_croupier_play(1);
                }
            }
        } else if (!strcmp(buffer, "croupier_get_hit")) {
            // pthread_mutex_lock(&players_mutex);
            croupier_hit();
            // sleep(1);
            // pthread_mutex_unlock(&players_mutex);
        } else if (!strcmp(buffer, "hand_end")) {
            unready_to_play_hand_players();
        } 
        // else if (!strcmp(buffer, "game_over")) {

        //     // TODO: nefunguje
        //     players[player_id].loses_game = 1;


        //     if ((player_id == 0 && players[1].loses_game) || (player_id == 1 && players[0].loses_game)) {
        //         broadcast_message("lose");
        //     } else {
        //         if (send(players[player_id].socket_fd, "lose", strlen("lose"), 0) < 0) {
        //             perror("send failed");
        //         }
        //         int winner = player_id == 1 ? 0 : 1;
        //         if (send(players[winner].socket_fd, "win", strlen("win"), 0) < 0) {
        //             perror("send failed");
        //         }
        //     }

        //     printf("starting new game\n");
            
        //     // TODO: new game
        // } else if (!strcmp(buffer, "game_win")) {

        //     // TODO: nefunguje
        //     // TODO: broadcast, to close and start new game - new window
        //     players[player_id].win_game = 1;

        //     if ((player_id == 0 && players[1].win_game) || (player_id == 1 && players[0].win_game)) {
        //         broadcast_message("draw");
        //     } else {
        //         if (send(players[player_id].socket_fd, "win", strlen("win"), 0) < 0) {
        //             perror("send failed");
        //         }
        //         int loser = player_id == 1 ? 0 : 1;
        //         if (send(players[loser].socket_fd, "lose", strlen("lose"), 0) < 0) {
        //             perror("send failed");
        //         }
        //     }

        //     printf("starting new game\n");

        //     // TODO: new game
        // }

        
    }
}


void start_clients() {
    char message[50]; // Allocate a fixed-size buffer
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players[i].id) { // If the player exists and has a valid id
            sprintf(message, "start_game:%d;", players[i].id); // Format the message
            if (send(players[i].socket_fd, message, strlen(message), 0) < 0) {
                perror("send failed");
            }
        }
    }
}


void get_first_players_cards() {
    // for (int i = 0; i < MAX_PLAYERS; ++i) {
    //     player_hit(i);
    //     player_hit(i);
    // }
}

void unready_to_play_hand_players() {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        players[i].has_first_cards = 0;
        players[i].is_ready_to_play_hand = 0;
        players[i].loses_hand = 0;
    }
}

void start_croupier_play(int player_id) {
    char *message = "start_croupier_play;";
    if (send(players[player_id].socket_fd, message, strlen(message), 0) < 0) {
        perror("send failed");
    }
}

void hide_players_buttons(int player_id) {
    char *message = "hide_play_buttons;";
    if (send(players[player_id].socket_fd, message, strlen(message), 0) < 0) {
        perror("send failed");
    }
}

void show_players_buttons(int player_id) {
    char *message = "show_play_buttons;";
    if (send(players[player_id].socket_fd, message, strlen(message), 0) < 0) {
        perror("send failed");
    }
}

void player_hit(int player_id) {
    pthread_mutex_lock(&players_mutex);
    
    sleep(1);
    char mess[20];
    char random_card = get_random_card();
    // sprintf(mess, "player_hit:%c_%c;", player_id + 1, random_card);
    sprintf(mess, "player_hit:%d_%c;", (player_id + 1), random_card);
    // sprintf(mess, mess, random_card);
    printf("odesílám zprávu hitnutí: %s\n", mess);

    broadcast_message(mess);
    // if (send(players[player_id].socket_fd, mess, strlen(mess), 0) < 0) {
    //     perror("send failed");
    // }

    pthread_mutex_unlock(&players_mutex);
}

void croupier_hit() {
    sleep(1);
    char mess[20];
    sprintf(mess, "croupier_hit:%c;", get_random_card());
    broadcast_message(mess);
}

char get_random_card() {

    // Řetězec obsahující možné hodnoty karet
    const char *values = "23456789TJQKA";
    srand(time(0));
    int random_id = rand() % 12 + 1;

    // Vrácení náhodného znaku (hodnoty karty)
    return values[random_id];
    
}

int check_ready_to_play_hand_of_players() {
    int all_ready_to_play_hand = 1;
    pthread_mutex_lock(&players_mutex);

        for (int i = 0; i < MAX_PLAYERS; ++i) {
            if (!players[i].id || !players[i].is_ready_to_play_hand) {
                all_ready_to_play_hand = 0;
            }
        }

    pthread_mutex_unlock(&players_mutex);
    return all_ready_to_play_hand;
}

int check_ready_of_players() {
    int all_ready = 1;
    pthread_mutex_lock(&players_mutex);
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            if (!players[i].id || !players[i].is_ready) {
                all_ready = 0;
            }
        }
    pthread_mutex_unlock(&players_mutex);
    return all_ready;
}

void broadcast_message(const char *message) {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players[i].id) {  // If the player exists and has a valid socket_fd
            if (send(players[i].socket_fd, message, strlen(message), 0) < 0) {
                perror("send failed");
            }
        }
    }
}

int create_shared_memory() {
    int shmid;

    // Create shared memory segment for MAX_PLAYERS
    shmid = shmget(IPC_PRIVATE, sizeof(struct player) * MAX_PLAYERS, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget failed");
        return -1;
    }

    // Attach shared memory segment to the players pointer
    players = (struct player *)shmat(shmid, NULL, 0);
    if (players == (void *)-1) {
        perror("shmat failed");
        return -1;
    }

    // Initialize player data in shared memory
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        players[i].id = 0;
        players[i].is_ready = 0;
        players[i].is_ready_to_play_hand = 0;
        players[i].can_play = 0;
        players[i].nickname = malloc(50 * sizeof(char));
        if (!players[i].nickname) {
            exit(1);
        }
        players[i].loses_game = -1;
        players[i].win_game = -1;
    }

    return 0;
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <IP>:<port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *ip_port = argv[1];
    char *colon = strchr(ip_port, ':');
    if (!colon) {
        fprintf(stderr, "Invalid parameter format. Use <IP>:<port>.\n");
        exit(EXIT_FAILURE);
    }

    // Split IP and port
    *colon = '\0';
    const char *ip = ip_port;
    int port = atoi(colon + 1);

    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number: %d\n", port);
        exit(EXIT_FAILURE);
    }

    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Set up signal handler to reap zombie processes
    signal(SIGCHLD, handle_sigchld);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &address.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address: %s\n", ip);
        exit(EXIT_FAILURE);
    }

    // Binding socket to the specified IP and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server started on %s:%d. Waiting for clients...\n", ip, port);

    // Create shared memory for players
    if (create_shared_memory() == -1) {
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Accept new client connection
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        if (players_count < MAX_PLAYERS) {
            players[players_count].id = players_count + 1;
            players[players_count].socket_fd = client_socket;

            printf("Client connected: %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
            printf("Client %d connected with socket_fd %d.\n", players[players_count].id, client_socket);

            // Create unique player ID to pass to the thread
            int *player_id = malloc(sizeof(int));
            *player_id = players_count;

            // Create thread to handle client
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, handle_client, (void *)player_id) != 0) {
                perror("pthread_create failed");
                close(client_socket);
                continue;
            }

            players[players_count].thread = thread_id;

            // Increment player count
            players_count++;
        } else {
            // Max players reached, send error message to client
            char *error_message = "Max players reached. Try again later.\n";
            send(client_socket, error_message, strlen(error_message), 0);
            close(client_socket);
        }
    }

    // Wait for all threads to finish before closing the server
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        pthread_join(players[i].thread, NULL);
    }

    close(server_fd);

    return 0;
}


// int main() {
//     int server_fd, client_socket;
//     struct sockaddr_in address;
//     int opt = 1;
//     int addrlen = sizeof(address);
    

//     // Set up signal handler to reap zombie processes
//     signal(SIGCHLD, handle_sigchld);

//     // Creating socket file descriptor
//     if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
//         perror("socket failed");
//         exit(EXIT_FAILURE);
//     }

//     // Forcefully attaching socket to the port 8080
//     if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
//         perror("setsockopt");
//         exit(EXIT_FAILURE);
//     }

//     address.sin_family = AF_INET;
//     address.sin_addr.s_addr = INADDR_ANY;
//     address.sin_port = htons(PORT);

//     // Binding socket to the port
//     if (bind(server_fd, (struct sockaddr *)&address, addrlen) < 0) {
//         perror("bind failed");
//         exit(EXIT_FAILURE);
//     }

//     if (listen(server_fd, 3) < 0) {
//         perror("listen");
//         exit(EXIT_FAILURE);
//     }

//     printf("Server started. Waiting for clients...\n");

//     // Create shared memory for players
//     if (create_shared_memory() == -1) {
//         exit(EXIT_FAILURE);
//     }

//     while (1) {
//         struct sockaddr_in address;
//         int addrlen = sizeof(address);
//         // Accept new client connection
//         if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
//             perror("accept");
//             exit(EXIT_FAILURE);
//         }

//         if (players_count < MAX_PLAYERS) {
//             players[players_count].id = players_count + 1;
//             players[players_count].socket_fd = client_socket;

//             printf("Client connected: %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
//             printf("Client %d connected with socket_fd %d.\n", players[players_count].id, client_socket);

//             // Create unique player ID to pass to the thread
//             int *player_id = malloc(sizeof(int));
//             *player_id = players_count;

//             // Create thread to handle client
//             pthread_t thread_id;
//             if (pthread_create(&thread_id, NULL, handle_client, (void *)player_id) != 0) {
//                 perror("pthread_create failed");
//                 close(client_socket);
//                 continue;
//             }

//             players[players_count].thread = thread_id;

//             // Increment player count
//             players_count++;
//         } else {
//             // Max players reached, send error message to client
//             char *error_message = "Max players reached. Try again later.\n";
//             send(client_socket, error_message, strlen(error_message), 0);
//             close(client_socket);
//         }
//     }

//     // Wait for all threads to finish before closing the server
//     for (int i = 0; i < MAX_PLAYERS; ++i) {
//         pthread_join(players[i].thread, NULL);
//     }

//     close(server_fd);
    
//     return 0;
// }
