#include <netinet/in.h> //structure for storing address information
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> //for socket APIs
#include <sys/types.h>
#include <string.h>
#include <time.h>

void processClient(int client_sd)
{
    printf("Message from the client\n");
    char buff1[1024];
    ssize_t bytes_read = read(client_sd, buff1, sizeof(buff1) - 1);

    if (bytes_read <= 0)
    {
        perror("read");
        close(client_sd);
        return;
    }

    buff1[bytes_read] = '\0';
    printf("Client command: %s\n", buff1);

    if (strcmp(buff1, "quit") == 0)
    {
        printf("Quitting...");
        // break;
        // compare the first 6 characters with "fgets"
    }
    else if (strncmp(buff1, "fgets ", 6) == 0)
    {

        // execute fgets command
        // const char *command = buff1 + 6; // Extract the command after "fgets "
        // printf("Client entered: fgets %s",command);
        // iterate through list of files

        // check if file exists

        // if -e then
        // search for file in home directory
        // make a tar of the file and send it to the client
        // else
        // printf this file does not exist
        const char *file_list = buff1 + 6; // Extract the list of files from the command
        printf("List of files: %s\n", file_list);

        // Tokenize the file list by space
        char *file_token = strtok(file_list, " ");
        int file_count = 0;
        char file_list_str[1024] = ""; // Buffer to store the file list

        while (file_token != NULL)
        {
            strcat(file_list_str, file_token);
            strcat(file_list_str, " ");
            file_token = strtok(NULL, " ");
            file_count++;
        }

        if (file_count > 0)
        {
            printf("Files found: %d\n", file_count);

            // Create a tar archive of all the files
            char tar_command[1024];
            snprintf(tar_command, sizeof(tar_command), "tar -cf temp.tar %s", file_list_str);
            system(tar_command);

            // Send the tar archive to the client
            FILE *tar_file = fopen("temp.tar.gz", "rb");

            if (tar_file)
            {
                fseek(tar_file, 0, SEEK_END);
                long tar_size = ftell(tar_file);
                rewind(tar_file);

                char *tar_buffer = (char *)malloc(tar_size);
                fread(tar_buffer, 1, tar_size, tar_file);
                fclose(tar_file);

                send(client_sd, tar_buffer, tar_size, 0);

                free(tar_buffer);
            }
            else
            {
                const char *error_msg = "Error creating tar.gz file";
                send(client_sd, error_msg, strlen(error_msg), 0);
            }
        }
        else
        {
            const char *not_found_msg = "No file found";
            send(client_sd, not_found_msg, strlen(not_found_msg), 0);
        }
    }
    else if (strncmp(buff1, "tarfgetz ", 8) == 0)
    {
        // code here
    }
    else if (strncmp(buff1, "filesrch ", 8) == 0)
    {
        // code here
    }
    else if (strncmp(buff1, "targzf ", 6) == 0)
    {
        // code here
    }
    else if (strncmp(buff1, "getdirf ", 7) == 0)
    {
        // code here
    }
}

int main(int argc, char *argv[])
{ // E.g., 1, server
    char *myTime;
    time_t currentUnixTime; // time.h
    int lis_sd, con_sd, portNumber;
    socklen_t len;
    struct sockaddr_in servAdd;

    if (argc != 2)
    {
        fprintf(stderr, "Call model: %s <Port#>\n", argv[0]);
        exit(0);
    }
    // socket()
    if ((lis_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "Could not create socket\n");
        exit(1);
    }

    servAdd.sin_family = AF_INET;
    servAdd.sin_addr.s_addr = htonl(INADDR_ANY);
    sscanf(argv[1], "%d", &portNumber);
    servAdd.sin_port = htons((uint16_t)portNumber);

    // bind
    bind(lis_sd, (struct sockaddr *)&servAdd, sizeof(servAdd));

    struct sockaddr_in serverAddress;
    socklen_t addrLen = sizeof(serverAddress);

    // Get the address information of the bound socket
    if (getsockname(lis_sd, (struct sockaddr *)&serverAddress, &addrLen) == -1)
    {
        perror("getsockname");
        exit(1);
    }

    // Convert binary IPv4 address to human-readable format
    char ipAddress[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(serverAddress.sin_addr), ipAddress, INET_ADDRSTRLEN) == NULL)
    {
        perror("inet_ntop");
        exit(1);
    }

    printf("Server IP Address: %s\n", ipAddress);

    // listen

    listen(lis_sd, 5);

    int numChildren = 0; // Count of child processes
    while (1)
    {
        if (numChildren >= 6)
        {
            // Limit the number of children to 6
            wait(NULL); // Wait for any child process to finish
            numChildren--;
        }
        printf("Server listening on port: %d...\n", portNumber);

        con_sd = accept(lis_sd, (struct sockaddr *)NULL, NULL); // accept()
        if (con_sd < 0)
        {
            perror("accept");
            continue;
        }

        int pid = fork();
        if (pid == 0)
        {
            // Child process
            // Close listening socket in child
            close(lis_sd);

            processClient(con_sd);

            close(con_sd);

            // Child process exits
            exit(0);
        }
        else if (pid > 0)
        {
            // Parent process
            close(con_sd); // Close connection socket in parent
            numChildren++;
        }
        else
        {
            printf("error forking");
        }
    }
}
