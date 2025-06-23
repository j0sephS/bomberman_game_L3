#include <stdio.h>
#include <stdlib.h>
#include "misc.h"

#ifndef __COMMUNICATION__H__

    #define __COMMUNICATION__H__
    

    

    typedef struct{
        unsigned int num : 13;
        unsigned int action: 3;
    }message_progress;

    char * create_msgs(header,int,ACTION,size_t, char *,mode_teams,mode_msg);
    int correct_args(int,short,short,short,short,mode_teams,mode_msg);
    
    char * join_party(int);
    char * ready_to_play(int,short,short);
    char * party_in_progress(header,int,int);
    char * write_tchat(int,short,short,size_t,char *);
    
    header create_header(int,short,short);
    

    int my_send(int,char *,size_t);
    int send_multicast(int sock,char * buf,size_t size ,struct sockaddr* addr , socklen_t len);
    char * my_recv(int,size_t);
    char * recv_multicast (int sock, struct sockaddr* addr, socklen_t len, size_t size);

#endif
    