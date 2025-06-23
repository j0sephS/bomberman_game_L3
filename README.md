
# Bomberman Project

## Overview
**Project carried out during my third year of a Bachelor's degree in Computer Science, in a team of three students.**
This project is a multiplayer Bomberman game implemented in C (made . The game consists of a server that manages the game state and multiple clients that connect to the server to play the game. The game supports different modes and allows players to interact with each other in real-time.

## Project Structure

The project is organized into several header files, each responsible for different aspects of the game:

- `server.h`: Defines the structures and functions related to the game server.
- `client.h`: Defines the structures and functions related to the game client.
- `game.h`: Contains functions to validate player moves and game logic.
- `curses.h`: Manages the graphical interface using the ncurses library.
- `communication.h`: Handles communication between the server and clients.
- `misc.h`: Contains miscellaneous utility functions and definitions.
- `affichage.h`: Provides functions for displaying information.

## Key Components

### Server

The server is responsible for managing the game state, handling player connections, and broadcasting updates to all clients. Key structures and functions include:

- `Game_server`: Structure to hold server-related information.
- `Game_state_server`: Structure to hold the current state of the game.
- `create_socket_server()`: Function to create a server socket.
- `recv_join_send_port()`: Function to handle player join requests.
- `initialisation_games()`: Function to initialize game settings.

### Client

The client connects to the server, sends player actions, and receives game updates. Key structures and functions include:

- `Game_client`: Structure to hold client-related information.
- `Game_state_client`: Structure to hold the current state of the game from the client's perspective.
- `create_socket_client()`: Function to create a client socket.
- `send_join_msg()`: Function to send a join request to the server.
- `start_game()`: Function to start the game and handle game loop.

### Game Logic

The game logic includes validating player moves, handling bomb placements, and updating the game state. Key functions include:

- `valid_move()`: Function to validate if a player's move is allowed.
- `perform_action()`: Function to perform a player's action and update the game state.

### Graphical Interface

The graphical interface is managed using the ncurses library, which provides a text-based interface for the game. Key functions include:

- `setup_board()`: Function to initialize the game board.
- `refresh_game()`: Function to refresh the game display.
- `control_and_tchat()`: Function to handle player controls and chat messages.

### Communication

Communication between the server and clients is handled using TCP and UDP sockets. Key functions include:

- `create_msgs()`: Function to create messages to be sent between server and clients.
- `my_send()`: Function to send data over a socket.
- `my_recv()`: Function to receive data from a socket.

## How to Run

1. Compile the server and client programs using a C compiler.
2. Start the server by running the server executable.
3. Start the client by running the client executable and connect to the server.
4. Enjoy the game!

## Dependencies

- ncurses library for the graphical interface.
- Standard C libraries for socket programming and utility functions.

## License

This project is licensed under the MIT License.

