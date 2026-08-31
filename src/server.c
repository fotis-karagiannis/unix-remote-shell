#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

// Signal handler for CTRL+C. //
void signalHandler(int signum)
{
	printf( "\nTermination signal has been caught.\n");
	exit(signum);
}

void parse(char *vector[10], char *line)
{

    int i;

    char *pch;
    pch = strtok (line," ");
    i=0;
    while( pch != NULL )
    {
        vector[i]=pch;
        pch = strtok (NULL, " ");
        i++;
    }
    vector[i]=NULL;
}

void runpipe(int pfd[],char *cmd1,char *cmd2)
{
    int pid,status;
	char *vector[10];
	 
	if( (pid=fork())==-1 )
	{
		error("fork");
	}
	else if( pid == 0 )
	{
		cmd2[strcspn(cmd2, "\n")] = 0;
		parse(vector, cmd2);
		dup2(pfd[0], 0);
		close(pfd[1]);
		// The child doesn't need this end of the pipe. //
		execvp(vector[0],vector);
		// Line below is executed only on exec failure. //
		error("execvp");
	}
	else
	{
		cmd1[strcspn(cmd1, "\n")] = 0;
		parse(vector, cmd1);
		dup2(pfd[1],1);
		close(pfd[0]);
		// The parent doesn't need this end of the pipe. //
		execvp(vector[0], vector);
		// Line below is executed only on exec failure. //
		error("execvp");
	}	
}

int main(int argc, char **argv)
{
	int sockfd, newsockfd, portno, n, status, status2, i, pfd[2];
	int pipepos = -1;
	int gamenumber;
	char gamenum[3];
	char buffer[1024],str[INET_ADDRSTRLEN],line[1024],*vector[10],cmd1[1024],cmd2[1024];
	pid_t pid,pid2;
	struct sockaddr_in serv_addr, cli_addr;
	socklen_t clilen;
	pipe(pfd);
	
	// String delimiter set to "|", so that client can identify when execvp output has finished. //
	// Not to be confused with pipe symbol. //
	char *delim = "|";
	
	signal(SIGINT, signalHandler);
	srand(getpid());
	
	// Argument count checking. //
	if( argc < 2 )
	{
		fprintf(stderr, "No port has been provided.\nUsage: %s <port>\n",argv[0]);
		exit(1);
	}	
	
	// Socket Creation. //
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if( sockfd < 0 )
	{
		error( "Error: Socket could not be created.\n" );
	}
	
	// Socket Binding. //
	portno = atoi(argv[1]);
	bzero((char*)&serv_addr,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	if( bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0 )
	{
		error( "Error: Binding the created socket has failed" );
	}	
	
	// Listen & Accept. //
	listen(sockfd, 5);	
	for(;;)
	{	
		clilen = sizeof(cli_addr);
		puts( "Listening for connections!" );
		newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
		if( newsockfd < 0 )
		{
			error( "Error: A problem occured while accepting the connection");
		}		
		
		if( (pid = fork()) == -1 )
		{
			close(newsockfd);
			continue;
		}
		// Child proccess handling the client. //
		else if( pid == 0 ) 
		{	
			// Get client IP. //
			if (inet_ntop(AF_INET, &cli_addr.sin_addr, str, INET_ADDRSTRLEN) == NULL) 
			{
				fprintf(stderr, "Could not convert byte to address\n");
				exit(1);
			}
			fprintf(stdout, "A new client connected : %s\n", str);
			
			// Scan Loop. //
			bzero(line, 1024);
			n = read(newsockfd, line, 1023);
			if (n < 0)
			{	
				error("Error: Could not read from socket");
			}				
			while( strcmp(line,"END")!=0 )
			{
				if( (pid2=fork()) == -1 )
				{
					error("fork");
				}	
				else if( pid2 == 0 )
				{
					for( i=0;i<strlen(line);i++)
					{
						if( line[i]=='|' )
						{
							pipepos = i;
						}	
					}
					if( pipepos == -1 )
					{
						// Simple  command executing. //
						parse(vector,line);
						printf("Executing: %s\n",line);
						
						// Redirect stdout of server to socket //
						dup2(newsockfd, STDOUT_FILENO);
						dup2(newsockfd, STDERR_FILENO);
						execvp(vector[0],vector); /*Execute command*/
						// Line below will only be executed on execvp failure. //
						error("execvp");
					}
					else
					{	
						// Pipe command executing. //
						// Seperate the string in 2 strings. //
						for( i=0;i<pipepos-1;i++)
						{
							cmd1[i] = line[i];
						}
						for( i=0;i<strlen(line);i++)
						{
							cmd2[i] = line[(pipepos+2+i)];
						}		
						printf("Executing: %s\n",line);						
						runpipe(pfd, cmd1, cmd2);
						exit(0);
					}	
				}
				else if( pid2 > 0)
				{
					while(wait(&status2)!=pid2);
					n = write(newsockfd, delim, strlen(delim));
					puts( "Current command execution finished." );
					bzero(line, 1024);
					n = read(newsockfd, line, 1023);
					if(n < 0)
					{	
						error("Error: Could not read from socket");
					}	
					continue;
				}			
			}
			puts( "Client finished." );
			// Generate the random number,convert to string and write it to socket. //
			// 3 slots are used for the string that will hold the integer because the number can be consisted of 2 digits (plus 1 for \0). //
			gamenumber = 1 + (rand()%20);
			bzero(gamenum,3);
			sprintf( gamenum, "%d", gamenumber );
			n = write(newsockfd, gamenum, 2);
			if( n < 0 )
			{
				error("Error: Could not write to socket");
			}	
			
			// Close the connection. //	
			close(newsockfd);
			exit(0);
		}
		else if( pid > 0 )
		{
			while(wait(&status)!=pid);
			
			close(newsockfd);
			continue;
		}
	}	
	close(sockfd);
	
	return 0;
}	