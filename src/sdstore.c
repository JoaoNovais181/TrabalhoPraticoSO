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

void removeFifoFile (int signum)
{
    printf("A remover fifo\n");
    if (unlink(fifoName)==-1)
    {
        perror("Error unlinking fifo");
    }
    exit(0);
}

int main (int argc, char *args[])
{
	if (argc < 2)
	{
		write(1, "./sdstore status\n", 17);
		write(1, "./sdstore proc-file priority input-filename output-filename transformation-id-1 transformation-id-2 ...\n", 104);
		return 0;
	}
    signal(SIGINT, removeFifoFile);


    int writingEndFifo = open(FIFO, O_WRONLY);
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

	if (argc == 2)
	{
		if (!strstr("exit status", args[1]))
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
		if (strcmp(args[1], "proc-file"))
		{
			int l = sprintf(line, "Token %s not recognized!\n", args[0]);
			write(1, line, l);
			exit(errno);
		}
		int inicio = 4;
		if (strstr("0 1 2 3 4 5", args[inicio]))
			inicio ++;
		
		for (int i=1 ; i<inicio ; i++)
		{
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
			else 
			{
				int l = sprintf(line, "Token %s not recognized!\n", buf);
				write(1, line, l);
				exit(errno);
			}
			strcat(pedido.line, args[i]);
			strcat(pedido.line, " ");
		}
	}

	strcat(pedido.line, "\n");

    write(writingEndFifo, &pedido, sizeof(struct pedido));
    
    close(writingEndFifo);
    
	for (i=1 ; i<argc ; i++) if (!strcmp(args[i], "exit")) isExit=1;

	

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
