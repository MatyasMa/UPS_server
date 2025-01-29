#ifndef PLAYERS_H
#define PLAYERS_H

int create_shared_memory(void);
// int check_ready_of_players(void);
int check_ready_of_players(struct session* curr_sess);
// int check_ready_to_play_hand_of_players(void);
int check_ready_to_play_hand_of_players(struct session* curr_sess);
void player_hit(int player_id);
void hide_players_buttons(int player_id);
void show_players_buttons(int player_id);
void unready_to_play_hand_players(void);
void clear_players_data(void);
int is_some_player_disconnected(void);


#endif
