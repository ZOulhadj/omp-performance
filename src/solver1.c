#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <assert.h>

#include "function.h"

struct Interval {
    double left;    // left boundary
    double right;   // right boundary
    double tol;     // tolerance
    double f_left;  // function value at left boundary
    double f_mid;   // function value at midpoint
    double f_right; // function value at right boundary
};

double simpson(double (*func)(double), struct Interval interval)
{
    assert(func);

    // Already have function evaluations at each end of the interval and in the middle
    // Now get function values at one-quarter and three-quarter points
    double h  = interval.right - interval.left;
    double c  = (interval.left + interval.right) / 2.0;
    double d  = (interval.left + c) / 2.0;
    double e  = (c + interval.right) / 2.0;
    double fd = func(d);
    double fe = func(e);

    // Compute integral estimates using 3 and 5 points respectively
    double q1 = h / 6.0 * (interval.f_left + 4.0 * interval.f_mid + interval.f_right);
    double q2 = h / 12.0 * (interval.f_left + 4.0 * fd + 2.0 * interval.f_mid + 4.0 * fe + interval.f_right);

    if ((fabs(q2 - q1) < interval.tol) || ((interval.right - interval.left) < 1.0e-12)) {
        // Tolerance is met, return
        return q2 + (q2 - q1) / 15.0;
    } else {
        // Tolerance is not met, split interval in two and make recursive calls
        struct Interval i1, i2;
        double quad1, quad2;

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

        // Spawn two subtasks per recursive function call, one for each interval

#pragma omp task default(none) shared(quad1, func) firstprivate(i1) 
        {
            quad1 = simpson(func, i1);
        }

#pragma omp task default(none) shared(quad2, func) firstprivate(i2)
        {
            quad2 = simpson(func, i2);
        }

        // Wait for both subtasks for the current recursive call to complete before
        // continuing.
#pragma omp taskwait
        return quad1 + quad2;
    }
}

int main(void)
{
    struct Interval whole;
    double quad = 0.0;

    double start = omp_get_wtime();

    // Create initial interval
    whole.left    = 0.0;
    whole.right   = 10.0;
    whole.tol     = 1e-06;
    whole.f_left  = func1(whole.left);
    whole.f_right = func1(whole.right);
    whole.f_mid   = func1((whole.left + whole.right) / 2.0);

    printf("Threads: %d\n", omp_get_max_threads());

    // Call recursive quadrature routine
#pragma omp parallel default(none) shared(quad, whole)
    {
#pragma omp single
        {
            quad = simpson(func1, whole);
        }
    }   

    printf("Result = %e\n", quad);
    printf("Time(s) = %f\n", omp_get_wtime() - start);
}
