#include <stdio.h>
#include <stdlib.h>
#include <mpc.h>

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

/* Otherwise include the editline headers */
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

/* Use operator string to see which operation to perform, perform it. */
long eval_op(long x, char *op, long y)
{
    if (strcmp(op, "+") == 0) { return x + y; }
    if (strcmp(op, "-") == 0) { return x - y; }
    if (strcmp(op, "*") == 0) { return x * y; }
    if (strcmp(op, "/") == 0) { return x / y; }
    if (strcmp(op, "%") == 0) { return x % y; }
    return 0;
}

/* Evaluate the Abstract Syntax Tree */
long eval(mpc_ast_t *t)
{
    /* If it is a number return it */
    if (strstr(t->tag, "number"))
        return atoi(t->contents);

    int i;
    long x;
    char *op;

    /* The operator is always second child (first is '(') */
    op = t->children[1]->contents;

    /* We store the third child in x */
    x = eval(t->children[2]);

    /* Iterate the remaining children and combining. */
    for (i=3; strstr(t->children[i]->tag, "expr"); i++)
        x = eval_op(x, op, eval(t->children[i]));

    return x;
}


int main(int argc, char *argv[])
{
    /* Create some parsers */
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Operator = mpc_new("operator");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *Caballa = mpc_new("caballa");

    /* Define them with the following language */
    mpca_lang(MPCA_LANG_DEFAULT,
            "                                                               \
            number      :     /-?[0-9]+/ ;                                  \
            operator    :   '+' | '-' | '*' | '/' | '%' ;                   \
            expr        :       <number> | '(' <operator> <expr>+ ')' ;     \
            caballa     :    /^/ <operator> <expr>+ /$/ ;                   \
            ",
            Number, Operator, Expr, Caballa);
    /* Print version and Exit information */
    puts("Caballa Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    mpc_result_t r;
    /* In a never ending loop */
    while (1) {
        /* Output our prompt and get input */
        char *input = readline("caballa> ");

        /* Add input to history */
        add_history(input);

        /* Attempt to parse the user input */
        if (mpc_parse("<stdin>", input, Caballa, &r)) {
            /* On success print the result of evaluation */
            printf("%li\n", eval(r.output));
            mpc_ast_delete(r.output);
        } else {
            /* Otherwise print the error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }
    puts("bye!");
    mpc_cleanup(4, Number, Operator, Expr, Caballa);
    return 0;
}
