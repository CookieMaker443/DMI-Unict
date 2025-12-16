#include "./lib-misc.h"
#include <bits/pthreadtypes.h> //definisce delle strutture legate a thread e sincronizzazione
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define BUFFER_SIZE 2048
typedef enum { INS, ADD, SUB, MUL, RES, DONE } operatore;
//INS = operazione iniziale, add = somma, sub = sottrazione, mul = moltiplicazione , res = risultato, done = fine

typedef struct {
    long long operando_1; //operando per le operazioni
    long long operando_2; //operando per le operazioni 
    long long risultato; //risultato del calcolo
    operatore op; //tipo di operazione da eseguire
    unsigned id_richiedente; //identificatore del thread calc che ha richiesto l'operazione
    unsigned done;  
    unsigned success; //indica il numero di operazioni eseguite correttamente

    // strumenti per la sincronizzazione e la mutua esclusione
    pthread_mutex_t lock; //mutex per garantire l'accesso esclusivo ai dati condivisi
    pthread_cond_t cond_calc; //variabile di condizione per sincronizzare i thread CALC
    pthread_cond_t cond_add; //variabile di condizione per le somme
    pthread_cond_t cond_sub; // variabile di condizione per le sottrazioni
    pthread_cond_t cond_mul; //varaibile di condizoine delle moltiplicazioni
} shared; //Dati condivisi tra tutti i thread 

typedef struct {
    // dati privati
    pthread_t tid; //identificatore del thread
    unsigned thread_n; //numero identificativo del thread
    unsigned ncalc; //numero di calcoli assegnati 
    char *input_file; //nome del file input assegnato a CALC

    // dati condivisi
    shared *sh;
} thread_data; //dati specifici per ogni thread

void init_shared(shared *sh) {
    int err;

    sh->done = sh->success = 0; //inizializzazione dei calcoli completati e conteggati 
    sh->op = INS; //imposta l'operazione allo stato iniziale

    // inizializzo il mutex a 0
    if ((err = pthread_mutex_init(&sh->lock, 0)) != 0) 
        exit_with_err("pthread_mutex_init", err);

    // inizializzo le variabili condizione
    if ((err = pthread_cond_init(&sh->cond_calc, 0)) != 0)
        exit_with_err("pthread_cond_init", err);

    if ((err = pthread_cond_init(&sh->cond_add, 0)) != 0)
        exit_with_err("pthread_cond_init", err);

    if ((err = pthread_cond_init(&sh->cond_sub, 0)) != 0)
        exit_with_err("pthread_cond_init", err);

    if ((err = pthread_cond_init(&sh->cond_mul, 0)) != 0)
        exit_with_err("pthread_cond_init", err);
}

void shared_destroy(shared *sh) {
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->cond_calc);
    pthread_cond_destroy(&sh->cond_add);
    pthread_cond_destroy(&sh->cond_sub);
    pthread_cond_destroy(&sh->cond_mul);
    free(sh);
}

void calc_thread(void *arg) {
    int err;
    thread_data *td = (thread_data *)arg;
    FILE *f;
    char buffer[BUFFER_SIZE]; 
    long long totale; //valore calcolato correttamente 
    long long risultato; //valore attesso dal file

    printf("[CALC-%u] file da verificare: '%s'\n", td->thread_n + 1,
           td->input_file);

    // apro il file in sola lettura
    if ((f = fopen(td->input_file, "r")) == NULL)
        exit_with_sys_err("fopen");

    // leggo il valore iniziale del calcolo
    if (fgets(buffer, BUFFER_SIZE, f)) {
        totale = atoll(buffer); //converto la prima stringa del file in un numero (long long)
        printf("[CALC-%u] valore iniziale della computazione: %lld\n",
               td->thread_n + 1, totale);
    } else {
        fprintf(stderr, "[Calc-%u] errore nella lettura del primo valore",
                td->thread_n + 1);
        exit(EXIT_FAILURE);
    }

    // leggo il file riga per riga
    while (fgets(buffer, BUFFER_SIZE, f)) {
        if (buffer[strlen(buffer) - 1] == '\n')
            buffer[strlen(buffer) - 1] = '\0';

        // bisognerebbe implementare un controllo migliore, magari contando il
        // numero di linee presenti nel file per individuare il risultato atteso
        if (buffer[1] != ' ') { //se la riga corrente non è un'operazione interpreta la riga come risultato attesso
            risultato = atoll(buffer);
            break;
        }

        printf("[CALC-%u] prossima operazione '%s' \n", td->thread_n + 1,
               buffer);

        // ottengo il lock sulla struttura dati condivisa
        if ((err = pthread_mutex_lock(&td->sh->lock)) != 0)
            exit_with_err("pthread_mutex_lock", err);

        // verifico le condizioni di operabilità e mi metto in attesa nel caso
        // in cui queste non sono soddisfatte
        while (td->sh->op != INS)
            if ((err = pthread_cond_wait(&td->sh->cond_calc, &td->sh->lock)) !=
                0)
                exit_with_err("pthread_cond_wait", err);

        td->sh->id_richiedente = td->thread_n; //imposto l'id del thread richiedente
        td->sh->operando_1 = totale; //assegno il primo valore 
        td->sh->operando_2 = atoll(buffer + 2); //converto il secondo operando come un numero

        if (buffer[0] == '+') { //se l'operazione è +
            td->sh->op = ADD; //imposto l'operazione della struttura dati condivisa dei thread in add

            // sveglio il thread ADD
            if ((err = pthread_cond_signal(&td->sh->cond_add)) != 0)
                exit_with_err("pthread_cond_signal", err);
        } else if (buffer[0] == '-') { //se l'operazione è -
            td->sh->op = SUB; //imposto l'operazione della struttura dati condivisa dei thread in SUB

            // sveglio il thread SUB
            if ((err = pthread_cond_signal(&td->sh->cond_sub)) != 0)
                exit_with_err("pthread_cond_signal", err); 
        } else if (buffer[0] == 'x') { //Se l'operazione è x
            td->sh->op = MUL; //imposto l'operazione della struttura dati condivisa dei thread in MUL

            // sveglio il thread MUL
            if ((err = pthread_cond_signal(&td->sh->cond_mul)) != 0)
                exit_with_err("pthread_cond_signal", err);
        } else {
            fprintf(stderr, "[CALC-%u] errore nel parsing del file in input\n",
                    td->thread_n + 1);
            exit(EXIT_FAILURE);
        }

        // rilascio il mutex
        if ((err = pthread_mutex_unlock(&td->sh->lock)) != 0)
            exit_with_err("pthread_mutex_unlock", err);

        // ottengo il lock sulla struttura dati condivisa
        if ((err = pthread_mutex_lock(&td->sh->lock)) != 0)
            exit_with_err("pthread_mutex_lock", err);

        // verifico le condizioni di operabilità ovvero se l'operazione non è RES o id del richiedente è diverso dal numero identificato del thread
        //  mi metto in attesa nel caso in cui queste non sono soddisfatte
        while (td->sh->op != RES || td->sh->id_richiedente != td->thread_n)
            if ((err = pthread_cond_wait(&td->sh->cond_calc, &td->sh->lock)) !=
                0)
                exit_with_err("pthread_cond_wait", err);

        // aggiorno il totale
        totale = td->sh->risultato; //assegno il calcolo finale
        td->sh->op = INS; //ripristino lo stato di INS

        printf("[CALC-%u] risultato ricevuto %lld\n", td->thread_n + 1, totale);

        // rilascio il mutex
        if ((err = pthread_mutex_unlock(&td->sh->lock)) != 0)
            exit_with_err("pthread_mutex_unlock", err);
    }

    // ottengo il lock sulla struttura dati condivisa
    if ((err = pthread_mutex_lock(&td->sh->lock)) != 0)
        exit_with_err("pthread_mutex_lock", err);

    while (td->sh->op != INS) //Fin quando L'Operazione non è nello stato iniziale chiama una wait per non andare avanti
        if ((err = pthread_cond_wait(&td->sh->cond_calc, &td->sh->lock)) != 0)
            exit_with_err("pthread_cond_wait", err);

    // incremento il numero di CALC-i che hanno terminato
    td->sh->done++;

    if (totale == risultato) { //se il valore calcolato (totale) è uguale al valore aspettato nel file
        td->sh->success++;
        printf("[CALC-%u] computazione terminata in modo corretto: %lld\n",
               td->thread_n + 1, totale);
    } else {
        printf("[CALC-%u] computazione terminata in modo NON corretto: %lld\n",
               td->thread_n + 1, totale);
    }

    // se sono l'ultimo CALC-i procedo con la terminazione dei thread ADD,SUB e
    // MUL
    if (td->sh->done == td->ncalc) { //se tutti i calcoli hanno terminato (done è un counter di tutti i calcoli conclusi)
        td->sh->op = DONE;

        // sveglio i thread ADD, SUB e MUL per farli terminare
        if ((err = pthread_cond_signal(&td->sh->cond_add)) != 0)
            exit_with_err("pthread_cond_signal", err);
        if ((err = pthread_cond_signal(&td->sh->cond_sub)) != 0)
            exit_with_err("pthread_cond_signal", err);
        if ((err = pthread_cond_signal(&td->sh->cond_mul)) != 0)
            exit_with_err("pthread_cond_signal", err);
    } else { // altrimenti sveglio tutti i CALC-i che devono ancora finire
        if ((err = pthread_cond_broadcast(&td->sh->cond_calc)) != 0)
            exit_with_err("pthread_cond_broadcast", err);
    }

    // rilascio il mutex
    if ((err = pthread_mutex_unlock(&td->sh->lock)) != 0)
        exit_with_err("pthread_mutex_unlock", err);

    fclose(f);
}

void add_thread(void *arg) {
    int err;
    thread_data *td = (thread_data *)arg; //puntatore che da accesso alla struttura condivsa dei thread

    while (1) { 
        // acquisisco il lock sulla struttura dati condivisa  
        if ((err = pthread_mutex_lock(&td->sh->lock)) != 0) 
            exit_with_err("pthread_mutex_lock", err);

        // verifico le condizioni di operabilità 
        while (td->sh->op != ADD && td->sh->op != DONE) //scorre fin qunado la variabile op non assume il valore add e done non è terminato
            if ((err = pthread_cond_wait(&td->sh->cond_add, &td->sh->lock)) != 0)
                exit_with_err("pthread_cond_wait", err);

        // verifico se devo terminare se si rilascio il lock
        if (td->sh->op == DONE) {
            if ((err = pthread_mutex_unlock(&td->sh->lock)) != 0)
                exit_with_err("pthread_mutex_unlock", err);
            break;
        }

        td->sh->risultato = td->sh->operando_1 + td->sh->operando_2; //pongo il risultato uguale alla somma dei due operandi
        td->sh->op = RES; //pongo l'operazione a res indicando che il risultato è pronto

        printf("[ADD] calcolo effettuato: %lld + %lld = %lld\n",
               td->sh->operando_1, td->sh->operando_2, td->sh->risultato);

        // sveglio tutti i CALC-i
        if ((err = pthread_cond_broadcast(&td->sh->cond_calc)) != 0)
            exit_with_err("pthread_cond_broadcast", err);

        // rilascio il lock sulla struttura dati condivisa
        if ((err = pthread_mutex_unlock(&td->sh->lock)) != 0)
            exit_with_err("pthread_mutex_lock", err);
    }
}

void sub_thread(void *arg) {
    int err;
    thread_data *td = (thread_data *)arg; 

    while (1) {
        // acquisisco il lock sulla struttura dati condivisa
        if ((err = pthread_mutex_lock(&td->sh->lock)) != 0)
            exit_with_err("pthread_mutex_lock", err);

        // verifico le condizioni di operabilità
        while (td->sh->op != SUB && td->sh->op != DONE)
            if ((err = pthread_cond_wait(&td->sh->cond_sub, &td->sh->lock)) !=
                0)
                exit_with_err("pthread_cond_wait", err);

        // verifico se devo terminare
        if (td->sh->op == DONE) {
            if ((err = pthread_mutex_unlock(&td->sh->lock)) != 0)
                exit_with_err("pthread_mutex_unlock", err);

            break;
        }

        td->sh->risultato = td->sh->operando_1 - td->sh->operando_2;
        td->sh->op = RES;

        printf("[SUB] calcolo effettuato: %lld - %lld = %lld\n",
               td->sh->operando_1, td->sh->operando_2, td->sh->risultato);

        // sveglio tutti i CALC-i in attesa sulla variabile condizione cond_calc
        if ((err = pthread_cond_broadcast(&td->sh->cond_calc)) != 0)
            exit_with_err("pthread_cond_broadcast", err);

        // rilascio il lock sulla struttura dati condivisa
        if ((err = pthread_mutex_unlock(&td->sh->lock)) != 0)
            exit_with_err("pthread_mutex_lock", err);
    }
}

void mul_thread(void *arg) {
    int err;
    thread_data *td = (thread_data *)arg;

    while (1) {
        if ((err = pthread_mutex_lock(&td->sh->lock)) != 0)
            exit_with_err("pthread_mutex_lock", err);

        while (td->sh->op != MUL && td->sh->op != DONE)
            if ((err = pthread_cond_wait(&td->sh->cond_mul, &td->sh->lock)) !=
                0)
                exit_with_err("pthread_cond_wait", err);

        if (td->sh->op == DONE) {
            if ((err = pthread_mutex_unlock(&td->sh->lock)) != 0)
                exit_with_err("pthread_mutex_unlock", err);

            break;
        }

        td->sh->risultato = td->sh->operando_1 * td->sh->operando_2;
        td->sh->op = RES;

        printf("[MUL] calcolo effettuato: %lld x %lld = %lld\n",
               td->sh->operando_1, td->sh->operando_2, td->sh->risultato);

        // sveglio tutti i CALC-i in attesa sulla variabile condizione cond_calc
        if ((err = pthread_cond_broadcast(&td->sh->cond_calc)) != 0)
            exit_with_err("pthread_cond_signal", err);

        if ((err = pthread_mutex_unlock(&td->sh->lock)) != 0)
            exit_with_err("pthread_mutex_lock", err);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <calc-file-1> <calc-file-2> ... <calc-file-n>\n",
               argv[0]);
        exit(EXIT_FAILURE);
    }
    int err;
    thread_data td[3 + argc - 1]; //argc - 1 (un thread per ogni file) e 3 thread per gli operatori add, sub, mul
    shared *sh = malloc(sizeof(shared));
    init_shared(sh);
        
    // Calc-i
    for (int i = 0; i < argc - 1; i++) {
        td[i].sh = sh; //struttura condivisa
        td[i].thread_n = i; //numero del thread
        td[i].input_file = argv[i + 1]; //nome del filo associato al thread
        td[i].ncalc = argc - 1; //numero totale di thread CALC-I

        if ((err = pthread_create(&td[i].tid, 0, (void *)calc_thread,
                                  &td[i])) != 0)
            exit_with_err("pthread_create", err);
    }

    // ADD
    td[argc - 1].sh = sh; //il file che ha solo le addizioni file 0

    if ((err = pthread_create(&td[argc - 1].tid, 0, (void *)add_thread,
                              &td[argc - 1])) != 0)
        exit_with_err("pthread_create", err);

    // SUB
    td[argc].sh = sh; //file sottrazioni  file 1

    if ((err = pthread_create(&td[argc].tid, 0, (void *)sub_thread,
                              &td[argc])) != 0)
        exit_with_err("pthread_create", err);

    // MUL  
    td[argc + 1].sh = sh; //file moltiplicazioni file 2

    if ((err = pthread_create(&td[argc + 1].tid, 0, (void *)mul_thread,
                              &td[argc + 1])) != 0)
        exit_with_err("pthread_create", err);

    for (int i = 0; i <= argc + 1; i++)
        if ((err = pthread_join(td[i].tid, NULL)) != 0)
            exit_with_err("pthread_join", err);

    printf("[MAIN] verifiche completate con successo: %u/%d\n", sh->success,
           argc - 1);

    shared_destroy(sh);
}