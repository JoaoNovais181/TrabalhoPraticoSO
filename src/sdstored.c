/**
 * @file sdstored.c 
 * @brief File that contains the implementation of the server SDSTORED
 */
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
} *Queue, *PriorityQueue[NUM_PRIORITIES];


/** @brief File Descriptor to the reding end of the clientToServer fifo */
int readingEndFifo;
 /** @brief File Descriptor to the writing end of the clientToServer fifo */ 
int writingEndFifo;
/** @brief int to store the number that is given to the next task that comes to the server */
int numAtual;        
static int receiving; 
static int created_processes;
static int maxOperations[7];
static int operationsInUse[7];
PriorityQueue waitingList;
Queue pedidos;
static char *transfDirectory;

/** 
 * @brief Function that prints s to stderr
 *
 * @param s String to print to stderr
 */
void myPerror (const char *s)
{
	write(2, s, strlen(s));
}

/** \fn char **tokenize (char *command, int *N)
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

/**
 * @brief Function to check if a certain task is executable, by verifying its usage
 * 
 * @param usage usage of the task which we want to check if it's executable
 * @return int 1 if it isn't executable, 0 if it is
 */
int isExecutable (int usage[7])
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
int isValid (int usage[7])
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
void useOperations (int usage[7])
{
    for (int i=0 ; i<7 ; i++) operationsInUse[i] += usage[i];
}

/**
 * @brief Function to update the operationsInUse of the server when a task is done
 * 
 * @param usage usage of the task that is Done
 */
void freeOperations (int usage[7])
{
    for (int i=0 ; i<7 ; i++) operationsInUse[i] -= usage[i];
}


/*                        Funcoes da lista de pedidos a ser processados */

/**
 * @brief Function to print every task in "pedidos"
 */
void printPedidos ()
{
	int isFirst = 1;
	Queue aux = pedidos;
	while (aux && !(aux==pedidos && !isFirst))
	{
		if (aux==pedidos)
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
int isPedidosEmpty ()
{
    return (pedidos==NULL);
}

void freePedidos ()
{
	if (!pedidos) return;
	if (pedidos->last) pedidos->last->prox = NULL;
	Queue aux = pedidos;
	pedidos = pedidos->prox;
	FREE(aux);
	freePedidos();
}

/**
 * @brief Function to remove a certain task from the proccessing list
 * 
 * @param pid pid of the task to remove from proccessing list
 * @return int 0 if found, not zero if not found
 */
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
            FREE(pedidos->last);
            pedidos->prox = NULL;
            pedidos->last = NULL;
            f=0;
        }
        else if (pedidos->prox->pid == pid)
        {
            freeOperations(pedidos->prox->pedido.usage);
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

/**
 * @brief Function to push task to the processing list
 * 
 * @param pedido task to proccess
 * @param pid proccess id of the proccess that is executing task
 * @param numP number of task
 * @return int 0 if everything went well
 */
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

/**
 * @brief Function to print every task in waitingList
 */
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
			int l = sprintf(buffer, "task #%d: %s", q->numP, q->pedido.line);
			write(1, buffer, l);
			q = q->prox;
		}
	}
}

/**
 * @brief Function to check if the Waiting List is empty
 * 
 * @return int 1 if empty, 0 if not empty
 */
int isWaitingListEmpty ()
{
	for (int i=0 ; i<=5 ; i++)
		if (waitingList[i] != NULL) return 0; 
    return 1;
}

void freeWaitingListP (int priority)
{
	if (!waitingList[priority]) return;
	if (waitingList[priority]->last) waitingList[priority]->last->prox = NULL;
	Queue aux = waitingList[priority];
	waitingList[priority] = waitingList[priority]->prox;
	FREE(aux);
	freeWaitingListP(priority);
}

void freeWaitingList ()
{
	for (int i=0 ; i<NUM_PRIORITIES ; freeWaitingListP(i++));
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

/**
 * @brief Function to take first element that is executable, in order of their priority
 * 
 * @param pedido Pointer to Pedido structure to store the task that is going to be proccessed
 * @param n Pointer to integer to store the number of the task
 * @return int 0 if everything went well
 */
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
			FREE(waitingList[i]);
			waitingList[i] = NULL;
		}
		else if ((waitingList[i]->last) == (waitingList[i]->prox))
		{
			waitingList[i] = waitingList[i]->prox;
			FREE(waitingList[i]->last);
			waitingList[i]->prox = NULL;
			waitingList[i]->last = NULL;
		}
		else
		{
			Queue aux = waitingList[i]->last;
			waitingList[i] = waitingList[i]->prox;
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


/**
 * @brief Function to ignore the SiGCONT signal on child proccesses
 * 
 * @param signum number of signal
 */
void recebeSinal (int signum)
{
}

/**
 * @brief Function to handle SIGTERM signal on child proccesses
 * 
 * If the child receives SIGTERM signal it sends the SIGTERM to father proccess
 * 
 * @param signum number of signal
 */
void childSIGTERMHandler (int signum)
{
    kill(getppid(), SIGTERM);
}

/**
 * @brief Function to print the information about the server
 * 
 * It prints every task that is being proccessed, every task that is pending and the usage of operations
 */
void status (Pedido pedido)
{
	char fifoName[40] = {0};
	sprintf(fifoName, "client%d", pedido.pid);

	int wef = open(fifoName, O_WRONLY);

	if (wef==-1)
	{
		perror("Error opening serverToClient fifo");
		exit(-1);
	}

	int fdSTDOUT = dup(1);
	dup2(wef, 1);
	close(wef);

	// Print pedidos que estão a processar	
	if (!isPedidosEmpty())
	{
		write(1, "Taks being proccessed:\n", 23);
		printPedidos();
	}
	// Print pedidos que estao em lista de espera
	/* if (!isWaitingListEmpty()) */
	/* { */
		/* write(1, "Tasks that are pending:\n", 24); */
		/* printWaitingList(); */
	/* } */
	
	// Print das instancias a correr
	char *str[64] = {"nop", "bcompress", "bdecompress", "gcompress", "gdecompress", "encrypt", "decrypt"};
	char buffer[128];
	for (int i=0 ; i<7 ; i++)
	{
		int l = sprintf(buffer, "transf %s: %d/%d (%s)\n", str[i], operationsInUse[i], maxOperations[i], transfDirectory);
		write(1, buffer, l);
	}

	dup2(fdSTDOUT, 1);
	close(fdSTDOUT);
}

/**
 * @brief Function to proccess a certain task
 * 
 * Firstly the line that contains the task is tokenized ( tokenize(pedido.line) )
 * 
 * Then we check if the first token is equal to "proc-file", if it isn't the task isn't valid nothing is done
 * 
 * Then the integrity of the task is checked to see if everything else is valid.
 * 
 * It is checked wheter the task has any specified priority or not (in case the task needs to be pending), and then we have the proccessing of the task
 * 
 * A child proccess is created to proccess the task. First it is checked if the task can be executed. If it can be executed, the child uses a pipe to communicate to the father that the process
 * is going to be executed or not, and the father adds it to the corresponing list.
 * 
 * Then the task is executed and everything that needs to be sent to the client is sent
 * 
 * @param pedido task to be proccessed
 * @return int 
 */
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
		
		int valid = isValid(pedido.usage);

		if (valid)
		{
			char buffer[128];
			int wef = open(fifoName, O_WRONLY);
			char *operations[32] = {"nop", "bcompress", "bdecompress", "gcompress", "gdecompress", "encrypt", "decrypt"};
			int l = sprintf(buffer, "Error: Task can't be executed: too many %s operations (max is %d, you have %d).\n", operations[valid-1], maxOperations[valid-1], pedido.usage[valid-1]);
			write(wef, buffer, l);
			close(wef);
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
				
				int wef = open(fifoName,O_WRONLY);
				if (wef==-1)
				{
					perror("Error opening serverToClient fifo: ");
					exit(errno);
				}
				freePedidos();
				freeWaitingList();

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


				sprintf(buffer, "free %d\n", getpid());
				Pedido ped = criaPedido(buffer, Done);
				write(writingEndFifo, &ped, sizeof(struct pedido));
				
				close(wef);
				free(dup);
				free(tokens);
				
				_exit(0);
			default:
				created_processes++;
				close(filedes[1]);

				if (read(filedes[0], &use, sizeof(int))<0) 
				{
					myPerror("Error on resource management");
					printf("pid:%d task:%s\n", pedido.pid, pedido.line);
					exit(0);
				}

				close(filedes[0]);

				if (use)
				{
					useOperations(pedido.usage);
					pushP(pedido, pid, numAtual++);
				}
				else
					pushWL(pedido, priority, pid, numAtual++);
				break;
		}
	}
	else
	{
		return -1;
	}

	free(dup);
	free(tokens);

    return 0;
}

/**
 * @brief Function to unpause pending tasks, called when a task is concluded
 *
 * This function unpauses the proccess that is proccessing every task that can be executed now
 */
void swapProccesses ()
{
	pid_t pid;
	Pedido aux;
	int numP;
	pid = dequeueWL(&aux, &numP);
	while (pid!=-1)
	{
		if (kill(pid,  SIGCONT))
		{
			perror("Error unpausing proccess");
			exit(errno);
		}
		pushP(aux, pid, numP);
		useOperations(aux.usage);
		pid = dequeueWL(&aux, &numP);
	}
}

/**
 * @brief Function that unpauses tasks if there is none being proccessed and the waitingList is with something
 *
 * @return 0 if everything went well
 */
int escalateProccesses ()
{
	if (!isWaitingListEmpty() && isPedidosEmpty())
		swapProccesses();
	return 0;
}

/**
 * @brief Function to send message to server saying that a task is done and it's usage should be freed
 * 
 * @param pedido Task containing the task saying which task should be freed
 * @return int 0 if everything went well
 */
int freePedido (Pedido pedido)
{
	pid_t pid;
	if (sscanf(pedido.line, "free %d\n", &pid)!=1)
	{
		myPerror("Erro a libertar recursos (ler pid)");
		exit(1);
	}

	if (removePedido(pid))
	{
		char buffer[128];
		sprintf(buffer, "Erro a libertar recursos (pedido #%d nao encontrado)", pid);
		myPerror(buffer);
		myPerror(pedido.line);
		exit(1);
	}
	else
	{
		waitpid(pid, NULL, 0);
	}
	
	if (!isWaitingListEmpty())
		swapProccesses();

	return 0;
}

/**
 * @brief Function to parse the task that a client sent to the server
 * 
 * 	First it is checked if it is a free task, because it is the only message that can be executed if server is not receiving tasks
 * 
 *  Then, if the server is receiving tasks, it is checked if the task is status, if it is then status is printed, otherwise we send the task to be executed
 * 
 * @param pedido Task to be parsed
 * @return int 0 if everything went well
 */
int parsePedido (Pedido pedido)
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
		status(pedido);
		return 0;
	}

    return executePedido(pedido);
}

/**
 * @brief Function to wait for every child of the parent proccess
 */
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

/**
 * @brief Handler of SIGTERM if server is closed to new tasks
 * 
 * @param signum number of signal
 */
void SIGTERMHandler2 (int signum) 
{
	receiving = 0;
}

/**
 * @brief Handler of SIGTERM if the server is opened and receives SIGTERM signal
 * 
 * @param signum number of signal
 */
void SIGTERMHandler (int signum)
{
    receiving = 0;
    signal(SIGTERM, SIG_IGN);
    int r=0;
    Pedido pedido;

    while ( !( (isWaitingListEmpty()) && (isPedidosEmpty()) ) )
    {
		/* printf("waitingList\n"); */
		/* printWaitingList(); */
		/* printf("pedidos\n"); */
		/* printPedidos(); */
        if ((r = read(readingEndFifo, &pedido, sizeof(struct pedido)))>0)
        {
            if (parsePedido(pedido)==-1)
            {
                char fifoName[20] = {0};
                sprintf(fifoName,"client%d", pedido.pid);
                int wef = open(fifoName, O_WRONLY);
                write(wef, "Error: programa nao esta a aceitar pedidos\n", 43);
            }
        }

		if (!isWaitingListEmpty() && isPedidosEmpty())
			swapProccesses();
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

/**
 * @brief Function to initialize global variables of the server
 * 
 * @param configFile Path to configFile to configure the server
 */
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
		if (!strncmp(buffer, "nop", 3)) sscanf(buffer, "nop %d\n", &maxOperations[nop]); 
		else if (!strncmp(buffer, "bcompress", 9)) sscanf(buffer, "bcompress %d\n", &maxOperations[bcompress]);
		else if (!strncmp(buffer, "bdecompress", 11)) sscanf(buffer, "bdecompress %d\n", &maxOperations[bdecompress]); //maxOperations[bdecompress]+=1;
		else if (!strncmp(buffer, "gcompress", 9)) sscanf(buffer, "gcompress %d\n", &maxOperations[gcompress]); //maxOperations[gcompress]+=1;
		else if (!strncmp(buffer, "gdecompress", 11)) sscanf(buffer, "gdecompress %d\n", &maxOperations[gdecompress]); //maxOperations[gdecompress]+=1;
		else if (!strncmp(buffer, "encrypt", 7)) sscanf(buffer, "encrypt %d\n", &maxOperations[encrypt]); //maxOperations[encrypt]+=1;
		else if (!strncmp(buffer, "decrypt", 7)) sscanf(buffer, "decrypt %d\n", &maxOperations[decrypt]); //maxOperations[decrypt]+=1;
	}

	close(fd);

	transfDirectory = calloc(1024, sizeof(char));

    created_processes=0;
    receiving = 1;
	numAtual = 1;
    for (int i=0 ; i<=5 ; i++) waitingList[i] = NULL;
    pedidos = NULL;
	for (int i=0; i<7 ; i++) operationsInUse[i] = 0;
}

void alarmHandler (int signum)
{
	printf("alarm\n");
	pid_t pid = waitpid(-1, NULL, 1);
	while (pid!=-1)
	{
		removePedido(pid);
		pid = waitpid(-1, NULL, 1);
	}
	swapProccesses();
}

/**
 * @brief Function to start the server
 * 
 * @param argc number of arguments
 * @param args argument list
 * @return int 0 if everything went well
 */
int main (int argc, char *args[])
{
	if (argc<3)
	{
		char buffer[] = "Usage: expected 2 arguments (config file and transformation directory)\n";
		write(1, buffer, strlen(buffer));
		exit(1);
	}



    initializeGlobals(args[1]);

	strcpy(transfDirectory, args[2]);

    signal(SIGTERM, SIGTERMHandler);
	signal(SIGALRM, alarmHandler);

    if (mkfifo(FIFO, 0640)==-1)
    {
        if (errno!=EEXIST)
        {
            perror("Error creating fifo");
            return 2;
        }
    }

    Pedido pedido;

    readingEndFifo = open(FIFO, O_RDONLY);
	writingEndFifo = open(FIFO, O_WRONLY);
    int r=0;

	while (!escalateProccesses() && (r = read(readingEndFifo, &pedido, sizeof(struct pedido)))>0)
	{
		alarm(10);
		parsePedido(pedido);
	}
    

    close(readingEndFifo);

    if (unlink(FIFO)==-1)
    {
        perror("Error unlinking fifo");
        exit(errno);
    }

    for (int i=0 ; i<created_processes ; i++)
        wait(NULL);

    return 0;
}
