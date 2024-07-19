#include<stdio.h>
#include<stdlib.h>
#include <semaphore.h>
#include<string.h>
#include"structs.h"
 
extern sem_t LRU_lock;

struct LRUcache *initLRUcache(int capacity)
{
    struct LRUcache *queue = (struct LRUcache *)malloc(sizeof(struct LRUcache));
    queue->front = queue->rear = NULL;
    queue->size = 0;
    queue->capacity = capacity;
    sem_init(&LRU_lock, 0, 1);
    return queue;
}

void enqueue(struct LRUcache *queue, char *key, ss_info *value)
{
    sem_wait(&LRU_lock);
    if (queue->size == queue->capacity)
    {
        struct LRUNode *temp = queue->front;
        queue->front = temp->next;
        free(temp);
        queue->size--;
    }
    
    struct LRUNode *newNode = (struct LRUNode *)malloc(sizeof(struct LRUNode));
    strcpy(newNode->key, key);
    newNode->value = value;
    newNode->next = NULL;

    if (queue->rear == NULL)
    {
        queue->front = queue->rear = newNode;
    }
    else
    {
        queue->rear->next = newNode;
        queue->rear = newNode;
    }

    queue->size++;
    sem_post(&LRU_lock);
}

void dequeue(struct LRUcache *queue, char *key)
{
    sem_wait(&LRU_lock);
    if (queue->front == NULL){
        sem_post(&LRU_lock);
        return;
    }

    struct LRUNode *temp = queue->front, *prev = NULL;
    while (temp != NULL && strcmp(temp->key, key) != 0)
    {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL){
        sem_post(&LRU_lock);
        return;
    }

    if (prev == NULL)
        queue->front = temp->next;
    else
        prev->next = temp->next;

    if (temp == queue->rear)
        queue->rear = prev;

    free(temp);
    queue->size--;
    sem_post(&LRU_lock);
}

ss_info *getFromLRUcache(struct LRUcache *queue, char *key)
{
    sem_wait(&LRU_lock);
    struct LRUNode *temp = queue->front;
    while (temp != NULL)
    {
        if (strcmp(temp->key, key) == 0)
        {
            sem_post(&LRU_lock);

            dequeue(queue, key);
            enqueue(queue, key, temp->value);
            return temp->value;
        }
        temp = temp->next;
    }

    sem_post(&LRU_lock);
    return NULL;
}