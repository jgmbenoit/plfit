/* plgen.c
 *
 * Copyright (C) 2012 Tamas Nepusz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "getopt.h"
#include "plfit.h"
#include "sampling.h"

typedef struct _cmd_options_t {
    long int num_samples;
    double gamma;
    double kappa;
    double offset;
    plfit_bool_t continuous;
} cmd_options_t;

cmd_options_t opts;

void show_version(FILE* f) {
    fprintf(f, "plgen " PLFIT_VERSION_STRING "\n");
    return;
}

void usage(char* argv[]) {
    show_version(stderr);

    fprintf(stderr, "\nUsage: %s [options] num_samples gamma [kappa]\n\n", argv[0]);
    fprintf(stderr,
            "Generates a given number of samples from a power-law distribution\n"
            "with an optional exponential cutoff. The pdf being sampled is given\n"
            "as follows:\n\n"
            "P(k) = C * k^(-gamma) * exp(-k/kappa)\n\n"
            "where C is an appropriate normalization constant. gamma is given by\n"
            "the second command line argument, kappa is given by the -k switch.\n\n"
            "Options:\n"
            "    -h         shows this help message\n"
            "    -v         shows version information\n"
            "    -c         generate continuous samples\n"
            "    -k KAPPA   use exponential cutoff with kappa = KAPPA\n"
            "    -o OFFSET  add OFFSET to each generated sample\n"
    );
    return;
}

int parse_cmd_options(int argc, char* argv[], cmd_options_t* opts) {
    int c;

    opts->offset = 0;
    opts->continuous = 0;
    opts->kappa = -1;

    opterr = 0;

    while ((c = getopt(argc, argv, "chk:o:v")) != -1) {
        switch (c) {
            case 'c':           /* force continuous samples */
                opts->continuous = 1;
                break;

            case 'h':           /* shows help */
                usage(argv);
                return 0;

            case 'k':           /* use exponential cutoff */
                if (!sscanf(optarg, "%lg", &opts->kappa)) {
                    fprintf(stderr, "Invalid value for option `-%c'\n", optopt);
                    return 1;
                }
                break;

            case 'o':           /* specify offset explicitly */
                if (!sscanf(optarg, "%lg", &opts->offset)) {
                    fprintf(stderr, "Invalid value for option `-%c'\n", optopt);
                    return 1;
                }
                break;

            case 'v':           /* version information */
                show_version(stdout);
                return 0;

            case '?':           /* unknown option */
                if (optopt == 'o')
                    fprintf(stderr, "Option `-%c' requires an argument\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Invalid option `-%c'\n", optopt);
                else
                    fprintf(stderr, "Invalid option character `\\x%x'.\n", optopt);
                return 1;

            default:
                abort();
        }
    }

    return -1;
}

int sample() {
    const long int BLOCK_SIZE = 16384;
    long int samples[BLOCK_SIZE];
    double* probs;
    size_t num_probs;
    plfit_walker_alias_sampler_t sampler;
    long int i, n;

    if (opts.num_samples <= 0)
        return 0;

    /* Construct probability array */
    num_probs = 100000;
    probs = (double*)calloc(num_probs, sizeof(double));
    if (probs == 0) {
        fprintf(stderr, "Not enough memory\n");
        return 7;
    }

    probs[0] = 0;
    if (opts.kappa == 0) {
        fprintf(stderr, "kappa may not be zero\n");
        return 8;
    } else if (opts.kappa > 0) {
        /* Power law with exponential cutoff */
        for (i = 1; i < num_probs; i++) {
            probs[i] = exp(-i / opts.kappa) * pow(i, -opts.gamma);
        }
    } else {
        /* Pure power law */
        for (i = 1; i < num_probs; i++) {
            probs[i] = pow(i, -opts.gamma);
        }
    }

    /* Initialize sampler */
    if (plfit_walker_alias_sampler_init(&sampler, probs, num_probs)) {
        fprintf(stderr, "Error while initializing sampler\n");
        free(probs);
        return 9;
    }

    /* Free "probs" array */
    free(probs);

    /* Sampling */
    while (opts.num_samples > 0) {
        n = opts.num_samples > BLOCK_SIZE ? BLOCK_SIZE : opts.num_samples;
        plfit_walker_alias_sampler_sample(&sampler, samples, n);

        if (opts.continuous) {
            fprintf(stderr, "Continuous sampling not implemented yet, sorry.\n");
            return 5;
        } else {
            for (i = 0; i < n; i++) {
                printf("%ld\n", (long int)(samples[i] + opts.offset));
            }
        }

        opts.num_samples -= n;
    }

    /* Destroy sampler */
    plfit_walker_alias_sampler_destroy(&sampler);

    return 0;
}

int main(int argc, char* argv[]) {
    int result = parse_cmd_options(argc, argv, &opts);
    int retval;

    if (result != -1)
        return result;

    retval = 0;
    if (argc - optind < 2) {
        /* not enough arguments */
        usage(argv);
        retval = 2;
    } else {
        if (!sscanf(argv[optind], "%ld", &opts.num_samples)) {
            fprintf(stderr, "Format of num_samples parameter is invalid.\n");
            retval = 3;
        } else if (!sscanf(argv[optind+1], "%lg", &opts.gamma)) {
            fprintf(stderr, "Format of gamma parameter is invalid.\n");
            retval = 4;
        } else {
            retval = sample();
        }
    }

    return retval;
}