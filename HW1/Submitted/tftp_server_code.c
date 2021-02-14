#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define BUFLEN 512
#define PACKLEN 516
#define RRQ 1
#define WRQ 2
#define DATA 3
#define ACK 4
#define ERROR 5

void send_error(int server_socket, struct sockaddr_in* sock_info, int error_num){
    //Sends the error code to the give client with the error number
    socklen_t sockaddr_len = sizeof(*sock_info);
    char buffer[BUFLEN];
    unsigned short int *opcode_ptr = (unsigned short int *)buffer;
    *opcode_ptr = htons(ERROR);
    *(opcode_ptr+1) = htons(error_num);
    sendto(server_socket, buffer, 5, 0, (struct sockaddr *)sock_info, sockaddr_len);
}

int checkTID(struct sockaddr_in* sock_info, unsigned short int client_port){
    //Returns true if the port matches the original sender, saved in client port
    if (sock_info->sin_port !=  client_port)
    {
        return -1;
    }
    else
    {
        return 1;
    }
}

void sig_alarm(int param){
    return;
}

void send_ack(int server_socket, unsigned int short* block_num, struct sockaddr_in* sock_info){
    //Sends the acknowledgement packet to the given sock info
    char buffer[BUFLEN];
    unsigned short int *opcode_ptr = (unsigned short int *)buffer;
    *opcode_ptr = htons(ACK);
    *(opcode_ptr+1) = htons(*block_num);
    socklen_t sockaddr_len = sizeof(*sock_info);
    if (sendto(server_socket, buffer, 4, 0, (struct sockaddr *)sock_info, sockaddr_len) != 4){
        perror("sendto");
        exit(-1);
    }
}

void handle_read(int server_socket, struct sockaddr_in* sock_info, char* file_name, unsigned short int client_port){
    //Handles get commands from the client
    printf("CHILD %d: Handling read for file %s\n", getpid(), file_name);
    FILE *fp; //file pointer
    char send_packet[PACKLEN];
    char recv_packet[5];
    size_t bytes_read;
    int bytes_recv;
    int timeouts = 0;
    uint16_t block_num = 1;
    unsigned short int *opcode_ptr = (unsigned short int *)send_packet;
    unsigned short int opcode;

    socklen_t sockaddr_len = sizeof(*sock_info);
    

    if( (fp = fopen(file_name, "r")) == NULL){
        perror("fopen");
        send_error(server_socket, sock_info, 1);
        close(server_socket);
        exit(-1);
    }
    *opcode_ptr = htons(DATA);
    do{
        bytes_read = fread(&send_packet[4], 1, BUFLEN, fp);
        opcode_ptr = (unsigned short int *)send_packet;
        *(opcode_ptr+1) = htons(block_num);

        //If the first DATA packet is less than 512, send last packet and terminate
        if(bytes_read < BUFLEN){
            sendto(server_socket, send_packet, bytes_read+4, 0, (struct sockaddr *)sock_info, sockaddr_len);
            printf("Finished Sending File, terminating connection\n");
            close(server_socket);
            return;
        }

        sendto(server_socket, send_packet, PACKLEN, 0, (struct sockaddr *)sock_info, sockaddr_len);
        
        timeout_recv:
        alarm(1);
        bytes_recv = recvfrom(server_socket, recv_packet, 5, 0, (struct sockaddr *)sock_info, &sockaddr_len);
        if(bytes_recv < 0){
            //Resending the last packet if 1 timeouts occurs
            //Close the connect if more than 10 timeouts occur 
            if(errno == EINTR){
                timeouts++;
                if (timeouts >= 10){
                    close(server_socket);
                    exit(-1);
                }
                sendto(server_socket, send_packet, BUFLEN+4, 0, (struct sockaddr *)sock_info, sockaddr_len);
                goto timeout_recv;
            }
        }
        else{
            //Resets the alarm and timeouts
            alarm(0);
            timeouts = 0;  
        }
        if(checkTID(sock_info, client_port) < 0){
            send_error(server_socket, sock_info, 5);
            continue;
        }
        opcode_ptr = (unsigned short int *)recv_packet;
        opcode = ntohs(*opcode_ptr);
        if (opcode != ACK){
            send_error(server_socket, sock_info, 0);
        }
        else if (ntohs(*(opcode_ptr+1)) != block_num){
            send_error(server_socket, sock_info, 0);
        }
        block_num++;
    } while ( bytes_read == BUFLEN );
}

void handle_write(int server_socket, struct sockaddr_in* sock_info, char* file_name, unsigned short int* block_num, unsigned short int client_port){
    //Handles the write request from the client
    int n;
    int fd; //file descriptor
    int timeouts = 0;
    int wrong_tid; //flag for the wrong TID
    char buffer[BUFLEN+4];
    char last_packet[5]; //array to store the last transmitted packet
    memset(&buffer[4], 0, BUFLEN);
    size_t bytes_written;
    unsigned short int *opcode_ptr;
    socklen_t sockaddr_len = sizeof(*sock_info);

    printf("CHILD %d: Handling write for file %s\n", getpid(), file_name);
    fd = open(file_name, O_CREAT | O_EXCL | O_WRONLY, 0666);

    if( fd < 0){
        perror("open");
        if (errno == EEXIST)
        {
            send_error(server_socket, sock_info, 2);
        }
        close(server_socket);
        exit(-1);
    }

    send_ack(server_socket, block_num, sock_info);

    do{
        opcode_ptr = (unsigned short int*)last_packet;
        *opcode_ptr = htons(ACK);
        *(opcode_ptr+1) = htons(*block_num);

        alarm(1);
        wrong_tid = 0;
        if ((n = recvfrom(server_socket, buffer, BUFLEN+4, 0, (struct sockaddr *)sock_info, &sockaddr_len))<0)
        {
            if (errno == EINTR)
            {
                timeouts++;
                if (timeouts == 10)
                {
                    close(server_socket);
                    exit(-1);
                }
                sendto(server_socket, last_packet, 4, 0, (struct sockaddr *)sock_info, sockaddr_len);
                printf("Last packet retransmitted\n");
            }
            else
            {
                perror("recvfrom");
            }       
        }
        else
        {
            alarm(0);
            timeouts = 0;
        }
        if (checkTID(sock_info, client_port)< 0)
        {
            send_error(server_socket, sock_info, 5);
            wrong_tid = 1;
            continue;
        }

        opcode_ptr = (unsigned short int*) buffer;
        *block_num = ntohs(*(opcode_ptr + 1));
        send_ack(server_socket, block_num, sock_info);

        bytes_written = write(fd, &buffer[4], n-4);
    }
    while (bytes_written == BUFLEN || wrong_tid == 1);

    close(server_socket);
}

int main(int argc, char* karg[]) {
    ssize_t n;
    char buffer[BUFLEN];
    int server_socket;
    struct sigaction act;
    
    unsigned short int opcode;
    unsigned short int * opcode_ptr;
    
    struct sockaddr_in sock_info;
    socklen_t sockaddr_len;
    
    /* Set up interrupt handlers */
    act.sa_handler = SIG_IGN;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGCHLD, &act, NULL);

    act.sa_handler = sig_alarm;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGALRM, &act, NULL);

    /* Set up UDP socket */
    sockaddr_len = sizeof(sock_info);
    memset(&sock_info, 0, sockaddr_len);
    
    sock_info.sin_addr.s_addr = htonl(INADDR_ANY);
    sock_info.sin_port = htons(0);
    sock_info.sin_family = PF_INET;
    
    if((server_socket = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(-1);
    }
    
    if(bind(server_socket, (struct sockaddr *)&sock_info, sockaddr_len) < 0) {
        perror("bind");
        exit(-1);
    }
    
    /* Get port and print it */
    getsockname(server_socket, (struct sockaddr *)&sock_info, &sockaddr_len);
    
    printf("%d\n", ntohs(sock_info.sin_port));
    
    /* Receive the first packet and deal w/ it accordingly */
    while(1) {
    intr_recv:
        n = recvfrom(server_socket, buffer, BUFLEN, 0,
                     (struct sockaddr *)&sock_info, &sockaddr_len);
        printf("Recieving from %d\n", ntohs(sock_info.sin_port));
        if(n < 0) {
            if(errno == EINTR) goto intr_recv;
            perror("recvfrom");
            exit(-1);
        }
        /* check the opcode */
        opcode_ptr = (unsigned short int *)buffer;
        opcode = ntohs(*opcode_ptr);
        if(opcode != RRQ && opcode != WRQ) {
            /* Illegal TFTP Operation */
            *opcode_ptr = htons(ERROR);
            *(opcode_ptr + 1) = htons(4);
            *(buffer + 4) = 0;
        intr_send:
            n = sendto(server_socket, buffer, 5, 0,
                       (struct sockaddr *)&sock_info, sockaddr_len);
            if(n < 0) {
                if(errno == EINTR) goto intr_send;
                perror("sendto");
                exit(-1);
            }
        }
        else {
            if(fork() == 0) {
                /* Child - handle the request */
                close(server_socket);
                break;
            }
            else {
                /* Parent - continue to wait */
            }
        }
    }
    
    struct sockaddr_in child_info;
    unsigned short int block_num = 0;

    sockaddr_len = sizeof(child_info);
    memset(&child_info, 0, sockaddr_len);
    
    child_info.sin_addr.s_addr = htonl(INADDR_ANY);
    child_info.sin_port = htons(0);
    child_info.sin_family = PF_INET;
    
    if((server_socket = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(-1);
    }
    
    if(bind(server_socket, (struct sockaddr *)&child_info, sockaddr_len) < 0) {
        perror("bind");
        exit(-1);
    }

    unsigned short int client_port = sock_info.sin_port;
    
    if(opcode == RRQ) handle_read(server_socket, &sock_info, (buffer+2), client_port);
    if(opcode == WRQ) handle_write(server_socket, &sock_info, (buffer+2), &block_num, client_port);
    return 0;
}