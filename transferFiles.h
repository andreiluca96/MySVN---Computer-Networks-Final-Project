#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <signal.h>
#include <pthread.h>
#include <libgen.h>
#include "connection.h"
#define MAXBUFFLENGTH 9999999
#define MAXFILENAMELENGTH 256

/*
* Primeste un fieser la socket-ul indicat si il scrie in fileName.
*/
int receiveFile(int sd, char *fileName, int key);

/*
* Trimite la socket-ul indicat fisierul trimis ca parametru.
*/
int sendFile(int sd, char *fileName, int key);

/*
* Verifica daca fisierul indicat exista in sistemul de fisiere.
* Return:
* 1 daca fisierul exista
* 0 daca fisierul nu exista
*/
int doesFileExist(const char *filename);

/*
* Sterge toate fisierele din directorul specificat.
* Return: 
* 1 daca stergerea a avut succes
* 0 altfel
*/
int removeFilesFromDirectory(char *directoryPath);
