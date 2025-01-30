

#include "server.h"
#include "game_control.h"
#include "players.h"
#include "sender.h"

#include <errno.h>
#include <sys/time.h>


#define PORT 8080

int players_count = 0;

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;


struct client_status client_status[MAX_PLAYERS];


void* keep_alive_thread(void* arg) {
    int player_id = *(int*)arg;

    free(arg); 
    
    while (1) {
        sleep(KEEP_ALIVE_INTERVAL);
        if (players[player_id].socket_fd == -1) {
            pthread_exit(NULL); 
        }

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
                // printf("  Čekal sekund: %ld\n", time(NULL) - client_status[player_id].last_response_time);
                printf("  Čekal sekund: %ld\n", time(NULL) - players[player_id].last_response_time);

                // Check for pong response
                if (time(NULL) - players[player_id].last_response_time > KEEP_ALIVE_TIMEOUT) {
                    printf("Client %d timeout\n", player_id);
                    if(handle_disconnect(player_id)) {
                        // TODO: -> uzavřít zde soket a vyčistit odpojeného uživatele, po timeout!!


                        // TODO: ukončit klienta a oznámit mu že vyhrál
                        // TODO: nastavit room na prázdnou, protože v ní zůstane jen jeden uživatel
                        // TODO: vyčistit klienta stejně jako v mainu po disconnectu natvrdo, udělat pro to funkci

                        // TODO: ukončit klienta, došlo k vypingování
                        // TODO: asi se ukončí když soket bude -1
                        close(players[player_id].socket_fd);
                        players[player_id].socket_fd = -1;
                        pthread_exit(NULL);  
                    } 
                    // if (curr_sess->is_full) {
                    //     create_and_send_reset_state_message(player_id);
                    // }               
                    // TODO: odeslat hráči stav hry -> ten je uložený v session, dát do vhodného tvaru a poslat hráči s player id
                                   
                }
                /*
                if (time(NULL) - client_status[player_id].last_response_time > KEEP_ALIVE_TIMEOUT) {
                    printf("Client %d timeout\n", player_id);
                    handle_disconnect(player_id); 
                    pthread_exit(NULL);                   
                }
                */
            }            
        // }        
    }
    return NULL;
}

int handle_disconnect(int player_id) {
    printf("Client %d disconnected\n", player_id);

    struct session* curr_sess = find_clients_session(player_id);
    
    int id_player_to_send = 0;
    int id_disconnected_position = 0;

    players[player_id].is_connected = 0;
    
    if (curr_sess->is_full) {
        if (curr_sess->players[0]->id - 1 == player_id) {
            id_disconnected_position = 0;
            id_player_to_send = curr_sess->players[1]->id - 1;
        } else if (curr_sess->players[1]->id - 1 == player_id) {
            id_player_to_send = curr_sess->players[0]->id - 1;
            id_disconnected_position = 1;
        }
        curr_sess->players[id_disconnected_position]->is_connected = 0;


        // informovani druheho uzivatele o tom, ze prvni vypadl
        char message[256];    
        snprintf(message, sizeof(message), "disconnected:%d;", id_disconnected_position);
        send_message_to_player(id_player_to_send, message);
    }
    
    
    // broadcast_message(message);
    // send_message_to_player(id_player_to_send, players[player_id].nickname);

    // players[player_id].is_connected = 0;
    // players[2] = players[player_id];
    

    // players_count -= 1;
    // client_status[player_id].is_connected = 0;

    // Nastaví časovač na obnovení
    time_t disconnect_time = time(NULL);

    while (time(NULL) - disconnect_time < TRY_RECONNECT_TIME) {
        if (players[player_id].is_connected) {
            printf("Server se připojil k hráči - Player %s reconnected\n", players[player_id].nickname);
            create_and_send_reset_state_message(player_id);
            // broadcast_message("Player reconnected!");
            return 0;
        }
        sleep(1);
    }

    // Odstranění hráče po překročení časového limitu
    printf("Player %d failed to reconnect. Ending game for this player.\n", player_id);
    return 1;
    // TODO: broadcast_message("Player failed to reconnect. Game over.");
    // TODO:  remove_player(player_id); - zrušit hráče hraje novej
}


void set_socket_timeout(int socket_fd, int seconds) {
    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

// Handle client interaction
void* handle_client(void* arg) {
    int player_id = *(int*)arg;
    char buffer[1024] = {0};
    int bytes_read;

    free(arg);  // Free the dynamically allocated player_id memory

    int keep_alive_counter = 0;

    pthread_t keep_alive_tid;

    struct session* clients_sess = find_clients_session(player_id);

    // Allocate memory for player_id
    int* player_id_ptr = malloc(sizeof(int));
    if (player_id_ptr == NULL) {
        perror("Failed to allocate memory for player_id");
        exit(EXIT_FAILURE);
    }

    *player_id_ptr = player_id;

    players[player_id].last_response_time = time(NULL);
    //client_status[player_id].last_response_time = time(NULL);
    if (pthread_create(&keep_alive_tid, NULL, keep_alive_thread, (void *)player_id_ptr) != 0) {
        perror("pthread_create for keep-alive failed");
        exit(EXIT_FAILURE);
    }

    // timeout pro ukončení když není přijmuta žádná zpráva x sekund
    set_socket_timeout(players[player_id].socket_fd, TRY_RECONNECT_TIME); 

    while (1) {
        memset(buffer, 0, sizeof(buffer)); // Clear buffer
        printf("Waiting for message from client %d...\n", player_id);

        bytes_read = recv(players[player_id].socket_fd, buffer, sizeof(buffer), 0);

        players[player_id].last_response_time = time(NULL);
        //client_status[player_id].last_response_time = time(NULL);

        if (bytes_read <= 0 || pthread_kill(keep_alive_tid, 0) == ESRCH || players[player_id].socket_fd == -1) {
            // Client disconnected or error
            printf("Client %d fully disconnected, he will be clear.\n", player_id);

            for (int i = 0; i < MAX_PLAYERS_IN_GAME; ++i) {
                if (clients_sess->players[i] && clients_sess->players[i]->id == players[player_id].id) {
                    clients_sess->players[i] = NULL;
                    if (clients_sess->players[1 - i] == NULL) {
                        clients_sess->is_full = 0;
                    }
                }
            }
            
            // TODO: udělat jako next free id
            --players_count;
            //handle_disconnect(player_id);            
            close(players[player_id].socket_fd);
            players[player_id].socket_fd = -1;
            players[player_id].id = 0;
            players[player_id].is_connected = 0;
            players[player_id].is_created = 0;
            players[player_id].is_ready = 0;
            players[player_id].is_ready_to_play_hand = 0;
            players[player_id].can_play = 0;

            
            pthread_exit(NULL);  // Properly exit the thread
        }

        buffer[bytes_read] = '\0';
        printf("buffer: %s\n",buffer);

    
        char *token = strtok(buffer, "/");
        while (token != NULL) {
            if (!strncmp(token, "pong", 4)) {        
                printf("%d: Client %d is alive\n", keep_alive_counter++, player_id);
            } else if (!strncmp(token, "reconnected", 10)) {
                char *nick;
                nick = malloc(50 * sizeof(char));
                if (!nick) {
                    exit(EXIT_FAILURE);
                }
                if (sscanf(token, "reconnected:%s", nick) == 1) {
                    /* pripojil se stejny hrac */
                    printf("Hráč z přezdívkou %s se připojil zpět na server\n", nick);
                    for (int i = 0; i < MAX_PLAYERS_IN_GAME; ++i) {
                        if (clients_sess->players[i] && strcmp(clients_sess->players[i]->nickname, nick) == 0) {
                            printf("Reconnected player found\n");
                            clients_sess->players[i]->is_connected = 1;
                        }
                    }

                    /*
                    int id_reconnected = -1;
                    if (players[player_id].nickname == nick) {
                        id_reconnected = player_id;                    
                        // TODO: získat data, která byla zasílána za obu nepřipojení - ty si můžu uložit do hráče id 2                    
                    } else {
                        if (player_id == 0) {
                            id_reconnected = 1;
                        } else {
                            id_reconnected = 0;
                        }
                    }

                    if (id_reconnected != -1) {
                        players[id_reconnected].is_connected = 1;
                        client_status[id_reconnected].is_connected = 1;
                    }
                    */

                    char message[50]; 
                    sprintf(message, "reconnected:%s;", nick); 
                    printf("odesílám zprávu: %s",message);
                    broadcast_message(message, clients_sess);      
                } else {
                    /* TODO: pripojil se jiny (novy) hrac (s jinou přezdívkou) */
                }

            } else if (!strncmp(token, "keep-alive", 10)) {
                printf("Klient %d je živej\n", player_id);
                continue;
            } else if (!strncmp(token, "ready:", 6)) {
                char *nick;
                nick = malloc(50 * sizeof(char));
                if (!nick) {
                    exit(EXIT_FAILURE);
                }
                if (sscanf(token, "ready:%s", nick) == 1) {
                    players[player_id].nickname = nick;
                }
                printf("Client %d - %s is ready\n", player_id, players[player_id].nickname);
                players[player_id].is_ready = 1;


                if (check_ready_of_players(clients_sess)) {
                    start_clients();
                    printf("nejsou ready\n");
                    // broadcast_message("start_game\n");
                }


            } else if (!strncmp(token, "unready:", 8)) {
                char *nick;
                nick = malloc(50 * sizeof(char));
                if (!nick) {
                    exit(EXIT_FAILURE);
                }
                if (sscanf(token, "unready:%s", nick) == 1) {
                    players[player_id].nickname = "null";
                }
                printf("Client %d - %s is NOT ready\n", players[player_id].id, players[player_id].nickname);
                players[player_id].is_ready = 0;

            } else if (!strncmp(token, "exit", 4)) {
                printf("Client %d requested exit.\n", players[player_id].id);
                close(players[player_id].socket_fd);
                pthread_exit(NULL);

            } else if (!strncmp(token, "ready_to_play_hand", 18)){
    
                players[player_id].is_ready_to_play_hand = 1;

                if (check_ready_to_play_hand_of_players(clients_sess)) {
                    strcpy(clients_sess->player_one_cards, "");
                    strcpy(clients_sess->player_two_cards, "");
                    strcpy(clients_sess->croupier_cards, "");
                    // get first card for croupier and send message for players to ask for cards
                    broadcast_message("ask_for_first_cards;", clients_sess);
                    pthread_mutex_lock(&players_mutex);
                    sleep(1);
                    croupier_hit(clients_sess);
                    // TODO: test
                    clients_sess->players[0]->can_play = 1;
                    // players[0].can_play = 1;
                    pthread_mutex_unlock(&players_mutex);
                }

            } else if (!strncmp(token, "get_first_cards", 15)) {
                get_first_cards(player_id, clients_sess);
                
            } else if (!strncmp(token, "player_get_hit", 14)) {

                if (players[player_id].can_play) {
                    player_hit(player_id, clients_sess);
                } else {
                    printf("Momentálně nejste na řadě.\n");
                }

                
            } else if (!strncmp(token, "player_stand:", 13)) {

                ++players[player_id].hands_played;
                // player_stand:C ... continue
                // player_stand:L ... lost
                char result;
                // Parsování znaku po "player_stand:"
                if (sscanf(token, "player_stand:%c", &result) == 1) {
                    if (result == 'L') {
                        players[player_id].loses_hand = 1;
                        printf("hrac %d prohral: to many\n",player_id);
                    } else {
                        printf("hrac %d stand:\n",player_id);
                    }
                    
                } else {
                    printf("Failed to extract character.\n");
                }

                // TODO: test
                if (clients_sess->players[0]->id - 1 == player_id) {
                    clients_sess->players[0]->can_play = 0;
                    hide_players_buttons(0, clients_sess);
                    clients_sess->players[1]->can_play = 1;
                    show_players_buttons(1, clients_sess);
                } else if (clients_sess->players[1]->id - 1 == player_id) {
                    clients_sess->players[1]->can_play = 0;
                    hide_players_buttons(1, clients_sess);
                    
                    if (clients_sess->players[0]->loses_hand && clients_sess->players[1]->loses_hand) {
                        broadcast_message("hand_ended_for_all;", clients_sess);
                    } else {
                        start_croupier_play(clients_sess->players[1]->id - 1);
                    }
                }

                // if (player_id == 0) {
                //     players[0].can_play = 0;
                //     hide_players_buttons(0, clients_sess);
                //     players[1].can_play = 1;
                //     show_players_buttons(1, clients_sess);
                // } else if (player_id == 1) {
                //     players[1].can_play = 0;
                //     hide_players_buttons(1, clients_sess);
                    
                //     if (players[0].loses_hand && players[1].loses_hand) {
                //         broadcast_message("hand_ended_for_all;", clients_sess);
                //     } else {
                //         // Posílám libovolném uživateli
                //         start_croupier_play(1);
                //     }
                // }
            } else if (!strncmp(token, "croupier_get_hit", 16)) {
                // pthread_mutex_lock(&players_mutex);
                croupier_hit(clients_sess);
                // sleep(1);
                // pthread_mutex_unlock(&players_mutex);
            } else if (!strncmp(token, "croupier_play_end", 17)) {

                for (int i = 0; i < MAX_PLAYERS_IN_GAME; ++i) {
                    clients_sess->players[i]->has_first_cards = 0;
                }
                
                broadcast_message("hand_ended_for_all;", clients_sess);

            } else if (!strncmp(token, "hand_end", 8)) {
                
                if (clients_sess->players[0]->hands_played == MAX_HANDS_PLAY || clients_sess->players[1]->hands_played == MAX_HANDS_PLAY) {

                    while (!(clients_sess->players[0]->hands_played == MAX_HANDS_PLAY && clients_sess->players[1]->hands_played == MAX_HANDS_PLAY)) {
                        sleep(1);
                        printf("čekám\n");
                    }

                    if (clients_sess->players[1]->id - 1 == player_id) {
                        broadcast_message("game_over;", clients_sess);                          
                    }
                }
                // if (players[0].hands_played == MAX_HANDS_PLAY || players[1].hands_played == MAX_HANDS_PLAY) {

                //     while (!(players[0].hands_played == MAX_HANDS_PLAY && players[1].hands_played == MAX_HANDS_PLAY)) {
                //         sleep(1);
                //         printf("čekám\n");
                //     }

                //     if (player_id == 1) {
                //         broadcast_message("game_over;", clients_sess);                          
                //     }
                // }
                            
                unready_to_play_hand_players(clients_sess);
            } else if (!strncmp(token, "send_game_over", 14)) {
                broadcast_message("game_over;", clients_sess);
            } else if (!strncmp(token, "balance:", 8)) {
                int result;
                // Parsování znaku po "player_stand:"
                if (sscanf(token, "balance:%d", &result) == 1) {
                    if (clients_sess->players[0]->id - 1 == player_id) {
                        clients_sess->players[0]->balance = result;
                    } else if (clients_sess->players[1]->id - 1 == player_id) {
                        clients_sess->players[1]->balance = result;
                    }
                    // players[player_id].balance = result;
                }
                
                

                    // while (players[0].balance < 0) {
                    //     pthread_cond_wait(&cond, &players_mutex);
                    //     printf("čekám");
                    // }
                pthread_mutex_lock(&players_mutex);

                // tento if vyhodnocuje pouze jeden hrac ze hry aby nedoslo ke dvojimu zaslani
                if (clients_sess->players[0]->id - 1 == player_id) {
                    // printf("croupier cards %s\n");
                    while (!all_players_have_balance(clients_sess)) {
                        // pthread_cond_wait(&cond, &players_mutex);
                        sleep(1);
                        printf("čekám\n");
                    }


                
                    char message1[50]; 
                    char message2[50]; 
                    if (clients_sess->players[0]->balance < clients_sess->players[1]->balance) {                    
                        sprintf(message1, "win:%d_%d", clients_sess->players[1]->balance, clients_sess->players[0]->balance); 
                        sprintf(message2, "lose:%d_%d", clients_sess->players[0]->balance, clients_sess->players[1]->balance); 
                        send_message_to_player(clients_sess->players[1]->id - 1, message1);
                        send_message_to_player(clients_sess->players[0]->id - 1, message2);
                    } else if (clients_sess->players[1]->balance < clients_sess->players[0]->balance) {
                        sprintf(message1, "win:%d_%d", clients_sess->players[0]->balance, clients_sess->players[1]->balance); 
                        sprintf(message2, "lose:%d_%d", clients_sess->players[1]->balance, clients_sess->players[0]->balance); 
                        send_message_to_player(clients_sess->players[0]->id - 1, message1);
                        send_message_to_player(clients_sess->players[1]->id - 1, message2);
                    } else {
                        sprintf(message1, "draw:%d", players[0].balance); 
                        broadcast_message(message1, clients_sess);
                    }
                    // if (players[0].balance < players[1].balance) {                    
                    //     sprintf(message1, "win:%d_%d", players[1].balance, players[0].balance); 
                    //     sprintf(message2, "lose:%d_%d", players[0].balance, players[1].balance); 
                    //     send_message_to_player(1, message1);
                    //     send_message_to_player(0, message2);
                    // } else if (players[1].balance < players[0].balance) {
                    //     sprintf(message1, "win:%d_%d", players[0].balance, players[1].balance); 
                    //     sprintf(message2, "lose:%d_%d", players[1].balance, players[0].balance); 
                    //     send_message_to_player(0, message1);
                    //     send_message_to_player(1, message2);
                    // } else {
                    //     sprintf(message1, "draw:%d", players[0].balance); 
                    //     broadcast_message(message1, clients_sess);
                    // }
    
                    clear_players_data(clients_sess);
                
                }
                pthread_mutex_unlock(&players_mutex);
            } else {
                // unknown message
                send_message_to_player(player_id, "UNKNOWN COMMAND\n");
                // TODO: ukončit i vlákna keep alive a handle client
                // TODO: vždy po close ho nastavit na -1 
                close(players[player_id].socket_fd);
            }
            token = strtok(NULL, "/");
        }
 
    }
}

void create_and_send_reset_state_message(int player_id) {
    
    struct session* curr_sess = find_clients_session(player_id);

    // Začátek zprávy
    char output[255];
    strcpy(output, "reset_state:");

    // Přidání karet hráče 1
    for (int i = 0; i < (int)strlen(curr_sess->player_one_cards); i++) {
        strncat(output, &curr_sess->player_one_cards[i], 1);  // Přidá jednu kartu
        if (i < (int)strlen(curr_sess->player_one_cards) - 1) {
            strcat(output, ",");  // Přidá čárku mezi kartami
        }
    }

    // Přidání oddělovače mezi hráči
    strcat(output, "_");

    // Přidání karet hráče 2
    for (int i = 0; i < (int)strlen(curr_sess->player_two_cards); i++) {
        strncat(output, &curr_sess->player_two_cards[i], 1);  // Přidá jednu kartu
        if (i < (int)strlen(curr_sess->player_two_cards) - 1) {
            strcat(output, ",");  // Přidá čárku mezi kartami
        }
    }

    // Přidání oddělovače mezi hráči a krupiérem
    strcat(output, "_");

    // Přidání karet krupiéra
    for (int i = 0; i < (int)strlen(curr_sess->croupier_cards); i++) {
        strncat(output, &curr_sess->croupier_cards[i], 1);  // Přidá jednu kartu
        if (i < (int)strlen(curr_sess->croupier_cards) - 1) {
            strcat(output, ",");  // Přidá čárku mezi kartami
        }
    }

    strcat(output, ";");

    send_message_to_player(player_id, output);

    printf("Odeslaný output: %s\n", output);
}

int all_players_have_balance(struct session* curr_sess) {
    int i;
    int all_have_balance = 1;
    for (i = 0; i < MAX_PLAYERS_IN_GAME; ++i) {
        if (curr_sess->players[i]->balance < 0) {
            all_have_balance = 0;
        }
    }
    return all_have_balance;
}

void get_first_cards(int player_id, struct session* curr_sess) {
    hide_players_buttons(player_id, curr_sess);

    // Čekání na správné ID hráče
    while (!curr_sess->players[0]->has_first_cards && curr_sess->players[1]->id == player_id + 1) {
        sleep(1);
    }

    // Kritická sekce: hráč provede své akce
    player_hit(player_id, curr_sess);
    player_hit(player_id, curr_sess);

    players[player_id].has_first_cards = 1;
    
    pthread_mutex_lock(&players_mutex);
    if (has_players_first_cards(curr_sess)) {
        show_players_buttons(0, curr_sess);
    }
    pthread_mutex_unlock(&players_mutex);
}

int has_players_first_cards(struct session* curr_sess) {
    int i;
    int all_has_first_cards = 1;
    for (i = 0; i < MAX_PLAYERS_IN_GAME; ++i) {
        if (!curr_sess->players[i]->has_first_cards) {
            all_has_first_cards = 0;
        }
    }
    return all_has_first_cards;
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


    while (1) {
        // Accept new client connection
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        if (players_count < MAX_PLAYERS) {
            struct session* sess = find_first_usable_game();

            if (sess) {
                if (sess->players[0]) {
                    sess->players[1] = &players[players_count];
                    sess->is_full = 1;
                } else {
                    sess->is_active = 1;
                    sess->players[0] = &players[players_count];
                }
            } else {
                printf("game not found\n");
                continue;
            }
            

            players[players_count].id = players_count + 1;
            players[players_count].socket_fd = client_socket;
            players[players_count].is_connected = 1;
            players[players_count].is_created = 1;
            players[players_count].is_ready = 0;
            players[players_count].is_ready_to_play_hand = 0;
            players[players_count].can_play = 0;
            // int disconnected_player_id = 0;

            // disconnected_player_id = is_some_player_disconnected();

            // if (disconnected_player_id == -1) {
            //     players[players_count].id = players_count + 1;
            //     players[players_count].socket_fd = client_socket;
            //     players[players_count].is_connected = 1;
            //     players[players_count].is_created = 1;
            // } else {
            //     players[disconnected_player_id].socket_fd = client_socket;
            //     players[disconnected_player_id].is_connected = 1;
            // }
            

            printf("Client connected: %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
            printf("Client %d connected with socket_fd %d.\n", players[players_count].id, client_socket);

            // Create unique player ID to pass to the thread
            int *player_id = malloc(sizeof(int));
            // if (disconnected_player_id != -1) {
            //     *player_id = disconnected_player_id;
            // } else {
            //     *player_id = players_count;
            // }

            *player_id = players_count;

            // Create thread to handle client
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, handle_client, (void *)player_id) != 0) {
                perror("pthread_create failed");
                close(client_socket);
                continue;
            }

            players[players_count].thread = thread_id;

            // if (disconnected_player_id == -1) {
            //     players[players_count].thread = thread_id;
            // } else {
            //     players[disconnected_player_id].thread = thread_id;
            // }

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

struct session* find_first_usable_game(void) {
    int i;
    for (i = 0; i < MAX_GAMES; ++i) {
        if (!games[i].is_full) {
            return &games[i];
        }
    }

    return NULL;
} 

struct session* find_clients_session(int player_id) {
    int i, j;
    for (i = 0; i < MAX_GAMES; ++i) {
        for (j = 0; j < MAX_PLAYERS_IN_GAME; ++j) {
            if (games[i].players[j]->id - 1 == player_id) {
                return &games[i];
            }
        }
        
    }

    return NULL;
}
