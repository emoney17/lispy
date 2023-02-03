#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>
#include <string.h>
#include "mpc.h"

typedef struct lval
{
    int type;
    long num;
    // error and symbol types have string data
    char* err;
    char* sym;
    // count and pointer toa  list of "lval*"
    int count;
    struct lval** cell;
} lval;

// create special field type for errors etc.
// typedef struct
// {
//     int type;
//     long num;
//     int err;
// }lval;

// enumeration of possible lval types and error types
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

// construct a pointer toa  new number lval
lval* lval_num(long x)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

// construct a pointer to a new error lval
lval* lval_err(char* m)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1); // extra space for null term
    strcpy(v->err, m);
    return v;
}

// construct a pointer to a new symbol lval
lval* lval_sym(char* s)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1); // extra space for null term
    strcpy(v->sym, s);
    return v;
}

// a pointer to a new empty sexpr lval
lval* lval_sexpr(void)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

// special function to delete 'lval*'
void lval_del(lval* v)
{
    switch (v->type) {
        // do nothing for special number type
        case LVAL_NUM: break;
        // for err or sym, free the string data
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;
        // if sexpr then delete all of the elements inside
        case LVAL_SEXPR:
            for (int i  = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }

            // also free the memory allocated to contain the pointers
            free(v->cell);
        break;
    }

    // free the momry allocated for the "lval" construct itself
    free(v);
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
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    // define them with the following lang
    mpca_lang(MPCA_LANG_DEFAULT,
            " \
            number : /-?[0-9]+/ ; \
            symbol : '+' | '-' | '*' | '/' ; \
            sexpr : '(' <expr>* ')' ; \
            expr : <number> | <symbol> | <sexpr> ; \
            lispy : /^/ <operator> <expr>+ /$/ ; \
            ",
            Number, Symbol, Sexpr, Expr, Lispy);

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
    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);

    return 0;
}
