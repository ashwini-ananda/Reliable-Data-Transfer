#include "sendto_dbg.h"
#include <sys/stat.h>
#include <unistd.h> 
#include <math.h>


static void Usage(int argc, char *argv[]);
static void Print_help();

static char *Server_IP;
static int Port;
static char *src_filename;
static char *dest_filename;
static char *env;
static int WINDOW_SIZE = 50;
static char *eof = "EOF";
static int loss_percent;
static const struct timeval Zero_time = {0, 0};
static struct timeval start_time = {0,0};
static int timeout_usec = 1000;
static int wait_sec = 180;
struct timeval cur_start_time = {0,0};
static int step;
static int n_frames, cur_start = 0, cur_size = 0, transfer_so_far_size = 0;
//static int seq;

int get_number_of_frames(int buf_size){
    struct stat st;
    stat(src_filename, &st);
    int f_size = st.st_size;
    printf("File Size: %d; Buffer Size: %d\n",f_size, buf_size);
    if(f_size % buf_size == 0)
        return f_size / buf_size;
    else
        return (f_size / buf_size) + 1;
}

static int get_data_buffer_size(){
    return BUF_SIZE;
}

// received ack/nack from receiver

packet reveice_ack(struct sockaddr_in from_addr, int sock){
    int from_len = sizeof(from_addr);
    packet ack;
    memset(&ack, 0, sizeof(ack));
    int bytes = recvfrom(sock, &ack, sizeof(ack), 0,  
                (struct sockaddr *)&from_addr, 
                &from_len);
    int from_ip = from_addr.sin_addr.s_addr;

    if(ack.ack_flag == 1){
        /*
        printf("Received ACK from (%d.%d.%d.%d): for %d\n", 
                (htonl(from_ip) & 0xff000000)>>24,
                (htonl(from_ip) & 0x00ff0000)>>16,
                (htonl(from_ip) & 0x0000ff00)>>8,
                (htonl(from_ip) & 0x000000ff),
                ack.ack_no);
        */
    }else{
        /*
        printf("Received NACK from (%d.%d.%d.%d): for %d\n", 
                (htonl(from_ip) & 0xff000000)>>24,
                (htonl(from_ip) & 0x00ff0000)>>16,
                (htonl(from_ip) & 0x0000ff00)>>8,
                (htonl(from_ip) & 0x000000ff),
                ack.ack_no);
        */
    }
    return ack;
}

// send the destination filename to receiver

void send_dest_filename(struct sockaddr_in send_addr, struct sockaddr_in from_addr, int sock, char *filename){
    packet first_packet, ack;
    fd_set mask, read_mask;
    memset(&first_packet, 0, sizeof(packet));
    memcpy(&first_packet.data_buf, filename, strlen(filename) * sizeof(char));
    gettimeofday(&first_packet.ts, NULL);
    first_packet.seq_no = -1;
    int ack_flag = 0;
    FD_ZERO(&read_mask);
    FD_SET(sock, &read_mask);
    struct timeval timeout;
    int init_flag = 1;
    
    for(;;){
        timeout.tv_sec = 0;
        timeout.tv_usec = timeout_usec;
        gettimeofday(&first_packet.ts,  NULL);
        sendto_dbg(sock, &first_packet, sizeof(first_packet), 0, 
                    (struct sockaddr *)&send_addr, sizeof(send_addr));
        //printf("Sending.. Destination Filename: %s\n", first_packet.data_buf);
        if(init_flag){
            memcpy(&start_time, &first_packet.ts, sizeof(first_packet.ts));
            init_flag = 0;
        }
        mask = read_mask;
                
        int num = select(FD_SETSIZE, &mask, NULL, NULL, &timeout);

        if (num > 0) {
            if (FD_ISSET(sock, &mask)) {
                int from_len = sizeof(from_addr);
                packet ack;
                memset(&ack, 0, sizeof(ack));
                int bytes = recvfrom(sock, &ack, sizeof(ack), 0,  
                            (struct sockaddr *)&from_addr, 
                            &from_len);
                int from_ip = from_addr.sin_addr.s_addr;
                    
                if(ack.ack_flag == 1){
                    ack_flag = 1;
                    /*
                    printf("Received ACK from (%d.%d.%d.%d): Destination Filename %s\n", 
                            (htonl(from_ip) & 0xff000000)>>24,
                            (htonl(from_ip) & 0x00ff0000)>>16,
                            (htonl(from_ip) & 0x0000ff00)>>8,
                            (htonl(from_ip) & 0x000000ff),
                            ack.data_buf);*/
                    break;
                }else if(ack.ack_flag == 0){
                    /*
                    printf("Received NACK from (%d.%d.%d.%d): Destination Filename %s\n", 
                            (htonl(from_ip) & 0xff000000)>>24,
                            (htonl(from_ip) & 0x00ff0000)>>16,
                            (htonl(from_ip) & 0x0000ff00)>>8,
                            (htonl(from_ip) & 0x000000ff),
                            ack.data_buf);*/
                }else{
                    /*
                    printf("Connection Blocked from (%d.%d.%d.%d):\n", 
                            (htonl(from_ip) & 0xff000000)>>24,
                            (htonl(from_ip) & 0x00ff0000)>>16,
                            (htonl(from_ip) & 0x0000ff00)>>8,
                            (htonl(from_ip) & 0x000000ff));
                    sleep(wait_sec);*/
                }
            }
        }else{
            continue;
        }    
    }
    return;
}




void ncp(struct sockaddr_in send_addr, struct sockaddr_in from_addr, int sock){
    int             seq = 0;
    int             data_buffer_size = get_data_buffer_size();
    n_frames = get_number_of_frames(data_buffer_size);
    printf("Total Number of Frames: %d\n", n_frames);
    int             wnd_size = WINDOW_SIZE;
    packet          sender_wnd[wnd_size];
    packet          ack;
    fd_set          mask, read_mask;
    struct timeval  diff_time;
    sendto_dbg_init(loss_percent);
    send_dest_filename(send_addr, from_addr, sock, dest_filename);
    FILE            *f_ptr = fopen(src_filename, "rb");
    if(f_ptr == NULL){
        printf("Error in accessing File. Terminating");
        exit(1);
    }
    int total_data_transmitted = 0;
    step = ceil(10000000.0/ BUF_SIZE);
    struct timeval last_receive_time;
    while(1){
        int wnd_idx = 0;
        memset(&sender_wnd, 0, wnd_size * sizeof(packet));
        for(int wnd_idx = 0; wnd_idx < wnd_size && seq<=n_frames; wnd_idx++){
            sender_wnd[wnd_idx].seq_no = seq;
            gettimeofday(&sender_wnd[wnd_idx].ts, NULL);
            
            if(seq < n_frames){
                int bytes = fread(sender_wnd[wnd_idx].data_buf, 1, BUF_SIZE, f_ptr);
                sender_wnd[wnd_idx].size = bytes;
                //printf("Seq: %d, Bytes: %d, last char: %c\n", sender_wnd[wnd_idx].seq_no, bytes, sender_wnd[wnd_idx].data_buf[bytes-1]);
            }else{
                //memcpy(&sender_wnd[wnd_idx].data_buf, eof, strlen(eof) * sizeof(char));
            }
            //printf("Seq: %d; Time: %ld\n",sender_wnd[wnd_idx].seq_no, sender_wnd[wnd_idx].ts.tv_sec);
            sendto_dbg(sock, &sender_wnd[wnd_idx], sizeof(sender_wnd[wnd_idx]), 0, 
                    (struct sockaddr *)&send_addr, sizeof(send_addr));
            total_data_transmitted += sender_wnd[wnd_idx].size;
            if(seq == 0){
                memcpy(&cur_start_time, &sender_wnd[wnd_idx].ts, sizeof(sender_wnd[wnd_idx].ts));
            }
            seq = seq + 1;
        }
        
        FD_ZERO(&read_mask);
        FD_CLR(sock, &read_mask);
        FD_SET(sock, &read_mask);

        int expected_ack_no = seq - 1;

        for(;;){
            mask = read_mask;
            
            
            int num = select(FD_SETSIZE, &mask, NULL, NULL, NULL);

            if (num > 0) {
                if (FD_ISSET(sock, &mask)) {
                    ack = reveice_ack(from_addr, sock);
                    gettimeofday(&last_receive_time, NULL);
                    if(ack.ack_flag == 1){
                        //gettimeofday(&last_receive_time, NULL);
                        if(ack.ack_no >= cur_start + step - 1){
                            timersub(&last_receive_time, &cur_start_time, &diff_time);
                            if(seq == n_frames){
                                cur_size = (ack.ack_no - cur_start) * BUF_SIZE + sender_wnd[wnd_size-1].size;
                            }else{
                                cur_size = (ack.ack_no - cur_start + 1) * BUF_SIZE;
                            }
                            transfer_so_far_size += cur_size;
                            cur_start = ack.ack_no + 1;
                            memcpy(&cur_start_time, &last_receive_time, sizeof(last_receive_time));
                            long int time_taken = diff_time.tv_sec * 1000000 + diff_time.tv_usec;
                            printf("File transferred so far: %f MB, Transfer Rate: %lf Megabits/sec\n", transfer_so_far_size / 1000000.0, (cur_size * 8.0) / (time_taken));
                        }
                        if(ack.ack_no == seq - 1){
                            break;
                        }
                        else{
                            for(int i = ack.ack_no + 1; i < seq; i++){
                                wnd_idx = i % wnd_size;
                                gettimeofday(&sender_wnd[wnd_idx].ts, NULL);
                                sendto_dbg(sock, &sender_wnd[wnd_idx], sizeof(sender_wnd[wnd_idx]), 0, 
                                        (struct sockaddr *)&send_addr, sizeof(send_addr));
                                total_data_transmitted += sender_wnd[wnd_idx].size;
                                //printf("Sending.. Seq: %d; Time: %ld\n",sender_wnd[wnd_idx].seq_no, sender_wnd[wnd_idx].ts.tv_sec);
                            }
                        }
                    }
                    else if(ack.ack_no < seq && ack.ack_flag == 0){
                        wnd_idx = ack.ack_no % wnd_size;
                        gettimeofday(&sender_wnd[wnd_idx].ts, NULL);
                        sendto_dbg(sock, &sender_wnd[wnd_idx], sizeof(sender_wnd[wnd_idx]), 0, (struct sockaddr *)&send_addr, sizeof(send_addr));
                        total_data_transmitted += sender_wnd[wnd_idx].size;
                        //printf("Sending.. Seq: %d; Time: %ld\n",sender_wnd[wnd_idx].seq_no, sender_wnd[wnd_idx].ts.tv_sec);
                    }
                }
            }
        }
        if(seq > n_frames){
            break;
        }
        //printf("Moving to the next window starting from Seq %d\n", seq);
    }
    timersub(&last_receive_time, &start_time, &diff_time);
    struct stat st;
    stat(src_filename, &st);
    int f_size = st.st_size;
    //printf("%d\n",f_size);
    long int time_taken = diff_time.tv_sec * 1000000 + diff_time.tv_usec;
    printf("Total Time: %lf sec, Filesize: %f MB, Transfer Rate: %lf Megabits/sec, Total Data Transmission (Including Retransmission): %lf MB\n", 
    time_taken / 1000000.0, f_size / 1000000.0, (f_size * 8.0)/(time_taken), total_data_transmitted / 1000000.0);
}

int main(int argc, char *argv[])
{
    struct sockaddr_in    send_addr;
    struct sockaddr_in    from_addr;
    socklen_t             from_len;
    struct hostent        h_ent;
    struct hostent        *p_h_ent;
    int                   host_num;
    int                   from_ip;
    int                   sock;
    fd_set                mask;
    fd_set                read_mask;
    int                   bytes;
    int                   num;
    char                  mess_buf[MAX_MESS_LEN];
    int                   seq;
    uhdr                  *hdr = (uhdr *)mess_buf;
    char                  *data_buf = &mess_buf[sizeof(uhdr)];
    FILE                  *f_ptr;
    int                   n_frames, f_size;
    packet                frame, ack;



    /* Parse commandline args */
    Usage(argc, argv);
    printf("Sending to %s at port %d file %s\n", Server_IP, Port, src_filename);


    /* Open socket for sending */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("udp_client: socket");
        exit(1);
    }
    
    /* Convert string IP address (or hostname) to format we need */
    p_h_ent = gethostbyname(Server_IP);
    if (p_h_ent == NULL) {
        printf("udp_client: gethostbyname error.\n");
        exit(1);
    }
    memcpy(&h_ent, p_h_ent, sizeof(h_ent));
    memcpy(&host_num, h_ent.h_addr_list[0], sizeof(host_num));

    send_addr.sin_family = AF_INET;
    send_addr.sin_addr.s_addr = host_num; 
    send_addr.sin_port = htons(Port);

    ncp(send_addr, from_addr, sock);
    return 0;

}

/* Read commandline arguments */
static void Usage(int argc, char *argv[]) {
    if (argc != 6) {
        Print_help();
    }
    int i = 1;
    loss_percent = atoi(argv[i++]);
    env = argv[i++];
    if(strcmp(env, "LAN") == 0){
        WINDOW_SIZE = 200;
    }else if(strcmp(env, "WAN") == 0){
        WINDOW_SIZE = 200;
    }
    src_filename = argv[i++];
    dest_filename = argv[i++];
    Server_IP = strtok(argv[i++], ":");
    if (Server_IP == NULL) {
        printf("Error: no server IP provided\n");
        Print_help();
    }   
    Port = atoi(strtok(NULL, ":"));
    //WINDOW_SIZE = atoi(argv[i++]);
}

static void Print_help() {
    printf("Usage: udp_client <loss_rate_percent> <env> <source_file_name> <dest_file_name> <server_ip>:<port>\n");
    exit(0);
}

static int Cmp_time(struct timeval t1, struct timeval t2) {
    if      (t1.tv_sec  > t2.tv_sec) return 1;
    else if (t1.tv_sec  < t2.tv_sec) return -1;
    else if (t1.tv_usec > t2.tv_usec) return 1;
    else if (t1.tv_usec < t2.tv_usec) return -1;
    else return 0;
}