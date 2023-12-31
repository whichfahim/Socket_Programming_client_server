#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>

/*
    CLIENT COMMANDS
    ===============
*/
void fgets_command(int client_sd, char buff1[])
{
    const char *file_list = buff1 + 6; // Extract the list of files from the command
    printf("List of files: %s\n", file_list);

    // Tokenize the file list by space
    char *file_token = strtok(file_list, " ");
    int file_count = 0;
    char file_list_str[1024] = ""; // Buffer to store the file list

    // Takes up to 4 files as parameters
    while (file_token != NULL && file_count <= 4)
    {
        // Check if the file exists
        if (access(file_token, F_OK) != -1)
        {
            // Concatenate file_list_str with file_token
            strcat(file_list_str, file_token);
            // Insert space in between
            strcat(file_list_str, " ");

            // only increment count if file exists
            file_count++;
        }
        else
        {
            // printf("File not found: %s\n", file_token);
            char msg[100];
            sprintf(msg, "File not found: %s\n", file_token);
            // send(client_sd, msg, strlen(not_found_msg), 0);

            write(client_sd, msg, 100);
        }

        file_token = strtok(NULL, " ");
    }

    if (file_count > 0)
    {
        printf("Files found: %d\n", file_count);

        // Create a tar archive of all the files
        char tar_command[1024];
        snprintf(tar_command, sizeof(tar_command), "tar -czf temp.tar.gz %s", file_list_str);

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
        // const char *not_found_msg = "No file found";
        char *msg = "No files found";

        write(client_sd, msg, 50);
    }
}

// =======TARFGETZ SIZE1 SIZE2===========
void tarfgetz(int client_sd, char buff1[])
{

    // code here
    unsigned long long minSize, maxSize;
    // returns the number of fields that were successfully converted and assigned

    int assigned = sscanf(buff1 + 9, "%llu %llu", &minSize, &maxSize);

    // validation for client input
    if (assigned >= 2 && minSize <= maxSize)
    {
        // only perform these operations if command format is accurate
        sscanf(buff1 + 9, "%llu %llu", &minSize, &maxSize);
        printf("Requested size range: %llu - %llu\n", minSize, maxSize);

        // unsigned long long minSize = 0; // Minimum file size in bytes
        // unsigned long long maxSize = 1000; // Maximum file size in bytes

        char *fileList = (char *)malloc(1); // Start with an empty string
        if (fileList == NULL)
        {
            perror("malloc");
            return EXIT_FAILURE;
        }
        fileList[0] = '\0';

        findFilesInSizeRange(minSize, maxSize, &fileList);

        // Create a temporary file to store the list of files
        char tmpFilePath[] = "/tmp/file_list.txt";
        int tmpFile = open(tmpFilePath, O_CREAT | O_WRONLY, 0644);
        if (tmpFile == -1)
        {
            perror("open");
            free(fileList);
            return EXIT_FAILURE;
        }
        write(tmpFile, fileList, strlen(fileList));
        close(tmpFile);

        // Create tar.gz archive
        pid_t pid = fork();
        if (pid == 0)
        {
            chdir("."); // Change to the current directory
            execlp("tar", "tar", "-czvf", "temp.tar.gz", "-T", tmpFilePath, NULL);
            perror("execlp");
            exit(EXIT_FAILURE);
        }
        else if (pid > 0)
        {
            wait(NULL);
        }
        else
        {
            perror("fork");
            free(fileList);
            return EXIT_FAILURE;
        }

        printf("Tar.gz archive created: temp.tar.gz\n");

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

void getdirf(int client_sd, char buff1[])
{
    // Extract date1 and date2 from the command
    char date1[20], date2[20];
    sscanf(buff1 + 9, "%s %s", date1, date2);

    // Open the client's home directory
    const char *home_dir = getenv("HOME");
    DIR *dir = opendir(home_dir);
    if (dir == NULL)
    {
        perror("opendir");
        return;
    }

    // Convert date strings to time_t values
    struct tm date1_tm, date2_tm;
    strptime(date1, "%Y-%m-%d", &date1_tm);
    strptime(date2, "%Y-%m-%d", &date2_tm);
    time_t start_date = mktime(&date1_tm);
    time_t end_date = mktime(&date2_tm);

    // Iterate through the directory entries
    struct dirent *entry;
    char fileList[1024] = ""; // To store the list of files
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type != DT_REG)
        {
            continue; // Skip non-regular files
        }

        char filePath[1024];
        snprintf(filePath, sizeof(filePath), "%s/%s", home_dir, entry->d_name);

        struct stat file_stat;
        if (stat(filePath, &file_stat) == 0)
        {
            // Check if the file modification date is within the specified range
            time_t file_mtime = file_stat.st_mtime;

            if (file_mtime >= start_date && file_mtime <= end_date)
            {
                // Concatenate file name to fileList
                strcat(fileList, entry->d_name);
                strcat(fileList, "\n");
            }
        }
    }
    closedir(dir);

    // Create a temporary text file to store the list of files
    char tmpFilePath[] = "/tmp/file_list.txt";
    int tmpFile = open(tmpFilePath, O_CREAT | O_WRONLY, 0644);
    if (tmpFile == -1)
    {
        perror("open");
        return;
    }
    write(tmpFile, fileList, strlen(fileList));
    close(tmpFile);

    // Create tar.gz archive of the files list
    pid_t pid = fork();
    if (pid == 0)
    {
        chdir(home_dir); // Change to the home directory
        execlp("tar", "tar", "-czvf", "temp.tar.gz", "-T", tmpFilePath, NULL);
        perror("execlp");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        wait(NULL);
    }
    else
    {
        perror("fork");
        return;
    }

    printf("Tar.gz archive created: temp.tar.gz\n");

    // Delete the temporary file
    if (unlink(tmpFilePath) != 0)
    {
        perror("unlink");
    }

    // Send the tar.gz archive to the client
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
        fgets_command(client_sd, buff1);
    }
    // compare first 8 characters of buff1 with "tarfgetz"
    else if (strncmp(buff1, "tarfgetz ", 8) == 0)
    {
        // execute tarfgetz command
        tarfgetz(client_sd, buff1);
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
        getdirf(client_sd, buff1);
    }
}

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

    printf("Mirror IP Address: %s\n", ipAddress);

    // listen

    listen(lis_sd, 5);

    int numChildren = 0; // Count of child processes
    while (1)
    {

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