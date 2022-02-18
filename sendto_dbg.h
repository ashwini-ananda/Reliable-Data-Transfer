#ifndef CS2520_DBG
#define CS2520_DBG

#include <sys/types.h>
#include <sys/socket.h> 
#include "net_include.h"

int sendto_dbg(int s, packet *buf, int len, int flags, 
               const struct sockaddr *to, int tolen);

void sendto_dbg_init(int percent);

#endif 

