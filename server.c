#include <netinet/in.h> //structure for storing address information
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> //for socket APIs
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

// ==== UTILITY METHODS =====
void searchDirectory(int client_sd, const char *search_path, const char *filename)
{
    DIR *dir = opendir(search_path);
    if (dir == NULL)
    {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", search_path, entry->d_name);

        struct stat file_stat;
        if (stat(filepath, &file_stat) == 0)
        {
            if (S_ISDIR(file_stat.st_mode))
            {
                // Recurse into subdirectory
                searchDirectory(client_sd, filepath, filename);
            }
            else if (S_ISREG(file_stat.st_mode) && strcmp(entry->d_name, filename) == 0)
            {
                // File found
                char response[1024];
                snprintf(response, sizeof(response), "File: %s\nSize: %ld bytes\nDate Created: %s", filepath, file_stat.st_size, ctime(&file_stat.st_ctime));
                write(client_sd, response, 1024);
                closedir(dir);
                return;
            }
        }
    }

    closedir(dir);
}

/*
    CLIENT COMMANDS
    ===============
*/

void filesrch(int client_sd, char buff1[])
{
    const char *filename = buff1 + 9; // Extract the filename from the command
    printf("Requested file: %s\n", filename);

    // Get the user's home directory
    const char *home_dir = getenv("HOME");
    printf("Searching directory tree rooted at: %s\n", home_dir);

    // Recursively search the home directory for the file
    searchDirectory(client_sd, home_dir, filename);

    // File not found
    char *msg = "File not found";
    write(client_sd, msg, 100);
}

void processClient(int client_sd)
{
    // printf("Message from the client\n");
    char buff1[1024];
    ssize_t bytes_read = read(client_sd, buff1, sizeof(buff1) - 1);

    if (bytes_read <= 0)
    {
        perror("read");
        close(client_sd);
        return;
    }

    buff1[bytes_read] = '\0';
    // printf("Client command: %s\n", buff1);

    if (strcmp(buff1, "quit") == 0)
    {
        printf("Quitting...");
        // break;
    }
    // compare the first 6 characters with "fgets"
    else if (strncmp(buff1, "fgets ", 6) == 0)
    {
        // execute fgets command
        // fgets_command(client_sd, buff1);
    }
    // compare first 8 characters of buff1 with "tarfgetz"
    else if (strncmp(buff1, "tarfgetz ", 8) == 0)
    {
        // execute tarfgetz command
        // tarfgetz(client_sd, buff1);
    }
    else if (strncmp(buff1, "filesrch ", 8) == 0)
    {
        // code here
        filesrch(client_sd, buff1);
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
