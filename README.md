# Multi-Lobby Checkers Game Server

A scalable TCP-based game server that supports up to 99 concurrent games of checkers between player pairs.

## Summary

This project consists of a server and client implementation for a networked checkers game. The system provides a multi-lobby environment where players can connect and automatically join available game lobbies. Each lobby handles a single game between two players, with the server managing lobby assignments, game state, and player synchronization.

## Key Features

- **Scalable Lobby System**: Supports up to 99 parallel game lobbies
- **Automatic Matchmaking**: Players are assigned to the first available lobby
- **Thread-Safe Design**: Each client and game runs in a dedicated thread with proper synchronization
- **Graceful Disconnection Handling**: Proper cleanup when a player disconnects
- **Checkers Game Implementation**: Full rules implementation with piece movement, capturing, and king promotion

## Technical Architecture

### Server Components

- **Lobby Management System**: Tracks up to 99 independent game lobbies with player connections and game state
- **Thread-Based Design**: Each client and active game runs in a dedicated thread
- **Mutex Synchronization**: Uses locks to prevent race conditions when accessing shared data
- **Client Handler**: Processes incoming connections and assigns players to appropriate lobbies

### Communication Protocol

The project implements a custom length-prefixed message protocol for reliable data exchange:

1. Each message is prefixed with its length in network byte order (4 bytes)
2. The actual message content follows the length prefix
3. This approach ensures complete message delivery despite TCP's stream-based nature

## How to Use

1. Compile the server and client with gcc using the pthread flag:
gcc s_pro.c -o s.out -pthread
gcc k_pro.c -o k.out
3. Start the server and client: ./s.out ./k.out
4. When prompted for a move, enter it in the format: from_row from_col to_row to_col


## Technology Stack

- **C Programming Language**: Core implementation for both server and client
- **POSIX Threads (pthread)**: For multi-threading capabilities
- **TCP/IP Sockets**: For network communication between server and clients
- **Mutex Locks**: For thread synchronization and data protection

## Network Communication Flow

1. Server listens on port 1100
2. When a client connects, a new thread is created to handle that client
3. The client is assigned to the first available lobby
4. When a lobby has two players, the game begins in a dedicated game thread
5. During the game, the server sends board state updates and turn notifications
6. Clients send move commands in the specified format
7. The server validates moves, updates the game state, and checks for game completion
8. Upon disconnection or game completion, the server cleans up resources and notifies opponents

## Implementation Details

The system uses POSIX libraries and is designed for Linux/Unix environments. Notable implementation details include:

- Non-blocking buffer output with `setvbuf(stdout, NULL, _IONBF, 0)`
- Signal handling to prevent server crashes when clients disconnect unexpectedly
- Thread detachment for automatic resource cleanup
- Mutex locks to protect shared lobby data
- Clear terminal-based UI for the game board

This project demonstrates the implementation of a concurrent server application that can handle multiple independent game sessions while maintaining proper synchronization and communication between clients.
