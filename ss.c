#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <stdbool.h>

// So, the storage server is basically a client to the Naming Server,
// and a server to the clients.

#define MAX_CLIENTS 100
#define MAX_CHARS 100
#define MAX_FILE_LENGTH 9999
#define MAX_PATHS 20
#define ALPHABET_SIZE (128)
#define CHUNK_SIZE 16
#define PORT_NM 5000

struct SS_INFO{
    char ip_addr[30];
    int port_nm;
    int port_cln;
    int no_paths;
    int sockid;
    char accessible_paths[100][100];
};
typedef struct SS_INFO* SS_INFO;

struct TrieNode
{
    struct TrieNode *children[ALPHABET_SIZE];

    sem_t rw_queue;
    sem_t write_lock;

    // isEndOfWord is true if the node represents
    // end of a word
    bool isEndOfWord;
    bool isDir;
};

// struct Message{
//     char file_permissions; // 0 or 1
//     char* content;
// };

// typedef struct Message* Message;

struct CLIENT
{
    int sockid;
    struct sockaddr_in addr;
};
typedef struct CLIENT *CLIENT;

struct Response
{
    int responseCode;
    char *responseBuffer;
};
typedef struct Response *Response;

struct TrieNode *getNode(void);
void insert(struct TrieNode *root, const char *key);
struct TrieNode *GetTrieNode(struct TrieNode *root, const char *key);
void delete(struct TrieNode *root, const char *key);
bool isNodeEmpty(struct TrieNode *root);

struct TrieNode *root;

// NM OPERATIONS
int CreateFileDirectory(char *token, char *return_buffer)
{
    // for now
    // - we expect that we don't existing files
    //

    if (strchr(token, '.'))
    {
        FILE *new_file = fopen(token, "w");
        if (new_file == NULL)
        {
            perror("Error opening file");
            return -1;
        }
        strcpy(return_buffer, "File created Successfully!\n");
    }
    else
    {
        if (mkdir(token, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0)
        {
            perror("Error creating directory");
            return -1;
        }
        strcpy(return_buffer, "Directory created Successfully!\n");
    }
    insert(root, token);
    return 0;
}

int DeleteFileDirectory(char *token, char *return_buffer)
{
    // we expect
    //

    struct TrieNode *node = GetTrieNode(root, token);
    sem_wait(&node->rw_queue);
    if (strchr(token, '.'))
    {
        if (remove(token) != 0)
        {
            perror("Error deleting file");
            return -1;
        }
        strcpy(return_buffer, "Deleted file successfully!\n");
    }
    else
    {
        if (rmdir(token) != 0)
        {
            perror("Error removing directory");
            return 1;
        }
        strcpy(return_buffer, "Directory removed successfully!\n");
    }
    sem_post(&node->rw_queue);

    delete (root, token);
}

const char *stop_packet = "<STOP>";
void sendChunks(int sockid, char *buffer)
{
    printf("sntt\n");
    int buffer_length = strlen(buffer);
    int chunks = buffer_length / CHUNK_SIZE;
    int mod_chunk = buffer_length % CHUNK_SIZE;

    int i = 0;
    while (i <= chunks)
    {
        printf("%s", &buffer[i * CHUNK_SIZE]);
        send(sockid, &buffer[i * CHUNK_SIZE], CHUNK_SIZE, 0);
        i++;
    }

    // sending the stop packet
    send(sockid, stop_packet, strlen(stop_packet) + 1, 0);
}

void receiveChunks(int sockid, char *recieve_buffer)
{
    char buffer[CHUNK_SIZE];
    int chunks = 0;
    while (strcmp(&buffer[(chunks - 1) * CHUNK_SIZE], stop_packet) != 0)
    {
        recv(sockid, &buffer[(chunks++) * CHUNK_SIZE], CHUNK_SIZE, 0);
    }
    buffer[(chunks - 1) * CHUNK_SIZE] = '\0';
}

// CLIENT OPERATIONS
int ReadFile(int sockid, char *token, char *return_buffer)
{

    FILE *fp = fopen(token, "r");
    if (fp == NULL)
    {
        perror("fopen");
        return -1;
    }
    printf("helli");
    struct TrieNode *node = GetTrieNode(root, token);
    sem_wait(&node->rw_queue);

    fgets(return_buffer, 1024, fp);
    printf("hoii %s\n", return_buffer);

    sem_post(&node->rw_queue);

    sendChunks(sockid, return_buffer);

    return 0;
}

int WriteFile(int sockid, char *token, char *return_buffer)
{
    // first token - "Info to be written"
    // second token - path

    char* file_name = strdup(token);
    FILE *fp = fopen(file_name, "w");
    
    char write_buffer[MAX_FILE_LENGTH];
    
    token = strtok(NULL, "\t");
    strcpy(write_buffer, token);
    
    int buffer_len = strlen(write_buffer);

    printf("LADIES & GENTLEMEN\n");

    struct TrieNode *node = GetTrieNode(root, file_name);
    sem_wait(&node->rw_queue);
    sem_wait(&node->write_lock);

    if (fputs(write_buffer, fp) == EOF)
    {
        perror("Error writing to file");
        fclose(fp);
        return 1;
    }

    strcpy(return_buffer, "Successfully written to file\n");
    sendChunks(sockid, return_buffer);

    sem_post(&node->write_lock);
    sem_post(&node->rw_queue);

    fclose(fp);
    return 0;
}

int GetSizeAndPermissions(char *token, char *return_buffer)
{
    struct TrieNode *node = GetTrieNode(root, token);

    sem_wait(&node->rw_queue);
    struct stat file_info;
    if (stat(token, &file_info) == -1)
    {
        perror("Error getting file information");
        return -1;
    }
    sem_post(&node->rw_queue);

    snprintf(return_buffer, MAX_CHARS * 2,
             "File Size: %lld bytes\nFile Permissions: %o\n",
             (long long)file_info.st_size,
             file_info.st_mode & 0777);

    return 0;
}

int GetFile(int sockid, char *token, char *return_buffer)
{
    char buffer[MAX_FILE_LENGTH];
    if (ReadFile(sockid, token, buffer) == 0)
    {
        sendChunks(PORT_NM, buffer);
        return 0;
    }
    else
    {
        return -1;
    }
}

int PutFile(char *token, char *return_buffer)
{
}

void *client_handler(void *arg);
void *naming_handler(void *arg);

int main(int argc, char *argv[])
{
    int port_cln = atoi(argv[1]);

    char *serv_ip = "192.168.166.156";
    char *nm_ip = "192.168.166.105";

    SS_INFO ss_info = (SS_INFO) malloc(sizeof(struct SS_INFO));
    strcpy(ss_info->ip_addr, serv_ip);
    ss_info->port_cln = port_cln;
    ss_info->port_nm = PORT_NM;

    root = getNode();

    int N;
    printf("No. of accessible paths: ");
    scanf("%d", &N);
    ss_info->no_paths = N;
    // ss_info->accessible_paths = (char **) malloc(sizeof(char *) * N);
    // for(int i = 0; i < N; i++){
    //     ss_info->accessible_paths[i] = (char *) malloc(sizeof(char) * 100);
    // }

    char paths[MAX_PATHS][100];
    for (int i = 0; i < N; i++)
    {
        scanf("%s", paths[i]);
        insert(root, paths[i]);
        // printf("<%s>", paths[i]);
        strcpy(ss_info->accessible_paths[i], paths[i]);
    }

    int nm_sock, ss_sock;
    struct sockaddr_in nm_addr, ss_addr;
    socklen_t addr_size;

    ss_sock = socket(AF_INET, SOCK_STREAM, 0);
    ss_info->sockid = ss_sock;

    nm_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (nm_sock < 0)
    {
        perror("[-]Socket error");
        exit(1);
    }
    printf("* | Created Storage Server's socket on port %d.\n", PORT_NM);

    // socket port for NM
    memset(&nm_addr, '\0', sizeof(nm_addr));
    nm_addr.sin_family = AF_INET;
    nm_addr.sin_port = PORT_NM;
    nm_addr.sin_addr.s_addr = inet_addr(nm_ip);

    // we use connect here, because it is a client to the naming server
    int conn = connect(nm_sock, (struct sockaddr *)&nm_addr, sizeof(nm_addr));
    printf("Connected to the Naming Server at port %d status: %d\n", PORT_NM, conn);

    send(nm_sock, &ss_info->no_paths, sizeof(int), 0);
    send(nm_sock, ss_info, sizeof(struct SS_INFO), 0);

    pthread_t naming_thread;
    pthread_create(&naming_thread, NULL, naming_handler, &nm_sock);

    // socket port for Clients
    memset(&ss_addr, '\0', sizeof(ss_addr));
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = port_cln;
    ss_addr.sin_addr.s_addr = inet_addr(serv_ip);

    // we bind here, to open a server for the clients to connect()
    if (bind(ss_sock, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0)
    {
        perror("[-]Bind error");
    }
    printf("* | Bind to the port_cln number: %d\n", port_cln);

    listen(ss_sock, MAX_CLIENTS);
    printf("* | Listening... \n");

    pthread_t threads[MAX_CLIENTS];
    int no_clients = 0;

    while (1)
    {
        CLIENT client = (CLIENT)malloc(sizeof(struct CLIENT));
        addr_size = sizeof(client->addr);

        // accepting clients, then creating threads for each
        client->sockid = accept(ss_sock, (struct sockaddr *)&client->addr, &addr_size);

        // printf("HELLO");

        pthread_create(&threads[no_clients++], NULL, client_handler, client);
    }

    return 0;
}

void *client_handler(void *arg)
{
    CLIENT client = (CLIENT) arg;
    printf("[+]Client connected. %d %d\n", client->sockid, client->addr.sin_port);

    char buffer[MAX_CHARS];

    // receiving the command from the client.
    bzero(buffer, MAX_CHARS);
    
    int n = recv(client->sockid, buffer, sizeof(buffer), 0);
    if(n == -1){
        perror("recv");
    }
    printf("Client: %s\n", buffer);

    char return_buffer[MAX_FILE_LENGTH];
    
    printf("HELLO\n");
    char *operation = strtok(buffer, "\t");
    if (strcmp(operation, "READ") == 0)
    {
        int response = ReadFile(client->sockid, strtok(NULL, "\t"), return_buffer);
    }
    else if (strcmp(operation, "WRITE") == 0)
    {
        int response = WriteFile(client->sockid, strtok(NULL, "\t"), return_buffer);
    }
    else if (strcmp(operation, "INFO") == 0)
    {
        int response = GetSizeAndPermissions(strtok(NULL, "\t"), return_buffer);
    }
    printf("HELL22O\n");


    // have to implement chunks here..
    // send(client->sockid, return_buffer, sizeof(return_buffer), 0);

    // STOP packet

    // close(client->sockid);
    // printf("[+]Client sock %d disconnected.\n\n", client->sockid);

    return NULL;
}

void *naming_handler(void *arg)
{
    int nm_sock = *(int *)arg;

    // while (1)
    // {
    //     char command_buffer[MAX_CHARS];
    //     recv(nm_sock, command_buffer, sizeof(command_buffer), 0);
    // }
}

struct TrieNode *getNode(void)
{
    struct TrieNode *pNode = NULL;

    pNode = (struct TrieNode *)malloc(sizeof(struct TrieNode));

    if (pNode)
    {
        int i;

        pNode->isDir = true;
        pNode->isEndOfWord = false;

        for (i = 0; i < ALPHABET_SIZE; i++)
            pNode->children[i] = NULL;
    }

    return pNode;
}

void insert(struct TrieNode *root, const char *key)
{
    int level;
    int length = strlen(key);
    int index;

    struct TrieNode *pCrawl = root;

    for (level = 0; level < length; level++)
    {
        index = key[level];
        if (!pCrawl->children[index])
            pCrawl->children[index] = getNode();

        pCrawl = pCrawl->children[index];
    }

    sem_init(&pCrawl->rw_queue, 0, 1);
    sem_init(&pCrawl->write_lock, 0, 1);

    if (strchr(key, '.'))
        pCrawl->isDir = false;

    // mark last node as leaf
    pCrawl->isEndOfWord = true;
}

struct TrieNode *GetTrieNode(struct TrieNode *root, const char *key)
{
    int level;
    int length = strlen(key);
    int index;
    struct TrieNode *pCrawl = root;
    struct TrieNode *parent = pCrawl;

    for (level = 0; level < length; level++)
    {
        index = key[level];

        if (!pCrawl->children[index])
            insert(parent, key + level);

        parent = pCrawl;
        pCrawl = pCrawl->children[index];
        printf("%c", key[level]);
    }
    printf("\n");

    return (pCrawl);
}

bool deleteHelper(struct TrieNode *root, const char *key, int level, int length)
{
    if (root)
    {
        // Base case: If the last character of the key is being processed
        if (level == length)
        {
            if (root->isEndOfWord)
            {
                // Unmark the end of the word flag
                root->isEndOfWord = false;

                // If the node has no other children, it is safe to delete
                return isNodeEmpty(root);
            }
        }
        else // Recursive case: Traverse to the next level
        {
            int index = key[level];
            if (deleteHelper(root->children[index], key, level + 1, length))
            {
                // Delete the node if it has no children and not marked as the end of a word
                if (!isNodeEmpty(root->children[index]) && !root->children[index]->isEndOfWord)
                {
                    free(root->children[index]);
                    root->children[index] = NULL;

                    // If the current node has no other children, it is safe to delete
                    return isNodeEmpty(root);
                }
            }
        }
    }

    return false;
}

// Delete a key from the trie
void delete(struct TrieNode *root, const char *key)
{
    int length = strlen(key);
    deleteHelper(root, key, 0, length);
}

bool isNodeEmpty(struct TrieNode *root)
{
    for (int i = 0; i < ALPHABET_SIZE; i++)
    {
        if (root->children[i] != NULL)
            return false;
    }
    return true;
}