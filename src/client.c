#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

// Signal handler that does not allow CTRL+C ,forcing the client to exit with END. //
// This will force the client to play the game with the server,and will secure that END is send to the server. // 
void signalHandler()
{
	printf( "\nPlease enter END to disconnect:\n" ); 
	return;
}

int main(int argc, char **argv)
{
    int sockfd, portno, n, i;
    int gamecount = 0;
    int servernumber;
    char servernum[3] = {0};
    char line[1024], checkfail[8], c;
    struct sockaddr_in serv_addr;
	
	signal(SIGINT, signalHandler);
	srand(getpid());
	
	// Argument count checking. //	
    if ( argc < 3 )
	{
		fprintf(stderr, "No ip address or port has been provided.\nUsage: %s <ip-address> <port>\n", argv[0]);
		exit(1);
	}
	
	// Socket Creation. //
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if( sockfd < 0 )
	{
		error("Error: Socket could not be created\n");
	}	
	
	// Socket Connection //
	portno = atoi(argv[2]);
    bzero((char*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr=inet_addr(argv[1]);
    serv_addr.sin_port = htons(portno);	
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
	{
		error("Error: Connection could not be established");
	}			
	
	// Shell Promt //
	printf("%s_>", argv[1]);
	bzero(line, 1024);
	fgets(line, 1023, stdin);
	line[strcspn(line, "\n")] = 0;
	n = write(sockfd, line, strlen(line));
	if( n < 0 )
	{
		error("Error: Could not write to socket");
	}	
	while( strcmp(line,"END")!=0 )
	{
		// Read all the content in sockfd , till server's delimiter ' | ' is found. //
		// We can't be sure about the size of buffer we need to get the executed command's output (for example, ls -l in a folder with many documents). //
		// Instead we read and print char by char. //
		i = 0;
		while(1)
		{
			n = read(sockfd, &c, 1);
			if( n == 0 )
			{
				puts( "The server is no longer running." );
				puts( "I will now exit." );
				close(sockfd);
				return -1;
			}
			if( n < 0 )
			{
				error("Error: Could not read from socket");
			}
			if( i < 7 )
			{
				checkfail[i] = c;
				i++;
			}	
			if( c == '|' )
			{
				break;
			}	
			putchar(c);
		}
		// Check if "execvp:" exists in input which indicates unsuccessful execution. //
		checkfail[i] = '\0';
		if ( strcmp(checkfail,"execvp:")!=0 )
		{
			gamecount+=1;
		}		
		printf("%s_>", argv[1]);
		bzero(line, 1024);
		fgets(line, 1023, stdin);
		line[strcspn(line, "\n")] = 0;
		n = write(sockfd, line, strlen(line));		
		if( n < 0 )
		{
			error("Error: Could not write to socket");
		}	
	}
	// Play the simple game with server. //
	if( gamecount==0 )
	{
		puts( "No succesful commands have been executed." );
		puts( "No game can be played." );
		close(sockfd);
		return 0;
	}	
	// Array with n size is created, where n is the number of succesfully executed commands. // 
	int *gamenumbers = malloc(gamecount * sizeof(int));
	if( gamenumbers == NULL )
	{
		error("malloc");
	}	
	// Calculate and show client's numbers. //
	printf( "My numbers are:");	
	for( i=0;i<gamecount;i++ )
	{
		gamenumbers[i] = 1 + (rand()%20);
		printf(" %d ", gamenumbers[i]);
	}
	// Print a dot and a new line for clarity. //
	printf( "." );
	puts( "" );
	n = read(sockfd, servernum, 2); 
	servernumber = atoi(servernum);
	for( i=0;i<gamecount;i++ )
	{
		if( gamenumbers[i] == servernumber )
		{
			printf( "I, the client, won because the server's number belongs in my number list!\nHis number was %d.\n",servernumber);
			puts( "I will now exit." );
			close(sockfd);
			return 0;
		}
	}
	printf( "Oh no, the server won!\nHis number , %d, does not belong in my number list!\n",servernumber); 
	puts( "I will now exit." );
    close(sockfd);
	return 0;
}	
