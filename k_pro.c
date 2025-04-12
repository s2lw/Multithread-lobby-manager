#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 1100
#define BUFFER_SIZE 1024

void send_with_length(int socket, const char *msg) {
    int len = htonl(strlen(msg));  // Konwersja
    send(socket, &len, sizeof(len), 0);  // Najpierw wysyłamy długość wiadomości
    send(socket, msg, strlen(msg), 0);   // Następnie wysyłamy treść wiadomości
}

int recv_with_length(int socket, char *buffer, int buffer_size) {
    int len;
    memset(buffer, 0, buffer_size);

    // Odbierz długość wiadomości (4 bajty)
    if (recv(socket, &len, sizeof(len), 0) <= 0) {
        return -1;  // Błąd lub rozłączenie klienta
    }
    len = ntohl(len);  // Konwersj

    // Sprawdź, czy wiadomość nie jest za duża
    if (len >= buffer_size) {
        fprintf(stderr, "Błąd: Wiadomość za długa dla bufora!\n");
        return -1;
    }

    // Odbierz właściwą wiadomość
    int received = recv(socket, buffer, len, 0);
    if (received <= 0) {
        return -1;  
    }

    buffer[received] = '\0'; 
    return received;  
}

void client_cleanup(int signal) {
    printf("\nZamykam połączenie...\n");
    close(client_socket);
    exit(0);
}

int client_socket;
int main() {
    signal(SIGINT, client_cleanup);
    signal(SIGTERM, client_cleanup);
    setvbuf(stdout, NULL, _IONBF, 0);  // Wyłączenie buforowania dla stdout
    
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    // Tworzenie gniazda klienta
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Błąd przy tworzeniu gniazda");
        exit(1);
    }

    // Konfiguracja adresu serwera
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Połączenie z serwerem
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Błąd połączenia z serwerem");
        close(client_socket);
        exit(1);
    }
    printf("Połączono z serwerem gier.\n");
    int flag = 0;
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        //int bytes_received = read(client_socket, buffer, BUFFER_SIZE);
        int bytes_received = recv_with_length(client_socket, buffer, BUFFER_SIZE);

        if (bytes_received <= 0) {
            printf("Serwer zamknął połączenie.\n");
            break;
        }
        if (strncmp(buffer, "exit", 4) == 0) {
            printf("Przeciwnik się rozłączył, kończę mecz.\n");
            exit(0); 
        }
        if (strncmp(buffer+1, "  0 1 2 3 4 5 6 7", 15) == 0) {
            system("clear");
        }
        if (strncmp(buffer, "Wygrales !!!", 12) == 0 || strncmp(buffer, "Przegrales :(", 12) == 0) {
            system("clear");
            printf("%s\n", buffer);
            printf("Kończę mecz.\n");
            exit(0);
        }
        //printf("buffer 1\n");
        printf("%s\n", buffer);
        //printf("end of buffer 1\n");
        
        

        if (strncmp(buffer, "Twoja tura!", 11) == 0) {
            //printf("%s", buffer);
            printf("\n");
            printf("Podaj ruch w formacie 'from_row from_col to_row to_col': ");
            fflush(stdout);
            char move[64];
            fgets(move, sizeof(move), stdin);

            // Wysylamy ruch do serwera
            //send(client_socket, move, strlen(move), 0);
            send_with_length(client_socket, move);

        } else {
            //printf("%s\n", buffer);
            continue;
        }
    }

    close(client_socket);
    return 0;
}
