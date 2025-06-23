
#include "game.h"
#include <stdbool.h>
#include "misc.h"


bool valid_move(char* map,int len_w, int len_h, int x, int y, ACTION a, bool alive){
    if(!alive){
        return false;
    }
    switch (a){
        case NONE:
            break;
        case UP:
            if (y>0) {
            if (map[(y-1)*len_w+x] == 0 || map[(y-1)*len_w+x] == 4){
                return true;
            }
            }
            break;
        case DOWN:
            if (y<len_h-1) {
                if (map[(y+1)*len_h+x] == 0 || map[(y+1)*len_h+x] == 4){
                    return true;
                }
            }
            break;
        case LEFT:
        
            if (x>0) {
                if (map[y*len_w+x-1] == 0 || map[y*len_w+x-1] == 4){
                    return true;
            }
            }
            break;
        case RIGHT:
            if (x<len_w-1) {
                if (map[y*len_w+x+1] == 0 || map[y*len_w+x+1] == 4){
                    return true;
                }
            }
            break;
        case BOMB:
            return true;
            break;
        case QUIT:
            break;
        default:
            break;
    }
    return false;
}