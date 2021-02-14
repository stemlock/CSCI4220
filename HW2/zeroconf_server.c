#include <dns_sd.h>
#include <stdio.h>            // For stdout, stderr
#include <string.h>            // For strlen(), strcpy(), bzero()
#include <errno.h>            // For errno, EINTR
#include <time.h>
#include <stdlib.h>
#include <pthread.h>
#include <ctype.h>
#ifdef _WIN32
#include <process.h>
typedef    int    pid_t;
#define    getpid    _getpid
#define    strcasecmp    _stricmp
#define snprintf _snprintf
#else
#include <sys/time.h>        // For struct timeval
#include <unistd.h>            // For getopt() and optind
#include <arpa/inet.h>        // For inet_addr()
#endif

// Note: the select() implementation on Windows (Winsock2)
//fails with any timeout much larger than this
#define LONG_TIME 100000000
#define BUFFER_LEN 1024
#define NUM_THREADS 2

static volatile int stopNow = 0;

typedef struct PlayerInfo{
    char* pName;
    char* pRPSChoice;
    int client_sd;
} PlayerInfo;

int portNum = 54037;

//Converts all the characters to uppercase, if this fails, it will convert up the failed character
// ie sentas1dfsad0 will be converted to SENTAS1dfasd0 and return false
bool convertToUpper(char* sentence){
    for(int i = 0; i < strlen(sentence); i++){
        if(!sentence[i] || isdigit(sentence[i]) || sentence[i] == '\n'){
            return false;
        }
        else if (isalpha(sentence[i])){
            sentence[i] = toupper(sentence[i]);
        }
    }
    return true;
}
void clearPlayerInfo(struct PlayerInfo* p){
    if(p->pName == NULL || p->pRPSChoice == NULL){
        return;
    }
    memset(&(p->pName[0]), 0, strlen(p->pName)); 
    memset(&(p->pRPSChoice[0]), 0, strlen(p->pRPSChoice));

}

void WhoWins(PlayerInfo* p1, PlayerInfo* p2, char resulting_string[]){
    if(p1->pRPSChoice == NULL || p2->pRPSChoice == NULL || p1->pName == NULL || p2->pName == NULL){
        sprintf(resulting_string, "No input detected from opponent, closed connection\n");
        return;
    }
    //CAN short theses three if statements to one, but this I believe increase readability
    if( strcmp(p1->pRPSChoice,"ROCK") == 0 && strcmp(p2->pRPSChoice,"SCISSORS") == 0){
        sprintf(resulting_string, "%s smashes %s!  %s defeats %s\n", p1->pRPSChoice, p2->pRPSChoice, p1->pName, p2->pName);
    }
    else if( strcmp(p1->pRPSChoice,"SCISSORS") == 0 && strcmp(p2->pRPSChoice,"PAPER")== 0){
        sprintf(resulting_string, "%s cuts %s!  %s defeats %s\n", p1->pRPSChoice, p2->pRPSChoice, p1->pName, p2->pName);
    }
    else if( strcmp(p1->pRPSChoice,"PAPER") == 0 && strcmp(p2->pRPSChoice,"ROCK") == 0){
        sprintf(resulting_string, "%s covers %s!  %s defeats %s\n", p1->pRPSChoice, p2->pRPSChoice, p1->pName, p2->pName);
    }
    //ties
    else if( strcmp(p1->pRPSChoice, p2->pRPSChoice) == 0){
        sprintf(resulting_string, "TIE! No wins!\n");
    }
    //second play wins if none of the above matches
    else if( strcmp(p2->pRPSChoice,"ROCK") == 0 && strcmp(p1->pRPSChoice,"SCISSORS") == 0){
        sprintf(resulting_string, "%s smashes %s!  %s defeats %s\n", p2->pRPSChoice, p1->pRPSChoice, p2->pName, p1->pName);
    }
    else if( strcmp(p2->pRPSChoice,"SCISSORS") == 0 && strcmp(p1->pRPSChoice,"PAPER") == 0){
        sprintf(resulting_string,"%s cuts %s!  %s defeats %s\n", p2->pRPSChoice, p1->pRPSChoice, p2->pName, p1->pName);
    }
    else if ( strcmp(p2->pRPSChoice,"PAPER") == 0 && strcmp(p1->pRPSChoice,"ROCK") == 0) {
        sprintf(resulting_string, "%s covers %s!  %s defeats %s\n", p2->pRPSChoice, p1->pRPSChoice, p2->pName, p1->pName);
    }
}

void* PlayGame(void* void_player){

    PlayerInfo* player = (PlayerInfo*) void_player;

    //Initialze Msg to send to client
    char msg_name[] = "What is your name?\n";
    char msg_RPS_choice[] = "Rock, paper, or scissors?\n";
    char errMsgName[] = "Error: Please enter valid alpha-based name\n";
    char errMsgChoice[] = "Error: Please enter either \"rock\", \"paper\" or \"scissors\"\n";
    ssize_t bytes_sent, bytes_recv;
    bool valid;
    bool is_space = true;

    // Wait to recieve player name and choice of rock, paper or scissors

    //Ask and recieve player's name - error check for white space
    do{
        bytes_sent = send(player->client_sd, msg_name, strlen(msg_name), 0);
        if(bytes_sent == -1){
            perror("send()");
            return NULL;
        }
        bytes_recv = recv(player->client_sd, player->pName, BUFFER_LEN, 0);
        for (int i = 0; i < strlen(player->pName); i++){
            if (isspace(player->pName[i]) == 0){
                is_space = false;
            }          
        }
        if (is_space){
            send(player->client_sd, errMsgName, strlen(errMsgName), 0);
        }
    } while (is_space);

    if(bytes_recv == -1){
        printf("%d: %d\n", player->client_sd, errno);
        perror("recv()");
        printf("Address: %d\n", player->client_sd);
        return NULL;
    }
    else if(bytes_recv == 0){
        printf("No bytes received\n");
        player->pName = NULL;
        player->pRPSChoice = NULL;
        return NULL;
    }
    player->pName[bytes_recv-1] = '\0';
    valid = convertToUpper(player->pName);
    while(!valid){
        send(player->client_sd, errMsgName, strlen(errMsgName), 0);
        bytes_recv = recv(player->client_sd, player->pName, BUFFER_LEN, 0);
        player->pName[bytes_recv-1] = '\0';
        valid = convertToUpper(player->pName);
    }

    //Asking the player's choice of rock, paper, or scissors
    bytes_sent = send(player->client_sd, msg_RPS_choice, strlen(msg_RPS_choice), 0);
    if(bytes_sent == -1){
        perror("send()");
        return NULL;
    }

    bytes_recv = recv(player->client_sd, player->pRPSChoice, BUFFER_LEN, 0);
    if(bytes_recv == -1){
        perror("recv()");
        printf("Address: %d\n", player->client_sd);
        return NULL;
    }
    else if(bytes_recv == 0){
        printf("No bytes received\n");
        player->pName = NULL;
        player->pRPSChoice = NULL;
        return NULL;
    }
    player->pRPSChoice[bytes_recv-1] = '\0';
    valid = convertToUpper(player->pRPSChoice);
    //Error Checking code for the correct choice if not keep asking user
    //Used strstr less strict
    while( !valid || 
        (strcmp(player->pRPSChoice, "ROCK") && 
        strcmp(player->pRPSChoice, "PAPER") && 
        strcmp(player->pRPSChoice, "SCISSORS") )){

        send(player->client_sd, errMsgChoice, strlen(errMsgChoice), 0);
        bytes_recv = recv(player->client_sd, player->pRPSChoice, BUFFER_LEN, 0);
        player->pRPSChoice[bytes_recv-1] = '\0';
        valid = convertToUpper(player->pRPSChoice);
    }
    return NULL;
}

void MyRegisterCallBack(){
    return;
}

void HandleEvents(DNSServiceRef serviceRef, struct sockaddr_in *server, int server_socket){
    int dns_sd_fd = DNSServiceRefSockFD(serviceRef);
    int nfds = dns_sd_fd + 1;
    fd_set readfds;
    struct timeval tv;
    int result;

    //-------------------Struct for Storing Playing Information--------------
    PlayerInfo player1;
    player1.pName = (char*) calloc(BUFFER_LEN, sizeof(char));
    player1.pRPSChoice = (char*) calloc(BUFFER_LEN, sizeof(char));

    PlayerInfo player2;
    player2.pName = (char*) calloc(BUFFER_LEN, sizeof(char));
    player2.pRPSChoice = (char*) calloc(BUFFER_LEN, sizeof(char));


    PlayerInfo players[NUM_THREADS]; //Create players equal to the number of threads
    pthread_t tid[NUM_THREADS];
    players[0] = player1;
    players[1] = player2;
    //---------------------------End of Player Struct------------------------

    while (!stopNow){
        FD_ZERO(&readfds);
        FD_SET(dns_sd_fd, &readfds);
        FD_SET(server_socket, &readfds);
        tv.tv_sec = LONG_TIME;
        tv.tv_usec = 0;
        //------------------Server Code Waiting for Connections------------------
        result = select(nfds, &readfds, (fd_set*)NULL, (fd_set*)NULL, &tv);
        if (result > 0){
            DNSServiceErrorType err = kDNSServiceErr_NoError;
            if (FD_ISSET(dns_sd_fd, &readfds)) err = DNSServiceProcessResult(serviceRef);
        }
        else{
            printf("select() returned %d errno %d %s\n", result, errno, strerror(errno));
            if (errno != EINTR) stopNow = 1;
        }

        printf( "SERVER: Waiting on first connection\n" );
        struct sockaddr_in client1;
        int client1_len = sizeof( client1 );
        player1.client_sd = accept(server_socket, (struct sockaddr*) &client1, (socklen_t*) &client1_len);
        printf( "SERVER: Accepted new client connection on sd %d from address: %s\n", player1.client_sd, inet_ntoa(client1.sin_addr));
        pthread_create(&tid[0], NULL, &PlayGame, &player1);

        printf( "SERVER: Waiting on second connection\n" );
        struct sockaddr_in client2;
        int client2_len = sizeof( client2 );
        player2.client_sd = accept(server_socket, (struct sockaddr*) &client2, (socklen_t*) &client2_len);
        printf( "SERVER: Accepted new client connection on sd %d from address: %s\n", player2.client_sd, inet_ntoa(client1.sin_addr));

        pthread_create(&tid[1], NULL, &PlayGame, &player2);

        printf("Starting RPS Game!\n");

        for(int i = 0; i < NUM_THREADS; i++){
            pthread_join(tid[i], NULL);
        }

        char resultMsg[1024];
        memset(&resultMsg[0], 0, 1024);
        WhoWins(&player1, &player2, &resultMsg[0]);

        send(player1.client_sd, resultMsg, strlen(resultMsg), 0);
        send(player2.client_sd, resultMsg, strlen(resultMsg), 0);
        close(player1.client_sd);
        close(player2.client_sd);
        printf("Game Over!\n");

        clearPlayerInfo(&player1);
        clearPlayerInfo(&player2);
        //-----------------------------------------------------------------------
    }
}
static DNSServiceErrorType MyDNSServiceRegister(struct sockaddr_in *server, int server_socket){
    DNSServiceErrorType error;  
    DNSServiceRef serviceRef;
    error = DNSServiceRegister(&serviceRef,
                                0,                  // no flags
                                0,                  // all network interfaces
                                "liy38_temloo",     // name
                                "_rps._tcp",        // service type
                                "",                 // register in default domain(s)
                                NULL,               // use default host name
                                server->sin_port,    // port number
                                0,                  // length of TXT record
                                NULL,               // no TXT record
                                NULL,               // call back function
                                NULL);              // no context
    if (error == kDNSServiceErr_NoError){
        HandleEvents(serviceRef, server, server_socket);
        DNSServiceRefDeallocate(serviceRef);
    }
    return error;
}

int main(){
    
    //-------------------Setting up the connection-------------------------
    struct sockaddr_in server;
    int server_socket;
    socklen_t sockaddr_len = sizeof(server);
    memset(&server, 0, sockaddr_len);

    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(0);
    server.sin_family = PF_INET;
    if((server_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(-1);
    }
    
    if(bind(server_socket, (struct sockaddr *)&server, sockaddr_len) < 0) {
        perror("bind");
        exit(-1);
    }

    getsockname(server_socket, (struct sockaddr *)&server, &sockaddr_len);
    printf("%d\n", ntohs(server.sin_port));

    if ( listen( server_socket, 5 ) == -1 ){
        perror( "listen()" );
        exit(-1);
    }
    //----------------------------Socket Ready ------------------------------

    DNSServiceErrorType error = MyDNSServiceRegister(&server, server_socket);
    fprintf(stderr, "DNSServiceDiscovery returned %d\n", error);
    return 0;

}