#include "transferFiles.h"

/*
* Returneaza structura de tip command pentru login pentru user-ul citit.
*/
struct command setLoginCommand(struct user myUser);

/*
* Verifica argumentele functiei main.
* Daca acestea nu sunt valide, va inchide clientul.
*/
void verifySyntax(int argc, char **argv);

/*
* Realizeaza logarea clientului la server.
* Daca logarea a avut succes, currentUser.state va fi setat corespunzator.
*/
void loginUser();

/*
* Prelucreaza o comanda sub forma de sir de caractere si o transforma intr-o structura
* de tip command.
*/
struct command cutCommand(char *fullCommand);

/*
* Citeste comenzile, le interpreteaza si comunica cu server-ul corespunzator.
*/
void processCommands();

/*
* Trimite la server fisierele care se afla in directorul "./work"
*/
int sendWorkFiles(int sd);

/*
* Printeaza pe ecran numele fisierelor din directorul ./work al clientului.
*/
void printWorkFiles();

/*
 * Inchide clientul in momentul in care primeste semnalul SIGINT.
 * Trimite la server o comanda de inchidere.
*/
void closeClient();

/*
 * Creeaza directorul cu care va lucra clientul in directorul client.
 * Va fi creat directorul "./work/" daca acesta nu exista.
*/
void createWorkDirectory();

/*
 * Proceseaza o comanda de tip createUser.
*/
void processCreateUserCommand();

/*
 * Proceseaza o comanda de tip upload.
*/
void processUploadCommand();

/*
 * Proceseaza o comanda de tip delete.
*/
void processDeleteCommand();

/*
 * Proceseaza o comanda de tip currentFiles.
*/
void processCurrentFilesCommand();

/*
 * Proceseaza o comanda de tip uploadedFiles.
*/
void processUploadedFilesCommand();

/*
 * Proceseaza o comanda de tip download.
*/
void processDownloadCommand();

/*
 * Proceseaza o comanda de tip status.
*/
void processStatusCommand();

/*
 * Proceseaza o comanda de tip commit.
*/
void processCommitCommand();

/*
 * Proceseaza o comanda de tip revert.
*/
void processRevertCommand();

/*
 * Proceseaza o commanda de tip showDiff.
*/
void processShowDiffCommand();

//retine datele user-ului curent.
struct user currentUser;
//retin tipul comenzii curente si parametrii acesteia.
struct command currentCommand;
//socket-ul prin care fac transmiterea de date catre server.
int sd;
//cheia privata a s
int key;
// ip-ul serverului
char ip[NMAX];

void main(int argc, char **argv) {
	srand(time(0));
	/* Generez cheia clientului prin care se va face criptarea. */
	key = rand() %128;

	/* Asignez semnalul SIGINT metoda closeClient pentru a fi tratat. */
	signal(SIGINT, closeClient);

	/* Creez directoarele necesare daca acestea nu au fost create. */
	createWorkDirectory();

	/* Verifica daca utilizatorul a introdus ip-ul si portul. */
	verifySyntax(argc, argv);

	/* Copiez intr-o variabila auxiliara, deoarece va fi necesar in momentul in care vom face upload/download. */
	strcpy(ip, argv[1]);
	printf("%s\n", ip);

	/* Realizez conexiunea la server. Este returnat socket-ul prin care se va face transferul de date. */
	sd = connectToServer(argv[1], argv[2]);

	/* Astept logarea utilizatorului. */
	loginUser();

	/* Procesez comenzile introduse de utilizator. */
	processCommands();

	/* Inchid conexiunea cu server-ul. */
	close(sd);
}

void processCommands() {
	char *fullCommand = (char *)malloc(MAXCOMMANDLENGTH * sizeof(char));

	/* Procesez comenzi pana cand comanda primita este una de iesire(quit). */
	while (strcmp(currentCommand.commandName, "quit") != 0) {
		printf("Insert command:\n");

		/* Citesc comanda de la tastatura */
		fgets(fullCommand, MAXCOMMANDLENGTH, stdin);

		/* "Tai" comanda intr-un format care ma ajuta. */
		currentCommand = cutCommand(fullCommand);

		/* Marchez ca momentan nu am executat nici o comanda. */
		int ok = 0;
		int result;

		if (strcmp(currentCommand.commandName, "create_user") == 0 && currentUser.state == ADMIN) {
			processCreateUserCommand();
			ok = 1;
		}
		if (strcmp(currentCommand.commandName, "upload") == 0 && currentUser.state == ADMIN) {
			/* Verific daca fisierul indicat exista. */
			if (doesFileExist(currentCommand.parameters[0]) == 0) {
				printf("This file doesn't exist!\n");
				continue;
			}

			processUploadCommand();
			ok = 1;
		}

		if (strcmp(currentCommand.commandName, "delete") == 0 && currentUser.state == ADMIN) {
			processDeleteCommand();
			ok = 1;
		}

		if (strcmp(currentCommand.commandName, "currentFiles") == 0) {
			processCurrentFilesCommand();
			ok = 1;
		}

		if (strcmp(currentCommand.commandName, "uploadedFiles") == 0 && currentUser.state == ADMIN) {
			processUploadedFilesCommand();
			ok = 1;
		}

		if (strcmp(currentCommand.commandName, "workFiles") == 0) {
			/* Verific ca numarul de parametri sa fie valid. */
			if (currentCommand.nrParameters != 0) {
				printf("Error: Invalid number of parameters!\n");
				continue;
			}

			/* Afisez numele fisierelor din directorul "./work/" */
			printWorkFiles();
			ok = 1;
		}

		if (strcmp(currentCommand.commandName, "download") == 0) {
			processDownloadCommand();
			ok = 1;
		}

		if (strcmp(currentCommand.commandName, "status") == 0) {
			processStatusCommand();
			ok = 1;
		}

		if (strcmp(currentCommand.commandName, "commit") == 0 && currentUser.state == ADMIN) {
			processCommitCommand();
			ok = 1;
		}

		if (strcmp(currentCommand.commandName, "revert") == 0 && currentUser.state == ADMIN) {
			processRevertCommand();			
			ok = 1;
		}

		if (strcmp(currentCommand.commandName, "showDiff") == 0) {
			processShowDiffCommand();
			ok = 1;
		}

		if (strcmp(currentCommand.commandName, "quit") == 0) {
			/* Trimit comanda la server pentru a-l notifica ca trebuie sa-si inchida conexiunea. */
			if (sendCommand(sd, currentCommand, key) == -1) {
				printf("Erorr while sending the command!\n");
			}
			break;
		}

		if (ok == 0 && strcmp(currentCommand.commandName, "quit") != 0) {
			printf("Wrong command or you don't have rights to execute it!\n");
		}

		/* Citesc caracterele random daca au ramas in buffer. */
		readJunkFromFD(sd);

		printf("\n");
	}
	free(fullCommand);
}

int sendWorkFiles(int sd) {
	/* Fisiere auxiliare care ma vor ajuta sa parcurg directorul ./work */
	DIR *directory;
	struct dirent *inFile;
	/* Setez calea directorului. */
	char *directoryPath = (char *)malloc(sizeof(char) * MAXFILENAMELENGTH);
	char *filePath = (char *)malloc(sizeof(char) * MAXFILENAMELENGTH);
	strcpy(directoryPath, "./work/");

	/* Deschid directorul. */
	if (NULL == (directory = opendir(directoryPath))) {
		printf("Failed to open the directory!\n");

		return 0;
	}
	/* Parcurg fisier cu fisier directorul. */
	while ((inFile = readdir(directory))) {
		if (!strcmp (inFile->d_name, "."))
            continue;
        if (!strcmp (inFile->d_name, ".."))    
            continue;
        /* Setez calea fisierului curent. */
        strcpy(filePath, directoryPath);
        strcat(filePath, inFile->d_name);

        /* Trimit la server numele directorului curent. */
        if (writeInFdWithTPP(sd, inFile->d_name, key) == 0) {
        	printf("Failed to write in socket!\n");
        	return 0;
        }

        /* Trimit efectiv fisierul la server. */
        sendFile(sd, filePath, key);
	}

	/* Trimit un sir de caractere "finished" prin care marchez ca nu mai am fisiere de trimis. */
	strcpy(filePath, "finished");
	if (writeInFdWithTPP(sd, filePath, key) == 0) {
		printf("Failed to write in socket!\n");
		return 0;
	}

	/* Eliberez memoria. */
	free(directoryPath);
	free(filePath);

	return 1;
}

void loginUser() {
	/* Marchez ca utilizatorul curent nu este logat. */
	currentUser.state = NOTLOGGEDIN;

	/* Cat timp utilizatorul nu este logat */
	while (currentUser.state == NOTLOGGEDIN) {
		/* Citesc username-ul si parola */
		currentUser = getUser();
		printf("\n");
		/* Trimit comanda la server pentru a fi procesata. */
		currentCommand = setLoginCommand(currentUser);
		sendCommand(sd, currentCommand, key);

		/* Citesc rezultatul trimis de server. */
		read(sd, &currentUser.state, sizeof(int));

		/* Convertesc de la formatul retea la formatul host-ului. */
		currentUser.state = ntohl(currentUser.state);

		/* Interpretez raspunsul de la server. */
		if (currentUser.state == NOTLOGGEDIN) {
			printf("Username/Password incorrect!\n");
		}
		if (currentUser.state == ADMIN) {
			printf("You are logged in as ADMINISTRATOR!\n");
		}
		if (currentUser.state == USUALUSER) {
			printf("You are logged in as an usual user!\n");
		}
		printf("\n");
	}
}

struct command cutCommand(char *fullCommand) {
	char *p;
	struct command builtCommand;

	builtCommand.commandName = (char *)malloc(MAXCOMMANDLENGTH * sizeof(char));
	builtCommand.parameters = (char **)malloc(MAXNRPARAMETERS * sizeof(char *));

	/* Tai sirul pana la primul spatiu sau newline. Acesta reprezinta numele comenzii.*/
	p = strtok(fullCommand, " \n");
	strcpy(builtCommand.commandName, p);

	int nrP = 0;
	p = strtok(NULL, " \n");
	/* Procesez parametru cu parametru si il adaug in structura builtCommand ca parametru. */
	while (p) {
		builtCommand.parameters[nrP] = (char *)malloc(MAXPARAMETERLENGTH * sizeof(char));
		strcpy(builtCommand.parameters[nrP], p);

		nrP++;

		p = strtok(NULL, " \n");
	}


	builtCommand.nrParameters = nrP;

	return builtCommand;
}

struct command setLoginCommand(struct user myUser) {
	struct command loginCommand;

	/* initializez/aloc campurile structuri loginCommand. */
	loginCommand.commandName = (char *)malloc(MAXCOMMANDLENGTH * sizeof(char));
	loginCommand.parameters = (char **)malloc(MAXNRPARAMETERS * sizeof(char *));
	for (int i = 0; i < MAXNRPARAMETERS; i++) {
		loginCommand.parameters[i] = (char *)malloc(sizeof(char) * MAXPARAMETERLENGTH);
	}
	/* Setez corespunzator in structura de tip command datele utilizatorului. */
	strcpy(loginCommand.commandName, "login");
	loginCommand.nrParameters = 2;
	strcpy(loginCommand.parameters[0], currentUser.username);
	strcpy(loginCommand.parameters[1], currentUser.password);

	return loginCommand;
}

void verifySyntax(int argc, char **argv) {
	/* Verific daca avem formatul specificat. */
	if (argc != 3) {
    	printf ("Syntax: %s <server_adress> <port> must be the format!\n", argv[0]);
    	exit(-1);
    }
}

void printWorkFiles() {
	/* Structuri de date ajutatoare la parcurgerea directorului. */
	DIR *workDirectory;
	struct dirent *inFile;

	/* Setez calea directorului care va fi parcurs. */
	char *workPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(workPath, "./work/");

	/* Deschid directorul. */
	if (NULL == (workDirectory = opendir(workPath))) {
		printf("Failed to open the directory!\n");

		return ;
	}
	printf("\n");

	/* Parcurg directorul fisier cu fisier. */
	while ((inFile = readdir(workDirectory))) {
		if (!strcmp (inFile->d_name, "."))
            continue;
        if (!strcmp (inFile->d_name, ".."))    
            continue;
        /* Afisez fisierul gasit. */
        printf("%s\n", inFile->d_name);
    }
    printf("\n");

    free(workPath);
}

void closeClient() {
	/* Trimit la server o comanda de tip quit. */
	strcpy(currentCommand.commandName, "quit");

	if (sendCommand(sd, currentCommand, key) == -1) {
		printf("Erorr while sending the command!\n");
		return;
	}

	/* Intrerup aplicatia. */
	exit(0);
}

void createWorkDirectory() {
	struct stat st = {0};

	/* Verific daca directorul exista */
	if (stat("./work", &st) == -1) {
		/* Nu exista, deci il creez */
    	mkdir("./work", 0700);
	}
}

void processCreateUserCommand() {
	int result;

	/* Verific daca numarul de parametri este valid. */
	if (currentCommand.nrParameters != 3) {
		printf("Error: Invalid number of parameters for createUser command!\n");
		return;
	}

	/* Trimit comanda la server. */
	if (sendCommand(sd, currentCommand, key) == -1) {
		printf("Erorr while sending the command!\n");
		return;
	}

	/* Citesc rezultatul crearii. */
	if (read(sd, &result, sizeof(int)) < sizeof(int)) {
		printf("Error: Couldn't read the create_user result!\n");

		return;
	}
	/* Fac conversia network to host. */
	result = ntohl(result);

	/* Tratez valorile pe care le poate lua result. */
	if (result == SUCCESS) {
		printf("The user was created!\n");
	}
	if(result == ERRWRONGFORMAT) {
		printf("Error: Wrong format!\n");
	}
	if (result == ERRSQL) {
		printf("Error: Something gone wrong in the sql statement! Maybe this user is already in database!\n");
	}
	if (result == ERRNORIGHTS) {
		printf("Error: You don't have rights to execute this command!\n");
	}
}

void processUploadCommand() {
	int result;
	/* Trimit comanda la server. */
	if (sendCommand(sd, currentCommand, key) == -1) {
		printf("Erorr while sending the command!\n");
		return;
	}
	/* Socket-ul prin care se va face transferul de fisier. */
	int dataSD;
	/* Structura ce contine datele celui care va primi fisierul. */
	struct sockaddr_in dataSender;

	/* Creez socket-ul */
	if ((dataSD = socket (AF_INET, SOCK_STREAM, 0)) == -1) {
    	printf("Error: Couldn't create data transfer socket!\n");

    	return;
    }
    /* Familia socket-ului */
    dataSender.sin_family = AF_INET;
    /* Ip-ul specificat ca parametru la executie. */
    dataSender.sin_addr.s_addr = inet_addr(ip);
    /* Citesc portul la care trebuie sa ma conectez. */
    if (read(sd, &dataSender.sin_port, sizeof(dataSender.sin_port)) < sizeof(dataSender.sin_port)) {
    	printf("Error: Couldn't read the port to connect!\n");

    	return;
    }
    /* Ma conectez la noua legatura creata de server. */
    if (connect (dataSD, (struct sockaddr *) &dataSender,sizeof (struct sockaddr)) == -1) {
    	printf("Eroarr couldn't connect to data transfer socket!.\n");
      
    	return;
    }

    /* Trimit fisierul prin legatura noua creata. */
	if (sendFile(dataSD, currentCommand.parameters[0], key)) {
		printf("File was succesfully sent!\n");
	} else {
		printf("Couldn't send file!\n");
	}

	/* Inchid legatura */
	close(dataSD);
	/* Citesc rezultatul upload-ului. */
	read(sd, &result, sizeof(int));

	if(result == 1) {
	 	printf("The file was succesfully received!\n");
	}
	if(result == 0) {
		printf("Error: The file couldn't be received!\n");
	}
}

void processDeleteCommand() {
	int result;

	/* Verific numarul parametrilor */
	if (currentCommand.nrParameters != 1) {
		printf("Error: Invalid number of parameters!\n");
		return;
	}

	/* Trimit comanda la server. */
	if (sendCommand(sd, currentCommand, key) == -1) {
		printf("Erorr while sending the command!\n");
		return;
	}

	/* Citesc rezultatul trimis de server. */
	if (read(sd, &result, sizeof(int)) < sizeof(int)) {
		printf("Erorr: Couldn't read the result!\n");
	}
	/* Convertesc rezulatul de la network la host. */
	result = ntohl(result);

	/* Printez rezultatul comeznii. */
	if(result == 1) {
	 	printf("The file was succesfully deleted!\n");
	}
	if(result == 0) {
		printf("Error: The file couldn't be deleted!\n");
	}
}

void processCurrentFilesCommand() {
	/* Verific numarul parametrilor. */
	if (currentCommand.nrParameters > 0) {
		printf("Invalid number of parameters!\n");
		return;
	}
	/* Trimit comanda pentru a fi procesata de server. */
	if (sendCommand(sd, currentCommand, key) == -1) {
		printf("Erorr while sending the command!\n");
		return;
	}
	
	/* Citesc mesajul primit de la server. */
	char *currentFiles = readFromFdWithTPP(sd, key);

	/* Citesc un caracter junk ce ramane in buffer. */
	char c;
	if (read(sd, &c, sizeof(char)) < 0) {
		printf("Error while reading the junk character!\n");
		return;
	}

	/* Verific daca nu a intervenit o eroare. */
	if (strcmp(currentFiles, "error") == 0) {
		printf("An error has occured while getting the filenames!\n");
		
		return;
	}

	/* Printez textul primit de la server. */
	printf("%s\n", currentFiles);
	/* Eliberez memoria */
	free(currentFiles);
}

void processUploadedFilesCommand() {
	/* Verific numarul de parametri */
	if (currentCommand.nrParameters > 0) {
		printf("Invalid number of parameters!\n");

		return;
	}
	/* Trimit comanda la server pentru a putea fi procesata. */
	if (sendCommand(sd, currentCommand, key) == -1) {
		printf("Erorr while sending the command!\n");

		return;
	}
	
	/* Preiau sirul de caractere generat de server. */
	char *uploadedFiles = readFromFdWithTPP(sd, key);

	/* Citesc caracterul junk ramas pe conexiune. */
	char c;
	if (read(sd, &c, sizeof(char)) < 0) {
		printf("Error while reading the junk character!\n");

		return;
	}

	/* Verific daca nu am avut eroare. */
	if (strcmp(uploadedFiles, "error") == 0) {
		printf("An error has occured while getting the filenames!\n");

		return;
	}

	/* Scriu la ecran sirul de caractere generat de server. */
	printf("%s\n", uploadedFiles);
	/* Eliberez memoria. */
	free(uploadedFiles);
}

void processDownloadCommand() {
	int result;

	/* Verific daca numarul de parametri este corespunzator. */
	if (currentCommand.nrParameters != 1) {
		printf("Invalid number of parameters!\n");
		return;
	}
	/* Trimit comanda la server pentru a fi procesata. */
	if (sendCommand(sd, currentCommand, key) == -1) {
		printf("Erorr while sending the command!\n");
		return;
	}
	/* Socket-ul prin care se va face transferul de date. */
	int dataSD;
	/* Structura ce va tine minte datele punctului de la care se va trimite fisierul. */
	struct sockaddr_in dataSender;

	/* Creez socket-ul. */
	if ((dataSD = socket (AF_INET, SOCK_STREAM, 0)) == -1) {
    	printf("Error: Couldn't create data transfer socket!\n");

    	return;
    }

    /* Setez familia socket-ului. */
    dataSender.sin_family = AF_INET;
    /* Setez ip-ul pe care client-ul l-a inserat la lansarea aplicatiei. */
    dataSender.sin_addr.s_addr = inet_addr(ip);
    /* Citesc portul la care va trebui sa ma conectez */
    if (read(sd, &dataSender.sin_port, sizeof(dataSender.sin_port)) < sizeof(dataSender.sin_port)) {
    	printf("Error: Couldn't read the port to connect!\n");

    	return;
    }

    /* Ma conectez la noua legatura. */
    if (connect (dataSD, (struct sockaddr *) &dataSender,sizeof (struct sockaddr)) == -1) {
    	printf("Eroarr couldn't connect to data transfer socket!.\n");
      
    	return;
    }
    /* Citesc daca pot incepe download-ul. */
	if (read(sd, &result, sizeof(int)) < sizeof(int)) {
		printf("Erorr: Couldn't read the download result!\n");
		return;
	}
	/* Convertesc din network la host rezultatul. */
	result = ntohl(result);

	/* Daca rezultatul este 0 inseamna ca acel fisier nu exista. */
	if (result == 0) {
		printf("The file required doesn't exist!\n");
		return;
	}

	/* Setez unde doresc sa ajunga fisierul downloadat. */
	char *receivedFile = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(receivedFile, "./work/");
	strcat(receivedFile, currentCommand.parameters[0]);

	/* Primesc fisierul. */
	if (receiveFile(dataSD, receivedFile, key)) {
		printf("File was succesfully received!\n");
	} else {
		printf("Couldn't receive file!\n");
	}

	/* Inchid legatura. */
	close(dataSD);
	/* Eliberez memoria. */
	free(receivedFile);
}

void processStatusCommand() {
	/* Verific numarul de parametri */
	if (currentCommand.nrParameters != 1) {
		printf("Error: Invalid number Of parameters!\n");
		return;
	}
	/* Trimit comanda la server pentru a fi procesata. */
	if (sendCommand(sd, currentCommand, key) == -1) {
		printf("Erorr while sending the command!\n");
		return;
	}

	/* Citesc sirul de caractere generat de server ca raspuns. */
	char *result = readFromFdWithTPP(sd, key);

	/* Citesc un caracter de tip junk ce a ramas in buffer. */
	char c;
	if (read(sd, &c, sizeof(char)) < 0) {
		printf("Eroare la citirea caracterului junk!\n");
		return;
	}

	/* Afisez sirul de caractere generat de server pe ecran. */
	printf("%s\n", result);
	/* Eliberez memoria. */
	free(result);
}

void processCommitCommand() {
	int result;

	/* Trimit comanda server-ului pentru a fi procesata. */
	if (sendCommand(sd, currentCommand, key) == -1) {
		printf("Erorr while sending the command!\n");
		return;
	}

	/* Trimit fisierele mele din directorul ./work/ */
	result = sendWorkFiles(sd);
	if (result == 0) {
		printf("The work files coudln't be sent!\n");
		return;
	} else {
		printf("The work files were sent!\n");
	}

	/* Citesc rezultatul commit-ului. */
	if (read(sd, &result, sizeof(char)) < 0) {
		printf("Erorr: Couldn't read commit result!\n");
		return;
	}
	/* Convertesc de la formatul retea la formatul host. */
	result = ntohl(result);

	/* Interpretez rezultatul. */
	if (result == 0) {
		printf("The work files were received!\n");

	} else {
		printf("The work files weren't received!\n");

	}
}

void processRevertCommand() {
	int result;
	/* Trimite comanda la server pentru a fi procesata. */
	if (sendCommand(sd, currentCommand, key) == -1) {
		printf("Erorr while sending the command!\n");
		return;
	}

	/* Primesc rezultatul comenzii de la server. */
	if (read(sd, &result, sizeof(int)) < 0) {
		printf("Erorr: Couldn't read revert result!\n");
		return;
	}
	/* Convertesc rezultatul de la formatul network la host. */
	result = ntohl(result);

	/* Interpretez rezultatul. */
	if (result == 1) {
		printf("The revert succeded!\n");

	} else {
		printf("The revert failed!\n");

	}
}

void processShowDiffCommand() {
	/* Verific daca formatul este corespunzator. */
	if (currentCommand.nrParameters != 2) {
		printf("Error: Wrong format!\n");

		return;
	}

	/* Trimite comanda la server pentru a fi procesata. */
	if (sendCommand(sd, currentCommand, key) == -1) {
		printf("Erorr while sending the command!\n");
		return;
	}

	/* Citesc rezultatul de la server. */
	int result;
	if (read(sd, &result, sizeof(int)) < sizeof(int)) {
		printf("Error: Couldn't read the showDiff result!\n");
	}

	/* Convertesc de la formatul retea la host. */
	result = ntohl(result);
	/* Unul din fisiere nu exista. */
	if (result == 0) {
		printf("Error: The file/version required doesn't exist.");

		return;
	}
	/* Fisierul/directorul exista. */

	/* Citesc diferentele de la server. */
	char *diffFile = readFromFdWithTPP(sd, key);

	/* Afisez pe ecran rezultatul. */
	printf("Diff %s %s:\n%s\n", currentCommand.parameters[0], currentCommand.parameters[1], diffFile);

	/* Eliberez memoria. */
	free(diffFile);
}