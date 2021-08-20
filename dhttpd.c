/*
Copyright 2021, dettus@dettus.net

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this 
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation 
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE 
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#define MAX 80
#define PORT 8080
#define SA struct sockaddr

void printlicense()
{
	
	printf("-------------------------------------------------------------------------------\n");	
	printf("Copyright 2021, dettus@dettus.net\n");
	printf("\n");
	printf("Redistribution and use in source and binary forms, with or without modification,\n");
	printf("are permitted provided that the following conditions are met:\n");
	printf("\n");
	printf("1. Redistributions of source code must retain the above copyright notice, this \n");
	printf("list of conditions and the following disclaimer.\n");
	printf("\n");
	printf("2. Redistributions in binary form must reproduce the above copyright notice, \n");
	printf("this list of conditions and the following disclaimer in the documentation \n");
	printf("and/or other materials provided with the distribution.\n");
	printf("\n");
	printf("THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\" AND\n");
	printf("ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED \n");
	printf("WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE \n");
	printf("DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE \n");
	printf("FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL \n");
	printf("DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR \n");
	printf("SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER \n");
	printf("CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, \n");
	printf("OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE \n");
	printf("OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n\n");
	printf("-------------------------------------------------------------------------------\n");	

}

void printprefix(int connfd)
{
	time_t t;
	struct tm* now;
	t=time(NULL);
	now=gmtime(&t);
	printf("%05d> ",(int)connfd);
	printf("%04d-%02d-%02d %02d:%02d:%02d  ",1900+now->tm_year,now->tm_mon+1,now->tm_mday,  now->tm_hour,now->tm_min,now->tm_sec);

}

char allowedfilesfilename[1024];

char* findrequestfile(char* requestbuf,int requestbuflen)
{
	int i;
	int state;
	int tokencnt;
	char* retval=NULL;
	state=0;
	tokencnt=0;
	for (i=0;i<requestbuflen;i++)
	{
		char c;
		c=requestbuf[i];
		if (c<=' ' || (c=='?' && state==1)) 
		{
			requestbuf[i]=0;
			c=0;
		}
		if (c==0 && state==1)
		{
			state=0;
		}
		if (c!=0 && state==0)
		{
			state=1;
			tokencnt++;
			if (tokencnt==2)
			{
				retval=&requestbuf[i];
			}
		}
	}
	if (tokencnt<3) retval=NULL;
	return retval;
}

int findreply(char* mimetype,char* filename,char* requestfile)
{
	int i;
	int l;
	char line[1024];
	char *token;
	char *line_mimetype;
	char *line_filename;
	int found;
	FILE *f;
	
	
	f=fopen(allowedfilesfilename,"rb");
	mimetype[0]=0;
	filename[0]=0;
	found=0;
	while (!feof(f) && !found)
	{
		int tokencnt;
		if (fgets(line,sizeof(line),f)==NULL) break;
		l=strlen(line);
		if (line[0]=='#') l=0;
		for (i=1;i<l;i++)
		{
			if (line[i]=='#' && line[i-1]!='\\') l=i;
		}
		token=NULL;
		tokencnt=0;
		token=&line[0];
		for (i=0;i<l && !found;i++)
		{
			char c;
			c=line[i];
			if (c<=' ') 
			{
				line[i]=0;
				tokencnt++;
				switch(tokencnt)
				{
					case 0:
						break;
					case 1:
						line_mimetype=token;
						break;
					case 2:
						line_filename=token;
						break;
					default:
						if (token)
						{
							if (found==0 && atoi(token)==404)
							{
								memcpy(mimetype,line_mimetype,strlen(line_mimetype)+1);
								memcpy(filename,line_filename,strlen(line_filename)+1);
							}
							else if (requestfile!=NULL)
							{
								int l1,l2;
								l1=strlen(requestfile);
								l2=strlen(token);
								if (l1==l2)
								{
									if (strncmp(requestfile,token,l1)==0)
									{
										memcpy(mimetype,line_mimetype,strlen(line_mimetype)+1);
										memcpy(filename,line_filename,strlen(line_filename)+1);
										found=1;
									}
								}
							}
						}
						break;
				}
				token=NULL;
			} else if (token==NULL) token=&line[i];
		}
	}
	fclose(f);
	return found;
}
void * reply_func(void * arg)
{
	int n;
	int* pThis=(int*)arg;
	int connfd;
	char requestbuf[1024];
	int requestbufidx;

	char *requestfile;
	char replyheader[64];
	char mimetype[64];
	char filename[1024];
	int contentsize;
	int done;

	char replybuf[1024];

	connfd=*pThis;

	requestbufidx=0;
	done=0;
	do
	{
		n=read(connfd, &requestbuf[requestbufidx], sizeof(requestbuf)-requestbufidx);
		if (n>=0) 
		{
			int i;
			requestbufidx+=n;
			for (i=0;i<requestbufidx;i++)
			{
				if (requestbuf[i]<' ') done=1;
			}
		}
		if (n==0) done=1;
	} while (!done);
	printprefix(connfd);

	requestfile=findrequestfile(requestbuf,requestbufidx);
	if (requestfile)
	{
		printf("REQUEST [%s] okay  ",requestfile);
	}
	else
	{
		printf("unknown request  ");
	}
	mimetype[0]=filename[0]=0;
	if (findreply(mimetype,filename,requestfile))
	{
		snprintf(replyheader,sizeof(replyheader)-1,"HTTP/1.1 200 OK");
		printf("200 ");
	}
	else
	{
		snprintf(replyheader,sizeof(replyheader)-1,"HTTP/1.1 404 Not Found");
		printf("404 ");
	}
	printf("[%s] [%s]\n",filename,mimetype);

	if (filename[0])
	{
		int idx;
		FILE *f;

		f=fopen(filename,"rb");
		if (f)
		{
			fseek(f,0,SEEK_END);
			contentsize=ftell(f);
			fseek(f,0,SEEK_SET);
			idx=0;

			snprintf(replybuf,sizeof(replybuf)-idx,"%s\r\nContent-Length: %d\r\nContent-Type: %s\r\n\n",replyheader,contentsize,mimetype);
			idx=strlen(replybuf);
			#define	CHUNKSIZE	sizeof(replybuf)
			do	
			{
				int n;
				int m;
				n=fread(&replybuf[idx],sizeof(char),CHUNKSIZE-idx,f);
				n+=idx;
				idx=0;
				m=0;
				do
				{
					m=write(connfd,&replybuf[idx],n-idx);
					idx+=m;
				}
				while (m && idx<n);
				idx=0;
			}
			while (!feof(f));
	
			fclose(f);
		} else {
			printf("<403> file [%s] not found\n",filename);
			exit(1);
		}
	} else {
		printf("ERROR! 404 not given!\n");
		snprintf(replybuf,sizeof(replybuf)-1,"HTTP/1.1 404 Not Found\nContent-Length: 37\nContent-Type: text/html\n\n\n\n<html>\n404 Coming soon...\n</html>\n\n");
		snprintf(mimetype,sizeof(mimetype)-1,"text/html");
	}
	fflush(stdout);

/*	
	
	send_idx=0;
	while (send_idx<replysize)
	{
		n=write(connfd,&replybuf[send_idx],replysize-send_idx);
		if (n<=0) break;
		send_idx+=n;
	}
*/
	close(connfd);
	return NULL;
}
// Driver function
int main(int argc,char** argv)
{
	int sockfd;
	unsigned int len;
	struct sockaddr_in servaddr, cli;
	int port;
	printlicense();


	if (argc!=3)
	{
		printf("please run with %s PORT ALLOWEDFILES.txt\n",argv[0]);
		printf(" for example: %s 8080 helloworld/allowedfiles.csv\n",argv[0]);
		return 1;
	}
	port=atoi(argv[1]);
	memcpy(allowedfilesfilename,argv[2],strlen(argv[2])+1);

	// socket create and verification
	sockfd = socket(AF_INET, SOCK_STREAM,  0);
	if (sockfd == -1) {
		printf("socket creation failed...\n");
		exit(0);
	}
	else
		printf("Socket successfully created..\n");
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	// Binding newly created socket to given IP and verification
	if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
		printf("bind() failed\n");
		exit(0);
	}

	// Now server is ready to listen and verification
	if ((listen(sockfd, 5)) != 0) {
		printf("isten() failed\n");
		exit(0);
	}
	else
		printf("%s ready\n",argv[0]);
	len = sizeof(cli);
	while (1)
	{
		pthread_t thread_id;
		int ret;
		int connfd;
		// Accept the data packet from client and verification
		connfd = accept(sockfd, (SA*)&cli, &len);
		if (connfd < 0) {
			printf("acccept() failed\n");
			exit(0);
		}
		else
		{
			struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&cli;
		//	struct in_addr ipAddr = pV4Addr->sin_addr;



			printprefix(connfd);
			printf("CONNECTION  ");
			printf("%d.%d.%d.%d\n",
					(int)(pV4Addr->sin_addr.s_addr&0xFF),
					(int)((pV4Addr->sin_addr.s_addr&0xFF00)>>8),
					(int)((pV4Addr->sin_addr.s_addr&0xFF0000)>>16),
					(int)((pV4Addr->sin_addr.s_addr&0xFF000000)>>24));
			fflush(stdout);


			ret=pthread_create(
					&thread_id,
					NULL,
					reply_func,
					(void*)&connfd);
			if (ret!=0) 
			{
				printf("error from pthread:%d\n",ret);
				return 1;
			}
			fflush(stdout);
		}
		usleep(10000);
		// After chatting close the socket
	}
}
