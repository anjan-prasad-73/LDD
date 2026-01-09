#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 4
#define ITERATIONS  1000000

int shared_counter = 0;
pthread_spinlock_t spinlock;

void* worker(void* arg)
{
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_spin_lock(&spinlock);
        shared_counter++;
        pthread_spin_unlock(&spinlock);
    }
    return NULL;
}

int main()
{
    pthread_t threads[NUM_THREADS];

    /* Initialize spinlock (PTHREAD_PROCESS_PRIVATE for same process) */
    pthread_spin_init(&spinlock, PTHREAD_PROCESS_PRIVATE);

    /* Create threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }

    /* Wait for threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_spin_destroy(&spinlock);

    printf("Final counter value: %d\n", shared_counter);
    printf("Expected value: %d\n", NUM_THREADS * ITERATIONS);

    return 0;
}

