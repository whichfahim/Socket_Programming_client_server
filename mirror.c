#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define IPC_SOCKET_PATH "/tmp/ipc_socket"

void handle_client(int client_sd)
{
    // Your client handling logic here
    // For demonstration purposes, let's just print a message
    printf("Mirror: Handling client socket descriptor %d\n", client_sd);
    close(client_sd); // Close the client socket
}

int main()
{
    int ipc_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ipc_socket == -1)
    {
        perror("ipc_socket");
        return 1;
    }

    struct sockaddr_un ipc_addr;
    memset(&ipc_addr, 0, sizeof(ipc_addr));
    ipc_addr.sun_family = AF_UNIX;
    strncpy(ipc_addr.sun_path, IPC_SOCKET_PATH, sizeof(ipc_addr.sun_path) - 1);

    if (connect(ipc_socket, (struct sockaddr *)&ipc_addr, sizeof(ipc_addr)) == -1)
    {
        perror("ipc_connect");
        return 1;
    }

    int client_sd;
    recv(ipc_socket, &client_sd, sizeof(client_sd), 0);

    // Handle the client connection
    handle_client(client_sd);

    close(ipc_socket);
    return 0;
}
