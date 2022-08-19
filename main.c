#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#define DEBUG

struct term;

typedef struct {
    char* arg;
    struct term* term;
} abstraction;

typedef struct {
    struct term* left;
    struct term* right;
} application;

typedef const char* variable;

typedef enum {
    TYPE_ABSTRACTION,
    TYPE_APPLICATION,
    TYPE_VARIABLE,
} term_type;

typedef union {
    abstraction abstraction;
    application application;
    variable    var;
} term_val;

typedef struct term {
    term_type type;
    term_val value;
} term;

void dump_(const term *term) {
    if (term == NULL) {
        printf("NULL");
        return;
    }
    switch (term->type) {
    case TYPE_ABSTRACTION: {
        char *str = malloc(strlen(term->value.abstraction.arg)+1);
        strcpy(str, term->value.var);
        if (strchr(str, '-')) {
            while (*str++ != '-');
        }
#ifdef DEBUG
        printf("\\%s.", term->value.abstraction.arg);
#else
        printf("\\%s.", str);
#endif //DEBUG
        dump_(term->value.abstraction.term);
        break;
  } case TYPE_APPLICATION:
        printf("(");
        dump_(term->value.application.left);
        printf(")(");
        dump_(term->value.application.right);
        printf(")");
        break;
    case TYPE_VARIABLE: {
        char *str = malloc(strlen(term->value.var)+1);
        strcpy(str, term->value.var);
        if (strchr(str, '-')) {
            while (*str++ != '-');
        }
#ifdef DEBUG
        printf("%s", term->value.var);
#else
        printf("%s", str);
#endif //DEBUG
        break;
    }}
}

void dump(const term *term) {
    dump_(term);
    printf("\n");
}

#define skip_spaces(str, pos)                                           \
    while (*pos < strlen(str) && (str[*pos] == ' ' || str[*pos] == '\t')) { \
        (*pos)++;                                                        \
    }

char *parse_var(const char *str, size_t *pos) {
    size_t i = 0;
    while (*pos < strlen(str)
           && str[*pos] != ' '  && str[*pos] != '.'
           && str[*pos] != '\t' && str[*pos] != '\\'
           && str[*pos] != '('  && str[*pos] != ')'
           && str[*pos] != '=') {
        *pos = *pos + 1;
        i++;
    }

    char *string = malloc(i * sizeof(char) + 1);
    memcpy(string, str+*pos-i, i);
    string[i] = '\0';
    return string;
}

typedef struct {
    bool error;
    const char *msg;
    term *term;
} term_result;

#define check(exp, x)                                                   \
    do {                                                                \
        term_result _x = x;                                             \
        if (_x.error) {                                                 \
            return _x;                                                  \
        } else {                                                        \
            exp = _x.term;                                              \
        }                                                               \
    } while (0);

#define VA_ARGS(...) , ##__VA_ARGS__
#define ERR(str, ...)                                               \
    do {                                                            \
        term_result res;                                            \
        size_t sz = snprintf(NULL, 0, str VA_ARGS(__VA_ARGS__));   \
        char *buf = malloc(sz+1);                                   \
        snprintf(buf, sz+1, str VA_ARGS(__VA_ARGS__));             \
        res.error = true;                                           \
        res.msg = buf;                                              \
        return res;                                                 \
    } while(0);                                                     \

#define OK(val)                                 \
    do {                                        \
        term_result _res;                       \
        _res.error = false;                     \
        _res.term = val;                        \
        return _res;                            \
    } while(0);                                 \

term_result parse(const char *str, size_t *pos) {
    term *current = NULL;
    term *next = NULL;
    bool init = false;
    if (sizeof(str) == 0) ERR("unexpected end of file\n");

    for (;;) {
        skip_spaces(str, pos);
        switch (str[*pos]) {
        case '.':
            ERR("unexpected token '.' at position %ld\n", *pos)
                break;
        case '\\':
            (*pos)++;
            init = true;
            if (next == NULL) next = malloc(sizeof(term));
            next->type = TYPE_ABSTRACTION;
            skip_spaces(str, pos);
            next->value.abstraction.arg = parse_var(str, pos);
            // expect '.'
            if (str[*pos] != '.') {
                free(next);
                ERR("invalid character '%c', '.' expected at position %ld\n",
                    str[*pos], *pos);
            }
            (*pos)++;
            check(next->value.abstraction.term, parse(str, pos));
            current = next;
            break;
        case '(':
            (*pos)++;
            skip_spaces(str, pos);
            term *nxt;
            check(nxt, parse(str, pos));
            skip_spaces(str, pos);
            if (next == NULL) next = malloc(sizeof(term));
            switch (str[*pos]) {
            case ')':
            case '\0':
                (*pos)++;
                init = true;
                next = nxt;
                if (current == NULL) {
                    current = next;
                } else {
                    term *left = current;
                    term *right = next;

                    current = malloc(sizeof(term));
                    current->type = TYPE_APPLICATION;
                    current->value.application.left = left;
                    current->value.application.right = right;
                    next = current;
                }
                break;
            default:
                skip_spaces(str, pos);
                init = true;
                next->type = TYPE_APPLICATION;
                next->value.application.left = nxt;
                check(next->value.application.right, parse(str, pos));
                current = next;
                break;
            }
            break;
        case ')':
            OK(current)
            break;
        default: {
            // expect variable
            variable var = parse_var(str, pos);

            skip_spaces(str, pos);
            if (next == NULL) next = malloc(sizeof(term));
            init = true;
            next->type = TYPE_VARIABLE;
            next->value.var = var;
            skip_spaces(str, pos);
            init = true;
            if (current == NULL) {
                current = next;
            } else {
                term *left = current;
                term *right = next;

                current = malloc(sizeof(term));
                current->type = TYPE_APPLICATION;
                current->value.application.left = left;
                current->value.application.right = right;
                next = current;
            }
            break;
        }}
        skip_spaces(str, pos);
        if (*pos >= strlen(str)) OK(next);
        next = NULL;
    }

    if (!init) {
        free(current);
        ERR("Not implemented");
    };

    OK(next);
}

typedef enum {
    NONE = 0,
    TYPE_INT,
    TYPE_BOOL,
} display_type;

typedef struct {
    const char *name;
    term *term;
    display_type type;
} line_t;

line_t *parse_line(const char *line) {
    if (strlen(line) == 0) return NULL;
    // comments
    if (line[0] == '-' && line[1] == '-') return NULL;
    size_t pos = 0;
    skip_spaces(line, &pos);
    const char *name = parse_var(line, &pos);
    skip_spaces(line, &pos);

    display_type tp = NONE;

    // expect `=`
    if (line[pos++] != '=') {
        // allow type specification
        if (strcmp(name, "main") == 0) {
            pos--;
            const char *type = parse_var(line, &pos);
            if (strcmp(type, "bool") == 0) {
                tp = TYPE_BOOL;
            } else if (strcmp(type, "int") == 0 || strcmp(type, "num") == 0) {
                tp = TYPE_INT;
            }
            skip_spaces(line, &pos);
        }
        if (line[pos++] != '=') {
            printf("Parse error: expected `=`, found `%c`\n", line[pos-1]);
            exit(1);
        }
    }
    skip_spaces(line, &pos);
    term_result res = parse(line, &pos);
    if (res.error) {
        printf("Parse error: %s\n", res.msg);
        return NULL;
    } else {
        line_t *ln = malloc(sizeof(line_t));
        ln->name = name;
        ln->term = res.term;
        ln->type = tp;
        return ln;
    }
}

typedef struct list {
    struct list *next;
    line_t line;
} list;

void set(list **list, line_t *line) {
    if (*list == NULL) {
        *list = malloc(sizeof(struct list));
        (*list)->line = *line;
        (*list)->next = NULL;
        return;
    }
    if ((*list)->line.name == NULL) {
        (*list)->line = *line;
        (*list)->next = NULL;
        return;
    }

    struct list *last = *list;
    struct list *current = last->next;
    for (;current != NULL; last = current, current = current->next) {
        if (current->line.name == NULL ||
            strcmp(current->line.name,line->name) == 0) {
            last->next = current;
            current->line = *line;
            current->next = NULL;
            return;
        }
    }
    last->next = malloc(sizeof(struct list));
    last->next->line = *line;
    last->next->next = NULL;
}

bool contains(const list *list, const char *name) {
    for (;list != NULL; list = list->next) {
        if (list->line.name == NULL) return NULL;
        if (strcmp(list->line.name, name) == 0) {
            return true;
        }
    }
    return false;
}

term *get(const list *list, const char *name) {
    for (;list != NULL; list = list->next) {
        if (list->line.name == NULL) return NULL;
        if (strcmp(list->line.name, name) == 0) {
            return list->line.term;
        }
    }
    return NULL;
}

void delete(list **list, const char *name) {
    if (*list == NULL) {
        return;
    }

    if ((*list)->next == NULL) {
        (*list) = NULL;
        return;
    }

    struct list *last = *list;
    struct list *current = last->next;
    for (;current != NULL; current = current->next) {
        if (strcmp(current->line.name, name) == 0) {
            last->next = current->next;
            return;
        }
    }
}

void dump_list(const list *list) {
    for (;list != NULL; list = list->next) {
        printf("%p: {next: %p, line: "
               "{name: '%s', term: ",
               (void*)list, (void*)list->next, list->line.name);
        dump_(list->line.term);
        printf("}}\n");
    }
}


term *clone(term *other) {
    term *new = malloc(sizeof(term));
    new->type = other->type;
    switch (other->type) {
    case TYPE_ABSTRACTION:
        new->value.abstraction.arg = other->value.abstraction.arg;
        new->value.abstraction.term = clone(other->value.abstraction.term);
        break;
    case TYPE_APPLICATION:
        new->value.application.left = clone(other->value.application.left);
        new->value.application.right = clone(other->value.application.right);
        break;
    case TYPE_VARIABLE:
        new->value.var = other->value.var;
        break;
    }
    return new;
}

void update(term *tm, line_t *line) {
    switch (tm->type) {
    case TYPE_ABSTRACTION:
        update(tm->value.abstraction.term, line);
        break;
    case TYPE_APPLICATION:
        update(tm->value.application.left, line);
        update(tm->value.application.right, line);
        break;
    case TYPE_VARIABLE:
        if (strcmp(tm->value.var, line->name) == 0) {
            *tm = *clone(line->term);
        }
        break;
    }
}

void prefix_args(term *tm, const char *prefix, list *vars, const list *functions) {
    switch (tm->type) {
    case TYPE_ABSTRACTION: {
        char *new_str = malloc((strlen(tm->value.abstraction.arg)
                                + 1 + strlen(prefix)+1)*sizeof(char));
        memcpy(new_str, prefix, strlen(prefix));
        new_str[strlen(prefix)] = '-';
        memcpy(new_str+strlen(prefix) + 1,
               tm->value.abstraction.arg, strlen(tm->value.abstraction.arg));
        new_str[strlen(prefix)+strlen(tm->value.abstraction.arg)+1] = '\0';

        line_t *ln = malloc(sizeof(line_t));
        ln->name = tm->value.abstraction.arg;
        set(&vars, ln);
        prefix_args(tm->value.abstraction.term, prefix, vars, functions);
        delete(&vars, ln->name);
        free(ln);

        tm->value.abstraction.arg = new_str;
        break;
    } case TYPE_APPLICATION:
        prefix_args(tm->value.application.left,  prefix, vars, functions);
        prefix_args(tm->value.application.right, prefix, vars, functions);
        break;
    case TYPE_VARIABLE: {
        if (!contains(functions, tm->value.var) && strchr(tm->value.var, '-') == NULL) {
            char *new_str = malloc((strlen(tm->value.var)
                                    + 1 + strlen(prefix)+1)*sizeof(char));
            memcpy(new_str, prefix, strlen(prefix));
            new_str[strlen(prefix)] = '-';
            memcpy(new_str+strlen(prefix)
                   + 1, tm->value.var, strlen(tm->value.var));
            new_str[strlen(prefix)+strlen(tm->value.var)+1] = '\0';
            tm->value.var = new_str;
        }
        break;
    }}
}

bool eval(const list *functions, const char *current_function,
          term *tm, int num) {
    list *next_functions        = (list*)functions;
    char *next_current_function = (char*)current_function;
    term *next_tm = tm;
    int next_num  = num;

    switch (tm->type) {
    case TYPE_ABSTRACTION:
        if (tm->value.abstraction.term->type == TYPE_VARIABLE) {
            return true;
        } else {
            next_functions        = (list*)functions;
            next_current_function = (char*)current_function;
            next_tm               = tm->value.abstraction.term;
            next_num              = num++;
            break;
        }
    case TYPE_APPLICATION: {
        term *left = tm->value.application.left;
        eval(functions, current_function,
             tm->value.application.right, num++);
        
        while (left->type != TYPE_ABSTRACTION) {
            if (eval(functions, current_function, left, num++) && left->type != TYPE_ABSTRACTION) {
                return true;
            }
        }
        
        line_t line;
        line.name = left->value.abstraction.arg;
        line.term = tm->value.application.right;
        
        update(left->value.abstraction.term, &line);
        
        *tm = *left->value.abstraction.term;
        break;
    } case TYPE_VARIABLE: {
          // detect recursion:

          if (strcmp(tm->value.var, current_function) == 0) {
              fprintf(stderr,
                      "ERROR: Recursion detected in function `%s`.\n",
                      current_function);
              exit(1);
          }
          term *new_term = get(functions, tm->value.var);
          if (new_term == NULL) {
              return true;
          } else {
              term *eval_term = clone(new_term);
              list *ls = malloc(sizeof(list));
              
              int len = snprintf(NULL, 0, "%d-%s", num, tm->value.var);
                  if (len < 0) {
                      fprintf(stderr,
                              "sprintf(3) ERROR: %s (ERRNO %d)",
                              strerror(errno), errno);
                      exit(1);
                  }
                  char *prefix = malloc(len+1);
                  snprintf(prefix, len+1, "%d-%s", num, tm->value.var);
                  
                  prefix_args(eval_term, prefix, ls, functions);
                  free(ls);
                  eval(functions, tm->value.var, eval_term, num++);
                  new_term = malloc(sizeof(term));
                  *new_term = *eval_term;
              }
          
              *tm = *new_term;
              return false;
      }}
    
    return eval(next_functions, next_current_function,
                next_tm, next_num);
}

void eval_line(const list *functions, const char *name) {
    term *term = get(functions, name);
    eval(functions, name, term, 0);
}

int main(int argc, char **argv) {
    if (argc < 1) {
        fprintf(stderr, "What did you do..., just open it normally...\n");
        exit(69);
    }
    if (argc < 2) {
        fprintf(stderr,
                "Not enough arguments!\nUsage: %s <file name>\n", argv[0]);
        exit(1);
    }

    FILE *file = fopen(argv[1], "r");
    if (file == NULL) {
        fprintf(stderr,
                "ERROR: could not open file: %s (ERRNO %d)\n",
                strerror(errno), errno);
        exit(1);
    }

    list *list = NULL;
    char *line = NULL;
    size_t l;
    ssize_t read;

    display_type type = NONE;

    while ((read = getline(&line, &l, file)) != -1) {
        line[read-1] = '\0';
        line_t *parsed_line = parse_line(line);
        if (parsed_line != NULL) {
            if (strcmp(parsed_line->name, "main") == 0) {
                type = parsed_line->type;
            }
            set(&list, parsed_line);
        }
        free(line);
        line = NULL;
    }

    eval_line(list, "main");
    term *t = get(list, "main");

    switch (type) {
    case TYPE_INT:
        if (t->type == TYPE_ABSTRACTION) {
            const char *arg1 = t->value.abstraction.arg;
            if (t->value.abstraction.term->type == TYPE_ABSTRACTION) {
                const char *arg2 = t->value.abstraction.term->
                    value.abstraction.arg;
                term *current_body = t->value.abstraction.term->
                    value.abstraction.term;
                bool invalid = false;
                for (unsigned long long int i = 0;;i++) {
                    if (current_body->type == TYPE_APPLICATION) {
                        term *left = current_body->value.application.left;
                        if (left->type == TYPE_VARIABLE &&
                            strcmp(left->value.var, arg1) == 0) {
                            current_body = current_body->
                                value.application.right;
                            continue;
                        } else {
                            invalid = true;
                            break;
                        }
                    } else if (current_body->type == TYPE_VARIABLE) {
                        if (strcmp(current_body->value.var, arg2) == 0) {
                            printf("%lld\n", i);
                            invalid = false;
                            break;
                        } else {
                            invalid = true;
                            break;
                        }
                    } else {
                        invalid = true;
                        break;
                    }
                }
                if (!invalid) break;
                dump(t);
                break;
            }
        }
        dump(t);
        break;
    case TYPE_BOOL:
        if (t->type == TYPE_ABSTRACTION) {
            if (t->value.abstraction.term->type == TYPE_ABSTRACTION) {
                if (strcmp(t->value.abstraction.arg,
                           t->value.abstraction.term->
                           value.abstraction.term->value.
                           abstraction.arg) == 0) {
                    printf("true\n");
                    break;
                } else if (strcmp(t->value.abstraction.term->
                                  value.abstraction.arg,
                                  t->value.abstraction.term->
                                  value.abstraction.term->
                                  value.abstraction.arg
                                  ) == 0) {
                    printf("false\n");
                    break;
                }
            }
        }
        dump(t);
        break;
    default:
        dump(t);
        break;
    }
}
