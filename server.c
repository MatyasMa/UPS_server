

#include "server.h"
#include "game_control.h"
#include "players.h"
#include "sender.h"


#define PORT 8080

int players_count = 0;

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int current_player_id = 0;

struct client_status client_status[MAX_PLAYERS];


void* keep_alive_thread(void* arg) {
    int player_id = *(int*)arg;

    free(arg); 
    
    while (1) {
        sleep(KEEP_ALIVE_INTERVAL);

        // if (check_ready_of_players()) {
            if (players[player_id].is_connected == 1) {
                // Send ping message
                printf("\nSending ping for client %d\n", players[player_id].id - 1);
                if (send(players[player_id].socket_fd, "ping;", strlen("ping;"), 0) < 0) {
                    perror("Ping send failed");
                    // handle_disconnect(i);
                    continue;
                }

                
                // Výpis informací před podmínkou
                printf("Checking timeout for client %d:\n", player_id);
                printf("  Čekal sekund: %ld\n", time(NULL) - client_status[player_id].last_response_time);

                // Check for pong response
                if (time(NULL) - client_status[player_id].last_response_time > KEEP_ALIVE_TIMEOUT) {
                    printf("Client %d timeout\n", player_id);
                    handle_disconnect(player_id); 
                    pthread_exit(NULL);                   
                }
            }            
        // }        
    }
    return NULL;
}

void handle_disconnect(int player_id) {
    printf("Client %d disconnected\n", player_id);

    int id_player_to_send = 0;
    if (player_id == 0) {
        id_player_to_send = 1;
    }

    char message[256];
    snprintf(message, sizeof(message), "disconnected:%d;", player_id);
    send_message_to_player(id_player_to_send, message);
    // broadcast_message(message);
    // send_message_to_player(id_player_to_send, players[player_id].nickname);

    players[player_id].is_connected = 0;
    players[2] = players[player_id];
    

    players_count -= 1;

    client_status[player_id].is_connected = 0;

    // Nastaví časovač na obnovení
    time_t disconnect_time = time(NULL);

    while (time(NULL) - disconnect_time < TRY_RECONNECT_TIME) {
        if (client_status[player_id].is_connected) {
            printf("Player %d reconnected\n", player_id);
            // TODO: zpracovat tuto zprávu
            // broadcast_message("Player reconnected!");
            return;
        }
        sleep(1);
    }

    // Odstranění hráče po překročení časového limitu
    printf("Player %d failed to reconnect. Ending game for this player.\n", player_id);
    // TODO: broadcast_message("Player failed to reconnect. Game over.");
    // TODO:  remove_player(player_id); - zrušit hráče hraje novej
}



// Handle client interaction
void* handle_client(void* arg) {
    int player_id = *(int*)arg;
    char buffer[1024] = {0};
    int bytes_read;

    free(arg);  // Free the dynamically allocated player_id memory

    int keep_alive_counter = 0;

    pthread_t keep_alive_tid;

    // Allocate memory for player_id
    int* player_id_ptr = malloc(sizeof(int));
    if (player_id_ptr == NULL) {
        perror("Failed to allocate memory for player_id");
        exit(EXIT_FAILURE);
    }

    *player_id_ptr = player_id;

    client_status[player_id].last_response_time = time(NULL);
    if (pthread_create(&keep_alive_tid, NULL, keep_alive_thread, (void *)player_id_ptr) != 0) {
        perror("pthread_create for keep-alive failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        memset(buffer, 0, sizeof(buffer)); // Clear buffer
        printf("Waiting for message from client %d...\n", player_id);
        bytes_read = recv(players[player_id].socket_fd, buffer, sizeof(buffer), 0);
        client_status[player_id].last_response_time = time(NULL);

        if (bytes_read <= 0) {
            // Client disconnected or error
            printf("Client %d disconnected.\n", player_id);
            handle_disconnect(player_id);


            close(players[player_id].socket_fd);
            pthread_exit(NULL);  // Properly exit the thread
        }

        buffer[bytes_read] = '\0';
        printf("buffer: %s\n",buffer);


        if (!strncmp(buffer, "pong", 4)) {        
            printf("%d: Client %d is alive\n", keep_alive_counter++, player_id);
        } else if (!strncmp(buffer, "reconnected", 10)) {
            char *nick;
            nick = malloc(50 * sizeof(char));
            if (!nick) {
                exit(EXIT_FAILURE);
            }
            if (sscanf(buffer, "reconnected:%s", nick) == 1) {
                /* pripojil se stejny hrac */
                if (players[player_id].nickname == nick) {
                    printf("hráč z přezdívkou %s se připojil zpět\n", nick);
                    players[player_id].is_connected = 1;
                    client_status[player_id].is_connected = 1;
                    // TODO: získat data, která byla zasílána za obu nepřipojení - ty si můžu uložit do hráče id 2

                    char message[50]; 
                    sprintf(message, "reconnected:%d;", player_id); 
                    printf("odesílám zprávu: %s",message);
                    broadcast_message(message);
                }                
            } else {
                /* TODO: pripojil se jiny (novy) hrac (s jinou přezdívkou) */
            }

        } else if (!strncmp(buffer, "keep-alive", 10)) {
            printf("Klient %d je živej\n", player_id);
            continue;
        } else if (!strncmp(buffer, "ready:", 6)) {
            char *nick;
            nick = malloc(50 * sizeof(char));
            if (!nick) {
                exit(EXIT_FAILURE);
            }
            if (sscanf(buffer, "ready:%s", nick) == 1) {
                players[player_id].nickname = nick;
            }
            printf("Client %d - %s is ready\n", player_id, players[player_id].nickname);
            players[player_id].is_ready = 1;


            if (check_ready_of_players()) {
                start_clients();
                printf("nejsou ready\n");
                // broadcast_message("start_game\n");
            }


        } else if (!strncmp(buffer, "unready:", 8)) {
            char *nick;
            nick = malloc(50 * sizeof(char));
            if (!nick) {
                exit(EXIT_FAILURE);
            }
            if (sscanf(buffer, "unready:%s", nick) == 1) {
                players[player_id].nickname = "null";
            }
            printf("Client %d - %s is NOT ready\n", players[player_id].id, players[player_id].nickname);
            players[player_id].is_ready = 0;

        } else if (!strcmp(buffer, "exit")) {
            printf("Client %d requested exit.\n", players[player_id].id);
            close(players[player_id].socket_fd);
            pthread_exit(NULL);

        } else if (!strcmp(buffer, "ready_to_play_hand")){
  
            players[player_id].is_ready_to_play_hand = 1;

            if (check_ready_to_play_hand_of_players()) {
                // get first card for croupier and send message for players to ask for cards
                broadcast_message("ask_for_first_cards;");
                pthread_mutex_lock(&players_mutex);
                sleep(1);
                croupier_hit();
                players[0].can_play = 1;
                pthread_mutex_unlock(&players_mutex);
            }

        } else if (!strcmp(buffer, "get_first_cards")) {
            
            get_first_cards(player_id);
            
        } else if (!strcmp(buffer, "player_get_hit")) {

            if (players[player_id].can_play) {
                player_hit(player_id);
            } else {
                printf("Momentálně nejste na řadě.\n");
            }

            
        } else if (!strncmp(buffer, "player_stand:", 13)) {

            ++players[player_id].hands_played;
            // player_stand:C ... continue
            // player_stand:L ... lost
            char result;
            // Parsování znaku po "player_stand:"
            if (sscanf(buffer, "player_stand:%c", &result) == 1) {
                if (result == 'L') {
                    players[player_id].loses_hand = 1;
                    printf("hrac %d prohral: to many\n",player_id);
                } else {
                    printf("hrac %d stand:\n",player_id);
                }
                
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
                
                if (players[0].loses_hand && players[1].loses_hand) {
                    broadcast_message("hand_ended_for_all");
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
        } else if (!strcmp(buffer, "croupier_play_end")) {
            broadcast_message("hand_ended_for_all");
        } else if (!strcmp(buffer, "hand_end")) {
            
            if (players[0].hands_played == MAX_HANDS_PLAY || players[1].hands_played == MAX_HANDS_PLAY) {

                while (!(players[0].hands_played == MAX_HANDS_PLAY && players[1].hands_played == MAX_HANDS_PLAY)) {
                    sleep(1);
                    printf("čekám\n");
                }

                if (player_id == 1) {
                    broadcast_message("game_over");
                }
            }
            
            unready_to_play_hand_players();
        } else if (!strcmp(buffer, "send_game_over")) {
            broadcast_message("game_over");
        } else if (!strncmp(buffer, "balance:", 8)) {
            int result;
            // Parsování znaku po "player_stand:"
            if (sscanf(buffer, "balance:%d", &result) == 1) {
                players[player_id].balance = result;
            }

            pthread_mutex_lock(&players_mutex);
            if (player_id == 1) {

                while (players[0].balance < 0) {
                    pthread_cond_wait(&cond, &players_mutex);
                    printf("čekám");
                }

            
                char message1[50]; 
                char message2[50]; 
                if (players[0].balance < players[1].balance) {                    
                    sprintf(message1, "win:%d_%d", players[1].balance, players[0].balance); 
                    sprintf(message2, "lose:%d_%d", players[0].balance, players[1].balance); 
                    send_message_to_player(1, message1);
                    send_message_to_player(0, message2);
                } else if (players[1].balance < players[0].balance) {
                    sprintf(message1, "win:%d_%d", players[0].balance, players[1].balance); 
                    sprintf(message2, "lose:%d_%d", players[1].balance, players[0].balance); 
                    send_message_to_player(0, message1);
                    send_message_to_player(1, message2);
                } else {
                    sprintf(message1, "draw:%d", players[0].balance); 
                    broadcast_message(message1);
                }
 
                clear_players_data();
            } 
            
            pthread_mutex_unlock(&players_mutex);
        } else {
            // unknown message
            send_message_to_player(player_id, "UNKNOWN COMMAND\n");
            close(players[player_id].socket_fd);
        }
 
    }
}

void get_first_cards(int player_id) {
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
}



void start_clients(void) {
    char message[100]; // Allocate a fixed-size buffer
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players[i].id) { // If the player exists and has a valid id
            sprintf(message, "start_game:%d_%s_%s;", players[i].id, players[0].nickname, players[1].nickname); // Format the message
            if (send(players[i].socket_fd, message, strlen(message), 0) < 0) {
                perror("send failed");
            }
        }
    }
}

// Signal handler to reap zombie processes
void handle_sigchld(void) {
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
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
    signal(SIGCHLD, (void (*) (int)) (handle_sigchld));

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

    // pthread_t keep_alive_tid;
    // if (pthread_create(&keep_alive_tid, NULL, keep_alive_thread, NULL) != 0) {
    //     perror("pthread_create for keep-alive failed");
    //     exit(EXIT_FAILURE);
    // }

    while (1) {
        // Accept new client connection
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        if (players_count < MAX_PLAYERS) {
            int disconnected_player_id = 0;

            disconnected_player_id = is_some_player_disconnected();

            if (disconnected_player_id == -1) {
                players[players_count].id = players_count + 1;
                players[players_count].socket_fd = client_socket;
                players[players_count].is_connected = 1;
                players[players_count].is_created = 1;
            } else {
                players[disconnected_player_id].socket_fd = client_socket;
                players[disconnected_player_id].is_connected = 1;
            }
            

            printf("Client connected: %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
            printf("Client %d connected with socket_fd %d.\n", current_player_id, client_socket);

            // Create unique player ID to pass to the thread
            int *player_id = malloc(sizeof(int));
            if (disconnected_player_id != -1) {
                *player_id = disconnected_player_id;
            } else {
                *player_id = players_count;
            }

            // Create thread to handle client
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, handle_client, (void *)player_id) != 0) {
                perror("pthread_create failed");
                close(client_socket);
                continue;
            }

            // TODO: přepíše původní thread id ?
            if (disconnected_player_id == -1) {
                players[players_count].thread = thread_id;
            } else {
                players[disconnected_player_id].thread = thread_id;
            }

            // players[players_count].thread = thread_id;

            // Increment player count
            players_count++;
        } else {
            // Max players reached, send error message to client
            // char *error_message = "Max players reached. Try again later.\n";
            // send(client_socket, error_message, strlen(error_message), 0);
            printf("maximum hráčů!!!\n");
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

