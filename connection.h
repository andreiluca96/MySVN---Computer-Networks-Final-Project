#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <dirent.h>
#include "login.h"
#define PORT 2908
#define MAXJUNK 99999
#define MAXCLIENTS 10
#define MAXCOMMANDLENGTH 50
#define MAXNRPARAMETERS 10
#define MAXPARAMETERLENGTH 32
#define ERRWRONGFORMAT -1
#define SUCCESS 1
#define ERRSQL 0
#define FAIL 0
#define ERRNORIGHTS 2
#define MAXNRCOMMITS 1024
#define MAXCOMMITNAMELENGTH 256
#define MAXVERSIONLENGTH 10

/*
* Structura folosita pentru a trimite comenzile in retea.
*/
struct command {
	char *commandName;
	int nrParameters;
	char **parameters;
};

/*
* Cripteaza datele folosind Cifrul lui Cezar.
*/
void encryptData(char *data, int length, int key);

/*
* Decripteaza datele folosind Cifrul lui Cezar.
*/
void decryptData(char *data, int length, int key);

/*
* Scrie in descriptorul de fisier intai dimenisunea buffer-ului,
* iar apoi sirul de caractere.
*/
int writeInFd(int fd, char *buff);

/*
* Citeste un sir de caractere din descriptor.
* Sirul trimis este precedat de dimensiunea sirului.
*/
char *readFromFd(int fd);

/*
* Scrie in descriptorul de fisier intai dimenisunea buffer-ului,
* iar apoi sirul de caractere.
* Utilizeaza protocolul three-pass.
*/
int writeInFdWithTPP(int fd, char *buff, int key);

/*
* Citeste un sir de caractere din descriptor.
* Sirul trimis este precedat de dimensiunea sirului.
* Utilizeaza protocolul three-pass.
*/
char *readFromFdWithTPP(int fd, int key);

/*
* Folosita pentru trimiterea unei structuri de tip comanda
* de-a lungul retelei(prin socket-ul trimis ca parametru).
*/
int sendCommand(int sd, struct command commandToBeSent, int key);

/*
* Returneaza o structura de tip comanda citita din descriptor.
*/

struct command receiveCommand(int sd, int key);

/*
* Initializeaza server-ul.
* Si incepe sa asculte clienti.
* Urmeaza doar realizarea accept-ului si tratarea clientilor.
* Returneaza socket-ul server-ului(la care sa va face acceptul).
*/
int initServer();

/*
* Realizeaza conexiunea la server.
* Returneaza socket-ul prin care se va realiza comunicarea cu serverul.
*/
int connectToServer(char *ip, char *port);

/*
 * Citeste tot textul din buffer fara a-l retine.
*/
void readJunkFromFD(int fd);