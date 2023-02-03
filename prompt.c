#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>
#include <string.h>
#include "mpc.h"

static char buffer[2048];

// add sym and sexpr as possible lval types
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR };

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

lval* lval_add(lval* v, lval* x)
{
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

lval* lval_pop(lval* v, int i)
{
    // find the iotem at i
    lval* x = v->cell[i];

    // shift memory after the item at i over the top
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));

    // decrease the count of items in th list
    v->count--;

    // reallocate the memory used
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval* lval_take(lval* v, int i)
{
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close)
{
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        // print value contained within
        lval_print(v->cell[i]);

        // dont print trailing space if its the last element
        if (i != (v->count-1)) {
            putchar(' ');
        }
    }

    putchar(close);
}

void lval_print(lval* v)
{
    switch (v->type) {
        case LVAL_NUM: printf("%li", v->num); break;
        case LVAL_ERR: printf("Error: %s", v->err); break;
        case LVAL_SYM: printf("%s", v->sym); break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    }
}

void lval_println(lval* v)
{
    lval_print(v);
    putchar('\n');
}

lval* builtin_op(lval* a, char* op)
{
    // ensure all the arguments are numbers
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on non-number");
        }
    }

    // pop the first element
    lval* x = lval_pop(a, 0);

    // if no arguments and sub then perform unary negation'
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    // while there are still elements remaining
    while (a->count > 0) {
        // pop the next element
        lval* y = lval_pop(a, 0);

        if (strcmp(op, "+") == 0) x->num += y->num;
        if (strcmp(op, "-") == 0) x->num -= y->num;
        if (strcmp(op, "*") == 0) x->num *= y->num;
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                x = lval_err("Division by zero"); break;
            }
            x->num /= y->num;
        }

        lval_del(y);
    }

    lval_del(a);
    return x;
}

lval* lval_eval(lval* v);

lval* lval_eval_sexpr(lval* v)
{
    // eval children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    // error checking
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) return lval_take(v, i);
    }

    // empty expression
    if (v->count == 0) return v;

    // single expression
    if (v->count == 1) return lval_take(v, 0);

    // ensure first element is a symbol
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression does not start with a symbol");
    }

    // call builtin with operator
    lval* result = builtin_op(v, f->sym);
    lval_del(f);
    return result;
}

lval* lval_eval(lval* v)
{
    // evaluate s expressions
    if (v->type == LVAL_SEXPR) return lval_eval_sexpr(v);
    // all other lval types remain the same
    return v;
}

lval* lval_read_num(mpc_ast_t* t)
{
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ?
        lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t)
{
    // if synbol or number return conversion to that type
    if (strstr(t->tag, "number")) return lval_read_num(t);
    if (strstr(t->tag, "symbol")) return lval_sym(t->contents);

    // if root (>) or sexpr then create empty list
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) x = lval_sexpr();
    if (strcmp(t->tag, "sexpr")) x = lval_sexpr();

    // fill this list with any valid expression contained within
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) continue;
        if (strcmp(t->children[i]->contents, ")") == 0) continue;
        if (strcmp(t->children[i]->tag, "regex") == 0) continue;
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
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
            // lval result = eval(r.output);
            // lval_println(result);
            // mpc_ast_delete(r.output);

            lval* x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);
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
