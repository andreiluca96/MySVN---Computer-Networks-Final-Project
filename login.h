#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#define NMAX 35
#define ADMIN 0
#define USUALUSER 1
#define NOTLOGGEDIN -1

/*
* Structura ce contine datele unui user.
*/
struct user {
	char username[35];
	char password[35];
	int state;
};

/*
* Citeste de la tastatura un username si o parola.
* Returneaza o structura care contine aceste date.
*/
struct user getUser();
/*
* Intervine in momentul in care in getUser este nevoie sa fie citita parola.
* Blocheaza temporar afisarea in consola pana utilizatorul apasa ENTER.
*/
void getPassword(char *lineptr, FILE *stream);