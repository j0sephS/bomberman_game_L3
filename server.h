#ifndef SERVER_H
#define SERVER_H

#define LEN_UPDATE_CACHE 256


typedef struct
{
int x;
int y;
int num;

} Bombe;




typedef struct {
    short nb_players;
    int game_id;
    int CODEREQ;
    int PORTUDP;
    int PORTMDIFF;
    int sock_tcp_client[4];
    int sock_udp;
    char * ADRMDIFF;
} Game_server;

typedef struct {
    int team;
    int id;
    int x;
    int y;
    int alive;
} players;

typedef struct {
    char * map;
    int8_t len_h;
    int8_t len_w;
    players * player;
    int sock_server_mdiff;
    struct sockaddr_in6 addr_mdiff;
    int sock_server_udp;
    char ** update;
    int len_cache_update;
    int code_game;
    Bombe *bombes;
    int mode;
    int *sockClientlist;
} Game_state_server;

typedef struct{
    int i;
    Game_state_server *game;

} infoBombe;


int create_socket_server();
int recv_join_send_port(int,int);
char * server_send_port(Game_server);
Game_server trouve_partie_dispo(int,int);
void initialisation_games();
void* thread_new_game(void *);
void remove_game(Game_server);
char * create_complete_grid(int8_t len_h,int8_t len_w);

#endif