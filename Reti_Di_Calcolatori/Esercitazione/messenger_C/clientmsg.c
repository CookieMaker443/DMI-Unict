#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_BUFF 1024

int socketFileDescriptor;

void threadRicezione(void * argv){

    char buffer[MAX_BUFF];
    for(;;){

        int byteRicevuti = recv(socketFileDescriptor, buffer, MAX_BUFF, 0);

        if(byteRicevuti == 0){
            //server ha chiuso la connessione
            exit(1);
        } else {
            buffer[byteRicevuti] = '\0';
            printf("Ricevuto: %s\n", buffer);
        }
        fflush(stdout);

    }

}

void threadInvio(void * argv){
    char buffer[MAX_BUFF];
    for(;;){
        fflush(stdout);
        fgets(buffer, MAX_BUFF, stdin);

        //tanto per non svuotare ogni volta il buffer
        int byteDaInviare = strcspn(buffer, "\n");
        buffer[byteDaInviare] = '\0';

        if (strcmp(buffer, "exit") == 0) {
            close(socketFileDescriptor);
            exit(0);
        }

        send(socketFileDescriptor, buffer, byteDaInviare, 0);
    }
}

int main(int argumentCount, char * argumentValues[]) {

  if(argumentCount < 3){
    printf("Coglione, devi usarmi come %s <IP> <PORTA> \n", argumentValues[0]);
    exit(1);
  }

  
  int numBytes;
  struct sockaddr_in serverAddressPort;
  char buffer[MAX_BUFF];  // max buffer for I/O operations for the system + null termination

 if ((socketFileDescriptor = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
   perror("socket");
   return 1;
 }

 serverAddressPort.sin_family = AF_INET;
 serverAddressPort.sin_port = htons(atoi(argumentValues[2]));
 serverAddressPort.sin_addr.s_addr = inet_addr(argumentValues[1]);

 if (connect(socketFileDescriptor, (struct sockaddr *)&serverAddressPort, sizeof serverAddressPort) < 0) {
   perror("connect");
   close(socketFileDescriptor);
   return 2;
 }

 printf("Connesso al server!\n");

 pthread_t send_thread, recv_thread;
 pthread_create(&send_thread, NULL, (void *) threadInvio, NULL);
 pthread_create(&recv_thread, NULL, (void *) threadRicezione, NULL);

 pthread_join(send_thread, NULL);
 pthread_join(recv_thread, NULL);

 close(socketFileDescriptor);
 return 0;
}
