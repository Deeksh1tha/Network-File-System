#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "structs.h"
int main()
{
    struct LRUcache *lruQueue=initLRUcache(10);
    ss_info *ss_array = malloc(sizeof(ss_info));
    //    ss_array->ip_addr="196.162.87.00";
    strcpy(ss_array->ip_addr, "192.126.205.80");
    ss_array->no_acc_paths = 4;
    ss_array->port_no_client = 5500;
    ss_array->port_no_ns = 5592;
    ss_array->socket_id = 12345;
    strcpy(ss_array->accesible_paths[0], "/home/desktop/SEM1/osn/courseproject");
    enqueue(lruQueue, ss_array->accesible_paths[0], ss_array);
    strcpy(ss_array->accesible_paths[1], "/home/desktop/SEM2/osn/courseproject");
   ss_info* ss_struct = getFromLRUcache(lruQueue, ss_array->accesible_paths[1]);
    if (ss_struct == NULL)
    {
        enqueue(lruQueue, ss_array->accesible_paths[1], ss_array);
    }
    ss_struct = getFromLRUcache(lruQueue, ss_array->accesible_paths[0]);
    if (ss_struct != NULL)
        printf("ip: %s\nsock_id: %d\n", ss_struct->ip_addr, ss_struct->socket_id);
}