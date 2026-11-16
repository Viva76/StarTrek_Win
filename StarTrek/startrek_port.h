/*
 * startrek_port.h - подключение игры Super Star Trek к терминалу приложения.
 *
 * Как использовать: в startrek_ru.c ПОСЛЕ стандартных #include (stdio.h, stdlib.h, string.h,
 * math.h, time.h) добавить одну строку:
 *
 *     #include "startrek_port.h"
 *
 * Больше ничего в логике игры менять не нужно. Макросы ниже перенаправляют ввод/вывод игры
 * в функции term_* (они реализованы в main.cpp):
 *   printf / putchar -> вывод в область терминала
 *   gets / getchar   -> ввод строки с клавиатуры окна
 *   fopen / fprintf  -> бывший вывод startrek.doc: вместо файла открывается окно справки (F1)
 *   exit             -> завершение игры с ожиданием Enter, затем закрытие окна
 *   main             -> startrek_main, игра запускается в отдельном потоке приложения
 *   getline          -> st_getline (имя совпадает с getline из stdio.h в MinGW/POSIX)
 *
 * Кодировка: тексты игры в UTF-8. В Visual Studio нужна опция компилятора /utf-8
 * (Свойства проекта -> C/C++ -> Командная строка -> Дополнительные параметры).
 *
 * Файл startrek_ru.c в проекте должен компилироваться как C (расширение .c).
 */
#ifndef STARTREK_PORT_H
#define STARTREK_PORT_H

#include <stdio.h>

#if defined(_MSC_VER) && defined(_MSVC_EXECUTION_CHARACTER_SET) && (_MSVC_EXECUTION_CHARACTER_SET != 65001)
#error Для startrek_ru.c нужна опция компилятора /utf-8 (Свойства проекта - C/C++ - Командная строка).
#endif

#ifdef _MSC_VER
#pragma warning(disable: 4996)   /* strcpy, strcat и др.: код игры старый, переписывать не нужно */
#pragma warning(disable: 4267)   /* size_t -> int в getline: длина строки не больше 80 */
#endif

int   term_printf(const char *fmt, ...);
int   term_putchar(int c);
char *term_gets(char *s);
int   term_getchar(void);
void *term_fopen(const char *name, const char *mode);
int   term_fprintf(void *f, const char *fmt, ...);
void  term_exit(int code);

/* Сохранения (startrek_save.c): имя капитана, загрузка и команда sav */
int   st_session_start(void);   /* 1 - игра загружена из saves.json */
void  st_save_command(void);

#define printf   term_printf
#define putchar  term_putchar
#define gets     term_gets
#define getchar  term_getchar
#define fopen    term_fopen
#define fprintf  term_fprintf
#define exit     term_exit
#define main     startrek_main
#define getline  st_getline

#endif /* STARTREK_PORT_H */
