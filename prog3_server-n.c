#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#define QLEN 6 /* size of request queue */
#define MAX_CLIENTS 255 /* Max number of participants & clients */

const char n = 'N';
const char y = 'Y';
const char t = 'T';

/*------------------------------------------------------------------------
* Program: prog3_server
*
* Purpose: allocate a socket and then repeatedly execute the following:
*
*
* Syntax: ./prog3_server parPort obsPort
*
* port - protocol port number to use
*
*------------------------------------------------------------------------
*/

typedef struct participantStruct {
	int parSD;
	int obsSD;
	int active; /* 0 is inactive, 1 is active */
	uint8_t usernameSize;
	uint16_t messageSize;
	char username[11];
	char message[1014] ;
} participantStruct;

// New Clients
int handleNewParticipant(int sd);
int handleNewObserver(int sd);

// Client Disconnect
int handleParticipantDisconnect(int i);
int handleObserverDisconnect(int i);

// Messaging
int handlePublicMessages(char message[], uint16_t messageSize);
int handlePrivateMessages(char message[], uint16_t messageSize, int sender);
int handleNewMessage(int i);

// I/O
int sendMessage(int parID, char* message, uint16_t messageSize);
int receiveUsername(int index, char message[], uint8_t* size, int exists);
int readn(int socket, void* buffer, int maxBytes);

// Helper Functions
int handleNewUsername(int i);
int checkUsername(char username[]);
int addParticpant(participantStruct* participant);
int getParticipantByName(char* username);
void resetFdSet(int parSD, int obsSD);
void modifyParticipant(int i, void* buffer, int bufferSize);

// Debug Printing
void printParticipants();
void printParticipant(participantStruct* participant);

int numParticipants = 0;
int numObservers = 0;
int maxSD = 0;
participantStruct* participants[MAX_CLIENTS] = { NULL };
fd_set fdSet;

int main(int argc, char **argv) {
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in cad; /* structure to hold server's address */
	struct sockaddr_in pad; /* structure to hold client's address */
	struct sockaddr_in oad; /* structure to hold client's address */
	int parSD, obsSD;
	socklen_t alen; /* length of address */
	int optval = 1; /* boolean value when we set socket option */
	int obsPort; /* protocol port number */
 	int parPort; /* protocol port number */
	char buf[1011]; /* buffer for string the server sends */

	struct sockaddr_in sad; /* structure to hold server's address */
	int port; /* protocol port number */

	if (argc != 3) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./prog3_server parPort obsPort \n");
		exit(EXIT_FAILURE);
	}

	// Clear sockaddr structures
	memset((char *)&pad, 0, sizeof(pad));
	memset((char *)&oad, 0, sizeof(oad));

	// Set socket family to AF_INET
	pad.sin_family = AF_INET;
	oad.sin_family = AF_INET;

	// Set local IP address to listen to all IP addresses this server can assume. You can do it by using INADDR_ANY
	pad.sin_addr.s_addr = INADDR_ANY;
	oad.sin_addr.s_addr = INADDR_ANY;

	// Convert to binary
	parPort = atoi(argv[1]);
	obsPort = atoi(argv[2]);

	// Test for illegal value
	if (obsPort < 0 || parPort < 0) {
    fprintf(stderr,"Error: Bad port number %s\n",argv[1]);
		exit(EXIT_FAILURE);
	} else {
		// Set port number. The data type is u_short
		pad.sin_port = htons(parPort);
		oad.sin_port = htons(obsPort);
	}

	// Map TCP transport protocol name to protocol number
	if (((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	// Create a socket with AF_INET as domain, protocol type as SOCK_STREAM, and protocol as ptrp->p_proto. This call returns a socket descriptor named parSD.
	parSD = socket(AF_INET, SOCK_STREAM, ptrp->p_proto);
	if (parSD < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}
    obsSD = socket(AF_INET, SOCK_STREAM, ptrp->p_proto);
    if (obsSD < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	// Allow reuse of port - avoid "Bind failed" issues
	if (setsockopt(parSD, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
		fprintf(stderr, "Error Setting socket option failed\n");
		exit(EXIT_FAILURE);
	}

  	if (setsockopt(obsSD, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
		fprintf(stderr, "Error Setting socket option failed\n");
		exit(EXIT_FAILURE);
	}

	// Bind a local address to the socket.
	if (bind(parSD, (struct sockaddr*) &pad, sizeof(pad)) < 0) {
		fprintf(stderr,"Error: Bind failed\n");
		exit(EXIT_FAILURE);
	}

	if (bind(obsSD, (struct sockaddr*) &oad, sizeof(oad)) < 0) {
		fprintf(stderr,"Error: Bind failed\n");
		exit(EXIT_FAILURE);
	}

	// Specify size of request queue.
	if (listen(parSD, QLEN) < 0) {
		fprintf(stderr,"Error: Listen failed\n");
		exit(EXIT_FAILURE);
	}

  	if (listen(obsSD, QLEN) < 0) {
		fprintf(stderr,"Error: Listen failed\n");
		exit(EXIT_FAILURE);
	}


	maxSD = (parSD < obsSD) ? obsSD : parSD;

	while(1) {
		int ready;

		resetFdSet(parSD, obsSD);

		printf("Max SD: %d\n", maxSD);
		fflush(stdout);

		ready = select(maxSD + 1, &fdSet, NULL, NULL, NULL);
		printf("\n");

		// Error with Select
		if (ready < 0) {
			fprintf(stderr, "Error: Select");
			exit(EXIT_FAILURE);
		}

		for (int i = 0; i < MAX_CLIENTS; i++) {
			//Participant exists
			if (participants[i]) {
				participantStruct* participant = participants[i];

				// Data ready to be read on observer SD
				if (FD_ISSET(participant->obsSD, &fdSet)) {
					char junk;

					// Observer Disconnected
					if (readn(participant->obsSD, &junk, 1) <= 0) {
						handleObserverDisconnect(i);
					}
				}

				// Data ready to be read on participant SD
				if (FD_ISSET(participant->parSD, &fdSet)) {
					void* buffer;

					// Read up to 16 bytes
				  printf("About to read from participants[%d]->parSD...\n", participant->parSD);
					int bytesRead = readn(participant->parSD, buffer, 16);
					printf("Done! Read %d bytes!\n", bytesRead);

					if (bytesRead <= 0) {
						handleParticipantDisconnect(i);
					}
					// Process Bytes
					modifyParticipant(i, buffer, bytesRead);
				}
			}
		}

		// Check for new participants
		if (FD_ISSET(parSD, &fdSet)) {
			printf("New Participant: %d\n", parSD);
			alen = sizeof(parPort);
			int newFD = accept(parSD, (struct sockaddr *)&pad, &alen);
			fcntl(newFD, F_SETFL, O_NONBLOCK);
			handleNewParticipant(newFD);
		}

		// Check for new observers
		if (FD_ISSET(obsSD, &fdSet)) {
			printf("New Observer: %d\n", obsSD);
			alen = sizeof(obsPort);
			int newFD = accept(obsSD, (struct sockaddr *)&oad, &alen);
			fcntl(newFD, F_SETFL, O_NONBLOCK);
			handleNewObserver(newFD);
		}
	}
}

// -1 = error, 0 = max clients, 1 = success
int handleNewParticipant(int sd) {

	// Check if at capacity
	if (numParticipants == MAX_CLIENTS) {
		if (send(sd, &n, 1, 0) <= 0) {
			close(sd);
			return -1;
		}

		close(sd);
		return 0;
	}

	// Send confirmation
	if (send(sd, &y, 1, 0) <= 0) {
		close(sd);
		return -1;
	}

	// Allocate new participant
	participantStruct* newParticipant = malloc(sizeof(participantStruct));
	newParticipant->parSD = sd;
	newParticipant->obsSD = -1;
	newParticipant->usernameSize = -1;
	memset(newParticipant->username, 0, 11);
	newParticipant->messageSize = -1;
	//newParticipant->messageSize = { 0 };
	newParticipant->active = 0;

	// Add Participant
	addParticpant(newParticipant);
}

// -1 = error, 0 = failed (max capacity, invalid name, observer exists), 1 = success
int handleNewObserver(int sd) {
	uint8_t usernameSize;
	char username[11];
	participantStruct* participant;

	// Check Capacity
	if (numParticipants == MAX_CLIENTS) {
		// Send Rejection
		if (send(sd, &n, 1, 0) <= 0) {
			close(sd);
			return -1;
		}

		// Close Socket
		close(sd);
		return 0;
	}

	// Send Confirmation
	if (send(sd, &y, 1, 0) <= 0) {
		close(sd);
		return -1;
	}

	// Re-evaluate Max Socket descriptor
	maxSD = (sd < maxSD) ? maxSD : sd;

	// Get Username
	recv(sd, &usernameSize, sizeof(uint8_t), 0);
	recv(sd, username, usernameSize, 0);


	username[usernameSize] = '\0';

	// Get participant with given name
	int index = getParticipantByName(username);

	// No participant with name found
	if (index < 0) {
		// Send Rejection
		if (send(sd, &n, 1, 0) <= 0) {
			close(sd);
			return -1;
		}
		return 0;
	}

	participant = participants[index];

	// Participant with name found
	if (participant->obsSD < 0) {
		// Send Confirmation
		if (send(sd, &y, 1, 0) <= 0) {
			close(sd);
			return -1;
		}

		// Update participant's info
		participant->obsSD = sd;

		// Increment observers
		numObservers++;

		// Send Observer Message
		uint8_t size = 26;
		char message[size];

		sprintf(message, "A new observer has joined");

		handlePublicMessages(message, size);

		return 1;
	}

	// Participant with name already has an observer
	if (send(sd, &t, 1, 0) <= 0) {
		close(sd);
		return -1;
	}
	return 0;
}

int handleParticipantDisconnect(int i) {
	uint16_t messageSize = 24;
	char message[messageSize];
	sprintf(message, "User %s has left", participants[i]->username);
	handlePublicMessages(message, messageSize);

	// Close Sockets
	close(participants[i]->parSD);

	if (participants[i]->obsSD > 0) {
		handleObserverDisconnect(i);
	}

	// Free the memory
	free(participants[i]);

	// Free participant
	participants[i] = NULL;

	// Decrement clients
	numParticipants--;

	printf("participant disconnected\n");
}

int handleObserverDisconnect(int i) {
	printParticipants();
	// Close Sockets
	close(participants[i]->obsSD);

	// Free observer
	participants[i]->obsSD = -1;

	// Decrement Observers
	numObservers--;
	printf("observer disconnected\n");
}

// Send message to all observers
int handlePublicMessages(char message[], uint16_t messageSize) {
	printf("Public message\n");

	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (participants[i]) {
			// Get participant's observer SD
			int sd = participants[i]->obsSD;

			// Check if user has an observer
			if (sd >= 0) {
				// Send Message
				sendMessage(i, message, messageSize);
			}
		}
	}
}

int handlePrivateMessages(char* message, uint16_t messageSize, int sender) {
	char username[11];

	int i;
	for (i = 0; message[15 + i] != ' '; i++) {
		username[i] = message[15 + i];
	}
	username[i] = 0;

	int index = getParticipantByName(username);
	if (index >= 0) {
		if (sendMessage(index, message, messageSize) < 0) {
			return 0;
		}
	} else {
		sprintf(message, "Warning: user %s doesn't exist...", username);
		messageSize = 41;
	}

	return sendMessage(sender, message, messageSize);
}

// -1 = error, 0 = client disconnected, 1 = success
int handleNewMessage(int i) {
	char message[1000];
	char newMessage[1014];
	uint16_t messageSize;

	// Get Message size
	/*
	if (recv(participants[i]->parSD, &messageSize, sizeof(uint16_t), 0) <= 0) {
		handleParticipantDisconnect(i);
		return 0;
	}
	*/

	if (messageSize > 1000) {
		// Message too large
		printf("messageSize: %d\n", messageSize);
		handleParticipantDisconnect(i);
		return 0;
	}

	// Get Message
	/*
	if (recv(participants[i]->parSD, message, messageSize, 0) <= 0) {
		handleParticipantDisconnect(i);
		return 0;
	}
	*/


	//printf("before: %s\n", message);
	sprintf(newMessage, ">%11s: %s", participants[i]->username, message);
	messageSize += 14;
	//newMessage[messageSize] = 0;
	//printf("after: %s\n", newMessage);

	// Check if private message
	if (message[0] == '@') {
		newMessage[0] = '-';
		return handlePrivateMessages(newMessage, messageSize, i);
	}

	// Public message
	return handlePublicMessages(newMessage, messageSize);
}

int handleNewUsername(int i) {
	char username[11];
	uint8_t usernameSize;
	int valid;
	participantStruct* participant = participants[i];

	// Get Username
	/*
	if (receiveUsername(i, username, &usernameSize, 1) < 0) {
		return -1;
	}

	username[usernameSize] = '\0';
	*/

	// Check if name is valid and available
	valid = checkUsername(participant->username);

	if (valid > 0) {
		// Send Confirmation
		if (send(participant->parSD, &y, 1 ,0) <= 0) {
			close(participant->parSD);
			return -1;
		}

		// Update Participant
		//strncpy(participant->username, username, usernameSize+1);
		participant->active = 1;


		uint16_t size = strlen(participant->username) + 16;

		char message[size];
		sprintf(message, "User %s has joined", participant->username);
		printf("%s\n", message);

		// Send connection message
		handlePublicMessages(message, size);

	} else if (valid < 0) {
		// Invalid Name
		if (send(participant->parSD, &n, 1, 0) <= 0) {
			close(participant->parSD);
		}
	} else {
		// Username Taken
		if (send(participant->parSD, &t, 1, 0) <= 0) {
			close(participant->parSD);
		}
	}
}

int sendMessage(int parID, char* message, uint16_t messageSize) {
	if (send(participants[parID]->obsSD, &messageSize, sizeof(uint16_t), 0) < 0) {
		handleObserverDisconnect(parID);
		return -1;
	}
	if (send(participants[parID]->obsSD, message, messageSize, 0) < 0) {
		handleObserverDisconnect(parID);
		return -1;
	}

	return 0;
}

int receiveUsername(int index, char message[], uint8_t* size, int exists) {
	printf ("name size: %d\n", (int)*size);

	if (recv(participants[index]->parSD, size, sizeof(uint8_t), 0) <= 0) {
		if (exists) {
			handleParticipantDisconnect(index);
			return -1;
		}
	}

	if (recv(participants[index]->parSD, message, *size, 0) <= 0) {
		if (exists) {
			handleParticipantDisconnect(index);
			return -1;
		}
	}

	return 0;
}

// valid = 1, invalid = -1, taken = 0
int checkUsername(char username[]) {
	int result;
	char c;
	int size = strlen(username);
	int participantsChecked = 0;

	// Check Validity
	for (int i = 0; i < size; i++) {
		c = username[i];
		if (!isalnum(c) && !(c == '_')) {
			return -1;
		}
	}

	// Check Availability
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (participants[i] && participants[i]->active) {
			if (!strcmp(participants[i]->username, username)) {
				return 0;
			}
			participantsChecked++;

			if (participantsChecked >= numParticipants) {
				break;
			}
		}
	}

	// Name is valid and available
	return 1;
}


// Returns 0 if successfully added partipant
int addParticpant(participantStruct* participant) {
	numParticipants++;
	maxSD = (participant->parSD < maxSD) ? maxSD : participant->parSD;

	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (!participants[i]) {
			participants[i] = participant;
			return i;
		}
	}

	return -1;
}

int getParticipantByName(char username[]) {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (participants[i] && !strcmp(participants[i]->username, username)) {
			return i;
		}
	}
	return -1;
}

void resetFdSet(int parSD, int obsSD) {
	FD_ZERO(&fdSet);
	FD_SET(parSD, &fdSet);
	FD_SET(obsSD, &fdSet);
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (participants[i]) {
			printf("PSD: %d\n",participants[i]->parSD);
			FD_SET(participants[i]->parSD, &fdSet);
			if (participants[i]->obsSD > 0) {
				FD_SET(participants[i]->obsSD, &fdSet);
			}
		}
	}
}

// Reads data from socket until max is received
// Return number of bytes read
int readn(int socket, void* buffer, int maxBytes) {
	char* bufferLocal;
	int size, bytesRead;

	bufferLocal = buffer;
	bytesRead = 0;

	while (bytesRead < maxBytes) {
		size = read (socket, bufferLocal+bytesRead, maxBytes-bytesRead);
		if (size <= 0) {
			if (bytesRead == 0) {
				return size;
			}
			break;
		}
		bytesRead += size;
	}
	return bytesRead;
}

void printParticipants () {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (participants[i]) {
			printParticipant(participants[i]);
		}
	}
}

void printParticipant (participantStruct* participant) {
	printf ("%s: \tpar:%d\tobs:%d\n", participant->username, participant->parSD, participant->obsSD);
}

void modifyParticipant(int i, void* buffer, int bufferSize) {
	participantStruct* participant = participants[i];
	int offset = 0;

	printf("Checking where to store buffer\n");
	printf("Buffer is :%s\n", (char*)buffer);
	printf("Message Size: %d\n", (int)participant->messageSize);
	// Buffer is usernameSize
	if (participant->usernameSize > 100) { //UNSIGNED int 8
		printf("size\n");
		participant->usernameSize = *(uint8_t*)buffer;
		offset++;
	}
	if (strlen(participant->username) < participant->usernameSize && offset < bufferSize) {
		printf("username\n");
		strncpy(participant->username, (char *)(buffer + offset), participant->usernameSize);
		offset += participant->usernameSize;
		handleNewUsername(i);
	}
	if (participant->messageSize > 30000 && offset < bufferSize) { //UNSIGNED int 16
		printf("msgSize\n");
		participant->messageSize = *(uint16_t*)(buffer + offset);
		offset += 2;
	}
	if (strlen(participant->message) < participant->messageSize && offset < bufferSize) {
		printf("msg\n");
		int index = strlen(participant->message);
		strncpy(participant->message + index, (char *)(buffer + offset), bufferSize - offset);

		// Send Message if at max
		if (index + bufferSize >= participant->messageSize) {
			handleNewMessage(i);
			memset(participant->message, 0, participant->messageSize);
			participant->messageSize = -1;
		}
	}
}
