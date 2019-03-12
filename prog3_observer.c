#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>

/*------------------------------------------------------------------------
* Program: demo_client
*
* Purpose: allocate a socket, connect to a server, and print all output
*
* Syntax: ./demo_client server_address server_port
*
* server_address - name of a computer on which server is executing
* server_port    - protocol port number server is using
*
*------------------------------------------------------------------------
*/
int main( int argc, char **argv) {
	struct hostent *ptrh; /* pointer to a host table entry */
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
    struct timeval timeout;
	int sd; /* socket descriptor */
	int port; /* protocol port number */
	int n; /* number of characters read */
    int ready;
	char *host; /* pointer to host name */
	char buf[1000]; /* buffer for data from the server */
    char response;
    fd_set sdSet;

	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */

	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./client server_address server_port\n");
		exit(EXIT_FAILURE);
	}

	port = atoi(argv[2]); /* convert to binary */
	if (port > 0) {
		sad.sin_port = htons((u_short)port);
	} else {
		fprintf(stderr,"Error: bad port number %s\n",argv[2]);
		exit(EXIT_FAILURE);
	}

	host = argv[1]; /* if host argument specified */

	/* Convert host name to equivalent IP address and copy to sad. */
	ptrh = gethostbyname(host);
	if ( ptrh == NULL ) {
		fprintf(stderr,"Error: Invalid host: %s\n", host);
		exit(EXIT_FAILURE);
	}

	memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

	/* Map TCP transport protocol name to protocol number. */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create a socket. */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Connect the socket to the specified server. You have to pass correct parameters to the connect function.*/
	if (connect(sd, (struct sockaddr*) &sad, sizeof(sad)) < 0) {
		fprintf(stderr,"connect failed\n");
		exit(EXIT_FAILURE);
	}

    // Get Response
    recv(sd, &response, 1, 0);

    if (response == 'N') {
        printf("Could not connect to server, max number of observers reached.\n");
        close(sd);
        exit(EXIT_SUCCESS);
    } else if (response != 'Y') {
       fprintf(stderr, "Error: Invalid response from server.\n");
       close(sd);
       exit(EXIT_FAILURE);
    }

    // Get Username
    char username[10];
    uint8_t size;

    int connected = 0;
    while(!connected) {
        FD_SET(0, &sdSet);

        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        printf("Enter Username: ");
        fflush(stdout);

        ready = select(1, &sdSet, NULL, NULL, &timeout);

        if (ready < 0) {
            fprintf(stderr, "Error: Select.\n");
            close(sd);
            exit(EXIT_FAILURE);
        }

        if (!ready) {
            close(sd);
            exit(EXIT_SUCCESS);
        }

        size = read(0, username, 11) - 1;

        printf("\n");

        // Send Username
        send(sd, &size, sizeof(uint8_t), 0);
        send(sd, username, strlen(username), 0);

        // Wait for a response
        recv(sd, &response, 1, 0);

        if (response ==  'Y') {
            // Success
            connected = 1;

        } else if (response == 'N') {
            // No user with name found
            close(sd);
            exit(EXIT_SUCCESS);
        } else if (response == 'T') {
            // User already associated with an Observer

        } else {
            fprintf(stderr, "Error: Invalid response.\n");
            exit(EXIT_FAILURE);
        }
    }

    while (1) {
        char message[1015];
        char message_size;
        int ready;

        FD_ZERO(&sdSet);
        FD_SET(0, &sdSet);
        FD_SET(sd, &sdSet);

        ready = select(sd + 1, &sdSet, NULL, NULL, NULL);

        if (ready > 0) {

            if (FD_ISSET(0, &sdSet)) {

                fgets(message, 6, stdin);

                if (!strcmp(message, "/quit")) {
                    close(sd);
                    exit(EXIT_SUCCESS);
                }
            }

            if (FD_ISSET(sd, &sdSet)) {

                if (recv(sd, &message_size, sizeof(uint16_t), 0) <= 0) {
                    break;
                }
                if (recv(sd, &message, message_size, 0) <= 0) {
                    break;
                }

                message[message_size] = '\0';

                if (message[message_size - 1] == '\n') {
                    printf("%s", message);
                } else {
                    printf("%s\n", message);
                }
            }
        } else {
            printf ("Select error\n");
            exit(EXIT_FAILURE);
        }
    }
    
    printf("Server died\n");
    exit(EXIT_SUCCESS);
}
