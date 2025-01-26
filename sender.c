#include "server.h"
#include "game_control.h"
#include "players.h"
#include "sender.h"
void broadcast_message(const char *message) {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players[i].id) {  // If the player exists and has a valid socket_fd
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
