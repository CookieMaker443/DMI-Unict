#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ctype.h>
#define MAX_LINE_LENGTH 1024
#define MAX_FILENAME_LENGTH 256

typedef struct {
    char line[MAX_LINE_LENGTH];
    char filename[MAX_FILENAME_LENGTH];
    pthread_mutex_t mutex;
    pthread_cond_t cond_reader;
    pthread_cond_t cond_filterer;
    int ready; // 1 se la riga è pronta per il Filterer, 0 altrimenti
} ReaderFiltererSharedData;

typedef struct {
    char output_line[MAX_LINE_LENGTH + MAX_FILENAME_LENGTH + 2];
    pthread_mutex_t mutex;
    pthread_cond_t cond_filterer;
    pthread_cond_t cond_writer;
    int ready; // 1 se la riga è pronta per il Writer, 0 altrimenti
} FiltererWriterSharedData;

typedef struct {
    char *word;
    int invert_match;
    int ignore_case;
} FilterOptions;

void *reader_thread(void *arg) {
    ReaderFiltererSharedData *shared_data = (ReaderFiltererSharedData *)arg;
    char *filename = shared_data->filename;
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("open");
        pthread_exit(NULL);
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        close(fd);
        pthread_exit(NULL);
    }

    char *file_content = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_content == MAP_FAILED) {
        perror("mmap");
        close(fd);
        pthread_exit(NULL);
    }

    close(fd);

    char *line = strtok(file_content, "\n");
    while (line != NULL) {
        pthread_mutex_lock(&shared_data->mutex);
        while (shared_data->ready == 1) {
            pthread_cond_wait(&shared_data->cond_reader, &shared_data->mutex);
        }
        strncpy(shared_data->line, line, MAX_LINE_LENGTH);
        shared_data->ready = 1;
        pthread_cond_signal(&shared_data->cond_filterer);
        pthread_mutex_unlock(&shared_data->mutex);

        line = strtok(NULL, "\n");
    }

    munmap(file_content, sb.st_size);
    pthread_exit(NULL);
}

void *filterer_thread(void *arg) {
    FiltererWriterSharedData *writer_shared_data = (FiltererWriterSharedData *)arg;
    ReaderFiltererSharedData *reader_shared_data = (ReaderFiltererSharedData *)arg;
    FilterOptions *options = (FilterOptions *)arg;

    while (1) {
        pthread_mutex_lock(&reader_shared_data->mutex);
        while (reader_shared_data->ready == 0) {
            pthread_cond_wait(&reader_shared_data->cond_filterer, &reader_shared_data->mutex);
        }

        char *line = reader_shared_data->line;
        char *filename = reader_shared_data->filename;
        int match = 0;

        if (options->ignore_case) {
            match = (strcasecmp(line, options->word) != NULL);
        } else {
            match = (strstr(line, options->word) != NULL);
        }

        if (options->invert_match) {
            match = !match;
        }

        if (match) {
            pthread_mutex_lock(&writer_shared_data->mutex);
            while (writer_shared_data->ready == 1) {
                pthread_cond_wait(&writer_shared_data->cond_filterer, &writer_shared_data->mutex);
            }
            snprintf(writer_shared_data->output_line, sizeof(writer_shared_data->output_line), "%s:%s", filename, line);
            writer_shared_data->ready = 1;
            pthread_cond_signal(&writer_shared_data->cond_writer);
            pthread_mutex_unlock(&writer_shared_data->mutex);
        }

        reader_shared_data->ready = 0;
        pthread_cond_signal(&reader_shared_data->cond_reader);
        pthread_mutex_unlock(&reader_shared_data->mutex);
    }

    pthread_exit(NULL);
}

void *writer_thread(void *arg) {
    FiltererWriterSharedData *shared_data = (FiltererWriterSharedData *)arg;

    while (1) {
        pthread_mutex_lock(&shared_data->mutex);
        while (shared_data->ready == 0) {
            pthread_cond_wait(&shared_data->cond_writer, &shared_data->mutex);
        }

        printf("%s\n", shared_data->output_line);
        shared_data->ready = 0;
        pthread_cond_signal(&shared_data->cond_filterer);
        pthread_mutex_unlock(&shared_data->mutex);
    }

    pthread_exit(NULL);
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s [-v] [-i] <word> <file-1> [file-2] [file-3] [...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    FilterOptions options = {0};
    int opt;
    while ((opt = getopt(argc, argv, "vi")) != -1) {
        switch (opt) {
            case 'v':
                options.invert_match = 1;
                break;
            case 'i':
                options.ignore_case = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-v] [-i] <word> <file-1> [file-2] [file-3] [...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    options.word = argv[optind];
    int num_files = argc - optind - 1;
    char **filenames = &argv[optind + 1];

    pthread_t readers[num_files];
    pthread_t filterer, writer;

    ReaderFiltererSharedData reader_shared_data[num_files];
    FiltererWriterSharedData writer_shared_data;

    pthread_mutex_init(&writer_shared_data.mutex, NULL);
    pthread_cond_init(&writer_shared_data.cond_filterer, NULL);
    pthread_cond_init(&writer_shared_data.cond_writer, NULL);
    writer_shared_data.ready = 0;

    for (int i = 0; i < num_files; i++) {
        pthread_mutex_init(&reader_shared_data[i].mutex, NULL);
        pthread_cond_init(&reader_shared_data[i].cond_reader, NULL);
        pthread_cond_init(&reader_shared_data[i].cond_filterer, NULL);
        reader_shared_data[i].ready = 0;
        strncpy(reader_shared_data[i].filename, filenames[i], MAX_FILENAME_LENGTH);
        pthread_create(&readers[i], NULL, reader_thread, &reader_shared_data[i]);
    }

    pthread_create(&filterer, NULL, filterer_thread, &options);
    pthread_create(&writer, NULL, writer_thread, &writer_shared_data);

    for (int i = 0; i < num_files; i++) {
        pthread_join(readers[i], NULL);
    }

    pthread_join(filterer, NULL);
    pthread_join(writer, NULL);

    for (int i = 0; i < num_files; i++) {
        pthread_mutex_destroy(&reader_shared_data[i].mutex);
        pthread_cond_destroy(&reader_shared_data[i].cond_reader);
        pthread_cond_destroy(&reader_shared_data[i].cond_filterer);
    }

    pthread_mutex_destroy(&writer_shared_data.mutex);
    pthread_cond_destroy(&writer_shared_data.cond_filterer);
    pthread_cond_destroy(&writer_shared_data.cond_writer);

    return 0;
}