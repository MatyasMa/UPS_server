#include "server.h"
#include "game_control.h"
#include "players.h"
#include "sender.h"

char get_random_card(void) {

    // Řetězec obsahující možné hodnoty karet
    const char *values = "23456789TJQKA";
    srand(time(0));
    int random_id = rand() % 12 + 1;

    // Vrácení náhodného znaku (hodnoty karty)
    return values[random_id];    
}

void croupier_hit(void) {
    sleep(1);
    char mess[20];
    sprintf(mess, "croupier_hit:%c;", get_random_card());
    broadcast_message(mess);
}

void start_croupier_play(int player_id) {
    char *message = "start_croupier_play;";
    if (send(players[player_id].socket_fd, message, strlen(message), 0) < 0) {
        perror("send failed");
    }
}

