// Build with -lncurses option
#define _DEFAULT_SOURCE
#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "curses.h"
#include "client.h"
#include "game.h"


void setup_board(board* board) {
    int lines; int columns;
    getmaxyx(stdscr,lines,columns);
    board->h = lines - 2 - 1; // 2 rows reserved for border, 1 row for chat
    board->w = columns - 2; // 2 columns reserved for border
    board->grid = calloc((board->w)*(board->h),sizeof(char));
}
void setup_board_with_game_state(board* board, Game_state_client gs) {
    board->h = gs.hauteur;
    board->w = gs.largeur;
    board->grid = calloc((board->w)*(board->h),sizeof(char));
    memcpy(board->grid,gs.grid,(board->w)*(board->h));
}

void free_board(board* board) {
    free(board->grid);
}

int get_grid(board* b, int x, int y) {
    return b->grid[y*b->w + x];
}

void set_grid(board* b, int x, int y, int v) {
    b->grid[y*b->w + x] = v;
}

void refresh_game(board* b, line* l, pos* p, Game_client me, int socket_tcp) {
    int ID = me.ID + 5;
    // Update grid
    int x,y;
    for (y = 0; y < b->h; y++) {
        for (x = 0; x < b->w; x++) {
            char c;
            switch (get_grid(b,x,y)) {
                case 0:
                    c = ' ';
                    break;
                case 1:
                    c = '#';
                    break;
                case 2:
                    c = 'X';
                    break;
                case 3:
                    c = '0';
                    break;
                case 4:
                    c = '_';
                    break;
                case 5:
                    c = '1';
                    if (ID == 0){
                        p -> x = x;
                        p -> y = y;
                    }
                    break;
                case 6:
                    c = '2';
                    if (ID == 1){
                        p -> x = x;
                        p -> y = y;
                    }
                    break;
                case 7:
                    c = '3';
                    if (ID == 2){
                        p -> x = x;
                        p -> y = y;
                    }
                    break;
                case 8:
                    c = '4';
                    if (ID == 3){
                        p -> x = x;
                        p -> y = y;
                        
                    }
                    break;
                default:
                    c = '?';
                    break;
            }
            mvaddch(y+1,x+1,c);

        }
    }
    for (x = 0; x < b->w+2; x++) {
        mvaddch(0, x, '-');
        mvaddch(b->h+1, x, '-');
    }
    for (y = 0; y < b->h+2; y++) {
        mvaddch(y, 0, '|');
        mvaddch(y, b->w+1, '|');
    }
    // Update chat text
    attron(COLOR_PAIR(1)); // Enable custom color 1
    attron(A_BOLD); // Enable bold
    if (l != NULL){
    for (x = 0; x < b->w+2; x++) {
        if (x >= TEXT_SIZE || x >= l->cursor)
            mvaddch(b->h+2, x, ' ');
        else
            mvaddch(b->h+2, x, l->data[x]);
    }
    }
    // select if ready to read
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(socket_tcp, &readfds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    int retval = select(socket_tcp+1, &readfds, NULL, NULL, &tv);
    if (retval == -1) {
        perror("select()");
    } else if (retval) {
        // clear the line
        for (x = 0; x < TEXT_SIZE; x++) {
            mvaddch(b->h+3, x, ' ');
        }

        char * msg = my_recv(socket_tcp, 3);
        header h;
        convert_littleendian(msg, 2);
        memcpy(&h, msg, 2);
        if (h.CODEREQ == 13){ // tout le monde
            char * data = my_recv(socket_tcp, (int)msg[2]);
            data[(int)msg[2]] = '\0';
            mvprintw(b->h+3, 0, "Player %d : %s", h.ID, data);
            free(data);
        }
        if (h.CODEREQ == 14){ // equipe
            if (h.EQ == me.EQ){
            char * data = my_recv(socket_tcp, (int)msg[2]);
            data[(int)msg[2]] = '\0';
            mvprintw(b->h+3, 0, "Team %d : %s", h.ID, data);
            free(data);
            }
        }
        free(msg);
    }

    attroff(A_BOLD); // Disable bold
    attroff(COLOR_PAIR(1)); // Disable custom color 1
    refresh(); // Apply the changes to the terminal
}
void refresh_tchat (line* l, board* b){
    int x;
    for (x = 0; x < TEXT_SIZE; x++) {
        if (x >= TEXT_SIZE || x >= l->cursor)
            mvaddch(b->h+4, x, ' ');
        else
            mvaddch(b->h+4, x, l->data[x]);
    }
    refresh();
}

ACTION control_and_tchat(line* l, Game_client player, int sock_tcp) {
    int c;
    int prev_c = ERR;
    // We consume all similar consecutive key presses
    while ((c = getch()) != ERR) { // getch returns the first key press in the queue
        if (prev_c != ERR && prev_c != c) {
            ungetch(c); // put 'c' back in the queue
            break;
        }
        prev_c = c;
    }
    ACTION a = NONE;
    char * data;
    switch (prev_c) {
        case ERR: break;
        case KEY_LEFT:
            a = LEFT; break;
        case KEY_RIGHT:
            a = RIGHT; break;
        case KEY_UP:
            a = UP; break;
        case KEY_DOWN:
            a = DOWN; break;
        case '\t':
            a = BOMB; break;
        case '$': 
            a = QUIT; break;
        case KEY_BACKSPACE:
            if (l->cursor > 0) l->cursor--;
            break;
        case 10: // message sending
            data = malloc(sizeof(char)*l->cursor);
            memcpy(data, l->data, l->cursor);
            char *msg;
            if(l->cursor > 3 && data[0] == 't' && data[1] == ':'){
                msg = write_tchat(8, player.ID, player.EQ, l->cursor, data);

            }else{
                msg = write_tchat(7, player.ID, player.EQ, l->cursor, data);
            }
            
            my_send(sock_tcp, msg, 3+l->cursor);
            free(msg);
            free(data);
            l->cursor = 0;
            break;
        default:
            if (prev_c >= ' ' && prev_c <= '~' && l->cursor < TEXT_SIZE)
                l->data[(l->cursor)++] = prev_c;
            break;
    }
    return a;
}

void bomb_place(board* b, int x, int y) {
    set_grid(b,x,y,3);
}

bool perform_action(board* b, pos* p, ACTION a, int ID, bool alive) {
    int xd = 0;
    int yd = 0;
    if (valid_move(b->grid,b->h,b->w,p->x,p->y,a, alive)){
    switch (a) {
        case NONE:
            return false;
        case LEFT:
            xd = -1; yd = 0; break;
        case RIGHT:
            xd = 1; yd = 0; break;
        case UP:
            xd = 0; yd = -1; break;
        case DOWN:
            xd = 0; yd = 1; break;
        case BOMB:
            bomb_place(b,p->x,p->y);
            return false;
        case QUIT:
            return true;
        default: break;
    }
    if (get_grid(b,p->x,p->y) != 3){ 
        set_grid(b,p->x,p->y,0);
    }
    p->x += xd; p->y += yd;
    set_grid(b,p->x,p->y,ID);
    }
    return false;
}

