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
