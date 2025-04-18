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
#include <pthread.h>
#include <sys/queue.h>
#include <time.h>
#include <sys/file.h>

#define PORT 9000
#define PORT_STR "9000"
#define BACKLOG 5
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define LOCK_FILE "/tmp/aesdsocket.lock"
#define LOCK_WAIT_TIME 15 // Maximum wait time in seconds
#define LOCK_RETRY_INTERVAL 1 // Retry interval in seconds

volatile sig_atomic_t stop = 0;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

// Thread management structure
typedef struct thread_info {
    pthread_t thread_id;
    int client_fd;
    struct sockaddr *client_addr;
    SLIST_ENTRY(thread_info) entries;
} thread_info_t;

SLIST_HEAD(thread_list, thread_info) thread_head;

void handle_signal(int signal) {
    stop = 1;
    syslog(LOG_INFO, "Caught signal, exiting");
}

void* handle_client_thread(void* arg) {
    thread_info_t *thread_info = (thread_info_t*)arg;
    int client_fd = thread_info->client_fd;
    char client_ip[INET6_ADDRSTRLEN];
    struct sockaddr *client_addr = thread_info->client_addr;
    char buffer[1024];
    ssize_t bytes_received;

    // Log client IP address
    memset(client_ip, 0, sizeof(client_ip));    
    if (client_addr->sa_family == AF_INET) {
        inet_ntop(AF_INET, &((struct sockaddr_in *)client_addr)->sin_addr, client_ip, INET_ADDRSTRLEN);
    } else if (client_addr->sa_family == AF_INET6) {
        inet_ntop(AF_INET6, &((struct sockaddr_in6 *)client_addr)->sin6_addr, client_ip, INET6_ADDRSTRLEN);
    }
    syslog(LOG_INFO, "Accepted connection from %s handling client in thread %ld", client_ip,thread_info->thread_id);

    // Open file to store received messages
    int file_fd = open(FILE_PATH, O_CREAT | O_APPEND | O_RDWR, 0644);
    if (file_fd == -1) {
        syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
        close(client_fd);
        pthread_exit(NULL);
    }

    // Receive data from the client
    while ((bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0'; // Null-terminate the received data

        // Write data to the file synchronized with mutex
        pthread_mutex_lock(&file_mutex);
        if (write(file_fd, buffer, bytes_received) == -1) {
            syslog(LOG_ERR, "Failed to write to file: %s", strerror(errno));
            pthread_mutex_unlock(&file_mutex);
            close(file_fd);
            close(client_fd);
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&file_mutex);
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
                    pthread_exit(NULL);
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
    pthread_exit(NULL);
}

void clean_up_threads() {
    thread_info_t *thread_info;
    while (!SLIST_EMPTY(&thread_head)) {
        thread_info = SLIST_FIRST(&thread_head);
        SLIST_REMOVE_HEAD(&thread_head, entries);
        pthread_join(thread_info->thread_id, NULL);
        free(thread_info);
    }
}

// Function to append a timestamp to the file
void* timer_thread(void* arg) {
    struct timespec next_wakeup;
    clock_gettime(CLOCK_REALTIME, &next_wakeup);

    while (!stop) {
        // Increment the wakeup time by 10 seconds
        next_wakeup.tv_sec += 10;

        // Generate the timestamp
        time_t now = time(NULL);
        struct tm *time_info = localtime(&now);
        char timestamp[128];
        strftime(timestamp, sizeof(timestamp), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", time_info);

        // Write the timestamp to the file
        pthread_mutex_lock(&file_mutex);
        int file_fd = open(FILE_PATH, O_CREAT | O_APPEND | O_RDWR, 0644);
        if (file_fd == -1) {
            syslog(LOG_ERR, "Failed to open file for timestamp: %s", strerror(errno));
            pthread_mutex_unlock(&file_mutex);
            continue;
        }
        if (write(file_fd, timestamp, strlen(timestamp)) == -1) {
            syslog(LOG_ERR, "Failed to write timestamp to file: %s", strerror(errno));
        }
        close(file_fd);
        pthread_mutex_unlock(&file_mutex);

        // Sleep until the next wakeup time
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_wakeup, NULL);
    }
    pthread_exit(NULL);
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
    int lock_fd;

    // Open the lock file
    lock_fd = open(LOCK_FILE, O_CREAT | O_RDWR, 0666);
    if (lock_fd == -1) {
        syslog(LOG_ERR, "Failed to open lock file: %s", strerror(errno));
        return -1;
    }

    // Try to acquire an exclusive lock with a retry mechanism
    int wait_time = 0;
    while (flock(lock_fd, LOCK_EX | LOCK_NB) == -1) {
        if (errno == EWOULDBLOCK) {
            if (wait_time >= LOCK_WAIT_TIME) {
                syslog(LOG_ERR, "Another instance of aesdsocket is still running or shutting down after %d seconds", LOCK_WAIT_TIME);
                close(lock_fd);
                return -1;
            }
            syslog(LOG_INFO, "Another instance is running, waiting for lock... (%d/%d seconds)", wait_time, LOCK_WAIT_TIME);
            sleep(LOCK_RETRY_INTERVAL);
            wait_time += LOCK_RETRY_INTERVAL;
        } else {
            syslog(LOG_ERR, "Failed to acquire lock: %s", strerror(errno));
            close(lock_fd);
            return -1;
        }
    }

    // Write the PID to the lock file
    if (ftruncate(lock_fd, 0) == -1 || dprintf(lock_fd, "%d\n", getpid()) < 0) {
        syslog(LOG_ERR, "Failed to write PID to lock file: %s", strerror(errno));
        close(lock_fd);
        return -1;
    }

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

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        syslog(LOG_ERR, "Failed to set socket options: %s", strerror(errno));
        close(server_fd);
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

    // Start the timer thread (AFTER daemonizing)
    pthread_t timer_tid;
    if (pthread_create(&timer_tid, NULL, timer_thread, NULL) != 0) {
        syslog(LOG_ERR, "Failed to create timer thread");
        close(server_fd);
        return -1;
    }

    while (!stop) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd == -1) {
            if (errno == EINTR) break; // Exit loop if interrupted by a signal
            syslog(LOG_ERR, "Failed to accept connection: %s", strerror(errno));
            continue;
        }

        // Create a new thread for the client
        thread_info_t *thread_info = malloc(sizeof(thread_info_t));
        if (!thread_info) {
            syslog(LOG_ERR, "Failed to allocate memory for thread info");
            close(client_fd);
            continue;
        }
        thread_info->client_fd = client_fd;
        thread_info->client_addr = (struct sockaddr *)&client_addr;
    
        if (pthread_create(&thread_info->thread_id, NULL, handle_client_thread, thread_info) != 0) {
            syslog(LOG_ERR, "Failed to create thread");
            close(client_fd);
            free(thread_info);
            continue;
        }
    
        SLIST_INSERT_HEAD(&thread_head, thread_info, entries);
    }

    // Cleanup and shutdown
    syslog(LOG_INFO, "Shutting down server...");
    syslog(LOG_INFO, "Threads cleanup ... started");
    clean_up_threads();
    syslog(LOG_INFO, "Threads cleanup ... done");

    // Wait for the timer thread to finish
    syslog(LOG_INFO, "Timer thread stop ...");
    pthread_join(timer_tid, NULL);
    syslog(LOG_INFO, "Timer thread stop ... done");

    if (close(server_fd) == -1) {
        syslog(LOG_ERR, "Failed to close server socket: %s", strerror(errno));
    }
    if (unlink(FILE_PATH) == -1) {
        syslog(LOG_ERR, "Failed to delete file: %s", strerror(errno));
    }
    if (close(lock_fd) == -1) {
        syslog(LOG_ERR, "Failed to close lock file: %s", strerror(errno));
    }
    if (unlink(LOCK_FILE) == -1) {
        syslog(LOG_ERR, "Failed to delete lock file: %s", strerror(errno));
    }
    syslog(LOG_INFO, "Server shutdown complete");
    closelog();
    // Exit the program
    if (daemon_mode) {
        syslog(LOG_INFO, "Daemon mode: exiting");
        exit(EXIT_SUCCESS);
    }
    // If not in daemon mode, return to the caller
    syslog(LOG_INFO, "Exiting");
    return 0;
}