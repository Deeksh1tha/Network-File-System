#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include"structs.h"


struct TrieNode *getNode(void)
{
    struct TrieNode *pNode = NULL;

    pNode = (struct TrieNode *)malloc(sizeof(struct TrieNode));

    if (pNode)
    {
        pNode->isEndOfWord = false;
        pNode->ss_ptr=NULL;
        for (int i = 0; i < ALPHABET_SIZE; i++)
            pNode->children[i] = NULL;
    }

    return pNode;
}

void insert(struct TrieNode *root, const char *key,ss_info* ptr)
{
    int level;
    int length = strlen(key);
    int index;

    struct TrieNode *pCrawl = root;

    for (level = 0; level < length; level++)
    {
        index = key[level];
        if (index == -1) {
            printf("Invalid character in path: %c\n", key[level]);
            return;
        }

        if (!pCrawl->children[index])
            pCrawl->children[index] = getNode();

        pCrawl = pCrawl->children[index];
    }

    // mark the last node as leaf
    pCrawl->isEndOfWord = true;
    pCrawl->ss_ptr=ptr;
}

int countCharacter(const char *str, char target) {
    int count = 0;

    while (*str != '\0') {
        if (*str == target) {
            count++;
        }
        str++;
    }

    return count;
}

ss_info* search(struct TrieNode *root, const char *key)
{
    int parent = 0;
    if(strchr(key, '.')){
        parent = 1;
    }
    int count = countCharacter(key, '/');

    int level;
    int length = strlen(key);
    int index;
    struct TrieNode *pCrawl = root;

    for (level = 0; level < length; level++)
    {
        index = key[level];
        if(index == '/'){
            if(count == 1 && parent)
                return pCrawl->children[index]->ss_ptr;
            count--;
        }

        if (index == -1 || !pCrawl->children[index])
            return NULL;

        pCrawl = pCrawl->children[index];
    }

    return pCrawl->ss_ptr;
}
