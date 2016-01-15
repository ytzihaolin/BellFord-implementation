//
//  main.c
//  BFusingSelect
//
//  Created by Zihao Lin on 12/7/14.
//  Copyright (c) 2014 Zihao Lin. All rights reserved.
//



#include<unistd.h>
#include<sys/socket.h>
#include<stdio.h>
#include<arpa/inet.h>
#include<string.h>//gets,puts
#include<stdlib.h>
#include<unistd.h>//close
#include<sys/types.h>
#include<signal.h>
#include<sys/time.h>
#include<time.h>
#include <netdb.h>

#define maxnode 20
#define infinite 999

struct addr{
    char IP[20];
    int port;
};

struct nb{
    struct addr ID;
    int isnneighbor;
    struct timeval timestamp;
}neighbor[maxnode];

struct routingtable{
    struct addr dst;
    struct addr via;
    int cost;
};

struct{
    struct addr src;
    struct routingtable rt[10];
    int cost;
    int numofentry;
}sendpkt,recvpkt;

struct timeval now,checktimeout;
int numberofneighbor=0;
int trigger;
int downneighbor;
int tout;

struct sockaddr_in readsocketaddr;

int DV[maxnode][maxnode];//distant vector
int linkdowntemp[maxnode];

int checkneighbor(struct addr checknb){//translate neighbor into integer
    int i;

    for(i=0;i<numberofneighbor+1;i++){
        if((checknb.port==neighbor[i].ID.port)&&(strcmp(checknb.IP,neighbor[i].ID.IP)==0)){
            return i;
        }
    }
    return numberofneighbor+1;
}

void printDV(){
    int i,j;
    printf("%d\t",neighbor[0].ID.port);
    for(i=1;i<maxnode;i++){
        printf("%d\t",neighbor[i].ID.port);
    }
    printf("\n");
    for(i=1;i<maxnode;i++){//printf Distant vector
        printf("%d\t",neighbor[i].ID.port);
        for(j=1;j<maxnode;j++){
            printf("%d\t",DV[i][j]);
        }
        printf("\n");
    }
    printf("\n\n\n");
}

void init(int argc, char ** argv){
    int i,j;
    for(i=0;i<maxnode;i++){
        for(j=0;j<maxnode;j++){
            if(i==0||j==0){
                DV[i][j]=1000;
            }else{
                DV[i][j]=infinite;
            }
        }
    }
    for(i=0;i<10;i++){
        bzero(&neighbor[i],sizeof(neighbor[i]));
    }
    
    //get local IP address and assign to the host
    char hname[128];
    struct hostent *hent;
    gethostname(hname, sizeof(hname));
    hent = gethostbyname(hname);
    
    readsocketaddr.sin_family=PF_INET;
    readsocketaddr.sin_addr.s_addr=inet_addr(inet_ntoa(*(struct in_addr*)(hent->h_addr_list[0])));
    readsocketaddr.sin_port=htons(atoi(argv[1]));
    
    strcat(neighbor[0].ID.IP,inet_ntoa(readsocketaddr.sin_addr));
    neighbor[0].ID.port=atoi(argv[1]);
    DV[0][0]=1000;
    
    tout=atoi(argv[2]);
    
    numberofneighbor=(argc-3+1)/3;// let initial neighbor copied into the neighbor array
    
    for (i=1;i<numberofneighbor+1;i++){
        strcat(neighbor[i].ID.IP,argv[3*i]);
        neighbor[i].ID.port=atoi(argv[3*i+1]);
        neighbor[i].isnneighbor=1;
        DV[i][i]=atoi(argv[3*i+2]);
        linkdowntemp[i]=DV[i][i];
    }
}

void sendingupdate(int writesocket){//sending update to all your neighbors
    struct sockaddr_in tempreadsockaddr;
    
    
    //make packet
    bzero(&sendpkt,sizeof(sendpkt));
    strcat(sendpkt.src.IP,neighbor[0].ID.IP);
    sendpkt.src.port=neighbor[0].ID.port;
    int i,j,indicator;
    for(i=1;i<numberofneighbor+1;i++){
        indicator=1;
        sendpkt.rt[i].cost=infinite;
        for(j=1;j<numberofneighbor+1;j++){
            if(DV[i][j]<DV[i][indicator]){
                indicator=j;
            }
        }
        strcat(sendpkt.rt[i].dst.IP,neighbor[i].ID.IP);
        sendpkt.rt[i].dst.port=neighbor[i].ID.port;
        strcat(sendpkt.rt[i].via.IP,neighbor[indicator].ID.IP);
        sendpkt.rt[i].via.port=neighbor[indicator].ID.port;
        sendpkt.rt[i].cost=DV[i][indicator];
        
        sendpkt.numofentry=numberofneighbor;
    }
    
    for(i=1;i<numberofneighbor+1;i++){
        if(neighbor[i].isnneighbor==1){
            sendpkt.cost=DV[i][i];
            
            
            /*/split horizon
            for (j=1; j<numberofneighbor+1; j++) {
                if (checkneighbor(sendpkt.rt[j].via)==i&&(checkneighbor(sendpkt.rt[j].via)!=checkneighbor(sendpkt.rt[j].dst))) {
                    sendpkt.rt[j].cost=infinite;
                }
            }
            */
            
            tempreadsockaddr.sin_family=PF_INET;
            tempreadsockaddr.sin_addr.s_addr=inet_addr(neighbor[i].ID.IP);
            tempreadsockaddr.sin_port=htons(neighbor[i].ID.port);
            socklen_t mm= sizeof(struct sockaddr_in);
            sendto(writesocket, &sendpkt, sizeof(sendpkt), 0, (struct sockaddr *)&tempreadsockaddr, mm);
        }
    }
    trigger=0;
}

int BFCalculate(){
    int i=checkneighbor(recvpkt.src);
    int j,k;
    if(DV[i][i]!=recvpkt.cost){
        if (recvpkt.cost==infinite) {
            neighbor[i].isnneighbor=0;
            for (j=1; j<numberofneighbor+1; j++) {
                DV[j][i]=infinite;
            }
            trigger=1;
            return 1;
            
        }else{
            DV[i][i]=recvpkt.cost;
            trigger=1;
        }
    }

    for(j=1;j<recvpkt.numofentry+1;j++){
        k=checkneighbor(recvpkt.rt[j].dst);
        
        if(k==numberofneighbor+1){//new node in the structure found
            numberofneighbor++;
            neighbor[numberofneighbor].ID.port=recvpkt.rt[j].dst.port;
            strcat(neighbor[numberofneighbor].ID.IP,recvpkt.rt[j].dst.IP);
        }
        if(k==i) continue;
        if(DV[k][i]!=DV[i][i]+recvpkt.rt[j].cost){
            
            if (DV[i][i]+recvpkt.rt[j].cost>infinite) {
                DV[k][i]=infinite;
            }
            else {
                DV[k][i]=DV[i][i]+recvpkt.rt[j].cost;
                trigger=1;
            }
        }
    }
    return 1;
}


int main(int argc, char ** argv)
{
   
    
    trigger=0;
    
   
    
    int readsocket=socket(PF_INET,SOCK_DGRAM,0);
    int writesocket=socket(PF_INET,SOCK_DGRAM,0);
    
    if(readsocket<0){
        puts("Cannot create read socket");
    }
    if(writesocket<0){
        puts("Cannot create write socket");
    }
    
    
    
    init(argc, argv);
    
    if (bind(readsocket,(struct sockaddr*)&readsocketaddr,sizeof(readsocketaddr)) < 0){
        perror("ERROR on write socket binding");
        return 0;
    }
    puts("read socket created and binded");
    
    
    
    printf("Your are at: %s:%d\n",neighbor[0].ID.IP,neighbor[0].ID.port);
    sendingupdate(writesocket);
    
    //printDV();
    puts("\n\nCommand:");
    
    char message[100];

    fd_set rdfds;//
    struct timeval tv; //store timeout
    int ret; // return val
    int maxfdp;
    struct addr tempaddr;
    int i,j;
    
    downneighbor=0;
    
    for (i=0; i<maxnode; i++) {
        linkdowntemp[i]=infinite;
        gettimeofday(&now, NULL);
        neighbor[i].timestamp=now;
    }
    
    gettimeofday(&checktimeout, NULL);
    
    while(1){
        FD_ZERO(&rdfds); //clear rdfds
        FD_SET(1, &rdfds);//add stdin handle into rdfds
        FD_SET(readsocket, &rdfds);
        
        maxfdp=readsocket>1?readsocket+1:1+1;
        
        tv.tv_sec = 0;
        tv.tv_usec = 500000;
        ret = select(maxfdp, &rdfds, NULL, NULL, &tv);
        if(ret < 0)
            perror("\nselect");
        
        
        else if(ret == 0){//check timeout
            gettimeofday(&now, NULL);
            
            for (i=1; i<numberofneighbor+1 ; i++) {
                if (neighbor[i].isnneighbor==1&&(now.tv_sec*1000000+now.tv_usec-neighbor[i].timestamp.tv_sec*1000000-neighbor[i].timestamp.tv_usec)>3*tout*1000000) {
                    for (j=1; j<numberofneighbor+1; j++) {
                        DV[j][i]=infinite;
                    }
                    neighbor[i].isnneighbor=0;
                    trigger=1;
                }
                if (trigger==1) {
                    sendingupdate(writesocket);
                   // printDV();
                }
            }
            
            
            if ((now.tv_sec*1000000+now.tv_usec)-(checktimeout.tv_sec*1000000+checktimeout.tv_usec)>tout*1000000) {
                sendingupdate(writesocket);
                gettimeofday(&checktimeout, NULL);
            }
        }
        
        
        else
        {
            if(FD_ISSET(1, &rdfds))//there is input from terminal
            {
                gets(message);
                
                if(strcmp(message,"showrt")==0){
                    gettimeofday(&now,NULL);
                    printf("%s	Distance vector list is:\n",ctime(&now));

                    int indicator,j;
                    for(i=1;i<numberofneighbor+1;i++){
                        indicator=1;
                        for(j=1;j<numberofneighbor+1;j++){
                            if(DV[i][j]<DV[i][indicator]){
                                indicator=j;
                            }
                        
                        }

                        printf("Destination = %s:%d, Cost=%d, Link=(%s:%d)\n",neighbor[i].ID.IP,neighbor[i].ID.port,DV[i][indicator],neighbor[indicator].ID.IP,neighbor[indicator].ID.port);
                      
                    }
                    
                    puts("\n\nCommand:");
                    
                }else
                    if(strncmp(message, "linkdown", 8)==0){
                        
       
                        for (i=9; message[i]!=' ' ; i++) {
                            tempaddr.IP[i-9]=message[i];
                        }
                        tempaddr.port=atoi(message+i);
                        
                        
                        downneighbor=checkneighbor(tempaddr);
                        linkdowntemp[downneighbor]=DV[downneighbor][downneighbor];
                        
                        for (j=1; j<numberofneighbor+1; j++) {
                            DV[j][downneighbor]=infinite;
                        }
                        
                        
                        //printDV();
                        sendingupdate(writesocket);
                        neighbor[downneighbor].isnneighbor=0;
                        downneighbor=0;
                        puts("\n\nCommand:");
                        
                    }else
                        if(strncmp(message,"linkup",6)==0){
                            bzero(&tempaddr, sizeof(tempaddr));
                            for (i=7; message[i]!=' ' ; i++) {
                                tempaddr.IP[i-7]=message[i];
                            }
                            tempaddr.port=atoi(message+i);
                            
                          
                            i=checkneighbor(tempaddr);
                            if (i<=numberofneighbor) {
                                DV[i][i]=linkdowntemp[i];
                                neighbor[i].isnneighbor=1;
                                
                                sendingupdate(writesocket);
                               // printDV();
                            }else{
                                puts("no such neighbor");
                            }
                            puts("\n\nCommand:");
                            
                        }else
                            if(strcmp(message,"close")==0){
                                break;

                            }else{
                                
                                puts("Wrong command!\n\nCommand:");
                            }
                
            }
            
            
            if(FD_ISSET(readsocket, &rdfds)){//incoming udp message
                struct sockaddr_in tempinsockaddr;
                socklen_t mm= sizeof(struct sockaddr_in);
                bzero(&recvpkt,sizeof(recvpkt));
                long nRecEcho = recvfrom(readsocket,&recvpkt, sizeof(recvpkt), 0, (struct sockaddr *)&tempinsockaddr, &mm);
                if (nRecEcho == -1)
                {
                    perror("recvfrom()/n");
                    break;
                }

                int i=checkneighbor(recvpkt.src);
                if(i==numberofneighbor+1){//indicating new directly connected neighbor
                    numberofneighbor++;
                    neighbor[numberofneighbor].ID.port=recvpkt.src.port;
                    strcat(neighbor[numberofneighbor].ID.IP,recvpkt.src.IP);
                    neighbor[numberofneighbor].isnneighbor=1;
                    DV[i][i]=recvpkt.cost;
                }else if(neighbor[i].isnneighbor==0){
                    neighbor[i].isnneighbor=1;
                    DV[i][i]=recvpkt.cost;
                }
                
                gettimeofday(&now, NULL);
                neighbor[i].timestamp=now;
               // puts(ctime(&now));
                
                BFCalculate();
                if(trigger==1) sendingupdate(writesocket);
               // printDV();
            }//end imcoming udp message

        }//end else(there is input)
        
    
       
        
    }//end while
    close(writesocket);
    close(readsocket);
    return 0;
}//end main