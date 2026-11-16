/*
 * startrek_save.c - сохранение и загрузка игры Super Star Trek в файл saves.json.
 *
 * Подключается к игре (startrek_ru.c) тремя небольшими правками, см. startrek_port.h.
 * Модуль работает с глобальными переменными состояния игры (extern) и с терминалом
 * приложения (term_*). Файл saves.json лежит в папке с exe.
 *
 * Формат saves.json: массив контейнеров, по одному на капитана:
 *   [ { "Commander": "Имя", "version": 1, "state": { ... } }, ... ]
 * При сохранении контейнер с тем же именем перезаписывается, иначе добавляется новый.
 * При загрузке все поля проверяются на тип и допустимый диапазон, и только после
 * успешной проверки значения переносятся в переменные игры.
 */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Терминал приложения (main.cpp) */
int   term_printf(const char *fmt, ...);
char *term_gets(char *s);
char *term_load_text(const char *name);               /* содержимое файла или NULL */
int   term_store_text(const char *name, const char *text);   /* 1 - записано */

/* Состояние игры (определено в startrek_ru.c) */
extern int b3, b4, b5, b9, d0, d1, e, e0, g[9][9], k[4][4], k3, k7, k9, p, p0;
extern int q1, q2, s, s3, t0, t9, z[9][9];
extern double d[9], d4, s1, s2, t, w1;
extern char sQ[194];

#define NAME_CHARS   10                       /* длина имени капитана в символах */
#define NAME_BYTES   (NAME_CHARS * 4 + 1)     /* запас под UTF-8 */
#define SAVE_FILE    "saves.json"
#define SAVE_VERSION 1

char sCommander[NAME_BYTES];                  /* имя капитана (игрока), до 10 символов */

static int  g_oom = 0;                        /* не хватило памяти */
static char gErr[128];                        /* причина отказа при загрузке */

/* ======================= Буфер строки ======================= */

typedef struct { char *p; size_t n, cap; } sb;

static int sb_add(sb *b, const char *src, size_t len)
{
  if (b->n + len + 1 > b->cap)
    {
      size_t nc = b->cap ? b->cap : 64;
      char *np;
      while (nc < b->n + len + 1) nc *= 2;
      np = (char *)realloc(b->p, nc);
      if (!np) { g_oom = 1; return 0; }
      b->p = np;
      b->cap = nc;
    }
  memcpy(b->p + b->n, src, len);
  b->n += len;
  b->p[b->n] = '\0';
  return 1;
}

static void sb_ch(sb *b, char ch) { sb_add(b, &ch, 1); }

static void sb_indent(sb *b, int lvl)
{
  int i;
  for (i = 0; i < lvl; i++) sb_add(b, "  ", 2);
}

static char *dupstr(const char *src)
{
  size_t n = strlen(src);
  char *r = (char *)malloc(n + 1);
  if (r) memcpy(r, src, n + 1);
  return r;
}

/* ======================= Дерево JSON ======================= */

enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ };

typedef struct jv
{
  int type;
  double num;                    /* число или 0/1 для bool */
  char *str;
  struct jv **item;              /* элементы массива / значения объекта */
  char **key;                    /* ключи объекта (для массива NULL) */
  int n, cap;
} jv;

static jv *jnew(int type)
{
  jv *v = (jv *)calloc(1, sizeof(jv));
  if (v) v->type = type;
  return v;
}

static void jfree(jv *v)
{
  int i;
  if (!v) return;
  for (i = 0; i < v->n; i++)
    {
      jfree(v->item[i]);
      if (v->key) free(v->key[i]);
    }
  free(v->item);
  free(v->key);
  free(v->str);
  free(v);
}

static int jgrow(jv *v)
{
  int nc;
  jv **ni;
  char **nk;

  if (v->n < v->cap) return 1;
  nc = v->cap ? v->cap * 2 : 8;
  ni = (jv **)realloc(v->item, nc * sizeof(jv *));
  if (!ni) return 0;
  v->item = ni;
  nk = (char **)realloc(v->key, nc * sizeof(char *));
  if (!nk) return 0;
  v->key = nk;
  v->cap = nc;
  return 1;
}

/* Добавляет child в массив (key == NULL) или в объект; ключ объекта заменяется */
static jv *jadd(jv *par, const char *key, jv *child)
{
  int i;

  if (!par || !child) { g_oom = 1; jfree(child); return NULL; }

  if (key)
    for (i = 0; i < par->n; i++)
      if (par->key[i] && strcmp(par->key[i], key) == 0)
        {
          jfree(par->item[i]);
          par->item[i] = child;
          return child;
        }

  if (!jgrow(par)) { g_oom = 1; jfree(child); return NULL; }
  par->item[par->n] = child;
  par->key[par->n] = key ? dupstr(key) : NULL;
  par->n++;
  return child;
}

static jv *jnum(double v)
{
  jv *j = jnew(J_NUM);
  if (j) j->num = v;
  return j;
}

static jv *jstr(const char *s)
{
  jv *j = jnew(J_STR);
  if (j)
    {
      j->str = dupstr(s);
      if (!j->str) { jfree(j); j = NULL; }
    }
  return j;
}

static const jv *jget(const jv *obj, const char *key)
{
  int i;
  if (!obj || obj->type != J_OBJ) return NULL;
  for (i = 0; i < obj->n; i++)
    if (obj->key[i] && strcmp(obj->key[i], key) == 0)
      return obj->item[i];
  return NULL;
}

/* ======================= Разбор JSON ======================= */

typedef struct { const char *p; int err; } jp;

static void skipws(jp *c)
{
  while (*c->p == ' ' || *c->p == '\t' || *c->p == '\n' || *c->p == '\r') c->p++;
}

static int hex4(jp *c, unsigned *out)
{
  unsigned v = 0;
  int i;
  for (i = 0; i < 4; i++)
    {
      char ch = *c->p;
      v <<= 4;
      if (ch >= '0' && ch <= '9')      v |= (unsigned)(ch - '0');
      else if (ch >= 'a' && ch <= 'f') v |= (unsigned)(ch - 'a' + 10);
      else if (ch >= 'A' && ch <= 'F') v |= (unsigned)(ch - 'A' + 10);
      else return 0;
      c->p++;
    }
  *out = v;
  return 1;
}

static void put_utf8(sb *o, unsigned u)
{
  if (u < 0x80)
    sb_ch(o, (char)u);
  else if (u < 0x800)
    {
      sb_ch(o, (char)(0xC0 | (u >> 6)));
      sb_ch(o, (char)(0x80 | (u & 0x3F)));
    }
  else if (u < 0x10000)
    {
      sb_ch(o, (char)(0xE0 | (u >> 12)));
      sb_ch(o, (char)(0x80 | ((u >> 6) & 0x3F)));
      sb_ch(o, (char)(0x80 | (u & 0x3F)));
    }
  else
    {
      sb_ch(o, (char)(0xF0 | (u >> 18)));
      sb_ch(o, (char)(0x80 | ((u >> 12) & 0x3F)));
      sb_ch(o, (char)(0x80 | ((u >> 6) & 0x3F)));
      sb_ch(o, (char)(0x80 | (u & 0x3F)));
    }
}

/* c->p указывает на открывающую кавычку; возвращает строку (malloc) или NULL */
static char *pstring(jp *c)
{
  sb o = { 0, 0, 0 };
  unsigned u, lo;

  c->p++;
  for (;;)
    {
      unsigned char ch = (unsigned char)*c->p;

      if (ch == 0 || ch < 32) { c->err = 1; free(o.p); return NULL; }
      c->p++;
      if (ch == '"') break;
      if (ch != '\\') { sb_ch(&o, (char)ch); continue; }

      ch = (unsigned char)*c->p;
      if (ch == 0) { c->err = 1; free(o.p); return NULL; }
      c->p++;

      switch (ch)
        {
        case '"': case '\\': case '/': sb_ch(&o, (char)ch); break;
        case 'b': sb_ch(&o, '\b'); break;
        case 'f': sb_ch(&o, '\f'); break;
        case 'n': sb_ch(&o, '\n'); break;
        case 'r': sb_ch(&o, '\r'); break;
        case 't': sb_ch(&o, '\t'); break;
        case 'u':
          if (!hex4(c, &u)) { c->err = 1; free(o.p); return NULL; }
          if (u >= 0xD800 && u <= 0xDBFF)
            {
              if (c->p[0] == '\\' && c->p[1] == 'u')
                {
                  c->p += 2;
                  if (!hex4(c, &lo) || lo < 0xDC00 || lo > 0xDFFF)
                    { c->err = 1; free(o.p); return NULL; }
                  u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                }
              else
                { c->err = 1; free(o.p); return NULL; }
            }
          else if (u >= 0xDC00 && u <= 0xDFFF)
            { c->err = 1; free(o.p); return NULL; }
          put_utf8(&o, u);
          break;
        default:
          c->err = 1; free(o.p); return NULL;
        }
    }

  sb_add(&o, "", 0);                       /* гарантирует выделение и завершающий 0 */
  return o.p;
}

static jv *pnumber(jp *c)
{
  const char *st = c->p, *q = c->p;
  char tmp[64];
  size_t len;
  jv *v;

  /* грамматика JSON: -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)? */
  if (*q == '-') q++;
  if (*q == '0') q++;
  else if (*q >= '1' && *q <= '9') { while (*q >= '0' && *q <= '9') q++; }
  else { c->err = 1; return NULL; }
  if (*q == '.')
    {
      q++;
      if (!(*q >= '0' && *q <= '9')) { c->err = 1; return NULL; }
      while (*q >= '0' && *q <= '9') q++;
    }
  if (*q == 'e' || *q == 'E')
    {
      q++;
      if (*q == '+' || *q == '-') q++;
      if (!(*q >= '0' && *q <= '9')) { c->err = 1; return NULL; }
      while (*q >= '0' && *q <= '9') q++;
    }

  len = (size_t)(q - st);
  if (len >= sizeof(tmp)) { c->err = 1; return NULL; }
  memcpy(tmp, st, len);
  tmp[len] = '\0';
  c->p = q;

  v = jnew(J_NUM);
  if (!v) { g_oom = 1; c->err = 1; return NULL; }
  v->num = strtod(tmp, NULL);
  return v;
}

static jv *pvalue(jp *c, int depth)
{
  jv *v, *child;
  char *key, *text;

  if (depth > 20) { c->err = 1; return NULL; }
  skipws(c);

  if (*c->p == '{' || *c->p == '[')
    {
      int isobj = (*c->p == '{');
      char close = isobj ? '}' : ']';

      v = jnew(isobj ? J_OBJ : J_ARR);
      if (!v) { g_oom = 1; c->err = 1; return NULL; }
      c->p++;
      skipws(c);
      if (*c->p == close) { c->p++; return v; }

      for (;;)
        {
          key = NULL;
          skipws(c);
          if (isobj)
            {
              if (*c->p != '"') { c->err = 1; jfree(v); return NULL; }
              key = pstring(c);
              if (!key) { jfree(v); return NULL; }
              skipws(c);
              if (*c->p != ':') { c->err = 1; free(key); jfree(v); return NULL; }
              c->p++;
            }
          child = pvalue(c, depth + 1);
          if (!child) { free(key); jfree(v); return NULL; }
          if (!jadd(v, key, child)) { c->err = 1; free(key); jfree(v); return NULL; }
          free(key);

          skipws(c);
          if (*c->p == ',') { c->p++; continue; }
          if (*c->p == close) { c->p++; return v; }
          c->err = 1; jfree(v); return NULL;
        }
    }

  if (*c->p == '"')
    {
      text = pstring(c);
      if (!text) return NULL;
      v = jnew(J_STR);
      if (!v) { g_oom = 1; free(text); c->err = 1; return NULL; }
      v->str = text;
      return v;
    }

  if (!strncmp(c->p, "true", 4))  { c->p += 4; v = jnew(J_BOOL); if (v) v->num = 1; return v; }
  if (!strncmp(c->p, "false", 5)) { c->p += 5; return jnew(J_BOOL); }
  if (!strncmp(c->p, "null", 4))  { c->p += 4; return jnew(J_NULL); }

  return pnumber(c);
}

/* Разбирает весь текст; NULL при любой ошибке или лишних символах в конце */
static jv *parse_text(const char *text)
{
  jp c;
  jv *v;

  c.p = text;
  c.err = 0;
  if ((unsigned char)c.p[0] == 0xEF && (unsigned char)c.p[1] == 0xBB && (unsigned char)c.p[2] == 0xBF)
    c.p += 3;                                  /* BOM */

  v = pvalue(&c, 0);
  if (!v || c.err) { jfree(v); return NULL; }
  skipws(&c);
  if (*c.p != '\0') { jfree(v); return NULL; }
  return v;
}

/* ======================= Запись JSON ======================= */

static void jwrite_str(sb *o, const char *s)
{
  char tmp[8];

  sb_ch(o, '"');
  for (; *s; s++)
    {
      unsigned char ch = (unsigned char)*s;
      if (ch == '"')       sb_add(o, "\\\"", 2);
      else if (ch == '\\') sb_add(o, "\\\\", 2);
      else if (ch == '\n') sb_add(o, "\\n", 2);
      else if (ch == '\r') sb_add(o, "\\r", 2);
      else if (ch == '\t') sb_add(o, "\\t", 2);
      else if (ch < 32)
        {
          sprintf(tmp, "\\u%04x", ch);
          sb_add(o, tmp, 6);
        }
      else
        sb_ch(o, (char)ch);
    }
  sb_ch(o, '"');
}

static int is_num_array(const jv *v)
{
  int i;
  if (v->type != J_ARR) return 0;
  for (i = 0; i < v->n; i++)
    if (v->item[i]->type != J_NUM) return 0;
  return 1;
}

static void jwrite(sb *o, const jv *v, int lvl)
{
  char tmp[40];
  int i;

  switch (v->type)
    {
    case J_NULL: sb_add(o, "null", 4); break;
    case J_BOOL: if (v->num != 0) sb_add(o, "true", 4); else sb_add(o, "false", 5); break;
    case J_NUM:
      if (v->num == 0)
        sprintf(tmp, "0");
      else if (v->num == floor(v->num) && fabs(v->num) < 1e15)
        sprintf(tmp, "%.0f", v->num);
      else
        sprintf(tmp, "%.17g", v->num);
      sb_add(o, tmp, strlen(tmp));
      break;
    case J_STR: jwrite_str(o, v->str ? v->str : ""); break;
    case J_ARR:
      if (v->n == 0) { sb_add(o, "[]", 2); break; }
      if (is_num_array(v))
        {
          sb_ch(o, '[');
          for (i = 0; i < v->n; i++)
            {
              if (i) sb_add(o, ", ", 2);
              jwrite(o, v->item[i], lvl + 1);
            }
          sb_ch(o, ']');
          break;
        }
      sb_add(o, "[\n", 2);
      for (i = 0; i < v->n; i++)
        {
          sb_indent(o, lvl + 1);
          jwrite(o, v->item[i], lvl + 1);
          sb_add(o, (i < v->n - 1) ? ",\n" : "\n", (i < v->n - 1) ? 2 : 1);
        }
      sb_indent(o, lvl);
      sb_ch(o, ']');
      break;
    case J_OBJ:
      if (v->n == 0) { sb_add(o, "{}", 2); break; }
      sb_add(o, "{\n", 2);
      for (i = 0; i < v->n; i++)
        {
          sb_indent(o, lvl + 1);
          jwrite_str(o, v->key[i] ? v->key[i] : "");
          sb_add(o, ": ", 2);
          jwrite(o, v->item[i], lvl + 1);
          sb_add(o, (i < v->n - 1) ? ",\n" : "\n", (i < v->n - 1) ? 2 : 1);
        }
      sb_indent(o, lvl);
      sb_ch(o, '}');
      break;
    }
}

/* ======================= Имя капитана и вопросы ======================= */

/* Убирает пробелы по краям и управляющие символы, оставляет не более NAME_CHARS символов.
   Возвращает 1, если имя пришлось сократить. */
static int norm_name(char *str)
{
  char out[NAME_BYTES];
  size_t o = 0;
  int chars = 0, cut = 0;
  const unsigned char *src = (const unsigned char *)str;

  while (*src == ' ') src++;
  for (; *src; src++)
    {
      if (*src < 32 || *src == 127) continue;
      if ((*src & 0xC0) != 0x80)                 /* начало нового символа UTF-8 */
        {
          if (chars == NAME_CHARS) { cut = 1; break; }
          chars++;
        }
      if (o < NAME_BYTES - 1) out[o++] = (char)*src;
    }
  while (o > 0 && out[o - 1] == ' ') o--;
  out[o] = '\0';
  strcpy(str, out);
  return cut;
}

static void ask_name(void)
{
  char line[255];

  for (;;)
    {
      term_printf("Введите имя капитана (до %d символов): ", NAME_CHARS);
      term_gets(line);
      if (norm_name(line))
        term_printf("Имя сокращено до %d символов: %s\n", NAME_CHARS, line);
      if (line[0]) break;
      term_printf("Имя не может быть пустым.\n");
    }
  strcpy(sCommander, line);
  term_printf("\n");
}

static int ask_yes_no(const char *prompt)
{
  char line[255];

  term_printf("%s", prompt);
  term_gets(line);
  term_printf("\n");
  return line[0] == 'y' || line[0] == 'Y' ||
         strncmp(line, "д", 2) == 0 || strncmp(line, "Д", 2) == 0;
}

/* ======================= Проверка данных при загрузке ======================= */

static int fail_msg(const char *text)
{
  snprintf(gErr, sizeof(gErr), "%s", text);
  return 0;
}

static int is_fin(double v) { return v == v && v - v == 0.0; }

static int need_int(const jv *o, const char *key, int lo, int hi, int *out)
{
  const jv *v = jget(o, key);

  if (!v || v->type != J_NUM)
    { snprintf(gErr, sizeof(gErr), "поле '%s': ожидается число", key); return 0; }
  if (!is_fin(v->num) || v->num != floor(v->num))
    { snprintf(gErr, sizeof(gErr), "поле '%s': ожидается целое число", key); return 0; }
  if (v->num < lo || v->num > hi)
    { snprintf(gErr, sizeof(gErr), "поле '%s': значение вне диапазона %d..%d", key, lo, hi); return 0; }
  *out = (int)v->num;
  return 1;
}

static int need_dbl(const jv *o, const char *key, double lo, double hi, double *out)
{
  const jv *v = jget(o, key);

  if (!v || v->type != J_NUM)
    { snprintf(gErr, sizeof(gErr), "поле '%s': ожидается число", key); return 0; }
  if (!is_fin(v->num) || v->num < lo || v->num > hi)
    { snprintf(gErr, sizeof(gErr), "поле '%s': значение вне допустимого диапазона", key); return 0; }
  *out = v->num;
  return 1;
}

/* Массив из 8 чисел (damage) */
static int need_vec8(const jv *o, const char *key, double lo, double hi, double *out)
{
  const jv *a = jget(o, key);
  int i;

  if (!a || a->type != J_ARR || a->n != 8)
    { snprintf(gErr, sizeof(gErr), "поле '%s': ожидается массив из 8 чисел", key); return 0; }
  for (i = 0; i < 8; i++)
    {
      if (a->item[i]->type != J_NUM || !is_fin(a->item[i]->num) ||
          a->item[i]->num < lo || a->item[i]->num > hi)
        { snprintf(gErr, sizeof(gErr), "поле '%s'[%d]: недопустимое значение", key, i + 1); return 0; }
      out[i] = a->item[i]->num;
    }
  return 1;
}

/* Таблица 8x8 целых кодов KBS: 0..399, десятки (звёздные базы) не больше 1 */
static int need_grid(const jv *o, const char *key, int out[8][8])
{
  const jv *a = jget(o, key);
  int i, j;

  if (!a || a->type != J_ARR || a->n != 8)
    { snprintf(gErr, sizeof(gErr), "поле '%s': ожидается таблица 8x8", key); return 0; }
  for (i = 0; i < 8; i++)
    {
      const jv *row = a->item[i];
      if (row->type != J_ARR || row->n != 8)
        { snprintf(gErr, sizeof(gErr), "поле '%s': строка %d не из 8 чисел", key, i + 1); return 0; }
      for (j = 0; j < 8; j++)
        {
          double v;
          if (row->item[j]->type != J_NUM) { v = -1; }
          else v = row->item[j]->num;
          if (!is_fin(v) || v != floor(v) || v < 0 || v > 399 || ((int)v % 100) / 10 > 1)
            {
              snprintf(gErr, sizeof(gErr), "поле '%s': недопустимый код в строке %d, столбце %d",
                       key, i + 1, j + 1);
              return 0;
            }
          out[i][j] = (int)v;
        }
    }
  return 1;
}

/* Карта квадранта: 8 строк по 24 символа (8 ячеек по 3) */
static int need_quadrant(const jv *o, const char *key, char *out)
{
  const jv *a = jget(o, key);
  int i;

  if (!a || a->type != J_ARR || a->n != 8)
    { snprintf(gErr, sizeof(gErr), "поле '%s': ожидается 8 строк", key); return 0; }
  for (i = 0; i < 8; i++)
    {
      const jv *row = a->item[i];
      if (row->type != J_STR || !row->str || strlen(row->str) != 24)
        { snprintf(gErr, sizeof(gErr), "поле '%s': строка %d должна быть из 24 символов", key, i + 1); return 0; }
      memcpy(out + i * 24, row->str, 24);
    }
  out[192] = ' ';
  out[193] = '\0';
  return 1;
}

/* Проверяет все данные контейнера и только при успехе переносит их в переменные игры */
static int validate_and_apply(const jv *cont)
{
  const jv *st;
  int ver, iT0, iT9, iE, iS, iP, iD0, iD1, iQ1, iQ2, iK7, iK9, iB9, iK3, iB3, iB4, iB5, iS3;
  int iG[8][8], iZ[8][8];
  int i, j, ci, nEnt = 0, nBase = 0, nStar = 0;
  double dT, dS1, dS2, dD4, dW1, dDam[8];
  char cQ[194];
  char cell[4];

  if (!cont || cont->type != J_OBJ) return fail_msg("контейнер не является объектом");
  if (!need_int(cont, "version", 1, SAVE_VERSION, &ver)) return 0;
  st = jget(cont, "state");
  if (!st || st->type != J_OBJ) return fail_msg("нет раздела 'state'");

  if (!need_dbl(st, "stardate", 0.0, 1000000.0, &dT) ||
      !need_int(st, "start_stardate", 0, 1000000, &iT0) ||
      !need_int(st, "mission_days", 1, 1000, &iT9) ||
      !need_int(st, "energy", 0, e0, &iE) ||
      !need_int(st, "shields", 0, e0, &iS) ||
      !need_int(st, "torpedoes", 0, p0, &iP) ||
      !need_int(st, "docked", 0, 1, &iD0) ||
      !need_int(st, "repair_flag", 0, 1, &iD1) ||
      !need_int(st, "quadrant_row", 1, 8, &iQ1) ||
      !need_int(st, "quadrant_col", 1, 8, &iQ2) ||
      !need_dbl(st, "sector_row", 1.0, 8.9999, &dS1) ||
      !need_dbl(st, "sector_col", 1.0, 8.9999, &dS2) ||
      !need_int(st, "klingons_start", 1, 192, &iK7) ||
      !need_int(st, "klingons_left", 1, 192, &iK9) ||
      !need_int(st, "starbases_total", 0, 64, &iB9) ||
      !need_int(st, "klingons_here", 0, 3, &iK3) ||
      !need_int(st, "starbase_here", 0, 1, &iB3) ||
      !need_int(st, "starbase_row", 0, 8, &iB4) ||
      !need_int(st, "starbase_col", 0, 8, &iB5) ||
      !need_int(st, "stars_here", 0, 8, &iS3) ||
      !need_dbl(st, "repair_time", 0.0, 1.0, &dD4) ||
      !need_dbl(st, "warp", -1000000.0, 1000000.0, &dW1) ||
      !need_vec8(st, "damage", -10000.0, 1000.0, dDam) ||
      !need_grid(st, "galaxy", iG) ||
      !need_grid(st, "computer_memory", iZ) ||
      !need_quadrant(st, "quadrant", cQ))
    return 0;

  /* --- согласованность значений --- */
  if (dT < iT0 || dT > iT0 + iT9)    return fail_msg("звёздная дата вне срока миссии");
  if (iE + iS > e0)                  return fail_msg("энергия и щиты вместе превышают максимум");
  if (iK9 > iK7)                     return fail_msg("клингонов осталось больше, чем было");
  if (iK3 != 0)                      return fail_msg("в сохранённом квадранте есть клингоны");
  if (iG[iQ1 - 1][iQ2 - 1] / 100 != 0 ||
      (iG[iQ1 - 1][iQ2 - 1] / 10) % 10 != iB3 ||
      iG[iQ1 - 1][iQ2 - 1] % 10 != iS3)
    return fail_msg("код квадранта в галактике не совпадает с его содержимым");
  if (iB3 == 1 && (iB4 < 1 || iB5 < 1)) return fail_msg("не указано место звёздной базы");

  /* --- карта квадранта: допустимые объекты и их количество --- */
  for (ci = 0; ci < 64; ci++)
    {
      memcpy(cell, cQ + ci * 3, 3);
      cell[3] = '\0';
      if (!strcmp(cell, "<*>"))      nEnt++;
      else if (!strcmp(cell, ">!<")) nBase++;
      else if (!strcmp(cell, " * ")) nStar++;
      else if (strcmp(cell, "   "))  return fail_msg("в карте квадранта недопустимый объект");
    }
  if (nEnt != 1 || nBase != iB3 || nStar != iS3)
    return fail_msg("карта квадранта не совпадает с числом объектов");
  if (strncmp(cQ + (((int)dS1 - 1) * 8 + ((int)dS2 - 1)) * 3, "<*>", 3))
    return fail_msg("«Энтерпрайз» стоит не в своём секторе");
  if (iB3 == 1 && strncmp(cQ + ((iB4 - 1) * 8 + (iB5 - 1)) * 3, ">!<", 3))
    return fail_msg("звёздная база стоит не в указанном секторе");

  /* --- всё корректно: переносим в переменные игры --- */
  t = dT;       t0 = iT0;     t9 = iT9;
  e = iE;       s = iS;       p = iP;
  d0 = iD0;     d1 = iD1;
  q1 = iQ1;     q2 = iQ2;     s1 = dS1;    s2 = dS2;
  k7 = iK7;     k9 = iK9;     b9 = iB9;
  k3 = iK3;     b3 = iB3;     b4 = iB4;    b5 = iB5;     s3 = iS3;
  d4 = dD4;     w1 = dW1;
  for (i = 0; i < 8; i++)
    {
      d[i + 1] = dDam[i];
      for (j = 0; j < 8; j++)
        {
          g[i + 1][j + 1] = iG[i][j];
          z[i + 1][j + 1] = iZ[i][j];
        }
    }
  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      k[i][j] = 0;
  memcpy(sQ, cQ, 194);
  return 1;
}

/* ======================= Интерфейс для игры ======================= */

static const jv *find_container(const jv *root, const char *name)
{
  int i;
  if (!root || root->type != J_ARR) return NULL;
  for (i = 0; i < root->n; i++)
    {
      const jv *c = jget(root->item[i], "Commander");
      if (c && c->type == J_STR && c->str && !strcmp(c->str, name))
        return root->item[i];
    }
  return NULL;
}

/*
 * Вызывается в начале новой игры: спрашивает имя капитана (один раз за запуск),
 * затем предлагает загрузить сохранение, если в saves.json есть контейнер
 * Commander с этим именем. Возвращает 1, если игра загружена (инициализация
 * не нужна), иначе 0.
 */
int
st_session_start(void)
{
  char *text;
  jv *root = NULL;
  const jv *cont = NULL;
  int loaded = 0;

  if (!sCommander[0])
    ask_name();

  text = term_load_text(SAVE_FILE);
  if (text)
    {
      root = parse_text(text);
      free(text);
      if (!root || root->type != J_ARR)
        term_printf("Файл %s повреждён и будет пропущен.\n\n", SAVE_FILE);
      else
        cont = find_container(root, sCommander);
    }

  if (cont && ask_yes_no("Загрузить сохранённую игру? (Y/N): "))
    {
      if (validate_and_apply(cont))
        {
          term_printf("Игра загружена. Капитан %s, звёздная дата %d.\n\n", sCommander, (int)t);
          loaded = 1;
        }
      else
        {
          term_printf("Ошибка загрузки: %s.\n", gErr);
          term_printf("Сохранение пропущено, начинается новая миссия.\n\n");
        }
    }

  jfree(root);
  return loaded;
}

/* Собирает контейнер Commander с текущим состоянием игры */
static jv *build_container(void)
{
  jv *c, *st, *arr, *row;
  char rowbuf[25];
  int i, j;

  c = jnew(J_OBJ);
  st = jnew(J_OBJ);
  if (!c || !st) { jfree(c); jfree(st); return NULL; }

  jadd(c, "Commander", jstr(sCommander));
  jadd(c, "version", jnum(SAVE_VERSION));
  jadd(c, "state", st);

  jadd(st, "stardate", jnum(t));
  jadd(st, "start_stardate", jnum(t0));
  jadd(st, "mission_days", jnum(t9));
  jadd(st, "energy", jnum(e));
  jadd(st, "shields", jnum(s));
  jadd(st, "torpedoes", jnum(p));
  jadd(st, "docked", jnum(d0));
  jadd(st, "repair_flag", jnum(d1));
  jadd(st, "quadrant_row", jnum(q1));
  jadd(st, "quadrant_col", jnum(q2));
  jadd(st, "sector_row", jnum(s1));
  jadd(st, "sector_col", jnum(s2));
  jadd(st, "klingons_start", jnum(k7));
  jadd(st, "klingons_left", jnum(k9));
  jadd(st, "starbases_total", jnum(b9));
  jadd(st, "klingons_here", jnum(k3));
  jadd(st, "starbase_here", jnum(b3));
  jadd(st, "starbase_row", jnum(b4));
  jadd(st, "starbase_col", jnum(b5));
  jadd(st, "stars_here", jnum(s3));
  jadd(st, "repair_time", jnum(d4));
  jadd(st, "warp", jnum(w1));

  arr = jadd(st, "damage", jnew(J_ARR));
  for (i = 1; i <= 8; i++)
    jadd(arr, NULL, jnum(d[i]));

  arr = jadd(st, "galaxy", jnew(J_ARR));
  for (i = 1; i <= 8; i++)
    {
      row = jadd(arr, NULL, jnew(J_ARR));
      for (j = 1; j <= 8; j++)
        jadd(row, NULL, jnum(g[i][j]));
    }

  arr = jadd(st, "computer_memory", jnew(J_ARR));
  for (i = 1; i <= 8; i++)
    {
      row = jadd(arr, NULL, jnew(J_ARR));
      for (j = 1; j <= 8; j++)
        jadd(row, NULL, jnum(z[i][j]));
    }

  arr = jadd(st, "quadrant", jnew(J_ARR));
  for (i = 0; i < 8; i++)
    {
      memcpy(rowbuf, sQ + i * 24, 24);
      rowbuf[24] = '\0';
      jadd(arr, NULL, jstr(rowbuf));
    }

  if (g_oom) { jfree(c); return NULL; }
  return c;
}

/* Команда sav: сохраняет игру в saves.json (только в квадранте без клингонов) */
void
st_save_command(void)
{
  jv *cont, *root = NULL;
  char *old, *text;
  sb out = { 0, 0, 0 };
  int i, found = 0, ok;

  if (k3 > 0)
    {
      term_printf("Сохранение невозможно: в квадранте есть клингонские корабли.\n");
      term_printf("Уничтожьте их или перелетите в другой квадрант.\n\n");
      return;
    }

  if (!sCommander[0])
    ask_name();

  /* значения должны быть конечными числами, иначе JSON получится неверным */
  if (!is_fin(t) || !is_fin(s1) || !is_fin(s2) || !is_fin(d4) || !is_fin(w1))
    { term_printf("Сохранение невозможно: повреждены данные игры.\n\n"); return; }
  for (i = 1; i <= 8; i++)
    if (!is_fin(d[i]))
      { term_printf("Сохранение невозможно: повреждены данные игры.\n\n"); return; }

  g_oom = 0;
  cont = build_container();
  if (!cont)
    { term_printf("Сохранение невозможно: не хватает памяти.\n\n"); return; }

  /* читаем имеющийся файл, чтобы не потерять контейнеры других капитанов */
  old = term_load_text(SAVE_FILE);
  if (old)
    {
      root = parse_text(old);
      if (!root || root->type != J_ARR)
        {
          jfree(root);
          root = NULL;
          term_store_text(SAVE_FILE ".bak", old);      /* копия повреждённого файла */
          term_printf("Файл %s был повреждён: копия сохранена как %s.bak.\n", SAVE_FILE, SAVE_FILE);
        }
      free(old);
    }
  if (!root)
    root = jnew(J_ARR);
  if (!root) { jfree(cont); term_printf("Сохранение невозможно: не хватает памяти.\n\n"); return; }

  /* перезаписываем контейнер этого капитана или добавляем новый */
  for (i = 0; i < root->n && !found; i++)
    {
      const jv *c = jget(root->item[i], "Commander");
      if (c && c->type == J_STR && c->str && !strcmp(c->str, sCommander))
        {
          jfree(root->item[i]);
          root->item[i] = cont;
          found = 1;
        }
    }
  if (!found && !jadd(root, NULL, cont))
    { jfree(root); term_printf("Сохранение невозможно: не хватает памяти.\n\n"); return; }

  g_oom = 0;
  jwrite(&out, root, 0);
  sb_ch(&out, '\n');
  text = out.p;
  ok = (text && !g_oom) ? term_store_text(SAVE_FILE, text) : 0;
  free(text);
  jfree(root);

  if (ok)
    term_printf("Игра сохранена: капитан %s, звёздная дата %d.\n\n", sCommander, (int)t);
  else
    term_printf("Не удалось записать файл %s (нет доступа к папке программы?).\n\n", SAVE_FILE);
}
