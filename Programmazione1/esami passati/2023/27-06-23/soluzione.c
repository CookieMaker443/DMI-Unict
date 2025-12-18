#define MAXLEN 255
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<stdbool.h>

unsigned int get_random() {
    static unsigned int m_w = 123456;
    static unsigned int m_z = 789123;
    m_z = 36969 * (m_z & 65535) + (m_z >> 16);
    m_w = 18000 * (m_w & 65535) + (m_w >> 16);
    return (m_z << 16) + m_w;
}

struct parameters {
    char filename[MAXLEN];
    int n;
};

struct record {
    int numeroConto;
    char nome[MAXLEN];
    char cognome[MAXLEN];
    double balance;
};

struct node {
    struct record data;
    struct node* next;
};

typedef struct record Record;
typedef struct parameters Parameters;
typedef struct node Node;

Node* createNode(Record data) {
    Node* newNode = malloc(sizeof(Node));
    if (newNode == NULL) {
        fprintf(stderr, "Errore di allocazione della memoria");
    }

    newNode->data = data;
    newNode->next = NULL;

    return newNode;
}

void insertRecord(Node** head, Record record) {
    Node* newNode = createNode(record);

    if (*head == NULL) {
        *head = newNode;
    } else {
        Node* current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newNode;
    }
}

void printRecord(Record r) {
    printf("%d %s %s %.2f\n", r.numeroConto, r.nome, r.cognome, r.balance);
}

void printRecords(Node* head) {
    Node* current = head;

    while (current != NULL) {
        Record r = current->data;
        printRecord(r);
        current = current->next;
    }
}

int listlen(Node* head) {
    Node* current = head;
    int i = 0;

    while (current != NULL) { 
        i++;
        current = current->next;
    }

    return i;
}

Parameters decodeParameters(int argc, char *argv[]) {
    Parameters p;

    if(argc != 3) {
        fprintf(stderr, "Errore, il numero di parametri deve essere uguale a 2\n");
        exit(-1);
    }

    strcpy(p.filename, argv[1]);

    int i = strlen(p.filename)-4;

    if(strcmp(p.filename+strlen(p.filename)-4, ".bin")!=0) {
        fprintf(stderr, "Errore, il nome del file non termina con '.bin'\n");
        exit(-1);
    }

    char *end;
    p.n = (int) strtol(argv[2], &end, 10);

    if(*end) {
        fprintf(stderr, "Errore: n non è un intero valido.\n");
        exit(-1);
    }

    if(p.n<3 || p.n>5) {
        fprintf(stderr, "Errore: n deve essere compreso tra 3 e 5.\n");
        exit(-1);
    }

    return p;
}

Record* getArray(Node *head) {
    Record* X = calloc(listlen(head), sizeof(Record));

    Node *ptr = head;

    int i = 0;

    while(ptr) {
        X[i++] = ptr->data;
        ptr = ptr->next;
    }

    return X;
}

void printArray(Record *X, int len) {
    for(int i=0; i<len; i++) {
        printRecord(X[i]);
    }
}

Record readRecord() {
    Record r;

    scanf("%d %s %s %lf", &r.numeroConto, r.nome, r.cognome, &r.balance);

    return r;
}

void loadRecords(Node **head) {
    bool end = false;
    while (!end) {
        Record r = readRecord();
        if (feof(stdin)) {
            end = true;
            break;
        }
        insertRecord(head, r);
    }
}

void sort(Record* X, int n) {
    double m = 0;
    for (int i=0; i<n; i++) {
        m += X[i].balance;
    }

    m/=n;

    for(int pass = 0; pass < n-1; pass++) {
        int idx_min = pass;
        for(int i = pass+1; i<n; i++) { 
            if((X[i].balance-m)*(X[i].balance-m) < (X[idx_min].balance-m)*(X[idx_min].balance-m))
                idx_min = i;
        }

        Record tmp = X[pass];
        X[pass] = X[idx_min];
        X[idx_min] = tmp;
    }
}

void saveToFile(Record X[], char filename[], int n) {
    FILE *cfPtr = NULL;

    if ((cfPtr = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "Impossibile aprire il file.");
        exit(-1);
    }

    for(int i = 0; i<n; i++){
        fwrite(&X[i], sizeof(Record), 1, cfPtr);
    }
    fclose(cfPtr);
}

int* getRandomOrder(int n) {
    int* order = calloc(n, sizeof(int));

    for(int i=0; i<n; i++) order[i]=i;

    for(int i=0; i<n; i++) {
        int j = get_random()%n;
        int tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }

    return order;
}

void showFileContent(char filename[], int n) {
    FILE *cfPtr = NULL;

    int* order = getRandomOrder(n);

    if ((cfPtr = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "Impossibile aprire il file.");
        exit(-1);
    }

    for(int i=0; i<n; i++) {
        fseek(cfPtr, order[i]*sizeof(Record), SEEK_SET);
        Record r;
        size_t result = fread(&r, sizeof(Record), 1, cfPtr);
        if(result!=0)
            printRecord(r);
    }

    fclose(cfPtr);
}

int main(int argc, char *argv[]) {
    Parameters p = decodeParameters(argc, argv);

    printf("PUNTO A - valori dei parametri presi in input:\n");
    printf("filename = %s\n", p.filename);
    printf("n = %d\n", p.n);

    Node* L = NULL;

    loadRecords(&L);

    printf("\nPUNTO B - Contenuto di L:\n");
    printRecords(L);

    Record* X = getArray(L);

    sort(X, listlen(L));
    printf("\nPUNTO C - Contenuto di X dopo l'ordinamento:\n");
    printArray(X, listlen(L));

    saveToFile(X, p.filename, listlen(L));
    printf("\nPUNTO D - Contenuto del file binario in ordine casuale:\n");
    showFileContent(p.filename, p.n);
}