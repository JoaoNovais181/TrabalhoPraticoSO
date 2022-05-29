#ifndef PEDIDO

#define PEDIDO
#include <unistd.h>
#include <string.h>

#define BUF_MAX 256

typedef enum operacao {nop=0, bcompress=1, bdecompress=2, gcompress=3, gdecompress=4, encrypt=5, decrypt=6} Operacao;

typedef enum status { Pending=0, Proccessing=1, Done=2} Status;


typedef struct pedido
{
    pid_t pid;
	Status status;
	char line[BUF_MAX];
    int usage[7];
} Pedido, *PPedido;

Pedido criaPedido (char *line, Status status);
int isExecutable (int usage[7], int operationsInUse[7], int maxOperations[7]);
int isValid (int usage[7], int maxOperations[7]);
void useOperations (int usage[7], int operationsInUse[7]);
void freeOperations (int usage[7], int operationsInUse[7]);

#endif
