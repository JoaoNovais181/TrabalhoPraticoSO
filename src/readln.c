#include "readln.h"

#define MAX_READ_BUFFER 2048
static char read_buffer[MAX_READ_BUFFER];
static int read_buffer_pos = 0;
static int read_buffer_end = 0;

int readC (int fd, char *c)
{
    // 1 char de cada vez
    // return read(fd, c, 1);

    if (read_buffer_pos == read_buffer_end)
    {
        read_buffer_end = read(fd, read_buffer, MAX_READ_BUFFER);
        switch(read_buffer_end)
        {
            case -1:
                return -1;
                break;
            case 0:
                return 0;
                break;
            default:
                read_buffer_pos = 0;
        }
    }

    *c = read_buffer[read_buffer_pos++];

    return 1;
}


ssize_t readln(int fd, char *buf, size_t size)
{
    int i = 0;

    while ( i<size && readC(fd, &buf[i])>0)
        if (buf[i++] == '\n') return i;

    return i;
}