/*
 * Mu Module - Math and Statistics
 * Part of Cipher-Core Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>


#define MU_VERSION "1.0.0"

static void mu_init(void) {
    printf("Mu math and statistics module initialized v%s\n", MU_VERSION);
}

static double mu_mean(double *data, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += data[i];
    }
    return sum / n;
}

static double mu_stddev(double *data, int n) {
    double mean = mu_mean(data, n);
    double sum_sq = 0.0;
    for (int i = 0; i < n; i++) {
        double diff = data[i] - mean;
        sum_sq += diff * diff;
    }
    return sqrt(sum_sq / n);
}
