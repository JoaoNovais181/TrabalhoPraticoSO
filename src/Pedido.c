#include "Pedido.h"


Pedido criaPedido (char *line, Status status)
{
    Pedido pedido;
    pedido.pid = getpid();
	pedido.status = status;
	strcpy(pedido.line, line);

	for (int i=0 ; i<7 ; i++)pedido.usage[i]=0;
	
    return pedido;
}
