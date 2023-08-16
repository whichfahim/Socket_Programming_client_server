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

void proxy_to_mirror(int client_sd)
{
    int mirror_fd;
    struct sockaddr_in mirror_addr;

    if ((mirror_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error for mirror connection");
        return;
    }

    mirror_addr.sin_family = AF_INET;
    mirror_addr.sin_port = htons(4500);
    if (inet_pton(AF_INET, "0.0.0.0", &mirror_addr.sin_addr) <= 0)
    {
        perror("Invalid mirror address / Address not supported");
        return;
    }

    if (connect(mirror_fd, (struct sockaddr *)&mirror_addr, sizeof(mirror_addr)) < 0)
    {
        perror("Connection to mirror failed");
        return;
    }

    // Relay data between client and mirror
    fd_set set;
    char buffer[1024];
    while (1)
    {
        FD_ZERO(&set);
        FD_SET(client_sd, &set);
        FD_SET(mirror_fd, &set);

        int max_fd = (client_sd > mirror_fd) ? client_sd : mirror_fd;

        if (select(max_fd + 1, &set, NULL, NULL, NULL) < 0)
        {
            perror("Select failed");
            break;
        }

        if (FD_ISSET(client_sd, &set))
        {
            int bytes_read = recv(client_sd, buffer, 1024, 0);
            if (bytes_read <= 0)
                break;
            send(mirror_fd, buffer, bytes_read, 0);
        }

        if (FD_ISSET(mirror_fd, &set))
        {
            int bytes_read = recv(mirror_fd, buffer, 1024, 0);
            if (bytes_read <= 0)
                break;
            send(client_sd, buffer, bytes_read, 0);
        }
    }
    close(mirror_fd);
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
                write(client_sd, response, strlen(response) + 1);
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
                // concatenates the filePath to the existing fileList
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
            // perror("stat");
        }
    }

    closedir(dir);
}

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

// targzf
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

void findFilesByExtensions(const char *extension_list, char **fileList)
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
            char *file_name = entry->d_name;
            char *extension = strrchr(file_name, '.'); // Get the file extension
            if (extension != NULL)
            {
                extension++; // Skip the dot in the extension

                // Convert the extensions to lowercase (or uppercase) for case-insensitive comparison
                char extension_lowercase[50];
                snprintf(extension_lowercase, sizeof(extension_lowercase), "%s", extension);
                for (int i = 0; extension_lowercase[i]; i++)
                {
                    extension_lowercase[i] = tolower(extension_lowercase[i]);
                }

                // Check if the file extension matches any of the provided extensions
                if (strstr(extension_list, extension_lowercase) != NULL)
                {
                    appendFilePath(fileList, file_name);
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

//=====fgets file1 file2 file3 file4========
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
    }

    if (file_count > 0)
    {
        printf("Files found: %d\n", file_count);

        // Create a tar archive of all the files
        char tar_command[1024];
        snprintf(tar_command, sizeof(tar_command), "tar -czf temp.tar.gz %s", file_list_str);

        const char *home_dir = getenv("HOME");
        chdir(home_dir); // Change to home directory
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

        // unsigned long long minSize = 0; // Minimum file size in bytes
        // unsigned long long maxSize = 1000; // Maximum file size in bytes

        char *fileList = (char *)malloc(1); // Start with an empty string
        if (fileList == NULL)
        {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        fileList[0] = '\0';

        findFilesInSizeRange(minSize, maxSize, &fileList, home_dir);

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
            chdir(home_dir); // Change to home directory

            // Create the tar.gz archive
            char *tmpFilePath = "/tmp/file_list.txt";
            execlp("tar", "tar", "-czvf", "temp.tar.gz", "-T", tmpFilePath, NULL);

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

        char *msg = "Finished tarring files";
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
    // Extract the extension list and -u flag from the command
    char *extension_list = buff1 + 7; // Skip "targzf "
    int unzip = 0;
    if (extension_list[0] == '-' && extension_list[1] == 'u')
    {
        unzip = 1;
        extension_list += 3; // Skip "-u "
    }

    // Tokenize the extension list by space
    char *extension_token = strtok(extension_list, " ");
    int extension_count = 0;
    char extension_list_str[1024] = ""; // Buffer to store the extension list

    // Takes up to 6 extensions as parameters
    while (extension_token != NULL && extension_count <= 6)
    {
        // Concatenate extension_list_str with extension_token
        strcat(extension_list_str, extension_token);
        // Insert space in between
        strcat(extension_list_str, " ");

        // only increment count if extension is not empty
        if (strlen(extension_token) > 0)
        {
            extension_count++;
        }

        extension_token = strtok(NULL, " ");
    }

    if (extension_count > 0)
    {
        printf("Extensions to search for: %s\n", extension_list_str);

        char *fileList = (char *)malloc(1); // Start with an empty string
        if (fileList == NULL)
        {
            perror("malloc");
            return;
        }
        fileList[0] = '\0';

        findFilesByExtensions(extension_list, &fileList);

        // Create a temporary file to store the list of files
        char tmpFilePath[] = "/tmp/file_list.txt";
        int tmpFile = open(tmpFilePath, O_CREAT | O_WRONLY, 0644);
        if (tmpFile == -1)
        {
            perror("open");
            free(fileList);
            return;
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
    else
    {
        char *msg = "No extensions provided";

        write(client_sd, msg, strlen(msg));
    }

    if (unzip)
    {
        // Send a signal to the client to indicate the -u flag
        char unzip_signal[10] = "-u";
        send(client_sd, unzip_signal, strlen(unzip_signal), 0);
    }
}

void getdirf(int client_sd, char buff1[])
{
    // Extract date1 and date2 from the command
    char date1[20], date2[20];
    // date1 (earlier)< date2
    sscanf(buff1 + 9, "%s %s", date1, date2);

    // Open the client's home directory
    const char *home_dir = getenv("HOME");
    DIR *dir = opendir(home_dir);
    if (dir == NULL)
    {
        perror("opendir");
        return;
    }

    // Iterate through the directory entries
    struct dirent *entry;
    char fileList[1024] = ""; // To store the list of files
    while ((entry = readdir(dir)) != NULL)
    {
        char filePath[1024];
        snprintf(filePath, sizeof(filePath), "%s/%s", home_dir, entry->d_name);

        struct stat file_stat;
        if (stat(filePath, &file_stat) == 0)
        {
            // Check if the file creation date is within the specified range
            time_t file_ctime = file_stat.st_ctime;
            struct tm *file_time = localtime(&file_ctime);
            struct tm date1_tm, date2_tm;
            strptime(date1, "%Y-%m-%d", &date1_tm);
            strptime(date2, "%Y-%m-%d", &date2_tm);

            if (difftime(mktime(file_time), mktime(&date1_tm)) >= 0 &&
                difftime(mktime(&date2_tm), mktime(file_time)) >= 0)
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

    char *msg = "Finished tarring files";
    write(client_sd, msg, strlen(msg) + 1);
    // printf("Tar.gz archive created: temp.tar.gz\n");

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
// end of client commands
//=======================

/*
//=====processClient=======
main method for processing client requests
*/
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
        tarfgetz(client_sd, buff1);
    }
    else if (strncmp(buff1, "filesrch ", 8) == 0)
    {
        // code here
        filesrch(client_sd, buff1);
    }
    else if (strncmp(buff1, "targzf ", 7) == 0)
    {
        // code here
        // const char *extension_list = buff1 + 6; // Extract the list of files from the command
        // printf("List of extensions: %s\n", extension_list);

        targzf(client_sd, buff1);
        /*
        // Tokenize the file list by space
        char *file_token = strtok(file_list, " ");
        int file_count = 0;
        char file_list_str[1024] = ""; // Buffer to store the file list
        */
    }
    else if (strncmp(buff1, "getdirf ", 7) == 0)
    {
        // code here
        getdirf(client_sd, buff1);
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

        printf("Server listening on port: %d...\n", portNumber);

        con_sd = accept(lis_sd, (struct sockaddr *)NULL, NULL); // accept()
        if (con_sd < 0)
        {
            perror("accept");
            continue;
        }

        /*if (numChildren >= 6)
        {
            // Limit the number of children to 6
            wait(NULL); // Wait for any child process to finish
            numChildren--;
        }*/

        if (numChildren > 6 && numChildren <= 2 * 6)
        {
            printf("Redirecting client %d to mirror.\n", numChildren);
            proxy_to_mirror(con_sd);
            close(con_sd);
            continue;
        }
        else if (numChildren > 2 * 6 && numChildren % 2 == 0)
        {
            printf("Redirecting client %d to mirror.\n", numChildren);
            proxy_to_mirror(con_sd);
            close(con_sd);
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

    int status;
    pid_t finished_child = wait(&status); // Wait for any child process to finish
    if (finished_child > 0)
    {
        numChildren--;
        printf("Child process with PID %d has finished.\n", finished_child);
    }
}