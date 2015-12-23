#include <stdio.h>
#include <stdlib.h>
#include <mpc.h>
#include <math.h>

/* Macros */
#define min(a, b) ((a > b) ? b : a)
#define max(a, b) ((a < b) ? b : a)
#define streq(a, b) (strcmp(a, b) == 0)

/* *********** WINDOWS SHIT *********** */

/* If we are compiling on Windows compile these functions */
#ifdef _WIN32
#include <string.h>
static char buffer[2048];
/* Fake readline function */
char *readline(char *prompt)
{
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char *cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';
    return cpy;
}
/* Fake add_hisotry function */
void add_history(char *unused) {}

/* *********** / WINDOWS SHIT *********** */

/* Otherwise include the editline headers */
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif


/* struct to hold the result of an evaluation */
typedef struct lval {
    int type;
    long num;
    /* Error and symbol types have some string data */
    char *err;
    /* sym is a string identifying the lval (when it's a symbol) */
    char *sym;
    /* Count and pointer to a list of "lval*" */
    int count;
    /* Cell is a pointer to an array of cells */
    struct lval **cell;
} lval;

/***** Prototypes *****/
void lval_print(lval *v);
lval* lval_add(lval *v, lval *x);
lval* builtin_op(lval *a, char *op);
lval* lval_eval(lval *v);
lval* lval_take(lval *v, int i);
lval* lval_pop(lval *v, int i);
/**********************/

/* possible lval types
 * LVAL_ERR: an error
 * LVAL_NUM: a number
 * LVAL_SYM: a symbol (eg. operators, variables)
 * LVAL_SEXPR: a symbolic expression (S-Expression). A S-Expression is a
 *             variable length list of other values.
 *             (http://www.buildyourownlisp.com/chapter9_s_expressions)
 */
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR };

/* construct a pointer to a new Number lval */
lval* lval_num(long x)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* construct a pointer to a new Error lval */
lval* lval_err(char *m)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

/* construct a pointer to a new Symbol lval */
lval* lval_sym(char *s)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

/* a pointer to a new empty Sexpr lval */
lval* lval_sexpr(void)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* delete a lval and it's children (if it's a S-Expression) */
void lval_del(lval *v)
{
    switch (v->type) {
        /* Do nothing special for number type. */
        case LVAL_NUM:
            break;
        /* For Err or Sym free the string data. */
        case LVAL_ERR:
            free(v->err);
            break;
        case LVAL_SYM:
            free(v->sym);
            break;

        /* If Sexpr then delete all elements inside. */
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            /* Also free the memory allocated to contain the pointers. */
            free(v->cell);
            break;
    }
    /* Free the memory allocated for the "lval" struct itself. */
    free(v);
}

lval* lval_read_num(mpc_ast_t *t)
{
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t *t)
{
    /* If Symbol or Number return conversion to that type */
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    /* If root (>) or sexpr then create empty list */
    lval *x = NULL;
    if (streq(t->tag, ">") ||
        (strstr(t->tag, "sexpr"))) {
        x = lval_sexpr();
    }

    /* Fill this list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++) {
        if (streq(t->children[i]->contents, "(") ||
            streq(t->children[i]->contents, ")") ||
            streq(t->children[i]->contents, "}") ||
            streq(t->children[i]->contents, "{") ||
            streq(t->children[i]->tag, "regex")) {
            continue;
        }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

/* Given two lvals, "v" and "x", adds "x" to the list of children of "v" */
lval* lval_add(lval *v, lval *x)
{
    /* Increment the count of children */
    v->count++;
    /* Increment accordingly the space allocated for the cells */
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    /* Add the lval as the last children */
    v->cell[v->count - 1] = x;
    return v;
}

void print_error(char *msg)
{
    printf("Error: %s\n", msg);
}

/* Print a lvalue (with all it's children) between chars open and close
 * (usually '(' and ')')
 */
void lval_expr_print(lval *v, char open, char close)
{
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        /* Print Value contained within */
        lval_print(v->cell[i]);
        /* Don't print trailing space if last element */
        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

/* Handle different representations depending on the type of lval. */
void lval_print(lval *v)
{
    switch(v->type) {
        case LVAL_NUM:
            printf("%li", v->num);
            break;
        case LVAL_ERR:
            printf("Error: %s", v->err);
            break;
        case LVAL_SYM:
            printf("%s", v->sym);
            break;
        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
    }
}

/* Print a lval followed by a newline. */
void lval_println(lval *v)
{
    lval_print(v);
    putchar('\n');
}

/* Evaluate a S-Expression */
lval* lval_eval_sexpr(lval *v)
{
    /* Evaluate children */
    int i;
    for (i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    /* Error checking */
    for (i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) {
            return lval_take(v, i);
        }
    }

    /* Empty expression */
    if (v->count == 0) {
        return v;
    }

    /* Single expression */
    if (v->count == 1) {
        return lval_take(v, 0);
    }

    /* Ensure First Element is Symbol */
    lval *f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression does not start with  symbol!");
    }

    /* Call builtin with operator */
    lval *result = builtin_op(v, f->sym);
    lval_del(f);
    return result;
}

/* Returns the ith lval of sepxr "v", removing it from "v" */
lval* lval_pop(lval *v, int i)
{
    /* Find the item at "i". */
    lval *x = v->cell[i];

    /* Shift memory after the item at "i" over the top. */
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*)*(v->count - i - 1));

    /* Decrease the count of items in the list. */
    v->count--;

    /* Reallocate the memory used. */
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

/* Returns the ith lval of sexpr "v", destroying "v". */
lval* lval_take(lval *v, int i)
{
    lval *x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* lval_eval(lval *v)
{
    /* Evaluate S-Expressions */
    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(v);
    }
    /* All other lval types remain the same. */
    return v;
}

lval* builtin_op(lval *a, char *op)
{
    /* Ensure all arguments are numbers. */
    int i;
    for (i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on non-number!");
        }
    }

    /* Pop the first element. */
    lval *x = lval_pop(a, 0);

    /* If no arguments and sub then perform unary negation. */
    if (streq(op, "-") && a->count == 0) {
        x->num = - x->num;
    }

    /* While there are still elements remainin... */
    while (a->count > 0) {
        /* Pop the next element. */
        lval *y = lval_pop(a, 0);

        if (streq(op, "+")) { x->num += y->num; }
        if (streq(op, "-")) { x->num -= y->num; }
        if (streq(op, "*")) { x->num *= y->num; }
        if (streq(op, "/")) {
            if (y->num == 0) {
                lval_del(x);
                lval_del(y);
                x = lval_err("Division by zero!");
                break;
            }
            x->num /= y->num;
        }

        lval_del(y);
    }
    lval_del(a);
    return x;
}

int main(int argc, char *argv[])
{
    /* Create some parsers. */
    mpc_parser_t *Number  =     mpc_new("number");
    mpc_parser_t *Symbol  =     mpc_new("symbol");
    mpc_parser_t *Sexpr   =     mpc_new("sexpr");
    mpc_parser_t *Expr    =     mpc_new("expr");
    mpc_parser_t *Caballa =     mpc_new("caballa");

    /* Define them with the following language. */
    mpca_lang(MPCA_LANG_DEFAULT,
            "                                             \
            number      : /-?[0-9]+/ ;                    \
            symbol      : '+' | '-' | '*' | '/' | '%' ;   \
            sexpr       : '(' <expr>* ')' ;               \
            expr        : <number> | <symbol> | <sexpr> ; \
            caballa     : /^/ <expr>* /$/ ;               \
            ",
            Number, Symbol, Sexpr, Expr, Caballa);
    /* Print version and Exit information. */
    puts("Caballa Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    mpc_result_t r;
    lval *x;
    /* Main loop. */
    while (1) {
        /* Output our prompt and get input. */
        char *input = readline("caballa> ");

        /* Add input to history. */
        add_history(input);

        /* Attempt to parse the user input. */
        if (mpc_parse("<stdin>", input, Caballa, &r)) {
            /* On success print the result of evaluation. */
            x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);
        } else {
            /* Otherwise print the error. */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }
    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Caballa);
    return 0;
}
