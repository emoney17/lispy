#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>
#include "mpc.h"

static char input[2048];

// create special field type for errors etc.
typedef struct
{
    int type;
    long num;
    int err;
}lval;

// enumeration of possuble lval types and error types
enum { LVAL_NUM, LVAL_ERR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM};

// create a new number type lval
lval lval_num(long x)
{
    lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

// create a new error type lval
lval lval_err(int x)
{
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

// use operator string to see which operation to perform
lval eval_op(lval x, char* op, lval y)
{
    // if either value is an error return it
    if (x.type == LVAL_ERR) return x;
    if (y.type == LVAL_ERR) return y;

    // otherwise do the math on the values
    if (strcmp(op, "+") == 0) return lval_num(x.num + y.num);
    if (strcmp(op, "-") == 0) return lval_num(x.num - y.num);
    if (strcmp(op, "*") == 0) return lval_num(x.num * y.num);
    if (strcmp(op, "/") == 0) {
        // if the second operand is zero return an error
        return y.num == 0
            ? lval_err(LERR_DIV_ZERO)
            : lval_num(x.num / y.num);
    }
    return lval_err(LERR_BAD_OP);
}

lval eval (mpc_ast_t* t)
{
    // if taged as a number, return it directly
    if (strstr(t->tag, "number")) {
        // check if there is an error in conversion
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }

    // the operator is always the second child
    char* op = t->children[1]->contents;

    // store the third child in x
    lval x = eval(t->children[2]);

    // iterate the remaining childrena nd combining
    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}

// print an lval
void lval_print(lval v)
{
    switch (v.type) {
        // if type is a number print it
        case LVAL_NUM: printf("%li\n", v.num);
        // if type is an error
        case LVAL_ERR:
            if (v.err == LERR_DIV_ZERO) printf("Error: Division by zero");
            if (v.err == LERR_BAD_OP) printf("Error: Invalid operator");
            if (v.err == LERR_BAD_NUM) printf("Error: Invalid number");
            break;
    }
}

// print lval followed by a new line
void lval_println(lval v)
{
    lval_print(v);
    putchar('\n');
}

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
            // on success print the abstract syntax tree
            // mpc_ast_print(r.output);
            // mpc_ast_delete(r.output);

            // print out the evaluation
            lval result = eval(r.output);
            lval_println(result);
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
