#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

int main(int argc,char**argv){
    int sockfd, n;
    struct sockaddr_in6 local_addr, remote_addr;
    char ipv6[46];
    socklen_t len = sizeof(struct sockaddr_in6);
    char msg[1000];
    if (argc<2){
        printf("Inserire il numero di porta");
        return -1;
    }
    if((sockfd=socket(AF_INET6,SOCK_DGRAM,0))<0){
        printf("Errore nell'apertura socket");
        return -1;
    }
    memset(&local_addr,0,len);
    local_addr.sin6_family = AF_INET6;
    local_addr.sin6_port = htons(atoi(argv[1]));
    if(bind(sockfd,(struct sockaddr *)&local_addr,len)<0){
        printf("\nCambiare numero di porta");
        return -1;
    }
    for(;;){
        n = recvfrom(sockfd,msg,999,0,(struct sockaddr*)&remote_addr,&len);
        msg[n] = 0;
        inet_ntop(AF_INET6,&(remote_addr.sin6_addr),ipv6,INET6_ADDRSTRLEN);
        printf("IP = %s, P = %d, msg = %s",ipv6,ntohs(remote_addr.sin6_port),msg);
        sendto(sockfd,msg,n,0,(struct sockaddr *)&remote_addr,len);
        if(strcmp(msg,"fine\n")==0){
            break;
        }
    }
    close(sockfd);
}