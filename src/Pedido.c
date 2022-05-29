#include "Pedido.h"

/**
 * @brief Function to create Pedido
 * 
 * @param line line to add to structure
 * @param status status of the task
 * @return Pedido Task with specified parameters
 */
Pedido criaPedido (char *line, Status status)
{
    Pedido pedido;
    pedido.pid = getpid();
	pedido.status = status;
	strcpy(pedido.line, line);

	for (int i=0 ; i<7 ; i++)pedido.usage[i]=0;
	
    return pedido;
}

/**
 * @brief Function to check if a certain task is executable, by verifying its usage
 * 
 * @param usage usage of the task which we want to check if it's executable
 * @return int 1 if it isn't executable, 0 if it is
 */
int isExecutable (int usage[7], int operationsInUse[7], int maxOperations[7])
{
    for (int i=0 ; i<7 ; i++)
        if (operationsInUse[i] + usage[i] > maxOperations[i]) return 1;
    
    return 0;
}

/**
 * @brief Function to check if a certain task is Valid (number of operations less than max operations)
 * 
 * @param usage usage of the task which we want to check if it's valid
 * @return int 0 if valid, 1 if not valid
 */
int isValid (int usage[7], int maxOperations[7])
{
	for (int i=0 ; i<7 ; i++)
		if (usage[i] > maxOperations[i]) return (i+1);
	return 0;
}

/**
 * @brief Function to update the operationsInUse of the server when a task is being proccessed
 * 
 * @param usage usage of the task that is proccessing
 */
void useOperations (int usage[7], int operationsInUse[7])
{
    for (int i=0 ; i<7 ; i++) operationsInUse[i] += usage[i];
}

/**
 * @brief Function to update the operationsInUse of the server when a task is done
 * 
 * @param usage usage of the task that is Done
 */
void freeOperations (int usage[7], int operationsInUse[7])
{
    for (int i=0 ; i<7 ; i++) operationsInUse[i] -= usage[i];
}
