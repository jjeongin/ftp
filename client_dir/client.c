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
#include <errno.h>

#define MAX_BUFFER 200   // max buffer size
#define MAX_COMMAND 100  // max command size
#define MAX_RESPONSE 100 // max response size
#define MAX_FILENAME 100 // max file name
#define DATA_SIZE 300    // max data size per packet for file upload/download

void write_file(FILE *fp, int socket)
{
    char data[DATA_SIZE] = "";
    while (recv(socket, data, sizeof(data), 0) > 0)
    { // receive file content from client
        fputs(data, fp);
        bzero(data, sizeof(data));
    }

}

void send_file(FILE *fp, int socket)
{
    char data[DATA_SIZE] = "";
    while (fgets(data, sizeof(data), fp) != NULL)
    {
        // printf("Sending message: \"%s\"\n", data); // TEST
        if (send(socket, data, sizeof(data), 0) < 0)
        {
            perror("Error: sending file.");
            exit(-1);
        }
        bzero(data, sizeof(data));
    }
}

// PORT command
char *get_port_command(char *client_ip, int data_port)
{
    // PORT message to send
    char port_command[MAX_COMMAND];
    // replace all . with , in client IP
    char *current_pos = strchr(client_ip, '.');
    while (current_pos)
    {
        *current_pos = ',';
        current_pos = strchr(current_pos, '.');
    }
    // convert port to p1 and p2
    int p1 = data_port / 256;
    int p2 = data_port % 256;
    sprintf(port_command, "PORT %s,%d,%d\n", client_ip, p1, p2); // PORT command string
    return port_command;
}

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
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)server_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    socklen_t socket_len = sizeof(server_addr);
    if (connect(client_sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error: connect");
        exit(-1);
    }

    // get client IP and Port number
    char client_ip[16];
    unsigned int client_port;
    struct sockaddr_in client_addr;
    int addr_size = sizeof(client_addr);
    getsockname(client_sd, (struct sockaddr *)&client_addr, &addr_size);
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    client_port = ntohs(client_addr.sin_port);
    // port number for data channel
    unsigned int data_port = client_port + 1;
    printf("Client IP: %s\n", client_ip);     // TEST
    printf("Client Port: %d\n", client_port); // TEST

    char filename[MAX_FILENAME];                                           // filename
    char buffer[MAX_BUFFER];                                               // user input
    char command[MAX_COMMAND];                                             // command (LIST, PWD, CWD, etc.)
    char response[MAX_RESPONSE];                                           // response from server
    char *port_command = (char *)malloc((MAX_COMMAND + 1) * sizeof(char)); // PORT command
    while (1)
    {                                  // repeat
        bzero(buffer, sizeof(buffer)); // place null bytes in the buffer
        bzero(command, sizeof(command));
        printf("ftp> ");
        fgets(buffer, sizeof(buffer), stdin); // get user input and store it in the buffer
        if (buffer[0] == '!') // client command
        {
            // parse user input to command and args
            char *delim = "\t\r\n ";
            char *args = strtok(buffer, delim); // first argument
            strcpy(command, args);              // copy first argument into command
            if (args != NULL)
            {
                args = strtok(NULL, delim); // second argument
            }
            // execute client command
            if (strcmp(command, "!LIST") == 0)
            {
                system("ls");
            }
            else if (strcmp(command, "!CWD") == 0)
            {
                if (chdir(args) < 0)
                {
                    printf("550 No such file or directory.\n");
                }
            }
            else if (strcmp(command, "!PWD") == 0)
            {
                system("pwd");
            }
            else
            {
                printf("202 Command not implemented.\n");
            }
        }
        else if (strncmp(buffer, "STOR", 4) == 0)
        {
            port_command = get_port_command(client_ip, data_port);  // generate port command
            send(client_sd, port_command, strlen(port_command), 0); // send PORT command to server
            // receive PORT success message
            bzero(response, sizeof(response));
            recv(client_sd, &response, sizeof(response), 0);
            printf("%s\n", response);
            if (strcmp(response, "200 PORT command successful.") == 0) // PORT command successful
            {
                // send STOR command to server
                send(client_sd, buffer, sizeof(buffer), 0);
                // get filename to send
                char *delim = "\t\r\n ";
                char *args = strtok(buffer, delim); // first argument
                strcpy(command, args);              // copy first argument into command
                if (args != NULL)
                {
                    args = strtok(NULL, delim); // second argument
                }
                else 
                {
                    printf("503 Bad sequence of commands.\n");
                    continue;
                }
                bzero(filename, sizeof(filename));
                strcpy(filename, args);
                // open file to send
                FILE *fp = fopen(filename, "r");
                if (fp == NULL)
                {
                    printf("550 No such file or directory.\n");
                    continue;
                }
                // open data connection
                int pid = fork();
                if (pid == 0)
                {
                    // open new data connection
                    int data_sd = socket(AF_INET, SOCK_STREAM, 0);
                    if (data_sd < 0)
                    {
                        perror("Error opening socket");
                        exit(-1);
                    }
                    // set socket option (to rerun the server immediately after we kill it)
                    int value = 1;
                    setsockopt(data_sd, SOL_SOCKET, SO_REUSEADDR, (const void *)&value, sizeof(value));
                    // build data address at data_port
                    struct sockaddr_in data_addr;
                    bzero((char *)&data_addr, sizeof(data_addr));
                    data_addr.sin_family = AF_INET;
                    data_addr.sin_port = htons((unsigned short)data_port);
                    data_addr.sin_addr.s_addr = INADDR_ANY;
                    // bind
                    if (bind(data_sd, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0)
                    {
                        printf("Error: could not bind to process (%d) %s\n", errno, strerror(errno));
                        exit(-1);
                    }
                    // listen
                    if (listen(data_sd, 5) < 0)
                    {
                        printf("Listen failed..\n");
                        exit(-1);
                    }
                    // receive file ready to transfer message
                    bzero(response, sizeof(response));
                    recv(client_sd, &response, sizeof(response), 0);
                    printf("%s\n", response);
                    if (strcmp(response, "150 File status okay; about to open data connection.") == 0)
                    {
                        // accept data connection from server
                        struct sockaddr_in data_server_addr; // to store data server address
                        int data_server_addrlen = sizeof(data_server_addr);
                        int data_server_sd = accept(data_sd, (struct sockaddr *)&data_server_addr, (socklen_t *)&data_server_addrlen);
                        if (data_server_sd < 0)
                        {
                            printf("Error: accept failed\n");
                            exit(EXIT_FAILURE);
                        }
                        // upload file
                        send_file(fp, data_server_sd);
                        fclose(fp);
                        close(data_server_sd);
                    }
                    // close data connection
                    close(data_sd);
                    exit(0);
                }
                wait(NULL);                        // wait for child process to finish
                bzero(response, sizeof(response)); // wait for the Transfer success message
                recv(client_sd, &response, sizeof(response), 0);
                printf("%s\n", response);
                // after the data transfer, increment data port by 1 for the future connection
                data_port++;
            }
        }
        else if (strncmp(buffer, "RETR", 4) == 0)
        {
            port_command = get_port_command(client_ip, data_port);  // generate port command
            send(client_sd, port_command, strlen(port_command), 0); // send PORT command to server
            // receive PORT success message
            bzero(response, sizeof(response));
            recv(client_sd, &response, sizeof(response), 0);
            printf("%s\n", response);
            if (strcmp(response, "200 PORT command successful.") == 0) // if PORT successful
            {
                send(client_sd, buffer, sizeof(buffer), 0); // send RETR command to server
                // get filename to send
                char *delim = "\t\r\n ";
                char *args = strtok(buffer, delim); // first argument
                strcpy(command, args);              // copy first argument into command
                if (args != NULL)
                {
                    args = strtok(NULL, delim); // second argument
                }
                else 
                {
                    printf("503 Bad sequence of commands.\n");
                    continue;
                }
                // create temp file
                char temp_filename[MAX_FILENAME + 4];
                strcpy(temp_filename, args);
                strcat(temp_filename, ".tmp");
                FILE *fp = fopen(temp_filename, "w");
                if (fp == NULL)
                {
                    perror("Error: creating temp file");
                    exit(-1);
                }
                // open data connection
                int pid = fork();
                if (pid == 0)
                {
                    // open new data connection
                    int data_sd = socket(AF_INET, SOCK_STREAM, 0);
                    if (data_sd < 0)
                    {
                        perror("Error opening socket");
                        exit(-1);
                    }
                    // set socket option (to rerun the server immediately after we kill it)
                    int value = 1;
                    setsockopt(data_sd, SOL_SOCKET, SO_REUSEADDR, (const void *)&value, sizeof(value));
                    // build data address at data_port
                    struct sockaddr_in data_addr;
                    bzero((char *)&data_addr, sizeof(data_addr));
                    data_addr.sin_family = AF_INET;
                    data_addr.sin_port = htons((unsigned short)data_port);
                    data_addr.sin_addr.s_addr = INADDR_ANY;
                    // bind
                    if (bind(data_sd, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0)
                    {
                        printf("Error: could not bind to process (%d) %s\n", errno, strerror(errno));
                        exit(-1);
                    }
                    // listen
                    if (listen(data_sd, 5) < 0)
                    {
                        printf("Listen failed..\n");
                        exit(-1);
                    }
                    // receive file ready to transfer message
                    bzero(response, sizeof(response));
                    recv(client_sd, &response, sizeof(response), 0);
                    printf("%s\n", response);
                    if (strcmp(response, "150 File status okay; about to open data connection.") == 0)
                    {
                        // accept data connection from server
                        struct sockaddr_in data_server_addr; // to store data server address
                        int data_server_addrlen = sizeof(data_server_addr);
                        int data_server_sd = accept(data_sd, (struct sockaddr *)&data_server_addr, (socklen_t *)&data_server_addrlen);
                        if (data_server_sd < 0)
                        {
                            printf("Error: accept failed\n");
                            exit(EXIT_FAILURE);
                        }
                        // download file
                        write_file(fp, data_server_sd);
                        fclose(fp);
                        close(data_server_sd); // close data connection
                    }
                    close(data_sd);
                    exit(0);
                }
                wait(NULL);                        // wait for child process to finish
                rename(temp_filename, args);   // rename temp file to original filename
                bzero(response, sizeof(response)); // wait for the Transfer success message
                recv(client_sd, &response, sizeof(response), 0);
                printf("%s\n", response);
                // after the data transfer, increment data port by 1 for the future connection
                data_port++;
            }
        }
        else if (strncmp(buffer, "LIST", 4) == 0)
        {
            port_command = get_port_command(client_ip, data_port);  // generate port command
            send(client_sd, port_command, strlen(port_command), 0); // send PORT command to server
            // receive PORT success message
            bzero(response, sizeof(response));
            recv(client_sd, &response, sizeof(response), 0);
            printf("%s\n", response);
            if (strcmp(response, "200 PORT command successful.") == 0) // if PORT successful
            {
                send(client_sd, buffer, sizeof(buffer), 0); // send LIST command to server
                // open data connection
                int pid = fork();
                if (pid == 0)
                {
                    // open new data connection
                    int data_sd = socket(AF_INET, SOCK_STREAM, 0);
                    if (data_sd < 0)
                    {
                        perror("Error opening socket");
                        exit(-1);
                    }
                    // set socket option (to rerun the server immediately after we kill it)
                    int value = 1;
                    setsockopt(data_sd, SOL_SOCKET, SO_REUSEADDR, (const void *)&value, sizeof(value));
                    // build data address at data_port
                    struct sockaddr_in data_addr;
                    bzero((char *)&data_addr, sizeof(data_addr));
                    data_addr.sin_family = AF_INET;
                    data_addr.sin_port = htons((unsigned short)data_port);
                    data_addr.sin_addr.s_addr = INADDR_ANY;
                    // bind
                    if (bind(data_sd, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0)
                    {
                        printf("Error: could not bind to process (%d) %s\n", errno, strerror(errno));
                        exit(-1);
                    }
                    // listen
                    if (listen(data_sd, 5) < 0)
                    {
                        printf("Listen failed..\n");
                        exit(-1);
                    }
                    // receive file ready to transfer message
                    bzero(response, sizeof(response));
                    recv(client_sd, &response, sizeof(response), 0);
                    printf("%s\n", response);
                    // accept data connection from server
                    struct sockaddr_in data_server_addr; // to store data server address
                    int data_server_addrlen = sizeof(data_server_addr);
                    int data_server_sd = accept(data_sd, (struct sockaddr *)&data_server_addr, (socklen_t *)&data_server_addrlen);
                    if (data_server_sd < 0)
                    {
                        printf("Error: accept failed\n");
                        exit(EXIT_FAILURE);
                    }
                    // recieve LIST output through data connection
                    bzero(response, sizeof(response));
                    recv(data_server_sd, &response, sizeof(response), 0);
                    printf("%s\n", response);
                    // close data connection
                    close(data_server_sd);
                    close(data_sd);
                    exit(0);
                }
                wait(NULL);
                bzero(response, sizeof(response)); // wait for "Transfer completed" message
                recv(client_sd, &response, sizeof(response), 0);
                printf("%s\n", response);
                // after the data transfer, increment data port by 1 for the future connection
                data_port++;
            }
        }
        else
        {
            send(client_sd, buffer, sizeof(buffer), 0); // send command to server
            bzero(response, sizeof(response));
            recv(client_sd, &response, sizeof(response), 0); // recieve output from the server
            printf("%s\n", response);
            if (strcmp(response, "221 Service closing control connection.") == 0)  // close connection and terminate the program
            {
                break;
            }
        }
    }

    // close socket
    close(client_sd);
    return 0;
}