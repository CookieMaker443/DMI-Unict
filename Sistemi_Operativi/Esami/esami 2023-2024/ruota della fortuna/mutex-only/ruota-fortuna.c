#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <ctype.h>

#define MAX_FRASE_SIZE 100
#define ALFABETO_SIZE 26

typedef struct
{
    char frase_da_scoprire[MAX_FRASE_SIZE]; //contiene la frase con le lettere oscurate
    int contatore_lettere[ALFABETO_SIZE]; //tiene traccia delle lettere chiamate
    int n_giocatori; //numero di giocatori
    int *punteggi;//array che contiene il punteggio dei giocatori
    char lettera; //memorizza l'ultima lettera scelta dal giocatore
    int fine;
    sem_t sync_sem;

} tabellone_t;

typedef struct
{
    int codice; //identificatore del giocatore
    tabellone_t *tabellone;
    pthread_mutex_t read_mutex;  // ha la funzione di bloccare il thread fino a quando il giocatore non ottiene il suo turno
    pthread_mutex_t write_mutex; // ha la funzione bloccare il thread principale fino a quando il giocatore non ha finito il turno

} giocatore_t;

void shuffle(int *array, int n){ //randomizza l'ordine delle frasi
    if (n > 1){
        for (int i = 0; i < n; i++){
            int j = rand() % n;
            int t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}

char seleziona_lettera(int *contatore_lettere){
    int j = 0;
    int lettere_non_chiamate[ALFABETO_SIZE];
    for (int i = 0; i < ALFABETO_SIZE; i++){
        if (contatore_lettere[i] == 0){
            lettere_non_chiamate[j++] = i;
        }
    }

    if (j == 0){
        return 0;
    }else{
        int indice_lettera = lettere_non_chiamate[rand() % j];
        return 'A' + indice_lettera;
    }
}

void reset(int *array, int n){
    for (int i = 0; i < n; i++)
        array[i] = 0;
}

void nascondi_lettere(char *dest_str, char *orginal_str){
    int size = strlen(orginal_str);
    for (int i = 0; i < size; i++){
        if (isalpha(orginal_str[i])){ //prende le lettere dell'alfabeto eccetto gli spazzi e li sostiuisce con #
            dest_str[i] = '#';
        }else{
            dest_str[i] = orginal_str[i];
        }
    }
    dest_str[size] = '\0';
}

int mostra_lettere(char *dest_str, char *original_str, char c){
    int n = 0;
    int size = strlen(original_str);
    for (int i = 0; i < size; i++){
        if (original_str[i] == c){ //se il carattere è uguale a uno dei caratteri presenti nella stringa
            dest_str[i] = c; //mostra il carattere 
            n++; //aumenta il contatore di caratteri trovati
        }
    }
    return n;
}

int argmax(int *array, int n){ //vincitore
    int index = -1;
    int max = 0;
    for (int i = 0; i < n; i++){
        if (array[i] > max){
            max = array[i];
            index = i;
        }
    }
    return index;
}

int tabellone_completato(char *str){
    return strchr(str, '#') == NULL; //strchr ricerca un carattere nella stringa, se non viene trovato il carattere specificato restituisce NULL
}

void *giocatore_thread(void *args){
    giocatore_t *giocatore = (giocatore_t *)args;
    tabellone_t *tabellone = (tabellone_t *)giocatore->tabellone;

    printf("[G%d] avviato e pronto\n", giocatore->codice + 1);
    sem_post(&tabellone->sync_sem); // notifica al main che questo giocatore è pronto per iniziare

    while (tabellone->fine == 0){
        pthread_mutex_lock(&giocatore->read_mutex); // blocca il thread fino a quando non viene il turno di questo giocatore
        if (tabellone->fine != 0) //se è finito il gioco interrompi
            break;

        char lettera = seleziona_lettera(tabellone->contatore_lettere); //faccio selezionare una lettera
        tabellone->lettera = lettera; //memorizzo la lettera scelta all'interno del buffer che contiene le lettere selezionate
        printf("[G%d] scelgo la lettera '%c'\n", giocatore->codice + 1, lettera);
        pthread_mutex_unlock(&giocatore->write_mutex); // sbloca il thread principale
    }
    return NULL;
}

void main(int argc, char **argv){
    // leggo gli argomenti
    if (argc < 4){
        perror("Argomenti non validi");
        exit(EXIT_FAILURE);
    }

    int n_giocatori = atoi(argv[1]);
    int n_partite = atoi(argv[2]);
    char *frasi_filename = argv[3];

    // carico le frasi
    char buffer[MAX_FRASE_SIZE];
    FILE *frasi_fp = fopen(frasi_filename, "r");
    if (frasi_fp == NULL){
        perror("Errore apertura file delle frasi");
        exit(EXIT_FAILURE);
    }
    // recupero il numero dello frasi per costruirmi il vettore
    int n_frasi = 0;
    while (!feof(frasi_fp)){
        if (fgetc(frasi_fp) == '\n')
            n_frasi++;
    }
    fseek(frasi_fp, 0, SEEK_SET);

    // verifico se il numero di frasi è sufficiente
    if (n_frasi < n_partite){
        perror("Numero delle frasi inferiore al numero delle partite");
        exit(EXIT_FAILURE);
    }

    int j = 0; //conta il numero di frasi lette
    char **elenco_frasi = (char **)malloc(sizeof(char *) * n_frasi); //array che conterrà le frasi lette
    while (fgets(buffer, MAX_FRASE_SIZE, frasi_fp) != NULL){
        int l = strcspn(buffer, "\n"); // restituisce la vera dimensione senza conteggiare \n
        // metto tutto le lettere in maiuscolo cosi non ho problemi dopo !
        for (int i = 0; i < l; i++){
            buffer[i] = toupper(buffer[i]);
        }
        elenco_frasi[j] = (char *)malloc(sizeof(char) * l); //alloco la memoria per la frase per poi
        strncpy(elenco_frasi[j], buffer, l); // copiare esattamente un numero di caratteri pari a l
        j++;
    }
    fclose(frasi_fp);
    printf("[M] lette %d possibili frasi da indovinare per %d partite\n", n_frasi, n_partite);

    // creo la struttura dati condivisa
    tabellone_t *tabellone = (tabellone_t *)malloc(sizeof(tabellone_t));
    tabellone->n_giocatori = n_giocatori;
    tabellone->punteggi = calloc(n_giocatori, sizeof(int)); // ogni elemento del vettore contiene il punteggio associato al corrispettivo giocatore
    tabellone->fine = 0;

    // questo semaforo ha la funzione di comunicare a Mike che il gioco puo iniziare
    // ogni volta che è un giocatore è pronto incrementa il valore del semaforo, quando è zero il gioco puo iniziare
    if (sem_init(&tabellone->sync_sem, 0, 0) < 0){
        perror("Errore nella creazione di sync_sem");
        exit(EXIT_FAILURE);
    }

    // creo i giocatori
    giocatore_t **giocatori = (giocatore_t **)malloc(sizeof(giocatore_t *) * n_giocatori);
    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * n_giocatori); //creo thread in base al numero di giocatori 
    for (int i = 0; i < n_giocatori; i++){
        giocatore_t *giocatore = (giocatore_t *)malloc(sizeof(giocatore_t));
        giocatore->codice = i; //id del giocatore 
        giocatore->tabellone = tabellone;

        if (pthread_mutex_init(&giocatore->read_mutex, NULL) < 0){
            perror("Errore nella creazione del read_mutex");
            exit(EXIT_FAILURE);
        }
        pthread_mutex_lock(&giocatore->read_mutex);

        if (pthread_mutex_init(&giocatore->write_mutex, NULL) < 0)
        {
            perror("Errore nella creazione del write_mutex");
            exit(EXIT_FAILURE);
        }
        pthread_mutex_lock(&giocatore->write_mutex);

        if (pthread_create(&threads[i], NULL, giocatore_thread, giocatore) < 0)
        {
            perror("Errore nella creazione del thread");
            exit(EXIT_FAILURE);
        }
        giocatori[i] = giocatore;
    }

    // attende che tutti i giocatori sono pronti
    for (int i = 0; i < n_giocatori; i++){
        sem_wait(&tabellone->sync_sem);
    }
    printf("[M] Tutti i giocatori sono pronti, possiamo iniziare!\n");

    // genero la sequenza delle frasi in maniera casuale
    int *frasi_indici = (int *)malloc(n_frasi * sizeof(int));
    for (int i = 0; i < n_frasi; i++)
        frasi_indici[i] = i; 
    shuffle(frasi_indici, n_frasi); //randomizzo 

    int frase_indice_corrente = 0; //contatore delle frasi attuali
    for (int i = 0; i < n_partite; i++){
        // inizio partita

        // seleziono la frase
        char *frase = elenco_frasi[frasi_indici[frase_indice_corrente++]];
        printf("[M] scelta la frase %s per la partina n. %d\n", frase, i + 1);

        // aggiorno il tabellone
        nascondi_lettere(tabellone->frase_da_scoprire, frase); //nascondo le lettere della frase da scopire
        reset(tabellone->contatore_lettere, ALFABETO_SIZE); //resetto il contatore di lettere per ogni frase nuova

        giocatore_t *giocatore;
        while (!tabellone_completato(tabellone->frase_da_scoprire))
        {
            int k = 0;
            while (k < n_giocatori && !tabellone_completato(tabellone->frase_da_scoprire))
            {
                int n_turni = 0;
                while (!tabellone_completato(tabellone->frase_da_scoprire)){
                    printf("[M] tabellone: %s\n", tabellone->frase_da_scoprire);
                    if (n_turni == 0)
                        printf("[M] adesso è il turno di G%d\n", k + 1);
                    else
                        printf("[M] adesso è di nuovo il turno di G%d\n", k + 1);

                    pthread_mutex_unlock(&giocatori[k]->read_mutex); // da il turno al giocatore
                    pthread_mutex_lock(&giocatori[k]->write_mutex);  // attende che il giocatore sceglie una lettera

                    char lettera = tabellone->lettera;
                    tabellone->contatore_lettere[lettera - 'A']++; // incrementa il contattore delle lettere
                    int lettere_trovate = mostra_lettere(tabellone->frase_da_scoprire, frase, lettera);
                    int punteggio_base = (rand() % 4 + 1) * 100;
                    int punteggio = punteggio_base * lettere_trovate;
                    tabellone->punteggi[k] += punteggio;

                    if (lettere_trovate == 0){
                        printf("[M] nessuna occorrenze per %c\n", lettera);
                        break;
                    }
                    else
                    {
                        printf("[M] ci sono %d occorrenze per %c; assegnati %dx%d=%d\n", lettere_trovate, lettera, punteggio_base, lettere_trovate, punteggio);
                        n_turni++;
                    }
                }
                k++;
            }
        }

        printf("[M] frase completata; punteggi attuali: ");
        for (int i = 0; i < n_giocatori; i++){
            printf("G%d:%d ", i + 1, tabellone->punteggi[i]);
        }
        printf("\n");
    }

    // stampo il vincitore
    printf("[M] questa era l'ultima partita: il vincitore e' G%d\n", argmax(tabellone->punteggi, n_giocatori) + 1);

    // deallocazione pulita
    free(frasi_indici);
    free(elenco_frasi);

    tabellone->fine = 1;
    for (int i = 0; i < n_giocatori; i++)
    {
        pthread_mutex_unlock(&giocatori[i]->read_mutex);
        pthread_join(threads[i], NULL);

        pthread_mutex_destroy(&giocatori[i]->read_mutex);
        pthread_mutex_destroy(&giocatori[i]->write_mutex);
        free(giocatori[i]);
    }
    free(threads);

    sem_destroy(&tabellone->sync_sem);
    free(tabellone->punteggi);
    free(tabellone);
}