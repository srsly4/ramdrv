/*
 * helloworld user program
 *
 * Author: Dillon Hicks (hhicks@ittc.ku.edu)
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#define __USE_GNU
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <linux/unistd.h>
#include <time.h>
#include <string.h>
#include <linux/ramdrv.h>
#include <ramdrv.h>

#include "hwuser.h"

/* User cmd line parameters */
struct user_params Params = {
        .increment = 1,
        .pprint = 0
};


/**
 *  This subroutine processes the command line options 
 */
void process_options (int argc, char *argv[])
{

    int error = 0;
    int c;
    for (;;) {
        int option_index = 0;

        static struct option long_options[] = {
                {"inc",              required_argument, NULL, 'i'},
                {"pprint",           no_argument,       NULL, 'p'},
                {"help",             no_argument,       NULL, 'h'},
                {NULL, 0, NULL, 0}
        };

        /**
         * c contains the last in the lists above corresponding to
         * the long argument the user used.
         */
        c = getopt_long(argc, argv, "n:p", long_options, &option_index);

        if (c == -1)
            break;
        switch (c) {
            case 0:
                switch (option_index) {
                    case 0:
//				printf(help_string, argv[0]);
                        exit(EXIT_SUCCESS);
                        break;
                }
                break;

            case 'i':
                Params.increment = atoi(optarg);
                break;

            case 'h':
                error = 1;
                break;

        }
    }

    if (error) {

        exit(EXIT_FAILURE);
    }
}


int main(int argc, char** argv){

    int fd;
    int i;

    process_options(argc, argv);

    fd = ramdrv_open();


    if (fd < 0) {
        printf("There was an error opening Helloworld.\n");
        exit(EXIT_FAILURE);
    }

    printf("INCREMENT=%d\n", Params.increment);

    for (i = 0; i < Params.increment; i++) {

        if (ramdrv_inc(fd)) {
            printf("There was an error calling ramdrv_inc().\n");
            exit(EXIT_FAILURE);
        }

    }


    if (ramdrv_close(fd)) {
        printf("There was an error closing Ramdrv.\n");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}