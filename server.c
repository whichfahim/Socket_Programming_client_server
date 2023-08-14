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
#include <unistd.h>
#include <arpa/inet.h>

// ==== UTILITY METHODS =====
void searchFiles(char *file_list_str, const char *path, const char *filename, int *file_count)
{
    DIR *dir = opendir(path);
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

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat entry_stat;
        if (stat(full_path, &entry_stat) != 0)
        {
            // perror("stat");
            continue;
        }

        if (S_ISDIR(entry_stat.st_mode))
        {
            searchFiles(file_list_str, full_path, filename, file_count);
        }
        else if (S_ISREG(entry_stat.st_mode) && strcmp(entry->d_name, filename) == 0)
        {
            // Concatenate file_list_str with full_path
            strcat(file_list_str, full_path);
            // Insert space in between
            strcat(file_list_str, " ");
            (*file_count)++;
        }
    }

    closedir(dir);
}

void searchDirectory(int client_sd, const char *search_path, const char *filename)
{
    DIR *dir = opendir(search_path);
    if (dir == NULL)
    {
        perror("opendir");
        return;
    }

    // printf("filename: %s\n",filename);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // continue execution if directory name is "." or ".." meaning
        // ignore the current and the directory "above"
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char filepath[1024];

        // basically append the directory name to the filepath, i.e. path/to/directory/dirname
        snprintf(filepath, sizeof(filepath), "%s/%s", search_path, entry->d_name);

        struct stat file_stat;
        // check if file exists
        if (stat(filepath, &file_stat) == 0)
        {
            // check if file is a directory
            if (S_ISDIR(file_stat.st_mode))
            {
                // call searchDirectory recursively to parse through each directory
                searchDirectory(client_sd, filepath, filename);
            }
            else if (S_ISREG(file_stat.st_mode) && strcmp(entry->d_name, filename) == 0)
            {
                // if filesrch
                //  File found
                char response[1024];
                snprintf(response, sizeof(response), "File: %s\nSize: %ld bytes\nDate Created: %s", filepath, file_stat.st_size, ctime(&file_stat.st_ctime));
                write(client_sd, response, 1024);
                /*
                //if targzf
                // Check if the file has a matching extension
                const char *file_extension = strrchr(entry->d_name, '.');
                if (file_extension != NULL)
                {
                    for (int i = 0; i < extension_count; i++)
                    {
                        if (strcmp(file_extension, extension_list[i]) == 0)
                        {
                            // Write the file path to the file_list_file
                            fprintf(file_list_file, "%s\n", file_path);
                            break;
                        }
                    }
                }
                */
                closedir(dir);
                return;
            }
        }
    }

    closedir(dir);
}

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

void searchAndWriteFiles(FILE *file_list, const char *path, const char *extensions)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        perror("opendir");
        return;
    }

    // printf("path: %s",dir);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        /*
        if (snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name) >= sizeof(full_path)) {
            fprintf(stderr, "Full path is too long: %s/%s\n", path, entry->d_name);
            continue;
        }
        */

        // printf("full path: %s",full_path);

        struct stat entry_stat;
        if (stat(full_path, &entry_stat) != 0)
        {
            perror("stat");
            continue;
        }

        if (S_ISDIR(entry_stat.st_mode))
        {
            searchAndWriteFiles(file_list, full_path, extensions);
        }
        else
        {
            if (S_ISREG(entry_stat.st_mode))
            {
                char *ext = strrchr(entry->d_name, '.');
                if (ext != NULL && strstr(extensions, ext))
                {
                    fprintf(file_list, "%s\n", full_path);
                }
            }
        }
    }

    closedir(dir);
}

/*
    CLIENT COMMANDS
    ===============
*/

void fgets_command(int client_sd, char buff1[])
{
    char *file_list = buff1 + 6; // Extract the list of files from the command
    printf("List of files: %s\n", file_list);

    char *file_token = strtok(file_list, " ");
    int file_count = 0;
    char file_list_str[1024] = ""; // Buffer to store the file list

    while (file_token != NULL && file_count <= 4)
    {
        // Recursively search for the file in all folders
        searchFiles(file_list_str, getenv("HOME"), file_token, &file_count);

        file_token = strtok(NULL, " ");
        file_count++;
    }

    if (file_count > 0)
    {
        printf("Files found: %d\n", file_count);

        // Create a tar archive of all the files
        char tar_command[1024];
        // snprintf(tar_command, sizeof(tar_command), "tar -cf temp.tar %s", file_list_str);
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
            char *msg = "Finished tarring files";
            write(client_sd, msg, strlen(msg) + 1);
            send(client_sd, tar_buffer, tar_size, 0);

            free(tar_buffer);
        }
        else
        {
            char *error_msg = "Error creating tar.gz file";
            send(client_sd, error_msg, strlen(error_msg), 0);
        }
    }
    else
    {
        char *msg = "No files found";

        write(client_sd, msg, 50);
    }
}

void tarfgetz(char buff1[])
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
        printf("Wrong input format.\n");
        printf("Usage: tarfgetz minSize maxSize\n");
        printf("Where, minSize<maxSize");
    }
}

//=====filesrch filename=====
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

//======targzf <extension list> <-u>====
void targzf(int client_sd, char buff1[])
{
    const char *extension_list = buff1 + 7; // Extract the extension list from the command
    const char *home_dir = getenv("HOME");

    FILE *file_list = fopen("file_list.txt", "w");
    if (file_list == NULL)
    {
        perror("fopen");
        return;
    }

    // Recursively search for files with the specified extensions in the home directory
    searchAndWriteFiles(file_list, home_dir, extension_list);

    fclose(file_list);

    // Create a tar.gz archive from the file list
    char tar_command[1024];
    snprintf(tar_command, sizeof(tar_command), "tar -czf temp.tar.gz -T file_list.txt");

    system(tar_command);

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

    const char *msg = "Finished tarring files.";
    write(client_sd, msg, strlen(msg));

    // Clean up
    remove("file_list.txt");
}

/*
//=====processClient=======
main method for processing client requests
*/
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
    }
    // compare the first 6 characters with "fgets"
    else if (strncmp(buff1, "fgets ", 6) == 0)
    {
        // execute fgets command
        fgets_command(client_sd, buff1);
    }
    else if (strncmp(buff1, "tarfgetz ", 8) == 0)
    {
        // execute tarfgetz command
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
