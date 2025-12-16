#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <linux/limits.h>
#include <sys/stat.h>

typedef struct _node{
    unsigned long value;
    struct _node* next;
    struct _node* prev;
}node;

typedef struct{
    node* head;
    unsigned size;
}list;

void init_list(list* l){
    l->head = NULL;
    l->size = 0;
}

void insert_list(list* l, unsigned long value){
    node* n = malloc(sizeof(node));
    n->value = value;
    n->next = l->head;
    n->prev = NULL;
    l->head = n;

    if(n->next != NULL){
        n->next->prev = n;
    }

    l->size++;
}

unsigned long extract_min(list* l){
    unsigned long min;
    node* ptr = l->head->next;
    node* min_ptr = l->head;

    if(l->head == NULL){
        return 0;
    }

    while(ptr != NULL){
        if(min_ptr->value > ptr->value){
            min_ptr = ptr;
        }

        ptr = ptr->next;
    }

    min = min_ptr;

    if(min_ptr->prev != NULL){
        min_ptr->prev->next = min_ptr->next;
    }

    if(min_ptr->next != NULL){
        min_ptr->next->prev = min_ptr->prev;
    }

    if(l->head == min_ptr){
        l->head = l->head->next;
    }

    free(min_ptr);
    l->size--;

    return min;
}

unsigned long extract_max(list* l){
    unsigned long max;
    node* ptr = l->head->next;
    node* max_ptr = l->head;

    while(ptr != NULL){
        if(max_ptr->value < ptr->value){
            max_ptr = ptr;
        }
        ptr = ptr->next;
    }

    max = max_ptr;

    if(max_ptr->prev != NULL){
        max_ptr->prev->next = max_ptr->next;
    }

    if(max_ptr->next->prev != NULL){
        max_ptr->next->prev = max_ptr->prev;
    }

    if(l->head == max_ptr){
        l->head = l->head->next;
    }
    
    free(max_ptr);
    l->size--;

    return max;
}

void list_destroy(list* l){
    node* temp;
    node* ptr = l->head;

    while(ptr != NULL){
        temp = ptr;
        ptr = ptr->next;
        free(temp);
    }

    free(l);
}

typedef struct{
    list* l;
    unsigned done;

    pthread_mutex_t lock;
    pthread_cond_t cond;
}share;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    unsigned ndir;
    char* dirname;

    share* sh;
}thread_data;

share* init_share(){
    share* sh = malloc(sizeof(share));

    sh->done = 0;
    sh->l = malloc(sizeof(list));
    init_list(sh->l);

    if(pthread_mutex_init(&sh->lock, 0) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->cond, 0) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void destroy_sh(share* sh){
    list_destroy(sh->l);
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->cond);
    free(sh);
}

void dir_scanner(void* arg){
    thread_data* td = (thread_data*)arg;
    DIR* dr;
    struct dirent* dirent;
    struct stat statbuf;
    char path[PATH_MAX];

    if(dr = opendir(td->dirname) == NULL){
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while(dirent = readdir(dr)){
        snprintf(path, PATH_MAX, "%s/%s", td->dirname, dirent->d_name);

        if(lstat(path, &statbuf) == -1){
            perror("lstat");
            exit(EXIT_FAILURE);
        }

        if(S_ISREG(statbuf.st_mode)){
            printf("[D-%u] trovato il file '%s' in %s\n", td->thread_n, dirent->d_name, td->dirname);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(pthread_cond_signal(&td->sh->cond) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    td->sh->done++;

    if(pthread_cond_broadcast(&td->sh->cond) != 0){
        perror("pthread_cond_broadcast");
        exit(EXIT_FAILURE);
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(ExIT_FAILURE);
    }

    closedir(dr);
}

void add(void* arg){
    thread_data* td = (thread_data*)arg;
    unsigned long min, max, sum;
    unsigned done = 0;

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("ptread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->l->size < 2 && td->sh->done != td->ndir){
            if(pthread_cond_wait(&td->sh->done, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->sh->done == td->ndir && td->sh->l->size == 1){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }
        
        min = extract_min(td->sh->l);
        max = extract_max(td->sh->l);
        sum = min + max;

        printf("[ADD-%u] il minimo (%lu) ed il massimo (%lu) sono stati sostituiti da %lu; l'insieme ha adesso %u elementi.\n", td->thread_n, min, max, sum, td->sh->l->size);

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv){
    if(argc < 2){
        printf("Usage: %s <dir-1> <dir-2> ... <dir-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    unsigned ndir = argc - 1; //numero directory 
    thread_data td[ndir + 2];
    share* sh = init_share();

    for(int i = 0; i < ndir; i++){
        td[i].thread_n = i + 1;
        td[i].dirname = argv[i+1];
        td[i].sh = sh;

        if(pthread_create(&td[i].tid, NULL, (void*)dir_scanner, &td[i] != 0)){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i<2; i++){
        td[i + ndir].sh = sh;
        td[i + ndir].thread_n = i + 1;
        td[i + ndir].ndir = ndir;

        if(pthread_create(&td[i + ndir].tid, NULL, (void*)add, &td[i + ndir]!= 0)){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    for(int i=0; i< ndir + 2; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    printf("[MAIN] i thread secondari hanno terminato e il totale finale è di %lu byte.\n", sh->l->head->value);

    destroy_sh(sh);
}