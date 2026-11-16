/*
 * startrek.c
 *
 * Super Star Trek Classic (v1.1)
 * Ретро-игра «Звёздный путь»
 * Порт на C, авторские права (C) 1996  <Chris Nystrom>
 * 
 * Эта программа является свободным программным обеспечением; вы можете
 * распространять и изменять её любым угодным вам способом. _Star Trek_ —
 * торговая марка Paramount, насколько мне известно.
 * 
 * Программа распространяется в надежде, что она будет полезной,
 * но БЕЗ КАКИХ-ЛИБО ГАРАНТИЙ; даже без подразумеваемых гарантий
 * ТОВАРНОГО СОСТОЯНИЯ или ПРИГОДНОСТИ ДЛЯ КОНКРЕТНЫХ ЦЕЛЕЙ.
 *
 * Это порт на C старой программы на BASIC: классической игры Super Star Trek.
 * Она взята из книги _BASIC Computer Games_ под редакцией Дэвида Ала
 * (David Ahl) из Creative Computing. Книга издана в 1978 году издательством
 * Workman Publishing, 1 West 39 Street, New York, New York, ISBN: 0-89489-052-3.
 * 
 * Дополнительная информация: http://www.cactus.org/~nystrom/startrek.html
 *
 * Связаться с автором порта на C:
 *
 * Chris Nystrom
 * 1013 Prairie Dove Circle
 * Austin, Texas  78758
 *
 * E-Mail: cnystrom@gmail.com, nystrom@cactus.org
 *
 * Проблемы преобразования BASIC -> C
 *
 *     - Имена строк изменены с A$ на sA
 *     - Массивы изменены с G(8,8) на g[9][9], чтобы индексы
 *       могли остаться прежними.
 *
 * Ниже приведён оригинальный заголовок на BASIC:
 *
 * SUPER STARTREK - MAY 16, 1978 - REQUIRES 24K MEMORY
 *
 ***        **** STAR TREK ****        ****
 *** SIMULATION OF A MISSION OF THE STARSHIP ENTERPRISE,
 *** AS SEEN ON THE STAR TREK TV SHOW.
 *** ORIGINAL PROGRAM BY MIKE MAYFIELD, MODIFIED VERSION
 *** PUBLISHED IN DEC'S "101 BASIC GAMES", BY DAVE AHL.
 *** MODIFICATIONS TO THE LATTER (PLUS DEBUGGING) BY BOB
 *** LEEDOM - APRIL & DECEMBER 1974,
 *** WITH A LITTLE HELP FROM HIS FRIENDS . . .
 *** COMMENTS, EPITHETS, AND SUGGESTIONS SOLICITED --
 *** SEND TO:  R. C. LEEDOM
 ***           WESTINGHOUSE DEFENSE & ELECTRONICS SYSTEMS CNTR.
 ***           BOX 746, M.S. 338
 ***           BALTIMORE, MD  21203
 ***
 *** CONVERTED TO MICROSOFT 8 K BASIC 3/16/78 BY JOHN BORDERS
 *** LINE NUMBERS FROM VERSION STREK7 OF 1/12/75 PRESERVED AS
 *** MUCH AS POSSIBLE WHILE USING MULTIPLE STATMENTS PER LINE
 *
 * Примечание по русской локализации: исходный файл и все сообщения
 * в кодировке UTF-8; терминал тоже должен работать в UTF-8.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "startrek_port.h"

#ifndef FALSE
#define FALSE        0
#endif
 
#ifndef TRUE
#define TRUE         ! FALSE
#endif

/* Стандартная длина строки */
 
#define MAXLEN     255
 
/* Стандартные размеры терминала */
 
#define MAXROW      24
#define MAXCOL      80
 
/* Стандартный размер страницы */
 
#define MAXLINES    66
 
/* Полезные определения типов */
 
typedef int bool;
typedef char line[MAXCOL];
typedef char string[MAXLEN];

/*
 * ======================================================================
 *  ОБЗОР СТРУКТУРЫ ПРОГРАММЫ
 * ======================================================================
 *
 *  Модель мира:
 *   - Галактика 8x8 квадрантов (массив g), в каждом квадранте сетка
 *     секторов 8x8. Содержимое квадранта закодировано трёхзначным числом
 *     KBS: сотни = клингоны (0..3), десятки = звёздные базы (0..1),
 *     единицы = звёзды (1..8). Пример: 207 = 2 клингона, 0 баз, 7 звёзд.
 *   - Текущий квадрант хранится как строка sQ из 8x8 ячеек по 3 символа
 *     ("<*>", "+K+", ">!<", " * ", "   ").
 *   - Массив z — «память компьютера»: то, что игрок уже видел сканами.
 *   - Массив d[1..8] — состояние устройств (отрицательное = повреждено).
 *
 *  Функциональные блоки (в порядке следования в файле):
 *    1. Точка входа и главный игровой цикл     (main, intro, new_game)
 *    2. Инициализация игры и квадранта         (initialize, new_quadrant)
 *    3. Навигация и расход энергии             (course_control ... maneuver_energy)
 *    4. Датчики                                (short_range_scan, long_range_scan)
 *    5. Бой: оружие игрока                     (phaser_control, photon_torpedoes, torpedo_hit)
 *    6. Системы корабля                        (damage_control, sheild_control)
 *    7. Библиотека-компьютер                   (library_computer ... sub2)
 *    8. Завершение игры                        (ship_destroyed ... end_of_game)
 *    9. Бой: действия клингонов                (klingons_move, klingons_shoot)
 *   10. Повреждения и ремонт                   (repair_damage)
 *   11. Вспомогательные функции игры           (find_empty_place ... cint)
 *   12. Ввод-вывод, ГСЧ и служебные функции    (showfile ... utf8_strlen)
 */

/* Объявления функций */

void intro(void);
void new_game(void);
void initialize(void);
void new_quadrant(void);
void course_control(void);
void complete_maneuver(void);
void exceed_quadrant_limits(void);
void maneuver_energy(void);
void short_range_scan(void);
void long_range_scan(void);
void phaser_control(void);
void photon_torpedoes(void);
void torpedo_hit(void);
void damage_control(void);
void sheild_control(void);
void library_computer(void);
void galactic_record(void);
void status_report(void);
void torpedo_data(void);
void nav_data(void);
void dirdist_calc(void);
void galaxy_map(void);
void end_of_time(void);
void resign_commision(void);
void won_game(void);
void end_of_game(void);
void klingons_move(void);
void klingons_shoot(void);
void repair_damage(void);
void find_empty_place(void);
void insert_in_quadrant(void);
void get_device_name(void);
void string_compare(void);
void quadrant_name(void);
int function_d(int i);
int function_r(void);
void mid_str(char *a, char *b, int x, int y);
int cint(double d);
void compute_vector(void);
void sub1(void);
void sub2(void);
void showfile(char *filename);
int openfile(char * sFilename, char * sMode);
void closefile(void);
int getline(char *s);
void randomize(void);
int get_rand(int iSpread);
double rnd(void);
int utf8_strlen(const char *s);

/*
 * Глобальные переменные. Имена унаследованы от BASIC, поэтому
 * краткая «расшифровка» по группам:
 *   - Галактика и квадрант:  g, z, g5, k, k3, k7, k9, b3..b5, b9, s3, sQ, s8
 *   - Звездолёт:             e, e0, s, p, p0, q1, q2, s1, s2, d, d0, d1, d4, sC
 *   - Время:                 t, t0, t9
 *   - Навигация и векторы:   c, n, w1, x, y, x1, x2, a, c1
 *   - Временные/рабочие:     r1, r2, z1..z5, sA, sG2
 *   - Файловый ввод-вывод:   stream, bFlag
 */

int b3;                      /* Звёздные базы в квадранте */
int b4, b5;           /* Положение звёздной базы в секторе */
int b9;                        /* Всего звёздных баз */

/* @@@ int c[2][10] = */  /* Используется для положения и движения */         
int c[3][10] = /* изменено под индексы MS BASIC */
   {
     { 0 },
     { 0, 0, -1, -1, -1,  0,  1, 1, 1, 0 },
     { 1, 1,  1,  0, -1, -1, -1, 0, 1, 1 }
   };

int d0;                                 /* Флаг стыковки */
int d1;                       /* Флаг ремонта повреждений */
int e;                                 /* Текущая энергия */
int e0 = 3000;                        /* Начальная энергия */
int g[9][9];                                   /* Галактика */
int g5;                       /* Флаг названия квадранта */
int k[4][4];                          /* Данные о клингонах */
int k3;                       /* Клингонов в квадранте */
int k7;                            /* Клингонов в начале */
int k9;                         /* Всего осталось клингонов */
int n;                    /* Количество секторов для перелёта */
int p;                       /* Осталось фотонных торпед */
int p0 = 10;                   /* Ёмкость фотонных торпед */
int q1, q2;      /* Положение «Энтерпрайза» в квадрантах */
int r1, r2;           /* Временные координаты положения */
int s;                         /* Текущее значение щитов */
int s3;                              /* Звёзд в квадранте */
int s8;                       /* Индекс позиции в квадранте */
int s9 = 200;                         /* Мощность клингонов */
int t0;                          /* Начальная звёздная дата */
int t9;                                  /* Конец времени */
int z[9][9];            /* Накопительная запись о галактике */
int z3;           /* Возвращаемое значение string_compare */
int z1, z2;               /* Временные координаты сектора */
int z4, z5;             /* Временные координаты квадранта */

double a, c1;          /* Используются библиотекой-компьютером */
double d[9];                       /* Массив повреждений */
double d4;     /* Используется для расчёта времени ремонта */
double s1, s2;  /* Текущее положение «Энтерпрайза» в секторе */
double t;                         /* Текущая звёздная дата */
double w1;                                /* Варп-фактор */
double x, y, x1, x2;         /* Навигационные координаты */

char sA[4];                    /* Объект в секторе */
char sC[24];                           /* Состояние (UTF-8) */
char sQ[194];            /* Визуальное изображение квадранта */

string sG2;           /* Используется для передачи строк-результатов */

FILE *stream;
bool bFlag = FALSE;   /* Предотвращает многократное открытие файла */

/*
 * ======================================================================
 *  БЛОК 1. Точка входа и главный игровой цикл
 *  Показывает заставку, при желании — инструкцию, затем запускает
 *  игру и в бесконечном цикле читает и диспетчеризует команды.
 * ======================================================================
 */

/* main: запускает заставку и игру */
int
main(void)
{
  intro();

  new_game();

  /* @@@ exit(0);  */ /* вызывает предупреждение в C++ */
  return(0);
}

/*
 * intro: заставка, предложение показать инструкцию, рисунок «Энтерпрайза»,
 * инициализация ГСЧ и выбор случайной начальной звёздной даты
 * (от 2100 до 3900 с шагом 100).
 */
void
intro(void)
{
  string sTemp;

  printf ("\n\n");
  printf (" *************************************\n");
  printf (" *                                   *\n");
  printf (" *                                   *\n");
  printf (" *      * * Super Star Trek * *      *\n");
  printf (" *                                   *\n");
  printf (" *                                   *\n");
  printf (" *************************************\n\n\n\n\n");

  printf("\nНужны ли вам инструкции (y/n): ");

  gets(sTemp);

  if (sTemp[0] == 'y' || sTemp[0] == 'Y')
    showfile("startrek.doc");

  printf ("\n\n\n\n\n\n\n");
  printf("                         ------*------\n");
  printf("         -------------   `---  ------'\n");
  printf("         `-------- --'      / /\n");
  printf("                  \\\\-------  --\n");
  printf("                  '-----------'\n");
  printf("\n       Звездолёт «Энтерпрайз» --- NCC - 1701\n\n\n");

  randomize();

  t = (get_rand(20) + 20) * 100;
}

/*
 * new_game: готовит новую игру (initialize, new_quadrant, первый srs)
 * и входит в главный цикл. В каждой итерации:
 *   1) проверяет «фатальное» состояние (энергии нет, а щиты не могут
 *      подать питание в двигатели — корабль застрял навсегда);
 *   2) читает команду и вызывает обработчик по первым трём символам.
 * Цикл завершается только выходом из программы (см. end_of_game).
 */
void
new_game(void)
{
  string sTemp;

  if (st_session_start())      /* имя капитана и загрузка сохранения */
      short_range_scan();
  else
  {
      initialize();
      new_quadrant();
      short_range_scan();
  }

  while(1)
    {
      if (s + e <= 10 && (e < 10 || d[7] < 0))
        {
          printf("\n** Фатальная ошибка **   ");
          printf("Вы только что посадили свой корабль на мель в космосе.\n\n");
          printf("У вас недостаточно энергии для маневрирования,");
          printf(" а Управление щитами сейчас\n");
          printf("не способно перекинуть питание в машинное отделение!!\n\n");
          end_of_time();
        }

      printf("Команда? ");

      gets(sTemp);
      printf("\n");

      if (! strncmp(sTemp, "nav", 3))
        course_control();
      else if (! strncmp(sTemp, "srs", 3))
        short_range_scan();
      else if (! strncmp(sTemp, "lrs", 3))
        long_range_scan();
      else if (! strncmp(sTemp, "pha", 3))
        phaser_control();
      else if (! strncmp(sTemp, "tor", 3))
        photon_torpedoes();
      else if (! strncmp(sTemp, "she", 3))
        sheild_control();
      else if (! strncmp(sTemp, "dam", 3))
        damage_control();
      else if (! strncmp(sTemp, "com", 3))
        library_computer();
      else if (! strncmp(sTemp, "xxx", 3))
        resign_commision();
      else if (! strncmp(sTemp, "sav", 3))
          st_save_command();
      else
    {
          printf("Введите одну из следующих команд:\n\n");
      printf("  nav - Задать курс\n");
      printf("  srs - Датчики ближнего действия\n");
      printf("  lrs - Датчики дальнего действия\n");
      printf("  pha - Фазеры\n");
      printf("  tor - Фотонные торпеды\n");
      printf("  she - Управление щитами\n");
      printf("  dam - Управление повреждениями\n");
      printf("  com - Библиотека-компьютер\n");
      printf("  xxx - Сложить командование\n");
      printf("  sav - Сохранить игру\n");
      printf("\n");
    }
    }
}

/*
 * ======================================================================
 *  БЛОК 2. Инициализация игры и квадранта
 * ======================================================================
 */

/*
 * initialize: задаёт стартовые значения (время, энергия, торпеды,
 * положение корабля, исправность систем) и случайно заполняет галактику:
 *   - клингоны в квадранте: 3 (≈2%), 2 (≈3%), 1 (≈15%), иначе 0;
 *   - звёздная база в квадранте: ≈4%;
 *   - звёзд: 1..8.
 * Гарантирует хотя бы одну звёздную базу, задаёт срок миссии t9
 * (не меньше числа клингонов + 1) и печатает боевой приказ.
 */
void
initialize(void)
{
  int i, j;

  /* Сброс счётчиков клингонов и баз (важно при повторной миссии) */

  k9 = 0;
  b9 = 0;

  /* Инициализация времени */

  /* @@@ t0 = t; */
  t0 = (int)t;
  t9 = 25 + get_rand(10);

  /* Инициализация «Энтерпрайза» */

  d0 = 0;
  e = e0;
  p = p0;
  s = 0;

  q1 = function_r();
  q2 = function_r();
  s1 = (double) function_r();
  s2 = (double) function_r();

  for (i = 1; i <= 8; i++)
    d[i] = 0.0;

  /* Настройка того, что существует в галактике */

  for (i = 1; i <= 8; i++)
    for (j = 1; j <= 8; j++)
      {
        k3 = 0;
        z[i][j] = 0;
        r1 = get_rand(100);
        if (r1 > 98)
          k3 = 3;
        else if (r1 > 95)
          k3 = 2;
        else if (r1 > 80)
          k3 = 1;

        k9 = k9 + k3;
        b3 = 0;

        if (get_rand(100) > 96)
          b3 = 1;

        b9 = b9 + b3;

        g[i][j] = k3 * 100 + b3 * 10 + function_r();
      }

  if (k9 > t9)
    t9 = k9 + 1;

  if (b9 == 0)
    {
      if (g[q1][q2] < 200)
        {
          g[q1][q2] = g[q1][q2] + 100;
          k9++;
        }

      g[q1][q2] = g[q1][q2] + 10;
      b9++;

      q1 = function_r();
      q2 = function_r();
    }

  k7 = k9;

  printf("Ваши приказы таковы:\n\n");
  printf("   Уничтожьте %d клингонских боевых кораблей, вторгшихся в\n", k9);
  printf(" галактику, прежде чем они атакуют Штаб Федерации\n");
  printf(" в звёздную дату %d. Это даёт вам %d дней.\n",
    t0 + t9, t9);
  printf(" Количество звёздных баз в галактике для снабжения вашего\n");
  printf(" корабля: %d.\n\n", b9);

  printf("Нажмите Enter, чтобы принять командование. ");
  getchar();
}

/*
 * new_quadrant: «входит» в квадрант (q1, q2). Печатает его название,
 * раскодирует g[q1][q2] в k3/b3/s3, выводит предупреждение о боевой
 * зоне, затем заново строит карту квадранта sQ: ставит «Энтерпрайз»
 * на его место, а клингонов (с мощностью 100..299), звёздную базу
 * и звёзды — в случайные пустые сектора.
 */
void
new_quadrant(void)
{
  int i;

  z4 = q1;
  z5 = q2;
  k3 = 0;
  b3 = 0;
  s3 = 0;
  g5 = 0; 
  d4 = (double) get_rand(100) / 100 / 50;
  z[q1][q2] = g[q1][q2];

  if (q1 >= 1 && q1 <= 8 && q2 >= 1 && q2 <= 8)
    {
      quadrant_name();

      if (t0 != t)
        printf("Вход в квадрант %s...\n\n", sG2);
      else
        {
          printf("\nВаша миссия начинается: ваш звездолёт находится\n");
          printf("в галактическом квадранте %s.\n\n", sG2);
        }
    }

  /* @@@ k3 = g[q1][q2] * .01; */
  k3 = (int)(g[q1][q2] * .01);
  /* @@@ b3 = g[q1][q2] * .1 - 10 * k3; */
  b3 = (int)(g[q1][q2] * .1 - 10 * k3);
  s3 = g[q1][q2] - 100 * k3 - 10 * b3;

  if (k3 > 0)
    {
      printf("Зона боевых действий. Состояние: красное\n");

      if (s < 200)
        printf("Щиты опасно ослаблены\n");
    }

  for (i = 1; i <= 3; i++)
    {
      k[i][1] = 0;
      k[i][2] = 0;
      k[i][3] = 0;
    }

  for (i = 0; i <= 192; i++)
    sQ[i] = ' ';

  sQ[193] = '\0';

  /* Размещаем «Энтерпрайз», затем клингонов, звёздные базы и звёзды */

  strcpy(sA, "<*>");
  /* @@@ z1 = cint(s1); */
  z1 = (int)s1;
  /* @@@ z2 = cint(s2); */
  z2 = (int)s2;
  insert_in_quadrant();

  if (k3 > 0)
    {
      for (i = 1; i <= k3; i++)
        {
          find_empty_place();

          strcpy(sA, "+K+");
          z1 = r1;
          z2 = r2;
          insert_in_quadrant();

          k[i][1] = r1;
          k[i][2] = r2;
          k[i][3] = 100 + get_rand(200);
        }
    }

  if (b3 > 0)
    {
      find_empty_place();

      strcpy(sA, ">!<");
      z1 = r1;
      z2 = r2;
      insert_in_quadrant();

      b4 = r1;
      b5 = r2;
    }

  for (i = 1; i <= s3; i++)
    {
      find_empty_place();

      strcpy(sA, " * ");
      z1 = r1;
      z2 = r2;
      insert_in_quadrant();
    }
}

/*
 * ======================================================================
 *  БЛОК 3. Навигация и расход энергии (команда nav)
 *  Курс 1..9 (дробные значения интерполируются по таблице c[][]),
 *  варп-фактор 1.0 = 8 секторов = один квадрант.
 * ======================================================================
 */

/*
 * course_control: команда nav. Запрашивает курс и варп-фактор, проверяет
 * исправность двигателей (при повреждении максимум варп 0.2) и запас
 * энергии, даёт клингонам ход и ремонтирует системы, затем сектор за
 * сектором двигает корабль. При столкновении с объектом корабль
 * останавливается; при выходе за границу квадранта вызывается
 * exceed_quadrant_limits.
 */
void
course_control(void)
{
  register i;
  /* @@@ int c2, c3, q4, q5; */
  int q4, q5;
  string sTemp;
  double c1;
  char sX[4] = "8";

  printf("Курс (0-9): ");

  gets(sTemp);

  printf("\n");

  c1 = atof(sTemp);
 
  if (c1 == 9.0)
    c1 = 1.0;

  if (c1 < 0 || c1 > 9.0)
    {
      printf("Лейтенант Сулу докладывает:\n");
      printf("  Неверные данные курса, сэр!\n\n");
      return;
    }

  if (d[1] < 0.0)
    strcpy(sX, "0.2");

  printf("Варп-фактор (0-%s): ", sX);

  gets(sTemp);

  printf("\n");

  w1 = atof(sTemp);

  if (d[1] < 0.0 && w1 > 0.21)
    {
      printf("Варп-двигатели повреждены. ");
      printf("Максимальная скорость = Варп 0.2.\n\n");
      return;
    }

  if (w1 <= 0.0)
    return;

  if (w1 > 8.1)
    {
      printf("Главный инженер Скотт докладывает:\n");
      printf("  Двигатели не выдержат варп %4.1f!\n\n", w1);
      return;
    }

  n = cint(w1 * 8.0); /* @@@ примечание: в оригинальном BASIC это настоящее округление */
  
  if (e - n < 0)
    {
      printf("Инженерный отдел докладывает:\n");
      printf("  Недостаточно энергии для маневрирования");
      printf(" на варп %4.1f!\n\n", w1);

      if (s >= n && d[7] >= 0.0)
        {
          printf("Пост управления дефлекторами подтверждает:\n");
          printf("  %d единиц энергии в настоящее время направлено на щиты.\n", s);
        }

      return;
    }

  klingons_move();

  repair_damage();

  strcpy(sA, "   ");
  /* @@@ z1 = cint(s1); */
  z1 = (int)s1;
  /* @@@ z2 = cint(s2); */
  z2 = (int)s2;
  insert_in_quadrant();

  /* @@@ c2 = cint(c1); */
  /* @@@ c3 = c2 + 1; */

  /* @@@ x1 = c[0][c2] + (c[0][c3] - c[0][c2]) * (c1 - c2); */
  /* @@@ x2 = c[1][c2] + (c[1][c3] - c[1][c2]) * (c1 - c2); */

  x1 = c[1][(int)c1] + (c[1][(int)c1 + 1] - c[1][(int)c1]) * (c1 - (int)c1);
  x2 = c[2][(int)c1] + (c[2][(int)c1 + 1] - c[2][(int)c1]) * (c1 - (int)c1);

  x = s1;
  y = s2;
  q4 = q1;
  q5 = q2;

  for (i = 1; i <= n; i++)
    {
      s1 = s1 + x1;
      s2 = s2 + x2;

      /* @@@ z1 = cint(s1); */
      z1 = (int)s1;
      /* @@@ z2 = cint(s2); */
      z2 = (int)s2;

      if (z1 < 1 || z1 >= 9 || z2 < 1 || z2 >= 9)
        {
          exceed_quadrant_limits();
          complete_maneuver();
          return;
        }

      string_compare();

      if (z3 != 1) /* Сектор не пуст */
        {
          s1 = s1 - x1;
          s2 = s2 - x2;
          printf("Варп-двигатели остановлены в секторе ");
          printf("%d, %d из-за ошибки навигации.\n\n", z1, z2);
          i = n + 1;
        }
    }

  complete_maneuver();
}

/*
 * complete_maneuver: общая концовка перелёта внутри квадранта — ставит
 * корабль на новое место, списывает энергию, продвигает время
 * (при варп < 1 — на w1 дат, иначе на 1), проверяет конец срока
 * и показывает srs.
 */
void
complete_maneuver(void)
{
  double t8;

  strcpy(sA, "<*>");
  /* @@@ z1 = cint(s1); */
  z1 = (int)s1;
  /* @@@ z2 = cint(s2); */
  z2 = (int)s2;
  insert_in_quadrant();

  maneuver_energy();

  t8 = 1.0;

  if (w1 < 1.0)
    t8 = w1;

  t = t + t8;

  if (t > t0 + t9)
    end_of_time();

  short_range_scan();
}

/*
 * exceed_quadrant_limits: перелёт в другой квадрант. Пересчитывает
 * абсолютные координаты в (квадрант, сектор); если корабль пытается
 * выйти за границы галактики, останавливает его на краю и печатает
 * отказ Звёздного флота. Затем списывает энергию, проверяет срок,
 * прибавляет 1 к времени и вызывает new_quadrant.
 */
void
exceed_quadrant_limits(void)
{
  int x5 = 0;   /* Флаг выхода за пределы галактики */

  /* @@@ x = (8 * (q1 - 1)) + x + (n * x1); */
  x = (8 * q1) + x + (n * x1);
  /* @@@ y = (8 * (q2 - 1)) + y + (n * x2); */
  y = (8 * q2) + y + (n * x2);

  /* @@@ q1 = cint(x / 8.0); */
  q1 = (int)(x / 8.0);
  /* @@@ q2 = cint(y / 8.0); */
  q2 = (int)(y / 8.0);
  
  /* @@@ s1 = x - ((q1 - 1) * 8); */
  s1 = x - (q1 * 8);
  /* @@@ s2 = y - ((q2 - 1) * 8); */
  s2 = y - (q2 * 8);

  /* @@@ if (cint(s1) == 0) */
  if ((int)s1 == 0)
    {
      q1 = q1 - 1;
      s1 = s1 + 8.0;
    }

  /* @@@ if (cint(s2) == 0) */
  if ((int)s2 == 0)
    {
      q2 = q2 - 1;
      s2 = s2 + 8.0;
    }

  /* проверка выхода за пределы галактики */

  if (q1 < 1)
    {
      x5 = 1;
      q1 = 1;
      s1 = 1.0;
    }

  if (q1 > 8)
    {
      x5 = 1;
      q1 = 8;
      s1 = 8.0;
    }

  if (q2 < 1)
    {
      x5 = 1;
      q2 = 1;
      s2 = 1.0;
    }

  if (q2 > 8)
    {
      x5 = 1;
      q2 = 8;
      s2 = 8.0;
    }

  if (x5 == 1)
    {
      printf("Лейтенант Ухура докладывает:\n");
      printf("  Сообщение от Командования Звёздного флота:\n\n");
      printf("  В разрешении на попытку пересечь границу галактики\n");
      printf("  *отказано*. Остановите двигатели.\n\n");
      printf("Главный инженер Скотт докладывает:\n");
      /* @@@ printf("  Warp Engines shut down at sector %d, ", cint(s1)); */
      printf("  Варп-двигатели остановлены в секторе %d, ", (int)s1);
      /* @@@ printf("%d of quadrant %d, %d.\n\n", cint(s2), q1, q2); */
      printf("%d квадранта %d, %d.\n\n", (int)s2, q1, q2);
    }
  /* else 
     new_quadrant(); @@@ это вызывает ошибки при отскоке от границ галактики.
                         По сути, если отскочить достаточно далеко, содержимое
                         квадранта не будет совпадать с показаниями LRS.
                         Круто, да? */


  maneuver_energy();

  /* в оригинале этот фрагмент расположен в другом порядке.
  t = t + 1;
  
  if (t > t0 + t9)
    end_of_time();
  */

  if (t > t0 + t9)
    end_of_time();

  /* @@@ что это делает?? Это есть в оригинале.
  if (8 * q1 + q2 = 8 * q4 + q5) 
    { 
      complete_maneuver();
    }
  */

  t = t + 1;

  new_quadrant();
}

/*
 * maneuver_energy: списывает за манёвр n + 10 единиц энергии; если
 * основного запаса не хватает, недостаток покрывается за счёт щитов.
 */
void
maneuver_energy(void)
{
  e = e - n - 10;

  if (e >= 0)
    return;

  printf("Управление щитами подаёт энергию для завершения маневра.\n\n");

  s = s + e;
  e = 0;

  if (s <= 0)
    s = 0;
}

/*
 * ======================================================================
 *  БЛОК 4. Датчики (команды srs и lrs)
 * ======================================================================
 */

/*
 * short_range_scan: команда srs. Определяет состояние (зелёное/жёлтое/
 * красное), проверяет стыковку (звёздная база в соседних 8 секторах:
 * пополняет энергию и торпеды, обнуляет щиты), затем рисует карту
 * квадранта и справа от неё — панель состояния. Вызывается и после
 * каждого манёвра.
 */
void
short_range_scan(void)
{
  register i, j;

  strcpy(sC, "ЗЕЛЁНОЕ");

  if (e < e0 * .1)
    strcpy(sC, "ЖЁЛТОЕ");

  if (k3 > 0)
    strcpy(sC, "*КРАСНОЕ*");

  /* @@@ здесь нужно сбросить флаг стыковки */
  d0 = 0;

  /* @@@ for (i = s1 - 1; i <= s1 + 1; i++) */
  for (i = (int)(s1 - 1); i <= (int)(s1 + 1); i++)
    /* @@@ for (j = s2 - 1; j <= s2 + 1; j++) */
    for (j = (int)(s2 - 1); j <= (int)(s2 + 1); j++)
      if (i >= 1 && i <= 8 && j >= 1 && j <= 8)
        {
          strcpy(sA, ">!<");
          z1 = i;
          z2 = j;
          string_compare();
          if (z3 == 1)
            {
              d0 = 1;
              strcpy(sC, "СТЫКОВКА");
              e = e0;
              p = p0;
              printf("Щиты опущены для стыковки.\n");
              s = 0;
            }
        }
          
  if (d[2] < 0.0)
    {
      printf("\n*** Датчики ближнего действия не работают ***\n");
      return;
    }

  printf("------------------------\n");
  for (i = 0; i < 8; i++)
    {
      for (j = 0; j < 24; j++)
        putchar(sQ[i * 24 + j]); 

      if (i == 0)
    printf("    Звёздная дата       %d\n", (int) t);
      if (i == 1)
    printf("    Состояние           %s\n", sC);
      if (i == 2)
    printf("    Квадрант            %d, %d\n", q1, q2);
      if (i == 3)
    /* @@@ printf("    Sector              %d, %d\n", cint(s1), cint(s2)); */
    printf("    Сектор              %d, %d\n", (int)s1, (int)s2);
      if (i == 4)
    printf("    Фотонные торпеды    %d\n", p);
      if (i == 5)
    printf("    Всего энергии       %d\n", e + s);
      if (i == 6)
    printf("    Щиты                %d\n", s);
      if (i == 7)
    printf("    Осталось клингонов  %d\n", k9);
    }
  printf("------------------------\n\n");

  return;
}

/*
 * long_range_scan: команда lrs. Показывает коды KBS для квадранта
 * корабля и 8 соседних (за краем галактики — «***») и запоминает
 * их в массиве z для компьютера.
 */
void
long_range_scan(void)
{
  register i, j;

  if (d[3] < 0.0)
    {
      printf("Датчики дальнего действия не работают.\n");
      return;
    }

  printf("Сканирование датчиками дальнего действия для квадранта %d, %d\n\n", q1, q2);

  for (i = q1 - 1; i <= q1 + 1; i++)
    {
      printf("--------------------\n:");
      for (j = q2 - 1; j <= q2 + 1; j++)
        if (i > 0 && i <= 8 && j > 0 && j <= 8)
          {
            z[i][j] = g[i][j];
            printf(" %3.3d :", z[i][j]);
          }
        else
          printf(" *** :");
      printf("\n");
    }

    printf("--------------------\n\n");
}

/*
 * ======================================================================
 *  БЛОК 5. Бой: оружие игрока (команды pha и tor)
 * ======================================================================
 */

/*
 * phaser_control: команда pha. Тратит введённую энергию, делит её
 * поровну между клингонами квадранта, урон каждому убывает с расстоянием
 * и содержит случайный множитель. При отказе компьютера точность
 * падает. Уничтожение последнего клингона — победа. После выстрела
 * отвечают оставшиеся клингоны.
 * Расстояние для расчёта урона берётся до каждой конкретной цели
 * (function_d(i), как FND(I) в оригинале BASIC).
 */
void
phaser_control(void)
{
  register i;
  int iEnergy;
  int h1, h;
  string sTemp;

  if (d[4] < 0.0)
    {
      printf("Фазеры не работают\n\n");
      return;
    }

  if (k3 <= 0)
    {
      printf("Научный офицер Спок докладывает:\n");
      printf("  'Датчики не обнаруживают вражеских кораблей в этом квадранте'\n\n");
      return;
    }

  if (d[8] < 0.0)
    /* @@@ printf("Computer failure happers accuracy.\n"); */
    printf("Отказ компьютера снижает точность.\n");

  printf("Фазеры наведены на цель;\n");
  printf("Доступно энергии = %d единиц\n\n", e);

  printf("Количество единиц для выстрела: ");

  gets(sTemp);

  printf("\n");

  iEnergy = atoi(sTemp);

  if (iEnergy <= 0)
    return;

  if (e - iEnergy < 0)
    {
      printf("Недостаточно энергии.\n\n");
      return;
    }

  e = e - iEnergy;

  if (d[8] < 0.0)
    /* @@@ iEnergy = iEnergy * rnd(); */
    iEnergy = (int)(iEnergy * rnd());

  h1 = iEnergy / k3;

  for (i = 1; i <= 3; i++)
    {
      if (k[i][3] > 0)
        {
          /* @@@ h = (h1 / function_d(0) * (rnd() + 2)); */
          /* исправлено: расстояние до клингона i, а не до k[0] */
          h = (int)(h1 / function_d(i) * (rnd() + 2));
          if (h <= .15 * k[i][3])
            {
              printf("Датчики не показывают повреждений у противника в ");
              printf("%d, %d\n\n", k[i][1], k[i][2]);
            }
          else
            {
              k[i][3] = k[i][3] - h;
              printf("Попадание %d ед. по клингону в секторе ", h);
              printf("%d, %d\n", k[i][1], k[i][2]);
              if (k[i][3] <= 0)
                {
                  printf("*** Клингон уничтожен ***\n\n");
                  k3--;
                  k9--;
                  z1 = k[i][1];
                  z2 = k[i][2];
                  strcpy(sA, "   ");
                  insert_in_quadrant();
                  k[i][3] = 0;
                  g[q1][q2] = g[q1][q2] - 100;
                  z[q1][q2] = g[q1][q2];
                  if (k9 <= 0)
                    won_game();
                }
              else
                /* @@@ printf("\n"); */
                printf("   (Датчики показывают остаток: %d ед.)\n\n", k[i][3]);
            }
        }
    }

  klingons_shoot();
}

/*
 * photon_torpedoes: команда tor. Запрашивает курс, тратит торпеду и
 * 2 единицы энергии, затем «летит» по траектории сектор за сектором,
 * печатая путь. При первом занятом секторе вызывается torpedo_hit.
 * Если торпеда вылетела за квадрант — промах. В любом случае
 * клингоны отвечают огнём.
 */
void
photon_torpedoes(void)
{
  /* @@@ int c2, c3, x3, y3, x5; */
  int x3, y3, x5;
  string sTemp;
  double c1;

  if (p <= 0)
    {
      printf("Все фотонные торпеды израсходованы\n");
      return;
    }

  if (d[5] < 0.0)
    {
      printf("Торпедные аппараты неисправны\n");
      return;
    }

  printf("Курс (0-9): ");

  gets(sTemp);

  printf("\n");

  c1 = atof(sTemp);

  if (c1 == 9.0)
    c1 = 1.0;

  /* @@@ if (c1 < 0 || c1 > 9.0) */
  if (c1 < 1.0 || c1 > 9.0)
    {
      printf("Энсин Чехов докладывает:\n");
      printf("  Неверные данные курса, сэр!\n\n");
      return;
    }

  e = e - 2;
  p--;

  /* @@@ c2 = cint(c1); */
  /* @@@ c3 = c2 + 1; */

  /* @@@ x1 = c[0][c2] + (c[0][c3] - c[0][c2]) * (c1 - c2); */
  /* @@@ x2 = c[1][c2] + (c[1][c3] - c[1][c2]) * (c1 - c2); */

  x1 = c[1][(int)c1] + (c[1][(int)c1 + 1] - c[1][(int)c1]) * (c1 - (int)c1);
  x2 = c[2][(int)c1] + (c[2][(int)c1 + 1] - c[2][(int)c1]) * (c1 - (int)c1);

  x = s1 + x1;
  y = s2 + x2;

  x3 = cint(x); /* @@@ примечание: в версии MS BASIC это настоящее целочисленное округление */
  y3 = cint(y); /* @@@ примечание: в версии MS BASIC это настоящее целочисленное округление */

  x5 = 0;

  printf("Траектория торпеды:\n");

  while (x3 >= 1 && x3 <= 8 && y3 >= 1 && y3 <= 8)
    {
      printf("    %d, %d\n", x3, y3);

      strcpy(sA, "   ");
      z1 = x3;
      z2 = y3;

      string_compare();

      if (z3 == 0)
        {
          torpedo_hit();
          klingons_shoot();
          return;
        }

      x = x + x1;
      y = y + x2;

      x3 = cint(x); /* @@@ примечание: в версии MS BASIC это настоящее целочисленное округление */
      y3 = cint(y); /* @@@ примечание: в версии MS BASIC это настоящее целочисленное округление */
    }

  printf("Торпеда прошла мимо\n\n");

  klingons_shoot();
}

/*
 * torpedo_hit: определяет, во что попала торпеда: звезда (поглощает),
 * клингон (уничтожен), звёздная база (уничтожена — угроза трибунала,
 * а если баз не осталось и время поджимает — немедленная отставка).
 * Обновляет карту квадранта и код квадранта в g и z.
 */
void
torpedo_hit(void)
{
  int i, x3, y3;

  x3 = cint(x); /* @@@ примечание: в версии MS BASIC это настоящее целочисленное округление */
  y3 = cint(y); /* @@@ примечание: в версии MS BASIC это настоящее целочисленное округление */

  z3 = 0;

  strcpy(sA, " * ");
  string_compare();

  if (z3 == 1)
    {
      printf("Звезда в %d, %d поглотила энергию торпеды.\n\n", x3, y3);
      return;
    }

  strcpy(sA, "+K+");
  string_compare();

  if (z3 == 1)
    {
      printf("*** Клингон уничтожен ***\n\n");
      k3--;
      k9--;

      if (k9 <= 0)
        won_game();

      for (i=0; i<=3; i++)
        if (x3 == k[i][1] && y3 == k[i][2])
          k[i][3] = 0;
    }

  strcpy(sA, ">!<");
  string_compare();

  if (z3 == 1)
    {
      printf("*** Звёздная база уничтожена ***\n");
      b3--;
      b9--;

      if (b9 <= 0 && k9 <= t - t0 - t9)
        {
          printf("С меня довольно, капитан!! ");
          printf("Вы отстраняетесь от командования\n");
          printf("и приговариваетесь к 99 звёздным датам каторжных ");
          printf("работ на Лебеде 12!!\n");
          resign_commision();
        }

      printf("Командование Звёздного флота рассматривает ваше личное дело\n");
      printf("на предмет военного трибунала!\n\n");

      d0 = 0;    /* Расстыковка */
    }

  z1 = x3;
  z2 = y3;
  strcpy(sA,"   ");
  insert_in_quadrant();

  g[q1][q2] = (k3 * 100) + (b3 * 10) + s3;
  z[q1][q2] = g[q1][q2];
}

/*
 * ======================================================================
 *  БЛОК 6. Системы корабля (команды dam и she)
 * ======================================================================
 */

/*
 * damage_control: команда dam. Если сама служба повреждена, отчёт
 * недоступен, но стоя на базе можно заказать ремонт (занимает время).
 * Иначе печатает таблицу состояния 8 устройств
 * (отрицательное значение = повреждено).
 */
void
damage_control(void)
{ 
  int a1;
  double d3 = 0.0;
  register i;

  if (d[6] < 0.0)
    {
      printf("Отчёт службы устранения повреждений недоступен.\n");

      if (d0 == 0)
        return;

      d3 = 0.0;
      for (i = 1; i <= 8; i++)
        if (d[i] < 0.0)
          d3 = d3 + .1;

      if (d3 == 0.0)
        return;

      d3 = d3 + d4;
      if (d3 >= 1.0)
        d3 = 0.9;

      printf("\nТехники готовы приступить к ремонту вашего");
      /* @@@ printf("ship; Will you authorize the repair order (Y/N)? "); */
      printf(" корабля;\nПредполагаемое время ремонта: %4.2f звёздной даты.\n", d3);
      printf("Разрешаете ли вы ремонт (Y/N)? ");

      a1 = getchar();

      if (a1 == 'Y' || a1 == 'y')
        {
          for (i = 1; i <= 8; i++)
            if (d[i] < 0.0)
              d[i] = 0.0;

          t = t + d3 + 0.1;
        }
    }

  printf("Устройство                 Состояние исправности\n");

  for (r1 = 1; r1 <= 8; r1++)
    {
      get_device_name();
      printf(sG2);
      /* @@@ for (i = 1; i < 25 - strlen(sG2); i++) */
      /* Для UTF-8 считаем символы, а не байты, чтобы колонки не «плыли» */
      for (i = 1; i < 25 - utf8_strlen(sG2); i++)
      printf(" ");
      /* @@@ printf("%4.1f\n", d[r1]); */
      printf("%4.2f\n", d[r1]);
    }

  printf("\n");
} 

/*
 * sheild_control: команда she. Перераспределяет энергию между основным
 * запасом (e) и щитами (s); суммарная энергия e + s сохраняется,
 * нельзя отдать щитам больше или всё, что есть.
 */
void
sheild_control(void)
{
  int i;
  string sTemp;

  if (d[7] < 0.0)
    {
      printf("Управление щитами не работает\n");
      return;
    }

  printf("Доступно энергии = %d\n\n", e + s);

  printf("Введите количество единиц для щитов: ");

  gets(sTemp);

  printf("\n");

  i = atoi(sTemp);

  if (i < 0 || s == i)
    {
      printf("<Щиты без изменений>\n\n");
      return;
    }

  if (i >= e + s)
    {
      printf("Управление щитами докладывает:\n");
      printf("  'Это вам не казначейство Федерации.'\n");
      printf("<Щиты без изменений>\n\n");
      return;
    }

  e = e + s - i;
  s = i;

  printf("Пост управления дефлекторами докладывает:\n");
  printf("  'Щиты теперь на %d единицах согласно вашему приказу.'\n\n", s);
}

/*
 * ======================================================================
 *  БЛОК 7. Библиотека-компьютер (команда com, опции 0..5)
 *  Работает только при исправном компьютере (d[8]).
 * ======================================================================
 */

/* library_computer: запрашивает номер опции и вызывает обработчик */
void
library_computer(void)
{
  string sTemp;

  if (d[8] < 0.0)
    {
      printf("Библиотека-компьютер не работает\n");
      return;
    }

  printf("Компьютер активен и ожидает команды: ");

  gets(sTemp);
  printf("\n");

  if (! strncmp(sTemp, "0", 1))
    galactic_record();
  else if (! strncmp(sTemp, "1", 1))
    status_report();
  else if (! strncmp(sTemp, "2", 1))
    torpedo_data();
  else if (! strncmp(sTemp, "3", 1))
    nav_data();
  else if (! strncmp(sTemp, "4", 1))
    dirdist_calc();
  else if (! strncmp(sTemp, "5", 1))
    galaxy_map();
  else
    {
      printf("Функции, доступные в Библиотеке-компьютере:\n\n");
      printf("   0 = Накопительная галактическая запись\n");
      printf("   1 = Отчёт о состоянии\n");
      printf("   2 = Данные для фотонных торпед\n");
      printf("   3 = Навигационные данные звёздной базы\n");
      printf("   4 = Калькулятор направления/расстояния\n");
      printf("   5 = Карта галактики с названиями регионов\n\n");
    }
}

/* galactic_record: опция 0. Таблица 8x8 с кодами KBS уже изученных квадрантов («***» — не изучен) */
void
galactic_record(void)
{
  int i, j;

  printf("\n     Компьютерная запись галактики для квадранта %d,%d\n\n", q1, q2);
  printf("     1     2     3     4     5     6     7     8\n");

  for (i = 1; i <= 8; i++)
  { 
    printf("   ----- ----- ----- ----- ----- ----- ----- -----\n");

    printf("%d", i);

    for (j = 1; j <= 8; j++)
    {
      printf("   ");

      if (z[i][j] == 0)
        printf("***");
      else
        printf("%3.3d", z[i][j]);
    }

    printf("\n");
  }

  printf("   ----- ----- ----- ----- ----- ----- ----- -----\n\n");
}

/* status_report: опция 1. Сколько клингонов осталось, сколько времени до конца миссии и сколько звёздных баз */
void
status_report(void)
{
  printf("   Отчёт о состоянии:\n\n");

  printf("Клингонов осталось: %d\n", k9);

  printf("Миссия должна быть выполнена за %4.1f звёздной даты\n",
    /* @@@ .1 * cint((t0 + t9 - t) * 10)); */
    .1 * (int)((t0 + t9 - t) * 10));

  if (b9 < 1)
  {
    printf("Ваша глупость оставила вас в галактике в одиночестве\n");
    printf(" -- у вас не осталось звёздных баз!\n");
  }
  else
  {  
    printf("Федерация располагает звёздными базами в галактике: %d\n", b9);
  }

  printf("\n");
}

/* torpedo_data: опция 2. Направление и расстояние от корабля до каждого клингона квадранта */
void
torpedo_data(void)
{
  int i;

  if (k3 <= 0)
  {
    printf("Научный офицер Спок докладывает:\n");
    printf("  'Датчики не обнаруживают вражеских кораблей в этом квадранте.'\n\n");
    return;
  }

  printf("От «Энтерпрайза» до клингонских боевых крейсеров:\n\n");

  for (i = 1; i <= 3; i++)
  {
    if (k[i][3] > 0)
    {
      w1 = k[i][1];
      x  = k[i][2];
      c1 = s1;
      a  = s2;

      compute_vector();
    }
  }
}

/* nav_data: опция 3. Направление и расстояние до звёздной базы квадранта */
void
nav_data(void)
{
  if (b3 <= 0)
  {
    printf("Мистер Спок докладывает:\n");
    printf("  'Датчики не обнаруживают звёздных баз в этом квадранте.'\n\n");
    return;
  }

  w1 = b4;
  x  = b5;
  c1 = s1;
  a  = s2;

  compute_vector();
}

/* dirdist_calc: опция 4. Ручной калькулятор направления/расстояния между двумя введёнными точками */
void
dirdist_calc(void)
{
  string sTemp;

  printf("Калькулятор направления/расстояния\n\n");
  printf("Вы находитесь в квадранте %d,%d, секторе %d,%d\n\n", q1, q2,
    /* @@@ cint(s1), cint(s2)); */
    (int)s1, (int)s2);
    
  printf("Введите начальную координату X: ");
  gets(sTemp);
  c1 = atoi(sTemp);

  printf("Введите начальную координату Y: ");
  gets(sTemp);
  a = atoi(sTemp);

  printf("Введите конечную координату X: ");
  gets(sTemp);
  w1 = atoi(sTemp);

  printf("Введите конечную координату Y: ");
  gets(sTemp);
  x = atoi(sTemp);

  compute_vector();
}

/* galaxy_map: опция 5. Карта названий 16 регионов (по два квадранта на регион); флаг g5 отключает римские номера */
void
galaxy_map(void)
{
  int i, j, j0;

  g5 = 1;

  printf("\n                   Галактика\n\n");
  printf("    1     2     3     4     5     6     7     8\n");

  for (i = 1; i <= 8; i++)
  {
    printf("  ----- ----- ----- ----- ----- ----- ----- -----\n");

    printf("%d ", i);

    z4 = i;
    z5 = 1;
    quadrant_name();

    j0 = (int)(11 - (strlen(sG2) / 2));

    for (j = 0; j < j0; j++)
      printf(" ");

    printf(sG2);

    for (j = 0; j < j0; j++)
      printf(" ");

    if (! (strlen(sG2) % 2))
      printf(" ");

    z5 = 5;
    quadrant_name();

    j0 = (int)(12 - (strlen(sG2) / 2));

    for (j = 0; j < j0; j++)
      printf(" ");

    printf(sG2); 
   
    printf("\n");
  }

  printf("  ----- ----- ----- ----- ----- ----- ----- -----\n\n");

}

/*
 * compute_vector: общая математика направления. По двум точкам
 * (c1,a) -> (w1,x) определяет октант и базовый курс (1, 3, 5 или 7),
 * затем передаёт расчёт в sub1 или sub2.
 */
void
compute_vector(void)
{
  x = x - a;
  a = c1 - w1;

  if (x <= 0.0)
  {
    if (a > 0.0)
    {    
      c1 = 3.0;
      sub2();
      return;
    }
    else
    {
      c1 = 5.0;
      sub1();
      return;
    }
  }
  else if (a < 0.0)
  {
    c1 = 7.0;
    sub2();
    return;
  }
  else
  {
    c1 = 1.0;
    sub1();
    return;
  }
}

/* sub1: уточняет курс и печатает направление и расстояние (курсы 1 и 5, ведущая ось — x) */
void
sub1(void)
{
  x = fabs(x);
  a = fabs(a);

  if (a <= x)
    /* x == 0 только если и a == 0 (точки совпадают): делить нельзя */
    printf("  НАПРАВЛЕНИЕ = %4.2f\n", (x == 0.0) ? c1 : c1 + (a / x));
  else
    printf("  НАПРАВЛЕНИЕ = %4.2f\n", c1 + (((a * 2) - x) / a));

  printf("  РАССТОЯНИЕ = %4.2f\n\n", (x > a) ? x : a);
}

/* sub2: то же для курсов 3 и 7, где ведущая ось — a */
void
sub2(void)
{
  x = fabs(x);
  a = fabs(a);

  if (a >= x)
    /* a == 0 только если и x == 0 (точки совпадают): делить нельзя */
    printf("  НАПРАВЛЕНИЕ = %4.2f\n", (a == 0.0) ? c1 : c1 + (x / a));
  else
    /* @@@ printf("  DIRECTION = %4.2f\n\n", c1 + (((x * 2) - a) / x)); */
    printf("  НАПРАВЛЕНИЕ = %4.2f\n", c1 + (((x * 2) - a) / x));

  /* @@@ printf("  DISTANCE = %4.2f\n", (x > a) ? x : a); */
  printf("  РАССТОЯНИЕ = %4.2f\n\n", (x > a) ? x : a);
}

/*
 * ======================================================================
 *  БЛОК 8. Завершение игры
 *  Цепочка вызовов: ship_destroyed -> end_of_time -> resign_commision
 *  -> end_of_game. won_game тоже заканчивается в end_of_game.
 * ======================================================================
 */

/* ship_destroyed: «Энтерпрайз» уничтожен (щиты упали до нуля) */
void
ship_destroyed(void)
{
  printf("«Энтерпрайз» уничтожен. ");
  printf("Федерация будет завоёвана.\n\n");

  end_of_time();
}

/* end_of_time: срок миссии истёк (или корабль потерян); печатает звёздную дату */
void
end_of_time(void)
{
  printf("Звёздная дата: %d.\n\n", (int) t);

  resign_commision();
}

/* resign_commision: команда xxx и общий «проигрышный» итог — сколько клингонов осталось */
void
resign_commision(void)
{
  printf("К концу вашей миссии осталось клингонских боевых крейсеров: %d.\n\n", k9);

  end_of_game();
}

/* won_game: победа; считает рейтинг эффективности 1000 * (k7 / затраченное время)^2 */
void
won_game(void)
{
  printf("Поздравляем, капитан!  Последний клингонский боевой крейсер,\n");
  printf("угрожавший Федерации, уничтожен.\n\n");
 
  if (t - t0 > 0)
    printf("Ваш рейтинг эффективности: %4.2f\n", 1000 * pow(k7 / (t - t0), 2));

  end_of_game();
}

/*
 * end_of_game: если звёздные базы остались, предлагает начать новую
 * миссию (ввод 'banzai'), иначе завершает программу. Счётчики k9 и b9
 * обнуляются в initialize, поэтому повторная миссия начинается с нуля.
 */
void
end_of_game(void)
{
  string sTemp;

  if (b9 > 0)
    {
      printf("Федерации нужен новый командир звездолёта");
      printf(" для аналогичной миссии.\n");
      printf("Если есть доброволец, пусть выйдет вперёд и");
      printf(" введёт 'banzai': ");

      gets(sTemp);
      printf("\n");

      if (! strncmp(sTemp, "banzai", 6))
        new_game();
    }

  exit(0);
}

/*
 * ======================================================================
 *  БЛОК 9. Бой: действия клингонов
 * ======================================================================
 */

/* klingons_move: каждый живой клингон перемещается в случайный пустой сектор, затем клингоны стреляют */
void
klingons_move(void)
{
  int i;

  for (i = 1; i <= 3; i++)
    {
      if (k[i][3] > 0)
        {
          strcpy(sA, "   ");
          z1 = k[i][1];
          z2 = k[i][2];
          insert_in_quadrant();

          find_empty_place();

          k[i][1] = z1;
          k[i][2] = z2;
          strcpy(sA, "+K+");
          insert_in_quadrant();
        }
    }

  klingons_shoot();
}

/*
 * klingons_shoot: ответный огонь всех живых клингонов. На базе огонь
 * поглощается щитами базы. Урон зависит от мощности клингона и
 * расстояния, снимается со щитов (щиты <= 0 — гибель корабля), мощность
 * клингона после выстрела падает. Сильное попадание (>= 20) может
 * повредить случайное устройство.
 */
void
klingons_shoot(void)
{
  int h, i;
  int sd;                 /* Безопасный делитель: значение щитов, но не меньше 1 */

  if (k3 <= 0)
    return;

  if (d0 != 0)
    {
      printf("Щиты звёздной базы защищают «Энтерпрайз»\n\n");
      return;
    }

  for (i = 1; i <= 3; i++)
    {
      if (k[i][3] > 0)
        {
          h = (int) ((k[i][3] / function_d(i)) * (2 + rnd()));
          s = s - h;
          /* @@@ k[i][3] = k[i][3] / (3 + rnd()); */
          k[i][3] = (int)(k[i][3] / (3 + rnd()));

          printf("Попадание %d ед. по «Энтерпрайзу» из сектора ", h);
          printf("%d, %d\n", k[i][1], k[i][2]);

              if (s <= 0)
                {
                  printf("\n");
                  ship_destroyed();
                }

              printf("    <Щиты упали до %d ед.>\n\n", s);

          if (h >= 20)
            {
              sd = (s > 0) ? s : 1;

              if (rnd() <= 0.6 || (h / sd) > 0.2)
                {
                  r1 = function_r();
                  d[r1] = d[r1] - (h / sd) - (0.5 * rnd());

                  get_device_name();

                  printf("Служба устранения повреждений докладывает:\n");
                  printf("   '%s' повреждено попаданием\n\n", sG2);
                }
            }
        }
    }
} 

/*
 * ======================================================================
 *  БЛОК 10. Повреждения и ремонт
 * ======================================================================
 */

/*
 * repair_damage: вызывается при каждом манёвре. Постепенно ремонтирует
 * повреждённые устройства (скорость зависит от варп-фактора) и с
 * вероятностью 20% случайно что-то ломает (60%) или улучшает (40%).
 */
void
repair_damage(void)
{
  int i;
  double d6;              /* Коэффициент ремонта */

  d6 = w1;

  if (w1 >= 1.0)
    d6 = w1 / 10;

  for (i = 1; i <= 8; i++)
    {
      if (d[i] < 0.0)
        {
          d[i] = d[i] + d6;
          if (d[i] > -0.1 && d[i] < 0)
            d[i] = -0.1;
          else if (d[i] >= 0.0)
            {
              if (d1 != 1)
                d1 = 1;

              printf("Отчёт службы устранения повреждений:\n");
              r1 = i;
              get_device_name();
              printf("    Ремонт устройства '%s' завершён\n\n", sG2);
            }
        }
    }

  if (rnd() <= 0.2)
    {
      r1 = function_r();

      if (rnd() < .6)
        {
          d[r1] = d[r1] - (rnd() * 5.0 + 1.0);
          printf("Отчёт службы устранения повреждений:\n");
          get_device_name();
          printf("    Устройство '%s' повреждено\n\n", sG2);
        }
      else
        {
          d[r1] = d[r1] + (rnd() * 3.0 + 1.0);
          printf("Отчёт службы устранения повреждений:\n");
          get_device_name();
          printf("    Состояние исправности устройства '%s' улучшилось\n\n", sG2);
        }
    }
}

/*
 * ======================================================================
 *  БЛОК 11. Вспомогательные функции игры
 *  Работа с картой квадранта sQ, названиями, расстояниями и округлением.
 * ======================================================================
 */

/* find_empty_place: подбирает случайный пустой сектор квадранта; результат — в r1, r2 */
void
find_empty_place(void)
{
  /* @@@ while (z3 == 0) это неприятный момент.*/
  do
    {
      r1 = function_r();
      r2 = function_r();

      strcpy(sA, "   ");

      z1 = r1;
      z2 = r2;

      string_compare();
    } while (z3 == 0);

  z3 = 0;
}

/* insert_in_quadrant: записывает 3-символьный объект sA в сектор (z1, z2) карты sQ */
void
insert_in_quadrant(void)
{
  int i, j = 0;

  /* @@@ s8 = ((z2 - 1) * 3) + ((z1 - 1) * 24) + 1; */
  s8 = ((int)(z2 - 0.5) * 3) + ((int)(z1 - 0.5) * 24) + 1;

  for (i = s8 - 1; i <= s8 + 1; i++)
    sQ[i] = sA[j++];

  return;
}

/* get_device_name: по номеру устройства r1 (1..8) кладёт его название в sG2 */
void
get_device_name(void)
{
  static char * device_name[] = {
    "", "Варп-двигатели","Датчики ближнего действия","Датчики дальнего действия",
    "Управление фазерами","Торпедные аппараты","Управление повреждениями","Управление щитами",
    "Библиотека-компьютер"};

  if (r1 < 0 || r1 > 8)
    r1 = 0;

  strcpy(sG2, device_name[r1]);

  return;
}

/* string_compare: проверяет, что в секторе (z1, z2) находится объект sA; результат — z3 (1 = совпало, 0 = нет) */
void
string_compare(void)
{
  int i;
  char sB[4];

  z1 = (int)(z1 + 0.5);
  z2 = (int)(z2 + 0.5);

  s8 = ((z2 - 1) * 3) + ((z1 - 1) * 24) + 1;

  mid_str(sB, sQ, s8, 3);

  i = strncmp(sB, sA, 3);

  if (i == 0)
    z3 = 1;
  else
    z3 = 0;

  return;
}

/* quadrant_name: формирует в sG2 название квадранта (z4, z5): регион + номер I..IV */
void
quadrant_name(void)
{
  /* Названия звёзд оставлены на латинице (имена собственные; карта
     галактики центрирует их по длине строки в байтах) */
  static char * quad_name[] = {"","Antares","Rigel","Procyon","Vega",
    "Canopus","Altair","Sagittarius","Pollux","Sirius","Deneb","Capella",
    "Betelgeuse","Aldebaran","Regulus","Arcturus","Spica"};

  static char * sect_name[] = {""," I"," II"," III"," IV"};

  if (z4 < 1 || z4 > 8 || z5 < 1 || z5 > 8)
    strcpy(sG2, "Неизвестно");

  if (z5 <= 4)
    strcpy(sG2, quad_name[z4]);
  else
    strcpy(sG2, quad_name[z4+8]);

  if (g5 != 1)
    {
      if (z5 > 4)
      z5 = z5 - 4;
      strcat(sG2, sect_name[z5]);
    }

  return;
}

/* function_d: целое расстояние от корабля до клингона i (евклидово, усечённое) */
int
function_d(int i)
{
  int j;

  /* @@@ j = sqrt(pow((k[i][1] - s1), 2) + pow((k[i][2] - s2), 2)); */
  j = (int)sqrt(pow((k[i][1] - s1), 2) + pow((k[i][2] - s2), 2));

  /* Результат используется как делитель; защита от деления на 0 */
  if (j < 1)
    j = 1;

  return j;
}

/* function_r: случайное целое от 1 до 8 (координата сектора/квадранта, номер устройства) */
int
function_r(void)
{
  return(get_rand(8));
}

/* mid_str: аналог MID$ из BASIC — копирует y символов строки b, начиная с позиции x (с 1), в a */
void
mid_str(char *a, char *b, int x, int y)
{
  --x;
  y += x;

  /* @@@ while (x < y && x <= strlen(b)) */
  while (x < y && x <= (int)strlen(b))
    *a++ = *(b + x++);

  *a = '\0';
}

/* Округляет числа с плавающей точкой вместо отбрасывания дробной части */

int
cint (double d)
{
  int i;

  i = (int) (d + 0.5);

  return(i);
}

/*
 * ======================================================================
 *  БЛОК 12. Ввод-вывод, ГСЧ и служебные функции
 * ======================================================================
 */

/* showfile: постранично (по нажатию Enter) выводит текстовый файл, например инструкцию */
void
showfile(char *filename)
{
  line lBuffer;
  int iRow = 0;

  if (openfile(filename, "r") != 0)
    return;

  while (getline(lBuffer) != 0)
    {
      printf(lBuffer);
      
      if (iRow++ > MAXROW - 3)
        {
          getchar();
          iRow = 0;
        }
    }

  closefile();
}

/* openfile: открывает файл (не более одного одновременно); 0 — успех, 1 — ошибка */
int
openfile(char * sFilename, char * sMode)
{
  if (bFlag || (stream = fopen (sFilename, sMode)) == NULL)
    {
      fprintf (stderr, "\nОшибка - невозможно открыть файл: %s.\n\n", sFilename);
      return 1;
    }

  bFlag = TRUE;

  return 0;
}

/* closefile: закрывает файл и сбрасывает флаг открытия */
void
closefile(void)
{
  if (! bFlag)
    fclose(stream);

  bFlag = FALSE;
}

/* getline: читает очередную строку из открытого файла; возвращает её длину или 0 в конце файла */
int
getline(char *s)
{
  if (fgets(s, MAXCOL, stream) == NULL)
    return(0);
  else
    return((int)strlen(s));
}

/* Инициализирует генератор случайных чисел значением таймера */
void
randomize(void)
{
  time_t timer;

  srand ((unsigned) time (&timer));
}

/* Возвращает целое число от 1 до iSpread */
int
get_rand(int iSpread)
{
  return((rand() % iSpread) + 1);
}

/* rnd: случайное вещественное число от 0 до 1 (аналог RND из BASIC) */
double
rnd(void)
{
  double d;

  d = rand() / (double) RAND_MAX;
  
  return(d);
}

/* Возвращает длину строки в символах UTF-8 (а не в байтах) */
int
utf8_strlen(const char *s)
{
  int len = 0;

  for (; *s; s++)
    if (((unsigned char)*s & 0xC0) != 0x80)
      len++;

  return(len);
}
