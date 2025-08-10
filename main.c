#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define PROGRAM_NAME "minilang"
#define streq(a, b) (strcmp((a), (b)) == 0)

typedef enum {
  OPCODE_MOV = 0,
  OPCODE_PRNT,
  OPCODE_PRNTL,
  OPCODE_ADD,
  OPCODE_ADD2,
  OPCODE_SUB,
  OPCODE_MUL,
  OPCODE_DIV,
  OPCODE_MOD,
  OPCODE_INCR,
  OPCODE_DECR,
  OPCODE_CLR,
  OPCODE_JMP,
  OPCODE_JMPNZ,
  OPCODE_JMPZ,
} Opcode;

typedef enum {
  ERROR_NONE = 0,
  ERROR_INVALID_REGISTER_TYPE,
  ERROR_INVALID_REGISTER_NUMBER,
  ERROR_INVALID_OPERATION,
  ERROR_MISSING_START,
  ERROR_DIV_ZERO,
  ERROR_UNKNOWN_LABEL,
  ERROR_DIFFERENT_REGISTER_TYPE,
} Error;

#define MAX_REGISTERS 256
#define MAX_ARGS        6
int    iregisters[MAX_REGISTERS] = { 0 };
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

int get_labels(program *pg, FILE *f);

int stmt_make_from_str(statement *stmt, char *src);
Opcode get_opcode_by_name(char *name);
void write_arg(char **arg, char *buffer, int *buffer_idx);

int parse_statement(statement stmt);
char get_register(char *s);
int is_valid_register(char c);
int get_register_index(char *s);

void mov(int target_reg, int reg, char *data);
void mov_register(int target_reg, int reg, int reg2);
void prnt(int target_reg, int reg, int newline);
int arithm(int target_reg, int reg_dst, int reg_src, int op);
int iarithm(int reg_dst, int reg_src, int op);
int farithm(int reg_dst, int reg_src, int op);
void add2(int target_reg, int reg, int reg1, int reg2);
void incr(int target_reg, int reg, int sign);
void clear(int target_reg, int reg);
void jmp(program *pg, int labelidx);
void jmp_cond(program *pg, int target_reg, int reg, int labelidx, int notzero);

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
static void clear_registers(void);

program pg;

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

  pg_init(&pg, INIT_PROGRAM_CAPACITY);

  int ret = get_labels(&pg, f);
  if (ret != ERROR_NONE) {
    fail(ret);
    return 1;
  }

  interpret(&pg);

  fclose(f);
  pg_deinit(&pg);
  clear_registers();

  return 0;
}

int get_labels(program *pg, FILE *f)
{
  label lb = {};
  statementlist sl = sl_make();

  char line[BUFFER_SIZE] = { 0 };

  while (fgets(line, BUFFER_SIZE, f) != NULL) {
    line[strcspn(line, "\n")] = '\0';
    if (is_empty(line))
      continue;

    if (streq(line, "end")) {
      lb = make_label(lb.name, sl);
      pg_append_label(pg, lb);
      sl = sl_make();
      continue;
    }

    int linesize = strlen(line);
    if (line[linesize - 1] == ':')
      lb.name = strndup(line, linesize - 1);
    else {
      statement stmt = {};

      // TODO: fix this dumb hack
      int ret = stmt_make_from_str(&stmt, line + 2);
      if (ret != ERROR_NONE)
        return ret;

      sl = sl_append(sl, stmt);
    }
  }

  return ERROR_NONE;
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
  else if (streq(name, "add2")) return OPCODE_ADD2;
  else if (streq(name, "sub")) return OPCODE_SUB;
  else if (streq(name, "mul")) return OPCODE_MUL;
  else if (streq(name, "div")) return OPCODE_DIV;
  else if (streq(name, "mod")) return OPCODE_MOD;
  else if (streq(name, "incr")) return OPCODE_INCR;
  else if (streq(name, "decr")) return OPCODE_DECR;
  else if (streq(name, "clr")) return OPCODE_CLR;
  else if (streq(name, "jmp")) return OPCODE_JMP;
  else if (streq(name, "jmpnz")) return OPCODE_JMPNZ;
  else if (streq(name, "jmpz")) return OPCODE_JMPZ;
  else return -1;
}

int parse_statement(statement stmt)
{
  int opcode = stmt.opcode;
  int target_reg, reg;

  if (opcode != OPCODE_JMP) {
    target_reg = get_register(stmt.args[0]);
    if (target_reg == -1)
      return ERROR_INVALID_REGISTER_TYPE;

    reg = get_register_index(stmt.args[0]);
    if (reg == -1)
      return ERROR_INVALID_REGISTER_NUMBER;
  }

  switch (opcode) {
    case OPCODE_MOV: {
      char *arg = stmt.args[1];

      // Check if arg is a register
      int target_reg2 = get_register(arg);
      if (target_reg2 == -1) {
        mov(target_reg, reg, arg);
        break;
      }

      int reg2 = get_register_index(arg);
      if (reg2 == -1)
        return ERROR_INVALID_REGISTER_NUMBER;

      if (target_reg != target_reg2)
        return ERROR_DIFFERENT_REGISTER_TYPE;

      mov_register(target_reg, reg, reg2);
    }
      break;
    case OPCODE_PRNT: case OPCODE_PRNTL: {
      int newline = (opcode == OPCODE_PRNTL)? 1 : 0;
      prnt(target_reg, reg, newline);
    }
      break;
    case OPCODE_ADD: case OPCODE_SUB:
    case OPCODE_MUL: case OPCODE_DIV: 
    case OPCODE_MOD: {
      int regsrc = get_register_index(stmt.args[1]);
      int ret = arithm(target_reg, reg, regsrc, opcode);
      if (ret != ERROR_NONE)
        return ret;
    }
      break;
    case OPCODE_ADD2: {
      int reg1 = get_register_index(stmt.args[1]);
      int reg2 = get_register_index(stmt.args[2]);
      add2(target_reg, reg, reg1, reg2);
    }
      break;
    case OPCODE_INCR:
    case OPCODE_DECR:
      incr(target_reg, reg, (opcode == OPCODE_INCR)? 1 : -1);
      break;
    case OPCODE_CLR:
      clear(target_reg, reg);
      break;
    case OPCODE_JMP: {
      char *labelname = stmt.args[0];
      int labelidx = find_label(&pg, labelname);
      if (labelidx == -1)
        return ERROR_UNKNOWN_LABEL;

      jmp(&pg, labelidx);
    }
      break;
    case OPCODE_JMPNZ: case OPCODE_JMPZ: {
      char *labelname = stmt.args[1];
      int labelidx = find_label(&pg, labelname);
      if (labelidx == -1)
        return ERROR_UNKNOWN_LABEL;

      int notzero = (opcode == OPCODE_JMPNZ)? 1 : 0;
      jmp_cond(&pg, target_reg, reg, labelidx, notzero);
    }
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
    case 's': sregisters[reg] = strdup(data); break;
    default: break;
  }
}

void mov_register(int target_reg, int reg, int reg2)
{
  switch (target_reg) {
    case 'i': iregisters[reg] = iregisters[reg2]; break;
    case 'f': fregisters[reg] = fregisters[reg2]; break;
    case 's': sregisters[reg] = strdup(sregisters[reg2]); break;
    default: break;
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
    default: break;
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
    case OPCODE_MOD:
      if (iregisters[reg_src] == 0)
        return ERROR_DIV_ZERO;
      iregisters[reg_dst] %= iregisters[reg_src];
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
    case OPCODE_MOD:
      if (fregisters[reg_src] == 0)
        return ERROR_DIV_ZERO;
      fregisters[reg_dst] = fmod(fregisters[reg_dst], fregisters[reg_src]);
      break;
    default: break;
  }

  return ERROR_NONE;
}

void add2(int target_reg, int reg, int reg1, int reg2)
{
  switch (target_reg) {
    case 'i': iregisters[reg] = iregisters[reg1] + iregisters[reg2]; break;
    case 'f': fregisters[reg] = fregisters[reg1] + fregisters[reg2]; break;
    default: break;
  }
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

void jmp(program *pg, int labelidx)
{
  label lb = pg->labels[labelidx];
  parse_statements(lb);
}

void jmp_cond(program *pg, int target_reg, int reg, int labelidx, int notzero)
{
#define jmpcond_condition(regs) (notzero? regs[reg] != 0 : regs[reg] == 0)
  switch (target_reg) {
    case 'i': if (jmpcond_condition(iregisters)) jmp(pg, labelidx);; break;
    case 'f': if (jmpcond_condition(fregisters)) jmp(pg, labelidx); break;
    case 's': if (jmpcond_condition(sregisters)) jmp(pg, labelidx); break;
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
    case ERROR_UNKNOWN_LABEL:
      printf("%s: unknown label\n", PROGRAM_NAME);
      break;
    case ERROR_DIFFERENT_REGISTER_TYPE:
      printf("%s: tried to move registers of different type\n", PROGRAM_NAME);
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
  for (int i = 0; i < pg->cap; ++i) {
    sl_free(pg->labels[i].statements);
    free(pg->labels[i].name);
  }

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
  int start_idx = find_label(pg, "start");
  if (start_idx == -1) {
    fail(ERROR_MISSING_START);
    return;
  }

  parse_statements(pg->labels[start_idx]);
}

int find_label(program *pg, char *name)
{
  for (int i = 0; i < pg->cap; ++i)
    if (pg->labels[i].name && streq(pg->labels[i].name, name))
      return i;

  return -1;
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

static void clear_registers(void)
{
  for (int i = 0; i < MAX_REGISTERS; ++i) {
    free(sregisters[i]);
    iregisters[i] = 0; fregisters[i] = 0.0;
  }
}
