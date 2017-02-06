#include "connection.h"

int initServer() {

	/* Structuri de date necesare pentru conexiune. */
	struct sockaddr_in server;
	struct sockaddr_in from;

	int sd;
	/* Creez socket-ul prin care se va face accept. */
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		printf("[server]Eroare la crearea socket-ului!\n");
		exit(-1);
	}

	/* utilizarea optiunii SO_REUSEADDR */
  	int on = 1;
  	setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
  	
  	/* Initializez structurile sockaddr. */
  	bzero (&server, sizeof (server));
  	bzero (&from, sizeof (from));

  	/* Setez familia */
  	server.sin_family = AF_INET;
  	/* Accept conexiune la orice adresa disponibila. */
  	server.sin_addr.s_addr = htonl(INADDR_ANY);
  	/* Accept conexiunile la portul PORT. */
  	server.sin_port = htons(PORT);

  	/* Realizam bind-ul intre socket si structura sockaddr */
  	if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) {
  		printf("[server]Eroare la bind!\n");
  		exit(-1);
  	}
  	/* Incepem ascultarea pentru conexiuni. */
  	if (listen(sd, MAXCLIENTS) == -1) {
  		printf("[server]Eroare la listen!\n");
  		exit(-1);
  	}

  	return sd;
}

int connectToServer(char *ip, char *port) {
	int sd;			
  	struct sockaddr_in server;	
  	int serverPort = atoi(port);

  	/* Cream socket-ul clientului. */
  	if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1) {
    	printf("[client]Eroare la socket().\n");
    	exit(-1);
    }

    /* Setam datele server-ului. */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_port = htons(serverPort);

    /* Realizam conexiunea. */
    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) {
    	printf("[client]Eroare la connect!\n");
    	exit(-1);
    }

    return sd;
}

struct command receiveCommand(int sd, int key) {
	struct command commandReceived;
	/* Citesc din socket numele comenzii. */
	if ((commandReceived.commandName = readFromFdWithTPP(sd, key)) == 0) {
		printf("[server]Eroare la citirea numelui comenzii!\n");
		commandReceived.commandName = (char *)malloc(MAXCOMMANDLENGTH * sizeof(char));
		strcpy(commandReceived.commandName, "Error");
		return commandReceived;
	}
	int x;
	char c;
	/* Citesc caracterul junk care ramane in buffer. */
	if (read(sd, &c, sizeof(char)) < 1) {
		printf("[server]Eroare la citirea caracterului junk!\n");
		strcpy(commandReceived.commandName, "Error");
		return commandReceived;
	}
	/* Citesc numarul de parametrii. */
	if (read(sd, &x, sizeof(int)) < sizeof(int)) {
		printf("[server]Eroare la citirea nrParameters!\n");
		strcpy(commandReceived.commandName, "Error");
		return commandReceived;
	}
	/* Il convertesc la formatul host-ului. */
	x = ntohl(x);
	commandReceived.nrParameters = x;
	commandReceived.parameters = (char **)malloc(MAXNRPARAMETERS * sizeof(char *));
	/* Citesc toti parametrii. */
	for (int i = 0; i < commandReceived.nrParameters; i++) {
		commandReceived.parameters[i] = readFromFdWithTPP(sd, key);
		/* Citesc caracterul junk care ramane in buffer. */
		if (read(sd, &c, sizeof(char)) < 1) {
			printf("[server]Eroare la citirea caracterului junk!\n");
			strcpy(commandReceived.commandName, "Error");
			return commandReceived;
		}
	}

	return commandReceived;
}

int sendCommand(int sd, struct command commandToBeSent, int key) {
	/* Trimit numele comenzii. */
	if (writeInFdWithTPP(sd, commandToBeSent.commandName, key) == 0) {
		return -1;
	}

	/* Trimit numarul de parametri convertit la formatul retea. */
	commandToBeSent.nrParameters = htonl(commandToBeSent.nrParameters);
	if (write(sd, &commandToBeSent.nrParameters, sizeof(int)) == 0) {
		return -1;
	}
	commandToBeSent.nrParameters = ntohl(commandToBeSent.nrParameters);
	/* Trimit parametru cu parametru. */
	for (int i = 0; i < commandToBeSent.nrParameters; i++) {
		
		if (writeInFdWithTPP(sd, commandToBeSent.parameters[i], key) == 0) {
			return -1;
		}
	}
	return 0;
}

int writeInFd(int fd, char *buff) {
	// NU ESTE UTILIZATA
	int n = strlen(buff);
	if (write(fd, &n, 4) < 4) {
		printf("Eroare la scriere!\n");
		return 0;
	}
	if (write(fd, buff, n + 1) < n + 1) {
		printf("Eroare la scriere!\n");
		return 0;
	}
	return 1;
};

char *readFromFd(int fd) {
	// NU ESTE UTILIZATA
	int n, m;
	if (read(fd, &n, 4) < 4) {
		return 0;
	}
	if (n == 0) {
		return 0;
	}
	fflush(stdout);
	char *buff = (char *)malloc(n * sizeof(char));
	if (read(fd, buff, n) < n) {
		return 0;
	}

	buff[n] = 0;

	return buff;
};

int writeInFdWithTPP(int fd, char *buff, int key) {
	
	/* PAS 1: Criptez si trimit mesajul */
	int n = strlen(buff);

	encryptData(buff, strlen(buff), key);

	if (write(fd, &n, 4) < 4) {
		printf("Eroare la scriere!\n");
		return 0;
	}
	if (write(fd, buff, n + 1) < n + 1) {
		printf("Eroare la scriere!\n");
		return 0;
	}
	decryptData(buff, strlen(buff), key);

	/* PAS 2: Citesc mesajul criptat de 2 ori. */
	int length, m;
	if (read(fd, &length, 4) < 4) {
		return 0;
	}
	if (length == 0) {
		return 0;
	}
	char *buffEncrypted = (char *)malloc(length * sizeof(char));
	if (read(fd, buffEncrypted, length) < length) {
		return 0;
	}
	buffEncrypted[length] = 0;

	char c;
	if (read(fd, &c, sizeof(char)) < 0) {
		printf("Erorr while reading the junk character!\n");
	}

	/* PAS 3: Decriptez mesajul si il trimit mai departe. */
	n = strlen(buffEncrypted);
	decryptData(buffEncrypted, n, key);
	if (write(fd, &n, 4) < 4) {
		printf("Eroare la scriere!\n");
		return 0;
	}
	if (write(fd, buffEncrypted, n + 1) < n + 1) {
		printf("Eroare la scriere!\n");
		return 0;
	}

	return 1;
};

char *readFromFdWithTPP(int fd, int key) {
	/* Citesc mesajul criptat */
	int n, m;
	if (read(fd, &n, 4) < 4) {
		return 0;
	}
	if (n == 0) {
		return 0;
	}
	char *buff = (char *)malloc(n * sizeof(char));
	if (read(fd, buff, n) < n) {
		return 0;
	}

	buff[n] = 0;

	char c;
	if (read(fd, &c, sizeof(char)) < 0) {
		printf("Erorr while reading the junk character!\n");
	}

	/* Criptez din nou mesajul si il trimit prin fd. */
	encryptData(buff, n, key);
	if (write(fd, &n, 4) < 4) {
		printf("Eroare la scriere!\n");
		return 0;
	}
	if (write(fd, buff, n + 1) < n + 1) {
		printf("Eroare la scriere!\n");
		return 0;
	}

	/* Citesc mesajul si il decriptez obtinand mesajul corect. */
	if (read(fd, &n, 4) < 4) {
		return 0;
	}
	if (n == 0) {
		return 0;
	}
	if (read(fd, buff, n) < n) {
		return 0;
	}

	buff[n] = 0;

	decryptData(buff, n, key);

	return buff;
};

void encryptData(char *data, int length, int key) {
	/* Criptez datele conform cifrului lui Cezar. */
	for (int i = 0; i < length; i++) {
		data[i] += key;
	}
}

void decryptData(char *data, int length, int key) {
	/* Decriptez datele conform cifrului lui Cezar. */
		for (int i = 0; i < length; i++) {
		data[i] -= key;
	}
}

void readJunkFromFD(int fd) {
	/* Fac read non-blocant. */
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
	char *buff = malloc(MAXJUNK * sizeof(char));
	read(fd, buff, MAXJUNK);
	/* Refac read blocant. */
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);

	/* Eliberez memoria. */
	free(buff);
}