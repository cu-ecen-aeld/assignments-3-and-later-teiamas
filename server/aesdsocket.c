#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>

#define PORT 9000
#define PORT_STR "9000"
#define BACKLOG 5
#define FILE_PATH "/var/tmp/aesdsocketdata"

volatile sig_atomic_t stop = 0;

void handle_signal(int signal) {
    stop = 1;
    syslog(LOG_INFO, "Caught signal, exiting");
}

void handle_client(int client_fd, struct sockaddr *client_addr) {
    char buffer[1024];
    ssize_t bytes_received;

    // Log client IP address
    char client_ip[INET6_ADDRSTRLEN];
    if (client_addr->sa_family == AF_INET) {
        inet_ntop(AF_INET, &((struct sockaddr_in *)client_addr)->sin_addr, client_ip, INET_ADDRSTRLEN);
    } else if (client_addr->sa_family == AF_INET6) {
        inet_ntop(AF_INET6, &((struct sockaddr_in6 *)client_addr)->sin6_addr, client_ip, INET6_ADDRSTRLEN);
    }
    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    // Open file to store received messages
    int file_fd = open(FILE_PATH, O_CREAT | O_APPEND | O_RDWR, 0644);
    if (file_fd == -1) {
        syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
        close(client_fd);
        return;
    }

    // Receive data from the client
    while ((bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0'; // Null-terminate the received data

        // Write data to the file
        if (write(file_fd, buffer, bytes_received) == -1) {
            syslog(LOG_ERR, "Failed to write to file: %s", strerror(errno));
            close(file_fd);
            close(client_fd);
            return;
        }

        // Check for newline and send the file content back to the client
        if (strchr(buffer, '\n')) {
            lseek(file_fd, 0, SEEK_SET); // Rewind the file
            char file_buffer[1024];
            ssize_t bytes_read;
            while ((bytes_read = read(file_fd, file_buffer, sizeof(file_buffer))) > 0) {
                if (send(client_fd, file_buffer, bytes_read, 0) == -1) {
                    syslog(LOG_ERR, "Failed to send data to client: %s", strerror(errno));
                    close(file_fd);
                    close(client_fd);
                    return;
                }
            }
        }
    }

    if (bytes_received == -1) {
        syslog(LOG_ERR, "Failed to receive data: %s", strerror(errno));
    }

    // Log client disconnection
    syslog(LOG_INFO, "Closed connection from %s", client_ip);

    close(file_fd);
    close(client_fd);
}

void daemonize() {
    pid_t pid = fork();

    if (pid < 0) {
        syslog(LOG_ERR, "Failed to fork process: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        // Parent process exits
        exit(EXIT_SUCCESS);
    }

    // Child process continues
    if (setsid() == -1) {
        syslog(LOG_ERR, "Failed to create new session: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Redirect standard file descriptors to /dev/null
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("/dev/null", O_RDONLY); // stdin
    open("/dev/null", O_WRONLY); // stdout
    open("/dev/null", O_WRONLY); // stderr
}

int main(int argc, char *argv[]) {
    int daemon_mode = 0;

    // Parse command-line arguments
    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = 1;
    }

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        syslog(LOG_ERR, "Failed to set up signal handlers: %s", strerror(errno));
        return -1;
    }

    int server_fd, client_fd;
    struct addrinfo hints, *servinfo;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    openlog("aesdsocket", LOG_PID, LOG_USER);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
        return -1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(NULL, PORT_STR, &hints, &servinfo);
    if (status != 0) {
        syslog(LOG_ERR, "getaddrinfo: %s", gai_strerror(status));
        close(server_fd);
        return -1;
    }

    if (bind(server_fd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        syslog(LOG_ERR, "Failed to bind socket: %s", strerror(errno));
        freeaddrinfo(servinfo);
        close(server_fd);
        return -1;
    }
    freeaddrinfo(servinfo);

    if (listen(server_fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Failed to listen on socket: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    syslog(LOG_INFO, "Server listening on port %d", PORT);

    // Daemonize if -d argument is provided
    if (daemon_mode) {
        daemonize();
    }

    while (!stop) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd == -1) {
            if (errno == EINTR) break; // Exit loop if interrupted by a signal
            syslog(LOG_ERR, "Failed to accept connection: %s", strerror(errno));
            continue;
        }

        handle_client(client_fd, (struct sockaddr *)&client_addr);
    }

    // Cleanup and shutdown
    syslog(LOG_INFO, "Shutting down server...");
    if (close(server_fd) == -1) {
        syslog(LOG_ERR, "Failed to close server socket: %s", strerror(errno));
    }
    if (unlink(FILE_PATH) == -1) {
        syslog(LOG_ERR, "Failed to delete file: %s", strerror(errno));
    }
    closelog();
    return 0;
}