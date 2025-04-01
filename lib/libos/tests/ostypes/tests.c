#include "tests.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

int tests_run(int argc, char* argv[], Test tests[]) {
    int pass_count = 0;
    int total_tests = 0;

    if(argc > 1) {
        for(int i=1; i < argc; i++) {
            bool found = false;

            for(Test *t=&tests[0]; t->name; t++) {
                if(strcmp(t->name, argv[i]) == 0) {
                    found = true;
                    break;
                }
            }
            
            if(!found) {
                fprintf(stderr, "Test %s is not a valid test function.\n", argv[i]);
                return 1;
            }
        }
    }

    for(Test *t=&tests[0]; t->name; t++) {
        if(argc > 1) {
            bool found = false;

            for(int i=1; i < argc; i++) {
                if(strcmp(argv[i], t->name) == 0) {
                    found = true;
                    break;
                }
            }

            if(!found) {
                continue;
            }
        }

        t->result = t->test();

        printf("%s: ", t->name);

        if(t->result == 0) {
            pass_count += 1;
            printf("\033[1;92mOK\033[0;37m\n");
        } else {
            printf("\033[1;91mFAIL\033[0;37m\n");
        }

        total_tests++;
    }

    printf("%d/%d tests passed.\n", pass_count, total_tests);

    return pass_count;
}