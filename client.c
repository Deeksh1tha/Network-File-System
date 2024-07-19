#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "structs.h"

int receiveChunks(int sockid, char *data_recv)
{
    int num_chunks = 0;
    while (1)
    {
        int n = recv(sockid, &data_recv[num_chunks * CHUNK_SIZE], CHUNK_SIZE, 0);
        if (n < 0)
        {
            perror("[-]Recv error");
            exit(1);
        }
        if (strcmp(&data_recv[num_chunks * CHUNK_SIZE], "<STOP>") == 0)
            break;
        num_chunks++;
    }
}

int main()
{
    char *ip = "127.0.0.1";
    int port;
    scanf("%d", &port);

    int sock;
    struct sockaddr_in addr;
    char buffer[100];
    char buff2[100];
    int n;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("[-]Socket error");
        exit(1);
    }
    printf("[+]TCP client NM socket created.\n");

    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = port;
    addr.sin_addr.s_addr = inet_addr(ip);
    // if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0)
    // {
    //     perror("[-]Invalid address");
    //     exit(1);
    // }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("[-]Connect error");
        exit(1);
    }
    printf("Connected to NM.\n");
    bzero(buff2, 100);
    getchar();
    fgets(buff2, 100, stdin);
    n = send(sock, buff2, sizeof(buff2), 0);
    if (n < 0)
    {
        perror("[-]Send error");
        exit(1);
    }
    char data_recv[MAX_CHARS];
    while (1)
    {
        bzero(buffer, 100);
        // getchar();
        printf("PRINTHERE\n");
        fgets(buffer, 100, stdin);
        printf("<%s>", buffer);
        char input[1024];
        strcpy(input, buffer);
        char *command = strtok(input, "\t\n");
        fflush(stdout);
        buffer[strlen(buffer) - 1] = '\0';

        n = send(sock, buffer, sizeof(buffer), 0);
        if (n < 0)
        {
            perror("[-]Send error");
            exit(1);
        }
        int next_step = -1;
        n = recv(sock, &next_step, sizeof(int), 0);

        if (next_step)
        {
            receiveChunks(sock, data_recv);
            continue;
        }

        ss_info *ss_struct = (ss_info *)malloc(sizeof(ss_info));
        // bzero(&ss_struct, sizeof(ss_struct));
        n = recv(sock, ss_struct, sizeof(ss_info), 0);
        if (n < 0)
        {
            perror("[-]Recv error");
            exit(1);
        }

        char ip_ss[30];
        strcpy(ip_ss, ss_struct->ip_addr);
        int port_ss = ss_struct->port_no_client;
        int sock_ss;
        struct sockaddr_in addr_ss;

        sock_ss = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_ss < 0)
        {
            perror("[-]Socket error");
            exit(1);
        }
        printf("[+]TCP client SS socket created.\n");

        memset(&addr_ss, '\0', sizeof(addr_ss));
        addr_ss.sin_family = AF_INET;
        addr_ss.sin_port = port_ss;
        addr_ss.sin_addr.s_addr = inet_addr(ip_ss);
        // if (inet_pton(AF_INET, ip_ss, &addr_ss.sin_addr) <= 0)
        // {
        //     perror("[-]Invalid address");
        //     exit(1);
        // }

        int flag = 1;
        if (connect(sock_ss, (struct sockaddr *)&addr_ss, sizeof(addr_ss)) < 0)
        {
            perror("[-]Connect error");
            // if (strcmp(command, "READ") == 0)
                flag = 0;
            // else
                // exit(1);
        }
        n = send(sock, &flag, sizeof(int), 0);
        if (n < 0)
        {
            perror("[-]Send error");
            exit(1);
        }
        if (flag == 0)
        {
            // recv
            ss_info *ss_struct_1 = (ss_info *)malloc(sizeof(ss_info));
            ss_info *ss_struct_2 = (ss_info *)malloc(sizeof(ss_info));
            n = recv(sock, ss_struct_1, sizeof(ss_info), 0);
            if (n < 0)
            {
                perror("[-]Recv error");
                exit(1);
            }
            n = recv(sock, ss_struct_2, sizeof(ss_info), 0);
            if (n < 0)
            {
                perror("[-]Recv error");
                exit(1);
            }
            char ip_ss1[30];
            char ip_ss2[30];
            strcpy(ip_ss1, ss_struct_1->ip_addr);
            strcpy(ip_ss2, ss_struct_2->ip_addr);
            int port_ss1 = ss_struct_1->port_no_client;
            int port_ss2 = ss_struct_2->port_no_client;
            int sock_ss1;
            int sock_ss2;
            struct sockaddr_in addr_ss1;
            struct sockaddr_in addr_ss2;

            sock_ss1 = socket(AF_INET, SOCK_STREAM, 0);
            sock_ss2 = socket(AF_INET, SOCK_STREAM, 0);
            if (sock_ss1 < 0 && sock_ss2 < 0)
            {
                perror("[-]Socket error");
                exit(1);
            }
            printf("[+]TCP client SS socket created.\n");

            memset(&addr_ss1, '\0', sizeof(addr_ss1));
            memset(&addr_ss2, '\0', sizeof(addr_ss2));
            addr_ss1.sin_family = AF_INET;
            addr_ss1.sin_port = port_ss1;
            addr_ss1.sin_addr.s_addr = inet_addr(ip_ss1);
            addr_ss2.sin_family = AF_INET;
            addr_ss2.sin_port = port_ss2;
            addr_ss2.sin_addr.s_addr = inet_addr(ip_ss2);
            // if (inet_pton(AF_INET, ip_ss, &addr_ss.sin_addr) <= 0)
            // {
            //     perror("[-]Invalid address");
            //     exit(1);
            // }
            int check_backup = 1;
            if (connect(sock_ss1, (struct sockaddr *)&addr_ss1, sizeof(addr_ss1)) < 0)
            {
                perror("[-]Connect error");
                // exit(1);
                check_backup = 0;
                sock_ss = sock_ss2;
            }
            else if (connect(sock_ss2, (struct sockaddr *)&addr_ss2, sizeof(addr_ss2)) < 0)
            {
                perror("[-]Connect error");
                exit(1);
            }
            if (check_backup == 1)
            {
                sock_ss = sock_ss1;
            }
            printf("Connected to backup SS.\n");
        }
        else
            printf("Connected to SS.\n");

        // buffer[strlen(buffer) - 1] = '\0';

        n = send(sock_ss, buffer, sizeof(buffer), 0);
        if (n < 0)
        {
            perror("[-]Send error");
            exit(1);
        }
        bzero(data_recv, MAX_CHARS);
        receiveChunks(sock_ss, data_recv);

        printf("%s", data_recv);
        // sleep(5);
        printf("closrd\n");
        close(sock_ss);
        // break;

        printf("Disconnected from SS.\n");
        // getchar();
    }
    return 0;
}