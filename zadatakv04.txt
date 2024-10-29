#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include "timer_event.h"
#include "ring_buffer.h"


char getch(void);
void resetTermios(void);

/* Globalne promenljive. */
static struct RingBuffer ring =
{0, 0, {'0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0'}};

static sem_t semEmpty;
static sem_t semFull;
static sem_t semPrintSignal;

static pthread_mutex_t bufferAccess;
static pthread_mutex_t brojac2_mutex;
int local_counter = 5;

int smanjiBrojac(){
    
    pthread_mutex_lock(&brojac2_mutex);
    if (local_counter > 0) {
        local_counter--;
    }
    int smanjen=local_counter;
    pthread_mutex_unlock(&brojac2_mutex);
    return smanjen;

}

int vratiBrojac(){
    
    pthread_mutex_lock(&brojac2_mutex);
    local_counter=5;
    pthread_mutex_unlock(&brojac2_mutex);
    return local_counter;
    
}

/* Nit proizvodjaca. */
void* producer (void *param)
{
    char c;

    while (1)
    {
        sem_wait(&semEmpty); //Blokira nit sve dok bafer nije prazan.

        /* Promena tipa ponistivosti niti proizvodjaca. */
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL); //podešava način otkazivanja niti tako da može odmah da se prekine.
        c = getch();
        pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

        pthread_mutex_lock(&bufferAccess);
        ringBufPutChar(&ring, c);
        pthread_mutex_unlock(&bufferAccess);

        sem_post(&semFull); //Oslobađa semFull semafor, signalizirajući da bafer sada ima karakter za čitanje.
    }

    return 0;
}

/* Nit potrosaca. */
void* consumer (void *param)
{
    char c;

    while (1)
    {
        if (local_counter == 0)
        {
            break;
        }


        if (sem_trywait(&semFull) == 0) //: Proverava da li postoji karakter u baferu, bez blokiranja niti.
        {
            /* Pristup kruznom baferu. */
            pthread_mutex_lock(&bufferAccess);
            c = ringBufGetChar(&ring);
            pthread_mutex_unlock(&bufferAccess);

            vratiBrojac();

            printf("Consumer: %c\n",c);
            fflush(stdout);
            sem_post(&semEmpty);//Oslobađa semEmpty semafor, signalizirajući da bafer ima slobodno mesto.
        }

       
    }


    return 0;
}

/* Nit za ispis vrednosti karaktera iz kruznog bafera. */
void* print_state (void *param)
{
    int i;

    while (1)
    {
    
        if (local_counter == 0)//Proverava signal kraja rada programa.
        {
            break;
        }

        if (sem_trywait(&semPrintSignal) == 0)//Proverava signal za ispis stanja bafera.
        {
            /* Pristup kruznom baferu. */
            printf("------------\n");
            pthread_mutex_lock(&bufferAccess); //moze da se canceluje, u vec definisanim
            for (i = 0 ; i < RING_SIZE ; i++)
            {
                printf("%c/", ring.data[i]);
            }
            printf("\n");
            pthread_mutex_unlock(&bufferAccess);
            printf("------------\n");
            fflush(stdout);
            
        }
        pthread_testcancel(); //za cancel
        
    }
    return 0;
}

/* Funkcija vremenske kontrole koje se poziva na svake dve sekunde. */
void* print_state_timer (void *param)
{
    sem_post(&semPrintSignal);// on kaze  e isprintaj, Oslobađa semafor semPrintSignal, signalizirajući niti print_state da ispiše sadržaj bafera.

    return 0;
}

void* brojac2_funkcija(void *param)
{
    smanjiBrojac();
    
    return 0;
}



int main (void)
{
    pthread_t hProducer;
    pthread_t hConsumer;
    pthread_t hPrintState;

    /* Promenljiva koja predstavlja vremensku kontrolu. */
    timer_event_t hPrintStateTimer;
    timer_event_t brojac2;


    sem_init(&semEmpty, 0, RING_SIZE); //Inicijalno ima vrednost RING_SIZE, što znači da je bafer potpuno prazan i da su sva mesta slobodna.
    sem_init(&semFull, 0, 0);
    sem_init(&semPrintSignal, 0, 0);

    pthread_mutex_init(&bufferAccess, NULL);
    pthread_mutex_init(&brojac2_mutex, NULL);

    pthread_create(&hProducer, NULL, producer, 0);
    pthread_create(&hConsumer, NULL, consumer, 0);
    pthread_create(&hPrintState, NULL, print_state, 0);
    

    /* Formiranje vremenske kontrole za funkciju print_state_timer. */
    timer_event_set(&hPrintStateTimer, 2000, print_state_timer, 0, TE_KIND_REPETITIVE); //da ne postavim repetitive vec create kill,  create kill etc
    timer_event_set(&brojac2, 1000, brojac2_funkcija, 0, TE_KIND_REPETITIVE); 

    
    pthread_join(hConsumer, NULL);
    pthread_cancel(hPrintState);
    pthread_join(hPrintState, NULL);
    pthread_cancel(hProducer);
    pthread_join(hProducer, NULL);

    /* Zaustavljanje vremenske kontrole. */
    timer_event_kill(hPrintStateTimer);
    timer_event_kill(brojac2);

    sem_destroy(&semEmpty);
    sem_destroy(&semFull);
    sem_destroy(&semPrintSignal);
    pthread_mutex_destroy(&bufferAccess);
    pthread_mutex_destroy(&brojac2_mutex);

    /* Nit proizvodjaca se najcesce prekida u izvrsavanju blokirajuce funkcije getch,
       koja u sebi sadrzi izmenu nacina rada terminala, a na samom kraju i vracanje
       na prethodno podesavanje terminala. Stoga je moguce da nakon prekida ove funkcije
       terminal ostatne u izmenjenom stanju. Funkcijom resetTermios obezbedjujemo da
       nakon ispravnog zavrsetka programa, terminal ostane u zatecenom stanju. */
    resetTermios();

    return 0;
}


