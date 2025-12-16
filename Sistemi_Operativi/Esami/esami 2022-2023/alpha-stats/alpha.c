#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum { AL_N, MZ_N, PARENT_N } thread_n;

typedef struct {
    char c;
    unsigned long stats[26];
    bool done;
    sem_t sem[3];
} shared;

typedef struct {
    pthread_t tid;
    char thread_n;
    shared *shared;
} thread_data;

shared *init_shared() {
    shared *sh = malloc(sizeof(shared));

    sh->done = false;
    memset(sh->stats, 0, sizeof(sh->stats));

    if (sem_init(&sh->sem[PARENT_N], 0, 1) != 0) {
        perror("sem_init PARENT_N");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&sh->sem[AL_N], 0, 0) != 0) {
        perror("sem_init AL_N");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&sh->sem[MZ_N], 0, 0) != 0) {
        perror("sem_init MZ_N");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void shared_destroy(shared *sh) {
    for (int i = 0; i < 3; i++) {
        sem_destroy(&sh->sem[i]);
    }
    free(sh);
}

void *stats(void *arg) {
    thread_data *td = (thread_data *)arg;
    while (1) {
        if (sem_wait(&td->shared->sem[td->thread_n]) != 0) {
            perror("sem_wait");
             exit(EXIT_FAILURE);
        }

        if (td->shared->done) break;

        td->shared->stats[td->shared->c - 'a']++;

        if (sem_post(&td->shared->sem[PARENT_N]) != 0) {
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.txt>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int fd;
    shared *sh = init_shared();
    thread_data td[2];
    char *map;
    struct stat s_file;

    for (int i = 0; i < 2; i++) {
        td[i].shared = sh;
        td[i].thread_n = i;
    }

    if (pthread_create(&td[AL_N].tid, NULL, stats, &td[AL_N]) != 0) {
        perror("pthread_create AL_N");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&td[MZ_N].tid, NULL, stats, &td[MZ_N]) != 0) {
        perror("pthread_create MZ_N");
        exit(EXIT_FAILURE);
    }

    if ((fd = open(argv[1], O_RDONLY)) == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    if (fstat(fd, &s_file) == -1) {
        perror("fstat");
        exit(EXIT_FAILURE);
    }

    map = mmap(NULL, s_file.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    close(fd);

    for (int i = 0; i < s_file.st_size; i++) {
        if ((map[i] >= 'a' && map[i] <= 'z') || (map[i] >= 'A' && map[i] <= 'Z')) {
            if (sem_wait(&sh->sem[PARENT_N]) != 0) {
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            sh->c = tolower(map[i]);

            if (sh->c <= 'l') {
                if (sem_post(&sh->sem[AL_N]) != 0) {
                    perror("sem_post AL_N");
                    exit(EXIT_FAILURE);
                }
            } else {
                if (sem_post(&sh->sem[MZ_N]) != 0) {
                    perror("sem_post MZ_N");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    if (sem_wait(&sh->sem[PARENT_N]) != 0) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }

    printf("Statistiche sul file: %s\n", argv[1]);
    for (int i = 0; i < 26; i++) {
        printf("%c: %lu\t", i + 'a', sh->stats[i]);
    }
    printf("\n");

    sh->done = true;
    
    if((sem_post(&sh->sem[AL_N])) != 0){
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
        
    if ((sem_post(&sh->sem[MZ_N])) != 0){
        perror("sem_post");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < 2; i++) {
        if((pthread_join(td[i].tid, NULL)) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    shared_destroy(sh);
    if((munmap(map, s_file.st_size)) == -1){
        perror("munmap");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
