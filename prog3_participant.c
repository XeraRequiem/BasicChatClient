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
	int sd; /* socket descriptor */
	int port; /* protocol port number */
	char *host; /* pointer to host name */
	int n; /* number of characters read */
	char buf[1000]; /* buffer for data from the server */
    fd_set rfds;
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    FD_ZERO(&rfds);
    FD_SET(0, &rfds);


	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */

	if (argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./client server_address server_port\n");
		exit(EXIT_FAILURE);
	}

	port = atoi(argv[2]); /* convert to binary */

    // Test for legal value
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

    char max;
    recv(sd, &max, 1, 0);

    // Y if not max, N if max
    if (max == 'N') {
        printf("Server is full\n");
        close(sd);
        exit(EXIT_SUCCESS);
    }

    char unique = 'I';
    while(unique != 'Y') {
        char username[10];
        uint8_t username_size;
        int retval;
        
        // Ask for username
        printf("Enter a username: ");
        fflush(stdout);

        // Enter Username (10 second window)
        retval = select(1, &rfds, NULL, NULL, &tv);

        if (retval == -1) {
		    fprintf(stderr, "Error: Problem with select.\n");
		    exit(EXIT_FAILURE);
        }

        if (retval) {
            // Word entered within time limit
            username_size = read(0, username, 15) - 1;
        } else {
            // No Word entered within time limit
            close(sd);
            exit(EXIT_SUCCESS);
        }

        if (username_size > 10 || username_size <= 0) {
            // clear stdin
            char c;
            while ((c = getchar()) != '\n' && c != EOF) { }
            continue;
        }

        // Send size to server
        send(sd, &username_size, sizeof(uint8_t), 0);

        // Send name to server
        send(sd, username, username_size, 0);

        // recv unique or not
        recv(sd, &unique, 1, 0);

        if (unique == 'T') {
            //printf("Username already taken.\n");
            // Reset timer
            tv.tv_sec = 10;
            tv.tv_usec = 0;
        } else if (unique == 'I') {
            //printf("Invalid Username.\n");
        }
    }

    // Send Messages
    while(1) {
        char message[1000];
        uint16_t size;

        printf("Enter your message: ");
        fflush(stdout);

        size = read(0, message, 1001) - 1;

        message[size] = 0;

        // check for '/quit'
        if (!strcmp(message, "/quit")) {
            close(sd);
            exit(EXIT_SUCCESS);
        }

        send(sd, &size, sizeof(uint16_t), 0);
		send(sd, message, size, 0);
    }
}
