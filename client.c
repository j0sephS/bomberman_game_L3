#define _DEFAULT_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include "affichage.h"
#include "misc.h"
#include "communication.h"
#include "client.h"
#include "curses.h"


#define PORT 7879  //numero de port du serveur
#define ADDR "::1"  //adresse du serveur


#define choose_mode(m) ((m==2)?MODE_2_TEAMS:(m==4)?MODE_4_ADV:MODE_ERROR)


int main(int argc, char ** argv){
  if (argc!=2){
    printf("Missing argument for the number of teams : (2 or 4)\n");
    return 1;
  }

  mode_teams mode = choose_mode(atoi(argv[1]));
  if (mode==MODE_ERROR){
    printf("Invalid mode\n");
    return 1;
  }

  int socket_tcp = create_socket_client(SOCK_STREAM,PORT,ADDR);
  if (socket_tcp==-1){
    perror("failed to create tcp socket");
    return 1;
  }
  set_timeout(socket_tcp,5);

  int r = send_join_msg(socket_tcp,mode);
  if (r==-1){
    perror("failed to send join msg");
    return 1;
  }

  Game_client g = recv_port(socket_tcp);
  print_game_c(g);

  int socket_udp = create_socket_client(SOCK_DGRAM,g.PORTUDP,ADDR);
  if (socket_udp<0) return 1;
  //envoi du message ready
  
  struct sockaddr_in6 address_sock;
  memset(&address_sock, 0,sizeof(address_sock));
  address_sock.sin6_family = AF_INET6;
  address_sock.sin6_port = htons(g.PORTMDIFF);
  address_sock.sin6_addr = in6addr_any; 

  int socket_mdiff = join_multicast_group(g.ADRMDIFF ,address_sock);
  if (socket_mdiff<0) return 1;
  r = send_ready_msg(socket_udp,g);
  if (r==-1){
    printf("failed to send ready msg\n");
    return 1;
  }
  start_game(socket_mdiff,socket_udp,socket_tcp,g,&address_sock);
  
  close(socket_udp);
  close (socket_tcp);
  return 0;
}

Game_state_client get_map(int socket, struct sockaddr_in6 * addr){
  size_t max_size = 65536;
  char * msg = recv_multicast(socket, (struct sockaddr*) addr, sizeof(struct sockaddr_in6), max_size);
  if (msg==NULL){
    perror("error reading");
    close(socket);
    exit(1);
  }
  int len_h = msg[4];
  int len_l = msg[5];
  int len = len_h * len_l;
  char * map = malloc(len*sizeof(char));
  memcpy(map,msg+6,len);
  if (map==NULL){
    perror("error reading");
    close(socket);
    exit(1);
  }
  header h;
  convert_littleendian(msg,2);
  memcpy(&h,msg,2);
  int eq = h.EQ;
  int id = h.ID;
  int codereq = h.CODEREQ;
  convert_littleendian(msg+2,2);
  int num = msg[2] << 8 | msg[3];
  free(msg);
  Game_state_client g = {codereq,id,eq,num,0,len_h,len_l,map};
  return g;
  
}
void update_board(board * b,char * update){
  int x = update[0];
  int y = update[1];
  int v = update[2];
  //integrity check of the udp packet
  if (x < 0 || x >= b->w || y < 0 || y >= b->h || v < 0 || v > 8){
    return;
  }
  set_grid(b,x,y,v);
}
bool get_update(int socket,board * b, Game_state_client g, struct sockaddr_in6 * addr, pos* p, Game_client me){
  size_t max_size = 65536;
  char * msg = recv_multicast(socket, (struct sockaddr*) addr, sizeof(struct sockaddr_in6), max_size);
  if (msg==NULL){
    perror("error reading");
    close(socket);
    exit(1);
  }
  header h;
  convert_littleendian(msg,2);
  memcpy(&h,msg,2);
  if(h.CODEREQ==15){
    //aficher le gagnant sur ncurses
    int num_gagnant = h.ID;
    //afficher dans le tchat
    line * l = malloc(sizeof(line));
    char * msg_tchat = malloc(100*sizeof(char));
    sprintf(msg_tchat,"Le joueur %d a gagne !",num_gagnant);
    l->cursor = strlen(msg_tchat);
    memcpy(l->data,msg_tchat,l->cursor);
    refresh_tchat(l,b);
    sleep(5);
    free(msg_tchat);
    free(msg);
    return false;
  }
  if(h.CODEREQ==16){
    //afficher l'équipe gagnante sur ncurses
    int num_equipe_gagnante = h.EQ;
    line * l = malloc(sizeof(line));
    char * msg_tchat = malloc(100*sizeof(char));
    sprintf(msg_tchat,"L'equipe %d a gagne",num_equipe_gagnante);
    l->cursor = strlen(msg_tchat);
    memcpy(l->data,msg_tchat,l->cursor);
    refresh_tchat(l,b);
    sleep(5);
    free(l);
    free(msg_tchat);
    free(msg);
    return false;
  }
  if (h.CODEREQ==11){
    uint16_t num = msg[2] << 8 | msg[3];
    int nb_updates = msg[4];
    int nb_bytes = nb_updates * 3;
    if (nb_bytes != 0){
    char * updates = malloc(nb_bytes*sizeof(char));
    memcpy(updates,msg+5,nb_bytes);
    if (updates==NULL){
      perror("error reading");
      close(socket);
      exit(1);
    }
    if (g.num_ms == 65535) g.num_ms = 0; // on remet a 0 si on depasse la taille max
    if (num > g.num_ms){ // on verifie que c'est bien la map suivante
      for (int i = 0; i < nb_bytes; i+=3){
        update_board(b,updates+i);
      }
      g.num_ms = num;
    }
    free(updates);
    }
  }
  else if (h.CODEREQ==12){
    uint16_t num = msg[2] << 8 | msg[3];
    int len_h = msg[4];
    int len_l = msg[5];
    int len = len_h * len_l;
    char * map = malloc(len*sizeof(char));
    memcpy(map,msg+6,len);
    if (map==NULL){
      perror("error reading");
      close(socket);
      exit(1);
    }
    if (num > g.num && len_h == g.hauteur && len_l == g.largeur){ // on verifie que c'est bien la map suivante
        memcpy(g.grid,map,len);
        memcpy(b->grid,map,len);
        refresh_game(b,NULL, p, me, socket);
        g.num = num;
    }
    free(map);
  }
  free(msg);
  return true;
}

void send_update(int socket,Game_client g, ACTION a, int* num){
  if (a!=NONE){
    *num += 1;
    int number = *num;
    char * msg;
    header h = {5,g.ID,g.EQ};
    if (g.CODEREQ==10){
      h.CODEREQ = 6;
    }
    message_progress progress = {number,a};
    msg = calloc(4,sizeof(char));
 
    memcpy(msg,&h,2);
    convert_bigendian(msg,2);
    memcpy(msg+2,&progress,2);
    convert_bigendian(msg+2,2);
    //write_msg_to_file(msg,SIZE_MSG_PROGRESS,"update_msg_client");// pour debug
    if (my_send(socket,msg,SIZE_MSG_PROGRESS)==0){
      perror("error writing");
      free(msg);
      close(socket);
      exit(1);
    }
    free(msg);
  }
}


void start_game(int socket_mdiff,int socket_udp,int socket_tcp, Game_client me, struct sockaddr_in6 * addr){
    socket_tcp = socket_tcp; // pour eviter warning
    socket_udp = socket_udp;

    Game_state_client map = get_map(socket_mdiff, addr);
    board * b = malloc(sizeof(board));
    line* l = malloc(sizeof(line));
    l->cursor = 0;
    pos* p = malloc(sizeof(pos));
    switch (me.ID)
    {
    case 0:
        p->x = 0;
        p->y = 0;
        break;
    case 1:
        p->x = map.largeur-1;
        p->y = 0;
        break;
    case 2:
        p->x = 0;
        p->y = map.hauteur-1;
        break;
    case 3:
        p->x = map.largeur-1;
        p->y = map.hauteur-1;
        break;
    default:
        printf("Invalid ID\n");
        p->x = 0;
        p->y = 0;
        break;
    }
    initscr(); /* Start curses mode */
    raw(); /* Disable line buffering */
    intrflush(stdscr, FALSE); /* No need to flush when intr key is pressed */
    keypad(stdscr, TRUE); /* Required in order to get events from keyboard */
    nodelay(stdscr, TRUE); /* Make getch non-blocking */
    noecho(); /* Don't echo() while we do getch (we will manually print characters when relevant) */
    curs_set(0); // Set the cursor to invisible
    start_color(); // Enable colors
    init_pair(1, COLOR_YELLOW, COLOR_BLACK); // Define a new color style (text is yellow, background is black)

    
    setup_board_with_game_state(b,map); // Initialisation du plateau
    int num = 0;
    bool alive = 1;
    while (true) {

        ACTION a = control_and_tchat(l, me, socket_tcp); // Récupération de l'action
        if (a == QUIT) break;
        if(alive !=0){
          send_update(socket_udp,me,a,&num);
          if (num >= 8192) num = 0;
        }
        if (perform_action(b, p, a, me.ID+5, alive)) break;
        refresh_game(b,l, p, me, socket_tcp);
        if (get_update(socket_mdiff,b, map, addr, p, me) != true) break;
        alive = 0;
        if(get_grid(b,p->x,p->y) == 3){
          alive = 1;
          continue;
        }
        for(int i = 0; i<b->h; i++){
          for(int j = 0; j<b->w; j++){
            if(get_grid(b,j,i) == me.ID+5){
              alive = 1;
            }
          }
        }
        
    }
    free(map.grid);
    free(me.ADRMDIFF);
    free_board(b);
    curs_set(1); // Set the cursor to visible again
    endwin(); /* End curses mode */
    free(p); free(l); free(b);
    close(socket_mdiff);
}

int join_multicast_group(char * adr, struct sockaddr_in6 addr){
  int fd = socket(AF_INET6,SOCK_DGRAM,0);
  if (fd<0){
    perror("echec de la creation de la socket");
    return -1;
  }
  //so_reuseaddr
  setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&(int){1},sizeof(int));
  //bind
  if (bind(fd,(struct sockaddr *) &addr,sizeof(addr))<0){
    perror("echec du bind");
    return -1;
  }

  //ajout au groupe multicast
  struct ipv6_mreq mreq;
  inet_pton(AF_INET6, adr, &mreq.ipv6mr_multiaddr);
  mreq.ipv6mr_interface = if_nametoindex("eth0");

  int r = setsockopt(fd,IPPROTO_IPV6,IPV6_JOIN_GROUP,&mreq,sizeof(mreq));
  if (r<0){
    perror("echec de la demande de connexion au groupe multicast");
    return -1;
  }

  return fd;
}

int create_socket_client(int proto,int port,char * adr){
  //*** creation de la socket ***
  int fdsock = socket(PF_INET6, proto, 0);
  if(fdsock == -1){
    perror("creation socket");
    return -1;
  }
  
  //*** creation de l'adresse du destinataire (serveur) ***
  struct sockaddr_in6 address_sock;
  memset(&address_sock, 0,sizeof(address_sock));
  address_sock.sin6_family = AF_INET6;
  address_sock.sin6_port = htons(port);
  inet_pton(AF_INET6, adr, &address_sock.sin6_addr);

  //*** demande de connexion au serveur ***
  int r = connect(fdsock, (struct sockaddr *) &address_sock, sizeof(address_sock));
  if(r == -1){
    perror("echec de la connexion");
    close(fdsock);
    return -1;
  }

  return fdsock;
}


int send_join_msg(int fd,mode_teams m){ // renvoie 0 si reussite ; -1 sinon;
  char * msg;
  header h; 
  switch (m){
    case MODE_4_ADV : h = create_header(1,-1,-1); break;
    case MODE_2_TEAMS: h = create_header(2,-1,-1); break;
    default : return -1;
  }

  msg = create_msgs(h,-1,-1,0,NULL,0,Join);
  if (msg==NULL) {return -1;}

  //write_msg_to_file(msg,SIZE_MSG_CLIENT,"join_msg_client");// pour debug
  if (my_send(fd,msg,SIZE_MSG_CLIENT)==0){
    perror("error writing");
    free(msg);
    close(fd);
    return -1;
  }

  free(msg);
  return 0;
}

int send_ready_msg(int fd,Game_client g){

  header h;
  switch (g.CODEREQ){
    case 9 : h = create_header(3,g.ID,g.EQ); break; // MODE_4_ADV
    case 10: h = create_header(4,g.ID,g.EQ); break; // MODE_2_TEAMS
    default : return -1;
  }
  char * msg = malloc(SIZE_MSG_CLIENT*sizeof(char));
  memcpy(msg,&h,2);
  convert_bigendian(msg,2);
  if (msg==NULL) {return -1;}
  //write_msg_to_file(msg,SIZE_MSG_CLIENT,"ready_msg_client");// pour debug
  if (my_send(fd,msg,SIZE_MSG_CLIENT)==0){
    perror("erreur ecriture");
    free(msg);
    close(fd);
    return -1;
  }
  free(msg);

  return 0;
}


Game_client recv_port(int socket){

  char * msg = my_recv(socket,SIZE_MSG_SERV_JOIN);

  header header;
  int16_t PORTUDP;
  int16_t PORTMDIFF;
  char ADRMDIFF[16];
  convert_littleendian(msg,2);
  convert_littleendian(msg+2,2);
  convert_littleendian(msg+4,2);
  memcpy(&header,msg,2);
  memcpy(&PORTUDP,msg+2,2);
  memcpy(&PORTMDIFF,msg+4,2);
  memcpy(ADRMDIFF,msg+6,16);
  convert_littleendian(ADRMDIFF,16);

  if (PORTUDP==0 && PORTMDIFF==0){
    printf("Game full attendre qu'une partie se termine");
    //fermer les trucs avant de quitter
    exit(0);
  }

  Game_client g = get_game(header,PORTUDP,PORTMDIFF,ADRMDIFF);
  
  free(msg);
  return g;

}

Game_client get_game(header header,int PORTUDP ,int PORTMDIFF,char* ADRMDIFF){

  Game_client g;
  g.ADRMDIFF = malloc(16*sizeof(char));

  g.PORTUDP = PORTUDP;
  g.PORTMDIFF = PORTMDIFF;

  memcpy(g.ADRMDIFF,ADRMDIFF,16*sizeof(char));
  g.EQ = header.EQ;
  g.ID = header.ID;
  g.CODEREQ = header.CODEREQ;
  return g;
}

void print_game_c(Game_client g){
  printf("o CODEREQ : %d\n\n",g.CODEREQ);
  printf("o ID : %d\n\n",g.ID);
  printf("o EQ : %d\n\n",g.EQ);
  printf("o PORTUDP : %d\n\n",g.PORTUDP);
  printf("o PORTMDIFF : %d\n\n",g.PORTMDIFF);
  printf("o ADRMDIFF : %s\n\n",g.ADRMDIFF);
  printf("--------------------------\n\n");
}
