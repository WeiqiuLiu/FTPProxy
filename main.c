#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
/****************************
***Final project-FTP proxy***
****************************/
int main(int argc, const char *argv[])
{
	fd_set master_set, working_set;     //description word set
	struct timeval timeout;
	int proxy_cmd_socket    = 0;
	int accept_cmd_socket[3]= {0,0,0};  //an array to store accept_cmd_socket, because it will establish more than one accept_cmd_socket
	int connect_cmd_socket[3] ={0,0,0}; //same as above
	int proxy_data_socket   = 0;
	int accept_data_socket  = 0;
	int connect_data_socket = 0;
	int accept_data_port 	= 20000;    //The port that listened in proxy
	int connect_data_port	= 0;        //The port that proxy connect to
	int selectResult 	    = 0;
	int select_sd 		    = 20;
	int BUFFSIZE 		    = 65536;
	int clientaddresslen,serveraddresslen;
	struct sockaddr_in proxyaddress;
	struct sockaddr_in clientaddress;
	struct sockaddr_in serveraddress;

	int fileexist=0;//A flag that means file exist in cache when it equals to 1
	int pasv=0;     //A flag that means passive mode when it equals to 1, active mode when 0
	int fd=0;       //Description word of the file

	const int reuse=1;//Using for reuse port

	FD_ZERO(&master_set);
	bzero(&timeout, sizeof(timeout));


	//proxy_cmd_socket bind() listen()
	if((proxy_cmd_socket=socket(PF_INET,SOCK_STREAM,0))<0)printf("socket() failed\n");
	memset(&proxyaddress,0,sizeof(proxyaddress));
	proxyaddress.sin_family=AF_INET;
	proxyaddress.sin_addr.s_addr=htonl(INADDR_ANY);
	proxyaddress.sin_port=htons(21);
	setsockopt(proxy_cmd_socket,SOL_SOCKET,SO_REUSEADDR,(const char*)&reuse,sizeof(reuse));
	if(bind(proxy_cmd_socket,(struct sockaddr *)&proxyaddress,sizeof(proxyaddress))<0)printf("proxy_cmd_socket bind() failed\n");
	if(listen(proxy_cmd_socket,5)<0)printf("listen() failed\n");



	//proxy_data_socket bind() listen()
	if((proxy_data_socket=socket(PF_INET,SOCK_STREAM,0))<0)printf("socket() failed\n");
	proxyaddress.sin_port=htons(accept_data_port);
	setsockopt(proxy_data_socket,SOL_SOCKET,SO_REUSEADDR,(const char*)&reuse,sizeof(reuse));
	if(bind(proxy_data_socket,(struct sockaddr *)&proxyaddress,sizeof(proxyaddress))<0)printf("proxy_data_socket bind() failed\n");
	if(listen(proxy_data_socket,5)<0)printf("listen() failed\n");

	memset(&serveraddress,0,sizeof(serveraddress));
	serveraddress.sin_family=AF_INET;
	serveraddress.sin_addr.s_addr=inet_addr("192.168.56.1");

	FD_SET(proxy_cmd_socket, &master_set);//Add proxy_cmd_socket to master_set
    FD_SET(proxy_data_socket, &master_set);//Add proxy_data_socket to master_set

    timeout.tv_sec = 6000;//Initial the timeout struct
    timeout.tv_usec = 0;

	while (1) {

        FD_ZERO(&working_set);
        memcpy(&working_set, &master_set, sizeof(master_set));//copy master_set to working_set
        selectResult = select(select_sd, &working_set, NULL, NULL, &timeout);//select the socket that read event happened, clear the socket that not happened
        if (selectResult < 0) {
        	perror("select() failed\n");
        	exit(1);
        }

        if (selectResult == 0) {
			continue;
        }
        int i;
        for (i = 0; i < select_sd; i++) {//Aim to find the socket then process instruction
			if (FD_ISSET(i, &working_set)) {//Judge whether i belong to working_set
                if (i == proxy_cmd_socket) {
					int k=0;
					for(k=0;k<3;k++){//Find the not used cmd_socket(accept or connect)
					if(accept_cmd_socket[k]==0)break;
					}
				memset(&clientaddress,0,sizeof(clientaddress));
				clientaddresslen=sizeof(clientaddress);
				if((accept_cmd_socket[k] = accept(proxy_cmd_socket,(struct sockaddr *)&clientaddress,&clientaddresslen))<0)printf("accept() failed\n");
				serveraddress.sin_port=htons(21);//Make proxy connect to server port 21
				if((connect_cmd_socket[k]=socket(PF_INET,SOCK_STREAM,0))<0)printf("socket() failed\n");//connect_cmd_socket
                if(connect(connect_cmd_socket[k],(struct sockaddr *)&serveraddress,sizeof(serveraddress))<0)printf("connect() failed\n");
                FD_SET(accept_cmd_socket[k], &master_set);
                FD_SET(connect_cmd_socket[k], &master_set);
				}

                int k;
                for(k=0;k<3;k++){//Find which accept_cmd_socket have information to read
                if (i == accept_cmd_socket[k]) {
                            char buff[BUFFSIZE];
                            memset(buff,'\0',BUFFSIZE);
                            char aa[250];
                            if (read(i, buff, BUFFSIZE) == 0) {//if read==0, close the cmd_socket, both accept and connect
                                    close(i);
                                    close(connect_cmd_socket[k]);
                                    FD_CLR(i, &master_set);
                                    FD_CLR(connect_cmd_socket[k], &master_set);
                                    connect_cmd_socket[k]=0;
                                    accept_cmd_socket[k]=0;
                                    printf("<Test> accept_cmd_socket closed\n");
                            }
                            else {
                                if(buff[0]=='R'&&buff[1]=='E'&&buff[2]=='T'&&buff[3]=='R'&&buff[4]==' '){//If receive the RETR
                                    int j=0;
                                    memset(aa,'\0',strlen(buff)-2);
                                    for(j=5;j<(strlen(buff)-2);j++){//get the file name
                                        aa[j-5]=buff[j];
                                    }
                                    if((fd=open(aa,O_RDWR))<0){//try to open this file in proxy
                                        fd=creat(aa,00600);//open() failed means no such a file, so we create it
                                        fileexist=2;//means proxy will read a file to cache
                                    }else{
                                        fileexist=1;//means proxy have this file in cache
                                        memset(buff,'\0',BUFFSIZE);
                                        strcpy(buff,"150 Opening data channel for file download from server of \"/");
                                        strcat(buff,aa);
                                        strcat(buff,"\"\r\n");//change the buff then write back to accept_cmd_socket
                                    }
                                }

                                if(buff[0]=='P'&&buff[1]=='O'&&buff[2]=='R'&&buff[3]=='T'){//If receive PORT details
                                    int j=0,num=0,a=0,b=0;
                                    char aaa[4]={'\0','\0','\0','\0'};
                                    char bbb[4]={'\0','\0','\0','\0'};
                                    for(j=0;j<strlen(buff);j++){//get the port number
                                            if(buff[j]==',')num++;
                                            if((num==4)&&(buff[j]!=','))aaa[strlen(aaa)]=buff[j];
                                            if((num==5)&&(buff[j]!=',')&&(buff[j]!=')'))bbb[strlen(bbb)]=buff[j];
                                    }
                                    a=atoi(aaa);
                                    b=atoi(bbb);
                                    connect_data_port=a*256+b;//calculate the port number
                                    memset(buff,'\0',BUFFSIZE);
                                    close(proxy_data_socket);//close the old socket and new a socket with port number+1
                                    FD_CLR(proxy_data_socket, &master_set);
                                    proxy_data_socket=0;
                                    printf("	 old socket has been closed\n 	 start establish new proxy_data_socket...\n");
                                    if((proxy_data_socket=socket(PF_INET,SOCK_STREAM,0))<0)printf("socket() failed\n");
                                    accept_data_port++;
                                    proxyaddress.sin_port=htons(accept_data_port);
                                    if(bind(proxy_data_socket,(struct sockaddr *)&proxyaddress,sizeof(proxyaddress))<0)printf("proxy_data_socket bind() failed\n");
                                    if(listen(proxy_data_socket,5)<0)printf("listen() failed\n");
                                    FD_SET(proxy_data_socket,&master_set);

                                    a=accept_data_port/256;
                                    b=accept_data_port%256;
                                    strcpy(buff,"PORT 192,168,56,101,");//change the buff with the proxy port
                                    sprintf(aaa,"%d",a);
                                    strcat(buff,aaa);
                                    buff[strlen(buff)]=',';
                                    sprintf(bbb,"%d",b);
                                    strcat(buff,bbb);
                                    strcat(buff,"\r\n");
                                }

                                if(fileexist==0||fileexist==2)write(connect_cmd_socket[k], buff, strlen(buff));//if file not exist or other request, normally write to connect_cmd_socket
                                else {//if file exist, transport this file from proxy to client
                                    write(accept_cmd_socket[k],buff,strlen(buff));//write the 150 opening data connection...
                                    if(pasv==0){//active mode
                                        serveraddress.sin_port=htons(connect_data_port);
                                        if((connect_data_socket=socket(PF_INET,SOCK_STREAM,0))<0)printf("socket() failed\n");//establish the connection
                                        proxyaddress.sin_port=htons(20);
                                        setsockopt(connect_data_socket,SOL_SOCKET,SO_REUSEADDR,(const char*)&reuse,sizeof(reuse));
                                        if(bind(connect_data_socket,(struct sockaddr *)&proxyaddress,sizeof(proxyaddress))<0)printf("connect_data_socket bind() failed 1\n");
                                        if(connect(connect_data_socket,(struct sockaddr *)&serveraddress,sizeof(serveraddress))<0)printf("connect_data_socket connect() failed\n");
                                        char buff[BUFFSIZE];
                                        memset(buff,'\0',BUFFSIZE);
                                        int readlen=0;
                                        for(;(readlen=read(fd,buff,BUFFSIZE))>0;){//read the file and then write to socket
                                            write(connect_data_socket,buff,readlen);
                                            memset(buff,'\0',BUFFSIZE);
                                        }
                                        char aaaa[300];
                                        memset(aaaa,'\0',300);
                                        strcpy(aaaa,"226 Successfully transferred \"/");//write 226 to accept_cmd_socket, it means transfer success
                                        strcat(aaaa,aa);
                                        strcat(aaaa,"\"\r\n");
                                        write(accept_cmd_socket[k],aaaa,strlen(aaaa));
                                        close(connect_data_socket);
                                        connect_data_socket=0;
                                        fileexist=0;//clear to 0
                                    }else{//passive mode
                                        if(accept_data_socket!=0){//Judge whether the data connection is already found, !=0 means it already exist
                                            char buff[BUFFSIZE];
                                            memset(buff,'\0',BUFFSIZE);
                                            int readlen;
                                            for(readlen=0;(readlen=read(fd,buff,BUFFSIZE))>0;){//read file and write to socket
                                                write(accept_data_socket,buff,readlen);
                                                memset(buff,'\0',BUFFSIZE);
                                            }
                                            char aaaa[400];
                                            memset(aaaa,'\0',400);
                                            strcpy(aaaa,"226 Successfully transferred \"/");//write 226 to accept_cmd_socket, it means transfer success
                                            strcat(aaaa,aa);
                                            strcat(aaaa,"\"\r\n");
                                            write(accept_cmd_socket[k],aaaa,strlen(aaaa));
                                            close(accept_data_socket);
                                            close(connect_data_socket);//close connect_data_socket because the socket is founded by proxy_data_socket, so we need close this two socket at the same time
                                            FD_CLR(accept_data_socket,&master_set);
                                            FD_CLR(connect_data_socket,&master_set);
                                            accept_data_socket=0;
                                            connect_data_socket=0;
                                            fileexist=0;
                                            pasv=0;//Clear to 0
                                        }else{//Data connection not found
                                            clientaddresslen=sizeof(clientaddress);
                                            if((accept_data_socket=accept(proxy_data_socket,(struct sockaddr *)&clientaddress,&clientaddresslen))<0)printf("accept() failed\n");
                                            char buff[BUFFSIZE];
                                            memset(buff,'\0',BUFFSIZE);
                                            int readlen;
                                            for(readlen=0;(readlen=read(fd,buff,BUFFSIZE))>0;){//read file and write to socket
                                                write(accept_data_socket,buff,readlen);
                                                memset(buff,'\0',BUFFSIZE);
                                            }
                                            char aaaa[400];
                                            memset(aaaa,'\0',400);
                                            strcpy(aaaa,"226 Successfully transferred \"/");//write 226 to accept_cmd_socket, it means transfer success
                                            strcat(aaaa,aa);
                                            strcat(aaaa,"\"\r\n");
                                            write(accept_cmd_socket[k],aaaa,strlen(aaaa));
                                            close(accept_data_socket);
                                            accept_data_socket=0;
                                            fileexist=0;
                                            pasv=0;//Clear to 0
                                        }
                                    }
                                    close(fd);//close the file
                                    fd=0;//fd clear to 0
                                }
                            }
                        }
                    }

                for(k=0;k<3;k++){//Find which connect_cmd_socket have information to read
                    if (i == connect_cmd_socket[k]) {
                        char buff[BUFFSIZE];
                        memset(buff,'\0',BUFFSIZE);
                        if(read(i,buff,BUFFSIZE)==0){//if read==0, close the cmd_socket, both accept and connect
                            close(i);
                            close(accept_cmd_socket[k]);
                            FD_CLR(i,&master_set);
                            FD_CLR(accept_cmd_socket[k],&master_set);
                            connect_cmd_socket[k]=0;
                            accept_cmd_socket[k]=0;
                            printf("	<Test> connect_cmd_socket closed\n");
                        }
                        else{
                            if(buff[0]=='2'&&buff[1]=='2'&&buff[2]=='7'){//Means passive mode, and we need to get the port number
                                printf("	Entering 227 PASV mode...\n");
                                pasv=1;//set pasv flag to 1
                                int j=0,num=0,a=0,b=0;
                                char aa[4]={'\0','\0','\0','\0'};
                                char bb[4]={'\0','\0','\0','\0'};
                                for(j=0;j<strlen(buff);j++){//get the port number
                                    if(buff[j]==',')num++;
                                    if((num==4)&&(buff[j]!=','))aa[strlen(aa)]=buff[j];
                                    if((num==5)&&(buff[j]!=',')&&(buff[j]!=')'))bb[strlen(bb)]=buff[j];
                                }
                                a=atoi(aa);
                                b=atoi(bb);
                                connect_data_port=a*256+b;
                                memset(buff,'\0',BUFFSIZE);

                                close(proxy_data_socket);//close the old socket and new a socket with port number+1
                                FD_CLR(proxy_data_socket, &master_set);
                                proxy_data_socket=0;
                                printf("	 old socket has been closed\n 	 start establish new proxy_data_socket...\n");

                                if((proxy_data_socket=socket(PF_INET,SOCK_STREAM,0))<0)printf("socket() failed\n");
                                accept_data_port++;
                                proxyaddress.sin_port=htons(accept_data_port);
                                setsockopt(proxy_data_socket,SOL_SOCKET,SO_REUSEADDR,(const char*)&reuse,sizeof(reuse));
                                if(bind(proxy_data_socket,(struct sockaddr *)&proxyaddress,sizeof(proxyaddress))<0)printf("proxy_data_socket bind() failed\n");
                                if(listen(proxy_data_socket,5)<0)printf("listen() failed\n");
                                FD_SET(proxy_data_socket,&master_set);

                                a=accept_data_port/256;
                                b=accept_data_port%256;
                                strcpy(buff,"227 Entering Passive Mode (192,168,56,101,");//change the buff with the proxy port
                                sprintf(aa,"%d",a);
                                strcat(buff,aa);
                                buff[strlen(buff)]=',';
                                sprintf(bb,"%d",b);
                                strcat(buff,bb);
                                strcat(buff,")\r\n");
                            }
                            write(accept_cmd_socket[k],buff,strlen(buff));//write buff to accept_cmd_socket
                        }
                    }
                }

                if (i == proxy_data_socket) {//accept() and connect() data connection
                    clientaddresslen=sizeof(clientaddress);
                    if((accept_data_socket=accept(proxy_data_socket,(struct sockaddr *)&clientaddress,&clientaddresslen))<0)printf("accept() failed\n");
                    serveraddress.sin_port=htons(connect_data_port);
                    if((connect_data_socket=socket(PF_INET,SOCK_STREAM,0))<0)printf("socket() failed\n");
                    if(pasv==0){//if active mode, it should use 20 port to connect to client
                        proxyaddress.sin_port=htons(20);
                        setsockopt(connect_data_socket,SOL_SOCKET,SO_REUSEADDR,(const char*)&reuse,sizeof(reuse));
                        if(bind(connect_data_socket,(struct sockaddr *)&proxyaddress,sizeof(proxyaddress))<0)printf("connect_data_socket bind() failed 2\n");
                    }
                    if(connect(connect_data_socket,(struct sockaddr *)&serveraddress,sizeof(serveraddress))<0)printf("connect_data_socket connect() failed\n");
                    FD_SET(accept_data_socket, &master_set);
                    FD_SET(connect_data_socket, &master_set);
                }

                if (i == accept_data_socket) {//receive data and store data in cache if needed
                    char buff[BUFFSIZE];
                    memset(buff,'\0',BUFFSIZE);
                    int readlen;
                    if ((readlen=read(i, buff, BUFFSIZE)) == 0) {//if the receive data length==0 ,close the connection
                        if(fileexist==2){//if it is transferring a file, close the file
                            close(fd);
                            fd=0;
                        }
                        fileexist=0;
                        close(i);
                        close(connect_data_socket);
                        FD_CLR(i, &master_set);
                        FD_CLR(connect_data_socket, &master_set);
                        connect_data_socket=0;
                        accept_data_socket=0;
                        pasv=0;
                        printf("<Test> accept_data_socket close\n");
                    }
                    else {//deliver to connect_data_socket
                        if(fileexist==2){//If it is a file information, store the file in cache
                            write(fd,buff,readlen);
                        }
                        write(connect_data_socket, buff, readlen);
                        }
                    }

                if (i == connect_data_socket) {//receive data and store data in cache if needed
                    char buff[BUFFSIZE];
                    memset(buff,'\0',BUFFSIZE);
                    int readlen=0;
                    if ((readlen=read(i, buff, BUFFSIZE)) == 0) {//if the receive data length==0 ,close the connection
                        if(fileexist==2){//if it is transferring a file, close the file
                            close(fd);
                            fd=0;
                            }
                        fileexist=0;
                        close(i);
                        close(accept_data_socket);
                        FD_CLR(i, &master_set);
                        FD_CLR(accept_data_socket, &master_set);
                        connect_data_socket=0;
                        accept_data_socket=0;
                        pasv=0;
                        printf("	<Test> accept_data_socket close\n");
                    }else {//deliver to accept_data_socket
                        write(accept_data_socket, buff, readlen);
                        if(fileexist==2){//If it is a file information, store the file in cache
                            write(fd,buff,readlen);
                        }
                    }
                }
            }
        }
    }
    return 0;
}
