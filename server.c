#include <netinet/in.h> //structure for storing address information
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> //for socket APIs
#include <sys/types.h>
#include <string.h>
#include <time.h>

void processclient(int client_sd)
{
    printf("Message from the client\n");
    char buff1[50];
    read(client_sd, buff1, 50);
    printf("%s\n", buff1);

    if (strcmp(buff1, "quit") == 0)
    {
        printf("Quitting...");
        // break;
        // compare the first 6 characters with "fgets"
    }
    else if (strncmp(buff1, "fgets ", 6) == 0)
    {
        // execute fgets command

        // iterate through list of files

        // check if file exists

        // if -e then
        // search for file in home directory
        // make a tar of the file and send it to the client
        // else
        // printf this file does not exist
    }
    else if (strncmp(buff1, "tarfgetz ", 8) == 0)
    {
        // code here
    }
    else if (strncmp(buff1, "filesrch ", 8) == 0)
    {
        // code here
        char filename[100];
        sscanf(command + 9, "%s", filename);

        // Perform file search logic
        DIR *dir;
        struct dirent *ent;
        struct stat file_stat;
        int found = 0;

        if ((dir = opendir("~")) != NULL) {
            while ((ent = readdir(dir)) != NULL) {
                if (strcmp(ent->d_name, filename) == 0) {
                    char filepath[256];
                    snprintf(filepath, sizeof(filepath), "~/%s", filename);
                    if (stat(filepath, &file_stat) == 0) {
                        char result[256];
                        snprintf(result, sizeof(result), "Found: %s, Size: %ld bytes, Date: %s", filename, file_stat.st_size, ctime(&file_stat.st_ctime));
                        write(client_sd, result, sizeof(result));
                        found = 1;
                        break;
                    }
                }
            }
            closedir(dir);
        }

        if (!found) {
            char result[] = "File not found";
            write(client_sd, result, sizeof(result));
        }
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

            processclient(con_sd);

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