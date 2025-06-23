#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#ifndef __MISC_H__

    #define __MISC_H__
    #define SIZE_MSG_CLIENT 2
    #define SIZE_MSG_SERV_JOIN 22
    #define SIZE_MSG_HEADER_GRID 6
    #define SIZE_MSG_HEADER_UPDATE 5
    #define SIZE_MSG_HEADER 2
    #define SIZE_MSG_PROGRESS 4

    typedef struct {
        unsigned int CODEREQ : 13;
        unsigned short ID : 2;
        unsigned short EQ : 1;
    }header;

    typedef enum ACTION { NONE, UP, DOWN, LEFT, RIGHT, BOMB, QUIT } ACTION;

    typedef enum {
        Join,
        Ready,
        Progress,
        Tchat
    } mode_msg;

    typedef enum {
        MODE_ERROR,
        MODE_4_ADV,
        MODE_2_TEAMS
    } mode_teams;

    


    void convert_bigendian(char *,size_t);
    void convert_littleendian(char *,size_t);
    void set_timeout(int,int);
    void write_msg_to_file(char *,size_t,char *);
    void print_header(header);

#endif
    