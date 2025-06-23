#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include "misc.h"

bool valid_move(char* map,int len_w, int len_h, int x, int y, ACTION a, bool alive);

#endif