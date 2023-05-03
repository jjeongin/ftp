#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h> 
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_BUFFER 200 // max buffer size
#define MAX_COMMAND 100 // max command size
#define MAX_RESPONSE 100 // max response size

int main()
{
	int server_sd;
	server_sd = socket(AF_INET, SOCK_STREAM, 0); // socket(domain/family of the socket, type of socket,  protocol for connection)
	if(server_sd < 0)
	{
		perror("Error: opening socket");
		exit(1);
	}
    int port_no = 21; // server port number for control channel
	struct sockaddr_in server_addr;
    bzero((char *) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short) port_no);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    socklen_t socket_len = sizeof(server_addr);
    if (connect(server_sd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error: connect");
        exit(-1);
    }

	char buffer[MAX_BUFFER]; // user command
    char command[MAX_COMMAND]; // command (LIST, PWD, CWD, etc.)
	char response[MAX_RESPONSE]; // response from server
	while (1) { // repeat
        bzero(buffer, sizeof(buffer)); // place null bytes in the buffer
        bzero(command, sizeof(command));

		printf("ftp> ");
		fgets(buffer, sizeof(buffer), stdin); // get user input and store it in the buffer
        if (buffer[0] == '!') { // execute if local command
            /* 
             * parse user input into command and argument
             */
            char * delim = "\t\r\n ";
            char * args = strtok(buffer, delim); // first argument
            strcpy(command, args); // copy first argument into command
            if (args != NULL) {
                args = strtok(NULL, delim); // second argument
            }
            
            if (strcmp(command, "!LIST") == 0) {
                system("ls");
            }
            else if (strcmp(command, "!CWD") == 0) {
                if (chdir(args) < 0) {
                    perror("Error: chdir() failed");
                }
                // system("cd %s", args);
            }
            else if (strcmp(command, "!PWD") == 0) {
                system("pwd");
            }
        }
        else {
            send(server_sd, buffer, sizeof(buffer), 0); // send command to server
            bzero(response, sizeof(response));
            recv(server_sd, &response, sizeof(response), 0); // recieve output from the server
            if (strcmp(response, "221 Service closing control connection.") == 0) { // close connection and terminate the program
                break;
            }
            else
                printf("%s\n", response);
        }
	}
	
	// close socket
	close(server_sd);
	return 0;
}