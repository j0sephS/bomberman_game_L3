#include "communication.h"
#include <fcntl.h>
#include "curses.h"


header create_header(int CODEREQ,short ID,short EQ){
  header h = {CODEREQ,ID,EQ};
  return h;
}

char * create_msgs(header h,int NUM,ACTION a,
size_t LEN, char * DATA,
mode_teams mt,mode_msg mm){

  if (!correct_args(h.CODEREQ,h.ID,h.EQ,NUM,a,mt,mm)){
      // erreur
      return NULL;
  }

  switch (mm){
    case Join : return join_party(h.CODEREQ); // integrer la partie

    case Ready : return ready_to_play(h.CODEREQ,h.ID,h.EQ); // prêt à jouer
  
    case Progress : return party_in_progress(h,NUM,a); // deroulement;

    case Tchat : return write_tchat(h.CODEREQ,h.ID,h.EQ,LEN,DATA); // ecrire dans le tcaht;

    default : return NULL;
  }
}

// Test messages bon format
int correct_args(int CODEREQ,short ID,short EQ,short NUM,short ACTION,mode_teams mt,mode_msg mm){
  switch(mm){

    case Join : 
    return CODEREQ==1 || CODEREQ==2; // integrer la partie

    case Ready : return (CODEREQ==3 || (CODEREQ==4 && mt==MODE_2_TEAMS && (EQ==0 || EQ==1))) // prêt à jouer
        &&  (ID >= 0 && ID <= 3);
  

    case Progress : return (CODEREQ==5 || (CODEREQ==6 && mt==MODE_2_TEAMS  && (EQ==0 || EQ==1))) // deroulement
        &&  (ID >= 0 && ID <= 3)
        &&  (NUM >= 0 && NUM < 8192) // NUM est le numéro du message modulo 2^(13)=8192
        &&  (ACTION >= 0 && ACTION <= 5);

    case Tchat : return (CODEREQ==7 || (CODEREQ==8 && mt==MODE_2_TEAMS   && (EQ==0 || EQ==1))) // tchat
      &&  (ID >= 0 && ID <= 3);
  }

  return -1;
}
char * join_party(int CODEREQ){

  header h = {CODEREQ,0,0};
  char * msg = malloc(2*sizeof(char));
  memcpy(msg,&h,2);
  convert_bigendian(msg,2);
  return msg;
}
char * ready_to_play(int CODEREQ,short ID,short EQ){

  char * msg = malloc(2*sizeof(char)); 
  header h = {CODEREQ,ID,EQ};
  memcpy(msg,&h,2);  
  convert_bigendian(msg,2);
  return msg;

}

char * party_in_progress(header h,int NUM,int a){
  char * msg = malloc(4*sizeof(char)); 
  message_progress mp = {NUM,a};
  memcpy(msg,&h,2);
  convert_bigendian(msg,2);
  memcpy(msg+2,&mp,2);
  convert_bigendian(msg+2,2);
  return msg;
}

char * write_tchat(int CODEREQ,short ID,short EQ,size_t LEN,char * DATA){ 

  LEN = LEN & 0xFF; // LEN sera tronqué (1 octet);
  char * msg = malloc((3+ LEN)*sizeof(char));
  char * header = ready_to_play(CODEREQ,ID,EQ);
  memcpy(msg,header,2);
  msg[2] = LEN;
  memcpy(msg+3,DATA,LEN*sizeof(char));
  free(header);
  return msg;
}

int my_send(int sock,char * buf,size_t size){
    size_t ecrit = 0;
    //printf("send of = %ld\n",size);
    while (ecrit<size){

        ecrit += send(sock,buf+ecrit, size-ecrit, 0);

        if(ecrit <= 0){
            perror("erreur ecriture");
            free(buf);
            return 0;
        }   
    }
    

    return 1;
}

int send_multicast(int sock,char * buf,size_t size, struct sockaddr* addr , socklen_t len){
    size_t ecrit = 0;
    //printf("send multicast of = %ld\n",size);
    ecrit = sendto(sock,buf, size, 0, addr, len);
    if(ecrit != size){
            perror("erreur ecriture");
            free(buf);
            return 0;
        }   
    return 1;
}

char * recv_multicast (int sock, struct sockaddr* addr, socklen_t len, size_t size){ //should be do in oneshot for one message
    char * buf = malloc(size*sizeof(char));
    memset(buf,0,size);
    //printf("recv multicast of = %ld\n",size);
    size_t lu = 0;
    lu = recvfrom(sock, buf, size, 0, addr, &len);
        if(lu <= 0){
            perror("connexion perdue");
            close(sock);
            free(buf);
            return NULL;
        }
    return buf;
}

char * my_recv(int sock,size_t size){
    char * buf = malloc(size*sizeof(char));
    //printf("recv of = %ld\n",size);
    //define a timeout for the recv

    size_t lu = 0;
    while(lu<size){
        lu += read(sock, &(buf[lu]), size-lu);
        if(lu <= 0){
            perror("connexion perdue");
            close(sock);
            free(buf);
            return NULL;
        }
    }

    return buf;
}