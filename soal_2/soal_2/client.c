#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9000
#define BUFFER_SIZE 4096

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char send_buffer[BUFFER_SIZE];
    char recv_buffer[BUFFER_SIZE];
    int bytes_received;

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    // Convert IP address
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connected to DB Server on port %d\n", SERVER_PORT);
    printf("Type HELP for available commands\n");
    printf("Type EXIT to quit\n");

    // Main loop for interactive session
    while (1) {
        printf("db > ");
        fflush(stdout);

        // Read user input
        if (fgets(send_buffer, BUFFER_SIZE - 1, stdin) == NULL) {
            if (feof(stdin)) {
                break;
            }
            perror("fgets");
            continue;
        }

        // Remove newline from input
        size_t len = strlen(send_buffer);
        if (len > 0 && send_buffer[len - 1] == '\n') {
            send_buffer[len - 1] = '\0';
        }

        // Check for EXIT command
        if (strcasecmp(send_buffer, "EXIT") == 0) {
            printf("Disconnecting from server...\n");
            close(sock);
            exit(EXIT_SUCCESS);
        }

        // Skip empty commands
        if (strlen(send_buffer) == 0) {
            continue;
        }

        // Send command to server
        if (send(sock, send_buffer, strlen(send_buffer), 0) < 0) {
            perror("send");
            close(sock);
            exit(EXIT_FAILURE);
        }

        // Receive response from server
        memset(recv_buffer, 0, BUFFER_SIZE);
        bytes_received = recv(sock, recv_buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received < 0) {
            perror("recv");
            close(sock);
            exit(EXIT_FAILURE);
        } else if (bytes_received == 0) {
            printf("Server closed connection\n");
            close(sock);
            exit(EXIT_FAILURE);
        }

        recv_buffer[bytes_received] = '\0';
        printf("%s\n", recv_buffer);
    }

    close(sock);
    return EXIT_SUCCESS;
}