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
} Opcode;

typedef enum {
  ERROR_NONE = 0,
  ERROR_INVALID_REGISTER_TYPE,
  ERROR_INVALID_REGISTER_NUMBER,
  ERROR_INVALID_OPERATION,
} Error;

#define MAX_REGISTERS 256
#define MAX_ARGS        6
int    iregisters[MAX_REGISTERS] = {};
double fregisters[MAX_REGISTERS] = {};
char  *sregisters[MAX_REGISTERS] = {};

#define BUFFER_SIZE 1024

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

int stmt_make_from_str(statement *stmt, char *src);
Opcode get_opcode_by_name(char *name);
void write_arg(char **arg, char *buffer, int *buffer_idx);

int parse_statement(statement stmt);
char get_register(char *s);
int is_valid_register(char c);
int get_register_index(char *s);

void mov(int target_reg, int reg, char *data);
void prnt(int target_reg, int reg, int newline);
void add(int target_reg, int reg_dst, int reg_src);

statementlist sl_make(void);
void sl_free(statementlist sl);
statementlist sl_append(statementlist sl, statement stmt);
void parse_statements(statementlist sl);

void fail(Error code);

static statementnode *new_node(statement stmt, statementnode *next);

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

  statementlist sl = sl_make();
  char line[BUFFER_SIZE] = { 0 };
  while (fgets(line, BUFFER_SIZE, f) != NULL) {
    line[strcspn(line, "\n")] = '\0';

    statement stmt = {};
    int ret = stmt_make_from_str(&stmt, line);
    if (ret != ERROR_NONE) {
      fail(ret);
      return 1;
    }

    sl = sl_append(sl, stmt);
  }

  parse_statements(sl);

  sl_free(sl);
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
    case OPCODE_ADD:
      int regsrc = get_register_index(stmt.args[1]);
      add(target_reg, reg, regsrc);
      break;
    default: break;
  }

  return 0;
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

void add(int target_reg, int reg_dst, int reg_src)
{
  switch (target_reg) {
    case 'i':
      iregisters[reg_dst] = iregisters[reg_dst] + iregisters[reg_src];
      break;
    case 'f':
      fregisters[reg_dst] = fregisters[reg_dst] + fregisters[reg_src];
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

statementlist sl_append(statementlist sl, statement stmt)
{
  if (!sl) return new_node(stmt, NULL);
  sl->next = sl_append(sl->next, stmt);

  return sl;
}

void parse_statements(statementlist sl)
{
  for (statementnode *tmp = sl; tmp; tmp = tmp->next) {
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
    default: break;
  }
}

static statementnode *new_node(statement stmt, statementnode *next)
{
  statementnode *x = malloc(sizeof(statementnode));
  x->s = stmt; x->next = next;
  return x;
}
