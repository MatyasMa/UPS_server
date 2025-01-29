#ifndef GAME_CONTROL_H
#define GAME_CONTROL_H

char get_random_card(void);
void croupier_hit(struct session* curr_sess);
void start_croupier_play(int player_id);


#endif
