#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define ever (;;)
#define MAX_CLIENTS 4 //max clients in queue
#define PORT 8080
#define MAX_BUFF 1024


typedef struct {
   int sockfd;
   struct sockaddr_in address;
   char last_msg[MAX_BUFF];
} client_info;

// shared data structure with mutex
typedef struct {
   client_info *clients[MAX_CLIENTS];
   int count;
   pthread_mutex_t mutex;
} client_list;

client_list clients = { .count = 0 };


void gestioneClient(void * argv){

    client_info *info = (client_info *)argv;

    int client_sock = info->sockfd;
    char buffer[MAX_BUFF];
    char client_ip[INET_ADDRSTRLEN];
  
    inet_ntop(AF_INET, &info->address.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(info->address.sin_port);
    printf("New connection from %s:%d\n", client_ip, client_port);

    pthread_mutex_lock(&clients.mutex);
    clients.clients[clients.count++] = info;
    pthread_mutex_unlock(&clients.mutex);

    for (;;){
        ssize_t bytes_received = recv(client_sock, buffer, MAX_BUFF, 0);

        if(bytes_received == 0){
            printf("Client %s:%d disconnected\n", client_ip, client_port);
            pthread_mutex_lock(&clients.mutex);
            for(int i = 0; i < clients.count; i++) {
                if(clients.clients[i]->sockfd == client_sock) {
                    free(clients.clients[i]);
                    for(int j = i; j < clients.count - 1; j++) {
                        clients.clients[j] = clients.clients[j + 1];
                    }
                    clients.count--;
                    break;
                }
            }
            pthread_mutex_unlock(&clients.mutex);

            break;
        }

        buffer[bytes_received] = '\0';
        printf("[%s : %d] %s \n", client_ip, client_port, buffer);
        broadcast_message(buffer, client_sock);

    }

    close(client_sock);
}

void broadcast_message(const char *message, int sender_fd) {

   pthread_mutex_lock(&clients.mutex);
  
   for(int i = 0; i < clients.count; i++) {
       if(clients.clients[i]->sockfd != sender_fd) {
           send(clients.clients[i]->sockfd, message, strlen(message), 0);
       } else {
        strcpy(clients.clients[i]->last_msg, message);
       }
   }
  
   pthread_mutex_unlock(&clients.mutex);
}





int create_server(uint16_t port){

   int socketServerFD;
   struct sockaddr_in addr;
   socklen_t addr_len = sizeof(addr);

   if ((socketServerFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
       perror("socket failed");
       return -1;
   }

   memset(&addr, 0, sizeof(addr));
   addr.sin_family = AF_INET;
   addr.sin_port = htons(port);
   addr.sin_addr.s_addr = htonl(INADDR_ANY);

   // Bind and listen
   if (bind(socketServerFD, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
       perror("bind failed");
       return -2;
   }

   if (listen(socketServerFD, MAX_CLIENTS) < 0) {
       perror("listen failed");
       return -3;
   }

   return socketServerFD;
}


int main(int argc, char * argv[]){

    if(argc < 2){
        printf("Allora sei scemo, mi devi usare come %s <PORT>\n", argv[0]);
        exit(1);
    }

    int serverSocketFD = create_server(atoi(argv[1]));

    if(serverSocketFD<0){
        exit(1);
    }

    printf("Server in ascolto sulla porta %s\n", argv[1]);

    for(;;){

        client_info *client = malloc(sizeof(client_info));
        socklen_t addrlen = sizeof(client->address);

        client->sockfd = accept(serverSocketFD, (struct sockaddr*)&client->address, &addrlen);

        //devo separare logicamente il codice in due processi

        pthread_t threadclient;
        pthread_create(&threadclient, NULL, (void *)gestioneClient, (void*)client);
        pthread_detach(threadclient);
        
    }
    

    // devo poter gestire il client in separata sede e continuare ad accettare altre connessioni
    printf("Chiusura del server\n");
    pthread_mutex_destroy(&clients.mutex);
    close(serverSocketFD);
    exit(0);

}

