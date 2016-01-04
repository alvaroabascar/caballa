#include <stdio.h>
#include <stdlib.h>
#include <mpc.h>
#include <math.h>

/* Macros */
#define min(a, b) ((a > b) ? b : a)
#define max(a, b) ((a < b) ? b : a)
#define STREQ(a, b) (strcmp(a, b) == 0)
#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { \
        lval* err = lval_err(fmt, ##__VA_ARGS__); \
        lval_del(args); \
        return err; \
    }

#define LASSERT_TYPE(del, got, expected, num, fn) \
    LASSERT(del, got->type == expected, \
            "Function '%s' passed incorrect type for argument %d. " \
            "Expected %s, but got %s.", \
            fn, num, ltype_name(expected), ltype_name(got->type))

#define LASSERT_NARGS(del, got, expected, fn) \
    LASSERT(del, got == expected, \
            "Function '%s' passed too %s arguments. " \
            "Expected %d, but got %d.", \
            fn, (got < expected ? "few" : "many"), expected, got)

#define LASSERT_NARGS_RANGE(del, got, min, max, fn) \
    LASSERT(del, (got >= min && got <= max), \
            "Function '%s' passed too %s arguments. " \
            "Expected from %d to %d, but got %d.", \
            fn, (got < min ? "few" : "many"), min, max, got)


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

/* *********** END WINDOWS SHIT *********** */

/* Otherwise include the editline headers */
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

/* Forward declarations */
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;
/* lbuiltin is a pointer to a function which takes an environment (lenv)
 * and a lvalue (lval) and returns a lval.
 */
typedef lval*(*lbuiltin)(lenv *, lval *);

/* possible lval types
 * LVAL_ERR: an error
 * LVAL_NUM: a number
 * LVAL_SYM: a symbol (eg. operators, variables)
 * LVAL_SEXPR: a symbolic expression (S-Expression). A S-Expression is a
 *             variable length list of other values.
 *             (http://www.buildyourownlisp.com/chapter9_s_expressions)
 * LVAL_QEXPR: a quoted expression (Q-Expression) = a "literal list".
 * LVAL_FUN: a function.
 */
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_FUN, LVAL_DEF };
/*         0         1         2          3          4          5         6     */

/* Struct to hold the result of an evaluation. */
struct lval {
    int type;

    /* Basic types */
    long num;
    char *err;
    char *sym;

    /* Function */
    lbuiltin builtin_fun;
    lenv *env;
    lval *formals;
    lval *body;

    /* Expression */
    int count;
    /* Cell is a pointer to an array of lvals (the children) */
    struct lval **cell;
};

/* Struct to represent an environment (set of symbols and associated values.) */
struct lenv {
    int count;
    char **syms;
    lval **vals;
    lenv *parent;
};

/***** Prototypes *****/
void lval_print(lval *v);
lval *lval_add(lval *v, lval *x);
lval *builtin_eval(lenv *e, lval *a);
lval *builtin_op(lenv *e, lval *a, char *op);
lval *builtin_list(lenv *e, lval *a);
lval *builtin_head(lenv *e, lval *a);
lval *builtin_tail(lenv *e, lval *a);
lval *builtin_join(lenv *e, lval *a);
lval *builtin_exit(lenv *e, lval *a);
lval *lval_join(lenv *e, lval *x, lval *y);
lval *lval_eval(lenv *e, lval *v);
lval *lval_take(lval *v, int i);
lval *lval_pop(lval *v, int i);
void lval_del(lval *v);
lval *lval_copy(lval *v);
lval *lenv_get(lenv *e, lval *v);

lenv *lenv_new(void);
void lenv_del(lenv *v);
void lenv_put(lenv *e, lval *sym, lval *val);
lenv *lenv_copy(lenv *v);
/**********************/

char *ltype_name(int t)
{
    switch(t) {
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

/******** Functions to create different types of lvals. *********/

/* Construct a pointer to a new Number lval. */
lval* lval_num(long x)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* Construct a pointer to a new Error lval. */
lval* lval_err(char *fmt, ...)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    /* Create and initialize va list. */
    va_list va;
    va_start(va, fmt);

    /* Allocate 2048 bytes of space (max size of error). */
    v->err = malloc(2048);
    /* Fill error with the correct string. */
    vsnprintf(v->err, 2047, fmt, va);

    /* Reallocate to number of bytes actually used. */
    v->err = realloc(v->err, strlen(v->err) + 1);

    /* Destroy va_list and return. */
    va_end(va);
    return v;
}

/* Construct a pointer to a new Symbol lval. */
lval* lval_sym(char *s)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

/* A pointer to a new empty Sexpr lval. */
lval* lval_sexpr(void)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* A pointer to a new empty Qexpr lval. */
lval *lval_qexpr(void)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* A pointer to a lval which contains a ptr to a builtin function. */
lval *lval_fun(lbuiltin func)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->builtin_fun = func;
    return v;
}

/* A pointer to a lval containing a user defined function.
 * formals: a q-expression containing the symbols of the parameters.
 * body: a q-expression containing the function body.
 */
lval *lval_lambda(lval *formals, lval *body)
{
   lval *v = malloc(sizeof(lval));
   v->type = LVAL_FUN;

   /* builtin_fun = null indicates that this is a user defined function, and not a
    * builtin function. */
   v->builtin_fun = NULL;

   v->env = lenv_new();

   /* Set formalas and body. */
   v->formals = formals;
   v->body = body;
   return v;
}

/*****************************************************************/
/****************** Functions to handle lvals. ******************/

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
        /* If Qexpr or Sexpr then delete all elements inside. */
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            /* Also free the memory allocated to contain the pointers. */
            free(v->cell);
            break;
        case LVAL_FUN:
            if (! v->builtin_fun) {
                lenv_del(v->env);
                lval_del(v->formals);
                lval_del(v->body);
            }
            break;
    }
    /* Free the memory allocated for the "lval" struct itself. */
    free(v);
}

/* Return pointer to a copy of the lval pointed to by v. */
lval *lval_copy(lval *v)
{
    int i;
    lval *x = malloc(sizeof(lval));
    x->type = v->type;

    switch(v->type) {
        /* Copy functions and numbers directly. */
        case LVAL_FUN:
            x->builtin_fun = v->builtin_fun;
            if (! x->builtin_fun) {
                x->env = lenv_copy(v->env);
                x->formals = lval_copy(v->formals);
                x->body = lval_copy(v->body);
            }
            break;

        case LVAL_NUM:
            x->num = v->num;
            break;

        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;

        /* Copy strings using malloc and strcpy. */
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;

        /* Copy lists by copying each sub-expression. */
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval *) * x->count);
            for (i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }

    return x;
}

/* Given two lvals, "v" and "x", adds "x" to the list of children of "v".
 * Returns v. */
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

/* Join two lvalues. */
lval* lval_join(lenv *e, lval *x, lval *y)
{
    while (y->count) {
        lval_add(x, lval_pop(y, 0));
    }

    /* Delete the empty 'y' and return 'x'. */
    lval_del(y);
    return x;
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
    if (STREQ(t->tag, ">") ||
        strstr(t->tag, "sexpr")) {
        x = lval_sexpr();
    }

    if (strstr(t->tag, "qexpr")) {
        x = lval_qexpr();
    }

    /* Fill this list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++) {
        if (STREQ(t->children[i]->contents, "(") ||
            STREQ(t->children[i]->contents, ")") ||
            STREQ(t->children[i]->contents, "}") ||
            STREQ(t->children[i]->contents, "{") ||
            STREQ(t->children[i]->tag, "regex")) {
            continue;
        }
        lval_add(x, lval_read(t->children[i]));
    }
    return x;
}

lval *lval_call(lenv *e, lval *f, lval *v)
{
    int given, total;
    /* If builtin then simply call that. */
    if (f->builtin_fun) {
        return f->builtin_fun(e, v);
    }
    /* Record argument counts. */
    given = v->count;
    total = f->formals->count;

    /* While there are still arguments to be processed... */
    while (v->count) {
        /* If we've ran out of formal arguments to bind... */
        if (f->formals->count == 0) {
            lval_del(v);
            return lval_err("Function passed too many arguments. "
                            "Expected %d, but got %d.", total, given);
        }

        /* Pop the first symbol from the formals. */
        lval *sym = lval_pop(f->formals, 0);

        /* Special case to deal with '&' */
        if (STREQ(sym->sym, "&")) {

            /* Ensure '&' is followed by another symbol */
            if (f->formals->count != 1) {
                lval_del(v);
                return lval_err("Function format is invalid. "
                                "Symbol '&' not followed by single symbol.");
            }

            /* Next formal should be bound to remaining arguments. */
            lval *nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, builtin_list(e, v));
            lval_del(sym);
            lval_del(nsym);
            break;
        }


        /* Pop the next argument from the list. */
        lval *val = lval_pop(v, 0);

        /* Bind a copy into the function's environment. */
        lenv_put(f->env, sym, val);

        /* Delete symbol and value. */
        lval_del(sym);
        lval_del(val);

    }
    /* Argument list is now bound so can be cleaned up. */
    lval_del(v);

    /* If '&' remains in formal list bind to empty list. */
    if (f->formals->count > 0 && STREQ(f->formals->cell[0]->sym, "&")) {
        /* Check to ensure that '&' is not passed invalidly. */
        if (f->formals->count != 2) {
            return lval_err("Function format is invalid. "
                            "Symbol '&' not followed by single symbol.");
        }

        /* Pop and delete '&' symbol. */
        lval_del(lval_pop(f->formals, 0));

        /* Pop next symbol and create empty list. */
        lval *sym = lval_pop(f->formals, 0);
        lval *val = lval_qexpr();

        /* Bind to environment and delete. */
        lenv_put(f->env, sym, val);
        lval_del(sym);
        lval_del(val);
    }


    /* If all formals have been bound evaluate. */
    if (f->formals->count == 0) {
        /* Set environment parent to evaluation environment. */
        f->env->parent = e;

        /* Evaluate and return. */
        return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
    } else {
        /* Otherwise return partially evaluated function. */
        return lval_copy(f);
    }
}

int lval_eq(lval *a, lval *b)
{
    /* If type is different, they are different. */
    if (a->type != b->type) {
        return 0;
    }

    switch(a->type) {

    case LVAL_NUM:
        return a->num == b->num;
    case LVAL_SYM:
        return STREQ(a->sym, b->sym);
    case LVAL_ERR:
        return STREQ(a->err, b->err);
    case LVAL_SEXPR:
    case LVAL_QEXPR:
        if (a->count != b->count) {
            return 0;
        }
        for (int i = 0; i < a->count; i++) {
            if (! lval_eq(a->cell[i], b->cell[i])) {
                return 0;
            }
        }
        return 1;
    case LVAL_FUN:
        /* If any is a builtin (ptr to a function), then
         * we can compare these pointers. */
        if (a->builtin_fun || b->builtin_fun) {
            return a->builtin_fun == b->builtin_fun;
        } else {
            return lval_eq(a->formals, b->formals) &&
                lval_eq(a->body, b->body);
        }
    }
    return 0;
}

/*****************************************************************/
/******************* Functions to print lvals ********************/
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
        case LVAL_QEXPR:
            lval_expr_print(v, '{', '}');
            break;
        case LVAL_FUN:
            if (v->builtin_fun) {
                printf("<function>");
            } else {
                printf("(\\ ");
                lval_print(v->formals);
                putchar(' ');
                lval_print(v->body);
                putchar(')');
            }
            break;
    }
}

/* Print a lval followed by a newline. */
void lval_println(lval *v)
{
    lval_print(v);
    putchar('\n');
}
/*****************************************************************/
/****************** Functions for evaluation. ********************/

/* Evaluate a S-Expression */
lval* lval_eval_sexpr(lenv *e, lval *v)
{
    /* Evaluate children. */
    int i;
    for (i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    /* Error checking */
    for (i = 0; i < v->count; i++) {
        /* If any children is an error, return it and destroy "v". */
        if (v->cell[i]->type == LVAL_ERR) {
            return lval_take(v, i);
        }
    }

    /* Empty expression */
    if (v->count == 0) {
        return v;
    }

    /* Single expression: unless it's a function,
     * return first children, destroy sexpr. */
    if (v->count == 1) {
        return lval_take(v, 0);
    }

    /* Ensure first element is a function after evaluation. */
    lval *f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval *err = lval_err("S-Expression starts with incorrect type. "
                        "Expected %s, but got %s.",
                        ltype_name(f->type), ltype_name(LVAL_FUN));
        lval_del(f);
        return err;
    }

    /* Call function to get result. */
    lval *result = lval_call(e, f, v);
    lval_del(f);
    return result;
}

lval* lval_eval(lenv *e, lval *v)
{
    /* Evaluate S-Expressions */
    if (v->type == LVAL_SYM) {
        lval *x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(e, v);
    }
    /* All other lval types remain the same. */
    return v;
}

/* Evaluate a S-Expression. */
lval *builtin_eval(lenv *e, lval *a)
{
    LASSERT_NARGS(a, a->count, 1, "eval");
    LASSERT_TYPE(a, a->cell[0], LVAL_QEXPR, 0, "eval");

    lval *x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}



/*****************************************************************/


/************** Functions to handle environments. ****************/
/* Create an environment:
 * count = number of variables.
 * syms = symbols
 * vals = values, in order.
 */
lenv *lenv_new(void)
{
    lenv *e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    e->parent = NULL;
    return e;
}

/* Remove an environment with all its content. */
void lenv_del(lenv *e)
{
    int i;
    for (i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

/* Copy an environment. */
lenv *lenv_copy(lenv *e)
{
    int i;
    lenv *n = malloc(sizeof(lenv));
    n->parent = e->parent;
    n->count = e->count;
    n->syms = malloc(sizeof(char *) * n->count);
    n->vals = malloc(sizeof(lval *) * n->count);
    for (i = 0; i < e->count; i++) {
        n->syms[i] = malloc(sizeof(char) * (strlen(e->syms[i]) + 1));
        strcpy(n->syms[i], e->syms[i]);
        n->vals[i] = lval_copy(e->vals[i]);
    }
    return n;
}

/* Get a lval from environment. */
lval *lenv_get(lenv *e, lval *k)
{
    /* Iterate over all items in environment. */
    int i;
    for (i = 0; i < e->count; i++) {
        /* Check if the stored string matches the symbol string. */
        if (STREQ(e->syms[i], k->sym)) {
            return lval_copy(e->vals[i]);
        }
    }
    /* If no symbol found try in parent, else return error. */
    if (e->parent) {
        return lenv_get(e->parent, k);
    }
    return lval_err("unbound symbol: '%s'", k->sym);
}

/* Put a lval inside an environment:
 * e -> environment
 * k -> symbol
 * v-> value
 */
void lenv_put(lenv *e, lval *k, lval *v)
{
    /* Iterate over all items in environment. */
    int i;
    for (i = 0; i < e->count; i++) {
        /* If variable is found delete item at that position. */
        if (STREQ(e->syms[i], k->sym)) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    /* If no existing entry found allocate space for new entry. */
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval *) * e->count);
    e->syms = realloc(e->syms, sizeof(char *) * e->count);

    /* Copy contents of lval and symbol string into new location. */
    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count-1], k->sym);
}

/* Define a variable in the global environment. */
void lenv_def(lenv *e, lval *sym, lval *v)
{
    /* Iterate until reaching global scope. */
    while (e->parent) {
        e = e->parent;
    }
    lenv_put(e, sym, v);
}

/*****************************************************************/

/******************** Builtin functions. *************************/

/* Print all the variables in the environment. */
lval *builtin_getenv(lenv *e, lval *v)
{
    int i;
    for (i = 0; i < e->count; i++) {
        printf("(\"%s\" . ", e->syms[i]);
        lval_print(e->vals[i]);
        printf("\")\n");
    }
    return lval_sexpr();
}


/* Given an environment, a symbol (or set of symbols) inside a Q-Expression,
 * and the same name of values, assign each value to each symbol in order inside the
 * provided environment.
 */
lval *builtin_var(lenv *e, lval *a, char *op)
{
    int i;
    lval *sym, *val;
    LASSERT(a, (a->count >= 2),
            "'def' must have at least two arguments: a quoted expression"
            " and an expression");

    LASSERT_TYPE(a, a->cell[0], LVAL_QEXPR, 0, "def");
    LASSERT(a, (a->cell[0]->count > 0),
            "first argument of 'def' cannot be the empty Q-Expression {}");
    LASSERT(a, (a->cell[0]->count == a->count - 1),
            "number of symbols in Q-Expression must match the number of values");
    for (i = 0; i < a->cell[0]->count; i++) {
        LASSERT_TYPE(a, a->cell[0]->cell[i], LVAL_SYM, i, "def");
    }
    /* Add all the symbols to the environment. */
    while (a->count > 1) {
        sym = lval_pop(a->cell[0], 0);
        val = lval_pop(a, 1);
        if (STREQ(op, "def")) {
            lenv_def(e, sym, val);
        }
        if (STREQ(op, "=")) {
            lenv_put(e, sym, val);
        }
        lval_del(sym);
        lval_del(val);
    }
    lval_del(a);
    return lval_sexpr();
}

lval *builtin_def(lenv *e, lval *a)
{
    return builtin_var(e, a, "def");
}

lval *builtin_put(lenv *e, lval *a)
{
    return builtin_var(e, a, "=");
}

lval *builtin_op(lenv *e, lval *a, char *op)
{
    /* Ensure all arguments are numbers. */
    int i;
    for (i = 0; i < a->count; i++) {
        LASSERT_TYPE(a, a->cell[i], LVAL_NUM, i, op);
    }

    /* Pop the first element. */
    lval *x = lval_pop(a, 0);

    /* If no arguments and sub then perform unary negation. */
    if (STREQ(op, "-") && a->count == 0) {
        x->num = - x->num;
    }

    /* While there are still elements remainin... */
    while (a->count > 0) {
        /* Pop the next element. */
        lval *y = lval_pop(a, 0);

        if (STREQ(op, "+")) { x->num += y->num; }
        if (STREQ(op, "-")) { x->num -= y->num; }
        if (STREQ(op, "*")) { x->num *= y->num; }
        if (STREQ(op, "/")) {
            /* Ensure we're not dividing by zero. */
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


lval *builtin_add(lenv *e, lval *a)
{
    return builtin_op(e, a, "+");
}

lval *builtin_sub(lenv *e, lval *a)
{
    return builtin_op(e, a, "-");
}

lval *builtin_mul(lenv *e, lval *a)
{
    return builtin_op(e, a, "*");
}

lval *builtin_div(lenv *e, lval *a)
{
    return builtin_op(e, a, "/");
}

lval* builtin_head(lenv *e, lval *a)
{
    /* Check error conditions. */
    LASSERT_NARGS(a, a->count, 1, "head");
    LASSERT_TYPE(a, a->cell[0], LVAL_QEXPR, 0, "head");
    LASSERT(a, a->cell[0]->count != 0,
            "Function 'head' expected a non-emtpy Q-Expr, but was passed '{}'.");

    /* Otherwise take first argument. */
    lval *v = lval_take(a, 0);

    /* Delete all elements that are not head and return. */
    while (v->count > 1) {
        lval_del(lval_pop(v, 1));
    }
    return v;
}

lval* builtin_tail(lenv *e, lval *a)
{
    /* Check error conditions. */
    LASSERT_NARGS(a, a->count, 1, "tail");
    LASSERT_TYPE(a, a->cell[0], LVAL_QEXPR, 0, "tail");
    LASSERT(a, a->cell[0]->count != 0,
            "Function 'tail' expected a non-emtpy Q-Expr, but was passed '{}'.");

    /* Take first argument. */
    lval *v = lval_take(a, 0);

    /* Delete first element and return. */
    lval_del(lval_pop(v, 0));
    return v;
}

/* Turn an S-Expression into a Q-Expression. */
lval* builtin_list(lenv *e, lval *a)
{
    a->type = LVAL_QEXPR;
    return a;
}

/* Join two Q-Expressions. */
lval* builtin_join(lenv *e, lval *a)
{
    int i;
    for (i = 0; i < a->count; i++) {
        LASSERT_TYPE(a, a->cell[i], LVAL_QEXPR, i, "join");
    }

    lval *x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(e, x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

/* Exit from the program. */
lval *builtin_exit(lenv *e, lval *v)
{
    int out = 0;
    LASSERT_NARGS_RANGE(v, v->count, 0, 1, "exit");
    if (v->count) {
        LASSERT_TYPE(v, v->cell[0], LVAL_NUM, 0, "exit");
        out = v->cell[0]->num;
    }
    exit(out);
}

lval *builtin_lambda(lenv *e, lval *a)
{
    int i;
    /* Check for two arguments, each a Q-Expression. */
    LASSERT_NARGS(a, a->count, 2, "\\");
    LASSERT_TYPE(a, a->cell[0], LVAL_QEXPR, 0, "\\");
    LASSERT_TYPE(a, a->cell[1], LVAL_QEXPR, 1, "\\");

    /* Check first Q-Expression contains only Symbols. */
    for (i = 0; i < a->cell[0]->count; i++) {
        LASSERT_TYPE(a, a->cell[0]->cell[i], LVAL_SYM, i, "\\");
    }

    /* Pop first two arguments and pass them to lval_lambda */
    lval *formals = lval_pop(a, 0);
    lval *body = lval_pop(a, 0);
    lval_del(a);

    return lval_lambda(formals, body);
}

/*** Builtins for comparison ***/

lval *builtin_ord(lenv *e, lval *v, char *op)
{
    LASSERT_NARGS(v, v->count, 2, op);
    LASSERT_TYPE(v, v->cell[0], LVAL_NUM, 0, op);
    LASSERT_TYPE(v, v->cell[1], LVAL_NUM, 1, op);

    int r;
    int a = v->cell[0]->num;
    int b = v->cell[1]->num;
    lval_del(v);

    if (STREQ(op, "equal")) {
        r = (a == b);
    }
    if (STREQ(op, "<=")) {
        r = (a <= b);
    }
    if (STREQ(op, ">=")) {
        r = (a >= b);
    }
    if (STREQ(op, "<")) {
        r = (a < b);
    }
    if (STREQ(op, ">")) {
        r = (a > b);
    }

    return lval_num(r);
}

/* equal */
lval *builtin_eq(lenv *e, lval *v)
{
    int res = lval_eq(v->cell[0], v->cell[1]);
    lval_del(v);
    return lval_num(res);
}

/* greater */
lval *builtin_gt(lenv *e, lval *v)
{
    return builtin_ord(e, v, ">");
}

/* lesser */
lval *builtin_lt(lenv *e, lval *v)
{
    return builtin_ord(e, v, "<");
}

/* greater or equal */
lval *builtin_ge(lenv *e, lval *v)
{
    return builtin_ord(e, v, ">=");
}

/* lesser or equal */
lval *builtin_le(lenv *e, lval *v)
{
    return builtin_ord(e, v, "<=");
}

/* negation */
lval *builtin_not(lenv *e, lval *v)
{
    /* Check for one argument, a number. */
    LASSERT_NARGS(v, v->count, 1, "not");
    LASSERT_TYPE(v, v->cell[0], LVAL_NUM, 0, "not");

    return lval_num(! v->cell[0]->num);
}

/* and */
lval *builtin_and(lenv *e, lval *v)
{
    int i;
    lval *res;
    /* Ensure all arguments are of type num. */
    for (i = 0; i < v->count; i++) {
        LASSERT_TYPE(v, v->cell[i], LVAL_NUM, i, "and");
    }
    for (i = 0; i < v->count; i++) {
        if (!v->cell[i]->num) {
            res = lval_pop(v, i);
            lval_del(v);
            return res;
        }
    }
    res = lval_pop(v, v->count-1);
    lval_del(v);
    return res;
}

/* or */
lval *builtin_or(lenv *e, lval *v)
{
    int i;
    lval *res;
    /* Ensure all arguments are of type num. */
    for (i = 0; i < v->count; i++) {
        LASSERT_TYPE(v, v->cell[i], LVAL_NUM, i, "or");
    }
    for (i = 0; i < v->count; i++) {
        if (v->cell[i]->num) {
            res = lval_pop(v, i);
            lval_del(v);
            return res;
        }
    }
    lval_del(v);
    return lval_num(0);
}

/* conditional (if (condition) {consequence} {alternative}) */
lval *builtin_if(lenv *e, lval *v)
{
    /* Ensure we have 2 or three arguments,
     * first is bool (number) and the other one
     * or two are qexprs (code) */
    LASSERT_NARGS_RANGE(v, v->count, 2, 3, "if");
    LASSERT_TYPE(v, v->cell[0], LVAL_NUM, 0, "if");
    LASSERT_TYPE(v, v->cell[1], LVAL_QEXPR, 1, "if");
    lval *code = NULL;
    if (v->cell[0]->num) {
        code = lval_pop(v, 1);
    } else if (v->count == 3) {
        LASSERT_TYPE(v, v->cell[2], LVAL_QEXPR, 2, "if");
        code = lval_pop(v, 2);
    }
    code = (code ? code : lval_sexpr());
    code->type = LVAL_SEXPR;
    lval *result = lval_eval(e, code);
    lval_del(v);
    return result;
}

/*************** Functions to handle builtins ****************/

void lenv_add_builtin(lenv *e, char *name, lbuiltin func)
{
    lval *k = lval_sym(name);
    lval *v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}

void lenv_add_builtins(lenv *e)
{
    /* List functions */
    lenv_add_builtin(e, "list", (lbuiltin)builtin_list);
    lenv_add_builtin(e, "head", (lbuiltin)builtin_head);
    lenv_add_builtin(e, "join", (lbuiltin)builtin_join);
    lenv_add_builtin(e, "eval", (lbuiltin)builtin_eval);
    lenv_add_builtin(e, "tail", (lbuiltin)builtin_tail);

    /* Mathematical functions */
    lenv_add_builtin(e, "+", (lbuiltin)builtin_add);
    lenv_add_builtin(e, "-", (lbuiltin)builtin_sub);
    lenv_add_builtin(e, "*", (lbuiltin)builtin_mul);
    lenv_add_builtin(e, "/", (lbuiltin)builtin_div);

    /* Variable handling functions */
    lenv_add_builtin(e, "def", (lbuiltin)builtin_def);
    lenv_add_builtin(e, "=", (lbuiltin)builtin_put);
    lenv_add_builtin(e, "getenv", (lbuiltin)builtin_getenv);
    lenv_add_builtin(e, "\\", (lbuiltin)builtin_lambda);

    /* Comparison */
    lenv_add_builtin(e, "not", (lbuiltin)builtin_not);
    lenv_add_builtin(e, "<", (lbuiltin)builtin_lt);
    lenv_add_builtin(e, "<=", (lbuiltin)builtin_le);
    lenv_add_builtin(e, ">", (lbuiltin)builtin_gt);
    lenv_add_builtin(e, ">=", (lbuiltin)builtin_ge);
    lenv_add_builtin(e, "eq", (lbuiltin)builtin_eq);

    /* Conditionals */
    lenv_add_builtin(e, "if", (lbuiltin)builtin_if);
    lenv_add_builtin(e, "and", (lbuiltin)builtin_and);
    lenv_add_builtin(e, "or", (lbuiltin)builtin_or);

    /* Other */
    lenv_add_builtin(e, "exit", (lbuiltin)builtin_exit);
}

/*************************************************************/
int main(int argc, char *argv[])
{
    /* Create some parsers. */
    mpc_parser_t *Number       =     mpc_new("number");
    mpc_parser_t *Symbol       =     mpc_new("symbol");
    mpc_parser_t *Sexpr        =     mpc_new("sexpr");
    mpc_parser_t *Qexpr        =     mpc_new("qexpr");
    mpc_parser_t *Expr         =     mpc_new("expr");
    mpc_parser_t *Caballa      =     mpc_new("caballa");

    /* Define them with the following language. */
    mpca_lang(MPCA_LANG_DEFAULT,
            "                                                      \
            number      : /-?[0-9]+/ ;                             \
            symbol      : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;       \
            sexpr       : '(' <expr>* ')' ;                        \
            qexpr       : '{' <expr>* '}' ;                        \
            expr        : <number> | <symbol> | <sexpr> | <qexpr>; \
            caballa     : /^/ <expr>* /$/ ;                        \
            ",
            Number, Symbol, Sexpr, Qexpr, Expr, Caballa);
    /* Print version and Exit information. */
    puts("Caballa Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    mpc_result_t r;
    lval *x;

    /* Create environment. */
    lenv *e = lenv_new();
    lenv_add_builtins(e);

    char *input;
    /* Main loop. */
    while (1) {
        /* Output our prompt and get input. */
        input = readline("caballa> ");

        /* Add input to history. */
        add_history(input);

        /* Attempt to parse the user input. */
        if (mpc_parse("<stdin>", input, Caballa, &r)) {
            /* On success print the result of evaluation */
            x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            lval_del(x);
        } else {
            /* Otherwise print the error. */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }
    lenv_del(e);
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Caballa);
    return 0;
}
