#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>
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

    struct Interval interval = queue_p->entry[queue_p->top];

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

double simpson(double (*func)(double), struct Queue *queue_p)
{
    assert(func && queue_p);

    double quad = 0.0;

    // Keeps track of number of threads currently processing intervals so that 
    // we only terminate if both the queue is empty and no threads are 
    // processing.
    int active_threads = 0;

#pragma omp parallel default(none) shared(func, queue_p, active_threads) reduction(+: quad)
{
    // Already have function values at left and right boundaries and midpoint
    // Now evaluate function at one-qurter and three-quarter points
    struct Interval interval;

    // Termination criteria must now be satisfied from within the loop
    while (1) {
        bool work = false, done = false;

        // Only dequeue an interval from the queue if the queue is not 
        // empty. Then set work status as true and update active thread
        // count.
        omp_set_lock(&queue_p->lock);
        {
            if (!isempty(queue_p)) {
                interval = dequeue(queue_p);
                work = true;

                // Ensure that enqueuing or dequeuing does not try to modify 
                // active_threads at the same time.
                #pragma omp atomic
                active_threads++;
            }
            done = (isempty(queue_p) && active_threads == 0);
        }
        omp_unset_lock(&queue_p->lock);
    
        // Checking if a queue is empty is not enough as other threads 
        // might be currently processing intervals. Only terminate if
        // both the queue is empty and no threads are executing.
        if (done)
            break;

        if (!work)
            continue;

        double h  = interval.right - interval.left;
        double c  = (interval.left + interval.right) / 2.0;
        double d  = (interval.left + c) / 2.0;
        double e  = (c + interval.right) / 2.0;
        double fd = func(d);
        double fe = func(e);

        // Calculate integral estimates using 3 and 5 points respectively
        double q1 = h / 6.0 * (interval.f_left + 4.0 * interval.f_mid + interval.f_right);
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
            omp_set_lock(&queue_p->lock);
            enqueue(i1, queue_p);
            enqueue(i2, queue_p);
            omp_unset_lock(&queue_p->lock);
        }

        // Ensure that enqueuing or dequeuing does not try to modify 
        // active_threads at the same time.
        #pragma omp atomic
        active_threads--;
        
    } // while
    
} // #pragma omp parallel

    return quad;
}

int main(void)
{
    struct Queue queue;
    struct Interval whole;

    // Initialise queue
    initialize(&queue);

    double start = omp_get_wtime();

    printf("Threads: %d\n", omp_get_max_threads());

    // Add initial interval to the queue
    whole.left    = 0.0;
    whole.right   = 10.0;
    whole.tol     = 1e-06;
    whole.f_left  = func1(whole.left);
    whole.f_right = func1(whole.right);
    whole.f_mid   = func1((whole.left + whole.right) / 2.0);

    enqueue(whole, &queue);

    // Call queue-based quadrature routine
    printf("Result = %e\n", simpson(func1, &queue));
    printf("Time(s) = %f\n", omp_get_wtime() - start);

    terminate(&queue);
}
