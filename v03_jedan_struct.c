#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <termios.h>
#include <unistd.h>
#include "getch.c"

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

static struct KruzniBafer ulazni_bafer, izlazni_bafer;

void InicijalizujStrukture(struct KruzniBafer *baf) {
    baf->rep = 0;
    baf->glava = 0;
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

void staviKarakter(struct KruzniBafer *baf, const char c) {
    sem_wait(&(baf->semPrazan));
    pthread_mutex_lock(&(baf->mutex));
    
    baf->niz[baf->rep] = c;
    baf->rep = (baf->rep + 1) % RING_SIZE;
    
    pthread_mutex_unlock(&(baf->mutex));
    sem_post(&(baf->semPun));
}

char UzmiKarakter(struct KruzniBafer *baf) {
    sem_wait(&(baf->semPun));
    pthread_mutex_lock(&(baf->mutex));

    int index = baf->glava;
    char c = baf->niz[index];
    baf->glava = (baf->glava + 1) % RING_SIZE;

    pthread_mutex_unlock(&(baf->mutex));
    sem_post(&(baf->semPrazan));

    return c;
}

void* ulazna_nit(void *param) {
    char c;
    while (1) {
        c = getch();
        if (c == 'q') {  // Kada je unet 'q', postavlja se signal za kraj
            sem_post(&(ulazni_bafer.semSignalKraja)); 
            sem_post(&(izlazni_bafer.semSignalKraja)); 
            break;
        }
        staviKarakter(&ulazni_bafer, c);
    }
    return 0;
}

void* obrada(void *param) {
    char c;

    while (1) {
        if (sem_trywait(&(ulazni_bafer.semSignalKraja)) == 0) {
            sem_post(&(izlazni_bafer.semSignalKraja));  // Signalizacija izlaznoj niti za kraj
            break;
        }

        c = UzmiKarakter(&ulazni_bafer);
        if (c >= 'a' && c <= 'z') {
            c -= 0x20;
        }
        staviKarakter(&izlazni_bafer, c);
        usleep(SLEEPING_TIME);
    }
    return 0;
}

void* izlazna_nit(void *param) {
    char c;

    while (1) {
        if (sem_trywait(&(izlazni_bafer.semSignalKraja)) == 0) {
            break;
        }
        c = UzmiKarakter(&izlazni_bafer);
        printf("%c", c);
        fflush(stdout);
        usleep(SLEEPING_TIME);
    }
    return 0;
}

int main() {
    InicijalizujStrukture(&ulazni_bafer);
    InicijalizujStrukture(&izlazni_bafer);

    pthread_t ulaz, nitobrada, izlaz;

    pthread_create(&ulaz, NULL, ulazna_nit, NULL);
    pthread_create(&nitobrada, NULL, obrada, NULL);
    pthread_create(&izlaz, NULL, izlazna_nit, NULL);

    pthread_join(ulaz, NULL);
    pthread_join(nitobrada, NULL);
    pthread_join(izlaz, NULL);

    UnistiStrukture(&ulazni_bafer);
    UnistiStrukture(&izlazni_bafer);

    return 0;
}
