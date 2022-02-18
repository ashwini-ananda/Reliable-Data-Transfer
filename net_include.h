#include <stdio.h>

#include <stdlib.h>

#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h> 
#include <netdb.h>
#include <sys/time.h>

#include <errno.h>

#define MAX_MESS_LEN 1400
#define BUF_SIZE (MAX_MESS_LEN - (4 * sizeof(int) + sizeof(struct timeval)))



/* Only used in udp_server_hdr.c / udp_client_hdr.c to give an example of how
 * to include header data in our messages. Note that technically we should only
 * use fixed-width types in the header to ensure the header size is the same on
 * different architectures, but for assignments in this course, you don't need
 * to worry about portability across different architectures */
typedef struct dummy_uhdr {
    int seq;
    struct timeval ts;
} uhdr;

typedef struct packets{
    int seq_no;
    int ack_no;
    int size;
    struct timeval ts;
    int ack_flag;
    char data_buf[BUF_SIZE];
} packet;

