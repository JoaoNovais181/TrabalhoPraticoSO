#include "Queue.h"
#include "Pedido.h"

#define FREE(algo) if (algo) free(algo);

/**
 * @brief Function to print every task in "pedidos"
 */
void printQueue (Queue queue)
{
	int isFirst = 1;
	Queue aux = queue;
	while (aux && !(aux==queue && !isFirst))
	{
		if (aux==queue)
			isFirst = 0;
		char buffer[20];
		int l = sprintf(buffer, "task #%d: ", aux->pid);
		write(1, buffer, l);
		write(1, aux->pedido.line, strlen(aux->pedido.line));
		aux = aux->prox;	
	}
}

/**
 * @brief Function to check if "pedidos" is empty
 * 
 * @return int 1 if empty, 0 if not empty
 */
int isEmpty (Queue queue)
{
    return (queue==NULL);
}

void freeQueue (Queue queue)
{
	if (!queue) return;
	if (queue->last) queue->last->prox = NULL;
	Queue aux = queue;
	queue = queue->prox;
	FREE(aux);
	freeQueue(queue);
}

/**
 * @brief Function to remove a certain task from the proccessing list
 * 
 * @param pid pid of the task to remove from proccessing list
 * @return int 0 if found, not zero if not found
 */
int removePedido (Queue *queue, pid_t pid, PPedido pedido)
{
	/* Queue pedidos = *queue; */
    if (!(*queue)) return -1;
    
    int f=1;

    if (!((*queue)->last) && (*queue)->pid == pid)
    {
        /* freeOperations(pedidos->pedido.usage); */
		(*pedido) = (*queue)->pedido;
		FREE((*queue));
		(*queue) = NULL;
        f=0;
    }
    else if (((*queue)->last) == ((*queue)->prox))
    {
        if ((*queue)->pid == pid)
        {
            /* freeOperations(pedidos->pedido.usage); */
			(*pedido) = (*queue)->pedido;
            (*queue) = (*queue)->prox;
            FREE((*queue)->last);
            (*queue)->prox = NULL;
            (*queue)->last = NULL;
            f=0;
        }
        else if ((*queue)->prox->pid == pid)
        {
            /* freeOperations((*queue)->prox->pedido.usage); */
			(*pedido) = (*queue)->prox->pedido;
            FREE((*queue)->prox);
            (*queue)->prox = NULL;
            (*queue)->last = NULL;
            f=0;
        }
    }
    else
    {
        Queue aux = (*queue);
        while (aux && aux->pid != pid && aux->prox!=(*queue))
            aux = aux->prox;
        if (aux->pid == pid)
        {
            /* freeOperations(aux->pedido.usage); */
			(*pedido) = aux->pedido;
            aux->last->prox = aux->prox;
			aux->prox->last = aux->last;
            if (aux == (*queue))
			{
				(*queue) = (*queue)->prox;
			}
			FREE(aux);
            f=0;
        }
    }
    return f;
}

/**
 * @brief Function to push task to the processing list
 * 
 * @param pedido task to proccess
 * @param pid proccess id of the proccess that is executing task
 * @param numP number of task
 * @return int 0 if everything went well
 */
int enqueue (Queue *queue, Pedido pedido, pid_t pid, int numP)
{
    Queue node = malloc(sizeof (struct queue));
    node->pedido = pedido;
    node->pid = pid;
	node->numP = numP;

    if (!(*queue))
    {
        (*queue) = node;
        node->prox = NULL;
        node->last = NULL;
        return 0;
    }

    if (!((*queue)->prox))
    {
        (*queue)->prox = node;
        (*queue)->last = node;
        node->prox = (*queue);
        node->last = (*queue);
        return 0;
    }

    node->last = (*queue)->last;
    node->last->prox = node;
    (*queue)->last = node;
    node->prox = (*queue);
    return 0;
}


/**
 * @brief Function to push task to the front of the Waiting List
 * 
 * @param pedido task to be pushed front
 * @param priority priority of the task
 * @param pid proccess id of the proccess that is going to proccess the task
 * @param numP number of the task
 * @return int 0 if everything went well
 */
int enqueueFront (Queue *queue, Pedido pedido, pid_t pid, int numP)
{
    Queue node = malloc(sizeof (struct queue));
    node->pedido = pedido;
    node->pid = pid;
	node->numP = numP;

    if (!(*queue))
    {
        (*queue) = node;
        node->prox = NULL;
        node->last = NULL;
        return 0;
    }
    if (!((*queue)->prox))
	{
		(*queue)->prox = node;
		(*queue)->last = node;
		node->prox = (*queue);
		node->last = (*queue);
		(*queue) = node;
		return 0;
	}

    node->last = (*queue)->last;
    node->last->prox = node;
    (*queue)->last = node;
    node->prox = (*queue);
	(*queue) = node;
	return 0;
}


/**
 * @brief Function to check if the Waiting List is empty
 * 
 * @return int 1 if empty, 0 if not empty
 */
int isPriorityQueueEmpty (PriorityQueue priorityQueue, int NUM_PRIORITIES)
{
	for (int i=0 ; i<=NUM_PRIORITIES ; i++)
		if (priorityQueue[i] != NULL) return 0; 
    return 1;
}

void freePriorityQueue (PriorityQueue priorityQueue, int NUM_PRIORITIES)
{
	for (int i=0 ; i<NUM_PRIORITIES ; freeQueue(priorityQueue[i++]));
}
/**
 * @brief Function to take first element that is executable, in order of their priority
 * 
 * @param pedido Pointer to Pedido structure to store the task that is going to be proccessed
 * @param n Pointer to integer to store the number of the task
 * @return int 0 if everything went well
 */
int dequeuePriorityQueue (PriorityQueue priorityQueue, int NUM_PRIORITIES, Pedido *pedido, int *n, int operationsInUse[7], int maxOperations[7])
{
	pid_t pid = -1;
	for (int i=NUM_PRIORITIES-1 ; i>=0 ; i--)
	{

		pid = -1;
		if (!priorityQueue[i])
			continue;

		Pedido auxP = priorityQueue[i]->pedido;
		int auxN = priorityQueue[i]->numP;
		pid = priorityQueue[i]->pid;
		if (!(priorityQueue[i]->last))
		{
			FREE(priorityQueue[i]);
			priorityQueue[i] = NULL;
		}
		else if ((priorityQueue[i]->last) == (priorityQueue[i]->prox))
		{
			priorityQueue[i] = priorityQueue[i]->prox;
			FREE(priorityQueue[i]->last);
			priorityQueue[i]->prox = NULL;
			priorityQueue[i]->last = NULL;
		}
		else
		{
			Queue aux = priorityQueue[i]->last;
			priorityQueue[i] = priorityQueue[i]->prox;
			FREE(priorityQueue[i]->last);
			aux->prox = priorityQueue[i];
			priorityQueue[i]->last = aux;
		}

		if (isExecutable(auxP.usage, operationsInUse, maxOperations))
		{
			pid_t aux = pid;
			pid = dequeuePriorityQueue(priorityQueue, NUM_PRIORITIES, pedido, n, operationsInUse, maxOperations);
			enqueueFront(&priorityQueue[i], auxP, aux, auxN);
		}
		else
		{
			(*pedido) = auxP;
			if (n) (*n) = auxN;
		}

		if (pid!=-1) return pid;
	}
	
	return pid;
}
