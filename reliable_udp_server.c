#include "sendto_dbg.h"
#include <sys/stat.h>
#include <math.h>

static void Usage(int argc, char *argv[]);
static void Print_help();
static int Cmp_time(struct timeval t1, struct timeval t2);
static char *filename;
static int timeout_usec = 2;
static int WINDOW_SIZE = 200;
//static void createFile();

static void sendAck(packet *frame, packet *ack, int sock, struct sockaddr_in *from_addr, int ackflag);
static const struct timeval Zero_time = {0, 0};

static int Port;
static int loss_percent;
static char* env;
static struct timeval cur_start_time = {0,0};
static int cur_start = 0, cur_in_order = 0, cur_size = 0, transfer_so_far_size = 0;

int is_empty(packet buf, size_t size)
{
    packet zero;
    memset(&zero, 0, sizeof(packet));
    return !memcmp(&zero, &buf, sizeof(packet));
}

int check_eof(char *buf){
    //printf("inside check_eof\n");
    char zero[BUF_SIZE];
    memset(&zero, 0, BUF_SIZE);
    return !memcmp(&zero, buf, BUF_SIZE);
}

void print_summary(int f_size, long int time){
    printf("Total Time: %lf sec, Filesize: %lf MB, Transfer Rate: %lf Megabits/sec\n", time / 1000000.0, f_size / 1000000.0, (f_size * 8.0)/(time));
}


void rcv(struct sockaddr_in from_addr, int sock)
{
    int from_ip, wnd_idx, seq = 0;
    /*FILE            *f_ptr = fopen(filename, "rb");
    if(f_ptr == NULL){
        printf("Error in accessing File. Terminating");
        exit(1);
    }*/
    int wnd_size = WINDOW_SIZE;
    packet recv_wnd[wnd_size];
    fd_set mask, read_mask;
    struct timeval timeout, now, diff_time, start_time, end_time, last_recv_time = {0, 0};
    FD_ZERO(&read_mask);
    FD_SET(sock, &read_mask);
    packet frame, ack;
    
    int next_expected_seq = 0, last_segment_end_seq = -1;
    int init_flag = 1, end_flag = 0, cur_ip = INADDR_ANY;
    FILE *fp;
    sendto_dbg_init(loss_percent);
    int f_size = 0;
    int step = ceil(10000000.0 / BUF_SIZE);
    //printf("STep: %d\n", step);
    int print_timeout_flag = 1;
    
    
    while(1){
        memset(&recv_wnd, 0, wnd_size * sizeof(packet));
        int n_frames = 0;
        for (;;)
        {
            mask = read_mask;
            timeout.tv_sec = 0;
            timeout.tv_usec = timeout_usec;

            /* Wait for message or timeout */
            int num = select(FD_SETSIZE, &mask, NULL, NULL, &timeout);
            //printf("Last segment end seq: %d\n",last_segment_end_seq);
            //printf("Next Expected Sequence: %d\n", next_expected_seq);
            if (num > 0)
            {
                if (FD_ISSET(sock, &mask))
                {
                    int from_len = sizeof(from_addr);
                    memset(&frame, 0, sizeof(frame));
                    int bytes = recvfrom(sock, &frame, sizeof(frame), 0,  
                            (struct sockaddr *)&from_addr, 
                            &from_len);
                    //frame.data_buf[bytes] = '\0'; /* ensure string termination for nice printing to screen */
                    from_ip = from_addr.sin_addr.s_addr;

                    if(end_flag == 1 && fp == NULL){
                            fp = NULL;
                            end_flag =0;
                            init_flag = 1;
                            n_frames = 0;
                            next_expected_seq = 0;
                            last_segment_end_seq = -1;
                            memset(&recv_wnd, 0, wnd_size * sizeof(packet));
                            print_timeout_flag = 1;
                    }else if (cur_ip != from_ip && init_flag != 1){
                        memset(&ack, 0, sizeof(ack));
                        ack.ack_flag = -1;
                        gettimeofday(&ack.ts, NULL);
                        sendto_dbg(sock, &ack, sizeof(ack), 0, (struct sockaddr *)&from_addr,
                                                sizeof(from_addr));
                        continue;
                    }

                    /* Record time we received this msg */
                    gettimeofday(&last_recv_time, NULL);

                    if(init_flag == 1){
                        cur_ip = from_ip;
                        filename = frame.data_buf;
                        fp = fopen(filename, "wb");
                        /*
                        printf("Received from (%d.%d.%d.%d): Destination File  %s\n", 
                                            (htonl(from_ip) & 0xff000000)>>24,
                                            (htonl(from_ip) & 0x00ff0000)>>16,
                                            (htonl(from_ip) & 0x0000ff00)>>8,
                                            (htonl(from_ip) & 0x000000ff), filename);*/
                        memset(&ack, 0, sizeof(ack));
                        ack.ack_flag = 1;
                        ack.seq_no = frame.seq_no;
                        gettimeofday(&ack.ts, NULL);
                        sendto_dbg(sock, &ack, sizeof(ack), 0, (struct sockaddr *)&from_addr,
                                                sizeof(from_addr));
                        init_flag = 0;
                        memcpy(&start_time, &last_recv_time, sizeof(last_recv_time));
                        print_timeout_flag = 0;
                    }else if (frame.seq_no >= 0){
                        if(frame.seq_no == 0){
                            memcpy(&cur_start_time, &last_recv_time, sizeof(last_recv_time));
                        }
                        wnd_idx = frame.seq_no % wnd_size;

                        if (is_empty(recv_wnd[wnd_idx], sizeof(recv_wnd[wnd_idx])) && frame.seq_no <= last_segment_end_seq + wnd_size && frame.seq_no > last_segment_end_seq)
                        {
                            memcpy(&recv_wnd[wnd_idx], &frame, sizeof(frame));
                            //printf("%d\n", frame.size);
                            n_frames++;
                                /*    
                                    printf("Received from (%d.%d.%d.%d): Seq %d, Bytes: %ld\n", 
                                    (htonl(from_ip) & 0xff000000)>>24,
                                    (htonl(from_ip) & 0x00ff0000)>>16,
                                    (htonl(from_ip) & 0x0000ff00)>>8,
                                    (htonl(from_ip) & 0x000000ff), frame.seq_no, sizeof(frame.data_buf));
                                */

                            if(frame.seq_no == cur_in_order + 1){
                                cur_in_order += 1;
                                transfer_so_far_size += frame.size;
                                cur_size += frame.size;
                                if(cur_in_order == cur_start + step - 1){
                                    timersub(&last_recv_time, &cur_start_time, &diff_time);
                                    long int time_taken = diff_time.tv_sec * 1000000 + diff_time.tv_usec;
                                    printf("File transferred so far: %f MB, Transfer Rate: %lf Megabits/sec\n", transfer_so_far_size / 1000000.0, (cur_size * 8.0) / (time_taken));
                                    cur_start = cur_start + step;
                                    cur_size = 0;
                                    memcpy(&cur_start_time, &last_recv_time, sizeof(last_recv_time));
                                }
                            }
                                    
                                
                            if(check_eof(frame.data_buf)){
                                printf("EOF Reached\n");
                                end_flag = 1;
                                next_expected_seq = frame.seq_no + 1;
                                //printf("%s\n",recv_wnd[wnd_idx-1].data_buf);
                            }
                            else if (frame.seq_no >= next_expected_seq) 
                            {
                                for(int i = next_expected_seq; i < frame.seq_no; i++){
                                    memset(&ack, 0, sizeof(packet));
                                    ack.ack_flag = 0;
                                    ack.ack_no = i;
                                    gettimeofday(&ack.ts, NULL);
                                    /*
                                    printf("In main: Sending NACK to (%d.%d.%d.%d): for %d\n", 
                                        (htonl(from_ip) & 0xff000000)>>24,
                                        (htonl(from_ip) & 0x00ff0000)>>16,
                                        (htonl(from_ip) & 0x0000ff00)>>8,
                                        (htonl(from_ip) & 0x000000ff),
                                        ack.ack_no);
                                    */
                                    
                                    sendto_dbg(sock, &ack, sizeof(ack), 0, (struct sockaddr *)&from_addr,
                                        sizeof(from_addr));

                                }
                                next_expected_seq = frame.seq_no + 1;
                            }
                        }
                        if(n_frames == wnd_size){
                            /* write to file */

                            if(end_flag == 1 & fp != NULL){
                                timersub(&last_recv_time, &start_time, &diff_time);
                                for (int i = 0; i < n_frames - 1; i++){
                                    fwrite(recv_wnd[i].data_buf, 1, recv_wnd[i].size, fp);
                                    f_size += recv_wnd[i].size;
                                }
                                fclose(fp);
                                long int time_taken = diff_time.tv_sec * 1000000 + diff_time.tv_usec;
                                print_summary(f_size, time_taken);
                                fp = NULL;
                            }else{
                                for (int i = 0; i < n_frames; i++){
                                    fwrite(recv_wnd[i].data_buf, 1, recv_wnd[i].size, fp);
                                    f_size += recv_wnd[i].size;
                                }
                            }

                            /*send cumulative ack */
                            //printf("window saturated!!!\n");
                            memset(&ack, 0, sizeof(packet));
                            ack.ack_flag = 1;
                            ack.ack_no = recv_wnd[wnd_size - 1].seq_no;
                            gettimeofday(&ack.ts, NULL);
                            /*
                            printf("In main: Sending ACK to (%d.%d.%d.%d): for %d\n", 
                                (htonl(from_ip) & 0xff000000)>>24,
                                (htonl(from_ip) & 0x00ff0000)>>16,
                                (htonl(from_ip) & 0x0000ff00)>>8,
                                (htonl(from_ip) & 0x000000ff),
                                ack.ack_no);
                            */
                            
                            sendto_dbg(sock, &ack, sizeof(ack), 0, (struct sockaddr *)&from_addr,
                                sizeof(from_addr));
                            if(ack.ack_no == last_segment_end_seq + wnd_size){
                                last_segment_end_seq = ack.ack_no;
                                break;
                            }
                        }
                    }
                }
            }else{
                if(print_timeout_flag){
                    printf("timeout...nothing received for %d microseconds.\n", timeout_usec);
                    gettimeofday(&now, NULL);
                    if (Cmp_time(last_recv_time, Zero_time) > 0) {
                        timersub(&now, &last_recv_time, &diff_time);
                        printf("last msg received %lf seconds ago.\n\n",
                                diff_time.tv_sec + (diff_time.tv_usec / 1000000.0));
                    }
                }
                if(init_flag == 1){
                    continue;
                }
                wnd_idx = -1;
                int in_order_flag = 1;
                for (int i = last_segment_end_seq + 1; i < next_expected_seq; i++){
                    wnd_idx = i % wnd_size;
                    if(is_empty(recv_wnd[wnd_idx], sizeof(recv_wnd[wnd_idx])))
                    {
                        in_order_flag = 0;
                        memset(&ack, 0, sizeof(packet));
                        ack.ack_flag = 0;
                        ack.ack_no = i;
                        gettimeofday(&ack.ts, NULL);
                        /*
                        printf("In Timeout: Sending NACK to (%d.%d.%d.%d): for %d\n", 
                            (htonl(from_ip) & 0xff000000)>>24,
                            (htonl(from_ip) & 0x00ff0000)>>16,
                            (htonl(from_ip) & 0x0000ff00)>>8,
                            (htonl(from_ip) & 0x000000ff),
                            ack.ack_no);
                        */
                        
                        sendto_dbg(sock, &ack, sizeof(ack), 0, (struct sockaddr *)&from_addr,
                                sizeof(from_addr));
                    }
                }
                if (in_order_flag == 1){
                    if(end_flag == 1 && fp != NULL){
                        timersub(&last_recv_time, &start_time, &diff_time);
                        for (int i = 0; i < n_frames - 1; i++){
                            fwrite(recv_wnd[i].data_buf, 1, recv_wnd[i].size, fp);
                            f_size += recv_wnd[i].size;
                        }
                        fclose(fp);
                        long int time_taken = diff_time.tv_sec * 1000000 + diff_time.tv_usec;
                        print_summary(f_size, time_taken);
                        fp = NULL;
                    }
                    memset(&ack, 0, sizeof(packet));
                    ack.ack_flag = 1;
                    ack.ack_no = next_expected_seq - 1;
                    gettimeofday(&ack.ts, NULL);
                    /*
                    printf("In Timeout: Sending ACK to (%d.%d.%d.%d): for %d\n", 
                        (htonl(from_ip) & 0xff000000)>>24,
                        (htonl(from_ip) & 0x00ff0000)>>16,
                        (htonl(from_ip) & 0x0000ff00)>>8,
                        (htonl(from_ip) & 0x000000ff),
                        ack.ack_no);
                    */
                    
                    sendto_dbg(sock, &ack, sizeof(ack), 0, (struct sockaddr *)&from_addr,
                            sizeof(from_addr));
                    if(ack.ack_no == last_segment_end_seq + wnd_size){
                        last_segment_end_seq = ack.ack_no;
                        break;
                    }
                }
            }
        }
    }
    
    
}

int main(int argc, char *argv[])
{
    struct sockaddr_in name;
    struct sockaddr_in from_addr;
    socklen_t from_len;
    int from_ip;
    int sock;
    fd_set mask;
    fd_set read_mask;
    int bytes;
    int num;
    char mess_buf[MAX_MESS_LEN];
    struct timeval timeout;
    struct timeval last_recv_time = {0, 0};
    struct timeval now;
    struct timeval diff_time;
    uhdr *hdr = (uhdr *)mess_buf;
    char *data_buf = &mess_buf[sizeof(uhdr)];
    char *tmpname;
    FILE *fp;
    char c;
    int wnd_size = 50;

    packet receiveBuffer[wnd_size];
    int nxtExpectedSeqNum = 0;
    int wnd_idx = 0;

    /* Parse commandline args */
    Usage(argc, argv);
    //fp = fopen("temp.txt", "w")
    printf("Listening for messages on port %d\n", Port);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("udp_server: socket");
        exit(1);
    }

    /* Bind receive socket to listen for incoming messages on specified port */
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = INADDR_ANY;
    name.sin_port = htons(Port);

    if (bind(sock, (struct sockaddr *)&name, sizeof(name)) < 0)
    {
        perror("udp_server: bind");
        exit(1);
    }

    rcv(from_addr, sock);

    return 0;
}

/* Read commandline arguments */
static void Usage(int argc, char *argv[])
{
    if (argc != 4)
    {
        Print_help();
    }

    int i = 1;
    loss_percent = atoi(argv[i++]);
    Port = atoi(argv[i++]);
    env = argv[i++];
    if(strcmp(env, "LAN") == 0){
        WINDOW_SIZE = 200;
        timeout_usec = 805;
    }else if(strcmp(env, "WAN") == 0){
        WINDOW_SIZE = 200;
        timeout_usec = 40950;
    }
}

static void Print_help()
{
    printf("Usage: udp_server <loss_rate_percentage> <port> <env>\n");
    exit(0);
}



/* Returns 1 if t1 > t2, -1 if t1 < t2, 0 if equal */
static int Cmp_time(struct timeval t1, struct timeval t2) {
    if      (t1.tv_sec  > t2.tv_sec) return 1;
    else if (t1.tv_sec  < t2.tv_sec) return -1;
    else if (t1.tv_usec > t2.tv_usec) return 1;
    else if (t1.tv_usec < t2.tv_usec) return -1;
    else return 0;
}
