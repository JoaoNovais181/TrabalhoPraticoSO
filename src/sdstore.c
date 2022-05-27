#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include "Pedido.h"
#include "readln.h"

#define FIFO "clientToServer"
#define BUF_SIZE 1024

char fifoName[20];

/**
 * @brief Function to remove fifo file (Handler of SIGINT)
 * 
 * @param signum number of signal
 */
void removeFifoFile (int signum)
{
    printf("A remover fifo\n");
    if (unlink(fifoName)==-1)
    {
        perror("Error unlinking fifo");
    }
    exit(0);
}

/**
 * @brief Function to send task to server
 * 
 * @param argc number of arguments
 * @param args list of arguments
 * @return int 0 if everything went well
 */
int main (int argc, char *args[])
{
	if (argc == 1)
	{
		write(1, "./sdstore status\n", 17);
		write(1, "./sdstore proc-file priority input-filename output-filename transformation-id-1 transformation-id-2 ...\n", 104);
		return 0;
	}
    signal(SIGINT, removeFifoFile);


    int writingEndFifo = open(FIFO, O_WRONLY);  // open fifo for writing
    if (writingEndFifo == -1)
    {
        perror("Error opening clientToServer fifo!");
        exit(-1);
    }
    // if (argc>1)
    // {
    //     for (int i=1 ; i<argc ; i++)
    //         write(writingEndFifo, args[i], strlen(args[i]));
    // }
    
    sprintf(fifoName, "client%d", getpid());
    if (mkfifo(fifoName, 0640)==-1)
    {
		perror("Error creating FIFO");
        return -2;
    }

    char line[BUF_SIZE] = {0};
    int isExit = 0, i;

	Pedido pedido = criaPedido(line, Proccessing);

	if (argc == 2) // If there is 1 argument (./client argument)
	{
		if (strcmp("status", args[1])) // check if argument isn't "status" (only available command)
		{
			int l = sprintf(line, "Token %s not recognized!\n", args[1]);
			write(1, line, l);
			exit(-1);
		}
		strcpy(pedido.line, args[1]); 
		isExit = 1;
	}
	else if (argc > 4)
	{
		if (strcmp(args[1], "proc-file")) // check if first argument isn't "proc-file"
		{
			int l = sprintf(line, "Token %s not recognized!\n", args[0]);
			write(1, line, l);
			exit(-1);
		}
		int inicio = 4;
		if (strstr("0 1 2 3 4 5", args[2]))  // verify if there is priority
			inicio ++;
		
		for (int i=1 ; i<inicio ; i++)
		{
			if (strstr("nop bcompress bdecompress gcompress gdecompress encrypt decrypt", args[i]))
			{
				if (i==inicio-2)  // make sure that there is input file
					write(1, "Missing Argument: input-filename\n", 33);
				else              // make sure that there is output file
					write(1, "Missing Argument: output-filename\n", 34);
				return -1;
			}
			strcat(pedido.line, args[i]);
			strcat(pedido.line, " ");
		}
		
		for (i=inicio ; i<argc ; i++)
		{
			char *buf = args[i];
			if (!strcmp(buf, "nop")) pedido.usage[nop]+=1;
			else if (!strcmp(buf, "bcompress")) pedido.usage[bcompress]+=1;
			else if (!strcmp(buf, "bdecompress")) pedido.usage[bdecompress]+=1;
			else if (!strcmp(buf, "gcompress")) pedido.usage[gcompress]+=1;
			else if (!strcmp(buf, "gdecompress")) pedido.usage[gdecompress]+=1;
			else if (!strcmp(buf, "encrypt")) pedido.usage[encrypt]+=1;
			else if (!strcmp(buf, "decrypt")) pedido.usage[decrypt]+=1;
			else      // if any other token is found it isn't valid
			{
				int l = sprintf(line, "Token %s not recognized!\n", buf);
				write(1, line, l);
				exit(errno);
			}
			strcat(pedido.line, args[i]);
			strcat(pedido.line, " ");
		}
	}
	else
	{
		write(1, "Error: Invalid task\n", 20);
		return -1;
	}

	strcat(pedido.line, "\n");

    write(writingEndFifo, &pedido, sizeof(struct pedido));  // send task to server
    
    close(writingEndFifo);
    
    if (!isExit)
    {
        int readingEndFifo = open(fifoName, O_RDONLY);

		char *str = malloc(BUF_SIZE);
	
		while ((i=read(readingEndFifo, str, BUF_SIZE))>0)
        {
            write(1, str, i);
            if (strstr(str, "Error"))
                break;
        }
        close(readingEndFifo);
    }
    
    
    if (unlink(fifoName)==-1)
    {
        perror("Error unlinking fifo");
    }

    // if (remove(fifoName)!=0)
    // {
        // perror("Error deleting fifo");
        // return -1;
    // }


    return 0;
}
