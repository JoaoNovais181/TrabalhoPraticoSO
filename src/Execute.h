#ifndef EXECUTE
#define EXECUTE

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>

int execute (char **tokens, int N, char *transfDirectory, int *entryBytes, int *outBytes);

#endif
