#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_BUFFER 200 // max buffer size
#define MAX_COMMAND 100 // max command size
#define MAX_RESPONSE 100 // max response size
#define MAX_USERS 100
#define MAX_USERNAME 300 // max username length
#define MAX_PASSWORD 300
#define DATA_SIZE 1024

void write_file(int socket, char* filename) {
    // writing into file the data from the client
    int n;
    FILE *fp;
    char buffer[DATA_SIZE] = {0};
    fp = fopen(filename, "w");
    if(NULL == fp)
    {
       	printf("Error opening file");
        return;
    }
    while (1) {
        n = recv(socket, buffer, sizeof(buffer), 0);
        if (strncmp(buffer, "\0", 1) == 0){ // fix later
            break;
        } 
        fputs(buffer, fp);
        bzero(buffer, DATA_SIZE);
    }
    fclose(fp);
}

//funtion to send file that was downloaded
void send_file(FILE* fp, int sd){
    char data[DATA_SIZE] = {0};
    while(fgets(data,DATA_SIZE, fp) != NULL) {
      //  printf("Sending message: -%s-\n", data);
        if (send(sd, data, sizeof(data), 0) == -1) {
            perror("Error: sending file.");
            break;
        }
    bzero(data, DATA_SIZE);
    }
  send(sd, "\0", 1, 0);
}

int main()
{
    /*
     * socket: create the parent socket 
     */
	int server_sd = socket(AF_INET,SOCK_STREAM,0);
	if (server_sd < 0)
	{
		perror("Error opening socket");
		exit(-1);
	}
	/* 
     * setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
     */
	int value  = 1;
	setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, (const void *) &value, sizeof(value));
    /*
     * build the server's internet address
     */
    int port_no = 21; // server port number for control channel
	struct sockaddr_in server_addr, new_addr;
	bzero((char *) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short) port_no);
	server_addr.sin_addr.s_addr = INADDR_ANY;
    /*
     * bind: associate the parent socket with a port
     */
	if (bind(server_sd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
	{
		perror("bind failed");
		exit(-1);
	}
    /*
     * listen: make this socket ready to accept connection requests
     */
    if (listen(server_sd, 5) < 0) {
        printf("Listen failed..\n");
        exit(-1);
    }
    /*
     * read registered username, passwords from users.txt
     */
    FILE *users_fp;
    char *line = NULL;
    size_t len = 0;
    users_fp = fopen("users.txt", "r");
    if (users_fp == NULL) {
        printf("Error: cannot open users.txt\n");
        exit(-1);
    }
    char usernames[MAX_USERS][MAX_USERNAME]; // username list
    char passwords[MAX_USERS][MAX_PASSWORD]; // password list
    int num_users = 0; // total number of users
    while (getline(&line, &len, users_fp) != -1) { // read from users.txt
        char * token = strtok(line, "\t\r\n ");
        strcpy(usernames[num_users], token);
        token = strtok(NULL, "\t\r\n ");
        strcpy(passwords[num_users], token);
        num_users++;
    }
    fclose(users_fp); // close users.txt file
    if (line) {
        free(line);
    }
    // TEST - print out all existing usernames, passwords
    for (int i = 0; i < num_users; i++) {
        printf("username: \"%s\"\n", usernames[i]);
        printf("password: \"%s\"\n", passwords[i]);
    }
    /*
     * GLOBAL VARIABLES
     */
    char buffer[MAX_BUFFER]; // user input from client
    char command[MAX_COMMAND];
    char * args;
    char response[MAX_RESPONSE]; // response to client
    int login_status[FD_SETSIZE]; // keep track of login status of users
    for (int i = 0; i < FD_SETSIZE; i++) {
        login_status[i] = 0;
    }
    int user_i = 0; // user index of current user (< num_users)

    char filename[MAX_BUFFER];
    int new_socket;
    socklen_t addr_size;
    /*
     * for select()
     */
    fd_set current_sockets, ready_sockets;
    FD_ZERO(&current_sockets);
    FD_SET(server_sd, &current_sockets);
    int max_sd = server_sd;
	while (1)
	{
        ready_sockets = current_sockets; // select is destructive
        if (select(max_sd + 1, &ready_sockets, NULL, NULL, NULL) < 0)
		{
			perror("Error: select failed");
            exit(-1);
		}
        
        for (int sd = 0; sd < max_sd + 1; sd++) {
            if (FD_ISSET(sd, &ready_sockets)) {
                if (sd == server_sd) { // if something happened in the server socket, then its an incoming connection
                    // accept a new conection from client
                    struct sockaddr_in client_addr;
                    int addrlen = sizeof(client_addr);
                    char client_ip[16];
                    unsigned int client_port;
                    int client_sd = accept(server_sd, (struct sockaddr *) &client_addr, (socklen_t *) &addrlen);
                    if (client_sd < 0) {
                        printf("Error: accept failed\n");
                        exit(EXIT_FAILURE);
                    }
                    FD_SET(client_sd, &current_sockets); // add new client socket to current_sockets
                    if (client_sd > max_sd) { // update max_sd
                        max_sd = client_sd;
                    }

                    // print client IP and Port number for this connection
                    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
                    client_port = ntohs(client_addr.sin_port);
                    printf("Client IP: %s\n", client_ip);
                    printf("Client Port: %d\n", client_port);
                }
                else {
                    bzero(buffer, sizeof(buffer)); // place null bytes in the buffer
                    if (read(sd, (void *) &buffer, sizeof(buffer)) < 0) { // read from buffer
                        printf("Error: read failed\n");
                        exit(EXIT_FAILURE);
                    }
                    if (strcmp(buffer, "") != 0) { // if buffer is not empty
                        //printf("From client_sd %d: \"%s\" \n", sd, buffer); // TEST
                        bzero(command, sizeof(command));
                        bzero(response, sizeof(response));
                        sprintf(response, "Default response");
                        // parse user input into command and argument
                        char * delim = "\t\n ";
                        args = strtok(buffer, delim); // first argument
                        strcpy(command, args); // copy first argument into command
                        if (args != NULL) {
                            args = strtok(NULL, delim); // second argument
                        }
                        // printf("command: %s\n", command); // TEST
                        // if (strcmp(args, "") != 0)
                        //     printf("args: %s\n", args); // TEST

                        // execute command
                        if (login_status[sd] == 2) { // if logged in
                            if (strcmp(command, "LIST") == 0) {
                                // redirect stdout to response
                                FILE *stdout_file = popen("ls", "r");
                                if (stdout_file) {
                                    bzero(response, sizeof(response));
                                    while (fgets(line, sizeof(line), stdout_file)) {
                                        strcat(response, line);
                                    }
                                    pclose(stdout_file);
                                }
                                response[strcspn(response, "\n")] = 0; // remove newline at the end
                            }
                            else if (strcmp(command, "CWD") == 0) {
                                if (chdir(args) < 0) { // CWD failed
                                    sprintf(response, "550 No such file or directory");
                                }
                                else { // CWD successful
                                    // redirect stdout to response
                                    FILE *stdout_file = popen("pwd", "r");
                                    if (stdout_file) {
                                        bzero(response, sizeof(response));
                                        sprintf(response, "200 directory changed to ");
                                        while (fgets(line, sizeof(line), stdout_file)) {
                                            strcat(response, line);
                                        }
                                        pclose(stdout_file);
                                    }
                                    response[strcspn(response, "\n")] = 0; // remove newline at the end
                                }
                            }
                            else if (strcmp(command, "PWD") == 0) {
                                // redirect stdout to response
                                FILE *stdout_file = popen("pwd", "r");
                                if (stdout_file) {
                                    bzero(response, sizeof(response));
                                    sprintf(response, "257 ");
                                    while (fgets(line, sizeof(line), stdout_file)) {
                                        strcat(response, line);
                                    }
                                    pclose(stdout_file);
                                }
                                response[strcspn(response, "\n")] = 0; // remove newline at the end
                            }
                            else if (strcmp(command, "PORT") == 0) {
                                // printf("PORT command from %s\n", args); // TEST
                                // parse user input into command and argument
                                char * delim = ",";
                                char data_address[1000];
                                int data_port, p1, p2;
                                char * token;
                                int counter = 0;

                                token = strtok(args, delim); // first token
                                strcpy(data_address, token); // copy first token
                                counter++;
                                while (token != NULL) {
                                    token = strtok(NULL, delim); // get new token
                                    if (counter < 4) {
                                        strcat(data_address, ".");
                                        strcat(data_address, token);
                                    }
                                    else if (counter == 4) {
                                        p1 = atoi(token);
                                    }
                                    else if (counter == 5) {
                                        p2 = atoi(token);
                                    }
                                    counter++;
                                }
                                // convert p1 and p2 to port
                                data_port = (p1 * 256) + p2;
                                printf("data address %s, data port %d\n", data_address, data_port); // TEST

                                // // create new socket for data connection ??
                                // int data_sd = socket(AF_INET, SOCK_STREAM, 0);
                                // if (data_sd < 0) {
                                //     perror("Error: socket() failed");
                                //     exit(EXIT_FAILURE);
                                // }
                                // // set socket options
                                // int opt = 1;
                                // if (setsockopt(data_sd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
                                //     perror("Error: setsockopt() failed");
                                //     exit(EXIT_FAILURE);
                                // }
                                // // set socket address
                                // struct sockaddr_in data_addr;
                                // data_addr.sin_family = AF_INET;
                                // data_addr.sin_addr.s_addr = INADDR_ANY;
                                // data_addr.sin_port = htons(data_port);
                                // // bind socket to address
                                // if (bind(data_sd, (struct sockaddr *) &data_addr, sizeof(data_addr)) < 0) {
                                //     perror("Error: bind() failed");
                                //     exit(EXIT_FAILURE);
                                // }
                                // // listen for connections
                                // if (listen(data_sd, 5) < 0) {
                                //     perror("Error: listen() failed");
                                //     exit(EXIT_FAILURE);
                                // }
                                // // accept a new conection from client
                                // int data_client_sd = accept(data_sd, (struct sockaddr *) &data_addr, (socklen_t *) &addrlen);
                                // if (data_client_sd < 0) {
                                //     perror("Error: accept() failed");
                                //     exit(EXIT_FAILURE);
                                // }
                                sprintf(response, "200 PORT command successful.");
                                // close(data_sd); // close connections
                                // close(data_client_sd);
                            }
                            else if (strcmp(command, "STOR") == 0) {
                                if (strcmp(args, "") != 0){
                                    strcpy(filename, args);
                                }
                                else {
                                    printf("Error: empty filename");
                                }
                                write_file(sd, filename);
                                sprintf(response, "200 Upload Successful");
                                send(sd, response, sizeof(response), 0);
                                //printf("\n200 Upload Successful\n");
                                break;
            	            }
                            else if (strcmp(command, "RETR")== 0) {
                                sprintf(response, "Success");
                                send(sd, response, sizeof(response), 0);
                                if (strcmp(args, "") != 0){
                                    strcpy(filename, args);
                                }
                                else {
                                    printf("Error: empty filename");
                                }
                                FILE *fp;
                                fp = fopen(filename, "r");
                                if (fp == NULL){
                                    perror("Error: reading file");
                                    exit(1);
                                }
                                send_file(fp, sd);
                                fclose(fp);
                            //    sprintf(response, "200 Download Successful");
                            //    send(sd, response, sizeof(response), 0);
                                //printf("\n200 Download Successful\n");
                                break;
                            }
                            else if (strcmp(command, "QUIT") == 0) {
                                sprintf(response, "221 Service closing control connection.");
                                FD_CLR(sd, &current_sockets); // remove client socket from current_sockets
                            }
                            else {
                                sprintf(response, "503 Bad sequence of commands.");
                            }
                        }
                        else { // trigger user authentication
                            if (strcmp(command, "USER") == 0) {
                                for (int i = 0; i < num_users; i++) {
                                    if (strcmp(args, usernames[i]) == 0) { // username found
                                        login_status[sd] = 1; // indicates username has been authenticated
                                        user_i = i; // set user index to the current index (to find the matching password)
                                        // printf("username found: %s\n", usernames[i]);
                                        // printf("user_i %d\n", user_i);
                                        sprintf(response, "331 User name OK, need password.");
                                        break;
                                    }
                                }
                                if (login_status[sd] == 0) { // failed to authenticate
                                    // printf("In USER: login_status: %d\n", login_status[sd]); // TEST
                                    sprintf(response, "530 Not logged in.");
                                }
                            }
                            else if (strcmp(command, "PASS") == 0 && login_status[sd] == 1) {
                                if (strcmp(args, passwords[user_i]) == 0) { // password matches
                                    login_status[sd] = 2; // indicates username has been authenticated
                                    sprintf(response, "230 User logged in, proceed.");
                                }
                                else { // failed to authenticate
                                    // printf("In PASS: login_status: %d\n", login_status[sd]); // TEST
                                    sprintf(response, "530 Not logged in.");
                                }
                            }
                            else if (strcmp(command, "QUIT") == 0) {
                                sprintf(response, "221 Service closing control connection.");
                                FD_CLR(sd, &current_sockets); // remove client socket from current_sockets
                            }
                            else if (!strcmp(command, "LIST") | !strcmp(command, "STOR") | !strcmp(command, "RETR") | 
                                    !strcmp(command, "CWD") | !strcmp(command, "PWD")) { // valid commands but not authenticated
                                sprintf(response, "530 Not logged in.");
                            }
                            else { // invalid commands
                                sprintf(response, "503 Bad sequence of commands.");
                            }
                        }
                        
                        // respond to client
                        send(sd, response, sizeof(response), 0);
                        // FD_CLR(sd, &current_sockets); // remove client socket from current_sockets
                    }
                }
            }
        }
	}
	// close
	close(server_sd);
	return 0;
}