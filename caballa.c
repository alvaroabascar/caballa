#include <stdio.h>
#include <stdlib.h>
#include <mpc.h>
#include <math.h>

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

/* Macros */
#define min(a, b) ((a > b) ? b : a)
#define max(a, b) ((a < b) ? b : a)

/* struct to hold the result of an evaluation */
typedef struct lval {
    int type;
    long num;
    /* Error and symbol types have some string data */
    char *err;
    char *sym;
    /* Count and pointer to a list of "lval*" */
    int count;
    struct lval **cell;
} lval;

/* Prototypes */
void lval_print(lval *v);
lval* lval_add(lval *v, lval *x);


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
    if ((strcmp(t->tag, ">") == 0) ||
        (strstr(t->tag, "sexpr"))) {
        x = lval_sexpr();
    }

    /* Fill this list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++) {
        if ((strcmp(t->children[i]->contents, "(") == 0) ||
            (strcmp(t->children[i]->contents, ")") == 0) ||
            (strcmp(t->children[i]->contents, "}") == 0) ||
            (strcmp(t->children[i]->contents, "{") == 0) ||
            (strcmp(t->children[i]->tag, "regex") == 0)) {
            x = lval_add(x, lval_read(t->children[i]));
        }
    }
    return x;
}

lval* lval_add(lval *v, lval *x)
{
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return v;
}



void print_error(char *msg)
{
    printf("Error: %s\n", msg);
}

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


void lval_print(lval *v)
{
    switch(v->type) {
        case LVAL_NUM:
            printf("%li\n", v->num);
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

/* print a lval followed by a newline */
void lval_println(lval *v)
{
    lval_print(v);
    putchar('\n');
}

/* Use symbol string to see which operation to perform, perform it. */
lval *eval_op(lval *x, char *op, lval *y)
{
    /* if either value is an error, return it */
    if (x->type == LVAL_ERR) { return lval_err(x->err); }
    if (y->type == LVAL_ERR) { return lval_err(y->err); }

    /* otherwise do maths on the numbers, create and return a lval of type number */
    if (strcmp(op, "+") == 0) { return lval_num(x->num + y->num); }
    if (strcmp(op, "-") == 0) { return lval_num(x->num - y->num); }
    if (strcmp(op, "*") == 0) { return lval_num(x->num * y->num); }
    if (strcmp(op, "%") == 0) { return lval_num(x->num % y->num); }
    if (strcmp(op, "/") == 0) {
        return (y->num == 0) ? lval_err("division by zero") : lval_num(x->num / y->num);
    }
    
    return lval_err("invalid number");
}

/* Evaluate the Abstract Syntax Tree */
lval* eval(mpc_ast_t *t)
{
    /* If it is a number return it */
    if (strstr(t->tag, "number")) {
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x): lval_err("invalid number");
    }

    /* The symbol is always second child (first is '(') */
    char *op = t->children[1]->contents;

    /* We store the third child in x */
    lval *x = eval(t->children[2]);

    /* Iterate the remaining children and combining. */
    int i;
    for (i=3; strstr(t->children[i]->tag, "expr"); i++)
        x = eval_op(x, op, eval(t->children[i]));

    return x;
}


int main(int argc, char *argv[])
{
    /* Create some parsers */
    mpc_parser_t *Number  =     mpc_new("number");
    mpc_parser_t *Symbol  =     mpc_new("symbol");
    mpc_parser_t *Expr    =     mpc_new("expr");
    mpc_parser_t *Sexpr   =     mpc_new("sexpr");
    mpc_parser_t *Caballa =     mpc_new("caballa");

    /* Define them with the following language */
    mpca_lang(MPCA_LANG_DEFAULT,
            "                                           \
            number      :   /-?[0-9]+/ ;                \
            symbol      : '+' | '-' | '*' | '/' | '%' ; \
            sexpr       : '(' <expr>* ')' ;             \
            expr        : <number> | <symbol> <sexpr> ; \
            caballa     :  /^/ <expr>* /$/ ;            \
            ",
            Number, Symbol, Sexpr, Expr, Caballa);
    /* Print version and Exit information */
    puts("Caballa Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    mpc_result_t r;
    lval *x;
    /* Main loop */
    while (1) {
        /* Output our prompt and get input */
        char *input = readline("caballa> ");

        /* Add input to history */
        add_history(input);

        /* Attempt to parse the user input */
        if (mpc_parse("<stdin>", input, Caballa, &r)) {
            /* On success print the result of evaluation */
            x = lval_read(r.output);
            lval_println(x);
            lval_del(x);
        } else {
            /* Otherwise print the error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }
    puts("bye!");
    mpc_cleanup(4, Number, Symbol, Sexpr, Expr, Caballa);
    return 0;
}
