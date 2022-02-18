CC=gcc

CFLAGS = -g -c -Wall -pedantic

all: reliable_udp_server reliable_udp_client tcp_server tcp_client

reliable_udp_server:	reliable_udp_server.o sendto_dbg.o sendto_dbg.h
	    	    	$(CC) -o reliable_udp_server reliable_udp_server.o sendto_dbg.o

reliable_udp_client:	reliable_udp_client.o sendto_dbg.o sendto_dbg.h
	    		$(CC) -o reliable_udp_client reliable_udp_client.o sendto_dbg.o

tcp_server:	tcp_server.o
	   	$(CC) -o tcp_server tcp_server.o  

tcp_client:	tcp_client.o
	   	$(CC) -o tcp_client tcp_client.o


clean:
	rm *.o
	rm reliable_udp_server 
	rm reliable_udp_client
	rm tcp_server 
	rm tcp_client

%.o:    %.c
	$(CC) $(CFLAGS) $*.c


