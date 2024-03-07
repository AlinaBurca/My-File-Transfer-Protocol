
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <limits.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <ctype.h>
#include <utmp.h>
#include <pwd.h>
#include <dirent.h>
#include <stdbool.h>

/* portul folosit */
#define PORT 2024
#define SIZE 1024

/* codul de eroare returnat de anumite apeluri */
extern int errno;

// funtie folosita pentru a scrie mesajul catre client
void msgClient(char msg[], int client)
{
    char s[1000];
    strcpy(s, msg);
    strcat(s, "\n");
    write(client, s, sizeof(s));
}

char *fileNameFormat(char comanda[])
{

    static char fileName[50];
    char *p = strtok(comanda, " ");
    p = strtok(NULL, " ");
    strcpy(fileName, p);
    int n = strlen(fileName);
    fileName[n - 1] = '\0';

    return fileName;
}
void handle_Upload(char comanda[], int client)
{

    char fileName[50], msg[500];
    char *p = strtok(comanda, " ");
    p = strtok(NULL, "/");
    do
    {
        strcpy(fileName, p);
        p = strtok(NULL, "/");
    } while (p != NULL);

    int n = strlen(fileName);
    fileName[n - 1] = '\0';

    if (strcmp(fileName, "") == 0)
    {

        char msg[200];
        sprintf(msg, "Nu ati introdus niciun nume de fisier! Sintaxa comanda: Upload <nume_fisier>");
        msgClient(msg, client);
    }
    else
    {
        // daca valid=1 atunci numele fisierului exista daca valid=0 atunci nu se mai face comanda
        int valid, bytes;

        bytes = read(client, &valid, sizeof(valid));
        if (bytes <= 0)
        {
            perror("Eroare la primirea numarului valid");
            exit(0);
        }

        if (valid)
        {
            struct stat st = {0};
            stat(fileName, &st);
            int file_already_exists;

            // serverul verifica existena fisierului deja in directorul curent daca exista comanda nu se va executa
            // iar daca nu exista trimite mesajul Wait la Client pentru a-l anunta ca asteapta datele
            if ((access(fileName, F_OK) == 0))
            {
                if (S_ISREG(st.st_mode))
                {
                    char msg[8];
                    strcpy(msg, "Existsf"); // exista si este fisier
                    write(client, msg, strlen(msg));
                }
                else if (S_ISDIR(st.st_mode))
                {
                    char msg[8];
                    strcpy(msg, "Existsd"); // exista si este director
                    write(client, msg, strlen(msg));
                }
            }
            else
            {
                char msg[8];
                strcpy(msg, "Wait...");
                write(client, msg, strlen(msg));
                printf("Asteptam datele...\n");

                char buffer[SIZE];
                int size, bytes_received;

                // Primește dimensiunea fișierului
                bytes_received = read(client, &size, sizeof(size));

                if (bytes_received <= 0)
                {
                    perror("Eroare la primirea dimensiunii");
                    exit(0);
                }

             //   printf("Am primit size-ul %d de la client\n", size);

                // Deschide un fișier pentru scriere
                FILE *file = fopen(fileName, "w");
                if (file == NULL)
                {
                    perror("Eroare la deschiderea fișierului pentru scriere");
                    // exit(EXIT_FAILURE);
                }
                else
                {
                    // Primește datele in chunckuri de cate 1024 bytes și le salvează in fișier
                    int total_bytes = 0;
                    while (total_bytes < size)
                    {
                        bytes_received = recv(client, buffer, SIZE, 0);

                        if (bytes_received <= 0)
                        {
                            perror("Eroare la primirea datelor");
                            exit(0);
                        }
                        // printf("Serverul a primit : %s\n", buffer);
                        fwrite(buffer, 1, bytes_received, file);
                        total_bytes += bytes_received;
                    }

                    // Închide fișierul și conexiunea
                    fclose(file);

                    printf("Fișier primit cu succes.\n");
                    // printf("Citit de la client: %s\n", buffer);
                }
            }
        }
    }
}
void handle_Create_file(char comanda[], int client)
{

    char fileName[50];

    strcpy(fileName, fileNameFormat(comanda));

    // verificam daca ati introdus un nume
    if (strcmp(fileName, "") == 0)
    {

        char msg[200];
        sprintf(msg, "Nu ati introdus niciun nume de fisier! Sintaxa comanda: Create_file <nume_fisier>");
        msgClient(msg, client);
    }
    else
    {
        struct stat st = {0};
        stat(fileName, &st);

        if (access(fileName, F_OK) == -1)
        {
            int fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd != -1)
            {
                close(fd);
                char msg[200];
                sprintf(msg, "S-a creat un nou fisier cu numele %s in direcotrul curent de lucru.", fileName);
                msgClient(msg, client);
            }
            else
            {
                perror("Eroare la crearea fisierului");
                msgClient("Eroare la crearea fisierului", client);
            }
        }
        else
        {
            if (S_ISREG(st.st_mode))
            {
                char msg[200];
                sprintf(msg, "Fisierul cu numele %s exista deja in direcotrul curent de lucru.", fileName);
                msgClient(msg, client);
            }
            else if (S_ISDIR(st.st_mode))
            {
                char msg[200];
                sprintf(msg, "In directorul curent de lucru exista un deja un folder cu numele %s si nu putem crea un fisier cu acelasi nume", fileName);
                msgClient(msg, client);
            }
        }
    }
}

void handle_Create_directory(char comanda[], int client)
{
    char directoryName[50];
    strcpy(directoryName, fileNameFormat(comanda));
    int n = strlen(directoryName);

    if (n == 0)
    {

        char msg[200];
        sprintf(msg, "Nu ati introdus niciun nume de director! Sintaxa comanda: Create_directory <nume_director>");
        msgClient(msg, client);
    }
    else
    {
        struct stat st = {0};

        if (stat(directoryName, &st) == -1) // verificam sa nu existe deja folderul cu numele respectiv
        {

            // Creeaza un folder nou
            if (mkdir(directoryName, 0700) == -1)
            {

                perror("Eroare la crearea folderului");
                msgClient("Eroare la crearea folderului.", client);
            }
            else
            {
                char msg[200];
                sprintf(msg, "S-a creat un nou folder cu numele %s in direcotrul curent de lucru.", directoryName);
                msgClient(msg, client);
            }
        }
        else
        {
            if (S_ISDIR(st.st_mode))
            {
                char msg[200];
                sprintf(msg, "Folderul cu numele %s exista deja in direcotrul curent de lucru.", directoryName);
                msgClient(msg, client);
            }

            if (S_ISREG(st.st_mode))
            {
                char msg[200];
                sprintf(msg, "In directorul curent de lucru exist deja un fisier cu numele %s si nu se poate crea un director cu acelasi nume.", directoryName);
                msgClient(msg, client);
            }
        }
    }
}
void handle_Rename_file(char comanda[], int client)
{
    char oldName[50], newName[50];

    int nr = 0;
    char *p = strtok(comanda, " ");
    while (p != NULL)
    {

        nr++;
        if (nr == 2)
            strcpy(oldName, p);
        if (nr == 3)
            strcpy(newName, p);
        p = strtok(NULL, " ");
    }

    newName[strlen(newName) - 1] = '\0';

    if (nr < 3 || nr > 3)
    {

        char msg[100];
        strcpy(msg, "Argumente invalide! Sintaxa comanda: Rename_file <nume_fisier> <nume_fisier_nou>");
        msgClient(msg, client);
    }
    else
    {
        struct stat st1 = {0};
        struct stat st2 = {0};
        stat(oldName, &st1);
        stat(newName, &st2);

        if ((access(oldName, F_OK) != -1) && S_ISREG(st1.st_mode) && (access(newName, F_OK) == -1))
        {
            if (rename(oldName, newName) == -1)
            {
                perror("Eroare la redenumirea fisierului.\n");
                msgClient("Eroare la redenumirea fisierului.", client);
            }
            else
            {

                char msg[200];
                sprintf(msg, "Fișierul %s a fost redenumit în %s", oldName, newName);
                msgClient(msg, client);
            }
        }
        else // tratam diferite cazuri ale numelor puse gresit
        {
            if ((access(oldName, F_OK) == -1)) // fisierul de schimbat nu exista in director
            {
                char msg[200];
                sprintf(msg, "Fisierul cu numele %s nu exista in directorul curent de lucru.", oldName);
                msgClient(msg, client);
            }
            else if (S_ISDIR(st1.st_mode)) // este introdus un nume de director in loc de fisier
            {
                msgClient("Ati introdus un nume de director! Sintaxa comenzii: Rename_file <nume_fisier> <nume_fisier_nou>", client);
            }
            else if (access(newName, F_OK) != -1)
            {                             // al doilea nume de fisier exista in director deja
                if (S_ISREG(st2.st_mode)) // daca este fisier
                {
                    msgClient("Ati introdus un nume nou al unui fisier deja existent!", client);
                }
                else if (S_ISDIR(st2.st_mode)) // daca este director
                {
                    msgClient("Ati introdus un nume nou de director deja existent!", client);
                }
            }
        }
    }
}
void handle_Rename_directory(char comanda[], int client)
{

    printf("Am intrat in rename\n");
    char oldName[50], newName[50];

    int nr = 0;
    char *p = strtok(comanda, " ");
    while (p != NULL)
    {
        nr++;
        if (nr == 2)
            strcpy(oldName, p);
        if (nr == 3)
            strcpy(newName, p);
        p = strtok(NULL, " ");
    }
    newName[strlen(newName) - 1] = '\0';

    if (nr < 3 || nr > 3)
    {

        msgClient("Argumente invalide! Sintaxa comanda: Rename_directory <nume_director> <nume_director_nou>", client);
    }
    else
    {
        struct stat st1 = {0};
        struct stat st2 = {0};

        if ((stat(oldName, &st1) == 0) && S_ISDIR(st1.st_mode) && (stat(newName, &st2) == -1))
        {
            if (rename(oldName, newName) == -1)
            {
                perror("Eroare la redenumirea directorului.\n");
                char msg[200];
                sprintf(msg, "Eroare la redenumirea directorului.");
                msgClient(msg, client);
            }
            else
            {

                char msg[200];
                sprintf(msg, "Directorul %s a fost redenumit în %s", oldName, newName);
                msgClient(msg, client);
            }
        }
        else // tratam diferite cazuri ale numelor puse gresit
        {
            if ((stat(oldName, &st1) == -1)) // folderul de schimbat nu exista in director
            {
                char msg[200];
                sprintf(msg, "Folderul cu numele %s nu exista in directorul curent de lucru.", oldName);
                msgClient(msg, client);
            }
            else if (S_ISREG(st1.st_mode)) // este introdus un nume de fisier in loc de director
            {
                msgClient("Ati introdus un nume de fisier! Sintaxa comenzii: Rename_directory <nume_director> <nume_director_nou>", client);
            }
            else if (stat(newName, &st2) == 0)
            {                             // al doilea nume de fisier exista in director deja
                if (S_ISREG(st2.st_mode)) // daca este fisier
                {

                    msgClient("Ati introdus un nume nou al unui fisier deja existent!", client);
                }
                else if (S_ISDIR(st2.st_mode)) // daca este director
                {

                    msgClient("Ati introdus un nume nou al unui director deja existent!", client);
                }
            }
        }
    }
}

void handle_Delete_file(char comanda[], int client)
{
    char fileName[50];

    strcpy(fileName, fileNameFormat(comanda));

    if (strlen(fileName) == 0)
    {
        msgClient("Trebuie sa dati numele fisierului! Sintaxa comanda: Delete_file <nume_fisier>", client);
    }
    else
    {

        struct stat st = {0};
        stat(fileName, &st);

        if ((access(fileName, F_OK) != -1) && S_ISREG(st.st_mode)) // veririficam ca fisierul sa existe si numele dat la comanda sa fie de fisier
        {

            if (unlink(fileName) == 0)
            {

                char msg[200];
                sprintf(msg, "Fisierul cu numele %s a fost sters.", fileName);
                printf("%s\n", msg);
                msgClient(msg, client);
            }
            else
            {
                char msg[200];
                perror("Eroare la stergerea fisierului");
                sprintf(msg, "A avut loc o eroare la stergerea fisierului %s", fileName);
                msgClient(msg, client);
            }
        }
        else
        {
            if (S_ISDIR(st.st_mode))
            {
                msgClient("Ati introdus un nume de director! Comanda nu se poate efectua!", client);
            }

            else
            {
                char msg[200];
                sprintf(msg, "Fisierul cu numele %s nu exista in directorul curent de lucru!", fileName);
                msgClient(msg, client);
            }
        }
    }
}
void handle_Delete_directory(char comanda[], int client)
{

    char directoryName[50];
    strcpy(directoryName, fileNameFormat(comanda));
    int n = strlen(directoryName);

    if (n == 0)
    {
        msgClient("Trebuie sa dati numele directorului! Sintaxa comanda: Delete_directory <nume_fisier>", client);
    }
    else{

    struct stat st = {0};

    if (stat(directoryName, &st) != -1 && S_ISDIR(st.st_mode)) // verificam sa existe deja folderul cu numele respectiv
    {
        if (rmdir(directoryName) == 0)
        {
            char msg[200];
            sprintf(msg, "Directorul cu numele %s a fost sters.", directoryName);
            msgClient(msg, client);
        }
        else
        {
            char msg[200];
            sprintf(msg, "Directorul cu numele %s contine fisiere si nu poate fi sters.", directoryName);
            msgClient(msg, client);
        }
    }
    else
    {
        if (S_ISREG(st.st_mode))
        {
            msgClient("Ati introdus un nume de fisier! Comanda nu se poate efectua!", client);
        }
        else
        {
            char msg[200];
            sprintf(msg, "Folderul cu numele %s nu exista in directorul curent de lucru", directoryName);
            msgClient(msg, client);
        }
    }
    }
}
void handle_List_files(char comanda[], int client)
{
    DIR *dir;
    struct dirent *entry;
    char current_directory[500], res[500];
    bzero(res, 500);

    if (getcwd(current_directory, sizeof(current_directory)) != NULL)
    {

        dir = opendir(current_directory);
        if (dir == NULL)
        {
            perror("Eroare la deschiderea directorului");
            msgClient("Eroare la deschiderea directorului", client);
        }

        while ((entry = readdir(dir)) != NULL)
        {

            strcat(res, entry->d_name);
            strcat(res, "\n");
        }
        msgClient(res, client);
    }
    else
    {
        perror("Eroare la getcwd()!\n");
        msgClient("A survenir o eroare la listarea fisierelor", client);
    }
    closedir(dir);
}

void handle_Copy_file(char comanda[], int client)
{

    int source, dest;
    char buffer[1024], fileSource[50], fileDest[50];
    int bytes_read, bytes_written;

    int nr = 0;
    char *p = strtok(comanda, " ");
    while (p != NULL)
    {

        nr++;
        if (nr == 2)
            strcpy(fileSource, p);
        if (nr == 3)
            strcpy(fileDest, p);
        p = strtok(NULL, " ");
    }

    fileDest[strlen(fileDest) - 1] = '\0'; // stergem caraterul newline de la sfarsitul comenzii

    if (nr < 3 || nr > 3)
    {

        char msg[100];
        strcpy(msg, "Argumente invalide! Sintaxa comanda: Copy_file <nume_fisier_sursa> <nume_fisier_destinatie>");
        msgClient(msg, client);
    }
    else
    {

        struct stat st1 = {0};
        struct stat st2 = {0};
        stat(fileSource, &st1);
        stat(fileDest, &st2);

        if ((stat(fileSource, &st1) == 0) && S_ISREG(st1.st_mode) && (stat(fileDest, &st2) == 0) && S_ISREG(st2.st_mode))
        {

            source = open(fileSource, O_RDONLY);
            if (source == -1)
            {
                perror("Eroare la deschiderea fișierului sursă");
                msgClient("Eroare la deschiderea fisierului sursa.", client);
                return;
            }

            dest = open(fileDest, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            if (dest == -1)
            {
                perror("Eroare la deschiderea fișierului destinație");
                msgClient("Eroare la deschiderea fisierului destinatie.", client);
                close(source);
            }

            while ((bytes_read = read(source, buffer, sizeof(buffer))) > 0)
            {
                bytes_written = write(dest, buffer, bytes_read);
                if (bytes_written == -1)
                {
                    perror("Eroare la scrierea în fișierul destinație");
                    msgClient("Eroare la scrierea în fișierul destinație.", client);
                    break;
                }
            }

            if (bytes_read == -1)
            {
                perror("Eroare la citirea din fișierul sursă");
                msgClient("Eroare la citirea din fișierul sursă.", client);
            }
            else
            {
                char msg[500];
                sprintf(msg, "Fișierul %s a fost copiat în %s.\n", fileSource, fileDest);
                msgClient(msg, client);
            }

            close(source);
            close(dest);
        }
        else if ((access(fileSource, F_OK) == -1))
        {

            msgClient("Fisierul sursa nu exista in directorul curent de lucru!", client);
        }
        else if ((access(fileDest, F_OK) == -1))
        {

            msgClient("Fisierul destinatie nu exista in directorul curent de lucru!", client);
        }
        else if (S_ISDIR(st1.st_mode))
        {

            msgClient("Ati dat un nume de folder care exista deja pentru fisierul sursa!", client);
        }
        else if (S_ISDIR(st2.st_mode))
        {

            msgClient("Ati dat un nume de folder care exista deja pentru fisierul destinatie!", client);
        }
    }
}

void handle_Working_directory(char comanda[], int client)
{

    char current_directory[100], msg[1000];

    if (getcwd(current_directory, sizeof(current_directory)) != NULL)
    {

        sprintf(msg, "Directorul curent de lucru este: %s\n", current_directory);
        msgClient(msg, client);
    }
    else
    {
        perror("Eroare la getcwd()!\n");
        msgClient("A survenit o eroare la aflarea directorului curent", client);
    }
}

int handle_Change_directory(char comanda[], int client)
{

    char current_directory[100], msg[1000];
    char dirName[50];
    char *p = strtok(comanda, " ");
    p = strtok(NULL, " ");

    strcpy(dirName, p);
    dirName[strlen(dirName) - 1] = '\0';

    if (strcmp(dirName, "") == 0)
    {
        msgClient("Nu ati introdus niciun nume de director! Sintaxa comanda: Change_directory <nume_director>", client);
    }
    else
    {

        struct stat st = {0};

        if ((stat(dirName, &st) == 0) && S_ISDIR(st.st_mode)) // verificam sa  existe folderul cu numele respectiv
        {
            chdir(dirName);
            if (getcwd(current_directory, sizeof(current_directory)) != NULL)
            {

                sprintf(msg, "Directorul curent de lucru s-a schimbat la: %s\n", current_directory);
                msgClient(msg, client);
                return 1;
            }
            else
            {
                perror("Eroare la getcwd()!\n");
                sprintf(msg, "Eroare la aflarea directorului curent de lucru!\n");
                msgClient(msg, client);
            }
        }
        else
        {
            char msg[200];
            sprintf(msg, "Folderul cu numele %s nu exista in directorul curent de lucru.", dirName);
            msgClient(msg, client);
            return 0;
        }
    }
}
int handle_Change_parent_directory(int client, char name[])
{
    char current_directory[100];

    if (chdir("..") == 0)
    {
        if (getcwd(current_directory, sizeof(current_directory)) != NULL)
        {
            int gasit = 0;
            char *p = strtok(current_directory, "/");
            while (p != NULL)
            {
                if (strcmp(name, p) == 0)
                    gasit = 1;
                p = strtok(NULL, "/");
            }
            if (gasit == 0)
            {
                msgClient("Acces interzis!", client); // nu avem voie sa iesim din directorul userului
                chdir(name);
            }
            else
            {
                msgClient("Schimbarea directorului parinte s-a realizat cu succes.", client);
                return 1;
            }
        }
        else
        {
            perror("Eroare la getcwd()!\n");
            msgClient("A survenit o eroare la aflarea directorului curent.", client);
            return 0;
        }
    }
    else
    {
        perror("Eroare la schimbarea directorului parinte.");
        return 0;
    }
}

int main()
{
    struct sockaddr_in server;
    struct sockaddr_in from;
    char msg[1000];
    char msgrasp[100] = " ";
    int sd;
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server]Eroare la socket().\n");
        return errno;
    }

    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[server]Eroare la bind().\n");
        return errno;
    }

    if (listen(sd, 1) == -1)
    {
        perror("[server]Eroare la listen().\n");
        return errno;
    }

    //creem folderul storage in care se vor afla folderele pentru utilizatori
    struct stat st = {0};

    if (stat("storage", &st) == -1)
    {
        
        if (mkdir("storage", 0700) == -1)
        {
            perror("Eroare la crearea folderului");
            return -1;
        }
    }

    while (1)
    {
        int client;
        int length = sizeof(from);

        printf("[server]Asteptam la portul %d...\n", PORT);
        fflush(stdout);

        /* acceptam un client (stare blocanta pina la realizarea conexiunii) */
        client = accept(sd, (struct sockaddr *)&from, &length);

        /* eroare la acceptarea conexiunii de la un client */
        if (client < 0)
        {
            perror("[server]Eroare la accept().\n");
            continue;
        }

        int pid;
        if ((pid = fork()) == -1)
        {
            close(client);
            continue;
        }
        else if (pid > 0)
        {
            // parinte
            close(client); // inchide descriptorul primit prin accept deoarece nu vrea sa trimita sau sa primeasca date
        }
        else if (pid == 0)
        {
            // copil
            close(sd); // inchide socket-ul initial deoarece nu asteapta conexiuni noi in retea

            // AUTENTIFICARE

            int nr_bytes;
            int logged = 0;
            int n = 0;
            int depth = 0;
            int intruderFound = 0;
            char usersData[1024];
            char intrudersData[1024];
            char s[1024];
            char i[1024];
            int nrLogs = 0;

            int fdUsers;
            if (-1 == (fdUsers = open("whitelist.txt", O_RDONLY, 0600)))
            {
                perror("Deschiderea fisierului 'whitelist.txt' a esuat. Motivul:");
                exit(1);
            }

            if (read(fdUsers, usersData, sizeof(usersData)) < 0)
            {
                perror("Eroare la citirea din fisierul 'whitelist.txt' ");
            }

            int fdIntruderR;
            if (-1 == (fdIntruderR = open("blacklist.txt", O_RDONLY, 0666)))
            {
                perror("Deschiderea fisierului 'blacklist.txt' a esuat. Motivul:");
               
            }
            if (read(fdIntruderR, intrudersData, sizeof(intrudersData)) < 0)
            {
                perror("Eroare la citirea din fisierul 'blacklist.txt' ");
            }

            FILE *fdIntruderW = fopen("blacklist.txt", "a");
            if (fdIntruderW == NULL)
            {
                perror("Eroare la deschiderea fișierului 'blacklist.txt' pentru scriere");
               
            }

            // bucla care se reia la fiecare relogare
            while (!n)
            {
                while (!logged && intruderFound != 3)
                {

                    n++;
                    nrLogs++;
                    //printf("Numarul de logari este: %d\n", nrLogs);

                    read(client, msg, sizeof(msg));
                    int m = strlen(msg);

                    strcpy(i, intrudersData);

                    char *p = strtok(i, "\n"); // cauta numele in fisier

                    while (p)
                    {
                        int k = strlen(p);

                        if (strcmp(msg, p) == 0)
                        {
                            intruderFound = 3;
                            break;
                        }
                        p = strtok(NULL, "\n");
                    }

                    if (intruderFound == 3)
                    {

                        write(client, &intruderFound, sizeof(int));
                    }

                    if (intruderFound != 3)
                    {

                        strcpy(s, usersData);

                        char *line = strtok(s, "\n"); // cauta numele in fisier

                        while (line)
                        {
                            int n = strlen(line);

                            if (strcmp(msg, line) == 0)
                            {
                                logged = 1;
                                break;
                            }
                            line = strtok(NULL, "\n");
                        }
                        close(fdUsers);
                        close(fdIntruderR);
                        // cazul in care un utilizator si-a introdus numele/parola de 3 ori gresit
                        if (n == 3 && logged == 0)
                        {

                            // adauga utilizatorul intrus in blacklist

                            char intruder[200];
                            strcpy(intruder, msg);
                            strcat(intruder, "\n");

                            int len = strlen(intruder);
                            fwrite(intruder, 1, len, fdIntruderW);

                            fclose(fdIntruderW);
                        }

                        write(client, &logged, sizeof(int));
                    }
                }

                struct stat st = {0};

                chdir("storage"); //depth=0


                char name[50];
                char *p = strtok(msg, " ");
                strcpy(name, p);

                // Creem un director cu numele utilizatorului unde vor fi incarcate fisierele

                // Daca folderul nu exista deja il creeaza
                if (stat(name, &st) == -1)
                {
                    // Creeaza un folder cu numele user-ului
                    if (mkdir(name, 0700) == -1)
                    {
                        perror("Eroare la crearea folderului");
                        return -1;
                    }
                }

                chdir(name); // navigam in folderul cu numele userului unde vor fi executate comenzile
                depth++;     // crestem adancimea

                // COMENZI

                char comanda[100];
                while (logged)
                {
                    
                    // Citim comanda de la client
                    int nr_bytes = read(client, comanda, sizeof(comanda));

                    if (nr_bytes == 0)
                        break;

                    if (strstr(comanda, "Upload "))
                    {
                        handle_Upload(comanda, client);
                    }
                    else if (strstr(comanda, "Create_file "))
                    {
                        handle_Create_file(comanda, client);
                    }
                    else if (strstr(comanda, "Create_directory "))
                    {
                        handle_Create_directory(comanda, client);
                    }
                    else if (strstr(comanda, "Rename_file "))
                    {
                        handle_Rename_file(comanda, client);
                    }
                    else if (strstr(comanda, "Rename_directory "))
                    {
                        handle_Rename_directory(comanda, client);
                    }
                    else if (strstr(comanda, "Delete_file "))
                    {
                        handle_Delete_file(comanda, client);
                    }
                    else if (strstr(comanda, "Delete_directory "))
                    {
                        handle_Delete_directory(comanda, client);
                    }
                    else if (strcmp(comanda, "List_files\n")==0)
                    {
                        handle_List_files(comanda, client);
                    }
                    else if (strstr(comanda, "Copy_file "))
                    {
                        handle_Copy_file(comanda, client);
                    }
                    else if (strcmp(comanda, "Working_directory\n")==0)
                    {
                        handle_Working_directory(comanda, client);
                    }
                    else if (strstr(comanda, "Change_directory "))
                    {
                        
                        int res = handle_Change_directory(comanda, client);
                        if (res)
                            depth++;
                    }
                    else if (strcmp(comanda, "Change_parent_directory\n")==0)
                    {

                        int res = handle_Change_parent_directory(client, name);
                        if (res)
                            depth--;
                    }

                    else if (strcmp(comanda, "Logout\n")==0)
                    {  // navigam inapoi in folderul storage
                      
                        while (depth > -1)
                        {
                            chdir("..");
                            depth--;
                        }
                        n = 0;
                        logged = 0;
                        msgClient("Delogarea a avut loc cu succes. Pentru a avea acess din nou la comenzi trebuie sa va logati din nou.", client);
                        break;
                    }
                    else if (strcmp(comanda, "Quit\n")==0)
                    {
                        printf("V-ati deconectat de la server...\n");
                        exit(0);
                    }
                    else
                    {
                        // char msg[100];
                        // strcpy(msg, "Comanda inexistenta!");

                        // strcat(msg, "\n");
                        // write(client, msg, sizeof(msg));
                        msgClient("Comanda inexistenta!", client);
                    }
                }
            }

            close(client);
            exit(0);
        }

    } /* while */
} /* main */
