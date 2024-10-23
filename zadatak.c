#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <termios.h>
#include <unistd.h>

#define RING_SIZE (16)
#define SLEEPING_TIME (400000)

char getch(void);

struct KruzniBafer {
    unsigned int rep;
    unsigned int glava;
    unsigned char niz[RING_SIZE];
    pthread_mutex_t mutex;
    sem_t semPrazan;
    sem_t semPun;
    sem_t semSignalKraja;
};

static struct KruzniBafer bafer;

char UzmiKarakter(struct KruzniBafer *baf) {
    int index = baf->glava;
    baf->glava = (baf->glava + 1) % RING_SIZE;
    return baf->niz[index];
}

void staviKarakter(struct KruzniBafer *baf, const char c) {
    baf->niz[baf->rep] = c;
    baf->rep = (baf->rep + 1) % RING_SIZE;
}

void InicijalizujStrukture(struct KruzniBafer *baf) {
    sem_init(&(baf->semPrazan), 0, RING_SIZE);
    sem_init(&(baf->semPun), 0, 0);
    sem_init(&(baf->semSignalKraja), 0, 0);
    pthread_mutex_init(&(baf->mutex), NULL);
}

void UnistiStrukture(struct KruzniBafer *baf) {
    sem_destroy(&(baf->semPrazan));
    sem_destroy(&(baf->semPun));
    sem_destroy(&(baf->semSignalKraja));
    pthread_mutex_destroy(&(baf->mutex));
}

void* ulazna_nit(void *param) {
    char c;
    while (1) {
        if (sem_trywait(&(bafer.semSignalKraja)) == 0) {
            break;
        }

        if (sem_wait(&(bafer.semPrazan)) == 0) {
            c = getch();

            pthread_mutex_lock(&(bafer.mutex));
            staviKarakter(&bafer, c);
            pthread_mutex_unlock(&(bafer.mutex));

            sem_post(&(bafer.semPun));
        }
    }
    return 0;
}

void* obrada(void *param) {
    char c;

    while (1) {
        if (sem_trywait(&(bafer.semSignalKraja)) == 0) {
            break;
        }

        if (sem_wait(&(bafer.semPun)) == 0) {
            pthread_mutex_lock(&(bafer.mutex));
            c = UzmiKarakter(&bafer);
            pthread_mutex_unlock(&(bafer.mutex));

            
            if (c >= 'a' && c <= 'z') {
                c -= 0x20;
            }

         
            if (sem_wait(&(bafer.semPrazan)) == 0) {
                pthread_mutex_lock(&(bafer.mutex));
                staviKarakter(&bafer, c);
                pthread_mutex_unlock(&(bafer.mutex));

                sem_post(&(bafer.semPun));
            }

            usleep(SLEEPING_TIME);
        }
    }
    return 0;
}

void* izlazna_nit(void *param) {
    char c;

    while (1) {
        if (sem_trywait(&(bafer.semSignalKraja)) == 0) {
            break;
        }

        if (sem_wait(&(bafer.semPun)) == 0) {
            pthread_mutex_lock(&(bafer.mutex));
            c = UzmiKarakter(&bafer);
            pthread_mutex_unlock(&(bafer.mutex));

            printf("%c", c);
            fflush(stdout);

            usleep(SLEEPING_TIME);

            sem_post(&(bafer.semPrazan));
        }
    }
    return 0;
}

int main() {
    InicijalizujStrukture(&bafer);

    pthread_t ulaz, nitobrada, izlaz;

    pthread_create(&ulaz, NULL, ulazna_nit, NULL);
    pthread_create(&nitobrada, NULL, obrada, NULL);
    pthread_create(&izlaz, NULL, izlazna_nit, NULL);

    pthread_join(ulaz, NULL);
    pthread_join(nitobrada, NULL);
    pthread_join(izlaz, NULL);

    UnistiStrukture(&bafer);

    return 0;
}

