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

void croupier_hit(struct session* curr_sess) {
    sleep(1);
    char mess[20];
    char random_card = get_random_card();

    // sprintf(mess, "player_hit:%c_%c;", player_id + 1, random_card);

    char new_card[2] = {random_card, '\0'};
    strcat(curr_sess->croupier_cards, new_card);
    sprintf(mess, "croupier_hit:%c;", random_card);
    broadcast_message(mess, curr_sess);
}

void start_croupier_play(int player_id) {
    char *message = "start_croupier_play;";
    if (send(players[player_id].socket_fd, message, strlen(message), 0) < 0) {
        perror("send failed");
    }
}

