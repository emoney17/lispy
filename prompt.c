#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>
#include "mpc.h"

static char input[2048];

// repeatedly write message and take in input
int main(int argc, char *argv[])
{
    // create some parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    // define them with the following lang
    mpca_lang(MPCA_LANG_DEFAULT,
            " \
            number : /-?[0-9]+/ ; \
            operator : '+' | '-' | '*' | '/' ; \
            expr : <number> | '(' <operator> <expr>+ ')' ; \
            lispy : /^/ <operator> <expr>+ /$/ ; \
            ",
            Number, Operator, Expr, Lispy);

    // print version and exit information
    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    while(1) {

        // output prompt and read input
        char* input = readline("lispy> ");

        // add input to add_history to record the input
        add_history(input);

        // attempt to parse input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            // on success print the ast
            mpc_ast_print(r.output);
            mpc_ast_delete(r.output);
        } else {
            // otherwise print an error
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        // free retreived input
        free(input);
    }

    // clean up code
    mpc_cleanup(4, Number, Operator, Expr, Lispy);

    return 0;
}
