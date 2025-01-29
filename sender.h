#ifndef SENDER_H
#define SENDER_H

void broadcast_message(const char *message, struct session* sess);
void send_message_to_player(int id_player, const char *message);

#endif
