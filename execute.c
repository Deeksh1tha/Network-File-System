#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "structs.h"

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
void execute(char *command, int client_socket, struct LRUcache *lruQueue, struct TrieNode *root)
{
    char input[1024];
    strcpy(input, command);
    char *token = strtok(command, "\t\n");

    if (strcpy(token, "COPY") == 0) 
    {
        // copy
        token = strtok(NULL, "\t\n");
        ss_info *ss_struct_1 = getFromLRUcache(lruQueue, token);
        if (ss_struct_1 == NULL)
        {
            ss_struct_1 = search(root, token);
            enqueue(lruQueue, token, ss_struct_1);
        }
        if (ss_struct_1 == NULL)
        {
            perror("File/Directory unavailable");
        }
        char path1[1024];
        strcpy(path1, token);
        token = strtok(NULL, "\t\n");
        ss_info *ss_struct_2 = getFromLRUcache(lruQueue, token);
        if (ss_struct_2 == NULL)
        {
            ss_struct_2 = search(root, token);
            enqueue(lruQueue, token, ss_struct_2);
        }
        if (ss_struct_2 == NULL)
        {
            perror("File/Directory unavailable");
        }
        // in ss if there only path (no create or delete when tokenized) -> copy
        int ss_socket=reconnectToSS(ss_struct_1);
        if (ss_socket != -1)
        {
            printf("Connected to SS.\n");
            int n = send(ss_socket, path1, sizeof(path1), 0);
            if (n < 0)
            {
                perror("[-]Send error");
                exit(1);
            }
        }
        get(path1, ss_struct_1);
        reconnectToSS(ss_struct_1);
        if (ss_socket != -1)
        {
            printf("Connected to SS.\n");
            int n = send(ss_socket, path1, sizeof(path1), 0);
            if (n < 0)
            {
                perror("[-]Send error");
                exit(1);
            }
        }
        put(token, ss_struct_2);
    }
    else if (strcpy(token, "create") == 0 || strcpy(token, "delete") == 0)
    {
        // delete or create
        token = strtok(NULL, "\t\n");
        ss_info *ss_struct = getFromLRUcache(lruQueue, token);
        if (ss_struct == NULL)
        {
            ss_struct = search(root, token);
            enqueue(lruQueue, token, ss_struct);
        }
        if (ss_struct == NULL)
        {
            perror("File/Directory unavailable");
        }

        // establish  connection with ss -> send create/delete filepath
        int ss_socket = reconnectToSS(ss_struct);
        if (ss_socket != -1)
        {
            printf("Connected to SS.\n");
            int n = send(ss_socket, input, sizeof(input), 0);
            if (n < 0)
            {
                perror("[-]Send error");
                exit(1);
            }
        }
    }
    else
    {
        token = strtok(NULL, "\t\n");
        ss_info *ss_struct = getFromLRUcache(lruQueue, token);
        if (ss_struct == NULL)
        {
            ss_struct = search(root, token);
            enqueue(lruQueue, token, ss_struct);
        }
        if (ss_struct == NULL)
        {
            perror("File/Directory unavailable");
        }
        int n = send(client_socket, ss_struct, sizeof(ss_info), 0);
        if (n == -1)
        {
            perror("[-]send error");
            exit(1);
        }
    }
}