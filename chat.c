#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <time.h>

#define SERVER_LISTEN_PORT 		44444
#define N_COMPLETE_QUEUE 		10
#define BUFSIZE 				1024
#define MAX_REGISTERED_USERS 	1024
#define MAX_ONLINE_USERS 		1024
#define SHMSZ     				27


typedef struct {
	char Nick[BUFSIZE];
	int isOnline;
} client_t;


int* nRegistered;
//int* nOnline;
client_t registeredUsers[MAX_REGISTERED_USERS];


client_t* shmem;
//client_t onlineUsers[MAX_ONLINE_USERS];


/* Client - MsgQ definitons */
struct sendData {
	char sender[BUFSIZE];
	char text[BUFSIZE];
	char time[BUFSIZE];
};

struct message {
	long message_type;
	struct sendData msg;
};

/* Serv - MsgQ definitons 
struct updateQuery {
	char sender[BUFSIZE];
	int isOnline;
	client_t registeredUsers[BUFSIZE];
};


struct servQuery {
	long message_type;
	struct updateQuery data;
};
*/

char serverPath[BUFSIZE];

void printOnlineUsers(int connfd) {
	char buffOut[BUFSIZE];
	
	sprintf(buffOut, "-----------ONLINE USERS --------\n");
	write(connfd, buffOut, strlen(buffOut));

	int nOnline=0;
	printf("nRegistered: %d\n", *nRegistered);
	for(int i=0; i<*nRegistered; i++) {
		if(shmem[i].isOnline==1) {
			sprintf(buffOut, "%s\n", shmem[i].Nick);
			write(connfd, buffOut, strlen(buffOut));
			//printf("%s\n", shmem[i].Nick);
			nOnline++;
		}
	}

	sprintf(buffOut, "Number of Users Online: %d\n", nOnline);
	write(connfd, buffOut, strlen(buffOut));
}

void printAllUsers(int connfd) {
	char buffOut[BUFSIZE];
	
	sprintf(buffOut, "-----------REGISTERED USERS --------\n");
	write(connfd, buffOut, strlen(buffOut));

	printf("nRegistered: %d\n", *nRegistered);
	for(int i=0; i<*nRegistered; i++) {
		sprintf(buffOut, "%s\n", shmem[i].Nick);
		write(connfd, buffOut, strlen(buffOut));
		//printf("%s\n", shmem[i].Nick);
	}

	sprintf(buffOut, "Number of Users Registered: %d\n", *nRegistered);
	write(connfd, buffOut, strlen(buffOut));
}

void Quit(int connfd, char nick[]) {
	char buffOut[BUFSIZE];
	printf("nRegistered: %d\n", *nRegistered);
	for(int i=0; i<*nRegistered; i++) {
		if(strcmp(nick, shmem[i].Nick) == 0) {
			shmem[i].isOnline = 0;
			sprintf(buffOut, "Bye %s :)\n", nick);
			write(connfd, buffOut, strlen(buffOut));
			exit(-1);
			break;
		}
	}
}


/* Reads a single line until \n through connfd */
int readLine(int connfd, char* buffIn) {
	char ch;
	int inPtr = 0, ret;
	while(1) {
		//printf("inPtr: %d, ch: %c", inPtr, ch);
		ret = read(connfd, &ch, 1);
		if(ret < 1) {
			break;
			// free(buffIn);
			// return -1;
		}
		if(ch=='\n' || ch=='\r') {
			buffIn[inPtr] = '\0';
			break;
		}
		buffIn[inPtr] = ch;
		inPtr++;
	}
	return inPtr;
}

void flushBuf(char buff[]) {
	for(int i=0; i<BUFSIZE; i++)
		buff[i] = '\0';
}

void getFirstToken(char* command, char* tempBuf) {
	int i=0;
	while(1) {
		if(tempBuf[i] == ' ' || tempBuf[i] == '\0') {
			command[i] = '\0';
			break;
		}
		command[i] = tempBuf[i];
		i++;
	}
}

void getSendNick(char* sendToNick, char* tempBuf) {
	int i=1, j=0;
	while(1) {
		if(tempBuf[i] == ' ' || tempBuf[i] == '\0') {
			sendToNick[j] = '\0';
			break;
		}
		sendToNick[j++] = tempBuf[i];
		i++;
	}
}

void getSendMessage(char* msg, char* tempBuf) {
	int i=0, j=0;
	while(1) {
		i++;
		if(tempBuf[i] == ' ') {
			i++;
			break;
		}
	}
	while(1) {
		if(tempBuf[i] == '\0') {
			msg[j] = '\0';
			break;
		}
		msg[j++] = tempBuf[i];
		i++;
	}
}

void sendToAll(char sendMessage[], char sender[], int connfd) {
	//printf("SENDTOALL INVOKED. HERE\n");
	for(int i=0; i<*nRegistered; i++) {
		
		char cwd[BUFSIZE], sendToPath[BUFSIZE], buffOut[BUFSIZE];
		//shmem[i].Nick

		if(getcwd(cwd, sizeof(cwd)) == NULL) {
			perror("getcwd error: ");
			printf("[-] getcwd ERROR\n");
			continue;
		}

		snprintf(sendToPath, sizeof(sendToPath), "%s/users/%s", cwd, shmem[i].Nick);
		// if(stat(sendToPath, &st) == -1) {
		// 	/* Bad Nick Specified */
		// 	flushBuf(buffOut);
		// 	sprintf(buffOut, "No Nick %s exists. Try Again.\n", sendToNick);
		// 	write(connfd, buffOut, strlen(buffOut));
		// 	continue;
		// }

		key_t sendToKey;
		if((sendToKey = ftok(sendToPath, 0)) == -1) {
			printf("[-] ftok failed for %s\n", sendToPath);
			perror("ftok: ");
			continue;
		}

		printf("%s\n", sendToPath);
		
		int sendToQID;
		if((sendToQID = msgget(sendToKey, 0777|IPC_CREAT)) == -1) {
			printf("[-] msgget failed for %s\n", shmem[i].Nick);
			perror("msgget: ");
			continue;
		}

		struct message msgqBuf;
		char timeBuf[BUFSIZE];
		strcpy(msgqBuf.msg.sender, sender);
		strcpy(msgqBuf.msg.text, sendMessage);
		time_t t = time(NULL);
		struct tm tm = *localtime(&t);
		sprintf(timeBuf, "%d-%d-%d %d:%d:%d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
		strcpy(msgqBuf.msg.time, timeBuf);
		msgqBuf.message_type=1;

		if(msgsnd(sendToQID, &msgqBuf, sizeof(struct sendData), 0) == -1) {
			printf("[-] MSGSEND FAILED TO %s", shmem[i].Nick);
			perror("msgsnd: ");
			continue;
		}

		flushBuf(buffOut);
		sprintf(buffOut, "Message sent to %s\n\n", shmem[i].Nick);
		write(connfd, buffOut, strlen(buffOut));
		continue;

	}
}

void help(int connfd) {
	char buffOut[BUFSIZE];
	sprintf(buffOut, "Default PortNo: 44444, can be changed in line 17 of chat.c\n\n");
	write(connfd, buffOut, strlen(buffOut));
	sprintf(buffOut, " -[Client Functions]- \n");
	write(connfd, buffOut, strlen(buffOut));
	sprintf(buffOut, "@<nick> <message>: \tSend message to User nick\n\t\t\t(If user is offline, message will be delivered on next login)\n");
	write(connfd, buffOut, strlen(buffOut));
	sprintf(buffOut, "@all <message>: \tBroadcase message to all registered users\n");
	write(connfd, buffOut, strlen(buffOut));
	sprintf(buffOut, "\\ShowAll: \t\tShow all Registered Users\n");
	write(connfd, buffOut, strlen(buffOut));
	sprintf(buffOut, "\\Online: \t\tShow all Online Users\n");
	write(connfd, buffOut, strlen(buffOut));
	sprintf(buffOut, "\\Help: \t\t\tbrings up this help box\n");
	write(connfd, buffOut, strlen(buffOut));
	sprintf(buffOut, "\\Quit: \t\t\tExit the chat room\n");
	write(connfd, buffOut, strlen(buffOut));
	sprintf(buffOut, "-------------------------------------\n\n");
	write(connfd, buffOut, strlen(buffOut));

}

void handleClient(int connfd) {
	char buffOut[BUFSIZE];
	char buffIn[BUFSIZE];
	char cwd[BUFSIZE], userPath[BUFSIZE];	
	int rlen;

	if(getcwd(cwd, sizeof(cwd)) == NULL) {
		perror("getcwd error: ");
		printf("[-] getcwd ERROR\n");
		close(connfd);
		exit(-1);
	}

	sprintf(buffOut, "[Chat Server] Enter Nick: ");
	write(connfd, buffOut, strlen(buffOut));
	readLine(connfd, buffIn);

	char nick[BUFSIZE];
	strcpy(nick, buffIn);
	
	snprintf(userPath, sizeof(userPath), "%s/users/%s", cwd, nick); //strcat wasn;t working. spent 1hr on this for no reason :(
	
	printf("%s\n", userPath);

	flushBuf(buffOut);
	flushBuf(buffIn);
	
	/* Check if User - Nick Exists */
	struct stat st = {0};
	if(stat(userPath, &st) == -1) { 

		/* User does NOT exist */
		printf("[#] User %s does NOT exist.\n", nick);
		if(mkdir(userPath, 0777)==0) {

			strcpy(shmem[*nRegistered].Nick, nick);
			shmem[*nRegistered].isOnline = 1;
			*nRegistered = *nRegistered + 1;

			sprintf(buffOut, "[+] New User %s Registered\n", nick);
			write(connfd, buffOut, strlen(buffOut));
		} else {
			printf("[-] mkdir UNSUCCESSFUL\n");
			perror("mkdir: ");
			close(connfd);
			exit(-1);
		}
	} else {
		/* User Exists */

		//set isOnline
		for(int i=0; i<*nRegistered; i++) {
			if(strcmp(shmem[i].Nick, nick)==0) {
				printf("Match Found: %d\n", i);
				shmem[i].isOnline = 1;
				break;
			}

		}

		sprintf(buffOut, "[+] Welcome Back, %s\n", nick);
		write(connfd, buffOut, strlen(buffOut));
		printf("[#] User %s EXISTs.\n", nick);
		
	}

	help(connfd);
	
	/* MSGQ - INIT */
	key_t clientKey;
	if((clientKey = ftok(userPath, 0)) == -1) {
		printf("[-] ftok failed on client\n");
		perror("ftok: ");
		exit(-1);
	}
	int clientQID;
	if((clientQID = msgget(clientKey, 0777|IPC_CREAT)) == -1) {
		printf("[-] msgget failed on client\n");
		perror("msgget: ");
		exit(-1);
	}

	printf("[+] Client Msgq Success\n");

	struct message msgqBuf;

	/* Display all pending messages */
	struct msqid_ds tempBuf;
	msgctl(clientQID, IPC_STAT, &tempBuf);
	uint pendingCount = (uint) tempBuf.msg_qnum;
	sprintf(buffOut, "You have %u unread messages:\n\n", pendingCount);
	write(connfd, buffOut, strlen(buffOut));
	for(int i=0; i<pendingCount; i++) {
		msgrcv(clientQID, &msgqBuf, sizeof(struct sendData), 0, 0);
		sprintf(buffOut, "SENDER:%s\nMESSAGE: %s\nTIME: %s\n", msgqBuf.msg.sender, msgqBuf.msg.text, msgqBuf.msg.time);
		write(connfd, buffOut, strlen(buffOut));
	}


	/* Chat Operation */
	while(1) {
		//printf("I'm running\n");

		msgctl(clientQID, IPC_STAT, &tempBuf);
		uint pendingCount = (uint) tempBuf.msg_qnum;
		if(pendingCount) {
			msgrcv(clientQID, &msgqBuf, sizeof(struct sendData), 0, 0);
			sprintf(buffOut, "SENDER:%s\nMESSAGE: %s\nTIME: %s\n", msgqBuf.msg.sender, msgqBuf.msg.text, msgqBuf.msg.time);
			write(connfd, buffOut, strlen(buffOut));
		}

		/* Setting Read() to Non-Blocking Mode */
		int flags = fcntl(connfd, F_GETFL, 0);
		fcntl(connfd, F_SETFL, flags | O_NONBLOCK);
		//flushBuf(buffIn);

		readLine(connfd, buffIn);
		char tempBuf[BUFSIZE], firstToken[BUFSIZE], sendToNick[BUFSIZE], sendMsg[BUFSIZE], sendToPath[BUFSIZE];
		
		strcpy(tempBuf, buffIn);
		
		getFirstToken(firstToken, tempBuf);
		getSendNick(sendToNick, tempBuf);
		getSendMessage(sendMsg, tempBuf);
		
		flushBuf(tempBuf);
		
		if(!firstToken)
			continue;

		/* Send Message */
		if(firstToken[0] == '@') {

			
			/* Check if user specified */
			if(*sendToNick == '\0') {
				flushBuf(buffOut);
				sprintf(buffOut, "No Nick supplied. Please Specify\n");
				write(connfd, buffOut, strlen(buffOut));
				continue;
			}

			/* Check if arg is Send to All */
			if(strcmp(sendToNick, "all")==0) {
				sendToAll(sendMsg, nick, connfd);
				continue;
			}


			/* Check if Nick exists */
			snprintf(sendToPath, sizeof(sendToPath), "%s/users/%s", cwd, sendToNick);
			if(stat(sendToPath, &st) == -1) {
				/* Bad Nick Specified */
				flushBuf(buffOut);
				sprintf(buffOut, "No Nick %s exists. Try Again.\n", sendToNick);
				write(connfd, buffOut, strlen(buffOut));
				continue;
			}

			key_t sendToKey;
			if((sendToKey = ftok(sendToPath, 0)) == -1) {
				printf("[-] ftok failed for %s\n", sendToPath);
				perror("ftok: ");
				continue;
			}

			printf("%s\n", sendToPath);
			
			int sendToQID;
			if((sendToQID = msgget(sendToKey, 0777|IPC_CREAT)) == -1) {
				printf("[-] msgget failed for %s\n", sendToNick);
				perror("msgget: ");
				continue;
			}

			strcpy(msgqBuf.msg.sender, nick);
			strcpy(msgqBuf.msg.text, sendMsg);
			msgqBuf.message_type=1;
			char timeBuf[BUFSIZE];
			time_t t = time(NULL);
			struct tm tm = *localtime(&t);
			sprintf(timeBuf, "%d-%d-%d %d:%d:%d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
			strcpy(msgqBuf.msg.time, timeBuf);	

			if(msgsnd(sendToQID, &msgqBuf, sizeof(struct sendData), 0) == -1) {
				printf("[-] MSGSEND FAILED TO %s", sendToNick);
				perror("msgsnd: ");
				continue;
			}

			flushBuf(buffOut);
			sprintf(buffOut, "Message sent to %s\n\n", sendToNick);
			write(connfd, buffOut, strlen(buffOut));
			continue;
		} 


		/* Special Function - Get Online Users */
		if(strcmp(firstToken, "\\Online")==0) {
			printf("%s\n", firstToken);
			printOnlineUsers(connfd);
		}


		/* Special Function - Quit */
		if(strcmp(firstToken, "\\Quit")==0) {
			printf("%s\n", firstToken);
			Quit(connfd, nick);
		}

		/* Special Function - Get All Users */
		if(strcmp(firstToken, "\\ShowAll")==0) {
			printf("%s\n", firstToken);
			printAllUsers(connfd);
		}

		/* Special Function - Help */
		if(strcmp(firstToken, "\\Help")==0) {
			//printf("%s\n", firstToken);
			help(connfd);
		}

		sleep(0.5); // To stop OverBurdening the server
	}

}

void* create_shared_memory(size_t size) {
	int protection = PROT_READ | PROT_WRITE;
	int visibility = MAP_ANONYMOUS | MAP_SHARED;
 	return mmap(NULL, size, protection, visibility, 0, 0);
}



int main(int argc, char* argv[]) {
	printf("**********************************\n");
	printf("*       CHAT.c - CHAT SERVER     *\n");
	printf("*           Assignment 2         *\n");
	printf("*   IS F462 NETWORK PROGRAMMING  *\n");
	printf("**********************************\n\n");
	printf("Default PortNo: 44444, can be changed in line 17 of chat.c\n\n");
	printf(" -[Client Functions]- \n");
	printf("@<nick> <message>:\tSend message to User nick\n\t\t\t(If user is offline, message will be delivered on next login)\n");
	printf("@all <message>:\t\tBroadcase message to all registered users\n");
	printf("\\ShowAll:\t\tShow all Registered Users\n");
	printf("\\Online:\t\tShow all Online Users\n");
	printf("\\Help:\t\t\tbrings up this help box\n");
	printf("\\Quit:\t\t\tExit the chat room\n");
	printf("-------------------------------------\n\n");


	int listenfd, connfd;
	pid_t childPid;
	struct sockaddr_in serverSocket, clientSocket;

	/* Server Socket definitions */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serverSocket.sin_family = AF_INET;
	serverSocket.sin_addr.s_addr = INADDR_ANY;
	serverSocket.sin_port = htons(SERVER_LISTEN_PORT);

	/* Socket Bind */
	if(bind(listenfd, (struct sockaddr*) &serverSocket, sizeof(serverSocket)) < 0) {
		perror("[-] Socket BIND FAILED. Try Again!\n");
		return -1;
	}
	printf("[+] Socket Bind Done\n");

	/* Listen */
	if(listen(listenfd, N_COMPLETE_QUEUE) < 0) {
		perror("[-] Socket LISTEN FAILED. Try Again!\n");
		return -1;
	}
	printf("[+] Socket Listening at Port %d\n", SERVER_LISTEN_PORT);
	printf("[+] **SERVER UP**\n");

	shmem = create_shared_memory(sizeof(registeredUsers));
	nRegistered = create_shared_memory(sizeof(int));
	*nRegistered = 0;
	memcpy(shmem, registeredUsers, sizeof(registeredUsers));

	char cwd[BUFSIZE];
	if(getcwd(cwd, sizeof(cwd)) == NULL) {
		perror("getcwd error: ");
		printf("[-] getcwd ERROR\n");
		exit(-1);
	}
	
	snprintf(serverPath, sizeof(serverPath), "%s/users", cwd); 
	mkdir(serverPath, 0777);

	/* Initiate Server Message Queue */
	key_t serverKey;
	if((serverKey = ftok(serverPath, 0)) == -1) {
		printf("[-] ftok failed on server\n");
		perror("ftok: ");
		exit(-1);
	}
	int serverQID;
	if((serverQID = msgget(serverKey, 0777|IPC_CREAT)) == -1) {
		printf("[-] msgget failed on server\n");
		perror("msgget: ");
		exit(-1);
	}

	/* Making accept() Non-Blocking 
	int flags = fcntl(listenfd, F_GETFL, 0);
	fcntl(listenfd, F_SETFL, flags | O_NONBLOCK);
	*/

	char tempBuf[BUFSIZE];
	/* Accept Clients */
	while(1) {
		//printf("SERVER IS RUNNING\n");
		socklen_t lenClientSocket = sizeof(clientSocket);

		connfd = accept(listenfd, (struct sockaddr*) &clientSocket, &lenClientSocket);
		if(connfd>0) {
			if(childPid = fork() == 0) {
				close(listenfd);
				
				handleClient(connfd);

				exit(0);
			}
		}

		close(connfd);


		sleep(1); // To stop overburdening the server

	}

	return 0;
}