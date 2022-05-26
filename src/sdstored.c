#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include "readln.h"
#include "Pedido.h"
#include "Execute.h"

#define FREE(algo) if (algo) free(algo);
#define FIFO "clientToServer"
#define LOG "log.txt"
#define BUF_SIZE 1024
#define NUM_PRIORITIES 6

typedef struct queue
{
    Pedido pedido;
    pid_t pid;
    int numP;
	struct queue *prox, *last;
} *Queue, *PriorityQueue[NUM_PRIORITIES];


/* int fd; */
int readingEndFifo;
int writingEndFifo;
int numAtual;
static int receiving; 
static int created_processes;
static int maxOperations[7];
static int operationsInUse[7];
static PriorityQueue waitingList;
static Queue pedidos;
static char *transfDirectory;

/**
 * @brief Tokenizes string by using ' ' as a delimiter
 * 
 * @param command String to tokenize
 * @param N Poiter to integer that will be written with the number of tokens in the string (if not NULL)
 * @return List of tokens in string
 */
char **tokenize (char *command, int *N)
{
    if (!command) return NULL;
    int max = 20, i=0;
    char *string;
    char **exec_args = malloc(max * sizeof(char *));


    while ((string = strsep(&command, " \n"))!=NULL)
    {
        if (strlen(string)<1) continue;
        if (i>=max)
        {
            max += max*0.5;
            exec_args = realloc(exec_args, max * sizeof(char *));
        }
        exec_args[i++] = string;
    }

    exec_args[i] = NULL;
    if (N) *N = i;

    return exec_args;
}


/*   Funcoes de gestao de operacoes */

int isExecutable (int usage[7])
{
    for (int i=0 ; i<7 ; i++)
        if (operationsInUse[i] + usage[i] > maxOperations[i]) return 1;
    
    return 0;
}

int isValid (int usage[7])
{
	for (int i=0 ; i<7 ; i++)
		if (usage[i] > maxOperations[i]) return (i+1);
	return 0;
}

void useOperations (int usage[7])
{
    for (int i=0 ; i<7 ; i++) operationsInUse[i] += usage[i];
}

void freeOperations (int usage[7])
{
    for (int i=0 ; i<7 ; i++) operationsInUse[i] -= usage[i];
}


/*                        Funcoes da lista de pedidos a ser processados */

void printPedidos ()
{
	int isFirst = 1;
	Queue aux = pedidos;
	while (!(aux==pedidos && !isFirst))
	{
		char buffer[20];
		int l = sprintf(buffer, "task #%d: ", aux->numP);
		write(1, buffer, l);
		write(1, aux->pedido.line, strlen(aux->pedido.line));
		/* write(1, "\n", 1); */
		aux = aux->prox;	
	}
}


int isPedidosEmpty ()
{
    return (pedidos==NULL);
}

int removeDone ()
{
	if (!pedidos) return -1;

	int removed = 1;

	if (!(pedidos->last) && pedidos->pedido.status == Done)
	{
		freeOperations(pedidos->pedido.usage);
		FREE(pedidos);
		pedidos = NULL;
		removed = 0;
	}
	else
	{
		Queue aux = pedidos;
		while (aux && aux->prox && (aux->prox != pedidos))
		{
			if (aux->pedido.status == Done)
			{
				removed = 0;
				if (aux == pedidos)
				{
					aux->prox->last = aux->last;
					aux->last->prox = aux->prox;
					pedidos = aux->prox;
					freeOperations(aux->pedido.usage);
					FREE(aux);
				}
				else 
				{
					aux->prox->last = aux->last;
					aux->last->prox = aux->prox;
					freeOperations(aux->pedido.usage);
					FREE(aux);
				}
			}
		}

		if (aux->prox == aux->last)
		{
			if (aux->prox == aux && aux->pedido.status == Done)
			{
				removed = 0;
				pedidos = NULL;
				freeOperations(aux->pedido.usage);
				FREE(aux);
				return removed;
			}
			else if (aux->prox != aux && aux->pedido.status == Done)
			{
				removed = 0;
				freeOperations(aux->pedido.usage);
				aux->last->prox = NULL;
				aux->last->last = NULL;
				pedidos = aux->last;
				FREE(aux);
			}
		}
			
		if (aux->pedido.status == Done)
		{
			removed = 0;
			aux->prox->last = aux->last;
			aux->last->prox = aux->prox;
			freeOperations(aux->pedido.usage);
			FREE(aux);
		}
	}
	return removed;
}

int removePedido (pid_t pid)
{
    if (!pedidos) return -1;
    
    int f=1;

    if (!(pedidos->last) && pedidos->pid == pid)
    {
        freeOperations(pedidos->pedido.usage);
        FREE(pedidos);
        pedidos = NULL;
        f=0;
    }
    else if ((pedidos->last) == (pedidos->prox))
    {
        if (pedidos->pid == pid)
        {
            freeOperations(pedidos->pedido.usage);
            pedidos = pedidos->prox;
            // if (pedidos->last) free(pedidos->last);
            FREE(pedidos->last);
            pedidos->prox = NULL;
            pedidos->last = NULL;
            f=0;
        }
        else if (pedidos->prox->pid == pid)
        {
            freeOperations(pedidos->prox->pedido.usage);
            // if (pedidos->prox) free(pedidos->prox);
            FREE(pedidos->prox);
            pedidos->prox = NULL;
            pedidos->last = NULL;
            f=0;
        }
    }
    else
    {
        Queue aux = pedidos;
        while (aux && aux->pid != pid && aux->prox!=pedidos)
            aux = aux->prox;
        if (aux->pid == pid)
        {
            freeOperations(aux->pedido.usage);
            aux->last->prox = aux->prox;
	        aux->prox->last = aux->last;
            // if (aux) free(aux);
            if (aux == pedidos)
			{
				pedidos = pedidos->prox;
			}
			FREE(aux);
            f=0;
        }
    }
    return f;
}


int pushP (Pedido pedido, pid_t pid, int numP)
{
    Queue node = malloc(sizeof (struct queue));
    node->pedido = pedido;
    node->pid = pid;
	node->numP = numP;

    if (!pedidos)
    {
        pedidos = node;
        node->prox = NULL;
        node->last = NULL;
        return 0;
    }

    if (!(pedidos->prox))
    {
        pedidos->prox = node;
        pedidos->last = node;
        node->prox = pedidos;
        node->last = pedidos;
        return 0;
    }

    node->last = pedidos->last;
    node->last->prox = node;
    pedidos->last = node;
    node->prox = pedidos;
    return 0;
}

/*       Funcoes da lista de espera */

void printWaitingList ()
{
	char buffer[1024] = {0};
	for (int i=0 ; i<6 ; i++)
	{
		Queue q = waitingList[i];
		int verificouPrim = 0;
		while (q && !(q==waitingList[i] && verificouPrim))
		{
			if (q == waitingList[i]) verificouPrim=1;
			int l = sprintf(buffer, "task #%d: %s", waitingList[i]->numP, waitingList[i]->pedido.line);
			write(1, buffer, l);
			q = q->prox;
		}
	}
}

int isWaitingListEmpty ()
{
	for (int i=0 ; i<=5 ; i++)
		if (waitingList[i] != NULL) return 0; 
    return 1;
}

int pushFrontWL (Pedido pedido, int priority, pid_t pid, int numP)
{
	if (priority>5 || priority<0) return -1;
    Queue node = malloc(sizeof (struct queue));
    node->pedido = pedido;
    node->pid = pid;
	node->numP = numP;

    if (!waitingList[priority])
    {
        waitingList[priority] = node;
        node->prox = NULL;
        node->last = NULL;
        return 0;
    }
    if (!(waitingList[priority]->prox))
	{
		waitingList[priority]->prox = node;
		waitingList[priority]->last = node;
		node->prox = waitingList[priority];
		node->last = waitingList[priority];
		waitingList[priority] = node;
		return 0;
	}

    node->last = waitingList[priority]->last;
    node->last->prox = node;
    waitingList[priority]->last = node;
    node->prox = waitingList[priority];
	waitingList[priority] = node;
	return 0;
}

int pushWL (Pedido pedido, int priority, pid_t pid, int numP)
{
	if (priority>5 || priority<0) return -1;
    Queue node = malloc(sizeof (struct queue));
    node->pedido = pedido;
    node->pid = pid;
	node->numP = numP;

    if (!waitingList[priority])
    {
        waitingList[priority] = node;
        node->prox = NULL;
        node->last = NULL;
        return 0;
    }
    if (!(waitingList[priority]->prox))
    {
        waitingList[priority]->prox = node;
        waitingList[priority]->last = node;
        node->prox = waitingList[priority];
        node->last = waitingList[priority];
        return 0;
    }

    node->last = waitingList[priority]->last;
    node->last->prox = node;
    waitingList[priority]->last = node;
    node->prox = waitingList[priority];
    return 0;
}

int dequeueWL (Pedido *pedido, int *n)
{
	pid_t pid = -1;
	for (int i=5 ; i>=0 ; i--)
	{

		pid = -1;
		if (!waitingList[i])
			continue;

		Pedido auxP = waitingList[i]->pedido;
		int auxN = waitingList[i]->numP;
		pid = waitingList[i]->pid;
		if (!(waitingList[i]->last))
		{
			// if (waitingList) free(waitingList);
			FREE(waitingList[i]);
			waitingList[i] = NULL;
		}
		else if ((waitingList[i]->last) == (waitingList[i]->prox))
		{
			waitingList[i] = waitingList[i]->prox;
			// if (waitingList->last) free(waitingList->last);
			FREE(waitingList[i]->last);
			waitingList[i]->prox = NULL;
			waitingList[i]->last = NULL;
		}
		else
		{
			Queue aux = waitingList[i]->last;
			waitingList[i] = waitingList[i]->prox;
			//if (waitingList->last) free(waitingList->last);
			FREE(waitingList[i]->last);
			aux->prox = waitingList[i];
			waitingList[i]->last = aux;
		}

		if (isExecutable(auxP.usage))
		{
			pid_t aux = pid;
			pid = dequeueWL(pedido, n);
			pushFrontWL(auxP, i, aux, auxN);
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

void status ()
{
	// Print pedidos que estão a processar	
	if (!isPedidosEmpty())
	{
		write(1, "Taks being proccessed:\n", 23);
		printPedidos();
	}
	// Print pedidos que estao em lista de espera
	if (!isWaitingListEmpty())
	{
		write(1, "Tasks that are pending:\n", 24);
		printWaitingList();
	}
	
	// Print das instancias a correr
	char *str[64] = {"nop", "bcompress", "bdecompress", "gcompress", "gdecompress", "encrypt", "decrypt"};
	char buffer[128];
	for (int i=0 ; i<7 ; i++)
	{
		int l = sprintf(buffer, "transf %s: %d/%d (%s)\n", str[i], operationsInUse[i], maxOperations[i], transfDirectory);
		write(1, buffer, l);
	}
}

void recebeSinal (int signum)
{
}

void childSIGTERMHandler (int signum)
{
    kill(getppid(), SIGTERM);
}


int executePedido (Pedido pedido)
{
    int filedes[2];
    if (pipe(filedes)==-1)
    {
        perror("Error creating pipe");
        exit(errno);
    }

	int priority = 0, N=0;
	char *dup = strdup(pedido.line), **tokens = tokenize(dup, &N);

	if (!strcmp(tokens[0], "proc-file"))
	{
		int inicio = 1;

		pid_t ppid = pedido.pid;
		char fifoName[40] = {0};
		sprintf(fifoName,"client%d", ppid);
		int wef = open(fifoName, O_WRONLY);
		if (wef==-1)
		{
			perror("Error opening serverToClient fifo: ");
			exit(errno);
		}
		
		int valid = isValid(pedido.usage);

		if (valid)
		{
			char buffer[128];
			char *operations[32] = {"nop", "bcompress", "bdecompress", "gcompress", "gdecompress", "encrypt", "decrypt"};
			int l = sprintf(buffer, "Error: Task can't be executed: too many %s operations (max is %d, you have %d).\n", operations[valid-1], maxOperations[valid-1], pedido.usage[valid-1]);
			write(wef, buffer, l);
			return -1;
		}

		if (strstr("0 1 2 3 4 5", tokens[inicio]))
			priority = atoi(tokens[inicio++]);

		int use;
		pid_t pid = fork();
		switch (pid)
		{
			case -1:
				perror("Error on fork() call");
				return -1;
			case 0:
				signal(SIGCONT, recebeSinal);
				signal(SIGTERM, childSIGTERMHandler);

				close(filedes[0]);

				char buffer[100];
				int l;

				use = (!isExecutable(pedido.usage));
				write(filedes[1], &use, sizeof(int));
				close(filedes[1]);

				if (!use)
				{
					l = sprintf(buffer, "pending\n");
					write(wef, buffer, l);
					pause();
				}

				l = sprintf(buffer, "proccessing\n");
				write(wef, buffer, l);
				int entryBytes, outBytes;

				execute(&tokens[inicio], N-inicio, transfDirectory, &entryBytes, &outBytes);

				l = sprintf(buffer, "concluded (bytes-input: %d, bytes-output: %d)\n", entryBytes, outBytes);
				write(wef, buffer, l);

				sprintf(buffer, "free %d\n", pedido.pid);
				Pedido ped = criaPedido(buffer, Done);
				write(writingEndFifo, &ped, sizeof(struct pedido));

				close(wef);
				free(dup);
				free(tokens);

				_exit(0);
			default:
				close(wef);
				created_processes++;
				close(filedes[1]);

				if (read(filedes[0], &use, sizeof(int))<=0) 
				{
					perror("Error on resource management");
					exit(0);
				}

				close(filedes[0]);

				if (use)
				{
					useOperations(pedido.usage);
					pushP(pedido, pedido.pid, numAtual++);
				}
				else
					pushWL(pedido, priority, pid, numAtual++);
				break;
		}
	}

	free(dup);
	free(tokens);

    return 0;
}

int freePedido (Pedido pedido)
{
	pid_t pid;
	if (sscanf(pedido.line, "free %d\n", &pid)!=1)
	{
		perror("Erro a libertar recursos (ler pid)");
		exit(1);
	}

	if (removePedido(pid))
	{
		perror("Erro a libertar recursos (pedido nao encontrado)");
		exit(1);
	}
	
	if (!isWaitingListEmpty())
	{
		Pedido aux;
		int numP;
		pid = dequeueWL(&aux, &numP);
		while (pid!=-1)
		{
			if (kill(pid,  SIGCONT))
				perror("Error unpausing proccess");
			useOperations(aux.usage);
			pushP(aux, aux.pid, numP);
			pid = dequeueWL(&aux, &numP);
		}
	}

	return 0;
}

int parsePedido (Pedido pedido, int *exitflag)
{
    if (strstr(pedido.line, "free"))
	{
		return freePedido(pedido);
	}

    if (!receiving)
	{
        return -1;
	}

	if (!strncmp(pedido.line, "status", 6))
	{
		status();
		return 0;
	}

    return executePedido(pedido);
}

void esperarPorFilhos() {
    while (1) {
        int status;
        pid_t done = wait(&status);
        if (done == -1) {
            if (errno == ECHILD) break; //não tem mais filhos
        } else {
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                exit(1);
            }
        }
    }
}

void SIGTERMHandler2 (int signum) 
{
	receiving = 0;
}

void SIGTERMHandler (int signum)
{
    receiving = 0;
    signal(SIGTERM, SIGTERMHandler2);
    int r=0;
    Pedido pedido;

    while ( !( (isWaitingListEmpty()) && (isPedidosEmpty()) ) )
    {
        if ((r = read(readingEndFifo, &pedido, sizeof(struct pedido)))>0)
        {
            if (parsePedido(pedido, NULL)==-1)
            {
                char fifoName[20] = {0};
                sprintf(fifoName,"client%d", pedido.pid);
                int wef = open(fifoName, O_WRONLY);
                write(wef, "Error: programa nao esta a aceitar pedidos\n", 43);
            }
        }
    }


    /* close(fd); */
    close(readingEndFifo);

    if (unlink(FIFO)==-1)
    {
        perror("Error unlinking fifo");
        exit(errno);
    }

    esperarPorFilhos();

    exit(0);
}

void initializeGlobals (char *configFile)
{
	int fd;

	if ((fd = open(configFile, O_RDONLY))==-1)
	{
		perror("Error opening config file. ");
		exit(errno);
	}

	int r = 0;
	char buffer[64];
	while ((r=readln(fd, buffer, 64))>0)
	{
		if (!strncmp(buffer, "nop", 4)) sscanf(buffer, "nop %d\n", &maxOperations[nop]); //maxOperations[nop] =1;
		else if (!strncmp(buffer, "bcompress", 9)) sscanf(buffer, "nop %d\n", &maxOperations[bcompress]); //maxOperations[bcompress]+=1;
		else if (!strncmp(buffer, "bdecompress", 11)) sscanf(buffer, "nop %d\n", &maxOperations[bdecompress]); //maxOperations[bdecompress]+=1;
		else if (!strncmp(buffer, "gcompress", 9)) sscanf(buffer, "nop %d\n", &maxOperations[gcompress]); //maxOperations[gcompress]+=1;
		else if (!strncmp(buffer, "gdecompress", 11)) sscanf(buffer, "nop %d\n", &maxOperations[gdecompress]); //maxOperations[gdecompress]+=1;
		else if (!strncmp(buffer, "encrypt", 7)) sscanf(buffer, "nop %d\n", &maxOperations[encrypt]); //maxOperations[encrypt]+=1;
		else if (!strncmp(buffer, "decrypt", 7)) sscanf(buffer, "nop %d\n", &maxOperations[decrypt]); //maxOperations[decrypt]+=1;
	}

	close(fd);

	transfDirectory = calloc(1024, sizeof(char));

    created_processes=0;
    receiving = 1;
    for (int i=0 ; i<=5 ; i++) waitingList[i] = NULL;
    pedidos = NULL;
    maxOperations[nop]=3;
    maxOperations[bcompress]=4;
    maxOperations[bdecompress]=4;
    maxOperations[gcompress]=2;
    maxOperations[gdecompress]=2;
    maxOperations[encrypt]=2;
    maxOperations[decrypt]=2;
	for (int i=0; i<7 ; i++) operationsInUse[i] = 0;
}

int main (int argc, char *args[])
{
    /* char buffer[BUF_SIZE]; **/

	if (argc<3)
	{
		char buffer[] = "Usage: expected 2 arguments (config file and transformation directory)\n";
		write(1, buffer, strlen(buffer));
		exit(1);
	}



    initializeGlobals(args[1]);

	strcpy(transfDirectory, args[2]);

    signal(SIGTERM, SIGTERMHandler);
	/* signal(SIGCHLD, pedidoTermina2); */

    if (mkfifo(FIFO, 0640)==-1)
    {
        if (errno!=EEXIST)
        {
            perror("Error creating fifo");
            return 2;
        }
    }

    /* if ((fd = open(LOG, O_WRONLY | O_TRUNC | O_CREAT, 0640))==-1) */
    /* { */
        /* perror("Error opening log.txt"); */
        /* return 2; */
    /* } */

    Pedido pedido;

    readingEndFifo = open(FIFO, O_RDONLY);
	writingEndFifo = open(FIFO, O_WRONLY);
    int r=0, exitFlag = 0;

    while (!exitFlag)
    {
        while ((r = read(readingEndFifo, &pedido, sizeof(struct pedido)))>0)
        {
			parsePedido(pedido, &exitFlag);
        }
    }
    

    close(readingEndFifo);
    /* close(fd); */

    if (unlink(FIFO)==-1)
    {
        perror("Error unlinking fifo");
        exit(errno);
    }

    for (int i=0 ; i<created_processes ; i++)
        wait(NULL);

    return 0;
}
