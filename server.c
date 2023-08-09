#include <netinet/in.h> //structure for storing address information
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> //for socket APIs
#include <sys/types.h>
#include <string.h>
#include <time.h>

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

    printf("Server listening on port: %d...\n", portNumber);

    while (1)
    {
        con_sd = accept(lis_sd, (struct sockaddr *)NULL, NULL); // accept()

        /*
        char buff[50];
        printf("\nType your message to the client\n");
        scanf("%s",&buff);
        write(con_sd, buff, 50);
        */

        printf("Message from the client\n");
        char buff1[50];
        read(con_sd, buff1, 50);
        printf("%s\n", buff1);
        close(con_sd);
    }
}