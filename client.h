#ifndef CLIENT_H
#define CLIENT_H

#include "affichage.h"
#include "misc.h"
#include "communication.h"

typedef struct {
    int CODEREQ;
    short ID;
    short EQ;
    int PORTUDP;
    int PORTMDIFF;
    char * ADRMDIFF;
} Game_client;

typedef struct {
    int CODEREQ;
    short ID;
    short EQ;
    uint16_t num;
    uint16_t num_ms;
    short hauteur;
    short largeur;
    char * grid;
} Game_state_client;


int create_socket_client(int, int, char *);
int send_join_msg(int, mode_teams);
int send_ready_msg(int,Game_client);
Game_client recv_port(int);
Game_client get_game(header,int,int,char*);
void print_game_c(Game_client);
int join_multicast_group(char*, struct sockaddr_in6);
void start_game(int,int,int, Game_client, struct sockaddr_in6*);


#endif