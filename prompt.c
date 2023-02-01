#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>

static char input[2048];

// repeatedly write message and take in input
int main(int argc, char *argv[])
{
    // print version and exit information
    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    while(1) {

        // output prompt and read input
        char* input = readline("lispy> ");

        // add input to add_history to record the input
        add_history(input);

        // echo input
        printf("%s\n", input);

        // free retreived input
        free(input);
    }

    return 0;
}
