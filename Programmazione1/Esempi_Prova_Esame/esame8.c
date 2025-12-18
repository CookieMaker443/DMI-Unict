#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned short int counter;
typedef struct data s_data;
struct data{
    int n;
    int m;
    int p;
    char nome[256];
};
typedef struct node s_node;
struct node{
    int data;
    s_node* next;

};

typedef struct stack s_stack;
struct stack{
    s_node* head;
    s_node* tail;
};

void readInput(int argc,char** argv,s_data* a){

    if(argc!=5){
    //     printf("numero di parametri non corretto (%i), arresto\n",argc);
    //     exit(-1);
    // }

    // a->n=strtol(argv[1],0,10);
    // a->m=strtol(argv[2],0,10);
    // a->p=strtol(argv[3],0,10);
    // strcpy(a->nome,argv[4]);

    a->n=5;
    a->m=3;
    a->p=2;
    strcpy(a->nome,"input.txt");



    printf("%i %i %i %s\n",a->m,a->n,a->p,a->nome);
    


    
    if(a->n<=0 || a->m<=0 || a->p<=0){
        puts("i valori non rispettano le sepecifiche, arresto");
        exit(-1);
    }
}
}

int*** initArray(s_data* a,char**argv){
    int*** A=(int***)malloc(a->n*sizeof(int**));
    FILE* fp=fopen(a->nome,"r");
    if(fp==NULL){
        fprintf(stderr,"errore apertura file\n");
        exit(-1);
    }
    
    for(counter i=0;i<a->n;i++){
        A[i]=(int**)malloc(a->m*sizeof(int*));
        for(counter j=0;j<a->m;j++){
            A[i][j]=(int*)malloc(a->p*sizeof(int));
            }
    }

    
    for(counter i=0;i<a->n;i++){
        puts("\n\n------------------------");
        for(counter j=0;j<a->m;j++){
            for(counter k=0;k<a->p;k++){
                fscanf(fp,"%i",&A[i][j][k]);
                printf("%i ",A[i][j][k]);
            }
            puts("");
        }
        puts("------------------------");
    }
    return A;

    fclose(fp);
}

int** initB(int*** A,s_data* a){
    int** B=(int**)malloc(a->n*sizeof(int*));
    for(counter i=0;i<a->n;i++){
        //printf("%i ",A[i][0][0]); non dimenticare questo viso, parola di giuseppe simone
        int* max=&A[i][0][0];
        for(counter j=0;j<a->m;j++){
            for(counter k=0;k<a->p;k++){
                if(*max<A[i][j][k]){
                    max=&A[i][j][k];
                }
            }
        }
        B[i]=max;
        printf("%i ",*B[i]);
    }
    puts("");
    return B;
}

s_stack* initStack(int** B,s_data* a){
    s_stack* pila=(s_stack*)malloc(sizeof(s_stack));

    pila->tail=(s_node*)malloc(sizeof(s_node));
    pila->tail->data=*B[0];
    pila->tail->next=NULL;
    pila->head=pila->tail;
    //printf(" |%i| <-",pila->head->data);
    
    for (counter i = 1; i < a->n; i++){
        s_node* new=(s_node*)malloc(sizeof(s_node));
        new->data=*B[i];
        new->next=pila->head;
        pila->head=new;
        //printf(" |%i| <-",pila->head->data);
    }


    return pila;
}

void saveStack(s_node* nodo,s_data* a){
    FILE* fp1=fopen("out.txt","w");
    if(!fp1){
        fprintf(stderr,"non riesco a creare il file, scappa scappa\n");
        exit(-1);
    }
    s_node* current=nodo;
    s_node* Bossetti;

    for(counter i=0;i<a->n;i++){
        fprintf(stdout,"%i ",current->data);
        fprintf(fp1,"%i ",current->data);
        Bossetti=current;
        current=current->next;
        free(Bossetti);
        }
        fclose(fp1);
}

void freeAll(int*** A,s_data* a,int** B,s_stack* pila){
    for(counter i=0;i<a->n;i++){
        for(counter j=0;j<a->m;j++){
            free(A[i][j]);
            }
    }
    for(counter i=0;i<a->n;i++){
        free(A[i]);
    }
    free(A);
    free(B);
    free(pila);
}

int main(int argc,char** argv){
    s_data a;
    readInput(argc,argv,&a);
    int*** A=initArray(&a,argv);
    int** B= initB(A,&a);
    s_stack* pila=initStack(B,&a);
    saveStack(pila->head,&a);
    freeAll(A,&a,B,pila);
    return 0;
}