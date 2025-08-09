#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define PROGRAM_NAME "minilang"
#define streq(a, b) (strcmp((a), (b)) == 0)

typedef enum {
  OPCODE_MOV = 0,
  OPCODE_PRNT,
  OPCODE_PRNTL,
  OPCODE_ADD,
  OPCODE_SUB,
  OPCODE_MUL,
  OPCODE_DIV,
  OPCODE_INCR,
  OPCODE_DECR,
  OPCODE_CLR,
} Opcode;

typedef enum {
  ERROR_NONE = 0,
  ERROR_INVALID_REGISTER_TYPE,
  ERROR_INVALID_REGISTER_NUMBER,
  ERROR_INVALID_OPERATION,
  ERROR_MISSING_START,
  ERROR_DIV_ZERO,
} Error;

#define MAX_REGISTERS 256
#define MAX_ARGS        6
int    iregisters[MAX_REGISTERS] = {};
double fregisters[MAX_REGISTERS] = {};
char  *sregisters[MAX_REGISTERS] = {};

#define BUFFER_SIZE 1024
#define INIT_PROGRAM_CAPACITY 256

typedef struct statement {
  Opcode opcode;
  int nargs;
  char *args[MAX_ARGS];
} statement;

typedef struct statementnode {
  statement s;
  struct statementnode *next;
} statementnode;

typedef statementnode *statementlist;

typedef struct label {
  char *name;
  statementlist statements;
} label;

typedef struct program {
  label *labels;
  unsigned cap;
  unsigned len;
} program;

int stmt_make_from_str(statement *stmt, char *src);
Opcode get_opcode_by_name(char *name);
void write_arg(char **arg, char *buffer, int *buffer_idx);

int parse_statement(statement stmt);
char get_register(char *s);
int is_valid_register(char c);
int get_register_index(char *s);

void mov(int target_reg, int reg, char *data);
void prnt(int target_reg, int reg, int newline);
int arithm(int target_reg, int reg_dst, int reg_src, int op);
int iarithm(int reg_dst, int reg_src, int op);
int farithm(int reg_dst, int reg_src, int op);
void incr(int target_reg, int reg, int sign);
void clear(int target_reg, int reg);

statementlist sl_make(void);
void sl_free(statementlist sl);
statementlist sl_make_from_file(FILE *f);
statementlist sl_append(statementlist sl, statement stmt);
void parse_statements(label lb);
void fail(Error code);

label make_label(char *name, statementlist statements);

void pg_init(program *pg, unsigned cap);
void pg_deinit(program *pg);
void pg_append_label(program *pg, label lb);
void interpret(program *pg);
int find_label(program *pg, char *name);

static statementnode *new_node(statement stmt, statementnode *next);
static int is_empty(char *s);

int main(int argc, char **argv)
{
  if (argc < 2) {
    printf("usage: %s <sourcefile>\n", PROGRAM_NAME);
    return 1;
  }

  char *filename = argv[1];
  FILE *f = fopen(filename, "r");
  if (!f) {
    printf("%s: file '%s' doesn't exist\n", PROGRAM_NAME, filename);
    return 1;
  }

  program pg;
  pg_init(&pg, INIT_PROGRAM_CAPACITY);

  statementlist sl = sl_make_from_file(f);
  label startlabel = make_label("start", sl);
  pg_append_label(&pg, startlabel);

  interpret(&pg);

  pg_deinit(&pg);
  return 0;
}

int stmt_make_from_str(statement *stmt, char *src)
{
  int srcsize = strlen(src);

  char buffer[BUFFER_SIZE] = { 0 };
  int buffer_idx = 0;
  int reading_opcode = 1;
  int inside_string = 0;

  for (int i = 0; i <= srcsize; ++i) {
    if ((src[i] == '\0' || src[i] == ' ') && !inside_string) {
      if (reading_opcode) {
        int opcode = get_opcode_by_name(buffer);
        if (opcode == -1)
          return ERROR_INVALID_OPERATION;

        stmt->opcode = opcode;
        reading_opcode = 0;

        memset(buffer, 0, buffer_idx);
        buffer_idx = 0;
        continue;
      }

      write_arg(&stmt->args[stmt->nargs], buffer, &buffer_idx);
      ++stmt->nargs;
      ++i;

      if (i >= srcsize)
        break;
    }

    if (src[i] == '"' && !inside_string) { inside_string = 1; ++i; }
    if (src[i] == '"' && inside_string) {
      inside_string = 0;
      write_arg(&stmt->args[stmt->nargs], buffer, &buffer_idx);
      ++stmt->nargs;
    }

    buffer[buffer_idx++] = src[i];
  }

  return ERROR_NONE;
}

void write_arg(char **arg, char *buffer, int *buffer_idx)
{
  buffer[*buffer_idx] = '\0';
  *arg = malloc(sizeof(char)*(*buffer_idx + 1));
  strcpy(*arg, buffer);

  memset(buffer, 0, *buffer_idx);
  *buffer_idx = 0;
}

Opcode get_opcode_by_name(char *name)
{
  if (streq(name, "mov")) return OPCODE_MOV;
  else if (streq(name, "prnt")) return OPCODE_PRNT;
  else if (streq(name, "prntl")) return OPCODE_PRNTL;
  else if (streq(name, "add")) return OPCODE_ADD;
  else if (streq(name, "sub")) return OPCODE_SUB;
  else if (streq(name, "mul")) return OPCODE_MUL;
  else if (streq(name, "div")) return OPCODE_DIV;
  else if (streq(name, "incr")) return OPCODE_INCR;
  else if (streq(name, "decr")) return OPCODE_DECR;
  else if (streq(name, "clr")) return OPCODE_CLR;
  else return -1;
}

int parse_statement(statement stmt)
{
  int opcode = stmt.opcode;
  int target_reg = get_register(stmt.args[0]);
  if (target_reg == -1)
    return ERROR_INVALID_REGISTER_TYPE;

  int reg = get_register_index(stmt.args[0]);
  if (reg == -1)
    return ERROR_INVALID_REGISTER_NUMBER;

  switch (opcode) {
    case OPCODE_MOV:
      mov(target_reg, reg, stmt.args[1]);
      break;
    case OPCODE_PRNT: case OPCODE_PRNTL:
      int newline = (opcode == OPCODE_PRNTL)? 1 : 0;
      prnt(target_reg, reg, newline);
      break;
    case OPCODE_ADD: case OPCODE_SUB:
    case OPCODE_MUL: case OPCODE_DIV:
      int regsrc = get_register_index(stmt.args[1]);
      int ret = arithm(target_reg, reg, regsrc, opcode);
      if (ret != ERROR_NONE)
        return ret;
      break;
    case OPCODE_INCR:
    case OPCODE_DECR:
      incr(target_reg, reg, (opcode == OPCODE_INCR)? 1 : -1);
      break;
    case OPCODE_CLR:
      clear(target_reg, reg);
      break;
    default: break;
  }

  return ERROR_NONE;
}

char get_register(char *s)
{
  char reg = s[0];
  return is_valid_register(reg)? reg : -1;
}

int is_valid_register(char c)
{
  return c == 'i' || c == 'f' || c == 's';
}

int get_register_index(char *s)
{
  int idx = atoi(s+1);
  return idx >= 0 && idx < MAX_REGISTERS? idx : -1;
}

void mov(int target_reg, int reg, char *data)
{
  switch (target_reg) {
    case 'i':
      int n = atoi(data);
      iregisters[reg] = n;
      break;
    case 'f':
      double f = atof(data);
      fregisters[reg] = f;
      break;
    case 's':
      int datasize = strlen(data);
      sregisters[reg] = malloc(sizeof(char)*(datasize + 1));
      strcpy(sregisters[reg], data);
      break;
  }
}

void prnt(int target_reg, int reg, int newline)
{
  switch (target_reg) {
    case 'i':
      printf("%d", iregisters[reg]);
      break;
    case 'f':
      printf("%g", fregisters[reg]);
      break;
    case 's':
      printf("%s", sregisters[reg]);
      break;
  }

  if (newline) printf("\n");
}

int arithm(int target_reg, int reg_dst, int reg_src, int op)
{
  int ret = ERROR_NONE;
  switch (target_reg) {
    case 'i':
      ret = iarithm(reg_dst, reg_src, op);
      break;
    case 'f':
      ret = farithm(reg_dst, reg_src, op);
      break;
    default: break;
  }

  return ret;
}

int iarithm(int reg_dst, int reg_src, int op)
{
  switch (op) {
    case OPCODE_ADD:
      iregisters[reg_dst] += iregisters[reg_src];
      break;
    case OPCODE_SUB:
      iregisters[reg_dst] -= iregisters[reg_src];
      break;
    case OPCODE_MUL:
      iregisters[reg_dst] *= iregisters[reg_src];
      break;
    case OPCODE_DIV:
      if (iregisters[reg_src] == 0)
        return ERROR_DIV_ZERO;
      iregisters[reg_dst] /= iregisters[reg_src];
      break;
    default: break;
  }

  return ERROR_NONE;
}

int farithm(int reg_dst, int reg_src, int op)
{
  switch (op) {
    case OPCODE_ADD:
      fregisters[reg_dst] += fregisters[reg_src];
      break;
    case OPCODE_SUB:
      fregisters[reg_dst] -= fregisters[reg_src];
      break;
    case OPCODE_MUL:
      fregisters[reg_dst] *= fregisters[reg_src];
      break;
    case OPCODE_DIV:
      if (fregisters[reg_src] == 0)
        return ERROR_DIV_ZERO;
      fregisters[reg_dst] /= fregisters[reg_src];
      break;
    default: break;
  }

  return ERROR_NONE;
}

void incr(int target_reg, int reg, int sign)
{
  switch (target_reg) {
    case 'i': iregisters[reg] += sign; break;
    case 'f': fregisters[reg] += sign; break;
    default: break;
  }
}

void clear(int target_reg, int reg)
{
  switch (target_reg) {
    case 'i': iregisters[reg] = 0; break;
    case 'f': fregisters[reg] = 0.0; break;
    case 's':
      free(sregisters[reg]);
      sregisters[reg] = NULL;
      break;
    default: break;
  }
}

statementlist sl_make(void)
{
  return NULL;
}

void sl_free(statementlist sl)
{
  statementnode *erase = sl;
  while (erase) {
    sl = sl->next;
    for (int i = 0; i < erase->s.nargs; ++i)
      free(erase->s.args[i]);

    free(erase);

    erase = sl;
  }
}

statementlist sl_make_from_file(FILE *f)
{
  statementlist sl = sl_make();

  char line[BUFFER_SIZE] = { 0 };
  while (fgets(line, BUFFER_SIZE, f) != NULL) {
    line[strcspn(line, "\n")] = '\0';
    if (is_empty(line))
      continue;

    statement stmt = {};
    int ret = stmt_make_from_str(&stmt, line);
    if (ret != ERROR_NONE) {
      fail(ret);
      return NULL;
    }

    sl = sl_append(sl, stmt);
  }

  return sl;
}

statementlist sl_append(statementlist sl, statement stmt)
{
  if (!sl) return new_node(stmt, NULL);
  sl->next = sl_append(sl->next, stmt);

  return sl;
}

void parse_statements(label lb)
{
  for (statementnode *tmp = lb.statements; tmp; tmp = tmp->next) {
    int ret = parse_statement(tmp->s);
    if (ret != ERROR_NONE) {
      fail(ret);
      return;
    }
  }
}

void fail(Error code)
{
  switch (code) {
    case ERROR_INVALID_REGISTER_TYPE:
      printf("%s: invalid register type (only i, f and s are accepted)\n", PROGRAM_NAME);
      break;
    case ERROR_INVALID_REGISTER_NUMBER:
      printf("%s: invalid register number (must be between 0 and %d)\n", PROGRAM_NAME, MAX_REGISTERS - 1);
      break;
    case ERROR_INVALID_OPERATION:
      printf("%s: unknown operation\n", PROGRAM_NAME);
      break;
    case ERROR_DIV_ZERO:
      printf("%s: division by zero\n", PROGRAM_NAME);
      break;
    case ERROR_MISSING_START:
      printf("%s: 'start' label not found\n", PROGRAM_NAME);
      break;
    default: break;
  }
}

label make_label(char *name, statementlist statements)
{
  return (label){ name, statements };
}

void pg_init(program *pg, unsigned cap)
{
  pg->labels = calloc(cap, sizeof(label));
  pg->cap = cap;
  pg->len = 0;
}

void pg_deinit(program *pg)
{
  for (int i = 0; i < pg->cap; ++i)
    sl_free(pg->labels[i].statements);

  free(pg->labels);
  pg->cap = pg->len = 0;
}

void pg_append_label(program *pg, label lb)
{
  if (pg->cap <= 0)
    return;

  if (pg->len + 1 >= pg->cap/2) {
    pg->cap *= 2;
    pg->labels = realloc(pg->labels, sizeof(label)*pg->cap);
  }

  pg->labels[pg->len++] = lb;
}

void interpret(program *pg)
{
  // index 0 should be start
  int start_exists = find_label(pg, "start");
  if (!start_exists) {
    fail(ERROR_MISSING_START);
    return;
  }

  parse_statements(pg->labels[0]);
}

int find_label(program *pg, char *name)
{
  for (int i = 0; i < pg->cap; ++i)
    if (pg->labels[i].name && streq(pg->labels[i].name, name))
      return 1;

  return 0;
}

static statementnode *new_node(statement stmt, statementnode *next)
{
  statementnode *x = malloc(sizeof(statementnode));
  x->s = stmt; x->next = next;
  return x;
}

static int is_empty(char *s)
{
  for (int i = 0; s[i]; ++i)
    if (!isspace(s[i]))
      return 0;

  return 1;
}
