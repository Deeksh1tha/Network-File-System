#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "structs.h"
#include <fcntl.h>
#include <sys/stat.h> 
#define LOG_FILE "log.txt"
#define MAX_LOG_MSG_SIZE 256
extern FILE *logFile;
const char *stop_packet = "<STOP>";



void sendChunks(int sockid, char *buffer)
{
    // printf("sntt\n");
    int buffer_length = strlen(buffer) + 1;
    int chunks = buffer_length / CHUNK_SIZE;
    int mod_chunk = buffer_length % CHUNK_SIZE;

    int i = 0;
    while (i <= chunks)
    {
        printf("> %s", &buffer[i * CHUNK_SIZE]);
        send(sockid, &buffer[i * CHUNK_SIZE], CHUNK_SIZE, 0);
        i++;
    }

    // sending the stop packet
    send(sockid, stop_packet, CHUNK_SIZE, 0);
    printf("Sent <STOP>!\n");
}

void receiveChunks(int sockid, char *recieve_buffer)
{
    // char buffer[MAX_FILE_LENGTH];
    int num_chunks = 0;
    while (1)
    {
        int n = recv(sockid, &recieve_buffer[num_chunks * CHUNK_SIZE], CHUNK_SIZE, 0);
        if (n < 0)
        {
            perror("[-]Recv error");
            exit(1);
        }
        if (strcmp(&recieve_buffer[num_chunks * CHUNK_SIZE], stop_packet) == 0)
            break;
        num_chunks++;
    }
    printf("recv: %s\n", recieve_buffer);
    logMessage(recieve_buffer,sockid,1);
}

char file_buffer[MAX_FILE_LENGTH];

int copyHelper(int sender_sock, int receiver_sock, char *path1, char *path2)
{
    printf("in helper\n");
    char command[MAX_CHARS];
    strcpy(command, "GET\t");
    strcat(command, path1);
    if (send(sender_sock, command, MAX_CHARS, 0) < 0)
    {
        perror("[-] send error");
    }

    strcpy(file_buffer, "");
    receiveChunks(sender_sock, file_buffer);
    printf("Recvv: %s\n", file_buffer);

    strcpy(command, "PUT\t");
    strcat(command, path2);
    if (send(receiver_sock, command, MAX_CHARS, 0) < 0)
    {
        perror("[-] send error");
    }
    sendChunks(receiver_sock, file_buffer);

    receiveChunks(receiver_sock, file_buffer);
}

int FILE_(char *operation, int sockid, char *path)
{
    char command[MAX_CHARS];
    strcpy(command, operation);
    strcat(command, path);

    int n = send(sockid, command, MAX_CHARS, 0);
    if (n < 0)
    {
        perror("[-] semd error");
    }
    char buff[MAX_FILE_LENGTH];
    receiveChunks(sockid, buff);
    printf("* | %s\n", buff);
}

int copyDir(ss_info *ss_struct_1, ss_info *ss_struct_2, int sender_id, int receiver_id, char *path1, char *path2)
{
    // request for the files in the directory.
    char command[MAX_CHARS];
    strcpy(command, "GIVEFILES\t");
    strcat(command, path1);
    int n = send(sender_id, command, MAX_CHARS, 0);
    if (n < 0)
    {
        perror("[-] send error");
    }
    char buffer[MAX_FILE_LENGTH];
    receiveChunks(sender_id, buffer);

    char new_path1[MAX_CHARS];
    char new_path2[MAX_CHARS];

    // have to create the directory with the name first, so, have to get the last token in the path.
    FILE_("CREATE\t", receiver_id, path2);

    printf("path2: %s\n", path2);
    char *token = strtok(buffer, ",");
    while (token != NULL)
    {
        strcpy(new_path1, path1);
        strcat(new_path1, token);
        strcpy(new_path2, path2);
        strcat(new_path2, token);
        int sender_sock = reconnectToSS(ss_struct_1);
        int receiver_sock = reconnectToSS(ss_struct_2);

        if (!strchr(new_path1, '.'))
        {
            strcat(new_path1, "/");
            strcat(new_path2, "/");
            printf("> %s %s\n", new_path1, new_path2);
            copyDir(ss_struct_1, ss_struct_2, sender_sock, receiver_sock, new_path1, new_path2);
        }
        else
        {
            copyHelper(sender_sock, receiver_sock, new_path1, new_path2);
            // close(sender_sock);
            // close(receiver_sock);
        }

        token = strtok(NULL, "\n");
    }

    // now, have to get files in the directory
    // for(each file){
    //     copyHelper(sender_sock, receiver_sock, path1+filename, path2+filename);
    // }
}

int reconnectToSS(ss_info *ss_struct)
{
    int ss_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (ss_socket < 0)
    {
        perror("[-]Socket error");
        return -1;
    }
    struct sockaddr_in ss_addr;
    memset(&ss_addr, '\0', sizeof(ss_addr));
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = ss_struct->port_no_ns;
    printf("re: %d %d\n", ss_struct->port_no_client, ss_struct->port_no_ns);
    ss_addr.sin_addr.s_addr = inet_addr(ss_struct->ip_addr);
    int n = connect(ss_socket, (struct sockaddr *)&ss_addr, sizeof(ss_addr));
    if (n < 0)
    {
        perror("[-]Connection error");
        close(ss_socket);
        return -1;
    }
    return ss_socket;
}

void* backup_copy(void *arg)
{
    char* socket = (char *)arg;
    char *path1 = strtok(socket, "?");
    char *path2 = strtok(NULL, "?");
    COPY(path1, path2);
}

int execute(char *command, int *client_socket, struct LRUcache *lruQueue, struct TrieNode *root)
{
    char input[1024];
    strcpy(input,command);
    printf("%s\n", command);
    char *token = strtok(command, "\t\n");

    if (strcmp(token, "COPY") == 0)
    {
        int nm = 1;
        int n = send(*client_socket, &nm, sizeof(int), 0);

        char path1[MAX_CHARS];
        token = strtok(NULL, "\t\n");
        strcpy(path1, token);

        char path2[MAX_CHARS];
        token = strtok(NULL, "\t\n");
        strcpy(path2, token);

        printf("copy %s %s\n", path1, path2);

        int responseCode = COPY(path1, path2);

        char buffer[MAX_CHARS];
        strcpy(buffer, "Copy Successful!\n");
        logMessage(input,*client_socket,1);
        printf("buff: %s", buffer);
        sendChunks(*client_socket, buffer);
    }
    else if (strcmp(token, "CREATE") == 0 || strcmp(token, "DELETE") == 0)
    {
        char *operation = strdup(token);
        token = strtok(NULL, "\t\n");
        int nm = 1;
        int n = send(*client_socket, &nm, sizeof(int), 0);

        ss_info *ss_struct = getFromLRUcache(lruQueue, token);
        if (ss_struct == NULL)
        {
            ss_struct = search(root, token);
            enqueue(lruQueue, token, ss_struct);
        }
        if (ss_struct == NULL)
        {
            perror("File/Directory unavailable");
            return 404;
        }

        // establish connection with ss -> send create/delete filepath
        int ss_socket = reconnectToSS(ss_struct);
        FILE_(operation, ss_socket, token);
    }
    else
    {
        printf("?%s\n", token);
        char command[1024];
        strcpy(command, token);
        token = strtok(NULL, "\t\n");
        // ss_info *ss_struct = search(root, token);

        int nm = 0;
        int n = send(*client_socket, &nm, sizeof(int), 0);
        //-------------
        ss_info *ss_struct = getFromLRUcache(lruQueue, token);
        printf("SDFF\n");
        if (ss_struct == NULL)
        {
            ss_struct = search(root, token);
            enqueue(lruQueue, token, ss_struct);
        }
        if (ss_struct == NULL)
        {
            perror("File/Directory unavailable");
        }
        printf("%s\n", token);
        n = send(*client_socket, ss_struct, sizeof(ss_info), 0);
        if (n == -1)
        {
            perror("[-]send error2");
            exit(1);
        }
        int flag;
        n = recv(*client_socket, &flag, sizeof(flag), 0);
        if (n == -1)
        {
            perror("[-]recv error2");
            exit(1);
        }
        //------------
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "backup/SS%d_1/%s", ss_struct->port_no_ns, token);
        ss_info *ss_struct_b1 = getFromLRUcache(lruQueue, buffer);
        if (ss_struct_b1 == NULL)
        {
            ss_struct_b1 = search(root, buffer);
            enqueue(lruQueue, token, ss_struct_b1);
        }
        if (ss_struct_b1 == NULL)
        {
            perror("File/Directory unavailable as backup");
        }
        snprintf(buffer, sizeof(buffer), "backup/SS%d_2/%s", ss_struct->port_no_ns, token);
        ss_info *ss_struct_b2 = getFromLRUcache(lruQueue, buffer);
        if (ss_struct_b2 == NULL)
        {
            ss_struct_b2 = search(root, buffer);
            enqueue(lruQueue, token, ss_struct_b2);
        }
        if (ss_struct_b2 == NULL)
        {
            perror("File/Directory unavailable  as backup");
        }
        if (flag == 0 && strcmp(command, "READ") == 0)
        {
            // no ss
            n = send(*client_socket, ss_struct_b1, sizeof(ss_info), 0);
            if (n == -1)
            {
                perror("[-]send error2");
                exit(1);
            }
            n = send(*client_socket, ss_struct_b2, sizeof(ss_info), 0);
            if (n == -1)
            {
                perror("[-]send error2");
                exit(1);
            }
        }
        else
        {
            if (ss_struct_b1 != NULL && ss_struct_b2 != NULL)
            {
                pthread_t ss_backup_threads[2];
                for (int i = 0; i < 2; i++)
                {
                    snprintf(buffer, sizeof(buffer), "%s?backup/SS%d_1/%s", token, ss_struct->port_no_ns, token);
                    if (pthread_create(&ss_backup_threads[i], NULL, backup_copy, &buffer) != 0)
                    {
                        printf("Failed to create thread\n");
                    }
                }
            }
        }
    }
}
