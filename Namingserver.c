#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include "structs.h"

sem_t x, y;
ss_info *ss_array;
pthread_t ss_thread[100];
pthread_t client_thread[100];
struct LRUcache*lruQueue;
FILE *logFile;
int ss_num = 0;
int c_num = 0;

struct TrieNode *root;

void logMessage(const char *message, struct sockaddr_in *addr)
{
    char ipAddr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr->sin_addr), ipAddr, INET_ADDRSTRLEN);

    fprintf(logFile, "[%s:%d] %s\n", ipAddr, ntohs(addr->sin_port), message);
    fflush(logFile);
}
void *ss_handler(void *param)
{
    int socket = *(int *)param;
    // sem_wait(&x);
    int num;
    int m = recv(socket, &num, sizeof(int), 0);
    if(m == -1){
        perror("[-]receive error45");
        exit(1);
    }
    // printf("<%d>\n", num);
    // ss_array[ss_num - 1].accesible_paths = (char**) malloc(sizeof(char *) * num+1);
    // for (int j = 0; j < num; j++)
    // {
    //     ss_array[ss_num - 1].accesible_paths[j] = (char*) malloc(sizeof(char) * 100);
    // }


    int n = recv(socket, &ss_array[ss_num - 1], sizeof(ss_info), 0);
    if (n == -1)
    {
        perror("[-]receive error44");
        exit(1);
    }
    for (int k = 0; k < ss_array[ss_num - 1].no_acc_paths; k++)
    {
        printf("%s\n", ss_array[ss_num - 1].accesible_paths[k]);
        insert(root, ss_array[ss_num - 1].accesible_paths[k], &ss_array[ss_num - 1]);
    }
    sem_post(&x);
}

void *client_handler(void *param)
{
    int socket = *(int *)param;
    // sem_wait(&x);

    char buffer[1024];
    while(1){
        bzero(buffer, 1024);
        int n = recv(socket, buffer, sizeof(buffer), 0);
        if (n == -1)
        {
            perror("[-]receive error00");
            exit(1);
        }

        // execute(buffer,socket,lruQueue,root);

//-----------
        char *token = strtok(buffer, "\t\n");
        token = strtok(NULL, "\t\n");

        ss_info *ss_struct = getFromLRUcache(lruQueue, token);

        if (ss_struct == NULL)
        {
            ss_struct = search(root, token);
            enqueue(lruQueue, token, ss_struct);
        }
        // ss_info* ss_struct = search(root, token);
        // sem_post(&x);
        if(ss_struct==NULL)
        {
            perror("File/Directory unavailable");
        }
        // should handle the case where other client is currently writing to the file
         printf("%s\n", ss_struct->ip_addr);
        n = send(socket, ss_struct, sizeof(ss_info), 0);
        if (n == -1)
        {
            perror("[-]send error");
            exit(1);
        }
//---------


    }
    // should receive ack from server and send the msg to client
    pthread_exit(NULL);
}

int main()
{
    logFile = fopen("log.txt", "w");
    if (logFile == NULL)
    {
        perror("[-] Log file opening error");
        exit(1);
    }
    int n, N;
    scanf("%d", &N);
    ss_array = malloc(sizeof(ss_info) * N);
    char *ip = "192.168.166.105";
    int port = 5000;
    lruQueue = initLRUcache(10);
    root = getNode();
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[1024];

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
        perror("[-]Bind error");
        exit(1);
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
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (client_sock == -1)
        {
            perror("[-]accept");
            exit(1);
        }
        ss_num++;
        if (pthread_create(&ss_thread[i], NULL, ss_handler, &client_sock) != 0){
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
        bzero(buffer, 1024);
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
    for(int i = 0; i < N; i++){
        pthread_join(ss_thread[i], NULL);
    }
    return 0;
}