#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include<time.h>
#include "net_include.h"

struct stat st;
struct timeval diff_time, startTime, endTime;

#define SIZE 1400
static void Usage(int argc, char *argv[]);
static void Print_help();
struct tm* timeInfo;
static int port;
int transferred=0;
char *filename;

int t_rcv(int sockfd){

  int n;
  FILE *fp;
  char buffer[SIZE];

  int bytes = 0;

  n = recv(sockfd, buffer, SIZE, 0);
    if (n <= 0){
      return 0;
    }
    //printf("Got file name: %s",buffer);
    
    
 
  filename = buffer;
  fp = fopen(filename, "w");
  gettimeofday(&startTime,NULL);
  printf("Transferring file...\n");
  
  while (1) {
    n = recv(sockfd, buffer, SIZE, 0);
    if (n <= 0){
      break;
    }
    fprintf(fp, "%s", buffer);
    bytes+=n;
    //printf("Transferred %ld",sizeof(buffer));
    bzero(buffer, SIZE);
  }
  fclose(fp);
  return bytes; 
}

int main(int argc, char *argv[]){
  //char *ip = "10.0.2.15";
  //int port = 8080;
  int e;
  Usage(argc,argv);

  int sockfd, new_sock;
  struct sockaddr_in server_addr, new_addr;
  socklen_t addr_size;
  char buffer[SIZE];

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0) {
    perror("[-]Error in socket");
    exit(1);
  }
  printf("[+]Server socket created successfully.\n");

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = port;
  server_addr.sin_addr.s_addr = INADDR_ANY;

  e = bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
  if(e < 0) {
    perror("[-]Error in bind");
    exit(1);
  }
  printf("[+]Binding successfull.\n");

  if(listen(sockfd, 10) == 0){
		printf("[+]Listening....\n");
	}else{
		perror("[-]Error in listening");
    exit(1);
	}

  addr_size = sizeof(new_addr);
  new_sock = accept(sockfd, (struct sockaddr*)&new_addr, &addr_size);
  int size = t_rcv(new_sock);
  gettimeofday(&endTime,NULL);
  timersub(&endTime, &startTime, &diff_time);
  printf("last msg received %lf seconds ago.\n\n",diff_time.tv_sec + (diff_time.tv_usec / 1000000.0));
  printf("[+]Data written in the file successfully.\n");
  printf("Amount of data transferred: %d\n", size);
  printf("Time taken to transfer %lf seconds\n", diff_time.tv_sec + (diff_time.tv_usec / 1000000.0));
  long int time_taken = diff_time.tv_sec * 1000000 + diff_time.tv_usec; 
  printf("Total Time: %ld sec, Filesize: %f MB, Transfer Rate: %lf bits/sec \n", diff_time.tv_sec, size / 1000000.0, (size * 8.0)/(time_taken));
  
  
  //printf("Rate of data transferred: %t", size);
  return 0;
}

/* Read commandline arguments */
static void Usage(int argc, char *argv[]) {
    if (argc != 2) {
        Print_help();
    }

    if (sscanf(argv[1], "%d", &port) != 1) {
        Print_help();
    }
}

static void Print_help() {
    printf("Usage: tcp_server <port>\n");
    exit(0);
}
