#include "transferFiles.h"
#include <unistd.h>
#include <sys/wait.h>
#define MAXSQLSTATEMENT 200
#define MAXNUMBEROFFILES 100
#define ERRDIRECTORYNOTEXISTS 0
#define MAXERRORLENGTH 7

/*
 * Creeaza directoarele necesare server-ului(daca este necesar). 
 * Aceste directoare sunt:
 * "./serverFiles"
 * "./serverFiles/uploadedFiles"
 * "./serverFiles/commitedFiles"
 * "./serverFiles/currentFiles"
 */
void createServerDirectories();

/*
* Asteapta logarea user-ului si verifica daca datele introduse sunt corecte.
* Trimite mai departe clientului ce drepturi are acesta.
*/

void loginUser();

/*
* Proceseaza comenzile primite de la user.
*/
void processCommands();

/*
* Folosita pentru a adauga utilizatori in baza de date.
* Acest drept il vor avea doar adminii.
*/
int insertInUsers(char *username, char *password, int right);

/*
* Cauta in baza de date user-ul dat ca parametru.
* Daca il va gasi, va apela functia verifyUser.
*/
void searchUserInDB(char *username, char *password);

/*
* Verifica ce drepturi are user-ul gasit de searchUserInDB.
* Modifica corespunzator variabila currentUser cu una din valorile: ADMIN, USUALUSER.
*/
int verifyUser(void *, int, char **, char **);

/*
* Copiaza fisierul sursa la destinatie.
*/
void copyFile(char *source, char *dest);

/*
* Parcurge folderul currentFiles si creeaza un sir de caractere cu numele acestor fisiere separate prin \n.
* Return: pointer spre sirul de caractere creat.
*/
char *getCurrentFilesNames();

/*
* Parcurge folderul uploadedFiles si creeaza un sir de caractere cu numele acestor fisiere separate prin \n.
* Return: pointer spre sirul de caractere creat.
*/
char *getUploadedFilesNames();

/*
* Parcurge fiecare versiune si creaza un sir de caractere cu fiecare commit si mesajul atasat acestuia.
* Return: pointer spre sirul de caractere creat.
*/
char *allCommitsMessage();

/*
* Extrage mesajul atasat commit-ului indicat prin parametrul "version".
* Return: pointer spre un sir de caractere creat de forma "Commit x: Message\n"
*/
char *commitMessageFor(char *version);

/*
* Verifica daca comanda curenta este valida si executa comenzile corespunzatoare asupra
* bazei de date din sistem.
*/
int executeCreateUserCommand();

/*
* Verifica daca comanda curenta este valida si primeste + scrie in folder fisierul indicat.
*/
int executeUploadCommand();

/*
* Verifica daca comanda curenta este valida si trimite fisierul indicat catre client.
*/
int executeDownloadCommand();

/*
* Verifica daca comanda curenta este valida si realizeaza operatia de commit.
*/
int executeCommitCommand();

/*
* Creeaza fisierul diff cu numele indicat pentru fisierele trimise ca parametru.
*/
int createDiffFile(char *fileName1, char *fileName2, char *diffFileName);

/*
* Aplica difrentele din fisierul diffFile pe fisierul fileToBePatched.
*/
int patchFile(char *fileToBePatched, char *diffFile);

/*
* Executa comanda de commit.
* Vor fi 2 etape:
* - Gasirea versiunii curente si crearea folder-ului corespunzator.
* - Crearea fisierelor diff corespunzatoare.
* NU ESTE UTILIZAT!!
* AM FOLOSIT ACEASTA FUNCTIE INTR-O VARIANTA PROTOTIP.
*/
int executeCommitCommand();

/*
* Executa comanda de revert.
* Copiaza fisierele din uploadFiles in currentFiles.
* Modifica fisierele utilizand diff-urile de la versiunea indicata.
* NU ESTE UTILIZAT!!
* AM FOLOSIT ACEASTA FUNCTIE INTR-O VARIANTA PROTOTIP.
*/
int executeRevertCommand();
/*
* Executa comanda de commit.
* Salveaza diferentele de la versiunea precedenta.
* Vor fi 2 etape:
* - Gasirea versiunii curente si crearea folder-ului corespunzator.
* - Crearea fisierelor diff corespunzatoare.
*/
int executeCommitEfficientCommand();

/*
* Executa comanda de revert.
* Copiaza fisierele din uploadFiles in currentFiles.
* Modifica fisierele utilizand diff-urile de la versiunea indicata.
*/
int executeRevertEfficientCommand(char *);

/*
 * Executa o comanda de tip showDiff.
 * Va verifica daca versiunea si fisierul indicat exista si in cazul unui raspuns pozitiv,
 * va trimite clientului continutul fisierului cu diferent.
*/
void executeShowDiffCommand();

// user-ul curent cu care interactioneaza server-ul
struct user currentUser;
// comanda primita de la client
struct command currentCommand;
// socket-ul prin care se vor accepta clientii
int sd;
// socket-ul prin care se va face comunicatia cu clientul
int client;
//cheia privata a serverului
int key;

void main(int argc, char **argv) {
	srand(0);
	/* Generez cheia de criptare a server-ului. */
	key = rand() % 128;

	/* Creez directoarele necesare server-ului. */
	createServerDirectories();

	/* Structura ce stocheaza datele clientului. */
	struct sockaddr_in from;
	/* O initializez cu  0. */
	bzero (&from, sizeof(from));
	/* Calculez dimensiunea structurii. */
	int length = sizeof (from);

	/* Initializez serverul. */
	sd = initServer();
	
	/* Procesez clientii. */
	while (1) {
		/* Accept clientul creand un nou socket: client prin care se va face comunicarea. */
		if ( (client = accept (sd, (struct sockaddr *) &from, &length)) < 0) {
			perror ("[server]Eroare la accept().\n");
			exit(-1);
		}

		/* Creez un proces fiu. */
		int pid = fork();
		/* Tratez o presupusa eroare. */
		if (pid == -1) {
			printf("Error while forking!\n");
			break;
		}
		/* Fiul */
		if (pid == 0) {
			/* Inchid socket-ul prin care se realizeaza acceptarea. */
			close(sd);
			printf("We have a client!\n");
			currentUser.state = NOTLOGGEDIN;

			/* Astept user-ul sa se logheze. */
			loginUser();

			/* Procesez comenzile clientului. */
			processCommands();

			/* Inchid conexiunea. */
			close(client);

			/* Termin procesul fiu. */
			exit(0);
		} else {
			/* Inchid conexiunea de comunicare, deoarece de aceasta se va ocupa fiul. */
			close(client);
		}
	}

	/* Inchidem conexiunea. */
	close(sd);
}

void processCommands() {
	/* Procesez comenzi cat timp nu a intervenit o eroare sau utilizatorul doreste sa inchida conexiunea. */
	while (strcmp(currentCommand.commandName, "quit") != 0 && strcmp(currentCommand.commandName, "Error") != 0) {

		/* Primesc comanda care se doreste a fi executata. */
		currentCommand = receiveCommand(client, key);

		/* Daca avem de-a face cu o eroare iesim din bucla. */
		if (strcmp(currentCommand.commandName, "Error") == 0) {
			printf("The command was not received correctly!\n");
			fflush(stdout);
			break;
		}

		/* Procesez o comanda de tip create_user */
		if (strcmp(currentCommand.commandName, "create_user") == 0) {
			int result = executeCreateUserCommand();

			/* Interpretez rezultatu. */
			if (result == 0) {
				printf("The create_user command couldn't be finished!\n");
			}
			if (result == 1) {
				printf("The user was succesfully created!\n");
			}
		}

		/* Procesez o comanda de tip download. */
		if (strcmp(currentCommand.commandName, "download") == 0) {
			int result = executeDownloadCommand();

			/* Interpretez rezultatul. */
			if (result == 0) {
				printf("Error: Couldn't execute download command!\n");
			} else {
				printf("The download of the file %s succeeded!\n", currentCommand.parameters[0]);
			}
		}

		/* Procesez o comanda de tip upload. */
		if (strcmp(currentCommand.commandName, "upload") == 0) {

			int result = executeUploadCommand();

			/* Interpretez rezultatul. */
			if (result == 0) {
				printf("Error: Couldn't execute upload command!\n");
			} else {
				printf("The upload of the file %s succeeded!\n", currentCommand.parameters[0]);
			}

			/* Convertesc rezultatul la formatul retea. */
			result = htonl(result);

			/* Trimit rezultatul operatiei clientului. */
			if (write(client, &result, sizeof(int)) < sizeof(int)) {
				printf("Erorr: Couldn't write the upload result!\n");
				continue;
			}
		}

		/* Procesez o comanda de tip currentFiles. */
		if (strcmp(currentCommand.commandName, "currentFiles") == 0) {
			/* Creez un sir de caractere care va contine numele fisierelor din directorul ./serverFiles/currentFiles */
			char *currentFiles = getCurrentFilesNames();

			if (currentFiles == 0) {
				currentFiles = (char *)malloc(MAXERRORLENGTH * sizeof(char));
				strcpy(currentFiles, "error");
			}

			/* Trimit acest sir de caractere server-ului. */
			writeInFdWithTPP(client, currentFiles, key);

			/* Eliberez memoria. */
			free(currentFiles);
		}

		/* Procesez o comanda de tip uploadedFiles. */
		if (strcmp(currentCommand.commandName, "uploadedFiles") == 0) {
			/* Creez un sir de caractere care va contine numele fisierelor din directorul ./serverFiles/uploaded */
			char *currentFiles = getUploadedFilesNames();

			if (currentFiles == 0) {
				currentFiles = (char *)malloc(MAXERRORLENGTH * sizeof(char));
				strcpy(currentFiles, "error");
			}

			/* Trimit acest sir de caractere server-ului. */
			writeInFdWithTPP(client, currentFiles, key);

			/* Eliberez memoria. */
			free(currentFiles);
		}

		/* Procesez o comanda de tip delete. */
		if (strcmp(currentCommand.commandName, "delete") == 0) {
			/* Creez un sir de caractere ce contine calea fisierului care va fi sters. */
			char *filePath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
			sprintf(filePath, "./serverFiles/uploadedFiles/%s", currentCommand.parameters[0]);
			int result;

			/* Verific daca fisierul exista. */
			if (doesFileExist(filePath) == 0) {
				printf("The file %s doesn't exist!\n", filePath);

				/* Fisierul nu exista, trimit un rezultat negativ clientului. */
				result = 0;
				result = htonl(result);
				write(client, &result, sizeof(int));
				continue;
			} else {
				if (remove(filePath) == 0) {
					printf("The file %s was succesfully deleted!\n", filePath);

					/* Fisierul a fost sters, trimit rezultat pozitiv clientului. */
					result = 1;
					result = htonl(result);
					write(client, &result, sizeof(int));
					continue;
				} else {
					printf("The file %s couldn't be deleted!\n", filePath);

					/* Fisierul nu a putut fi sters, trimit rezultat negativ clientului. */
					result = 0;
					result = htonl(result);
					write(client, &result, sizeof(int));
					continue;
				}
			}

			free(filePath);
		}

		/* Procesez o comanda de tip commit. */
		if (strcmp(currentCommand.commandName, "commit") == 0) {
			int result = executeCommitEfficientCommand();

			/* Interpretez rezultatul. */
			if (result == 0) {
				printf("Error: Couldn't execute commit command!\n");
			} else {
				printf("The commit succeeded!\n");
			}

			/* Convertesc rezultatul la formatul retea. */
			result = htonl(result);
			/* Trimit rezultatul clientului. */
			if (write(client, &result, sizeof(int)) < sizeof(int)) {
				printf("Error: Couldn't write in the client socket!\n");
			}
		}

		/* Procesez o comanda de tip status. */
		if (strcmp(currentCommand.commandName, "status") == 0) {
			char *result;
			/* Verific daca se doreste status-ul tutoror commit-urilor sau a unuia individual. */
			if (strcmp(currentCommand.parameters[0], "all") == 0) {
				/* Generez un sir de caractere cu status-ul tuturor commit-urilor. */
				result = allCommitsMessage();
			} else {
				/* Generez un sir de caractere cu status-ul commit-ului respectiv. */
				result = commitMessageFor(currentCommand.parameters[0]);
			}

			/* Trimit clientului sirul generat. */
			writeInFdWithTPP(client, result, key);
			/* Eliberez memoria. */
			free(result);
		}

		/* Procesez o comanda de tip revert. */
		if (strcmp(currentCommand.commandName, "revert") == 0) {
			int result = executeRevertEfficientCommand(currentCommand.parameters[0]);

			/* Interpretez rezultatul. */
			if (result == 0) {
				printf("Error: Couldn't execute commit command!\n");
			} else {
				printf("The revert succeeded!\n");
			}
			
			/* Convertesc rezultatul la formatul retea. */
			result = htonl(result);
			/* Trimit rezultatul clientului. */
			if (write(client, &result, sizeof(int)) < sizeof(int)) {
				printf("Error: Couldn't write in the client socket!\n");
			}
		}

		/* Procesez o comanda de tip showDiff. */
		if (strcmp(currentCommand.commandName, "showDiff") == 0) {
			executeShowDiffCommand();
		}
		/* Citesc caractere junk din buffer daca au ramas. */
		readJunkFromFD(client);	
	}
	printf("The client has disconnected!\n");
}

void loginUser() {
	/* Cat timp utilizatorul nu s-a conectat asteptam date de la client. */
	while (currentUser.state == NOTLOGGEDIN) {
		/* Primim comanda de login. */
		currentCommand = receiveCommand(client, key);

		/* Scriem in structura utilizatorului curent datele. */
		strcpy(currentUser.username, currentCommand.parameters[0]);
		strcpy(currentUser.password, currentCommand.parameters[1]);
		currentUser.state = NOTLOGGEDIN;

		/* Cautam user-ul in baza de date. */
		searchUserInDB(currentUser.username, currentUser.password);

		/* Convertesc starea utilizatorului la formatul retea. */
		currentUser.state = ntohl(currentUser.state);
		/* Trimit starea utilizatorului catre client. */
		write(client, &currentUser.state, sizeof(int));

		/* Revin la valoarea initiala a starii user-ului. */
		currentUser.state = htonl(currentUser.state);
		printf("%d\n", currentUser.state);		
	}
}

int executeRevertCommand() {
	// Nu voi comenta aceasta functie, este similara cu cea eficienta.
	char *revertVersionPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(revertVersionPath, "./serverFiles/");
	strcat(revertVersionPath, currentCommand.parameters[0]);
	DIR* dir = opendir(revertVersionPath);
	if (dir == 0)
	{
	    return ERRDIRECTORYNOTEXISTS;
	}

	DIR *uploadedFilesDirectory;
	struct dirent *inFile;
	char *uploadedFilesPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(uploadedFilesPath, "./serverFiles/uploadedFiles/");
	if (NULL == (uploadedFilesDirectory = opendir(uploadedFilesPath))) {
		printf("Failed to open the directory!\n");

		return 0;
	}

	char *currentFilesPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(currentFilesPath, "./serverFiles/currentFiles/");
	removeFilesFromDirectory(currentFilesPath);

	while ((inFile = readdir(uploadedFilesDirectory))) {
		if (!strcmp (inFile->d_name, "."))
            continue;
        if (!strcmp (inFile->d_name, ".."))    
            continue;
        strcat(currentFilesPath, inFile->d_name);
        strcat(uploadedFilesPath, inFile->d_name);

        copyFile(uploadedFilesPath, currentFilesPath);//copyFile(source, dest)

        strcpy(currentFilesPath, "./serverFiles/currentFiles/");
        strcpy(uploadedFilesPath, "./serverFiles/uploadedFiles/");
	}
	closedir(uploadedFilesDirectory);

	DIR *currentFilesDirectory;

	if (NULL == (currentFilesDirectory = opendir(currentFilesPath))) {
		printf("Failed to open the directory!\n");

		return 0;
	}

	char *diffFilePath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));

	while ((inFile = readdir(currentFilesDirectory))) {
		if (!strcmp (inFile->d_name, "."))
            continue;
        if (!strcmp (inFile->d_name, ".."))    
            continue;
        sprintf(diffFilePath,"%s/%s.diff", revertVersionPath, inFile->d_name);
        if ( access(diffFilePath, F_OK) == -1) {
        	continue;
        }
        sprintf(currentFilesPath, "./serverFiles/currentFiles/");
        strcat(currentFilesPath, inFile->d_name);
        patchFile(currentFilesPath, diffFilePath); //(fileToBePatched, diffFile)
	}
	closedir(currentFilesDirectory);

	return 1;
}

int executeCommitCommand() {
	// Nu voi comenta aceasta functie. Este similara cu cea eficienta.
	char c;	
	char *fileName = readFromFdWithTPP(client, key);
	if (read(client, &c, sizeof(char)) < 1) {
		printf("[server]Eroare la citirea caracterului junk!\n");

		return 0;
	}

	fflush(stdout);
	char *filePath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(filePath, "./serverFiles/commitedFiles/");
	removeFilesFromDirectory(filePath);

	while (strcmp(fileName, "finished") != 0) {
		strcpy(filePath, "./serverFiles/commitedFiles/");
		strcat(filePath, fileName);
		if (receiveFile(client, filePath, key) == 0) {
			printf("Eroare la primirea fisierului!\n");

			return 0;
		}
		if (read(client, &c, sizeof(char)) < 1) {
			printf("[server]Eroare la citirea caracterului junk!\n");

			return 0;
		}
		fileName = readFromFdWithTPP(client, key);
		if (read(client, &c, sizeof(char)) < 1) {
			printf("[server]Eroare la citirea caracterului junk!\n");

			return 0;
		}
	}

	int nrVersions = 1;
	char *directoryName = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	sprintf(directoryName, "%d", nrVersions);
	char *directoryPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(directoryPath, "./serverFiles/");
	strcat(directoryPath, directoryName);
	while (mkdir(directoryPath, 0700) == -1) {
		nrVersions++;
		sprintf(directoryName, "%d", nrVersions);
		strcpy(directoryPath, "./serverFiles/");
		strcat(directoryPath, directoryName);
	}

	DIR *commitedFilesDirectory;
	struct dirent *inFile;
	char *commitedFilesDirectoryPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(commitedFilesDirectoryPath, "./serverFiles/commitedFiles/");
	if (NULL == (commitedFilesDirectory = opendir(commitedFilesDirectoryPath))) {
		printf("Failed to open the directory!\n");

		return 0;
	}
	char *filePath1 = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	char *filePath2 = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	char *diffFilePath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	char *currentFilesPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(currentFilesPath, "./serverFiles/currentFiles/");
	removeFilesFromDirectory(currentFilesPath);

	while ((inFile = readdir(commitedFilesDirectory))) {
		if (!strcmp (inFile->d_name, "."))
            continue;
        if (!strcmp (inFile->d_name, ".."))    
            continue;
        printf("Diffing file: %s\n", inFile->d_name);
        strcpy(filePath1, "./serverFiles/uploadedFiles/");
        strcpy(filePath2, "./serverFiles/commitedFiles/");
        strcpy(diffFilePath, directoryPath);
        strcat(diffFilePath, "/");
        strcat(diffFilePath, inFile->d_name);
        strcat(diffFilePath, ".diff");
        strcat(filePath1, inFile->d_name);
        strcat(filePath2, inFile->d_name);
        createDiffFile(filePath1, filePath2, diffFilePath);
    	sprintf(currentFilesPath, "./serverFiles/currentFiles/%s", inFile->d_name);
    	copyFile(filePath2, currentFilesPath);// source, destination
    }

    if (currentCommand.nrParameters > 0) {
    	char *commitMessageFile = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
    	sprintf(commitMessageFile, "%s/message.txt", directoryPath);
    	FILE *out = fopen(commitMessageFile, "w");

    	for (int i = 0; i < currentCommand.nrParameters; i++) {
    		fprintf(out, "%s ", currentCommand.parameters[i]);
    	}
    	fclose(out);
    }
   

	return 1;
}

int executeCommitEfficientCommand() {
	/* Incep cu versiunea 1 pentru a verifica existenta ei. */
	int nrVersions = 1;
	/* Generez numele directoarelor. */
	char *directoryPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	char *directoryName = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	sprintf(directoryName, "%d", nrVersions);
	strcpy(directoryPath, "./serverFiles/");
	strcat(directoryPath, directoryName);

	/* Numar pana la ce versiune am ajuns cu commit-urile. */
	while (1) {
    	struct stat sb;
    	/* Verific daca directorul exista */
    	if (stat(directoryPath, &sb) == 0 && S_ISDIR(sb.st_mode)) {
    		/* Exista, deci trecem la versiunea urmatoare. */
        	nrVersions++;
			sprintf(directoryName, "%d", nrVersions);
			strcpy(directoryPath, "./serverFiles/");
			strcat(directoryPath, directoryName);
    	} else {
    		/* Nu exista, deci ne oprim. */
        	break;
    	}
	}

	char *version = (char *)malloc(MAXVERSIONLENGTH * sizeof(char));
	sprintf(version, "%d", nrVersions);
	/* Aduc cea mai recenta versiune a fisierelor. */
	executeRevertEfficientCommand(version);

	char c;	
	/* Incep sa citesc fisierele trimise de client. */
	char *fileName = readFromFdWithTPP(client, key);
	/* Citesc caracterul junk care ramane in buffer. */
	if (read(client, &c, sizeof(char)) < 1) {
		printf("[server]Eroare la citirea caracterului junk!\n");

		return 0;
	}

	/* Generez calea director-ului commitedFiles. */
	char *filePath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(filePath, "./serverFiles/commitedFiles/");
	/* Golesc directorul commitedFiles */
	removeFilesFromDirectory(filePath);

	/* Cat timp mai avem fisiere de primit. */
	while (strcmp(fileName, "finished") != 0) {
		/* Construiesc calea fisierului. */
		strcpy(filePath, "./serverFiles/commitedFiles/");
		strcat(filePath, fileName);
		/* Primesc fisierul la calea indicata. */
		if (receiveFile(client, filePath, key) == 0) {
			printf("Eroare la primirea fisierului!\n");

			return 0;
		}

		/* Citesc caracterul junk care ramane in buffer. */
		if (read(client, &c, sizeof(char)) < 1) {
			printf("[server]Eroare la citirea caracterului junk!\n");

			return 0;
		}

		/* Citesc urmatorul nume de fisier. */
		fileName = readFromFdWithTPP(client, key);
		if (read(client, &c, sizeof(char)) < 1) {
			printf("[server]Eroare la citirea caracterului junk!\n");

			return 0;
		}
	}

	/* Construiesc directorul pentru noua versiune de commit. */

	nrVersions = 1;
	sprintf(directoryName, "%d", nrVersions);
	/* Construiesc calea directorului de commit. */
	strcpy(directoryPath, "./serverFiles/");
	strcat(directoryPath, directoryName);
	/* Cat timp crearea directorului a esuat */
	while (mkdir(directoryPath, 0700) == -1) {
		/* Trec la versiunea urmatoare. */
		nrVersions++;
		sprintf(directoryName, "%d", nrVersions);
		strcpy(directoryPath, "./serverFiles/");
		strcat(directoryPath, directoryName);
	}

	/* Structuri de date ajutatoare pentru parcurgerea fisierelor. */
	DIR *commitedFilesDirectory;
	struct dirent *inFile;
	/* Construiesc calea directorului ce contine fisierele primite de la client. */
	char *commitedFilesDirectoryPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(commitedFilesDirectoryPath, "./serverFiles/commitedFiles/");
	/* Deschid directorul ce contine fisierele primite de la client. */
	if (NULL == (commitedFilesDirectory = opendir(commitedFilesDirectoryPath))) {
		printf("Failed to open the directory!\n");

		return 0;
	}
	/* Aloc spatiu necesar sirurilor de caractere de care voi avea nevoie. */
	char *filePath1 = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	char *filePath2 = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	char *diffFilePath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	char *currentFilesPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	/* Construiesc calea directorului currentFiles. */
	strcpy(currentFilesPath, "./serverFiles/currentFiles/");

	/* Parcurg fisier cu fisier din directorul commitedFiles. */
	while ((inFile = readdir(commitedFilesDirectory))) {
		if (!strcmp (inFile->d_name, "."))
            continue;
        if (!strcmp (inFile->d_name, ".."))    
            continue;
        /* Construiesc calea fisierelor corespunzatoare. */
        /* filePath1: fisierul din currentFiles */
        /* filePath2: fisierul din commitedFiles */
        /* diffFilePath: fisierul generat de diff */
        printf("Diffing file: %s\n", inFile->d_name);
        strcpy(filePath1, "./serverFiles/currentFiles/");
        strcpy(filePath2, "./serverFiles/commitedFiles/");
        strcpy(diffFilePath, directoryPath);
        strcat(diffFilePath, "/");
        strcat(diffFilePath, inFile->d_name);
        strcat(diffFilePath, ".diff");
        strcat(filePath1, inFile->d_name);
        strcat(filePath2, inFile->d_name);
        /* Verific daca fisierul exista. */
        if (doesFileExist(filePath1) == 1) {
        	printf("File exists!\n");
        } else {
        	printf("no");
        	/* Nu exista, deci fac diff pe fisierul din uploadedFiles */
        	char *uploadedFilesPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
        	sprintf(uploadedFilesPath ,"./serverFiles/uploadedFiles/%s",inFile->d_name);
        	if (doesFileExist(uploadedFilesPath) == 1) {
        		createDiffFile(uploadedFilesPath, filePath2, diffFilePath);
		    } else {
		    	printf("The file %s was not uploaded and will not be tracked. \n", inFile->d_name);
		    }
		    /* Copiez fisierul din commitedFiles in currentFiles. */
        	copyFile(filePath2, currentFilesPath);// source, destination
        	continue;
        }
        /* Creez fisierul de diferente. */
        createDiffFile(filePath1, filePath2, diffFilePath);
        /* Copiez fisierul din commitedFiles in currentFiles. */
    	sprintf(currentFilesPath, "./serverFiles/currentFiles/%s", inFile->d_name);
    	copyFile(filePath2, currentFilesPath);// source, destination
    }

    /* Generez fisierul message.txt daca commit-ul este insotit de un mesaj. */
    if (currentCommand.nrParameters > 0) {
    	char *commitMessageFile = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
    	sprintf(commitMessageFile, "%s/message.txt", directoryPath);
    	FILE *out = fopen(commitMessageFile, "w");

    	for (int i = 0; i < currentCommand.nrParameters; i++) {
    		fprintf(out, "%s ", currentCommand.parameters[i]);
    	}
    	fclose(out);
    }
   
   	/* Eliberez memoria. */
    free(directoryPath);
    free(directoryName);
    free(filePath);
    free(filePath1);
    free(filePath2);
    free(commitedFilesDirectoryPath);
    free(diffFilePath);
    free(currentFilesPath);

	return 1;
}

int executeRevertEfficientCommand(char *version) {
	/* Calculez calea versiunii la care va trebui sa fac revert. */
	char *revertVersionPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(revertVersionPath, "./serverFiles/");
	strcat(revertVersionPath, version);
	DIR* dir = opendir(revertVersionPath);

	/* Verific daca aceasta versiune exista. */
	if (dir == 0)
	{
	    return ERRDIRECTORYNOTEXISTS;
	}

	/* Structuri de date ajutatoare pentru parcurgerea fisierelor. */
	DIR *uploadedFilesDirectory;
	struct dirent *inFile;

	/* Calculez calea directorului uploadedFiles */
	char *uploadedFilesPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(uploadedFilesPath, "./serverFiles/uploadedFiles/");

	/* Deschid directorul. */
	if (NULL == (uploadedFilesDirectory = opendir(uploadedFilesPath))) {
		printf("Failed to open the directory!\n");

		return 0;
	}

	/* Golesc directorul currentFiles. */
	char *currentFilesPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(currentFilesPath, "./serverFiles/currentFiles/");
	removeFilesFromDirectory(currentFilesPath);

	/* Parcurg fisierele din directorul uploadedFiles. */
	while ((inFile = readdir(uploadedFilesDirectory))) {
		if (!strcmp (inFile->d_name, "."))
            continue;
        if (!strcmp (inFile->d_name, ".."))    
            continue;
        strcat(currentFilesPath, inFile->d_name);
        strcat(uploadedFilesPath, inFile->d_name);

        /* Copiez fisierul din uploadedFiles in currentFiles. */
        copyFile(uploadedFilesPath, currentFilesPath);//copyFile(source, dest)

        strcpy(currentFilesPath, "./serverFiles/currentFiles/");
        strcpy(uploadedFilesPath, "./serverFiles/uploadedFiles/");
	}

	/* Calculez cate versiuni de commit sunt pana acum. */
	int nrVersions = 1;
	char *directoryPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	char *directoryName = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	while (1) {
    	struct stat sb;

    	if (stat(directoryPath, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        	nrVersions++;
			sprintf(directoryName, "%d", nrVersions);
			strcpy(directoryPath, "./serverFiles/");
			strcat(directoryPath, directoryName);
    	} else {
        	break;
    	}
	}

	int revertVersion = atoi(version);

	/* Parcurg fiecare versiune pana la cea curenta. */
	for (int i = 1; i <= revertVersion; i++) {
		sprintf(revertVersionPath,"./serverFiles/%d", i);

		/* Structuri de date ajutatoare pentru parcurgerea fisierelor. */
		DIR *currentFilesDirectory;
		sprintf(currentFilesPath, "./serverFiles/currentFiles");
		/* Deschid directorul currentFiles. */
		if (NULL == (currentFilesDirectory = opendir(currentFilesPath))) {
			printf("Failed to open the directory in for!\n");

			return 0;
		}

		char *diffFilePath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));

		/* Parcurg directorul currentFiles. */
		while ((inFile = readdir(currentFilesDirectory))) {
			if (!strcmp (inFile->d_name, "."))
	            continue;
	        if (!strcmp (inFile->d_name, ".."))    
	            continue;
	        sprintf(diffFilePath,"%s/%s.diff", revertVersionPath, inFile->d_name);
	        if ( access(diffFilePath, F_OK) == -1) {
	        	continue;
	        }
	        /* Aplic diferentele din aceasta versiune de commit pe fisierele curente. */
	        sprintf(currentFilesPath, "./serverFiles/currentFiles/");
	        strcat(currentFilesPath, inFile->d_name);
	        patchFile(currentFilesPath, diffFilePath); //(fileToBePatched, diffFile)
		}

		/* Inchid directorul currentFiles. */
		closedir(currentFilesDirectory);
	}

	/* Eliberez memoria. */
	free(uploadedFilesPath);
	free(directoryName);
	free(directoryPath);
	free(revertVersionPath);
	free(currentFilesPath);
	return 1;
}

int executeDownloadCommand() {
	/* Verificam numarul de parametri. */
	if (currentCommand.nrParameters != 1) {
		int error = ERRWRONGFORMAT;
		error = htonl(error);
		if (write(client, &error, sizeof(int)) == 0) {
			printf("[server]Eroare la scriere!\n");
			return 0;
		}
		return 0;
	}
	/* Structuri folosite pentru realizarea conexiunii. */
	struct sockaddr_in dataSender;
	struct sockaddr_in dataReceiver;
	/* Socketul prin care se va face accept, respectiv cel prin care se va face trasferul de date. */
    int dataSD, acceptedDataSD;

    /* Cream socket-ul. */
    if ((dataSD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    	printf("Error: Couldn't create data transfer socket!\n");
    	return 0;
    }
    /* Adaugam optiunea REUSEADDR asupra socket-ului. */
    int option = 1;
    setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&option,sizeof(option));

    /* Initializam structurile dataSender si dataReceiver. */
	bzero (&dataSender, sizeof (dataSender));
	bzero (&dataReceiver, sizeof (dataReceiver));

	/* Setam campurile strcturi sockaddr corespunzator. */
	dataSender.sin_family = AF_INET;
	dataSender.sin_addr.s_addr = htonl(INADDR_ANY);
	dataSender.sin_port = htons(0);

	/* Realizam bind-ul intre socket si structura sockaddr */
	if (bind(dataSD, (struct sockaddr *) &dataSender, sizeof(struct sockaddr)) == -1) {
		perror("Error.");
		printf("Error: Coudln't bind the dataSender!\n");

		return 0;
	}

	/* Incepem ascultarea pentru conexiuni. */
	if (listen(dataSD, 2) == -1) {
		printf("Error: Couldn't listen in the data transfer socket!\n");
		return 0;
	}
	/* Calculam portul socket-ului dataSD. */
	int length = sizeof(dataSender);
	if (getsockname(dataSD, (struct sockaddr *)&dataSender, &length) == -1) {
	    perror("getsockname");
	} else {
	    printf("port number %d\n", ntohs(dataSender.sin_port));
	}

	/* Transmitem portul clientului pentru a initializa conexiunea. */
	if (write(client, &dataSender.sin_port, sizeof(dataSender.sin_port)) < sizeof(dataSender.sin_port)) {
		printf("Error: Couldn't write the port!\n");

		return 0;
	}

	/* Acceptam clientul. */
	length = sizeof(dataReceiver);
	if ((acceptedDataSD = accept(dataSD, (struct sockaddr *)&dataReceiver, &length)) < 0) {
		printf("S-a creat legatura de transfer de date!\n");
	}

	/* Cream calea fisierului care va fi trimis. */
	char *fileToBeSent = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(fileToBeSent, "./serverFiles/currentFiles/");
	strcat(fileToBeSent, currentCommand.parameters[0]);

	int ok;
	if (doesFileExist(fileToBeSent) == 0) {
		ok = 0;
	} else {
		ok = 1;
	}

	/* Notificam clientul ca putem incepe transferul de date. */
	ok = htonl(ok);
	write(client, &ok, sizeof(int));

	/* Trimitem fisierul. */
	int result = sendFile(acceptedDataSD, fileToBeSent, key);

	/* Inchidem conexiunile */
	close(dataSD);
	close(acceptedDataSD);

	/* Eliberam memoria. */
	free(fileToBeSent);
	return result;
}

int executeUploadCommand() {
	/* Verificam numarul de parametri. */
	if (currentCommand.nrParameters != 1) {
		int error = ERRWRONGFORMAT;
		error = htonl(error);
		if (write(client, &error, sizeof(int)) == 0) {
			printf("[server]Eroare la scriere!\n");
			return 0;
		}
		return 0;
	}
	/* Structuri folosite pentru realizarea conexiunii. */
	struct sockaddr_in dataSender;
	struct sockaddr_in dataReceiver;
	/* Socketul prin care se va face accept, respectiv cel prin care se va face trasferul de date. */
    int dataSD, acceptedDataSD;

    /* Cream socket-ul. */
    if ((dataSD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    	printf("Error: Couldn't create data transfer socket!\n");
    	return 0;
    }
    /* Adaugam optiunea REUSEADDR. */
    int option = 1;
    setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&option,sizeof(option));

	/* Initializam structurile dataSender si dataReceiver. */
	bzero (&dataSender, sizeof (dataSender));
	bzero (&dataReceiver, sizeof (dataSender));

	/* Setam campurile strcturi sockaddr corespunzator. */
	dataSender.sin_family = AF_INET;
	dataSender.sin_addr.s_addr = htonl(INADDR_ANY);
	dataSender.sin_port = htons(0);

	/* Realizam bind-ul intre socket si structura sockaddr */
	if (bind(dataSD, (struct sockaddr *) &dataSender, sizeof(struct sockaddr)) == -1) {
		perror("Error.");
		printf("Error: Coudln't bind the dataSender!\n");

		return 0;
	}

	/* Incepem ascultarea pentru conexiuni. */
	if (listen(dataSD, 2) == -1) {
		printf("Error: Couldn't listen in the data transfer socket!\n");
		return 0;
	}
	/* Calculam portul la care clientul va trebui sa se conecteze. */
	int length = sizeof(dataSender);
	if (getsockname(dataSD, (struct sockaddr *)&dataSender, &length) == -1) {
	    perror("getsockname");
	} else {
	    printf("port number %d\n", ntohs(dataSender.sin_port));
	}

	/* Trimitem clientului port-ul. */
	if (write(client, &dataSender.sin_port, sizeof(dataSender.sin_port)) < sizeof(dataSender.sin_port)) {
		printf("Error: Couldn't write the port!\n");

		return 0;
	}

	/* Acceptam conexiunea cu clientul. */
	length = sizeof(dataReceiver);
	if ((acceptedDataSD = accept(dataSD, (struct sockaddr *)&dataReceiver, &length)) < 0) {
		printf("S-a creat legatura de transfer de date!\n");
	}

	/* Trimitem clientului fisierul specificat. */
	char *receivedFile = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(receivedFile, "./serverFiles/uploadedFiles/");
	char *fileName;
	fileName = basename(currentCommand.parameters[0]);
	strcat(receivedFile, fileName);
	int result = receiveFile(acceptedDataSD, receivedFile, key);

	/* Inchidem conexiunea */
	close(dataSD);
    close(acceptedDataSD);

    /* Eliberam memoria. */
    free(receivedFile);
	return 1;
}

int executeCreateUserCommand() {
	/* Verific numarul de parametri */
	if (currentCommand.nrParameters != 3 || 
		!(atoi(currentCommand.parameters[2]) == 0 || atoi(currentCommand.parameters[2]) == 1)) {
		/* Avem un format invalid, deci notificam clientul. */
		int error = ERRWRONGFORMAT;
		error = htonl(error);
		if (write(client, &error, sizeof(int)) == 0) {
			printf("[server]Eroare la scriere!\n");
			return 0;
		}
		return 0;
	}
	int result;
	/* Inseram user-ul in baza de date. */
	result = insertInUsers(currentCommand.parameters[0], currentCommand.parameters[1], 
		atoi(currentCommand.parameters[2]));
	/* Convertesc din formatul host-ului la formatul retea. */
	result = htonl(result);
	/* Trimit clientului rezultatul obtinut. */
	if (write(client, &result, sizeof(int)) == 0) {
		printf("[server]Eroare la scriere!\n");
		return 0;
	}

	return 1;
}

char *allCommitsMessage() {
	char *result = (char *)malloc(MAXCOMMITNAMELENGTH * MAXNRCOMMITS * sizeof(char));
	strcpy(result, "\n");
	int nrVersions = 1;
	/* Calculez calea directorului primei versiuni. */
	char *directoryName = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	sprintf(directoryName, "%d", nrVersions);
	char *directoryPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(directoryPath, "./serverFiles/");
	strcat(directoryPath, directoryName);
	/* Calculez cate versiuni avem */
	while (1) {
    	struct stat sb;

    	if (stat(directoryPath, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        	nrVersions++;
			sprintf(directoryName, "%d", nrVersions);
			strcpy(directoryPath, "./serverFiles/");
			strcat(directoryPath, directoryName);
    	} else {
        	break;
    	}
		
	}

	char *version = (char *)malloc(MAXVERSIONLENGTH * sizeof(char));
	for (int i = 1; i < nrVersions; i++) {
		sprintf(version, "%d", i);
		/* Iau mesajul pentru versiunea i si il adaug la mesajul total. */
		char *currentMessage = commitMessageFor(version);

		strcat(result, currentMessage);
		strcat(result, "\n");
		/* Eliberez memoria. */
		free(currentMessage);
	}

	/* Eliberez memoria. */
	free(directoryName);
	free(directoryPath);

	return result;
}

char *commitMessageFor(char *version) {
	char *result = (char *)malloc(MAXCOMMITNAMELENGTH * sizeof(char));
	char *message = (char *)malloc(MAXCOMMITNAMELENGTH * sizeof(char));

	/* Calculez calea directorului pentru versiunea primita ca parametru. */
	char *messagePath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char)); 
	sprintf(messagePath, "./serverFiles/%s/message.txt", version);

	/* Verific daca s-a lasat un mesaj in momentul in care s-a facut commit. */
	if (doesFileExist(messagePath) == 0) {
		strcpy(message, "No message");
		sprintf(result, "Commit %s: %s\n", version, message);

		return result;
	}
	/* Exista, deci il deschid. */
	FILE *in = fopen(messagePath, "r");

	/* Citesc mesajul din fisier si il copiez in result. */
	if (fgets(message, MAXCOMMITNAMELENGTH, in) == 0) {
		strcpy(message, "error");
		sprintf(result, "Commit %s: %s\n", version, message);
		return result;
	}
	sprintf(result, "Commit %s: %s\n", version, message);

	fclose(in);

	free(messagePath);
	free(message);
	return result;
}

char *getCurrentFilesNames() {
	char *currentFilesNames = (char *)malloc(MAXFILENAMELENGTH * MAXNUMBEROFFILES * sizeof(char));
	strcpy(currentFilesNames, "\n");
	/* Calculez calea directorului currentFiles. */
	char *directoryPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(directoryPath, "./serverFiles/currentFiles/");

	/* Structuri de date ajutatoare pentru parcurgerea fisierelor. */
	DIR *currentFilesDirectory;
	struct dirent *inFile;

	/* Deschid directorul. */
	if (NULL == (currentFilesDirectory = opendir(directoryPath))) {
		printf("Failed to open the directory!\n");

		return 0;
	}

	/* Parcurg directorul. */
	while ((inFile = readdir(currentFilesDirectory))) {
		if (!strcmp (inFile->d_name, "."))
            continue;
        if (!strcmp (inFile->d_name, ".."))    
            continue;
        /* Adaugam numele fisierelor la rezultat. */
        strcat(currentFilesNames, inFile->d_name);
        strcat(currentFilesNames, "\n");
	}

	return currentFilesNames;
}

char *getUploadedFilesNames() {
	char *uploadedFilesNames = (char *)malloc(MAXFILENAMELENGTH * MAXNUMBEROFFILES * sizeof(char));
	strcpy(uploadedFilesNames, "\n");
	/* Calculez calea directorului uploadedFiles. */
	char *directoryPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	strcpy(directoryPath, "./serverFiles/uploadedFiles/");

	/* Structuri de date ajutatoare pentru parcurgerea fisierelor. */
	DIR *uploadedFilesDirectory;
	struct dirent *inFile;

	/* Deschid directorul. */
	if (NULL == (uploadedFilesDirectory = opendir(directoryPath))) {
		printf("Failed to open the directory!\n");

		return 0;
	}

	/* Parcurg directorul. */
	while ((inFile = readdir(uploadedFilesDirectory))) {
		if (!strcmp (inFile->d_name, "."))
            continue;
        if (!strcmp (inFile->d_name, ".."))    
            continue;
        /* Adaugam numele fisierelor la rezultat. */
        strcat(uploadedFilesNames, inFile->d_name);
        strcat(uploadedFilesNames, "\n");
	}

	return uploadedFilesNames;
}

void searchUserInDB(char *username, char *password) {
	sqlite3 *db;
    char *err_msg = 0;
    
    /* Deschid baza de date. */
    int rc = sqlite3_open("users.db", &db);
    
    /* Verific daca nu a aparut o eroare. */
    if (rc != SQLITE_OK) {
        
        fprintf(stderr, "Cannot open database: %s\n", 
        sqlite3_errmsg(db));
        sqlite3_close(db);
        
        return ;
    }
    
    char *sql = (char *)malloc(MAXSQLSTATEMENT * sizeof(char));
    /* Construiesc instructiunea select. */
    sprintf(sql, "SELECT right FROM Users where name = '%s' and password = '%s'", username, password);
    /* Execut interogarea. */
    rc = sqlite3_exec(db, sql, verifyUser, 0, &err_msg);
    
    /* Verific rezultatul. */
    if (rc != SQLITE_OK ) {
        
        fprintf(stderr, "Failed to select data\n");
        fprintf(stderr, "SQL error: %s\n", err_msg);

        sqlite3_free(err_msg);
        sqlite3_close(db);
        
        return ;
    } 
    
    /* Inchid baza de date. */
    sqlite3_close(db);
    /* Eliberez memoria. */
    free(sql);
}

int verifyUser(void *NotUsed, int argc, char **argv, 
                    char **azColName) {
	/* Am gasit user-ul cutat, deci returnam drepturile acestuia. */
    currentUser.state = atoi(argv[0]);
    return 0;
}

int insertInUsers(char *username, char *password, int right) {
	sqlite3 *db;
    char *err_msg = 0;
    
    /* Deschid baza de date. */
    int rc = sqlite3_open("users.db", &db);
    
    /* Verific daca nu au aparut erori. */
    if (rc != SQLITE_OK) {
        
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        
        return 0;
    }
    
    /* Creez instructiunea sql. */
    char *sql = (char *)malloc(200 * sizeof(char));
    sprintf(sql, "INSERT INTO Users VALUES('%s', '%s', %d);", username, password, right);

    /* Execut instructiunea select. */
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    
    /* Verific rezultatul. */
    if (rc != SQLITE_OK ) {
        
        fprintf(stderr, "SQL error: %s\n", err_msg);
        
        sqlite3_free(err_msg);        
        sqlite3_close(db);
        
        return 0;
    } 
    
    /* Inchid baza de date. */
    sqlite3_close(db);
    /* Eliberez memoria. */
    free(sql);

    return 1;
}

int createDiffFile(char *fileName1, char *fileName2, char *diffFileName) {
	/* Cream un nou fiu. */
	int pid = fork();
	if (pid == -1) {
		printf("[Server]Erorr while forking!\n");
		return 0;
	}
	if (pid == 0) {
		/* fiu */
		/* Redirectam stdout. */
		freopen(diffFileName, "w", stdout);
		/* Executam comanda diff. */
		execlp("diff", "diff", fileName1, fileName2, NULL);
		printf("Erorr while executin diff!\n");
		exit(0);
	} else {
		/* Asteptam terminarea fiului. */
		wait(NULL);

		return 1;
	}
}

int patchFile(char *fileToBePatched, char *diffFile) {
	/* Cream un nou fiu. */
	int pid = fork();
	if (pid == -1) {
		printf("[Server]Erorr while forking!\n");
		return 0;
	}
	if (pid == 0) {
		/* fiu */
		/* Redirectam stdin. */
		freopen(diffFile, "r", stdin);
		/* Executam comanda patch. */
		execlp("patch", "patch -R", fileToBePatched, NULL);
		printf("Erorr while executin diff!\n");
		exit(0);
	} else {
		/* Asteptam terminarea fiului. */
		wait(NULL);

		return 1;
	}
}

void copyFile(char *source, char *dest)
{
    pid_t pid;
    int status;
    /* Verific daca sirurile sunt valide */
    if (!source || !dest) {
        printf("Source or destination wrong!\n");
    }

    
    /* Copiez fisierul sursa in destinatie. */
    pid = fork();
    if (pid == 0) {
    	/* Fiul executa comanda cp. */
        execl("/bin/cp", "/bin/cp", source, dest, NULL);
        printf("Couldn't execute the cp command!\n");
    }
    else if (pid < 0) {
        printf("Error! Couldn't create the child process!\n");
    }
    /* Astept fiul */
    wait(&status);
}

void createServerDirectories() {
	struct stat st = {0};

	/* Verific daca directorul exista */
	if (stat("./serverFiles", &st) == -1) {
		/* Nu exista, deci il creez */
    	mkdir("./serverFiles", 0700);
	}

	/* Verific daca directorul exista */
	if (stat("./serverFiles/uploadedFiles", &st) == -1) {
		/* Nu exista, deci il creez */
    	mkdir("./serverFiles/uploadedFiles", 0700);
	}

	/* Verific daca directorul exista */
	if (stat("./serverFiles/currentFiles", &st) == -1) {
		/* Nu exista, deci il creez */
    	mkdir("./serverFiles/currentFiles", 0700);
	}

	/* Verific daca directorul exista */
	if (stat("./serverFiles/commitedFiles", &st) == -1) {
		/* Nu exista, deci il creez */
    	mkdir("./serverFiles/commitedFiles", 0700);
	}
}

void executeShowDiffCommand() {
	/* Calculez calea directorului ce contine versiunea specificata */
	char *versionPath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	char *filePath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));
	sprintf(versionPath, "./serverFiles/%s", currentCommand.parameters[0]);
	printf("%s\n", versionPath);

	struct stat st = {0};
	int ok = 1;
	/* Verific daca directorul exista */
	if (stat(versionPath, &st) == -1) {
		/* Nu exista, deci imarchez ca nu pot incepe procedura de trimitere a fisierului. */
    	ok = 0;
	}

	if (ok == 1) {
		/* Calculez calea fisierului diff. */
		sprintf(filePath, "%s/%s.diff",versionPath, currentCommand.parameters[1]);

		printf("%s\n", filePath);

		/* Verific daca exista. */
		if (doesFileExist(filePath) == 0) {
			ok = 0;
		}
	}

	/* Convertesc la formatul retea. */
	ok = htonl(ok);

	/* Trimit rezultatul clientului. */
	if (write(client, &ok, sizeof(int)) < sizeof(int)) {
		printf("Erorr: Couldn't write the result in socket!\n");

		return;
	}
	/* Convertesc de la formatul hostului la formatul retea. */
	ok = ntohl(ok);
	int buffSize;
	char *fileContent = (char *)malloc(MAXBUFFLENGTH * sizeof(int));
	/* Deschid fisierul. */
	FILE *in = fopen(filePath, "r");
	if (in == 0) {
		printf("Error: Couldn't open file!\n");

		strcpy(fileContent, "Error");

		if(writeInFdWithTPP(client, fileContent, key) == 0) {
            printf("ERROR: Failed to send file %s.\n", filePath);
            return;
        }
	}

	/* Citesc continutul fisierului. */
	if((buffSize = fread(fileContent, sizeof(char), MAXBUFFLENGTH, in)) >= 0) {
		if (buffSize == 0) {
			strcpy(fileContent, "No differences!\n");
		}
		/* Trimit continutul clientului. */
		if(writeInFdWithTPP(client, fileContent, key) == 0) {
            printf("ERROR: Failed to send file %s.\n", filePath);
            return;
        }
	}
}