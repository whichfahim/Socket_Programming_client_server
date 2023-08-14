#include <netinet/in.h> //structure for storing address information
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> //for socket APIs
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

// ======UTILITY METHODS==========
void appendFilePath(char **fileList, const char *filePath)
{
    size_t currentSize = strlen(*fileList);
    size_t filePathSize = strlen(filePath);
    // reallocates memory to accommodate the new file path
    // resize the memory block pointed to by *fileList
    char *newList = realloc(*fileList, currentSize + filePathSize + 2); // +2 for newline and null terminator
    if (newList == NULL)
    {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    // update the fileList pointer to point to the new memory block
    *fileList = newList;
    strcat(*fileList, filePath);
    strcat(*fileList, "\n");
}

void findFilesInSizeRange(unsigned long long minSize, unsigned long long maxSize, char **fileList, const char *searchPath)
{
    DIR *dir = opendir(searchPath);
    if (dir == NULL)
    {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char filePath[256];
        snprintf(filePath, sizeof(filePath), "%s/%s", searchPath, entry->d_name);

        struct stat fileStat;
        if (stat(filePath, &fileStat) == 0)
        {
            if (S_ISREG(fileStat.st_mode) && fileStat.st_size >= minSize && fileStat.st_size <= maxSize)
            {
                appendFilePath(fileList, filePath);
            }
            else if (S_ISDIR(fileStat.st_mode))
            {
                // Recursively search subdirectories
                findFilesInSizeRange(minSize, maxSize, fileList, filePath);
            }
        }
        else
        {
            perror("stat");
        }
    }

    closedir(dir);
}

// =======tarfgetz size1 size2 <-u>===========
void tarfgetz(int client_sd, char buff1[])
{

    // code here
    unsigned long long minSize, maxSize;
    const char *home_dir = getenv("HOME");
    int unzip = 0; // Default is not to unzip

    // Check if -u flag is provided
    // char *strstr(const char *haystack, const char *needle)
    if (strstr(buff1, " -u") != NULL)
    {
        // printf("-u flag found.");
        unzip = 1;
    }

    // returns the number of fields that were successfully converted and assigned
    int assigned = sscanf(buff1 + 9, "%llu %llu", &minSize, &maxSize);

    // validation for client input
    if (assigned >= 2 && minSize <= maxSize)
    {
        // only perform these operations if command format is accurate
        sscanf(buff1 + 9, "%llu %llu", &minSize, &maxSize);
        printf("Requested size range: %llu - %llu\n", minSize, maxSize);

        char *fileList = (char *)malloc(1); // Start with an empty string
        if (fileList == NULL)
        {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        fileList[0] = '\0';

        findFilesInSizeRange(minSize, maxSize, &fileList, home_dir);
        chdir(home_dir); // Change to home directory
        // Create a temporary file to store the list of files
        char tmpFilePath[] = "/tmp/file_list.txt";
        int tmpFile = open(tmpFilePath, O_CREAT | O_WRONLY, 0644);
        if (tmpFile == -1)
        {
            perror("open");
            free(fileList);
            exit(EXIT_FAILURE);
        }
        write(tmpFile, fileList, strlen(fileList));
        close(tmpFile);

        // Create tar.gz archive
        pid_t pid = fork();
        if (pid == 0)
        {
            // in the child process

            // Create the tar.gz archive
            char *tmpFilePath = "/tmp/file_list.txt";
            execlp("tar", "tar", "-czvf", "temp.tar.gz", "-C", tmpFilePath, NULL);

            perror("execlp");
            exit(EXIT_FAILURE);
        }
        else if (pid > 0)
        {
            wait(NULL); // wait for child process to finish
            if (unzip)
            {
                // Unzip the tar.gz archive in the client's home directory

                char unzip_command[1024];
                snprintf(unzip_command, sizeof(unzip_command), "tar -xzvf temp.tar.gz -C %s", home_dir);
                // invokes the default shell commands are read from string
                execlp("sh", "sh", "-c", unzip_command, NULL);
            }
        }
        else
        {
            perror("fork");
            free(fileList);
            exit(EXIT_FAILURE);
        }

        char *msg = "Finished tarring files.";

        write(client_sd, msg, strlen(msg) + 1);

        // printf("Tar.gz archive created: temp.tar.gz\n");

        // Delete the temporary file
        if (unlink(tmpFilePath) != 0)
        {
            perror("unlink");
        }

        free(fileList);
    }
    else
    {
        char *msg = "Usage: tarfgetz minSize maxSize";

        write(client_sd, msg, 50);
        // printf("Wrong input format.\n");
        // printf("Usage: tarfgetz minSize maxSize\n");
        // printf("Where, minSize<maxSize");
    }
}

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
    }
    // compare first 8 characters of buff1 with "tarfgetz"
    else if (strncmp(buff1, "tarfgetz ", 8) == 0)
    {
        tarfgetz(buff1);
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

void findFilesInSizeRange(unsigned long long minSize, unsigned long long maxSize, char **fileList)
{
    DIR *dir = opendir(".");
    if (dir == NULL)
    {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_REG)
        { // Regular file
            char filePath[256];
            snprintf(filePath, sizeof(filePath), "./%s", entry->d_name);

            struct stat fileStat;
            if (stat(filePath, &fileStat) == 0)
            {
                if (S_ISREG(fileStat.st_mode) && fileStat.st_size >= minSize && fileStat.st_size <= maxSize)
                {
                    appendFilePath(fileList, entry->d_name);
                }
            }
            else
            {
                perror("stat");
            }
        }
    }

    closedir(dir);
}

void appendFilePath(char **fileList, const char *filePath)
{
    size_t currentSize = strlen(*fileList);
    size_t filePathSize = strlen(filePath);
    char *newList = realloc(*fileList, currentSize + filePathSize + 2); // +2 for newline and null terminator
    if (newList == NULL)
    {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    *fileList = newList;
    strcat(*fileList, filePath);
    strcat(*fileList, "\n");
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
