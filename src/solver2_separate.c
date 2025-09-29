#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <omp.h>

#include "function.h"

#define MAXQUEUE 10000

struct Interval {
    double left;    // left boundary
    double right;   // right boundary
    double tol;     // tolerance
    double f_left;  // function value at left boundary
    double f_mid;   // function value at midpoint
    double f_right; // function value at right boundary
};

struct Queue {
    struct Interval entry[MAXQUEUE]; // array of queue entries
    int16_t top;                     // index of last entry
    omp_lock_t lock;                 // Queue lock    
};

// add an interval to the queue
void enqueue(struct Interval interval, struct Queue *queue_p)
{
    if (queue_p->top == MAXQUEUE - 1) {
        printf("Maximum queue size exceeded - exiting\n");
        exit(1);
    }

    // Ensure that memory location of top is incremented by a single thread as
    // another thread might attempt to dequeue and decrement this value at the
    // same time.
#pragma omp atomic
    queue_p->top++;

    queue_p->entry[queue_p->top] = interval;
}

// extract last interval from queue
struct Interval dequeue(struct Queue *queue_p)
{
    if (queue_p->top == -1) {
        printf("Attempt to extract from empty queue - exiting\n");
        exit(1);
    }

    struct Interval interval;
    interval = queue_p->entry[queue_p->top];

    // Ensure that memory location of top is decremented by a single thread as
    // another thread might attempt to enqueue and incrment this value at the
    // same time.
#pragma omp atomic
    queue_p->top--;

    return interval;
}

// initialise queue
void initialize(struct Queue *queue_p)
{
    queue_p->top = -1;
    omp_init_lock(&queue_p->lock);
}

// terminate queue
void terminate(struct Queue *queue_p)
{
    omp_destroy_lock(&queue_p->lock);
    queue_p->top = -1;
}

// return whether queue is empty
int isempty(struct Queue *queue_p)
{
    int result = (queue_p->top == -1);

    return result;
}

// get current number of queue entries
int size(struct Queue *queue_p)
{
    return (queue_p->top + 1);
}



double simpson(double (*func)(double), struct Queue *queues, int queues_size)
{
    assert(func && queues);

    double quad = 0.0;

    // Keeps track of number of threads currently processing intervals so that 
    // we only terminate if both the queue is empty and no threads are 
    // processing.
    int active_threads = 0;
    
    #pragma omp parallel default(none) shared(func, queues, active_threads, queues_size) reduction(+: quad)
    {
        int thread_id = omp_get_thread_num();
        struct Queue *local_queue = &queues[thread_id];
        
        // Termination criteria must now be satisfied from within the loop
        while (1) {
            // Already have function values at left and right boundaries and midpoint
            // Now evaluate function at one-qurter and three-quarter points
            struct Interval interval;
            bool thread_has_work = false;

            omp_set_lock(&local_queue->lock);
            {
                if (!isempty(local_queue)) {
                    interval = dequeue(local_queue);
                    thread_has_work = true;

                    // Ensure that enqueuing or dequeuing does not try to modify 
                    // active_threads at the same time.
                    #pragma omp atomic
                    active_threads++;
                }
            }
            omp_unset_lock(&local_queue->lock);

            if (!thread_has_work) {
                // Attempt to steal work in a round robin fashion relative 
                // from the current thread. This is so that earlier threads
                // do not get a lot of work load.
                for (int attempt = 0; attempt < queues_size; ++attempt) {
                    // Next thread relative to current thread
                    int other_thread_id = (thread_id + attempt) % queues_size;
                    if (other_thread_id == thread_id)
                        continue;

                    struct Queue *other_queue = &queues[other_thread_id];

                    // Attempt to steal work from another thread. If the other 
                    // queue is locked then skip and try another queue.
                    if (omp_test_lock(&other_queue->lock)) {
                        if (!isempty(other_queue)) {
                            interval = dequeue(other_queue);
                            thread_has_work = true;

                            // Ensure that enqueuing or dequeuing does not try to modify 
                            // active_threads at the same time.
                            #pragma omp atomic
                            active_threads++;
                        }
                        omp_unset_lock(&other_queue->lock);

                        if (thread_has_work)
                            break;
                    }
                }
            }


            // Checking if a queue is empty is not enough as other threads 
            // might be currently processing intervals. Only terminate if
            // both the queue is empty and no threads are executing.
            bool terminate = (isempty(local_queue) && active_threads == 0);
            if (terminate) {
                break;
            }

            // If the thread has no work then go back to the start
            if (!thread_has_work) {
                continue;
            }

            double h  = interval.right - interval.left;
            double c  = (interval.left + interval.right) / 2.0;
            double d  = (interval.left + c) / 2.0;
            double e  = (c + interval.right) / 2.0;
            double fd = func(d);
            double fe = func(e);

            // Calculate integral estimates using 3 and 5 points respectively
            double q1 = h / 6.0  * (interval.f_left + 4.0 * interval.f_mid + interval.f_right);
            double q2 = h / 12.0 * (interval.f_left + 4.0 * fd + 2.0 * interval.f_mid + 4.0 * fe + interval.f_right);

            if ((fabs(q2 - q1) < interval.tol) || ((interval.right - interval.left) < 1.0e-12)) {
                // Note that each thread has its own local copy of quad because of reduction clause
                // Tolerance is met, add to total
                quad += q2 + (q2 - q1) / 15.0;
            } else {
                // Tolerance is not met, split interval in two and add both halves to queue
                struct Interval i1, i2;

                i1.left    = interval.left;
                i1.right   = c;
                i1.tol     = interval.tol;
                i1.f_left  = interval.f_left;
                i1.f_mid   = fd;
                i1.f_right = interval.f_mid;

                i2.left    = c;
                i2.right   = interval.right;
                i2.tol     = interval.tol;
                i2.f_left  = interval.f_mid;
                i2.f_mid   = fe;
                i2.f_right = interval.f_right;

                // Add more intervals to be processed back to the top of the queue. 
                // Ensure that only a single thread can enqueue at any point in time.

                // Future work: If a queue becomes full, threads could 
                // attempt to distribute the work to others threads
                // where their queues are not full.                     

                omp_set_lock(&local_queue->lock);
                {
                    enqueue(i1, local_queue);
                    enqueue(i2, local_queue);
                }                
                omp_unset_lock(&local_queue->lock);
            }

            // Ensure that enqueuing or dequeuing does not try to modify 
            // active_threads at the same time.
            #pragma omp atomic
            active_threads--;

        } // while
    } // parallel

    return quad;
}

int main(void)
{
    int thread_count = omp_get_max_threads();
    printf("Threads: %d\n", thread_count);

    // Allocate a separate queue for each thread
    struct Queue *queues = (struct Queue *)malloc(sizeof(struct Queue) * thread_count);

    // Initiale queue for each thread
    for (int i = 0; i < thread_count; ++i) {
        initialize(&queues[i]);
    }

    double start = omp_get_wtime();

    // Add initial interval to the queue
    struct Interval whole;
    whole.left    = 0.0;
    whole.right   = 10.0;
    whole.tol     = 1e-06;
    whole.f_left  = func1(whole.left);
    whole.f_right = func1(whole.right);
    whole.f_mid   = func1((whole.left + whole.right) / 2.0);

    enqueue(whole, &queues[0]);

    // Call queue-based quadrature routine
    // Pass array queues into simpson function so that threads can begin working
    printf("Result = %e\n", simpson(func1, queues, thread_count));
    printf("Time(s) = %f\n", omp_get_wtime() - start);

    // Terminate queue for each thread.
    for (int i = 0; i < thread_count; ++i) {
        terminate(&queues[i]);
    }

    free(queues);
}
