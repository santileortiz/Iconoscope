/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#if !defined(COMMON_H)
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>
#include <math.h>
#include <stdarg.h>
#include <dirent.h>
#include <locale.h>
#include <float.h>

#ifdef __cplusplus
#define ZERO_INIT(type) (type){}
// TODO: Add the static assert and check it works in C++.
#else
#define ZERO_INIT(type) (type){0}
typedef enum {false, true} bool;
#endif

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))

#if !defined(MIN)
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#if !defined(MAX)
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#if !defined(CLAMP)
#define CLAMP(a,lower,upper) ((a)>(upper)?(upper):((a)<(lower)?(lower):(a)))
#endif

#if !defined(SIGN)
#define SIGN(a) ((a)<0?-1:1)
#endif

#define WRAP(a,lower,upper) ((a)>(upper)?(lower):((a)<(lower)?(upper):(a)))

#define LOW_CLAMP(a,lower) ((a)<(lower)?(lower):(a))

#define I_CEIL_DIVIDE(a,b) ((a%b)?(a)/(b)+1:(a)/(b))

#define kilobyte(val) ((val)*1024LL)
#define megabyte(val) (kilobyte(val)*1024LL)
#define gigabyte(val) (megabyte(val)*1024LL)
#define terabyte(val) (gigabyte(val)*1024LL)

#define invalid_code_path assert(0)

// These macros simplify the process of creating functions that receive format
// string like print does
#if __GNUC__ > 2
#define GCC_PRINTF_FORMAT(fmt_idx, arg_idx) __attribute__((format (printf, fmt_idx, arg_idx)))
#else
#define GCC_PRINTF_FORMAT(fmt_idx, arg_idx)
#endif

// A printf like function works in 3 stages: 1) compute the size of the
// resulting string, 2) allocate the char array for the string, 3) populate the
// array with the resulting string. The following macros implement the 1st and
// 3rd stages. The 2nd stage will be implemented by the user.
//
// This macro corresponds to the 1st stage. It takes as argument _format_ the
// name of the variable with format string. It then creates 2 new variables
// named _size_ and _args_. The first one contains the size of the resulting
// string including the null byte the second one is used by the 3rd stage.
#define PRINTF_INIT(format, size, args)                   \
va_list args;                                             \
size_t size;                                              \
{                                                         \
    va_list args_copy;                                    \
    va_start (args, format);                              \
    va_copy (args_copy, args);                            \
                                                          \
    size = vsnprintf (NULL, 0, format, args_copy) + 1;    \
    va_end (args_copy);                                   \
}

// This is the 3rd stage of a printf like function. It takes the same arguments
// as vsnprintf(). As a convenience the _args_ variable is the one created in
// the 1st stage.
#define PRINTF_SET(str, size, format, args)               \
{                                                         \
    vsnprintf (str, size, format, args);                  \
    va_end (args);                                        \
}

// Console color escape sequences
// TODO: Maybe add a way to detect if the output is a terminal so we don't do
// anything in that case.
#define ECMA_RED(str) "\033[1;31m\033[K"str"\033[m\033[K"
#define ECMA_GREEN(str) "\033[1;32m\033[K"str"\033[m\033[K"
#define ECMA_YELLOW(str) "\033[1;33m\033[K"str"\033[m\033[K"
#define ECMA_BLUE(str) "\033[1;34m\033[K"str"\033[m\033[K"
#define ECMA_MAGENTA(str) "\033[1;35m\033[K"str"\033[m\033[K"
#define ECMA_CYAN(str) "\033[1;36m\033[K"str"\033[m\033[K"
#define ECMA_WHITE(str) "\033[1;37m\033[K"str"\033[m\033[K"
#define ECMA_BOLD(str) "\033[1m\033[K"str"\033[m\033[K"

////////////
// STRINGS
//
// There are currently two implementations of string handling, one that
// optimizes operations on small strings avoiding calls to malloc, and a
// straightforward one that mallocs everything.
//
// Functions available:
//  str_new(char *c_str)
//     strn_new(char *c_str, size_t len)
//  str_set(string_t *str, char *c_str)
//     strn_set(string_t *str, char *c_str, size_t len)
//  str_len(string_t *str)
//  str_data(string_t *str)
//  str_cpy(string_t *dest, string_t *src)
//  str_cat(string_t *dest, string_t *src)
//  str_free(string_t *str)
//
// NOTE:
//  - string_t is zero initialized, an empty string can be created as:
//      string_t str = {0};
//    Use str_new() to initialize one from a null terminated string.
//
//  - str_data() returns a NULL terminated C type string.
//
// Both implementations were tested using the following code:

/*
//gcc -g -o strings strings.c -lm

#include "common.h"
int main (int argv, char *argc[])
{
    string_t str1 = {0};
    string_t str2 = str_new ("");
    // NOTE: str_len(str3) == ARRAY_SIZE(str->str_small)-1, to test correctly
    // the string implementation that uses small string optimization.
    string_t str3 = str_new ("Hercule Poirot");
    string_t str4 = str_new (" ");
    // NOTE: str_len(str3) < str_len(str5)
    string_t str5 = str_new ("is a good detective");

    printf ("str1: '%s'\n", str_data(&str1));
    printf ("str2: '%s'\n", str_data(&str2));
    printf ("str3: '%s'\n", str_data(&str3));
    printf ("str4: '%s'\n", str_data(&str4));
    printf ("str5: '%s'\n", str_data(&str5));

    string_t str_test = {0};
    str_set (&str_test, str_data(&str1));

    printf ("\nTEST 1:\n");
    str_cpy (&str_test, &str2);
    printf ("str_test: '%s'\n", str_data(&str_test));
    str_set (&str_test, str_data(&str3));
    printf ("str_set(str_test, '%s')\n", str_data(&str3));
    printf ("str_test: '%s'\n", str_data(&str_test));
    str_set (&str_test, str_data(&str5));
    printf ("str_set(str_test, '%s')\n", str_data(&str5));
    printf ("str_test: '%s'\n", str_data(&str_test));
    str_set (&str_test, str_data(&str4));
    printf ("str_set(str_test, '%s')\n", str_data(&str4));
    printf ("str_test: '%s'\n", str_data(&str_test));
    str_debug_print (&str_test);
    str_free (&str_test);

    str_set (&str_test, "");
    printf ("\nTEST 2:\n");
    str_cpy (&str_test, &str3);
    printf ("str_test: '%s'\n", str_data(&str_test));
    str_cat (&str_test, &str4);
    printf ("str_cat(str_test, str4)\n");
    printf ("str_test: '%s'\n", str_data(&str_test));
    str_cat (&str_test, &str5);
    printf ("str_cat(str_test, str5)\n");
    printf ("str_test: '%s'\n", str_data(&str_test));
    str_debug_print (&str_test);
    str_free (&str_test);
    return 0;
}
*/

#if 1
// String implementation with small string optimization.

#pragma pack(push, 1)
#if defined(__BYTE_ORDER__)&&(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
typedef union {
    struct {
        uint8_t len_small;
        char str_small[15];
    };

    struct {
        uint32_t capacity;
        uint32_t len;
        char *str;
    };
} string_t;
#elif defined(__BYTE_ORDER__)&&(__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
// TODO: This has NOT been tested!
typedef union {
    struct {
        char str_small[15];
        uint8_t len_small;
    };

    struct {
        char *str;
        uint32_t len;
        uint32_t capacity;
    };
} string_t;
#endif
#pragma pack(pop)

#define str_is_small(string) (!((string)->len_small&0x01))
#define str_len(string) (str_is_small(string)?(string)->len_small/2:(string)->len)
static inline
char* str_data(string_t *str)
{
    return (str_is_small(str)?(str)->str_small:(str)->str);
}

static inline
char* str_small_alloc (string_t *str, size_t len)
{
    str->len_small = 2*len; // Guarantee LSB == 0
    return str->str_small;
}

static inline
char* str_non_small_alloc (string_t *str, size_t len)
{
    str->capacity = (len+1) | 0xF; // Round up and guarantee LSB == 1
    str->str = (char*)malloc(str->capacity);
    return str->str;
}

static inline
void str_maybe_grow (string_t *str, size_t len, bool keep_content)
{
    if (!str_is_small(str)) {
        if (len >= str->capacity) {
            if (keep_content) {
                uint32_t tmp_len = str->len;
                char *tmp = str->str;

                str_non_small_alloc (str, len);
                memcpy (str->str, tmp, tmp_len);
                free (tmp);
            } else {
                free (str->str);
                str_non_small_alloc (str, len);
            }
        }
        str->len = len;

    } else {
        if (len >= ARRAY_SIZE(str->str_small)) {
            if (keep_content) {
                char tmp[ARRAY_SIZE(str->str_small)];
                strcpy (tmp, str->str_small);

                str_non_small_alloc (str, len);
                strcpy (str->str, tmp);
            } else {
                str_non_small_alloc (str, len);
            }
            str->len = len;
        } else {
            str_small_alloc (str, len);
        }
    }
}

static inline
char* str_init (string_t *str, size_t len)
{
    char *dest;
    if (len < ARRAY_SIZE(str->str_small)) {
        dest = str_small_alloc (str, len);
    } else {
        str->len = len;
        dest = str_non_small_alloc (str, len);
    }
    return dest;
}

void str_free (string_t *str)
{
    if (!str_is_small(str)) {
        free (str->str);
    }
    *str = (string_t){0};
}

void str_debug_print (string_t *str)
{
    if (str_is_small(str)) {
        printf ("string_t: [SMALL OPT]\n"
                "  Type: small\n"
                "  data: %s\n"
                "  len: %"PRIu32"\n"
                "  len_small: %"PRIu32"\n"
                "  capacity: %lu\n",
                str->str_small, str_len(str),
                str->len_small, ARRAY_SIZE(str->str_small));
    } else {
        printf ("string_t: [SMALL OPT]\n"
                "  Type: long\n"
                "  data: %s\n"
                "  len: %"PRIu32"\n"
                "  capacity: %"PRIu32"\n",
                str->str, str->len, str->capacity);
    }
}

#else
// Straightforward string implementation

typedef struct {
    char *str;
    uint32_t len;
    uint32_t capacity;
} string_t;

static inline
char* str_alloc (string_t *str, size_t len)
{
    str->capacity = (len+1) | 0x0F; // Round up
    str->str = malloc(str->capacity);
    return str->str;
}

#define str_len(string) ((string)->len)
char* str_data (string_t *str)
{
    if (str->str == NULL) {
        str_alloc (str, 0);
        str->len = 0;
    }
    return str->str;
}

static inline
void str_maybe_grow (string_t *str, size_t len, bool keep_content)
{
    if (len >= str->capacity) {
        if (keep_content) {
            uint32_t tmp_len = str->len;
            char *tmp = str->str;
            str_alloc (str, len);
            memcpy (str->str, tmp, tmp_len);
            free (tmp);
        } else {
            free (str->str);
            str_alloc (str, len);
        }
    }
    str->len = len;
}

static inline
char* str_init (string_t *str, size_t len)
{
    char *dest;
    str->len = len;
    dest = str_alloc (str, len);
    return dest;
}

void str_free (string_t *str)
{
    free (str->str);
    *str = (string_t){0};
}

void str_debug_print (string_t *str)
{
    printf ("string_t: [SIMPLE]\n"
            "  data: %s\n"
            "  len: %"PRIu32"\n"
            "  capacity: %"PRIu32"\n",
            str->str, str->len, str->capacity);
}

#endif

#define str_dup(str) strn_new(str_data(str), str_len(str))
#define str_new(data) strn_new((data),((data)!=NULL?strlen(data):0))
string_t strn_new (const char *c_str, size_t len)
{
    string_t retval = {0};
    char *dest = str_init (&retval, len);

    if (c_str != NULL) {
        memmove (dest, c_str, len);
    }
    dest[len] = '\0';
    return retval;
}

#define str_set(str,c_str) strn_set(str,(c_str),((c_str)!=NULL?strlen(c_str):0))
void strn_set (string_t *str, const char *c_str, size_t len)
{
    str_maybe_grow (str, len, false);

    char *dest = str_data(str);
    if (c_str != NULL) {
        memmove (dest, c_str, len);
    }
    dest[len] = '\0';
}

void str_put (string_t *dest, size_t pos, string_t *str)
{
    str_maybe_grow (dest, pos + str_len(str), true);

    char *dst = str_data(dest) + pos;
    memmove (dst, str_data(str), str_len(str));
    dst[str_len(str)] = '\0';
}

#define str_put_c(dest,pos,c_str) strn_put_c(dest,pos,(c_str),((c_str)!=NULL?strlen(c_str):0))
void strn_put_c (string_t *dest, size_t pos, const char *c_str, size_t len)
{
    str_maybe_grow (dest, pos + len, true);

    char *dst = str_data(dest) + pos;
    if (c_str != NULL) {
        memmove (dst, c_str, len);
    }
    dst[len] = '\0';
}

void str_cpy (string_t *dest, string_t *src)
{
    size_t len = str_len(src);
    str_maybe_grow (dest, len, false);

    char *dest_data = str_data(dest);
    memmove (dest_data, str_data(src), len);
    dest_data[len] = '\0';
}

void str_cat (string_t *dest, string_t *src)
{
    size_t len_dest = str_len(dest);
    size_t len = len_dest + str_len(src);

    str_maybe_grow (dest, len, true);
    char *dest_data = str_data(dest);
    memmove (dest_data+len_dest, str_data(src), str_len(src));
    dest_data[len] = '\0';
}

#define str_cat_c(dest,c_str) strn_cat_c(dest,(c_str),((c_str)!=NULL?strlen(c_str):0))
void strn_cat_c (string_t *dest, const char *src, size_t len)
{
    size_t len_dest = str_len(dest);
    size_t total_len = len_dest + len;

    str_maybe_grow (dest, total_len, true);
    char *dest_data = str_data(dest);
    memmove (dest_data+len_dest, src, len);
    dest_data[total_len] = '\0';
}

void str_cat_indented (string_t *str1, string_t *str2, int num_spaces)
{
    if (str_len(str2) == 0) {
        return;
    }

    char *c = str_data(str2);
    for (int i=0; i<num_spaces; i++) {
        strn_cat_c (str1, " ", 1);
    }

    while (c && *c) {
        if (*c == '\n' && *(c+1) != '\n' && *(c+1) != '\0') {
            strn_cat_c (str1, "\n", 1);
            for (int i=0; i<num_spaces; i++) {
                strn_cat_c (str1, " ", 1);
            }

        } else {
            strn_cat_c (str1, c, 1);
        }
        c++;
    }
}

void str_cat_indented_c (string_t *str1, char *c_str, int num_spaces)
{
    if (*c_str == '\0') return;

    for (int i=0; i<num_spaces; i++) {
        strn_cat_c (str1, " ", 1);
    }

    while (c_str && *c_str) {
        if (*c_str == '\n' && *(c_str+1) != '\n' && *(c_str+1) != '\0') {
            strn_cat_c (str1, "\n", 1);
            for (int i=0; i<num_spaces; i++) {
                strn_cat_c (str1, " ", 1);
            }

        } else {
            strn_cat_c (str1, c_str, 1);
        }
        c_str++;
    }
}

void printf_indented (char *str, int num_spaces)
{
    char *c = str;

    for (int i=0; i<num_spaces; i++) {
        printf (" ");
    }

    while (c && *c) {
        if (*c == '\n' && *(c+1) != '\n' && *(c+1) != '\0') {
            printf ("\n");
            for (int i=0; i<num_spaces; i++) {
                printf (" ");
            }

        } else {
            // Use putc?
            printf ("%c", *c);
        }
        c++;
    }
}

char str_last (string_t *str)
{
    assert (str_len(str) > 0);
    return str_data(str)[str_len(str)-1];
}

GCC_PRINTF_FORMAT(3, 4)
void str_put_printf (string_t *str, size_t pos, const char *format, ...)
{
    va_list args1, args2;
    va_start (args1, format);
    va_copy (args2, args1);

    size_t size = vsnprintf (NULL, 0, format, args1) + 1;
    va_end (args1);

    char *tmp_str = malloc (size);
    vsnprintf (tmp_str, size, format, args2);
    va_end (args2);

    strn_put_c (str, pos, tmp_str, size - 1);

    free (tmp_str);
}

// These string functions use the printf syntax, this lets code be more concise.
#define _define_str_printf_func(FUNC_NAME,STRN_FUNC_NAME)       \
GCC_PRINTF_FORMAT(2, 3)                                         \
void FUNC_NAME (string_t *str, const char *format, ...)         \
{                                                               \
    va_list args1, args2;                                       \
    va_start (args1, format);                                   \
    va_copy (args2, args1);                                     \
                                                                \
    size_t size = vsnprintf (NULL, 0, format, args1) + 1;       \
    va_end (args1);                                             \
                                                                \
    char *tmp_str = malloc (size);                              \
    vsnprintf (tmp_str, size, format, args2);                   \
    va_end (args2);                                             \
                                                                \
    STRN_FUNC_NAME (str, tmp_str, size - 1);                    \
                                                                \
    free (tmp_str);                                             \
}

_define_str_printf_func(str_set_printf, strn_set)
_define_str_printf_func(str_cat_printf, strn_cat_c)

#undef _define_str_printf_func

//////////////////////
// PARSING UTILITIES
//
// These are some small functions useful when parsing text files
//
// FIXME: (API BREAK) Applications using consume_line() and consume_spaces()
// will break!, to make them work again copy the deprecated code into the
// application.
//
// Even thought this seemed like a good idea for a simple parsing API that does
// not require any state but a pointer to the current position, turns out it's
// impossible to program it in a way that allows to be used by people that want
// const AND non-const strings.
//
// If people are using a const string we must declare the function as:
//
//      const char* consume_line (const char *c);
//
// This will keep the constness of the original string. But, if people are using
// a non const string even though it will be casted to const without problem, we
// will still return a const pointer that will be most likely assigned to the
// same non const variable sent in the first place, here the const qualifier
// will be discarded and the compiler will complain. There is no way to say "if
// we are sent const return const, if it's non-const return non-const". Maybe in
// C++ with templates this can be done.
//
// Because we want to cause the least ammount of friction when using this
// library, making the user think about constdness defeats this purpose, so
// better just not use this.
//
// We could try to declare functions as:
//
//      bool consume_line (const char **c);
//
// But in C, char **c won't cast implicitly to const char **c. Also this has the
// problem that now it will unconditionally update c, wheras before the user had
// the choice either to update it immediately, or store it in a variable and
// update it afterwards, if necessary.
//
// Maybe what's needed is a proper scanner API that stores state inside a struct
// with more clear semantics. I'm still experimenting with different
// alternatives.

//static inline
//char* consume_line (char *c)
//{
//    while (*c && *c != '\n') {
//           c++;
//    }
//
//    if (*c) {
//        c++;
//    }
//
//    return c;
//}
//
//static inline
//char* consume_spaces (char *c)
//{
//    while (is_space(c)) {
//           c++;
//    }
//    return c;
//}

static inline
bool is_space (const char *c)
{
    return *c == ' ' ||  *c == '\t';
}

static inline
bool is_end_of_line_or_file (const char *c)
{
    while (is_space(c)) {
           c++;
    }
    return *c == '\n' || *c == '\0';
}

static inline
bool is_end_of_line (const char *c)
{
    while (is_space(c)) {
           c++;
    }
    return *c == '\n';
}

// Set POSIX locale while storing the previous one. Useful while calling strtod
// and you know the decimal separator will always be '.', and you don't want it
// to break if the user changes locale.
// NOTE: Must be followed by a call to restore_locale() to avoid memory leaks
char* begin_posix_locale ()
{
    char *old_locale = NULL;
    old_locale = strdup (setlocale (LC_ALL, NULL));
    assert (old_locale != NULL);
    setlocale (LC_ALL, "POSIX");

    return old_locale;
}

// Restore the previous locale returned by begin_posix_locale
void restore_locale (char *old_locale)
{
    setlocale (LC_ALL, old_locale);
    free (old_locale);
}

#define VECT_X 0
#define VECT_Y 1
#define VECT_Z 2
#define VECT_W 3

#define VECT_R 0
#define VECT_G 1
#define VECT_B 2
#define VECT_A 3

typedef union {
    struct {
        double x;
        double y;
    };
    struct {
        double w;
        double h;
    };
    double E[2];
} dvec2;
#define DVEC2(x,y) ((dvec2){{x,y}})
#define CAST_DVEC2(v) (dvec2){{(double)v.x,(double)v.y}}

// TODO: These are 128 bit structs, they may take up a lot of space, maybe have
// another one of 32 bits for small coordinates.
// TODO: Also, how does this affect performance?
typedef union {
    struct {
        int64_t x;
        int64_t y;
    };
    int64_t E[2];
} ivec2;
#define IVEC2(x,y) ((ivec2){{x,y}})
#define CAST_IVEC2(v) (ivec2){{(int64_t)v.x,(int64_t)v.y}}

#define vec_equal(v1,v2) ((v1)->x == (v2)->x && (v1)->y == (v2)->y)

typedef struct {
    dvec2 min;
    dvec2 max;
} box_t;

void box_print (box_t *b)
{
    printf ("min: (%f, %f), max: (%f, %f)\n", b->min.x, b->min.y, b->max.x, b->max.y);
}

void dvec2_floor (dvec2 *p)
{
    p->x = floor (p->x);
    p->y = floor (p->y);
}

void dvec2_round (dvec2 *p)
{
    p->x = round (p->x);
    p->y = round (p->y);
}

static inline
dvec2 dvec2_add (dvec2 v1, dvec2 v2)
{
    dvec2 res;
    res.x = v1.x+v2.x;
    res.y = v1.y+v2.y;
    return res;
}

static inline
void dvec2_add_to (dvec2 *v1, dvec2 v2)
{
    v1->x += v2.x;
    v1->y += v2.y;
}

static inline
dvec2 dvec2_subs (dvec2 v1, dvec2 v2)
{
    dvec2 res;
    res.x = v1.x-v2.x;
    res.y = v1.y-v2.y;
    return res;
}

static inline
void dvec2_subs_to (dvec2 *v1, dvec2 v2)
{
    v1->x -= v2.x;
    v1->y -= v2.y;
}

static inline
dvec2 dvec2_mult (dvec2 v, double k)
{
    dvec2 res;
    res.x = v.x*k;
    res.y = v.y*k;
    return res;
}

static inline
void dvec2_mult_to (dvec2 *v, double k)
{
    v->x *= k;
    v->y *= k;
}

static inline
double dvec2_dot (dvec2 v1, dvec2 v2)
{
    return v1.x*v2.x + v1.y*v2.y;
}

static inline
double dvec2_norm (dvec2 v)
{
    return sqrt ((v.x)*(v.x) + (v.y)*(v.y));
}

double area_2 (dvec2 a, dvec2 b, dvec2 c)
{
    return (b.x-a.x)*(c.y-a.y) - (c.x-a.x)*(b.y-a.y);
}

// true if point c is to the left of a-->b
bool left (dvec2 a, dvec2 b, dvec2 c)
{
    return area_2 (a, b, c) > 0;
}

// true if vector p points to the left of vector a
#define left_dvec2(a,p) left(DVEC2(0,0),a,p)

// true if point c is to the left or in a-->b
bool left_on (dvec2 a, dvec2 b, dvec2 c)
{
    return area_2 (a, b, c) > -0.01;
}

// Angle from v1 to v2 clockwise.
//
// NOTE: The return value is in the range [0, 2*M_PI).
static inline
double dvec2_clockwise_angle_between (dvec2 v1, dvec2 v2)
{
    if (vec_equal (&v1, &v2)) {
        return 0;
    } else if (left_dvec2 (v2, v1))
        return acos (dvec2_dot (v1, v2)/(dvec2_norm (v1)*dvec2_norm(v2)));
    else
        return 2*M_PI - acos (dvec2_dot (v1, v2)/(dvec2_norm (v1)*dvec2_norm(v2)));
}

// Smallest angle between v1 to v2.
//
// NOTE: The return value is in the range [0, M_PI].
static inline
double dvec2_angle_between (dvec2 v1, dvec2 v2)
{
    if (vec_equal (&v1, &v2)) {
        return 0;
    } else
        return acos (dvec2_dot (v1, v2)/(dvec2_norm (v1)*dvec2_norm(v2)));
}

static inline
void dvec2_normalize (dvec2 *v)
{
    double norm = dvec2_norm (*v);
    assert (norm != 0);
    v->x /= norm;
    v->y /= norm;
}

static inline
void dvec2_normalize_or_0 (dvec2 *v)
{
    double norm = dvec2_norm (*v);
    if (norm == 0) {
        v->x = 0;
        v->y = 0;
    } else {
        v->x /= norm;
        v->y /= norm;
    }
}

dvec2 dvec2_clockwise_rotate (dvec2 v, double rad)
{
    dvec2 res;
    res.x =  v.x*cos(rad) + v.y*sin(rad);
    res.y = -v.x*sin(rad) + v.y*cos(rad);
    return res;
}

void dvec2_clockwise_rotate_on (dvec2 *v, double rad)
{
    dvec2 tmp = *v;
    v->x =  tmp.x*cos(rad) + tmp.y*sin(rad);
    v->y = -tmp.x*sin(rad) + tmp.y*cos(rad);
}

static inline
double dvec2_distance (dvec2 *v1, dvec2 *v2)
{
    if (vec_equal(v1, v2)) {
        return 0;
    } else {
        return sqrt ((v1->x-v2->x)*(v1->x-v2->x) + (v1->y-v2->y)*(v1->y-v2->y));
    }
}

void dvec2_print (dvec2 *v)
{
    printf ("(%f, %f) [%f]\n", v->x, v->y, dvec2_norm(*v));
}

// NOTE: W anf H MUST be positive.
#define BOX_X_Y_W_H(box,n_x,n_y,n_w,n_h) {     \
    (box).min.x=(n_x);(box).max.x=(n_x)+(n_w); \
    (box).min.y=(n_y);(box).max.y=(n_y)+(n_h); \
}

#define BOX_CENTER_X_Y_W_H(box,n_x,n_y,n_w,n_h) {(box).min.x=(n_x)-(n_w)/2;(box).max.x=(n_x)+(n_w)/2; \
                                                 (box).min.y=(n_y)-(n_h)/2;(box).max.y=(n_y)+(n_h)/2;}
#define BOX_POS_SIZE(box,pos,size) BOX_X_Y_W_H(box,(pos).x,(pos).y,(size).x,(size).y)

#define BOX_WIDTH(box) ((box).max.x-(box).min.x)
#define BOX_HEIGHT(box) ((box).max.y-(box).min.y)
#define BOX_AR(box) (BOX_WIDTH(box)/BOX_HEIGHT(box))

typedef union {
    struct {
        float x;
        float y;
        float z;
    };

    struct {
        float r;
        float g;
        float b;
    };
    float E[3];
} fvec3;
#define FVEC3(x,y,z) ((fvec3){{x,y,z}})

static inline
double fvec3_dot (fvec3 v1, fvec3 v2)
{
    return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;
}

static inline
fvec3 fvec3_cross (fvec3 v1, fvec3 v2)
{
    fvec3 res;
    res.x = v1.y*v2.z - v1.z*v2.y;
    res.y = v1.z*v2.x - v1.x*v2.z;
    res.z = v1.x*v2.y - v1.y*v2.x;
    return res;
}

static inline
fvec3 fvec3_subs (fvec3 v1, fvec3 v2)
{
    fvec3 res;
    res.x = v1.x-v2.x;
    res.y = v1.y-v2.y;
    res.z = v1.z-v2.z;
    return res;
}

static inline
fvec3 fvec3_mult (fvec3 v, float k)
{
    fvec3 res;
    res.x = v.x*k;
    res.y = v.y*k;
    res.z = v.z*k;
    return res;
}

static inline
fvec3 fvec3_mult_to (fvec3 *v, float k)
{
    fvec3 res;
    v->x = v->x*k;
    v->y = v->y*k;
    v->z = v->z*k;
    return res;
}

static inline
double fvec3_norm (fvec3 v)
{
    return sqrt ((v.x)*(v.x) + (v.y)*(v.y) + (v.z)*(v.z));
}

static inline
fvec3 fvec3_normalize (fvec3 v)
{
    fvec3 res;
    double norm = fvec3_norm (v);
    assert (norm != 0);
    res.x = v.x/norm;
    res.y = v.y/norm;
    res.z = v.z/norm;
    return res;
}

void fvec3_print_norm (fvec3 v)
{
    printf ("(%f, %f, %f) [%f]\n", v.x, v.y, v.z, fvec3_norm(v));
}

void fvec3_print (fvec3 v)
{
    printf ("(%f, %f, %f)\n", v.x, v.y, v.z);
}

typedef union {
    struct {
        double x;
        double y;
        double z;
    };

    struct {
        double r;
        double g;
        double b;
    };
    double E[3];
} dvec3;
#define DVEC3(x,y,z) ((dvec3){{x,y,z}})

static inline
double dvec3_dot (dvec3 v1, dvec3 v2)
{
    return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;
}

static inline
dvec3 dvec3_cross (dvec3 v1, dvec3 v2)
{
    dvec3 res;
    res.x = v1.y*v2.z - v1.z*v2.y;
    res.y = v1.z*v2.x - v1.x*v2.z;
    res.z = v1.x*v2.y - v1.y*v2.x;
    return res;
}

static inline
dvec3 dvec3_subs (dvec3 v1, dvec3 v2)
{
    dvec3 res;
    res.x = v1.x-v2.x;
    res.y = v1.y-v2.y;
    res.z = v1.z-v2.z;
    return res;
}

static inline
dvec3 dvec3_mult (dvec3 v, float k)
{
    dvec3 res;
    res.x = v.x*k;
    res.y = v.y*k;
    res.z = v.z*k;
    return res;
}

static inline
dvec3 dvec3_mult_to (dvec3 *v, float k)
{
    dvec3 res;
    v->x = v->x*k;
    v->y = v->y*k;
    v->z = v->z*k;
    return res;
}

static inline
double dvec3_norm (dvec3 v)
{
    return sqrt ((v.x)*(v.x) + (v.y)*(v.y) + (v.z)*(v.z));
}

static inline
dvec3 dvec3_normalize (dvec3 v)
{
    dvec3 res;
    double norm = dvec3_norm (v);
    assert (norm != 0);
    res.x = v.x/norm;
    res.y = v.y/norm;
    res.z = v.z/norm;
    return res;
}

void dvec3_print (dvec3 v)
{
    printf ("(%f, %f, %f) [%f]\n", v.x, v.y, v.z, dvec3_norm(v));
}

typedef union {
    struct {
        double x;
        double y;
        double z;
        double w;
    };

    struct {
        double r;
        double g;
        double b;
        double a;
    };

    struct {
        double h;
        double s;
        double l;
    };
    double E[4];
} dvec4;
#define DVEC4(x,y,z,w) ((dvec4){{x,y,z,w}})

void dvec4_print (dvec4 *v)
{
    printf ("(%f, %f, %f, %f)", v->x, v->y, v->z, v->w);
}

// NOTE: Transpose before uploading to OpenGL!!
typedef union {
    float E[16];
    float M[4][4]; // [down][right]
} mat4f;

// NOTE: Camera will be looking towards -Cz.
static inline
mat4f camera_matrix (dvec3 Cx, dvec3 Cy, dvec3 Cz, dvec3 Cpos)
{
    mat4f matrix;
    int i = 0, j;
    for (j=0; j<3; j++) {
        matrix.E[i*4+j] = Cx.E[j];
    }

    matrix.E[i*4+j] = -dvec3_dot (Cpos, Cx);

    i = 1;
    for (j=0; j<3; j++) {
        matrix.E[i*4+j] = Cy.E[j];
    }
    matrix.E[i*4+j] = -dvec3_dot (Cpos, Cy);

    i = 2;
    for (j=0; j<3; j++) {
        matrix.E[i*4+j] = -Cz.E[j];
    }
    matrix.E[i*4+j] = dvec3_dot (Cpos, Cz);

    i = 3;
    for (j=0; j<3; j++) {
        matrix.E[i*4+j] = 0;
    }
    matrix.E[i*4+j] = 1;
    return matrix;
}

static inline
mat4f look_at (dvec3 camera, dvec3 target, dvec3 up)
{
    dvec3 Cz = dvec3_normalize (dvec3_subs (camera, target));
    dvec3 Cx = dvec3_normalize (dvec3_cross (up, Cz));
    dvec3 Cy = dvec3_cross (Cz, Cx);
    return camera_matrix (Cx, Cy, Cz, camera);
}

static inline
mat4f rotation_x (float angle_r)
{
    float s = sinf(angle_r);
    float c = cosf(angle_r);
    mat4f res = {{
         1, 0, 0, 0,
         0, c,-s, 0,
         0, s, c, 0,
         0, 0, 0, 1
    }};
    return res;
}

static inline
mat4f rotation_y (float angle_r)
{
    float s = sinf(angle_r);
    float c = cosf(angle_r);
    mat4f res = {{
         c, 0, s, 0,
         0, 1, 0, 0,
        -s, 0, c, 0,
         0, 0, 0, 1
    }};
    return res;
}

static inline
mat4f rotation_z (float angle_r)
{
    float s = sinf(angle_r);
    float c = cosf(angle_r);
    mat4f res = {{
         c,-s, 0, 0,
         s, c, 0, 0,
         0, 0, 1, 0,
         0, 0, 0, 1
    }};
    return res;
}

void mat4f_print (mat4f mat)
{
    int i;
    for (i=0; i<4; i++) {
        int j;
        for (j=0; j<4; j++) {
            printf ("%f, ", mat.E[i*4+j]);
        }
        printf ("\n");
    }
    printf ("\n");
}

static inline
mat4f perspective_projection (float left, float right, float bottom, float top, float near, float far)
{
    // NOTE: There are several conventions for the semantics of the near and far
    // arguments to this function:
    //  - OpenGL assumes them to be distances to the camera and fails if either
    //    near or far is negative. This may be confusing because the other
    //    values are assumed to be coordinates and near and far are not,
    //    otherwise they would be negative because OpenGL uses RH coordinates.
    //  - We can make all argumets be the coordinates, then if the near plane or
    //    far plane coordinates are positive we throw an error.
    //  - A third approach is to mix both of them by taking the absolute value
    //    of the near and far plane and never fail. This works fine if they are
    //    interpreted as being Z coordinates, or distances to the camera at the
    //    cost of computing two absolute values.
    near = fabs (near);
    far = fabs (far);

    float a = 2*near/(right-left);
    float b = -(right+left)/(right-left);

    float c = 2*near/(top-bottom);
    float d = -(top+bottom)/(top-bottom);

    float e = (near+far)/(far-near);
    float f = -2*far*near/(far-near);

    mat4f res = {{
        a, 0, b, 0,
        0, c, d, 0,
        0, 0, e, f,
        0, 0, 1, 0,
    }};
    return res;
}

static inline
mat4f mat4f_mult (mat4f mat1, mat4f mat2)
{
    mat4f res = ZERO_INIT(mat4f);
    int i;
    for (i=0; i<4; i++) {
        int j;
        for (j=0; j<4; j++) {
            int k;
            for (k=0; k<4; k++) {
                res.M[i][j] += mat1.M[i][k] * mat2.M[k][j];
            }
        }
    }
    return res;
}

static inline
dvec3 mat4f_times_point (mat4f mat, dvec3 p)
{
    dvec3 res = DVEC3(0,0,0);
    int i;
    for (i=0; i<3; i++) {
        int j;
        for (j=0; j<3; j++) {
            res.E[i] += mat.M[i][j] * p.E[j];
        }
    }

    for (i=0; i<3; i++) {
        res.E[i] += mat.M[i][3];
    }
    return res;
}

// The resulting transform sends s1 and s2 to d1 and d2 respectiveley.
//   s1 = res * d1
//   s2 = res * d2
static inline
mat4f transform_from_2_points (dvec3 s1, dvec3 s2, dvec3 d1, dvec3 d2)
{
    float xs, x0, ys, y0, zs, z0;
    if (s1.x != s2.x) {
        xs = (d1.x - d2.x)/(s1.x - s2.x);
        x0 = (d2.x*s1.x - d2.x*s2.x - d1.x*s2.x + d2.x*s2.x)/(s1.x - s2.x);
    } else {
        xs = 1;
        x0 = 0;
    }

    if (s1.y != s2.y) {
        ys = (d1.y - d2.y)/(s1.y - s2.y);
        y0 = (d2.y*s1.y - d2.y*s2.y - d1.y*s2.y + d2.y*s2.y)/(s1.y - s2.y);
    } else {
        ys = 1;
        y0 = 0;
    }

    if (s1.z != s2.z) {
        zs = (d1.z - d2.z)/(s1.z - s2.z);
        z0 = (d2.z*s1.z - d2.z*s2.z - d1.z*s2.z + d2.z*s2.z)/(s1.z - s2.z);
    } else {
        zs = 1;
        z0 = 0;
    }

    mat4f res = {{
        xs, 0, 0,x0,
         0,ys, 0,y0,
         0, 0,zs,z0,
         0, 0, 0, 1
    }};
    return res;
}

// This may as well be a 3x3 matrix but sometimes the semantics are simpler like
// this. Of course this is subjective.
typedef struct {
    double scale_x;
    double scale_y;
    double dx;
    double dy;
} transf_t;

void apply_transform (transf_t *tr, dvec2 *p)
{
    p->x = tr->scale_x*p->x + tr->dx;
    p->y = tr->scale_y*p->y + tr->dy;
}

void apply_transform_distance (transf_t *tr, dvec2 *p)
{
    p->x = tr->scale_x*p->x;
    p->y = tr->scale_y*p->y;
}

void apply_inverse_transform (transf_t *tr, dvec2 *p)
{
    p->x = (p->x - tr->dx)/tr->scale_x;
    p->y = (p->y - tr->dy)/tr->scale_y;
}

void apply_inverse_transform_distance (transf_t *tr, dvec2 *p)
{
    p->x = p->x/tr->scale_x;
    p->y = p->y/tr->scale_y;
}

void transform_translate (transf_t *tr, dvec2 *delta)
{
    tr->dx += delta->x;
    tr->dy += delta->y;
}

// Calculates a ratio by which multiply box a so that it fits inside box b
double best_fit_ratio (double a_width, double a_height,
                       double b_width, double b_height)
{
    if (a_width/a_height < b_width/b_height) {
        return b_height/a_height;
    } else {
        return b_width/a_width;
    }
}

void compute_best_fit_box_to_box_transform (transf_t *tr, box_t *src, box_t *dest)
{
    double src_width = BOX_WIDTH(*src);
    double src_height = BOX_HEIGHT(*src);
    double dest_width = BOX_WIDTH(*dest);
    double dest_height = BOX_HEIGHT(*dest);

    tr->scale_x = best_fit_ratio (src_width, src_height,
                                dest_width, dest_height);
    tr->scale_y = tr->scale_x;
    tr->dx = dest->min.x + (dest_width-src_width*tr->scale_x)/2;
    tr->dy = dest->min.y + (dest_height-src_height*tr->scale_y)/2;
}

void swap (int*a, int*b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

// Merge sort implementation
void int_sort (int *arr, int n)
{
    if (n==1) {
        return;
    } else if (n == 2) {
        if (arr[1] < arr[0]) {
            swap (&arr[0], &arr[1]);
        }
    } else if (n==3) {
        if (arr[0] > arr[1]) swap(&arr[0],&arr[1]);
        if (arr[1] > arr[2]) swap(&arr[1],&arr[2]);
        if (arr[0] > arr[1]) swap(&arr[0],&arr[1]);
    } else {
        int res[n];
        int_sort (arr, n/2);
        int_sort (&arr[n/2], n-n/2);

        int i;
        int a=0;
        int b=n/2;
        for (i=0; i<n; i++) {
            if (b==n || (a<n/2 && arr[a] < arr[b])) {
                res[i] = arr[a];
                a++;
            } else {
                res[i] = arr[b];
                b++;
            }
        }
        for (i=0; i<n; i++) {
            arr[i] = res[i];
        }
    }
}

void swap_n_bytes (void *a, void*b, uint32_t n)
{
    while (sizeof(int)<=n) {
        n -= sizeof(int);
        int *a_c = (int*)((char*)a+n);
        int *b_c = (int*)((char*)b+n);
        *a_c = *a_c^*b_c;
        *b_c = *a_c^*b_c;
        *a_c = *a_c^*b_c;
    }

    if (n<0) {
        n += sizeof(int);
        while (n<0) {
            n--;
            char *a_c = (char*)a+n;
            char *b_c = (char*)b+n;
            *a_c = *a_c^*b_c;
            *b_c = *a_c^*b_c;
            *a_c = *a_c^*b_c;
        }
    }
}

// Templetized merge sort for arrays
//
// IS_A_LT_B is an expression where a and b are pointers
// to _arr_ true when *a<*b.
// NOTE: IS_A_LT_B as defined, will sort the array in ascending order.
#define templ_sort(FUNCNAME,TYPE,IS_A_LT_B)                                       \
void FUNCNAME ## _user_data (TYPE *arr, int n, void *user_data)                   \
{                                                                                 \
    if (arr == NULL || n<=1) {                                                    \
        return;                                                                   \
    } else if (n == 2) {                                                          \
        TYPE *a = &arr[1];                                                        \
        TYPE *b = &arr[0];                                                        \
        int c = IS_A_LT_B;                                                        \
        if (c) {                                                                  \
            swap_n_bytes (&arr[0], &arr[1], sizeof(TYPE));                        \
        }                                                                         \
    } else {                                                                      \
        TYPE res[n];                                                              \
        FUNCNAME ## _user_data (arr, n/2, user_data);                             \
        FUNCNAME ## _user_data (&arr[n/2], n-n/2, user_data);                     \
                                                                                  \
        /* Merge halfs until one of them runs out*/                               \
        int i;                                                                    \
        int h=0;                                                                  \
        int k=n/2;                                                                \
        for (i=0; k<n && h<n/2; i++) {                                            \
            TYPE *a = &arr[h];                                                    \
            TYPE *b = &arr[k];                                                    \
            int c = IS_A_LT_B;                                                    \
            if (c) {                                                              \
                res[i] = arr[h];                                                  \
                h++;                                                              \
            } else {                                                              \
                res[i] = arr[k];                                                  \
                k++;                                                              \
            }                                                                     \
        }                                                                         \
                                                                                  \
        /* If there are elements remaining in one half copy them. Separating
         * loop this way avoids evaluating CMP_A_TO_B for invalid elements (past
         * the maximum value for h).
         */                                                                       \
        int rest_idx = k==n ? h : k;                                              \
        for (; i<n; i++) {                                                        \
            res[i] = arr[rest_idx];                                               \
            rest_idx++;                                                           \
        }                                                                         \
                                                                                  \
        /* Copy the sorted array back from the temporary array. */                \
        for (i=0; i<n; i++) {                                                     \
            arr[i] = res[i];                                                      \
        }                                                                         \
    }                                                                             \
}                                                                                 \
                                                                                  \
void FUNCNAME(TYPE *arr, int n) {                                                 \
    FUNCNAME ## _user_data (arr,n,NULL);                                          \
}

// Stable templetized merge sort for arrays
//
// CMP_A_TO_B is an expression where a and b are pointers to _arr_, it's equal
// to -1, 0 or 1 if *a and *b compare as less than, equal to or grater than
// respectively.
//
// The reasoning behind this being separate from templ_sort() is that a stable
// sort requires a 3-way comparison, which makes it a little more inconvinient
// to instantiate. Writing a IS_A_LT_B expression is much more straightforward
// than a CMP_A_TO_B expression.
//
// NOTE: CMP_A_TO_B as defined, will sort the array in ascending order.
#define templ_sort_stable(FUNCNAME,TYPE,CMP_A_TO_B)                               \
void FUNCNAME ## _user_data (TYPE *arr, int n, void *user_data)                   \
{                                                                                 \
    if (arr == NULL || n<=1) {                                                    \
        return;                                                                   \
    } else if (n == 2) {                                                          \
        TYPE *a = &arr[1];                                                        \
        TYPE *b = &arr[0];                                                        \
        int c = CMP_A_TO_B;                                                       \
        if (c == -1) {                                                            \
            swap_n_bytes (&arr[0], &arr[1], sizeof(TYPE));                        \
        }                                                                         \
                                                                                  \
    } else {                                                                      \
        TYPE res[n];                                                              \
        FUNCNAME ## _user_data (arr, n/2, user_data);                             \
        FUNCNAME ## _user_data (&arr[n/2], n-n/2, user_data);                     \
                                                                                  \
        /* Merge halfs until one of them runs out*/                               \
        int i;                                                                    \
        int h=0;                                                                  \
        int k=n/2;                                                                \
        for (i=0; k<n && h<n/2; i++) {                                            \
            TYPE *a = &arr[h];                                                    \
            TYPE *b = &arr[k];                                                    \
            int c = CMP_A_TO_B;                                                   \
            if (c < 1) {                                                          \
                res[i] = arr[h];                                                  \
                h++;                                                              \
            } else {                                                              \
                res[i] = arr[k];                                                  \
                k++;                                                              \
            }                                                                     \
        }                                                                         \
                                                                                  \
        /* If there are elements remaining in one half copy them. Separating
         * loop this way avoids evaluating CMP_A_TO_B for invalid elements, past
         * the end of the array.
         */                                                                       \
        int rest_idx = k==n ? h : k;                                              \
        for (; i<n; i++) {                                                        \
            res[i] = arr[rest_idx];                                               \
            rest_idx++;                                                           \
        }                                                                         \
                                                                                  \
        /* Copy the sorted array back from the temporary array. */                \
        for (i=0; i<n; i++) {                                                     \
            arr[i] = res[i];                                                      \
        }                                                                         \
    }                                                                             \
}                                                                                 \
                                                                                  \
void FUNCNAME(TYPE *arr, int n) {                                                 \
    FUNCNAME ## _user_data (arr,n,NULL);                                          \
}

typedef struct {
    int origin;
    int key;
} int_key_t;

void int_key_print (int_key_t k)
{
    printf ("origin: %d, key: %d\n", k.origin, k.key);
}

templ_sort (sort_int_keys, int_key_t, a->key < b->key)

bool in_array (int i, int* arr, int size)
{
    while (size) {
        size--;
        if (arr[size] == i) {
            return true;
        }
    }
    return false;
}

void array_clear (int *arr, int n)
{
    while (n) {
        n--;
        arr[n] = 0;
    }
}

void array_print_full (int *arr, int n, const char *sep, const char *start, const char *end)
{
    int i=0;
    if (start != NULL) {
        printf ("%s", start);
    }

    if (n>0) {
        if (sep != NULL) {
            for (i=0; i<n-1; i++) {
                printf ("%d%s", arr[i], sep);
            }
        } else {
            for (i=0; i<n-1; i++) {
                printf ("%d", arr[i]);
            }
        }

        printf ("%d", arr[i]);
    }

    if (end != NULL) {
        printf ("%s", end);
    }
}

#define INT_ARR_PRINT_CALLBACK(name) void name(int *arr, int n)
typedef INT_ARR_PRINT_CALLBACK(int_arr_print_callback_t);

// NOTE: As long as we use array_print() as an INT_ARR_PRINT_CALLBACK, this needs
// to be a function and not a macro.
void array_print(int *arr, int n) {
    array_print_full(arr,n," ",NULL,"\n");
}

void sorted_array_print (int *arr, int n)
{
    int sorted[n];
    memcpy (sorted, arr, n*sizeof(int));
    int_sort (sorted, n);
    array_print (sorted, n);
}

// Does binary search over arr. If n is not present, insert it at it's sorted
// position. If n is found in arr, do nothing.
void int_array_set_insert (int n, int *arr, int *arr_len, int arr_max_size)
{
    bool found = false;
    int low = 0;
    int up = *arr_len;
    while (low != up) {
        int mid = (low + up)/2;
        if (n == arr[mid]) {
            found = true;
            break;
        } else if (n < arr[mid]) {
            up = mid;
        } else {
            low = mid + 1;
        }
    }

    if (!found) {
        assert (*arr_len < arr_max_size - 1);
        uint32_t i;
        for (i=*arr_len; i>low; i--) {
            arr[i] = arr[i-1];
        }
        arr[low] = n;
        (*arr_len)++;
    }
}

void print_u64_array (uint64_t *arr, int n)
{
    printf ("[");
    int i;
    for (i=0; i<n-1; i++) {
        printf ("%"PRIu64", ", arr[i]);
    }
    printf ("%"PRIu64"]\n", arr[i]);
}

void print_line (const char *sep, int len)
{
    int w = strlen(sep);
    char str[w*len+1];
    int i;
    for (i=0; i<len; i++) {
        memcpy (str+i*w, sep, w);
    }
    str[w*len] = '\0';
    printf ("%s", str);
}

struct ascii_tbl_t {
    const char* vert_sep;
    const char* hor_sep;
    const char* cross;
    int curr_col;
    int num_cols;
};

void print_table_bar (const char *hor_sep, const char* cross, int *lens, int num_cols)
{
    int i;
    for (i=0; i<num_cols-1; i++) {
        print_line (hor_sep, lens[i]);
        printf ("%s", cross);
    }
    print_line (hor_sep, lens[i]);
    printf ("\n");
}

void ascii_tbl_header (struct ascii_tbl_t *tbl, char **titles, int *widths, int num_cols)
{
    if (tbl->vert_sep == NULL) {
        tbl->vert_sep = " | ";
    }

    if (tbl->hor_sep == NULL) {
        tbl->hor_sep = "-";
    }

    if (tbl->cross == NULL) {
        tbl->cross = "-|-";
    }

    tbl->num_cols = num_cols;
    int i;
    for (i=0; i<num_cols-1; i++) {
        widths[i] = MAX(widths[i], (int)strlen(titles[i]));
        printf ("%*s%s", widths[i], titles[i], tbl->vert_sep);
    }
    widths[i] = MAX(widths[i], (int)strlen(titles[i]));
    printf ("%*s\n", widths[i], titles[i]);
    print_table_bar (tbl->hor_sep, tbl->cross, widths, num_cols);
}

void ascii_tbl_sep (struct ascii_tbl_t *tbl)
{
    if (tbl->curr_col < tbl->num_cols-1) {
        tbl->curr_col++;
        printf ("%s", tbl->vert_sep);
    } else {
        tbl->curr_col = 0;
        printf ("\n");
    }
}

// Function that returns an integer in [min,max] with a uniform distribution.
// NOTE: Remember to call srand() ONCE before using this.
#define rand_int_max(max) rand_int_range(0,max)
uint32_t rand_int_range (uint32_t min, uint32_t max)
{
    assert (max < RAND_MAX);
    assert (min < max);
    uint32_t res;
    uint32_t valid_range = RAND_MAX-(RAND_MAX%(max-min+1));
    do {
        res = rand ();
    } while (res >= valid_range);
    return min + res/(valid_range/(max-min+1));
}

// NOTE: Remember to call srand() ONCE before using this.
void fisher_yates_shuffle (int *arr, int n)
{
    int i;
    for (i=n-1; i>0; i--) {
        int j = rand_int_max (i);
        swap (arr+i, arr+j);
    }
}

// NOTE: Remember to call srand() ONCE before using this.
void init_random_array (int *arr, int size)
{
    int i;
    for (i=0; i<size; i++) {
        arr[i] = i;
    }
    fisher_yates_shuffle (arr, size);
}

// TODO: Make this zero initialized in all cases
typedef struct {
    uint32_t size;
    uint32_t len;
    int *data;
} int_dyn_arr_t;

void int_dyn_arr_init (int_dyn_arr_t *arr, uint32_t size)
{
    arr->data = (int*)malloc (size * sizeof (*arr->data));
    if (!arr->data) {
        printf ("Malloc failed.\n");
    }
    arr->len = 0;
    arr->size = size;
}

void int_dyn_arr_destroy (int_dyn_arr_t *arr)
{
    free (arr->data);
    *arr = (int_dyn_arr_t){0};
}

void int_dyn_arr_grow (int_dyn_arr_t *arr, uint32_t new_size)
{
    assert (new_size < UINT32_MAX);
    int *new_data;
    if ((new_data = (int*)realloc (arr->data, new_size * sizeof(*arr->data)))) {
        arr->data = new_data;
        arr->size = new_size;
    } else {
        printf ("Error: Realloc failed.\n");
        return;
    }
}

void int_dyn_arr_append (int_dyn_arr_t *arr, int element)
{
    if (arr->size == 0) {
        int_dyn_arr_init (arr, 100);
    }

    if (arr->len == arr->size) {
        int_dyn_arr_grow (arr, 2*arr->size);
    }
    arr->data[arr->len++] = element;
}

void int_dyn_arr_insert_and_swap (int_dyn_arr_t *arr, uint32_t pos, int element)
{
    int_dyn_arr_append (arr, element);
    swap (&arr->data[pos], &arr->data[arr->len-1]);
}

void int_dyn_arr_insert_and_shift (int_dyn_arr_t *arr, uint32_t pos, int element)
{
    assert (pos < arr->len);
    if (arr->len == arr->size) {
        int_dyn_arr_grow (arr, 2*arr->size);
    }
    
    uint32_t i;
    for (i=arr->len; i>pos; i--) {
        arr->data[i] = arr->data[i-1];
    }
    arr->data[pos] = element;
    arr->len++;
}

void int_dyn_arr_insert_multiple_and_shift (int_dyn_arr_t *arr, uint32_t pos, int *elements, uint32_t len)
{
    assert (pos < arr->len);
    if (arr->len + len > arr->size) {
        uint32_t new_size = arr->size;
        while (new_size < arr->len + len) {
            new_size *= 2;
        }
        int_dyn_arr_grow (arr, new_size);
    }

    uint32_t i;
    for (i=arr->len+len-1; i>pos+len-1; i--) {
        arr->data[i] = arr->data[i-len];
    }

    for (i=0; i<len; i++) {
        arr->data[pos] = elements[i];
        pos++;
    }
    arr->len += len;
}

void int_dyn_arr_print (int_dyn_arr_t *arr)
{
    array_print (arr->data, arr->len);
}

// This is a simple contiguous buffer, arbitrary sized objects are pushed and
// it grows automatically doubling it's previous size.
//
// NOTE: We use zero is initialization unless min_size is used
// WARNING: Don't create pointers into this structure because they won't work
// after a realloc happens.
#define CONT_BUFF_MIN_SIZE 1024
typedef struct {
    uint32_t min_size;
    uint32_t size;
    uint32_t used;
    void *data;
} cont_buff_t;

void* cont_buff_push (cont_buff_t *buff, int size)
{
    if (buff->used + size >= buff->size) {
        void *new_data;
        if (buff->size == 0) {
            int new_size = MAX (CONT_BUFF_MIN_SIZE, buff->min_size);
            if ((buff->data = malloc (new_size))) {
                buff->size = new_size;
            } else {
                printf ("Malloc failed.\n");
            }
        } else if ((new_data = realloc (buff->data, 2*buff->size))) {
            buff->data = new_data;
            buff->size = 2*buff->size;
        } else {
            printf ("Error: Realloc failed.\n");
            return NULL;
        }
    }

    void *ret = (uint8_t*)buff->data + buff->used;
    buff->used += size;
    return ret;
}

// NOTE: The same cont_buff_t can be used again after this.
void cont_buff_destroy (cont_buff_t *buff)
{
    free (buff->data);
    buff->data = NULL;
    buff->size = 0;
    buff->used = 0;
}

// Memory pool that grows as needed, and can be freed easily.
#define MEM_POOL_DEFAULT_MIN_BIN_SIZE 1024u
typedef struct {
    uint32_t min_bin_size;
    uint32_t size;
    uint32_t used;
    void *base;

    // total_data is the total used memory minus the memory used for
    // on_destroy_callback_info_t structs. We use this variable to compute the
    // ammount of empty space left in previous bins.
    uint32_t total_data;
    uint32_t num_bins;
} mem_pool_t;

// Sometimes we want to execute code when something we allocated in a pool gets
// destroyed, that's what this callback is for. The alloceted argument will
// point to the allocated memory when the callback was assigned.
//
// NOTE: When destroying a pool (or part of it when using temporary memory), ALL
// destruction callbacks will be called BEFORE we free any memory from the pool.
// The reasoning behind this is that maybe these callback functions would like
// to use data inside things being destroyed. At the moment it seems like the
// most sensible thing to do, but there may be cases where it isn't, maybe it
// could increase cache misses? I don't know.
//
// NOTE: I reluctantly added closures to ON_DESTROY_CALLBACK. For
// str_set_pooled() we need a way to pass the string to be freed. Heavy use of
// closures here is a slippery slope. Expecting to be able to run any code from
// here will be problematic and closures may give the wrong impression that
// code here is always safe. It's easy to use freed memory from here. Whenever
// you use these callbacks think carefully! this mechanism should be rareley be
// used.
#define ON_DESTROY_CALLBACK(name) void name(void *allocated, void *clsr)
typedef ON_DESTROY_CALLBACK(mem_pool_on_destroy_callback_t);

// TODO: As of now I've required the allocated pointer and the clsr pointer, but
// I've never required them both at the same time. Maybe 2 pointers here is too
// much overhead for allocating callbacks?, only time will tell... If it turns
// out it's too much then we will need to think better about it. It's a tradeoff
// between flexibility of the API and space overhead in the pool. For now, we
// choose more flexibility at the cost of space overhead in the pool.
struct on_destroy_callback_info_t {
    mem_pool_on_destroy_callback_t *cb;
    void *allocated;
    void *clsr;

    struct on_destroy_callback_info_t *prev;
};

struct _bin_info_t {
    void *base;
    uint32_t size;
    struct _bin_info_t *prev_bin_info;

    struct on_destroy_callback_info_t *last_cb_info;
};

typedef struct _bin_info_t bin_info_t;

// TODO: I hardly ever use these, instead I use ZERO_INIT, remove them?
enum alloc_opts {
    POOL_UNINITIALIZED,
    POOL_ZERO_INIT
};

// TODO: We should make this API more "generic" in the sense that any of the
// push modes (size,struct,array) should be able to receive either the options
// enum or a callback, with or without closure. Maybe create macros for all
// these configuration combinations that evaluate to a 'config' struct, then
// make a *_full version of these that receives this struct as the last
// parameter.
//
// The reasoning behind this is that we don't want users to be bothered with
// filling all 3 arguments for the current _full version, when they may only
// want one of those. Right now I've seen myself prefering a mem_pool_push_* call
// followed by a ZERO_INIT just because using the options will force me to write
// explicitly the allocation using the 'push_size' variation, and remember de
// order of the last arguments in the _full version of it. Making all options a
// single argument at the end simplifies memorizing the API a LOT.
//
// This is something that would be much much nicer if C had default values for
// function arguments.
#define mem_pool_push_cb(pool,cb,clsr) mem_pool_push_size_full(pool,0,POOL_UNINITIALIZED,cb,clsr)

#define mem_pool_push_size_cb(pool,size,cb) mem_pool_push_size_full(pool,size,POOL_UNINITIALIZED,cb,NULL)
#define mem_pool_push_size(pool,size) mem_pool_push_size_full(pool,size,POOL_UNINITIALIZED,NULL,NULL)
#define mem_pool_push_struct(pool,type) ((type*)mem_pool_push_size(pool,sizeof(type)))
#define mem_pool_push_array(pool,n,type) mem_pool_push_size(pool,(n)*sizeof(type))
void* mem_pool_push_size_full (mem_pool_t *pool, uint32_t size, enum alloc_opts opts,
                               mem_pool_on_destroy_callback_t *cb, void *clsr)
{
    assert (pool != NULL);

    uint32_t required_size = cb == NULL ? size : size + sizeof(struct on_destroy_callback_info_t);

    if (required_size == 0) return NULL;

    // If not enough space left in the current bin, grow the pool by adding a
    // new one.
    if (pool->used + required_size > pool->size) {
        pool->num_bins++;

        if (pool->min_bin_size == 0) {
            pool->min_bin_size = MEM_POOL_DEFAULT_MIN_BIN_SIZE;
        }

        int new_bin_size = MAX(pool->min_bin_size, required_size);
        void *new_bin;
        bin_info_t *new_info;
        if ((new_bin = malloc (new_bin_size + sizeof(bin_info_t)))) {
            new_info = (bin_info_t*)((uint8_t*)new_bin + new_bin_size);
        } else {
            printf ("Malloc failed.\n");
            return NULL;
        }

        new_info->base = new_bin;
        new_info->size = new_bin_size;
        new_info->last_cb_info = NULL;

        if (pool->base == NULL) {
            new_info->prev_bin_info = NULL;
        } else {
            bin_info_t *prev_info = (bin_info_t*)((uint8_t*)pool->base + pool->size);
            new_info->prev_bin_info = prev_info;
        }

        pool->used = 0;
        pool->size = new_bin_size;
        pool->base = new_bin;
    }

    void *ret = (uint8_t*)pool->base + pool->used;
    if (cb != NULL) {
        struct on_destroy_callback_info_t *cb_info =
            (struct on_destroy_callback_info_t*)(pool->base + pool->used + size);
        cb_info->allocated = size > 0 ? ret : NULL;
        cb_info->clsr = clsr;
        cb_info->cb = cb;

        bin_info_t *bin_info = pool->base + pool->size;
        cb_info->prev = bin_info->last_cb_info;
        bin_info->last_cb_info = cb_info;
    }

    pool->used += required_size;
    pool->total_data += size;

    if (opts == POOL_ZERO_INIT) {
        memset (ret, 0, size);
    }
    return ret;
}

// NOTE: Do NOT use _pool_ again after calling this. We don't reset pool because
// it could have been bootstrapped into itself. Reusing is better hendled by
// mem_pool_end_temporary_memory().
void mem_pool_destroy (mem_pool_t *pool)
{
    if (pool->base != NULL) {
        bin_info_t *curr_info = (bin_info_t*)((uint8_t*)pool->base + pool->size);

        // Call all on_destroy callbacks
        while (curr_info != NULL) {
            struct on_destroy_callback_info_t *cb_info = curr_info->last_cb_info;
            while (cb_info != NULL) {
                cb_info->cb(cb_info->allocated, cb_info->clsr);
                cb_info = cb_info->prev;
            }

            curr_info = curr_info->prev_bin_info;
        }

        // Free all allocated bins
        curr_info = (bin_info_t*)((uint8_t*)pool->base + pool->size);
        void *to_free = curr_info->base;
        curr_info = curr_info->prev_bin_info;
        while (curr_info != NULL) {
            free (to_free);
            to_free = curr_info->base;
            curr_info = curr_info->prev_bin_info;
        }
        free (to_free);
    }
}

uint32_t mem_pool_allocated (mem_pool_t *pool)
{
    uint64_t allocated = 0;
    if (pool->base != NULL) {
        bin_info_t *curr_info = (bin_info_t*)((uint8_t*)pool->base + pool->size);
        while (curr_info != NULL) {
            allocated += curr_info->size + sizeof(bin_info_t);
            curr_info = curr_info->prev_bin_info;
        }
    }
    return allocated;
}

// Computes how much memory of the pool is used to store
// on_destroy_callback_info_t structutres.
uint32_t mem_pool_callback_info (mem_pool_t *pool)
{
    uint64_t callback_info_size = 0;
    if (pool->base != NULL) {
        bin_info_t *curr_info = (bin_info_t*)((uint8_t*)pool->base + pool->size);
        while (curr_info != NULL) {
            struct on_destroy_callback_info_t *callback_info =
                curr_info->last_cb_info;
            while (callback_info != NULL) {
                callback_info_size += sizeof(struct on_destroy_callback_info_t);
                callback_info = callback_info->prev;
            }
            curr_info = curr_info->prev_bin_info;
        }
    }
    return callback_info_size;
}

// NOTE: This isn't supposed to be called often, we traverse all bins at least
// twice. Once in the call to mem_pool_allocated() and again in the call to
// mem_pool_callback_info().
void mem_pool_print (mem_pool_t *pool)
{
    uint32_t allocated = mem_pool_allocated(pool);
    printf ("Allocated: %u bytes\n", allocated);

    uint32_t available = pool->size-pool->used;
    printf ("Available: %u bytes (%.2f%%)\n", available, ((double)available*100)/allocated);

    printf ("Data: %u bytes (%.2f%%)\n", pool->total_data, ((double)pool->total_data*100)/allocated);

    uint32_t callback_info_size = mem_pool_callback_info (pool);
    printf ("Callback Info: %u bytes (%.2f%%)\n", callback_info_size, ((double)callback_info_size*100)/allocated);

    uint64_t info_size = pool->num_bins*sizeof(bin_info_t);
    printf ("Info: %lu bytes (%.2f%%)\n", info_size, ((double)info_size*100)/allocated);

    // NOTE: This is the amount of space left empty in previous bins. It's
    // different from 'available' space in that 'left_empty' space is
    // effectively wasted and won't be used in future allocations.
    uint64_t left_empty;
    if (pool->num_bins>0)
        left_empty = (allocated - pool->size - sizeof(bin_info_t))          /*allocated except last bin*/
                     - (pool->total_data + callback_info_size - pool->used) /*total_data except last bin*/
                     - (pool->num_bins-1)*sizeof(bin_info_t);               /*size used in bin_info_t*/
    else {
        left_empty = 0;
    }
    printf ("Left empty: %lu bytes (%.2f%%)\n", left_empty, ((double)left_empty*100)/allocated);
    printf ("Bins: %u\n", pool->num_bins);
}

typedef struct {
    mem_pool_t *pool;
    void* base;
    uint32_t used;
    uint32_t total_data;
} mem_pool_marker_t;

mem_pool_marker_t mem_pool_begin_temporary_memory (mem_pool_t *pool)
{
    mem_pool_marker_t res;
    res.total_data = pool->total_data;
    res.used = pool->used;
    res.base = pool->base;
    res.pool = pool;
    return res;
}

void mem_pool_end_temporary_memory (mem_pool_marker_t mrkr)
{
    if (mrkr.base != NULL) {
        // Call all on_destroy callbacks for bins that will be freed, starting
        // from the last bin.
        bin_info_t *curr_info = (bin_info_t*)((uint8_t*)mrkr.pool->base + mrkr.pool->size);
        while (curr_info->base != mrkr.base) {
            struct on_destroy_callback_info_t *cb_info = curr_info->last_cb_info;
            while (cb_info != NULL) {
                cb_info->cb(cb_info->allocated, cb_info->clsr);
                cb_info = cb_info->prev;
            }

            curr_info = curr_info->prev_bin_info;
        }

        // Call affected on_destroy callbacks in the new last bin (the one that
        // will stay allocated but may be partially freed).
        struct on_destroy_callback_info_t *cb_info = curr_info->last_cb_info;
        while (cb_info != NULL && (void*)cb_info >= mrkr.base + mrkr.used) {
            cb_info->cb(cb_info->allocated, cb_info->clsr);
            cb_info = cb_info->prev;
        }

        // Update last_cb_info in the bin_info of the last bin. This bin will
        // not be freed, future allocations will overwrite the end of it if
        // there's enough space.
        curr_info->last_cb_info = cb_info;

        // Free necessary bins
        curr_info = (bin_info_t*)((uint8_t*)mrkr.pool->base + mrkr.pool->size);
        while (curr_info->base != mrkr.base) {
            void *to_free = curr_info->base;
            curr_info = curr_info->prev_bin_info;
            free (to_free);
            mrkr.pool->num_bins--;
        }
        mrkr.pool->size = curr_info->size;
        mrkr.pool->base = mrkr.base;
        mrkr.pool->used = mrkr.used;
        mrkr.pool->total_data = mrkr.total_data;

    } else {
        // NOTE: Here mrkr was created before the pool was initialized, so we
        // destroy everything.
        mem_pool_destroy (mrkr.pool);

        // This assumes pool wasn't bootstrapped after taking mrkr
        mrkr.pool->size = 0;
        mrkr.pool->base = NULL;
        mrkr.pool->used = 0;
        mrkr.pool->total_data = 0;
    }
}

// The idea of this is to allow chaining multiple pools so we only need to
// delete the parent one.
ON_DESTROY_CALLBACK(pool_chain_destroy)
{
    mem_pool_destroy(clsr);
}

#define mem_pool_add_child(pool,child_pool) mem_pool_push_cb(pool, pool_chain_destroy, child_pool)

// pom == pool or malloc
#define pom_push_struct(pool, type) pom_push_size(pool, sizeof(type))
#define pom_push_array(pool, n, type) pom_push_size(pool, (n)*sizeof(type))
#define pom_push_size(pool, size) (pool==NULL? malloc(size) : mem_pool_push_size(pool,size))

#define pom_strdup(pool,str) pom_strndup(pool,str,((str)!=NULL?strlen(str):0))
static inline
char* pom_strndup (mem_pool_t *pool, const char *str, uint32_t str_len)
{
    char *res = (char*)pom_push_size (pool, str_len+1);
    memcpy (res, str, str_len);
    res[str_len] = '\0';
    return res;
}

static inline
void* pom_dup (mem_pool_t *pool, void *data, uint32_t size)
{
    void *res = pom_push_size (pool, size);
    memcpy (res, data, size);
    return res;
}

GCC_PRINTF_FORMAT(2, 3)
char* pprintf (mem_pool_t *pool, const char *format, ...)
{
    va_list args1, args2;
    va_start (args1, format);
    va_copy (args2, args1);

    size_t size = vsnprintf (NULL, 0, format, args1) + 1;
    va_end (args1);

    char *str = pom_push_size (pool, size);

    vsnprintf (str, size, format, args2);
    va_end (args2);

    return str;
}

// These functions implement pooled string_t structures. This means strings
// created using strn_new_pooled() will be automatically freed when the passed
// pool gets destroyed.
ON_DESTROY_CALLBACK (destroy_pooled_str)
{
    string_t *str;
    if (clsr != NULL) {
        assert (allocated == NULL);
        str = (string_t*)clsr;
    } else {
        str = (string_t*)allocated;
    }

    //printf ("Deleted pooled string: '%s'\n", str_data(str));
    str_free (str);
}
#define str_new_pooled(pool,c_str) strn_new_pooled((pool),(c_str),((c_str)!=NULL?strlen(c_str):0))
string_t* strn_new_pooled (mem_pool_t *pool, const char *c_str, size_t len)
{
    assert (pool != NULL && c_str != NULL);
    string_t *str = mem_pool_push_size_cb (pool, sizeof(string_t), destroy_pooled_str);
    *str = strn_new (c_str, len);
    return str;
}

#define str_set_pooled(pool,str,c_str) strn_set_pooled((pool),(str),(c_str),((c_str)!=NULL?strlen(c_str):0))
void strn_set_pooled (mem_pool_t *pool, string_t *str, const char *c_str, size_t len)
{
    assert (pool != NULL && str != NULL);
    str_set (str, "");
    mem_pool_push_cb (pool, destroy_pooled_str, str);
    strn_set (str, c_str, len);
}

#define str_pool(pool,str) mem_pool_push_cb(pool,destroy_pooled_str,str)

// Flatten an array of null terminated strings into a single string allocated
// into _pool_ or heap.
char* collapse_str_arr (char **arr, int n, mem_pool_t *pool)
{
    int len = 0;
    int i;
    for (i=0; i<n; i++) {
        len += strlen (arr[i]) + 1;
    }
    char *res = (char*)pom_push_size (pool, len);

    char *ptr = res;
    for (i=0; i<n; i++) {
        ptr = stpcpy (ptr, arr[i]);
        *ptr = ' ';
        ptr++;
    }
    ptr--;
    *ptr = '\0';
    return res;
}

// Flattens an array of num_arrs arrays (arrs) into a single array e. Elements
// in arrs are of size e_size and the lengths of each array is specified in the
// array arrs_lens.
void flatten_array (mem_pool_t *pool, uint32_t num_arrs, size_t e_size,
                    void **arrs, uint32_t *arrs_lens,
                    void **e, uint32_t *num_e)
{
    *num_e = 0;
    int i;
    for (i=0; i<num_arrs; i++) {
        *num_e += arrs_lens [i];
    }

    void *res = pom_push_size (pool, *num_e * e_size);
    uint8_t *ptr = res;

    for (i=0; i<num_arrs; i++) {
        memcpy (ptr, arrs[i], arrs_lens[i]*e_size);
        ptr += arrs_lens[i]*e_size;
    }

    *e = res;
}

// Expand _str_ as bash would, allocate it in _pool_ or heap. 
// NOTE: $(<cmd>) and `<cmd>` work but don't get too crazy, this spawns /bin/sh
// and a subprocess. Using env vars like $HOME, or ~/ doesn't.
// CAUTION: This is dangerous if called on untrusted user input. It will run
// arbitrary shell commands.
// CAUTION: It is a VERY bad idea to hide calls to this from users of an API.
// Mainly for 2 reasons:
//  - It's unsafe by default.
//  - It's common to have paths with () characters in them (i.e. duplicate
//    downloads).
// For some time all file manipulation functions started with a call to
// sh_expand(). Don't do that again!!. In fact, try to use sh_expand() as little
// as possible. When a user may input a path with ~, better resolve it using
// resolve_user_path().
char* sh_expand (char *str, mem_pool_t *pool)
{
    wordexp_t out;
    wordexp (str, &out, 0);
    char *res = collapse_str_arr (out.we_wordv, out.we_wordc, pool);
    wordfree (&out);
    return res;
}

static inline
char sys_path_sep ()
{
    // TODO: Actually return the system's path separator.
    return '/';
}

char *resolve_user_path (char *path, mem_pool_t *pool)
{
    assert (path != NULL);

    char *result;
    if (path[0] == '~' && (strlen(path) == 1 || path[1] == sys_path_sep())) {
        char *home_path = getenv ("HOME");
        result = pom_push_size (pool, strlen (home_path) + strlen (path));
        strcpy (result, home_path);
        strcat (result, path + 1);

    } else {
        result = pom_strdup (pool, path);
    }

    return result;
}

// It's common to want absolute paths before we start a series of path
// manipulation calls, so this is likely to be the first function to be called
// on a string that represents a path. Because real path needs to traverse
// folders to resolve "..", this fails if the path does not exist. Then... what
// is the point of having separate *_exists() functions?. I think the useful
// thing about those is for times when we already know a path is absolute and we
// need to make multiple tests. But for a single sequence of abs_path() followed
// by path_exists(), we can use abs_path()'s result.
//
// TODO: This function should probably also set a boolean value that tells the
// caller if there was a file not found error.
char* abs_path (char *path, mem_pool_t *pool)
{
    mem_pool_t l_pool = {0};

    char *user_path = resolve_user_path (path, &l_pool);

    char *absolute_path_m = realpath (user_path, NULL);
    if (absolute_path_m == NULL) {
        // NOTE: realpath() fails if the file does not exist.
        //printf ("Error: %s (%d)\n", strerror(errno), errno);
    }
    char *absolute_path = pom_strdup (pool, absolute_path_m);
    free (absolute_path_m);

    mem_pool_destroy (&l_pool);

    return absolute_path;
}

void file_write (int file, void *pos,  ssize_t size)
{
    if (write (file, pos, size) < size) {
        printf ("Write interrupted\n");
    }
}

void file_read (int file, void *pos,  ssize_t size)
{
    int bytes_read = read (file, pos, size);
    if (bytes_read < size) {
        printf ("Did not read full file\n"
                "asked for: %ld\n"
                "received: %d\n", size, bytes_read);
    }
}

// NOTE: If path does not exist, it will be created. If it does, it will be
// overwritten.
bool full_file_write (const void *data, ssize_t size, const char *path)
{
    bool failed = false;

    // TODO: If writing fails, we will leave a blank file behind. We should make
    // a backup in case things go wrong.
    int file = open (path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file != -1) {
        int bytes_written = 0;
        do {
            int status = write (file, data, size);
            if (status == -1) {
                printf ("Error writing %s: %s\n", path, strerror(errno));
                failed = true;
                break;
            }
            bytes_written += status;
        } while (bytes_written != size);
        close (file);

    } else {
        failed = true;
        if (errno != EACCES) {
            // If we don't have permissions fail silently so that the caller can
            // detect this with errno and maybe retry.
            printf ("Error opening %s: %s\n", path, strerror(errno));
        }
    }

    return failed;
}

char* full_file_read (mem_pool_t *pool, const char *path, uint64_t *len)
{
    bool success = true;
    char *expanded_path = NULL;

    mem_pool_marker_t mrk;
    if (pool != NULL) {
        mrk = mem_pool_begin_temporary_memory (pool);
    }

    char *loaded_data = NULL;
    struct stat st;
    if (stat(path, &st) == 0) {
        loaded_data = (char*)pom_push_size (pool, st.st_size + 1);

        int file = open (path, O_RDONLY);
        if (file != -1) {
            int bytes_read = 0;
            do {
                int status = read (file, loaded_data+bytes_read, st.st_size-bytes_read);
                if (status == -1) {
                    success = false;
                    printf ("Error reading %s: %s\n", path, strerror(errno));
                    break;
                }
                bytes_read += status;
            } while (bytes_read != st.st_size);
            loaded_data[st.st_size] = '\0';

            if (len != NULL) {
                *len = st.st_size;
            }

            close (file);
        } else {
            success = false;
            printf ("Error opening %s: %s\n", path, strerror(errno));
        }

    } else {
        success = false;
        printf ("Could not read %s: %s\n", path, strerror(errno));
    }

    char *retval = NULL;
    if (success) {
        retval = loaded_data;
    } else if (loaded_data != NULL) {
        if (pool != NULL) {
            mem_pool_end_temporary_memory (mrk);
        } else {
            free (loaded_data);
        }
    }

    if (expanded_path != NULL) {
        free (expanded_path);
    }
    return retval;
}

bool path_exists (char *path)
{
    if (path == NULL) return false;

    bool retval = true;

    struct stat st;
    int status;
    if ((status = stat(path, &st)) == -1) {
        retval = false;
        if (errno != ENOENT) {
            printf ("Error checking existance of %s: %s\n", path, strerror(errno));
        }
    }
    return retval;
}

bool dir_exists (char *path)
{
    if (path == NULL) return false;

    bool retval = true;

    struct stat st;
    int status;
    if ((status = stat(path, &st)) == -1) {
        retval = false;
        if (errno != ENOENT) {
            printf ("Error checking existance of %s: %s\n", path, strerror(errno));
        }

    } else {
        if (!S_ISDIR(st.st_mode)) {
            return false;
        }
    }

    return retval;
}

// NOTE: Returns false if there was an error creating the directory.
bool ensure_dir_exists (char *path)
{
    bool retval = true;

    struct stat st;
    int success = 0;
    if (stat(path, &st) == -1 && errno == ENOENT) {
        success = mkdir (path, 0777);
    }

    if (success == -1) {
        char *expl = strerror (errno);
        printf ("Could not create %s: %s\n", path, expl);
        free (expl);
        retval = false;
    }

    return retval;
}

// Checks if path exists (either as a file or directory). If it doesn't it tries
// to create all directories required for it to exist. If path ends in / then
// all components are checked, otherwise the last part after / is assumed to be
// a filename and is not created as a directory.
bool ensure_path_exists (char *path)
{
    bool success = true;

    char *c = path;
    if (*c == '/') {
        c++;
    }

    struct stat st;
    if (stat(path, &st) == -1) {
        if (errno == ENOENT) {
            while (*c && success) {
                while (*c && *c != '/') {
                    c++;
                }

                if (*c != '\0') {
                    *c = '\0';
                    if (stat(path, &st) == -1 && errno == ENOENT) {
                        if (mkdir (path, 0777) == -1) {
                            success = false;
                            printf ("Error creating %s: %s\n", path, strerror (errno));
                        }
                    }

                    *c = '/';
                    c++;
                }
            }
        } else {
            success = false;
            printf ("Error ensuring path for %s: %s\n", path, strerror(errno));
        }
    } else {
        // Path exists. Maybe check if it's the same type as on path, either
        // file or directory?.
    }

    return success;
}

bool read_dir (DIR *dirp, struct dirent **res)
{
    errno = 0;
    *res = readdir (dirp);
    if (*res == NULL) {
        if (errno != 0) {
            printf ("Error while reading directory: %s", strerror (errno));
        }
        return false;
    }
    return true;
}

////////////////////////////
// Recursive folder iterator
//
// Usage:
//  iterate_dir (path, callback_name, data);
//
// The function _callback_name_ will be called for each file under _path_. The
// function iterate_dir_printf() is an example callback. To define a new
// callback called my_cb use:
//
// ITERATE_DIR_CB (my_cb)
// {
//     // The pointer _data_ from iterate_dir will be passed here as data.
//     ....
// }
//
// TODO: Make a non-recursive version of this and maybe don't even use a
// callback but use a macro that hides a while or for loop behind. Making code
// much more readable, and not requiring closures.
#define ITERATE_DIR_CB(name) void name(char *fname, bool is_dir, void *data)
typedef ITERATE_DIR_CB(iterate_dir_cb_t);

ITERATE_DIR_CB (iterate_dir_printf)
{
    printf ("%s\n", fname);
}

void iterate_dir_helper (string_t *path,  iterate_dir_cb_t *callback, void *data)
{
    int path_len = str_len (path);

    struct stat st;
    callback (str_data(path), true, data);
    DIR *d = opendir (str_data(path));
    struct dirent *entry_info;
    while (read_dir (d, &entry_info)) {
        if (entry_info->d_name[0] != '.') { // file is not hidden
            str_put_c (path, path_len, entry_info->d_name);
            if (stat(str_data(path), &st) == 0) {
                if (S_ISREG(st.st_mode)) {
                    callback (str_data(path), false, data);

                } else if (S_ISDIR(st.st_mode)) {
                    str_cat_c (path, "/");
                    iterate_dir_helper (path, callback, data);
                }
            }
        }
    }
    closedir (d);
}

void iterate_dir (char *path, iterate_dir_cb_t *callback, void *data)
{
    string_t path_str = str_new (path);
    if (str_last (&path_str) != '/') {
        str_cat_c (&path_str, "/");
    }

    iterate_dir_helper (&path_str, callback, data);

    str_free (&path_str);
}

//////////////////////////////
//
// PATH/FILENAME MANIPULATIONS
//
char* change_extension (mem_pool_t *pool, char *path, char *new_ext)
{
    size_t path_len = strlen(path);
    int i=path_len;
    while (i>0 && path[i-1] != '.') {
        i--;
    }

    char *res = (char*)pom_push_size (pool, path_len+strlen(new_ext)+1);
    strcpy (res, path);
    strcpy (&res[i], new_ext);
    return res;
}

// NOTE: This correctly handles a filename like hi.autosave.repl where there are
// multiple parts that would look like an extensions. Only the last one will be
// removed.
char* remove_extension (mem_pool_t *pool, char *path)
{
    size_t end_pos = strlen(path)-1;
    while (end_pos>0 && path[end_pos] != '.') {
        end_pos--;
    }

    if (end_pos == 0) {
        // NOTE: path had no extension.
        return NULL;
    }

    return pom_strndup (pool, path, end_pos);
}

char* remove_multiple_extensions (mem_pool_t *pool, char *path, int num)
{
    char *retval;
    if (num > 1) {
        char *next_path = remove_extension (NULL, path);
        retval = remove_multiple_extensions (pool, next_path, num-1);
        free (next_path);

    } else {
        retval = remove_extension (pool, path);
    }

    return retval;
}

char* add_extension (mem_pool_t *pool, char *path, char *new_ext)
{
    size_t path_len = strlen(path);
    char *res = (char*)pom_push_size (pool, path_len+strlen(new_ext)+2);
    strcpy (res, path);
    res[path_len++] = '.';
    strcpy (&res[path_len], new_ext);
    return res;
}

// NOTE: Returns a pointer INTO _path_ after the last '.' or NULL if it does not
// have an extension. Hidden files are NOT extensions (/home/.bashrc returns NULL).
char* get_extension (char *path)
{
    size_t path_len = strlen(path);
    int i=path_len-1;
    while (i>=0 && path[i] != '.' && path[i] != '/') {
        i--;
    }

    if (i == -1 || path[i] == '/' || (path[i] == '.' && (i==0 || path[i-1] == '/'))) {
        return NULL;
    }

    return &path[i+1];
}

void path_split (mem_pool_t *pool, char *path, char **dirname, char **basename)
{
    if (path == NULL) {
        return;
    }

    size_t end = strlen (path);
    while (path[end] != '/') {
        end--;
    }

    if (dirname != NULL) {
        *dirname = pom_strndup (pool, path, end);
    }

    if (basename != NULL) {
        *basename = pom_strdup (pool, &path[end+1]);
    }
}

#ifdef __CURL_CURL_H
// Wrapper around curl to download a full file while blocking. There are two
// macros that offer different ways of calling it, curl_download_file() receives
// a URL and the name of a destination file. curl_download_file_progress() also
// receives a callback function that can be used to monitor the progress of the
// download.
//
// NOTE: I switched from cURL because my usecases were much more simple, the
// replacement I'm using is below. Using cURL implied some issues with
// portability. For more details read the following rant.
//
// RANT: CURL's packaging at least on Debian seems to be poorly done. These
// issues happened when going from elementaryOS Loki to elementaryOS Juno, which
// basically means going from Ubuntu Xenial to Ubuntu Bionic. There is a nasty
// situation going on where an ABI breaking change in OpenSSL made it impossible
// to have a binary that runs on old and new systems. Current systems now have
// two packages libcurl3 and libcurl4 that conflict with each other so
// installing one will remove the other. Meanwhile, application's executables
// built in old systems require libcurl3 but the ones built in newer systems
// require libcurl4. This happened because more than 10 years ago libcurl3 was
// packaged to provide libcurl.so.4 and libcurl.so.3 was a symlink pointing to
// libcurl.so.4 (because the package matainer didn't consider upstream's soname bump to be
// an actual ABI break). On top of this, Debian versioned all symbols defined by
// libcurl.so.4 as CURL_OPENSSL_3. Doing this caused applications built in this
// system to link dynamically with libcurl.so.4, but require symbols with
// version CURL_OPENSSL_3. This now causes problems because OpenSSL's change
// caused another soname bump in libcurl, creating two different packages,
// libcurl3 providing libcurl.so.4 with symbols versioned as CURL_OPENSSL_3, and
// libcurl4 providing libcurl.so.4 too, but with symbols versioned as
// CURL_OPENSSL_4 (providing the same file causes the conflict). This means that
// binaries built in new systems link dynamically to libcurl.so.4 but expect
// symbols with version CURL_OPENSSL_4, while binaries from previous systems
// link to the same soname, but expect symbols versioned as CURL_OPENSSL_3. In
// the end, this makes it hard to create an executable that works on old and new
// systems. The sad thing is upstream curl does NOT use symbol versioning, this
// is added by Debian's mantainers. The options are:
//
//      * Build different packages for each system (what I think Debian does).
//      * Statically link curl (which .deb lintian forbids) now having to
//        buid-depend on all curl's build dependencies.
//      * Link against an upstream version of libcurl without symbol versioning.
//      * Don't use curl at all.
//
//  - Santiago, March 2018

#define DOWNLOAD_PROGRESS_CALLBACK(name) \
    int name(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
typedef DOWNLOAD_PROGRESS_CALLBACK(download_progress_callback_t);

// NOTE: This function is blocking.
#define download_file(url,dest) download_file_full(url,dest,NULL,NULL,true)
#define download_file_cbk(url,dest,cbk,data) download_file_full(url,dest,cbk,data,false)
bool download_file_full (char *url, char *dest,
                         download_progress_callback_t callback, void *clientp,
                         bool progress_meter)
{
    CURL *h = curl_easy_init ();
    if (!h) {
        printf ("Error downloading: could not create curl handle.\n");
        return false;
    }

    char *temp_dest = malloc (strlen(dest) + 5 + 1);
    sprintf (temp_dest, "%s%s", dest, ".part");

    //curl_easy_setopt (h, CURLOPT_VERBOSE, 1);
    curl_easy_setopt (h, CURLOPT_WRITEFUNCTION, NULL); // Required on Windows

    if (callback != NULL) {
        curl_easy_setopt (h, CURLOPT_NOPROGRESS, 0);
        curl_easy_setopt (h, CURLOPT_XFERINFOFUNCTION, callback);
        curl_easy_setopt (h, CURLOPT_XFERINFODATA, clientp);
    } else {
        if (progress_meter) {
            printf ("Downloading: %s\n", url);
            curl_easy_setopt (h, CURLOPT_NOPROGRESS, 0);
        }
    }

    FILE *f = fopen (temp_dest, "w");
    curl_easy_setopt (h, CURLOPT_WRITEDATA, f);
    curl_easy_setopt (h, CURLOPT_URL, url);
    CURLcode rc = curl_easy_perform (h);
    fclose (f);
    curl_easy_cleanup (h);

    bool retval = true;
    if (rc != CURLE_OK) {
        printf ("Error downloading: %s\n", curl_easy_strerror (rc));
        unlink (temp_dest);
        retval = false;
    } else {
        rename (temp_dest, dest);
    }

    free (temp_dest);
    return retval;
}
#endif /*__CURL_CURL_H*/

#ifdef http_hpp
// Wrapper around Mattias Gustavsson's single header library [1]. Here is the common
// usecase of downloading some URL into a file, through HTTP. Note that this
// does not work with HTTPS but that's part of what makes it so simple.
//
// [1] https://github.com/mattiasgustavsson/libs/blob/master/http.h
bool download_file (const char *url, char *path)
{
    bool success = true;
    http_t* request = http_get (url, NULL);
    if (request)
    {
        http_status_t status = HTTP_STATUS_PENDING;
        while (status == HTTP_STATUS_PENDING)
        {
            status = http_process (request);
        }

        if( status != HTTP_STATUS_FAILED )
        {
            full_file_write (request->response_data, request->response_size, path);
        } else {
            printf( "HTTP request failed (%d): %s.\n", request->status_code, request->reason_phrase );
            success = false;
        }

        http_release( request );
    } else {
        success = false;
    }
    return success;
}
#endif

///////////////
//
//  THREADING

//  Handmade busywait mutex for GCC
void start_mutex (volatile int *lock) {
    while (__sync_val_compare_and_swap (lock, 0, 1) == 1) {
        // Busy wait
    }
}

void end_mutex (volatile int *lock) {
    *lock = 0;
}

///////////////////////
//
//   SHARED VARIABLE
//
//   These macros ease the creation of variables on shared memory. Useful when
//   trying to pass data to child processes.

#define NEW_SHARED_VARIABLE_NAMED(TYPE,SYMBOL,VALUE,NAME)                                         \
TYPE *(SYMBOL);                                                                                   \
{                                                                                                 \
    int shared_fd = shm_open (NAME, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);                \
    if (shared_fd == -1 && errno == EEXIST) {                                                     \
        printf ("Shared variable name %s exists, missing call to UNLINK_SHARED_VARIABLE*.\n",     \
                NAME);                                                                            \
        UNLINK_SHARED_VARIABLE_NAMED (NAME)                                                       \
                                                                                                  \
        shared_fd = shm_open (NAME,                                                               \
                              O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);                      \
    }                                                                                             \
    assert (shared_fd != -1 && "Error on shm_open() while creating shared variable.");            \
                                                                                                  \
    int set_size_status = ftruncate (shared_fd, sizeof (TYPE));                                   \
    assert (set_size_status == 0 && "Error on ftruncate() while creating shared variable.");      \
                                                                                                  \
    (SYMBOL) = (TYPE*) mmap (NULL, sizeof(TYPE), PROT_READ | PROT_WRITE, MAP_SHARED, shared_fd, 0); \
                                                                                                  \
    *(SYMBOL) = (VALUE);                                                                            \
}

#define UNLINK_SHARED_VARIABLE_NAMED(NAME)                                \
if (shm_unlink (NAME) == -1) {                                            \
    printf ("Error unlinking shared variable '%s': %s\n", NAME, strerror(errno));      \
}

///////////////////
//
//  LINKED LIST
//
//  These macros are a low level implementation of common liked list operations.
//  I've seen these be more useful than an actual liked list struct with a node
//  type and head. These make it easy to convert any struct into a linked list
//  and operate on it as such.
//
//  How to use:
//
//  struct my_struct_t {
//      int a;
//      float f;
//
//      struct my_struct_t *next;
//  };
//
//  {
//      mem_pool_t pool = {0};
//      struct my_struct_t *list = NULL;
//
//      LINKED_LIST_PUSH_NEW (&pool, struct my_struct_t, list, new_my_struct);
//      new_my_struct->a = 10;
//      new_my_struct->f = 5;
//
//      struct my_struct_t *head = LINKED_LIST_POP (new_my_struct);
//
//      ...
//
//      mem_pool_destroy (&pool);
//  }
//
//  Keep in mind:
//
//    - LINKED_LIST_APPEND() requires the existance of a variable with the same
//      name as the head but suffixed by _end. It's unlikely that the user wants
//      O(n) appending, so we make the user do this so they get a O(1)
//      implementation.
//
//  TODO: Allow users to pass the 'next' symbol name and the '_end' suffix.

// Maybe we want people to do this manually? so that they know exactly what it
// means storage wise to have a linked list?. It's also possible that you want
// to save up on the number of pointers and the end pointer will be computed
// before appending, or not used if they are only calling push.
#define LINKED_LIST_DECLARE(type,head_name) \
    type *head_name;                        \
    type *head_name ## _end;

#define LINKED_LIST_APPEND(head_name,node)                   \
{                                                            \
    if (head_name ## _end == NULL) {                         \
        head_name = node;                                    \
    } else {                                                 \
        head_name ## _end->next = node;                      \
    }                                                        \
    head_name ## _end = node;                                \
}

#define LINKED_LIST_PUSH(head_name,node)                     \
{                                                            \
    node->next = head_name;                                  \
    head_name = node;                                        \
}

// This is O(n) and requres the _end pointer.
//
// Discussion:
//  1) The only way to make this operation O(1) is to ask the user to pass the
//     parent pointer. The user will either have to do a O(n) operation before
//     to compute it, or complicate their code to keep track of the parent
//     pointer. A better solution would be to use a doubly linked list that
//     stores prev pointers in the nodes, instead of a linked list.
//
//  2) The _end pointer requirement should be optional, but there is no way we
//     can know if the user defined it or not until build time. For now, I
//     haven't needed this on lists without _end pointer. When I do, I will need
//     to create an alternate macro for it.
//
// TODO: Take a look at the trick in
// https://gist.github.com/santileortiz/9a5c60a545d01a70dc1d9a631b9fca40 see if
// it's useful to make the _end pointer optional.
#define LINKED_LIST_REMOVE(type,head,node)         \
{                                                  \
    /* Find the pointer to node */                 \
    type **curr_node_ptr = &head;                  \
    type *curr_node = head;                        \
    type *prev_node = NULL;                        \
    while (curr_node != NULL) {                    \
        if (*curr_node_ptr == node) break;         \
                                                   \
        curr_node_ptr = &((*curr_node_ptr)->next); \
        prev_node = curr_node;                     \
        curr_node = curr_node->next;               \
    }                                              \
                                                   \
    if (curr_node == node) {                       \
        /* Update the _end pointer if necessary */ \
        if ((curr_node)->next == NULL) {           \
            head ## _end = prev_node;              \
        }                                          \
                                                   \
        /* Remove the node */                      \
        *curr_node_ptr = (curr_node)->next;        \
        (curr_node)->next = NULL;                  \
    }                                              \
}

// NOTE: This doesn't update the _end pointer if its being used. Use
// LINKED_LIST_POP_END() in that case.
#define LINKED_LIST_POP(head)                                \
head;                                                        \
{                                                            \
    void *tmp = head->next;                                  \
    head->next = NULL;                                       \
    head = tmp;                                              \
}

// This requires _end pointer.
// TODO: I'm not sure this is the right way of making the _end pointer
// requirement optional. It's very easy to forget to use the right POP macro
// version and mess up everything. I wish there was a way to conditionally add
// or remove code if a symbols exists or not in the context.
#define LINKED_LIST_POP_END(head)                            \
head;                                                        \
{                                                            \
    if (head == head ## _end) {                              \
        head ## _end = NULL;                                 \
    }                                                        \
                                                             \
    void *tmp = head->next;                                  \
    head->next = NULL;                                       \
    head = tmp;                                              \
}

#define LINKED_LIST_REVERSE(type,head)                       \
{                                                            \
    type *curr_node = head;                                  \
    type *prev_node = NULL;                                  \
    while (curr_node != NULL) {                              \
        type *next_node = curr_node->next;                   \
        curr_node->next = prev_node;                         \
                                                             \
        prev_node = curr_node;                               \
        curr_node = next_node;                               \
    }                                                        \
    head = prev_node;                                        \
}

// These require passing the type of the node struct and a pool.

#define LINKED_LIST_APPEND_NEW(pool,type,head_name,new_node) \
type *new_node;                                              \
{                                                            \
    new_node = mem_pool_push_struct(pool,type);              \
    *new_node = ZERO_INIT(type);                             \
                                                             \
    LINKED_LIST_APPEND(head_name,new_node)                   \
}

#define LINKED_LIST_PUSH_NEW(pool,type,head_name,new_node)   \
type *new_node;                                              \
{                                                            \
    new_node = mem_pool_push_struct(pool,type);              \
    *new_node = ZERO_INIT(type);                             \
                                                             \
    LINKED_LIST_PUSH(head_name,new_node)                     \
}

// This macro requires the passed type parameter to be a pointer. I had another
// version of this macro that turned a normal type to a pointer type internally
// (appended *). I found I always passed a pointer type so I was getting a
// double pointer in the end. The best mnemonic I've found is to think of this
// macro like a foreach loop in other languages like java where you declare the
// full type of the variable that will be iterated on. This old macro has the
// same issue as *_ptr typedefs some people use.
//
// TODO: Can we check at compile time that type is a pointer? Right now if users
// forget the *, they get a long list of errors. Maybe a static assert would
// show a nicer error message?.
#define LINKED_LIST_FOR(type, varname,headname)              \
type varname = headname;                                     \
for (; varname != NULL; varname = varname->next)

/*
//@DEPRECATED
#define LINKED_LIST_FOR(type, varname,headname)              \
type *varname = headname;                                    \
for (; varname != NULL; varname = varname->next)
*/

// Linked list sorting
//
// IS_A_LT_B is an expression where a and b are pointers of type TYPE and should
// evaluate to true when a->val < b->val. NEXT_FIELD specifies the name of the
// field where the next node pointer is found. The macro templ_sort_ll is
// convenience for when this field's name is "next". Use n=-1 if the size is
// unknown, in this case the full linked list will be iterated to compute it.
// NOTE: IS_A_LT_B as defined, will sort the linked list in ascending order.
// NOTE: The last node of the linked list is expected to have NEXT_FIELD field
// set to NULL.
// NOTE: It uses a pointer array of size n, and calls merge sort on that array.

// We say a and b are pointers, for arrays it's well defined. When talking about
// linked lists we could mean a pointer to a node, or a pointer to an element of
// the pointer array generated internally (a double pointer to a node). I've
// seen the first one to be the expected way, but passing IS_A_LT_B as is, will
// have the second semantics. This macro injects some code that dereferences a
// and b so that we can do a->val < b->val easily.
#define _linked_list_A_B_dereference_injector(IS_A_LT_B,TYPE) \
    0;                                                        \
    TYPE* _a = *a;                                            \
    TYPE* _b = *b;                                            \
    {                                                         \
        TYPE *a = _a;                                         \
        TYPE *b = _b;                                         \
        c = IS_A_LT_B;                                        \
    }

// NOTE: The generated sorting function returns the last node of the linked list
// so the user can update it if needed.
#define _linked_list_sort_implementation(FUNCNAME,TYPE,NEXT_FIELD)  \
TYPE* FUNCNAME ## _user_data (TYPE **head, int n, void *user_data)  \
{                                                                   \
    if (head == NULL || n == 0) {                                   \
        return NULL;                                                \
    }                                                               \
                                                                    \
    if (n == -1) {                                                  \
        n = 0;                                                      \
        TYPE *node = *head;                                         \
        while (node != NULL) {                                      \
            n++;                                                    \
            node = node->NEXT_FIELD;                                \
        }                                                           \
    }                                                               \
                                                                    \
    TYPE *node = *head;                                             \
    TYPE *arr[n];                                                   \
                                                                    \
    int j = 0;                                                      \
    while (node != NULL) {                                          \
        arr[j] = node;                                              \
        j++;                                                        \
        node = node->NEXT_FIELD;                                    \
    }                                                               \
                                                                    \
    FUNCNAME ## _arr_user_data (arr, n, user_data);                 \
                                                                    \
    *head = arr[0];                                                 \
    for (j=0; j<n - 1; j++) {                                       \
        arr[j]->NEXT_FIELD = arr[j+1];                              \
    }                                                               \
    arr[j]->NEXT_FIELD = NULL;                                      \
                                                                    \
    return arr[n-1];                                                \
}                                                                   \
                                                                    \
TYPE* FUNCNAME(TYPE **head, int n) {                                \
    return FUNCNAME ## _user_data (head,n,NULL);                    \
}

// Linked list sorting
#define templ_sort_ll_next_field(FUNCNAME,TYPE,NEXT_FIELD,IS_A_LT_B)\
templ_sort(FUNCNAME ## _arr, TYPE*,                                 \
           _linked_list_A_B_dereference_injector(IS_A_LT_B,TYPE))   \
_linked_list_sort_implementation(FUNCNAME,TYPE,NEXT_FIELD)

#define templ_sort_ll(FUNCNAME,TYPE,IS_A_LT_B) \
    templ_sort_ll_next_field(FUNCNAME,TYPE,next,IS_A_LT_B)

// Stable linked list sorting
#define templ_sort_stable_ll_next_field(FUNCNAME,TYPE,NEXT_FIELD,CMP_A_TO_B)\
templ_sort_stable(FUNCNAME ## _arr, TYPE*,                                  \
           _linked_list_A_B_dereference_injector(CMP_A_TO_B,TYPE))          \
_linked_list_sort_implementation(FUNCNAME,TYPE,NEXT_FIELD)

#define templ_sort_stable_ll(FUNCNAME,TYPE,CMP_A_TO_B) \
    templ_sort_stable_ll_next_field(FUNCNAME,TYPE,next,CMP_A_TO_B)

///////////////////
//
//  DYNAMIC ARRAY
//
ON_DESTROY_CALLBACK (pooled_free_call)
{
    free (*(void**)clsr);
}

#define DYNAMIC_ARRAY_DEFINE(type, name)             \
    type *name;                                      \
    int name ## _len;                                \
    int name ## _size

#define DYNAMIC_ARRAY_REALLOC(head_name,new_size)           \
    if (new_size != 0) {                                    \
        void *new_head =                                    \
            realloc(head_name, new_size*sizeof(*head_name));\
        if (new_head) {                                     \
            head_name = new_head;                           \
            head_name ## _size = new_size;                  \
        }                                                   \
    }

#define DYNAMIC_ARRAY_INITIAL_SIZE 50

#define DYNAMIC_ARRAY_INIT(pool,head_name,initial_size)                                 \
{                                                                                       \
    mem_pool_push_cb(pool, pooled_free_call, &head_name);                               \
    head_name ## _size = initial_size == 0 ? DYNAMIC_ARRAY_INITIAL_SIZE : initial_size; \
    DYNAMIC_ARRAY_REALLOC (head_name, head_name ## _size);                              \
}

#define DYNAMIC_ARRAY_APPEND(head_name,element)                             \
{                                                                           \
    size_t new_size = 0;                                                    \
    if (0 == head_name ## _size) {                                          \
        new_size = DYNAMIC_ARRAY_INITIAL_SIZE;                              \
    } else if (head_name ## _size == head_name ## _len) {                   \
        new_size = 2*(head_name ## _size);                                  \
    }                                                                       \
                                                                            \
    DYNAMIC_ARRAY_REALLOC(head_name, new_size)                              \
                                                                            \
    (head_name)[(head_name ## _len)++] = element;                           \
}

// In some cases we can't assign to a type by assigning to it, for example in
// the case we are storing string_t structures, if we assign an empty string the
// allocated internal memory pointer will be leaked. In such cases we just want
// to get a pointer to the appended value and then the caller will handle what
// to do next.
//
// CAUTION: Do not store the pointers returned by this persistently! When the
// array grows and is reallocated the pointer will become invalid!.
// TODO: This is probably even useless, in cases where something like this is
// necessary, we should really be using linked lists.
#define DYNAMIC_ARRAY_APPEND_GET(head_name, type, name)                     \
{                                                                           \
    size_t new_size = 0;                                                    \
    if (0 == head_name ## _size) {                                          \
        new_size = DYNAMIC_ARRAY_INITIAL_SIZE;                              \
    } else if (head_name ## _size == head_name ## _len) {                   \
        new_size = 2*(head_name ## _size);                                  \
    }                                                                       \
                                                                            \
    DYNAMIC_ARRAY_REALLOC(head_name, new_size)                              \
}                                                                           \
type name = (head_name) + ((head_name ## _len)++);

#define COMMON_H
#endif
