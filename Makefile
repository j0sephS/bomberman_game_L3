all : client server

CCFLAGS = -Wall -Wpedantic -Wextra -Werror -std=c99 -g
FILES_CLIENT = client.c affichage.c misc.c communication.c curses.c game.c
FILES_SERVER = server.c affichage.c misc.c communication.c game.c

client :
	gcc $(CCFLAGS) -o client $(FILES_CLIENT) -lncurses

server :
	gcc $(CCFLAGS) -o server $(FILES_SERVER)
	
clean:
	rm client
	rm server