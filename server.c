#define _DEFAULT_SOURCE

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/socket.h>
#include <net/if.h>

#include "affichage.h"
#include "misc.h"
#include "communication.h"
#include "curses.h"
#include <sys/wait.h>
#include "game.h"
#include <fcntl.h>
#include <signal.h>
#include "server.h"

#define PORT 7879  //numero de port du serveur
#define ADDR "::1"  //adresse du serveur
#define MAX_PENDING_GAME 10

pthread_mutex_t verrou = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t verrou2 = PTHREAD_MUTEX_INITIALIZER;

Game_server games[MAX_PENDING_GAME];
pthread_t tab_thread [MAX_PENDING_GAME];


int main() {
    
    int sock_server = create_socket_server();
    int sock_client;
    initialisation_games();
    while(1){

        sock_client = accept(sock_server,NULL, NULL);

        if(sock_client == -1){
            perror("probleme socket client");
            close(sock_server);
            return 1;
        }

        int i=recv_join_send_port(sock_server,sock_client);

        if (i==-1){
            perror("probleme lecture fst msg");
            close(sock_client);
            close(sock_server);
            return 1;
        }
    }

    //*** fermeture socket serveur ***
    close(sock_server);

    return 0;
}



int create_socket_server(){

     //*** creation de la socket serveur ***
    int sock = socket(PF_INET6, SOCK_STREAM, 0);
    if(sock < 0){
    perror("creation socket");
    exit(1);
    }

    //*** creation de l'adresse du destinataire (serveur) ***
    struct sockaddr_in6 address_sock;
    memset(&address_sock, 0, sizeof(address_sock));
    address_sock.sin6_family = AF_INET6;
    address_sock.sin6_port = htons(PORT);
    address_sock.sin6_addr=in6addr_any;

    //*** on lie la socket au port PORT ***
    //reuseaddr pour eviter les erreurs de bind quand on relance trop vite le serveur
    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&(int){1},sizeof(int));
    int r = bind(sock, (struct sockaddr *) &address_sock, sizeof(address_sock));
    if (r < 0) {
    perror("erreur bind");
    exit(2);
    }

    //*** Le serveur est pret a ecouter les connexions sur le port PORT ***
    r = listen(sock, 0);
    if (r < 0) {
    perror("erreur listen");
    exit(2);
    }

    return sock;
}

// renvoie indice de la game si possible -1 sinon.
int recv_join_send_port(int sock_server, int sock_client){

    char * buf;

    if ((buf=my_recv(sock_client,SIZE_MSG_CLIENT))==NULL){
        close(sock_client);
        close(sock_server);
        return -1;
    }

    convert_littleendian(buf,SIZE_MSG_CLIENT);
    header h;
    memcpy(&h,buf,2);
    int CODEREQ;
    if (h.CODEREQ==1) CODEREQ=9; // mode 4 players
    else if (h.CODEREQ==2) CODEREQ=10; // mode 2 teams
    else return -1; // sinon erreur
    free(buf);
    Game_server game = trouve_partie_dispo(CODEREQ, sock_client);

    printf("find game %d\n",game.game_id);

    char * msg = server_send_port(game);

    
    //write_msg_to_file(msg,SIZE_MSG_SERV_JOIN,"join_msg_server");
    if (my_send(sock_client,msg,SIZE_MSG_SERV_JOIN)==0){
        close(sock_client);
        close(sock_server);
        return -1;
    }
    
    free(msg);

    return game.game_id;
    
}

char * server_send_port(Game_server g){

    int CODEREQ = g.CODEREQ;
    short ID = g.nb_players-1;
    short EQ;
    if (g.nb_players >2 && g.CODEREQ==10) EQ=1; // si le mode équipe est activé alors EQ est pris en compte
    else EQ=0;
    int PORTUDP = g.PORTUDP;
    int PORTMDIFF = g.PORTMDIFF;
    char * ADRMDIFF = malloc(16*sizeof(char));
    memcpy(ADRMDIFF,g.ADRMDIFF,16*sizeof(char));
    size_t len = SIZE_MSG_SERV_JOIN*sizeof(char); // 2 + 2 + 2 + 16 octets
    char * msg = malloc(len);
    memset(msg,0,len);
    char * header = ready_to_play(CODEREQ,ID,EQ);
    if (header==NULL) return NULL;
    PORTUDP = htons(PORTUDP);
    PORTMDIFF = htons(PORTMDIFF);
    convert_bigendian(ADRMDIFF,16);
    memcpy(msg,header,2);
    memcpy(msg+2,&PORTUDP,2);
    memcpy(msg+4,&PORTMDIFF,2);
    memcpy(msg+6,ADRMDIFF,16);
    free(header);
    free(ADRMDIFF);
    return msg;
}

int player_can_join_game(Game_server g,int CODEREQ){
    return g.nb_players!=4 && g.CODEREQ==CODEREQ; // la partie n'est pas full et c'est le bon mode
}

Game_server trouve_partie_dispo(int CODEREQ, int sock_client){

    Game_server g;     
    int ind_game_dispo;
    short fixed = 0;
    for (size_t i = 0;i<MAX_PENDING_GAME;i++){ // cherhcher si adr n'est pas deja utilisée.
        if (player_can_join_game(games[i],CODEREQ)) {
            games[i].nb_players += 1;
            games[i].sock_tcp_client[games[i].nb_players-1] = sock_client;
            memcpy(&g,&games[i],sizeof(g));
            return g;
        }
        else if (!fixed && games[i].CODEREQ==-1){ // indice si les games sont fulls 
            ind_game_dispo = i;
            fixed =1;
        }
    }
    if (fixed==0){ // toutes les games sont full
        printf("Plus de partie dispo --  to do --\n");
        memset(&g,0,sizeof(g));
        return g;
    }

    int * i = malloc(sizeof(int));
    *i = ind_game_dispo;
    // créer la socket udp avant le thread pour éviter les problèmes de concurrence
    
    int sock_server_udp = socket(PF_INET6,SOCK_DGRAM,0);
    if (sock_server_udp<0) return g;

    struct sockaddr_in6 address_sock;
    memset(&address_sock, 0, sizeof(address_sock));
    address_sock.sin6_family = AF_INET6;
    address_sock.sin6_port = htons(games[ind_game_dispo].PORTUDP);
    address_sock.sin6_addr=in6addr_any;
    //reuseaddr pour eviter les erreurs de bind quand on relance trop vite le serveur
    setsockopt(sock_server_udp,SOL_SOCKET,SO_REUSEADDR,&(int){1},sizeof(int));

    int r = bind(sock_server_udp, (struct sockaddr *) &address_sock, sizeof(address_sock));
    if (r < 0) {
        perror("erreur bind");
        exit(2);
    }
    games[ind_game_dispo].sock_udp = sock_server_udp;
    games[ind_game_dispo].nb_players += 1;
    games[ind_game_dispo].CODEREQ = CODEREQ;
    games[ind_game_dispo].sock_tcp_client[games[ind_game_dispo].nb_players-1] = sock_client;
    // création d'une nouvelle partie
    if (pthread_create(&(tab_thread[ind_game_dispo]),NULL,thread_new_game,i)){
        perror("thread_create");
        exit(1);
    }
    
    memcpy(&g,&games[ind_game_dispo],sizeof(g));
    return g;
}


char * generate_ARMDIFF(int i){ 
    char * ADRMDIFF = calloc(16,sizeof(char));
    sprintf(ADRMDIFF,"ff12::%X",i); // convertir i en base 16;
    return ADRMDIFF;
}
void initialisation_games(){
    Game_server g;
    memset(&g,0,sizeof(g));
    g.CODEREQ=-1;
    for (int i = 0;i<MAX_PENDING_GAME;i++) {
        g.PORTUDP = 1234+i;
        g.PORTMDIFF = 4321-i;
        g.game_id = i;
        g.ADRMDIFF = generate_ARMDIFF(i); // a free à la toute fin
        games[i] = g;
    }
}
void remove_game(Game_server g){
    int i = g.game_id;
    g.CODEREQ = -1;
    g.nb_players= 0;
    games[i] = g;
}
void random_walls(char * grid,int8_t len_h,int8_t len_w){
    int nb_walls = (len_h*len_w)/10;
    for (int i = 0;i<nb_walls;i++){
        int x = rand()%len_w;
        int y = rand()%len_h;
        int len = rand()%10+1;
        int nb_of_breakable = len - 1;
        bool vertical = rand()%2;
        for (int j = 0;j<len;j++){
            int pos = y*len_w+x + (vertical?j*len_w:j);
            if (pos>=len_h*len_w) break;
            if (grid[pos] == 0){
                
            if (rand()%2==0 && nb_of_breakable>0){
                grid[pos] = 2;
                nb_of_breakable--;
            } else{
                grid[pos] = 1;
            }
            }
        }
    }
}
char * create_complete_grid(int8_t len_h,int8_t len_w){
    char * grid = malloc(len_h*len_w*sizeof(char));
    memset(grid,0,len_h*len_w);
    grid [0] = 5;
    grid [len_w-1] = 6;
    grid [(len_h-1)*len_w] = 7;
    grid [len_h*len_w-1] = 8;
    //Create random walls
    random_walls(grid,len_h,len_w);
    return grid;
}

void* recv_update_from_player(void * arg){
    Game_state_server * game_state = (Game_state_server *) arg;
    int sock = game_state->sock_server_udp;
    char ** update = game_state->update;
    int len_update_cache = game_state->len_cache_update;
    int code_game = game_state->code_game;

    for (int i = 0;i<len_update_cache;i++){
        update[i] = NULL;
    }
    while (1){
        char * msg = malloc(SIZE_MSG_PROGRESS*sizeof(char));
        memset(msg,0,SIZE_MSG_PROGRESS);
        //need to wait when send update

        if ((msg=my_recv(sock,SIZE_MSG_PROGRESS))==NULL){
            close(sock);
            pthread_exit(NULL);
        }
        header h;
        convert_littleendian(msg,2);
        convert_littleendian(msg+2,2);
        memcpy(&h,msg,2);
        if (h.CODEREQ == code_game){
            int i = 0;
            while (update[i]!=NULL){
                i++;
            }
            if (i >= len_update_cache){
                perror("update cache full");
                close(sock);
                pthread_exit(NULL);
            }
            update[i] = msg;
        }
    }
}

void explode_bombe(Game_state_server *game_state, Bombe b){
    int x = b.x;
    int y = b.y;
    game_state->map[y*game_state->len_w+x] = 0;
    for (int i = 0; i<4; i++){
        if (x == game_state->player[i].x && y == game_state->player[i].y){
            game_state->player[i].alive = 0;
        }
    }
    for(int k = -2; k <= 2; k+=4){
        if(y+(k/2) >= 0 && y+(k/2) < game_state->len_h){
            if(game_state->map[(y+(k/2))*game_state->len_w+x] == 0 || game_state->map[(y+(k/2))*game_state->len_w+x] == 4){ // si le mur au nord/sud est est une case vide
                if(y+k >= 0 && y+k < game_state->len_h){
                    if (game_state->map[(y+k)*game_state->len_w+x] == 2){
                        game_state->map[(y+k)*game_state->len_w+x] = 4;
                    }
                    if (game_state->map[(y+k)*game_state->len_w+x] == 3){
                        //explode_bombe(game_state,game_state->bombes[(y+k)*game_state->len_w+x]);   
                    }
                    //remplacer par un if pour les joueurs puisque qu'un joueur peut être caché sous une bombe
                    for (int i = 0; i< 4; i ++){
                        if (x == game_state->player[i].x && y+k == game_state->player[i].y){
                            game_state->player[i].alive = 0;
                            game_state->map[(y+k)*game_state->len_w+x] = 0;
                        } 
                    }
                } 
            }
        }
        if(x+(k/2) >= 0 && x+(k/2) < game_state->len_w){
            if(game_state->map[y*game_state->len_w+(x+(k/2))] == 0 || game_state->map[y*game_state->len_w+(x+(k/2))] == 4){ // si le mur à l'est/ouest est une case vide
                if(x+k >= 0 && x+k < game_state->len_w){
                    if (game_state->map[y*game_state->len_w+(x+k)] == 2){
                        game_state->map[y*game_state->len_w+(x+k)] = 4;
                    }
                    if (game_state->map[y*game_state->len_w+(x+k)] == 3){    // cas ou une bombe est sur une cas à l'est ou à l'ouest
                        //explode_bombe(game_state,game_state->bombes[y*game_state->len_w+(x+k)]);
                    }
                    for (int i = 0; i< 4; i ++){
                        if (x+k == game_state->player[i].x && y == game_state->player[i].y){
                            game_state->player[i].alive = 0;
                            game_state->map[y*game_state->len_w+(x+k)] = 0;
                        } 
                    }
                }
            }
        }
    }
    for(int i = -1; i<=1; i++){
        for(int j= -1; j<=1; j++){
            if(i == 0 && j == 0){
                continue;
            }
            if(y+i >= 0 && y+i < game_state->len_h && x+j>=0 && x+j< game_state->len_w){
                if(game_state->map[(y+i)*game_state->len_w+(x+j)] == 3){
                    //explode_bombe(game_state,game_state->bombes[(y+i)*game_state->len_w+(x+j)]);
                }
                if (game_state->map[(y+i)*game_state->len_w+(x+j)] == 2){
                    game_state->map[(y+i)*game_state->len_w+(x+j)] = 4;
                }
                for (int k = 0; k< 4; k ++){
                        if (x+j == game_state->player[k].x && y+i == game_state->player[k].y){
                            game_state->player[k].alive = 0;
                            game_state->map[(y+i)*game_state->len_w+(x+j)] = 0;
                        } 
                    }
            }
        }
    }

    game_state->bombes[b.num].x = -1;
    game_state->bombes[b.num].y = -1;
    game_state->bombes[b.num].num = -1;
    return;
}


void *thread_bombe(void *arg){
    infoBombe * ib = (infoBombe *) arg;
    Game_state_server * game_state = ib->game;
    int i = ib->i;
    sleep(3);
    pthread_mutex_lock(&verrou2);
    if(game_state->bombes[i].x == -1){
        pthread_mutex_unlock(&verrou2);
        pthread_exit(NULL);
        return NULL;
    }

    Bombe b = game_state->bombes[i];
    explode_bombe(game_state,b);
    //Bombe *b = dequeue(game_state->queueBombe);
    pthread_mutex_unlock(&verrou2);

    

    return NULL;
}

int action_to_update(char * edit,header h, message_progress message, Game_state_server * game_state){
    int id = h.ID;
    int x = game_state->player[id].x;
    int y = game_state->player[id].y;
    ACTION action = message.action;
    if (valid_move(game_state->map,game_state->len_h,game_state->len_w,x,y,action, game_state->player[id].alive) == 0){
        return -1;
    }
    switch (action){
        case NONE:
            break;
        case UP:
                edit[0] = x;
                edit[1] = y;
                if (game_state->map[y*game_state->len_w+x] != 3){ // si il y a pas de bombe
                    edit[2] = 0;
                    game_state->map[y*game_state->len_w+x] = 0;
                }
                else {
                    edit[2] = 3;
                }
                edit[3] = x;
                edit[4] = y-1;
                edit[5] = id + 5;
                game_state->player[id].y = y-1;
                game_state->map[(y-1)*game_state->len_w+x] = id+5;
            break;
        case DOWN:
                edit[0] = x;
                edit[1] = y;
                if (game_state->map[y*game_state->len_w+x] != 3){ // si il y a pas de bombe
                    edit[2] = 0;
                    game_state->map[y*game_state->len_w+x] = 0;
                }
                else {
                    edit[2] = 3;
                }
                edit[3] = x;
                edit[4] = y+1;
                edit[5] = id + 5;
                game_state->player[id].y = y+1;
                game_state->map[(y+1)*game_state->len_w+x] = id+5;
            break;
        case LEFT:
                edit[0] = x;
                edit[1] = y;
                if (game_state->map[y*game_state->len_w+x] != 3){ // si il y a pas de bombe
                    edit[2] = 0;
                    game_state->map[y*game_state->len_w+x] = 0;
                }
                else {
                    edit[2] = 3;
                }
                edit[3] = x-1;
                edit[4] = y;
                edit[5] = id + 5;
                game_state->player[id].x = x-1;
                game_state->map[y*game_state->len_w+x-1] = id+5;
            break;
        case RIGHT:
                edit[0] = x;
                edit[1] = y;
                if (game_state->map[y*game_state->len_w+x] != 3){ // si il y a pas de bombe
                    edit[2] = 0;
                    game_state->map[y*game_state->len_w+x] = 0;
                }
                else {
                    edit[2] = 3;
                }
                edit[3] = x+1;
                edit[4] = y;
                edit[5] = id + 5;
                game_state->player[id].x = x+1;
                game_state->map[y*game_state->len_w+x+1] = id+5;
            break;
        case BOMB:
            edit [0] = x;
            edit [1] = y;
            edit [2] = 3;
            pthread_mutex_lock(&verrou);
            int bombeNum = 0;
            for(int i = 0; i<game_state->len_h*game_state->len_w; i++){
                if(game_state->bombes[i].x == -1){
                    game_state->bombes[i].x = x;
                    game_state->bombes[i].y = y;
                    game_state->bombes[i].num = i;
                    bombeNum = i;
                    break;

                }
            }
            Bombe b = {x,y,bombeNum};
            infoBombe *ib = malloc(sizeof(infoBombe));
            ib->game = game_state;
            ib->i = bombeNum;

            game_state->bombes[b.num] = b;
            pthread_mutex_unlock(&verrou);
            pthread_t t = pthread_create(&t,NULL,thread_bombe,(void *)ib);
            game_state->map[y*game_state->len_w+x] = 3;
            break;
        case QUIT:
            break;
        default:
            break;
    }
    return 0;
}


// void alarm_handler(int sig){
//     return;
// }

int finDePartie(Game_state_server * game_state){    // 0 si la partie n'est pas finie, 1 si la partie est finie en mode 1 joueur, 2 si la partie est finie en mode 2 joueurs
    int mode_jeu = game_state->mode;
    int nb_alive = 0;
    for (int i = 0;i<4;i++){
        if (game_state->player[i].alive == 1){
            nb_alive++;
        }
    }
    if(nb_alive == 0){
        return 0;
    }
    if (nb_alive == 1 && mode_jeu == 1 ){
        return 1;
    }else if(nb_alive == 1 && mode_jeu == 2){
        return 2;

    }else if(nb_alive == 2 && mode_jeu == 2){
        for(int i = 0; i<4; i++){
            if(game_state->player[i].alive == 1){
                for(int j = 0; j<4; j++){
                    if(i != j){
                        if(game_state->player[j].alive == 1){
                            if(game_state->player[i].team == game_state->player[j].team){
                                return 2;
                            }else{
                                return 0;
                            }
                        }
                    }
                }
            }
        }

    }
    return 0;
}

int winner(Game_state_server * game_state){
    for(int i = 0; i<4; i++){
        if(game_state->player[i].alive == 1){
            return game_state->player[i].id;
        }
    }
    return 0;

}

int winnerTeam(Game_state_server * game_state){
    for(int i = 0; i<4; i++){
        if(game_state->player[i].alive == 1){
            return game_state->player[i].team;
        }
    }
    return 0;

}

void *threadGetMessages(void *arg){
    Game_state_server * game_state = (Game_state_server *) arg;
    int listSockClient[4];
    for(int i = 0; i<4; i++){
        listSockClient[i] = game_state->sockClientlist[i];
    }
    while(1){
        fd_set readfds;
        FD_ZERO(&readfds);
        int max = 0;
        for(int i = 0; i<4; i++){
            FD_SET(listSockClient[i],&readfds);
            if(listSockClient[i] > max){
                max = listSockClient[i];
            }
        }
        select(max+1,&readfds,NULL,NULL,NULL);
        for(int i = 0; i<4; i++){
            if FD_ISSET(listSockClient[i],&readfds){
                header h;
                char * msh_header_len;
                if((msh_header_len = my_recv(listSockClient[i], 3)) == NULL){
                    perror("coupure du tchat TCP");
                    pthread_exit(NULL);
                }
                convert_littleendian(msh_header_len,2);
                memcpy(&h,msh_header_len,2);
                int len = (int) msh_header_len[2];
                if (len <= 0){
                    free(msh_header_len);
                    continue;
                }
                char * m;
                if((m = my_recv(listSockClient[i],len)) == NULL){
                    perror("coupure du tchat TCP");
                    pthread_exit(NULL);
                }

                if(h.CODEREQ == 7){
                    
                        header h2 = {13, game_state->player[i].id, game_state->player[i].team};
                        char * msg = malloc((3+len)*sizeof(char));
                        memcpy(msg,&h2,2);
                        convert_bigendian(msg,2);
                        msg[2] = len;
                        memcpy(msg+3,m,len);
                    for(int j = 0; j<4; j++){
                            if(my_send(listSockClient[j],msg,len+3)<=0){
                                perror("coupure du tchat TCP");
                                pthread_exit(NULL);
                            }
                        }
                    free(msg);
                }
                if(h.CODEREQ == 8){
                    header h2 = {14, game_state->player[i].id, game_state->player[i].team};
                    char * msg = malloc((3+len)*sizeof(char));
                    memcpy(msg,&h2,2);
                    convert_bigendian(msg,2);
                    msg[2] = len;
                    memcpy(msg+3,m,len);
                    for(int j = 0; j<4; j++){
                        if(game_state->player[j].team == game_state->player[i].team){
                            if(my_send(listSockClient[j],msg,len+3)<=0){
                                perror("coupure du tchat TCP");
                                pthread_exit(NULL);
                            }
                        }
                    }
                }
                free(m);
                free(msh_header_len);
            }
            
        }

    }
}

void* thread_send_update_to_player(void * arg){

    Game_state_server * game_state = (Game_state_server *) arg;
    int sock = game_state->sock_server_mdiff;
    char ** update = game_state->update;
    int len_update_cache = game_state->len_cache_update;

    uint16_t num = 0;

    while (1)
    {
    usleep(10000); // 10ms
    int nb_update = 0;
    for (int i = 0;i<len_update_cache;i++){
        if (update[i]!=NULL){
            message_progress m;

            memcpy(&m,update[i]+2,2);
            if (m.action == LEFT || m.action == RIGHT || m.action == UP || m.action == DOWN){
                nb_update++;
            }
            nb_update++;
        }
    }
    char * edit = malloc(nb_update* 3 * sizeof(char));
    int j = 0;
    int nb_update_save = nb_update;
    for (int i = 0;i<len_update_cache;i++){
        if (update[i]!=NULL){
            message_progress m;
            header h;
            memcpy(&h,update[i],2);
            memcpy(&m,update[i]+2,2);
            
            free(update[i]);
            update[i] = NULL;

            if (action_to_update(edit+j,h,m, game_state) == -1) {
                if (nb_update>0){
                nb_update--;
                if (m.action == LEFT || m.action == RIGHT || m.action == UP || m.action == DOWN){
                    nb_update--;
                }
                }
            }
            if (m.action == LEFT || m.action == RIGHT || m.action == UP || m.action == DOWN){
                j+=3;
            }
            j+=3;
        }

        
    }

    int end = finDePartie(game_state);
    if(end == 0){

    }
    // if(end == 1 ){
    //     int winnerP = winner(game_state);
    //     header h = {15, winnerP, 0};
    //     printf("fin de partie\n");
    // }
    // if(end == 2){
    //     int winnerT = winner(game_state);
    //     header h = {16, 0, winnerT};
        
    //     printf("fin de partie en equipe\n");
    // }
    if(end == 1){
        int winnerP = winner(game_state);
        printf("Le gagnant est : %d",winnerP);
        header h = {15, winnerP, 0};
        char * msg = malloc(2*sizeof(char));
        memcpy(msg,&h,2);
        convert_bigendian(msg,2);
        if (send_multicast(sock,msg,2,(struct sockaddr*)&game_state->addr_mdiff,sizeof(game_state->addr_mdiff))==0){
        close(sock);
    }
    free(edit);
    free(msg);
    pthread_exit(NULL);
    } else if(end == 2){
        int winnerT = winnerTeam(game_state);
        printf("L'equipe gagnante est : %d",winnerT);
        header h = {16, 0, winnerT};
        char * msg = malloc(2*sizeof(char));
        memcpy(msg,&h,2);
        convert_bigendian(msg,2);
        if (send_multicast(sock,msg,2,(struct sockaddr*)&game_state->addr_mdiff,sizeof(game_state->addr_mdiff))==0){
        close(sock);
    }
    free(edit);
    free(msg);
    pthread_exit(NULL);
    }
    header h = {11,0,0};
    char * msg = malloc((3*nb_update_save+5)*sizeof(char));
    memcpy(msg,&h,2);
    convert_bigendian(msg,2);
    int num_to_send = htons(nb_update_save);
    memcpy(msg+2,&num_to_send,2);
    msg[4] = nb_update;
    memcpy(msg+5,edit,3*nb_update);
    if (send_multicast(sock,msg,3*nb_update+5,(struct sockaddr*)&game_state->addr_mdiff,sizeof(game_state->addr_mdiff))==0){
        close(sock);
        pthread_exit(NULL);
    }
    free(edit);
    free(msg);
    num++;
    }

}
void* thread_mdiff_grid_complete(void * arg){
    Game_state_server * game_state = (Game_state_server *) arg;
    char * msg_grid = malloc(sizeof(char)*(SIZE_MSG_HEADER_GRID + game_state->len_h*game_state->len_w));
    header h = create_header(12,0,0);
    int num = 0;
    memcpy(msg_grid,&h,2);
    convert_bigendian(msg_grid,2);
    while(1){
    //integrity check of the grid (not useful i test and no error in the grid)
    //integrity_check(game_state->map,game_state->len_h,game_state->len_w);
    memset(msg_grid+2,0,SIZE_MSG_HEADER_GRID - 2 + game_state->len_h*game_state->len_w);
    int num_to_send = htons(num);
    memcpy(msg_grid+2,&num_to_send,2);
    msg_grid[4] = game_state->len_h;
    msg_grid[5] = game_state->len_w;
    memcpy(msg_grid+6,game_state->map,game_state->len_h*game_state->len_w);
    if(send_multicast(game_state->sock_server_mdiff,msg_grid,SIZE_MSG_HEADER_GRID + game_state->len_h*game_state->len_w,(struct sockaddr*)&game_state->addr_mdiff,sizeof(game_state->addr_mdiff))==0){
        pthread_exit(NULL);
    }
    sleep(1);
    num++;
    }
}

void* thread_new_game(void * arg){

    

    printf("thread new game\n");
    int index_of_game = *((int *) arg);
    Game_server g = games[index_of_game];
    int sock_server_udp = g.sock_udp;
    // Multicast
    struct sockaddr_in6 address_sock_mdiff;
    memset(&address_sock_mdiff, 0, sizeof(address_sock_mdiff));
    address_sock_mdiff.sin6_family = AF_INET6;
    address_sock_mdiff.sin6_port = htons(g.PORTMDIFF);
    address_sock_mdiff.sin6_scope_id = if_nametoindex("eth0");
    inet_pton(AF_INET6, g.ADRMDIFF, &address_sock_mdiff.sin6_addr);

    int sock_server_mdiff = socket(PF_INET6,SOCK_DGRAM,0);
    if (sock_server_mdiff<0) pthread_exit(NULL);;
    int ttl = 1;
    setsockopt(sock_server_mdiff,IPPROTO_IPV6,IPV6_MULTICAST_HOPS,&ttl,sizeof(ttl));

    
    int8_t len_h = 20;
    int8_t len_w = 20;

    int mode_jeu ;

    // get list of all players
    players * list = malloc(sizeof(players)*4);
    char * msg;
    for (int i = 0;i<4;i++){
        if ((msg=my_recv(sock_server_udp,SIZE_MSG_CLIENT))==NULL){
            for(int i = 0; i<4; i++){
                close(g.sock_tcp_client[i]);
            }
            close(sock_server_udp);
            close(sock_server_mdiff);
            pthread_exit(NULL);
        }
        header h;
        convert_littleendian(msg,2);
        memcpy(&h,msg,2);
        players p;
        if (h.CODEREQ == g.CODEREQ - 6){
            mode_jeu = h.CODEREQ-2;
            p.alive = 1;
            switch (h.ID){
                case 0:
                    p.team = h.EQ;
                    p.id = 0;
                    p.x = 0;
                    p.y = 0;
                    break;
                case 1:
                    p.team = h.EQ;
                    p.id = 1;
                    p.x = len_w-1;
                    p.y = 0;
                    break;
                case 2:
                    p.team = h.EQ;
                    p.id = 2;
                    p.x = 0;
                    p.y = len_h-1;
                    break;
                case 3:
                    p.team = h.EQ;
                    p.id = 3;
                    p.x = len_w-1;
                    p.y = len_h-1;
                    break;
                default:
                    perror("error id");
                    break;
            }
            
            list[i] = p;
        }
        free(msg);
    }
    
    printf("List of players\n");
    for (int i = 0;i<4;i++){
        printf("team = %d id = %d\n",list[i].team,list[i].id);
    }
    //can now send grid to all players
    char * grid = create_complete_grid(len_h,len_w);
    char * msg_grid = calloc((SIZE_MSG_HEADER_GRID + len_h*len_w),sizeof(char));
    header h = create_header(11,0,0);
    memcpy(msg_grid,&h,2);
    msg_grid[2] = 0;
    msg_grid[3] = 0;
    msg_grid[4] = len_h;
    msg_grid[5] = len_w;
    memcpy(msg_grid+6,grid,len_h*len_w);
    if(send_multicast(sock_server_mdiff,msg_grid,SIZE_MSG_HEADER_GRID + len_h*len_w,(struct sockaddr*)&address_sock_mdiff,sizeof(address_sock_mdiff))==0){
            for(int i = 0; i<4; i++){
                close(g.sock_tcp_client[i]);
            }
        close(sock_server_udp);
        close(sock_server_mdiff);
        pthread_exit(NULL);
    }
    printf("grid sent\n");

    char ** update_cache = calloc(LEN_UPDATE_CACHE,sizeof(char*));
    Bombe *bombes = malloc(sizeof(Bombe)*len_h*len_w);
    for(int i = 0; i<len_h*len_w; i++){
        bombes[i].x = -1;
        bombes[i].y = -1;
        bombes[i].num = 0;
    }
    int * socket_list = malloc(sizeof(int)*4);
    for(int i = 0; i<4; i++){
        socket_list[i] = games[index_of_game].sock_tcp_client[i];
    }
    Game_state_server game_state = {grid,len_h,len_w,list,sock_server_mdiff,address_sock_mdiff, sock_server_udp,update_cache,LEN_UPDATE_CACHE,g.CODEREQ - 4, bombes, mode_jeu, socket_list};

    pthread_t tab_thread_udp;
    //pthread create
    if (pthread_create(&tab_thread_udp,NULL,recv_update_from_player,&game_state)){
        perror("thread_create");
            for(int i = 0; i<4; i++){
                close(g.sock_tcp_client[i]);
            }
        close(sock_server_udp);
        close(sock_server_mdiff);
        pthread_exit(NULL);
    }


    
    pthread_t tchat_thread;
    if(pthread_create(&tchat_thread, NULL,threadGetMessages, &game_state)){
            for(int i = 0; i<4; i++){
                close(g.sock_tcp_client[i]);
            }
        close(sock_server_udp);
        close(sock_server_mdiff);
        pthread_exit(NULL);

    }

    //send with multicast update from players asynch with game_state
    pthread_t tab_thread_mdiff; 
    if (pthread_create(&tab_thread_mdiff,NULL,thread_send_update_to_player,&game_state)){
        perror("thread_create");
            for(int i = 0; i<4; i++){
                close(g.sock_tcp_client[i]);
            }
        close(sock_server_udp);
        close(sock_server_mdiff);
        pthread_exit(NULL);
    }

    //send multicast one second the grid
    pthread_t tab_thread_mdiff_grid;
    if (pthread_create(&tab_thread_mdiff_grid,NULL,thread_mdiff_grid_complete,&game_state)){
        perror("thread_create");
            for(int i = 0; i<4; i++){
                close(g.sock_tcp_client[i]);
            }
        close(sock_server_udp);
        close(sock_server_mdiff);
        pthread_exit(NULL);
    }
    //si un joueur quitte la partie alors le thread s'arrete et on arrête la partie
    pthread_join(tchat_thread,NULL); // attendre la fin du thread
    //shutdown all threads
    pthread_cancel(tab_thread_udp);
    pthread_cancel(tab_thread_mdiff);
    pthread_cancel(tab_thread_mdiff_grid);

    for (int i = 0;i<4;i++){
        close(games[index_of_game].sock_tcp_client[i]);
    }
    free(socket_list);
    free(bombes);
    
    free(list);
    free(update_cache);


    free(msg_grid);
    free(grid);
    // gerer la partie
    close(sock_server_udp);
    close(sock_server_mdiff);
    remove_game(g);
    pthread_exit(NULL);
}

 
