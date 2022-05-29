#ifndef QUEUE
#define QUEUE

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "Pedido.h"
/**
 * @typedef Queue 
 *  Definition of the data type Queue, used to store the tasks that are being proccessed, implemented with double linked lists
 *
 * @typedef PriorityQueue
 *	Definition of the data type PriorityQueue, used to implement priorities in our project, implemented with a 6 position array of double linked lists
 *
 * @struct queue
 *  data structure used as "blueprint" to the definition of Queue and PriorityQueue
 * */
typedef struct queue
{
    Pedido pedido;
    pid_t pid;
    int numP;
	struct queue *prox, *last;
} *Queue, **PriorityQueue;


void printQueue (Queue queue);
int isEmpty (Queue queue);
void freeQueue (Queue queue);
int removePedido (Queue *queue, pid_t pid, PPedido pedido);
int enqueue (Queue *queue, Pedido pedido, pid_t pid, int numP);
int enqueueFront (Queue *queue, Pedido pedido, pid_t pid, int numP);
int isPriorityQueueEmpty (PriorityQueue priorityQueue, int NUM_PRIORITIES);
void freePriorityQueue (PriorityQueue priorityQueue, int NUM_PRIORITIES);
int dequeuePriorityQueue (PriorityQueue priorityQueue, int NUM_PRIORITIES, Pedido *pedido, int *n, int operationsInUse[7], int maxOperations[7]);

#endif
