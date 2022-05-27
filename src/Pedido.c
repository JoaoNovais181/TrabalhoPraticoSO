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
