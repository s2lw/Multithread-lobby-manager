#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define NUM_PLAYERS 2
#define MAX_LOBBIES 99
#define BOARD_SIZE 8

typedef struct {
    int id;
    int player_count;
    int player_sockets[NUM_PLAYERS];
    int game_active;
    pthread_mutex_t lock;
    int board[BOARD_SIZE][BOARD_SIZE];
} Lobby;

Lobby lobbies[MAX_LOBBIES];
pthread_mutex_t lobby_lock = PTHREAD_MUTEX_INITIALIZER;

void init_lobbies() {
    for (int i = 0; i < MAX_LOBBIES; i++) {
        lobbies[i].id = i;
        lobbies[i].player_count = 0;
        lobbies[i].game_active = 0;
        pthread_mutex_init(&lobbies[i].lock, NULL);
    }
}

void remove_player_from_lobby(int lobby_id, int client_socket) {
    pthread_mutex_lock(&lobbies[lobby_id].lock);
    
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (lobbies[lobby_id].player_sockets[i] == client_socket) {
            lobbies[lobby_id].player_sockets[i] = 0;  // Zerujemy socket
            lobbies[lobby_id].player_count--;
            //printf("lobby id: %d, player count");
            printf("Gracz opuścił lobby %d (%d/2)\n", lobby_id, lobbies[lobby_id].player_count);
            break;
        }
    }

    // jeśli nie ma już graczy, resetujemy lobby
    if (lobbies[lobby_id].player_count == 0) {
        printf("Lobby %d jest puste. Resetowanie...\n", lobby_id);
        lobbies[lobby_id].game_active = 0;
        memset(lobbies[lobby_id].player_sockets, 0, sizeof(lobbies[lobby_id].player_sockets));
    }

    pthread_mutex_unlock(&lobbies[lobby_id].lock);
}

void send_with_length(int socket, const char *msg) {
    int len = htonl(strlen(msg));
    send(socket, &len, sizeof(len), 0); // długość wiadomości
    send(socket, msg, strlen(msg), 0); // treść
}

void notify_lobby_full(int lobby_id) {
    char message[] = "Lobby pełne! Gra się rozpoczyna...\n";
    
    pthread_mutex_lock(&lobbies[lobby_id].lock);
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (lobbies[lobby_id].player_sockets[i] != 0) {
            //send(lobbies[lobby_id].player_sockets[i], message, strlen(message), 0);
            send_with_length(lobbies[lobby_id].player_sockets[i], message);
        }
    }
    pthread_mutex_unlock(&lobbies[lobby_id].lock);
}



int recv_with_length(int socket, char *buffer, int buffer_size) {
    int len;
    memset(buffer, 0, buffer_size);

    // ddbieramy długość wiadomości (4 bajty)
    if (recv(socket, &len, sizeof(len), 0) <= 0) {
        return -1; // błąd lub rozłączenie klienta
    }
    len = ntohl(len); // Konwersja z big-endian do host-endian

    // Sprawdź, czy wiadomość nie jest za duża
    if (len >= buffer_size) {
        fprintf(stderr, "Błąd: Wiadomość za długa dla bufora!\n");
        return -1;
    }

    // Odbierz właściwą wiadomość
    int received = recv(socket, buffer, len, 0);
    if (received <= 0) {
        return -1; //błąd lub rozłączenie klienta
    }

    buffer[received] = '\0';//
    return received;
}


// game functions

void init_board(Lobby *lobby) {
    memset(lobby->board, 0, sizeof(lobby->board));
    for(int i = 0; i < BOARD_SIZE; i++) {
        for(int j = 0; j < BOARD_SIZE; j++) {
            if((i + j) % 2 == 1) {
                if(i < 3) lobby->board[i][j] = 1;// Gracz 1 (W)
                else if(i > 4) lobby->board[i][j] = 2; // Gracz 2 (B)
            }
        }
    }
}
void init_board_test(Lobby *lobby) {
    memset(lobby->board, 0, sizeof(lobby->board));

    //B
    lobby->board[1][2] = 3;

    //C
    lobby->board[4][5] = 2; 
}

void print_board(Lobby *lobby, char *buffer) {
    int pos = 0;
    pos += sprintf(buffer + pos, "\n  ");
    for(int i = 0; i < BOARD_SIZE; i++) pos += sprintf(buffer + pos, "%d ", i);
    pos += sprintf(buffer + pos, "\n");
    
    for(int i = 0; i < BOARD_SIZE; i++) {
        pos += sprintf(buffer + pos, "%d ", i);
        for(int j = 0; j < BOARD_SIZE; j++) {
            switch(lobby->board[i][j]) {
                case 0: pos += sprintf(buffer + pos, ". "); break;
                case 1: pos += sprintf(buffer + pos, "W "); break;
                case 2: pos += sprintf(buffer + pos, "B "); break;
                case 3: pos += sprintf(buffer + pos, "WK"); break;
                case 4: pos += sprintf(buffer + pos, "BK"); break;
            }
        }
        pos += sprintf(buffer + pos, "\n");
    }
}

int validate_move(Lobby *lobby, int player, int from_row, int from_col, int to_row, int to_col, int *jumped_row, int *jumped_col) {
    if (from_row < 0 || from_row >= BOARD_SIZE || from_col < 0 || from_col >= BOARD_SIZE ||
        to_row < 0 || to_row >= BOARD_SIZE || to_col < 0 || to_col >= BOARD_SIZE) {
        return 0;
        }

    int piece = lobby->board[from_row][from_col];
    if ((player == 1 && !(piece == 1 || piece == 3)) || (player == 2 && !(piece == 2 || piece == 4))) return 0;

    if (lobby->board[to_row][to_col] != 0) return 0;

    int row_diff = to_row - from_row;
    int col_diff = abs(to_col - from_col);

    // Sprawdzamy, czy ruch jest po przekątnej
    if (abs(row_diff) != abs(col_diff)) return 0;

    // damka
    if (piece >= 3) {
        int step_row = (row_diff > 0) ? 1 : -1;
        int step_col = (to_col > from_col) ? 1 : -1;
        int x = from_row + step_row;
        int y = from_col + step_col;
        int jumped_count = 0;

        // sprawdzamy czy na trasie jest co najwyżej jeden pionek przeciwnika
        while (x != to_row && y != to_col) {
            int current_piece = lobby->board[x][y];
            if (current_piece != 0) {
                if ((player == 1 && (current_piece == 2 || current_piece == 4)) ||
                    (player == 2 && (current_piece == 1 || current_piece == 3))) {
                    jumped_count++;
                    *jumped_row = x;
                    *jumped_col = y;
                } else {
                    return 0; 
                }
            }
            x += step_row;
            y += step_col;
        }

        if (jumped_count == 0) {
            // Ruch bez bicia: wszystkie pola na trasie muszą być puste
            *jumped_row = -1;
            return 1;
        } else if (jumped_count == 1) {
            // Ruch z biciem
            return 1;
        } else {
            return 0;  // Więcej niż jedno bicie
        }
    }

    // Sprawdź ruch zwykłego pionka
    if (abs(row_diff) == 1) {
        if (piece == 1 && row_diff != 1) return 0; // Gracz 1 może poruszać się tylko w dół
        if (piece == 2 && row_diff != -1) return 0; // Gracz 2 może poruszać się tylko w górę
        *jumped_row = -1;
        return 1;
    }

    // bicie zwykłego pionka
    if (abs(row_diff) == 2 && abs(col_diff) == 2) {
        *jumped_row = (from_row + to_row) / 2;
        *jumped_col = (from_col + to_col) / 2;
        int jumped = lobby->board[*jumped_row][*jumped_col];

        if ((player == 1 && !(jumped == 2 || jumped == 4)) ||
            (player == 2 && !(jumped == 1 || jumped == 3)))
            return 0;

        return 1;
    }

    return 0;
}

int check_win(Lobby *lobby, int active_player) {
    int opponent = (active_player == 1) ? 2 : 1; 
    int opponent_pieces = 0;
    int has_legal_moves = 0; // czy przeciwnik ma legalne ruchy

    //czy przeciwnik ma jeszcze pionki na planszy
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (lobby->board[i][j] == opponent || lobby->board[i][j] == opponent + 2) {
                opponent_pieces++;
            }
        }
    }
    if (opponent_pieces == 0) {
        return 1; // Aktywny gracz wygrał
    }

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (lobby->board[i][j] == opponent || lobby->board[i][j] == opponent + 2) {
                // wszystkie możliwe ruchy dla tego pionka
                for (int dx = -1; dx <= 1; dx += 2) {
                    for (int dy = -1; dy <= 1; dy += 2) {
                        int new_i = i + dx;
                        int new_j = j + dy;

                        // Sprawdzamy czy ruch jest legalny
                        if (new_i >= 0 && new_i < BOARD_SIZE && new_j >= 0 && new_j < BOARD_SIZE) {
                            int jumped_row, jumped_col;
                            if (validate_move(lobby, opponent, i, j, new_i, new_j, &jumped_row, &jumped_col)) {
                                has_legal_moves = 1; 
                                break;
                            }
                        }
                    }
                    if (has_legal_moves) break;
                }
                if (has_legal_moves) break;
            }
        }
        if (has_legal_moves) break;
    }

    if (!has_legal_moves) {
        return 1;  
    }

    return 0; 
}


// end of game functions

void *lobby_thread(void *arg) {
    Lobby *lobby = (Lobby *)arg;
    lobby->game_active = 1;
    printf("Lobby %d rozpoczęło grę!\n", lobby->id);

    int current_player = 1;
    

    init_board(lobby);
    //init_board_test(lobby);
    char buffer[1024];

    while (lobby->game_active) {
        sleep(1);
        int active_client = (current_player == 1) ? lobby->player_sockets[0] : lobby->player_sockets[1];
        int non_active_client = (current_player == 1) ? lobby->player_sockets[1] : lobby->player_sockets[0];
        pthread_mutex_lock(&lobby->lock);

        if (active_client <= 0 || non_active_client <= 0) {
            printf("Jeden z graczy opuścił lobby %d. Kończenie gry...\n", lobby->id);
            lobby->game_active = 0;
            pthread_mutex_unlock(&lobby->lock);
            break;
        }
        memset(buffer, 0, 1024);
        print_board(lobby, buffer);
        //printf("%s",buffer);
        send_with_length(active_client, buffer);
        send_with_length(non_active_client, buffer);
        if (current_player == 1) {
            send_with_length(active_client, "Twoja tura! Grasz bialymi");
        }
        else {
            send_with_length(active_client, "Twoja tura! Grasz czarnymi");
        }

        int break_flag = 0;
        
        int fr, fc, tr, tc;
        int flag_validation = 2;
        int jr = -1, jc = -1; // deklaracja

        while (flag_validation > 0) {
            char move[64];
            int bytes = recv_with_length(active_client, move, sizeof(move));
            if (bytes <= 0) {
                printf("Klient rozłączył się lub wystąpił błąd.\n");
                break_flag = 1;
                break; // wyjdź z pętli, jeśli klient się rozłączył
            }
            move[bytes] = '\0';
            printf("recived move = %s\n", move);
            // Walidacja ruchu
            flag_validation = 1;
            if(sscanf(move, "%d %d %d %d", &fr, &fc, &tr, &tc) != 4) {
                printf("Nieprawidłowy format ruchu!");
                send_with_length(active_client, "Nieprawidłowy format ruchu!");
                send_with_length(active_client,"Twoja tura!"); // prosi klienta o ruch
            }
            else if(!validate_move(lobby,current_player, fr, fc, tr, tc, &jr, &jc)) {
                printf("Nieprawidłowy ruch!");
                send_with_length(active_client, "Nieprawidłowy ruch!");
                send_with_length(active_client,"Twoja tura!"); // prosi klienta o ruch
            }
            else {
                flag_validation -= 1;
            }
        }
        if (break_flag) {
            pthread_mutex_unlock(&lobby->lock);
            break;
        }
        // Wykonaj ruch
        lobby->board[tr][tc] = lobby->board[fr][fc];
        lobby->board[fr][fc] = 0;

        // Usuwanie zbitego pionka
        if (jr != -1) {
            lobby->board[jr][jc] = 0;
        }

        // Awans na damkę
        if((current_player == 1 && tr == BOARD_SIZE-1) || 
           (current_player == 2 && tr == 0)) {
            lobby->board[tr][tc] += 2;
        }

        // Czy wygral
        if (check_win(lobby, current_player)) {
            printf("Gracz %d wygrał!\n", current_player);
            send_with_length(active_client,"Wygrales !!!");

            send_with_length(non_active_client,"Przegrales :(");
            
            
            lobby->game_active = 0; 
        }
        current_player = (current_player == 1) ? 2 : 1;

        pthread_mutex_unlock(&lobby->lock);
    }
    pthread_mutex_lock(&lobby->lock);
    for (int i = 0; i < NUM_PLAYERS; i++) {
        //send(lobby->player_sockets[i], "Lobby zamykane...\n", 50, 0);
        send_with_length(lobby->player_sockets[i], "Lobby zamykane...\n");
        
        close(lobby->player_sockets[i]);
        lobby->player_sockets[i] = 0;
        //printf("+\n");
    }
    lobby->player_count = 0;
    lobby->game_active = 0;
    pthread_mutex_unlock(&lobby->lock);
    
    //pthread_exit(NULL);
    return NULL;
}

void cleanup_and_exit(int signal) {
    printf("\nShutting down server...\n");
    
    for (int i = 0; i < MAX_LOBBIES; i++) {
        pthread_mutex_lock(&lobbies[i].lock);
        for (int j = 0; j < NUM_PLAYERS; j++) {
            if (lobbies[i].player_sockets[j] > 0) {
                send_with_length(lobbies[i].player_sockets[j], "Server shutting down");
                close(lobbies[i].player_sockets[j]);
            }
        }
        pthread_mutex_unlock(&lobbies[i].lock);
        pthread_mutex_destroy(&lobbies[i].lock);
    }
    
    pthread_mutex_destroy(&lobby_lock);
    
    close(server_socket);
    
    exit(0);
}


int join_lobby(int client_socket) {
    pthread_mutex_lock(&lobby_lock);
    for (int i = 0; i < MAX_LOBBIES; i++) {
        if (lobbies[i].player_count < NUM_PLAYERS) {
            int index = lobbies[i].player_count;
            lobbies[i].player_sockets[index] = client_socket;
            lobbies[i].player_count++;

            printf("Gracz dołączył do lobby %d (%d/2)\n", i, lobbies[i].player_count);
            char message[50];
            sprintf(message, "Lobby %d: %d/2 graczy\n", i, lobbies[i].player_count);
            //send(client_socket, message, strlen(message), 0);
            send_with_length(client_socket, message);

            if (lobbies[i].player_count == NUM_PLAYERS) {
                notify_lobby_full(i);  // Wysyłamy wiadomość do wszystkich graczy
                pthread_t game_thread;
                pthread_create(&game_thread, NULL, lobby_thread, &lobbies[i]);
                pthread_detach(game_thread);
            }
            pthread_mutex_unlock(&lobby_lock);
            return i;
        }
    }
    pthread_mutex_unlock(&lobby_lock);
    char error_message[] = "Brak dostępnych miejsc w lobby.\n";
    //send(client_socket, error_message, strlen(error_message), 0);
    send_with_length(client_socket, error_message);
    close(client_socket);
    return -1;
}

void *handle_client(void *arg) {
    int client_socket = *((int *)arg);
    free(arg);
    int lobby_id = join_lobby(client_socket);
    if (lobby_id == -1) return NULL;

    char buffer[1024];
    while (1) {
        int n = recv(client_socket, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT);
        if (n == 0) { // Klient zamknął połączenie
            printf("Klient w lobby %d rozłączył się.\n", lobby_id);
            // wyslij przeciwnikowi wiadomosc "exit"
            int opponent_socket = lobbies[lobby_id].player_sockets[0] == client_socket ?
                                  lobbies[lobby_id].player_sockets[1] :
                                  lobbies[lobby_id].player_sockets[0];

            if (opponent_socket > 0) {
                //send(opponent_socket, "exit", strlen("exit"), 0);
                send_with_length(opponent_socket, "exit");
                //remove_player_from_lobby(lobby_id, opponent_socket);
            }
            break;
        } else if (n < 0) {
            //
            continue;
           
        }
        
        sleep(1);
    }

    remove_player_from_lobby(lobby_id, client_socket);
    printf("removed player form lobby: %d\n", lobby_id);
    close(client_socket);
    return NULL;
}
int server_socket;
int main() {
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);
    signal(SIGPIPE, SIG_IGN); // serwer nie wyłącza się, gdy klient rozłączy się nagle
    setvbuf(stdout, NULL, _IONBF, 0);  // Wyłączenie buforowania dla stdout
    init_lobbies();
    int  *new_socket;
    struct sockaddr_in server_addr;
    struct sockaddr_storage server_storage;
    socklen_t addr_size;

    server_socket = socket(PF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1100);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(server_addr.sin_zero, '\0', sizeof server_addr.sin_zero);

    bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_socket, 50);
    printf("Serwer nasłuchuje na porcie 1100...\n");

    while (1) {
        addr_size = sizeof server_storage;
        new_socket = malloc(sizeof(int));
        *new_socket = accept(server_socket, (struct sockaddr *)&server_storage, &addr_size);

        pthread_t client_thread;
        pthread_create(&client_thread, NULL, handle_client, new_socket);
        pthread_detach(client_thread);
    }

    return 0;
}
