#include "server.h"
#include "game_control.h"
#include "players.h"
#include "sender.h"
void broadcast_message(const char *message, struct session* sess) {
    // for (int i = 0; i < MAX_PLAYERS; ++i) {
    //     if (players[i].id) {  // If the player exists and has a valid socket_fd
    //         if (send(players[i].socket_fd, message, strlen(message), 0) < 0) {
    //             perror("send failed");
    //         }
    //     }
    // }

    for (int i = 0; i < MAX_PLAYERS_IN_GAME; ++i) {
        if (sess->players[i]) {  // If the player exists and has a valid socket_fd
            if (send(players[i].socket_fd, message, strlen(message), 0) < 0) {
                perror("send failed");
            }
        }
    }
    
}

void send_message_to_player(int id_player, const char *message) {
    if (send(players[id_player].socket_fd, message, strlen(message), 0) < 0) {
        perror("send failed");
    }
}
