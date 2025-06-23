#ifndef CURSE_H
#define CURSE_H

#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"
#include "game.h"

#define TEXT_SIZE 255



typedef struct board {
    char* grid;
    int w;
    int h;
} board;

typedef struct line {
    char data[TEXT_SIZE];
    int cursor;
} line;

typedef struct pos {
    int x;
    int y;
} pos;

ACTION control_and_tchat(line* l, Game_client gc, int socket_tcp);

void setup_board(board* b);
void setup_board_with_game_state(board* board, Game_state_client gs);
void free_board(board* board);
int get_grid(board* b, int x, int y);
void set_grid(board* b, int x, int y, int v);
void refresh_game(board* b, line* l, pos* p, Game_client me, int socket_tcp);
bool perform_action(board* b, pos* p, ACTION a, int ID,bool alive);
bool valid_move(char* map, int len_w,int len_h, int x,int y, ACTION a,bool alive);
void refresh_tchat(line* l, board * b);

#endif


