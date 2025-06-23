#include "misc.h"
#include "communication.h"

void convert_bigendian(char * msg,size_t len){
  if (len<=1){
    return;
  }
  if (len==2){
    uint16_t * p = (uint16_t *)msg;
    *p = htons(*p);
    return;
  }
  if (len==4){
    uint32_t * p = (uint32_t *)msg;
    *p = htonl(*p);
    return;
  }
  return;
}
void convert_littleendian(char * msg,size_t len){
  if (len<=1){
    return;
  }
  if (len==2){
    uint16_t * p = (uint16_t *)msg;
    *p = ntohs(*p);
    return;
  }
  if (len==4){
    uint32_t * p = (uint32_t *)msg;
    *p = ntohl(*p);
    return;
  }
  return;
}



struct timeval{
    long tv_sec;
    long tv_usec;
};

void set_timeout(int socket,int sec){
  struct timeval timeout;
  timeout.tv_sec = sec;
  timeout.tv_usec = 0;
  setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));
}

void write_msg_to_file(char * msg,size_t len,char * filename){
  FILE * f = fopen(filename,"w");
  if (f==NULL){
    perror("error opening file");
    return;
  }
  for (size_t i=0;i<len;i++){
    fprintf(f,"%c",msg[i]);
  }
  fclose(f);
}
void print_header(header h){
  printf("CODEREQ = %d\n",h.CODEREQ);
  printf("ID = %d\n",h.ID);
  printf("EQ = %d\n",h.EQ);
}