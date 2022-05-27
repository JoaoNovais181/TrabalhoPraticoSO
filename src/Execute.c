#include "Execute.h"
#include <fcntl.h>
#include <unistd.h>

/**
 * @brief Executes the first command on piped command succession
 * 
 * @param filedes Pipe
 * @param command Command to execute
 * @param argc Number of arguments on command
 */
void filho0 (int fde, int *filedes, char *command)
{
    close(filedes[0]);

	dup2(fde, 0);
	close(fde);
    dup2(filedes[1], 1);
    close(filedes[1]);

	execl(command, command, NULL);
}

/**
 * @brief Executes the last command on piped command succession
 * 
 * @param filedes Pipe
 * @param command Command to execute
 * @param argc Number of arguments on comman:d
 */
void filhoN (int *filedes, int fds, char *command)
{
    close(filedes[1]);

    dup2(filedes[0], 0);
    close(filedes[0]);
	dup2(fds, 1);
	close(fds);

	execl(command, command, NULL);
}

/**
 * @brief Executes the command on the middle of the piped command succession
 * 
 * @param filedesE Entry pipe
 * @param filedesS Out pipe
 * @param command Command to execute
 * @param argc Number of arguments on command
 */
void filhoi (int *filedesE, int *filedesS, char *command)
{
    close(filedesE[1]);
    close(filedesS[0]);

    dup2(filedesE[0], 0);
    dup2(filedesS[1], 1);

    close(filedesE[0]);
    close(filedesS[1]);

	execl(command, command, NULL);
}

/**
 * @brief Executes the commands given as argument with pipes connecting their input/output
 * 
 * @param tokens List of commands to execute
 * @param N  Number of commands
 * @param entryBytes pointer to return the number of bytes read on the entry file
 * @param outBytes pointer to return the number of bytes written on the output file
 * @return 0 if everything went well
 */
int execute (char **tokens, int N, char *transfDirectory, int *entryBytes, int *outBytes)
{
    int nCommands = N-3, **filedes = malloc(nCommands*sizeof(int*));	


    pid_t pid;

	int fde, fds;
	if ((fde = open(tokens[0], O_RDONLY))==-1)
	{
		perror("Error opening input file");
		exit(errno);
	}

	if ((fds = open(tokens[1], O_TRUNC | O_WRONLY | O_CREAT, 0666))==-1)
	{
		perror("Error opening output file");
		exit(errno);
	}

	if (!nCommands)
	{
		pid = fork();
		if (pid==-1)
		{
			perror("Error on fork() call");
			exit(errno);
		}

		if (!pid)
		{
			char command[1024] = {0};
			strcat(command, transfDirectory);
			if (transfDirectory[strlen(transfDirectory)-1]!='/')  // Um path deve ser do tipo path/nomeFIcheiro, se faltar o / entre path e ficheiro é colocado
				strcat(command, "/");
			strcat(command, tokens[2]);
			strcat(command, "\0");
			dup2(fde, 0);
			dup2(fds, 1);
			execlp(command, command, NULL);
			_exit(errno);
		}

		else
			wait(NULL);

	}
	else 
	{
		for (int i=0 ; i<nCommands ; i++)
		{
			filedes[i] = malloc(2*sizeof(int));
			if (pipe(filedes[i])==-1)
			{
				perror("Error creating fifo");
				exit(errno);
			}
		}

		for (int i=0 ; i<nCommands+1 ; i++)
		{
			pid = fork();

			if (pid==-1)
			{
				perror("Error on fork() call");
				exit(errno);
			}

			if (!pid)
			{
				char command[1024] = {0};
				strcat(command, transfDirectory);
				if (transfDirectory[strlen(transfDirectory)-1]!='/') // Um path deve ser do tipo path/nomeFIcheiro, se faltar o / entre path e ficheiro é colocado
					strcat(command, "/");
				strcat(command, tokens[i+2]);
				strcat(command, "\0");
				if (i==0) 
					filho0(fde, filedes[i], command);
				else if (i==nCommands)
					filhoN(filedes[i-1], fds, command);
				else
					filhoi(filedes[i-1], filedes[i], command);
				_exit(errno);
			}
			
			/* free(tokens[i]); */

			if (!i)
				close(filedes[i][1]);    // como fica o extremo de escrita do filho aberto, é necessario fechar o extremo de escrita do pai
			else if (i==nCommands)
				close(filedes[i-1][0]);  // como fica o extremo de leitura do filho aberto, é necessario fechar o extremo de leitura do pai
			else
			{
				close(filedes[i-1][0]);  // como fica o extremo de leitura do filho aberto, é necessario fechar o extremo de leitura do pai
				close(filedes[i][1]);    // como fica o extremo de escrita do filho aberto, é necessario fechar o extremo de escrita do pai
			}
		}


		for (int i=0 ; i<nCommands+1 ; i++)
		{
			int status;
			pid = wait(&status);
			if (i<nCommands) free(filedes[i]);
		}
	}

	(*entryBytes) = lseek(fde, 0, SEEK_END);
	(*outBytes) = lseek(fds, 0, SEEK_END);

    if (filedes) free(filedes);
    return 0;
}

