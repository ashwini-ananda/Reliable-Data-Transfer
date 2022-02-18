#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "net_include.h"
struct timeval diff_time, startTime, endTime;
struct stat st;

#define SIZE 1400

static char *ip;
static int port;

static void Usage(int argc, char *argv[]);
static void Print_help();

void t_ncp(FILE *fp, char* destFile, int sockfd){
	
  int n;
  char data[SIZE] = {0};
  
  if (send(sockfd, destFile, sizeof(data), 0) == -1) {
      perror("[-]Error in sending file name.");
      exit(1);
    }
   
  printf("Starting file transfer...");
  gettimeofday(&startTime,NULL);
  while(fgets(data, SIZE, fp) != NULL) {
    if (send(sockfd, data, sizeof(data), 0) == -1) {
      perror("[-]Error in sending file.");
      exit(1);
    }
    bzero(data, SIZE);
  }
}

int main(int argc, char *argv[]){
  //char *ip = "10.0.2.15";
  //int port = 8080;
  int e;

  Usage(argc,argv);
  int sockfd;
  struct sockaddr_in server_addr;
  FILE *fp;
  char *filename = "test_file.txt";

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0) {
    perror("[-]Error in socket");
    exit(1);
  }
  printf("[+]Server socket created successfully.\n");

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = port;
  server_addr.sin_addr.s_addr = inet_addr(ip);

  e = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
  if(e == -1) {
    perror("[-]Error in socket");
    exit(1);
  }
	printf("[+]Connected to Server.\n");

  fp = fopen(filename, "r");
  if (fp == NULL) {
    perror("[-]Error in reading file.");
    exit(1);
  }
  //char destFile = argv[2];
  t_ncp(fp,argv[3],sockfd);
  gettimeofday(&endTime,NULL);
  timersub(&endTime, &startTime, &diff_time);
  
  printf("[+]File data sent successfully.\n");
  stat(filename, &st);
  int size = st.st_size;
  printf("Amount of data transferred: %d\n", size);
  printf("Time taken to transfer %lf seconds\n", diff_time.tv_sec + (diff_time.tv_usec / 1000000.0));
  long int time_taken = diff_time.tv_sec * 1000000 + diff_time.tv_usec; 
  printf("Total Time: %ld sec, Filesize: %f MB, Transfer Rate: %lf bits/sec \n", diff_time.tv_sec, size / 1000000.0, (size * 8.0)/(time_taken));
  
  
  
	printf("[+]Closing the connection.\n");
  close(sockfd);

  return 0;
}

/* Read commandline arguments */
static void Usage(int argc, char *argv[]) {
    if (argc != 4) {
        Print_help();
    }

    ip = strtok(argv[1], ":");
    if (ip == NULL) {
        printf("Error: no server IP provided\n");
        Print_help();
    }   
    port = atoi(strtok(NULL, ":"));
}

static void Print_help() {
    printf("Usage: udp_client <server_ip>:<port> <src_file> <dest file>\n");
    exit(0);
}
