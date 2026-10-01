# Ressources

## Débogage

### Localiser un segmentation fault avec GDB

Lancez le programme avec ses arguments :

```sh
gdb --args path/to/program/prog localhost 8080
```

Dans GDB, exécutez `run` (ou `r`). Lorsqu'un `SIGSEGV` survient, GDB indique la ligne concernée. Utilisez `backtrace` (ou `bt`) pour afficher la pile d'appel et `p <variable>` pour afficher une variable. Tapez `quit` pour quitter GDB.

```text
(gdb) r
Starting program: path/to/program/prog arg1 arg2

Program received signal SIGSEGV, Segmentation fault.
... at path/to/program/prog.c:24
(gdb) bt
#0  ... at path/to/program/prog.c:14
#1  ... at path/to/program/prog.c:28
(gdb) p ma_variable
$1 = (int *) 0x0
```

### Tester un serveur sans votre client

`telnet` est un client TCP. Pour vous connecter à un serveur qui écoute sur le port 8080 :

```sh
telnet localhost 8080
```

Pour quitter Telnet, tapez `Ctrl` + `Alt` + `]`, puis `quit`.

### Tester un client sans votre serveur

`nc` peut émuler un serveur TCP à l'écoute sur un port :

```sh
nc -l 8080
```

### Vérifier les sockets ouvertes

Cette commande aide à détecter les sockets TCP laissées ouvertes par le serveur :

```sh
lsof -c path/to/program/serveur 2>/dev/null | grep TCP | wc -l
```

### Détecter les fuites mémoire

```sh
valgrind path/to/program/serveur
```

## Rappel de C

### Structures

```c
struct module {
    int moduleId;
    double moduleGrade;
    char padding[20];
};

struct module resa1;
resa1.moduleId = 5;
resa1.moduleGrade = 12.5;
```

Avec un pointeur, accédez aux champs avec `->` :

```c
struct module resa1;
struct module *presa1 = &resa1;
presa1->moduleId = 5;
```

Un `typedef` peut créer un alias :

```c
typedef struct module s_module;
s_module resa1;
```

### Pointeurs

Les pointeurs associés aux types de base sont par exemple `int *`, `double *`, `float *` et `char *`. Prenez l'adresse d'une variable avec `&` et déréférencez un pointeur avec `*` :

```c
int a = 5;
int *pa = &a;
if (*pa == 5) {
    /* ... */
}
```

Les pointeurs peuvent être passés à une fonction :

```c
int func(int *a, int *b) {
    return *a + *b;
}

int a = 5;
int b = 7;
int res = func(&a, &b);
```

### Conversions de type

Une conversion explicite utilise la syntaxe `(type)` :

```c
int sum = 17;
int count = 5;
double mean = (double)sum / count;
```

Soyez particulièrement prudent avec les conversions de pointeurs : elles ne transforment pas les données en mémoire. Elles changent seulement la manière dont le programme les interprète.
