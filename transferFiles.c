#include "transferFiles.h"

int sendFile(int sd, char *fileName, int key) {
	FILE *in = fopen(fileName, "r");
	/* Verific daca fisierul exista. */
	if (doesFileExist(fileName) == 0) {
		return 0;
	} 

	char *sendBuff = (char *)malloc(MAXBUFFLENGTH * sizeof(char));
	bzero(sendBuff, MAXBUFFLENGTH);
    int buffSize;
    /* Citesc din fisier */
    if((buffSize = fread(sendBuff, sizeof(char), MAXBUFFLENGTH, in))>0) {
        printf("%s\n", sendBuff);
        fflush(stdout);

        /* Trimit continutul citit server-ului. */
		if(writeInFdWithTPP(sd, sendBuff, key) == 0) {
            printf("ERROR: Failed to send file %s.\n", fileName);
            return 0;
        }
        bzero(sendBuff, MAXBUFFLENGTH);
    }

    /* Eliberez memoria. */
    free(sendBuff);
	return 1;
}

int receiveFile(int sd, char *fileName, int key) {
	FILE *out = fopen(fileName, "w");
	/* Verific daca am putut deschide fisierul. */
	if (out == 0) {
		printf("Error while opening the file.\n");
		return 0;
	}
	
	int buffSize;
	int writeSize;
	char *recvbuf;
	/* Citesc continutul fisierului. */
	if((recvbuf = readFromFdWithTPP(sd, key)) != 0) {
		if (strlen(recvbuf) != 0) {
			printf("%s\n", recvbuf);
            fflush(stdout);
		}
		/* Scriu continutul in fisier. */
        for (int i = 0; i < strlen(recvbuf); i++) {
        	fprintf(out, "%c", recvbuf[i]);
        }
    }
    /* Inchid fisierul. */
    fclose(out);
    /* Eliberez memoria. */
    free(recvbuf);

	return 1;
}

int doesFileExist(const char *filename) {
    struct stat st;
    /* Verific daca fisierul exista. */
    int result = stat(filename, &st);

    return result == 0;
}

int removeFilesFromDirectory(char *directoryPath) {
	DIR *directory;
	struct dirent *inFile;
	/* Deschid directorul. */
	if (NULL == (directory = opendir(directoryPath))) {
		printf("Failed to open the directory!\n");

		return 0;
	}
	char *filePath = (char *)malloc(MAXFILENAMELENGTH * sizeof(char));

	/* Parcurg fisierele din director. */
	while ((inFile = readdir(directory))) {
		if (!strcmp (inFile->d_name, "."))
            continue;
        if (!strcmp (inFile->d_name, ".."))    
            continue;
        sprintf(filePath, "%s%s", directoryPath, inFile->d_name);
        /* Sterg fisierul. */
        if (remove(filePath) == -1) {
        	return 0;
        } 
    }

    return 1;
}