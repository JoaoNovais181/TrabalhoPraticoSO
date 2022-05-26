#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int readC (int fd, char *c);
ssize_t readln(int fd, char *buf, size_t size);