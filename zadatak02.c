#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <termios.h>
#include <semaphore.h>
#include <unistd.h>
#include "getch.c"

#define RING_SIZE (20)
#define SLEEPING_TIME (400000)

char getch(void);

struct UlazniKruzniBafer{
    unsigned int rep;
    unsigned int glava;
    unsigned char niz[RING_SIZE];
};
/* Operacije za rad sa kruznim baferom. */
char uzmiKarakter (struct UlazniKruzniBafer *ulaz_baf)
{
    int index;
    index = ulaz_baf->glava;
    ulaz_baf->glava = (ulaz_baf->glava + 1) % RING_SIZE;
    return ulaz_baf->niz[index];
}

void staviKarakter (struct UlazniKruzniBafer *ulaz_baf, const char c)
{
    ulaz_baf->niz[ulaz_baf->rep] = c;
    ulaz_baf->rep = (ulaz_baf->rep + 1) % RING_SIZE;
}

/* Globalne promenljive. */

/* Kruzni bafer. */
static struct UlazniKruzniBafer ulazni_bafer;

static sem_t semPrazan;
static sem_t semPun;
static sem_t semSignalKraja;

static pthread_mutex_t ulaz_mutex;

struct IzlazniKruzniBafer{
    unsigned int tail;
    unsigned int head;
    unsigned char data[RING_SIZE];
};
/* Operacije za rad sa kruznim baferom. */
char ringBufGetChar (struct IzlazniKruzniBafer *izlazbafer)
{
    int index;
    index = izlazbafer->head;
    izlazbafer->head = (izlazbafer->head + 1) % RING_SIZE;
    return izlazbafer->data[index];
}

void ringBufPutChar (struct IzlazniKruzniBafer *izlazbafer, const char c)
{
    izlazbafer->data[izlazbafer->tail] = c;
    izlazbafer->tail = (izlazbafer->tail + 1) % RING_SIZE;
}

/* Globalne promenljive. */

/* Kruzni bafer. */
static struct IzlazniKruzniBafer izlazni_bufer;

static sem_t semEmpty;
static sem_t semFull;
static sem_t semFinishSignal;

static pthread_mutex_t izlaz_mutex;


void* ulazna_nit(void *param){
    char c;

    while (1)
    {
        if (sem_trywait(&semSignalKraja) == 0)
        {
            break;
        }

        if (sem_wait(&semPrazan) == 0)
        {

            /* Funkcija za unos karaktera sa tastature. */
            c = getch();

            /* Ukoliko je unet karakter q ili Q signalizira se 
               programskim nitima zavrsetak rada. */
            if (c == 'q' || c == 'Q')
            {
                /* Zaustavljanje niti; Semafor se oslobadja 2 puta
                da bi signaliziralo obema nitima.*/
                sem_post(&semSignalKraja);
                sem_post(&semSignalKraja);
                break;
            }

            /* Pristup kruznom baferu. */
            pthread_mutex_lock(&ulaz_mutex);
            staviKarakter(&ulazni_bafer, c);
            pthread_mutex_unlock(&ulaz_mutex);

            sem_post(&semPun);
        }
    }

    return 0;
}


void* obrada(void *Pparam){
    char c;

    while (1)
    {
        if (sem_trywait(&semSignalKraja) == 0)
        {
            break;
        }

        if (sem_wait(&semPun) == 0)
        {
            /* Pristup kruznom baferu. */
            pthread_mutex_lock(&ulaz_mutex);
            c = uzmiKarakter(&ulazni_bafer);
            pthread_mutex_unlock(&ulaz_mutex);

            /* Ako je karakter malo slovo, pretvori ga u veliko. */
            if (c >= 'a' && c <= 'z') {
                c -= 0x20;
            }

            if (sem_wait(&semEmpty) == 0)
            {
                /* Pristup kruznom baferu. */
                pthread_mutex_lock(&izlaz_mutex);
                ringBufPutChar(&izlazni_bufer, c);
                pthread_mutex_unlock(&izlaz_mutex);

                sem_post(&semFull);
            }

            /* Cekanje da bi se ilustrovalo trajanje obrade. */
            usleep(SLEEPING_TIME);
        }
    }

    return 0;
}

void* izlazna(void *pParam){
    char c;

    while (1)
    {
        if (sem_trywait(&semSignalKraja) == 0)
        {
            break;
        }

        if (sem_wait(&semFull) == 0)
        {
            /* Pristup kruznom baferu. */
            pthread_mutex_lock(&izlaz_mutex);
            c = ringBufGetChar(&izlazni_bufer);
            pthread_mutex_unlock(&izlaz_mutex);

            /* Ispis na konzolu. */
            printf("%c", c);
            fflush(stdout);

            /* Cekanje da bi se ilustrovalo trajanje obrade. */
            usleep(SLEEPING_TIME);

            sem_post(&semEmpty);
        }
    }

    return 0;
}

int main(){

    pthread_t ulaz;
    pthread_t nitobrada;
    pthread_t izlaz;

    sem_init(&semEmpty, 0, RING_SIZE);
    sem_init(&semFull, 0, 0);
    sem_init(&semFinishSignal, 0, 0);

    sem_init(&semPrazan, 0, RING_SIZE);
    sem_init(&semPun, 0, 0);
    sem_init(&semSignalKraja, 0, 0);


    /* Inicijalizacija objekta iskljucivog pristupa. */
    pthread_mutex_init(&izlaz_mutex, NULL);
    pthread_mutex_init(&ulaz_mutex, NULL);

    pthread_create(&ulaz, NULL, ulazna_nit,0);
    pthread_create(&nitobrada, NULL, obrada, 0);
    pthread_create(&izlaz,NULL,izlazna, 0);

    pthread_join(ulaz, NULL);
    pthread_join(nitobrada, NULL);
    pthread_join(izlaz, NULL);

    /* Oslobadjanje resursa. */
    sem_destroy(&semEmpty);
    sem_destroy(&semFull);
    sem_destroy(&semFinishSignal);

    sem_destroy(&semPrazan);
    sem_destroy(&semPun);
    sem_destroy(&semSignalKraja);

    pthread_mutex_destroy(&izlaz_mutex);
    pthread_mutex_destroy(&ulaz_mutex);

    return 0;
}
