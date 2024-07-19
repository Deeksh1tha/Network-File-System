#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "structs.h"
#define LOG_FILE "log.txt"
#define MAX_LOG_MSG_SIZE 256

sem_t x, y;
ss_info *ss_array;
ss_backup_info *backup_ss_array;
pthread_t ss_thread[100];
pthread_t client_thread[100];
struct LRUcache *lruQueue;
sem_t LRU_lock;
FILE *logFile;
int ss_num = 0;
int c_num = 0;

struct TrieNode *root;
void logMessage(const char *message, int socket, int status)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    if (getpeername(socket, (struct sockaddr *)&addr, &addrlen) == 0)
    {
        char ipAddr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(addr.sin_addr), ipAddr, INET_ADDRSTRLEN);

        char logMsg[MAX_LOG_MSG_SIZE];
        int msgLength = snprintf(logMsg, MAX_LOG_MSG_SIZE, "[%s:%d] %s - Status: %d\n", ipAddr, ntohs(addr.sin_port), message, status);

        int logFile = open(LOG_FILE, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
        if (logFile == -1)
        {
            perror("Error opening log file");
            exit(EXIT_FAILURE);
        }

        if (write(logFile, logMsg, msgLength) == -1)
        {
            perror("Error writing to log file");
        }

        close(logFile);
    }
    else
    {
        perror("getpeername");
    }
}

int COPY(char *path1, char *path2)
{
    ss_info *ss_struct_1 = getFromLRUcache(lruQueue, path1);
    if (ss_struct_1 == NULL)
    {
        ss_struct_1 = search(root, path1);
        enqueue(lruQueue, path1, ss_struct_1);
    }
    if (ss_struct_1 == NULL)
    {
        perror("File/Directory unavailable");
        return 404;
    }

    printf("1\n");
    // SS for path2
    ss_info *ss_struct_2 = getFromLRUcache(lruQueue, path2);
    if (ss_struct_2 == NULL)
    {
        ss_struct_2 = search(root, path2);
        enqueue(lruQueue, path2, ss_struct_2);
    }
    if (ss_struct_2 == NULL)
    {
        perror("File/Directory unavailable");
        return 404;
    }

    printf("2\n");

    char filename[20];
    char *token = strdup(path1);
    token = strtok(token, "/");
    while (token != NULL)
    {
        strcpy(filename, token);
        token = strtok(NULL, "/");
    }
    strcat(path2, filename);
    printf("New file path: %s\n", path2);

    // for file
    int sender_sock = reconnectToSS(ss_struct_1);
    int receiver_sock = reconnectToSS(ss_struct_2);
    printf("Sockets created");

    char *m = strchr(filename, '.');
    // printf("%d\n", m-filename);
    if (m)
    {
        copyHelper(sender_sock, receiver_sock, path1, path2);
    }
    // for directory
    else
    {
        printf("hello DIR\n");
        strcat(path2, "/");
        copyDir(ss_struct_1, ss_struct_2, sender_sock, receiver_sock, path1, path2);
    }
}

void *ss_handler(void *param)
{
    if (ss_num > 2 && backup_ss_array[ss_num - 1].has_dup == 0)
    {
        backup_ss_array[ss_num - 1].has_dup = 1;
        strcpy(backup_ss_array[ss_num - 1].ip_addr, ss_array[ss_num - 1].ip_addr);
        backup_ss_array[ss_num - 1].port_no_ns_b1 = ss_array[ss_num - 2].port_no_ns;
        backup_ss_array[ss_num - 1].port_no_ns_b2 = ss_array[ss_num - 3].port_no_ns;
        backup_ss_array[ss_num - 1].port_no_client_b1 = ss_array[ss_num - 2].port_no_client;
        backup_ss_array[ss_num - 1].port_no_client_b2 = ss_array[ss_num - 3].port_no_client;
        char buffer[1024];
        int socket1 = reconnectToSS(&ss_array[ss_num - 2]);
        snprintf(buffer, sizeof(buffer), "backup/SS%d_1/", ss_array[ss_num - 1].port_no_ns);
        FILE_("CREATE", socket1, buffer);
        int n = close(socket1);
        if (n < 0)
        {
            perror("close error");
            exit(1);
        }
        int socket2 = reconnectToSS(&ss_array[ss_num - 3]);
        snprintf(buffer, sizeof(buffer), "backup/SS%d_2/", ss_array[ss_num - 1].port_no_ns);
        FILE_("CREATE", socket2, buffer);
        n = close(socket2);
        if (n < 0)
        {
            perror("close error");
            exit(1);
        }
    }
    int socket = *(int *)param;
    // sem_wait(&x);
    int num;
    int m = recv(socket, &num, sizeof(int), 0);
    if (m == -1)
    {
        perror("[-]receive error45");
        exit(1);
    }

    int n = recv(socket, &ss_array[ss_num - 1], sizeof(ss_info), 0);
    if (n == -1)
    {
        perror("[-]receive error44");
        exit(1);
    }
    close(socket);

    for (int k = 0; k < ss_array[ss_num - 1].no_acc_paths; k++)
    {
        char buffer[1024];
        printf("%s\n", ss_array[ss_num - 1].accesible_paths[k]);
        snprintf(buffer, sizeof(buffer), "backup/SS%d_%d/%s", ss_array[ss_num - 1].port_no_ns, backup_ss_array[ss_num - 1].port_no_ns_b1, ss_array[ss_num - 1].accesible_paths[k]);
        insert(root, ss_array[ss_num - 1].accesible_paths[k], &ss_array[ss_num - 1]);
        insert(root, buffer, &ss_array[ss_num - 2]);
        snprintf(buffer, sizeof(buffer), "backup/SS%d_%d/", ss_array[ss_num - 1].port_no_ns, backup_ss_array[ss_num - 1].port_no_ns_b1);
        COPY(ss_array[ss_num - 1].accesible_paths[k], buffer);
        snprintf(buffer, sizeof(buffer), "backup/SS%d_%d/%s", ss_array[ss_num - 1].port_no_ns, backup_ss_array[ss_num - 1].port_no_ns_b2, ss_array[ss_num - 1].accesible_paths[k]);
        insert(root, buffer, &ss_array[ss_num - 3]);
        snprintf(buffer, sizeof(buffer), "backup/SS%d_%d/", ss_array[ss_num - 1].port_no_ns, backup_ss_array[ss_num - 1].port_no_ns_b2);
        COPY(ss_array[ss_num - 1].accesible_paths[k], buffer);
    }
    // sem_post(&x);

    // added this line
}

void *client_handler(void *param)
{
    int socket = *(int *)param;
    // sem_wait(&x);

    char buffer[MAX_CHARS];
    while (1)
    {
        bzero(buffer, MAX_CHARS);
        int n = recv(socket, buffer, sizeof(buffer), 0);
        if (n == -1)
        {
            perror("[-]receive error00");
            // exit(1);
        }
        printf(">>%s\n", buffer);

        int resCode = execute(buffer, &socket, lruQueue, root);
        if (resCode == 404)
        {
            logMessage(buffer, socket, 0);
            strcpy(buffer, "File / Directory Not found\n");
            n = send(socket, buffer, sizeof(buffer), 0);
            if (n < 0)
            {
                perror("[-] Send error");
            }
        }
        else
        {
            logMessage(buffer, socket, 1);
        }
        printf("hey\n");
        // should receive ack from server and send the msg to client
    }

    pthread_exit(NULL);
}

int main()
{
    root = getNode();
    logFile = fopen("log.txt", "w");
    if (logFile == NULL)
    {
        perror("[-] Log file opening error");
        exit(1);
    }
    int n, N;
    scanf("%d", &N);
    ss_array = malloc(sizeof(ss_info) * N);
    backup_ss_array = malloc(sizeof(ss_backup_info) * N);

    char *ip = "127.0.0.1";
    int port = 5000;
    lruQueue = initLRUcache(10);

    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[MAX_CHARS];

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0)
    {
        perror("[-]Socket error");
        exit(1);
    }
    printf("[+]TCP server socket created.\n");

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = port;
    server_addr.sin_addr.s_addr = inet_addr(ip);

    n = bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (n < 0)
    {
        server_addr.sin_port = 5050;
        n = bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
        perror("[-]Bind error");
        // exit(1);
    }
    printf("[+]Bind to the port number: %d\n", port);

    n = listen(server_sock, 5);
    if (n == -1)
    {
        perror("[-]listen error");
        exit(1);
    }
    printf("Listening...\n");

    for (int i = 0; i < N; i++)
    {
        backup_ss_array[i].has_dup = 0;
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (client_sock == -1)
        {
            perror("[-]accept");
            exit(1);
        }
        ss_num++;
        if (pthread_create(&ss_thread[i], NULL, ss_handler, &client_sock) != 0)
        {
            printf("Failed to create thread\n");
        }
    }

    while (1)
    {
        int client_socket;
        struct sockaddr_in client_address;
        addr_size = sizeof(client_address);
        client_socket = accept(server_sock, (struct sockaddr *)&client_address, &addr_size);
        if (client_socket == -1)
        {
            perror("[-]accept");
            exit(1);
        }
        bzero(buffer, MAX_CHARS);
        // printf("S<%d>\n", client_sock);
        n = recv(client_socket, buffer, sizeof(buffer), 0);
        if (n == -1)
        {
            perror("[-]receive error99");
            exit(1);
        }
        // printf("%s", buffer);
        if (strcmp(buffer, "Storage Server") == 0)
        {
            if (pthread_create(&ss_thread[ss_num++], NULL, ss_handler, &client_socket) != 0)
                printf("Failed to create thread\n");
        }
        else
        {
            if (pthread_create(&client_thread[c_num++], NULL, client_handler, &client_socket) != 0)
                printf("Failed to create thread\n");
        }
    }
    n = close(client_sock);
    if (n < 0)
    {
        perror("close error");
        exit(1);
    }
    for (int i = 0; i < N; i++)
    {
        pthread_join(ss_thread[i], NULL);
    }
    return 0;
}