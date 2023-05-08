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
#define MAX_FILENAME 100 // max file name
#define DATA_SIZE 1024

void send_file(FILE* fp, int server_sd, char* buffer){
    send(server_sd, buffer, MAX_BUFFER, 0);
    char data[DATA_SIZE] = {0};
    while(fgets(data,DATA_SIZE, fp) != NULL) {
        
        //printf("Sending message: -%s-\n", data);
        if (send(server_sd, data, sizeof(data), 0) == -1) {
            
            perror("Error: sending file.");
            exit(1);
        }
    bzero(data, DATA_SIZE);
  }
  send(server_sd, "\0", 1, 0);
}

// void write_file(int socket, char* filename) {
//     //writing into file the data from the client
//     int n;
//     FILE *fp;
//     char buffer[DATA_SIZE] = {0};
//     fp = fopen(filename, "w");
//     if(NULL == fp)
//     {
//        	printf("Error opening file");
//         return;
//     }
//     while (1) {
//         n = recv(socket, buffer, sizeof(buffer), 0);
//         if (strncmp(buffer, "\0", 1) == 0){ // fix later
//             break;
//         } 
//         fputs(buffer, fp);
//         bzero(buffer, DATA_SIZE);
//     }
//     fclose(fp);
// }

int main()
{
	int client_sd;
	client_sd = socket(AF_INET, SOCK_STREAM, 0); // socket(domain/family of the socket, type of socket,  protocol for connection)
	if (client_sd < 0)
	{
		perror("Error: opening socket");
		exit(1);
	}
    int server_port = 21; // server port number for control channel
	struct sockaddr_in server_addr;
    bzero((char *) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short) server_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    socklen_t socket_len = sizeof(server_addr);
    if (connect(client_sd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error: connect");
        exit(-1);
    }
    
    // get client IP and Port number
    char client_ip[16];
    unsigned int client_port;
    unsigned int data_port = client_port + 1; // port number for data channel
    struct sockaddr_in client_addr;
    int addr_size = sizeof(client_addr);
    getsockname(client_sd, (struct sockaddr *) &client_addr, &addr_size);
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    client_port = ntohs(client_addr.sin_port);
    printf("Client IP: %s\n", client_ip); // TEST
    printf("Client Port: %d\n", client_port); // TEST

    FILE *fp;
    char filename[MAX_FILENAME]; //filename
	char buffer[MAX_BUFFER]; // user command
    char command[MAX_COMMAND]; // command (LIST, PWD, CWD, etc.)
	char response[MAX_RESPONSE]; // response from server
	while (1) { // repeat
        bzero(buffer, sizeof(buffer)); // place null bytes in the buffer
        bzero(command, sizeof(command));
		printf("ftp> ");
		fgets(buffer, sizeof(buffer), stdin); // get user input and store it in the buffer
        if (buffer[0] == '!') { // client command
            // parse user input to command and args
            char * delim = "\t\r\n ";
            char * args = strtok(buffer, delim); // first argument
            strcpy(command, args); // copy first argument into command
            if (args != NULL) {
                args = strtok(NULL, delim); // second argument
            }
            // execute client command
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
            else {
                printf("202 Command not implemented.\n");
            }
        }
        else if (strncmp(buffer, "STOR", 4) == 0) {
            char copy_buffer[MAX_BUFFER];
            strcpy(copy_buffer, buffer);
            char * delim = "\t\r\n ";
            char * args = strtok(buffer, delim); // first argument
            strcpy(command, args); // copy first argument into command
            if (args != NULL) {
                args = strtok(NULL, delim); // second argument
            }
            strcpy(filename, args);
            fp = fopen(filename, "r");
            if (fp == NULL){
                perror("Error: reading file");
                exit(1);
            }
            send_file(fp, client_sd, copy_buffer);
            fclose(fp);
            bzero(response, sizeof(response));
            recv(client_sd, &response, sizeof(response), 0); // recieve output from the server
            printf("%s\n", response);
        }
        else if (strncmp(buffer, "RETR", 4) == 0) {
            // fp = fopen(filename, "w");
            // if (fp == NULL){
            //     perror("Error: opening file");
            //     exit(1);
            // }
            // write_file(server_sd, fp);
            // fclose(fp);
        }
        else if (strncmp(buffer, "LIST", 4) == 0) {
            // // PORT command
            // // replace all . with , in client IP
            // char * current_pos = strchr(client_ip, '.');
            // while (current_pos) {
            //     * current_pos = ',';
            //     current_pos = strchr(current_pos, '.');
            // }
            // // convert port to p1 and p2
            // int p1 = data_port / 256;
            // int p2 = data_port % 256;
            // char port_command[MAX_COMMAND];
            // sprintf(port_command, "PORT %s,%d,%d", client_ip, p1, p2);
            // // send PORT command to server
            // send(client_sd, port_command, sizeof(port_command), 0);
            // // open new data connection
            // int data_sd = socket(AF_INET, SOCK_STREAM, 0);
            // if (data_sd < 0)
            // {
            //     perror("Error opening socket");
            //     exit(-1);
            // }
            // // set socket option
            // int value  = 1;
            // setsockopt(data_sd, SOL_SOCKET, SO_REUSEADDR, (const void *) &value, sizeof(value));
            // // build data address
            // struct sockaddr_in data_addr;
            // bzero((char *) &data_addr, sizeof(data_addr));
            // data_addr.sin_family = AF_INET;
            // data_addr.sin_port = htons((unsigned short) data_port);
            // data_addr.sin_addr.s_addr = INADDR_ANY;
            // // bind
            // if (bind(data_sd, (struct sockaddr *) &data_addr, sizeof(data_addr)) < 0)
            // {
            //     perror("bind failed");
            //     exit(-1);
            // }
            // // listen
            // if (listen(data_sd, 5) < 0) {
            //     printf("Listen failed..\n");
            //     exit(-1);
            // }

            // // recieve output from the server and check if success
            // bzero(response, sizeof(response));
            // recv(client_sd, &response, sizeof(response), 0);
            // printf("%s\n", response);

            // // after the data transfer, increment data port by 1 for the future connection
            // data_port++;
            send(client_sd, buffer, sizeof(buffer), 0); // send command to server
            bzero(response, sizeof(response));
            recv(client_sd, &response, sizeof(response), 0); // recieve output from the server
            printf("%s\n", response);
            if (strcmp(response, "221 Service closing control connection.") == 0) { // close connection and terminate the program
                break;
            }
        }
        else {
            send(client_sd, buffer, sizeof(buffer), 0); // send command to server
            bzero(response, sizeof(response));
            recv(client_sd, &response, sizeof(response), 0); // recieve output from the server
            printf("%s\n", response);
            if (strcmp(response, "221 Service closing control connection.") == 0) { // close connection and terminate the program
                break;
            }
        }
	}
	
	// close socket
	close(client_sd);
	return 0;
}