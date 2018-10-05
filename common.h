/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#if !defined(COMMON_H)
#include <stdio.h>
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

#ifdef __cplusplus
#define ZERO_INIT(type) (type){}
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

#define WRAP(a,lower,upper) ((a)>(upper)?(lower):((a)<(lower)?(upper):(a)))

#define LOW_CLAMP(a,lower) ((a)<(lower)?(lower):(a))

#define I_CEIL_DIVIDE(a,b) ((a%b)?(a)/(b)+1:(a)/(b))

#define kilobyte(val) ((val)*1024LL)
#define megabyte(val) (kilobyte(val)*1024LL)
#define gigabyte(val) (megabyte(val)*1024LL)
#define terabyte(val) (gigabyte(val)*1024LL)

#define invalid_code_path assert(0)

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
#define str_data(string) (str_is_small(string)?(string)->str_small:(string)->str)

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

char str_last (string_t *str)
{
    return str_data(str)[str_len(str)-1];
}

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

// Templetized merge sort
// IS_A_LT_B is an expression where a and b are pointers
// to _arr_ true when *a<*b.
#define templ_sort(FUNCNAME,TYPE,IS_A_LT_B)                     \
void FUNCNAME ## _user_data (TYPE *arr, int n, void *user_data) \
{                                                               \
    if (n<=1) {                                                 \
        return;                                                 \
    } else if (n == 2) {                                        \
        TYPE *a = &arr[1];                                      \
        TYPE *b = &arr[0];                                      \
        int c = IS_A_LT_B;                                      \
        if (c) {                                                \
            swap_n_bytes (&arr[0], &arr[1], sizeof(TYPE));      \
        }                                                       \
    } else {                                                    \
        TYPE res[n];                                            \
        FUNCNAME ## _user_data (arr, n/2, user_data);           \
        FUNCNAME ## _user_data (&arr[n/2], n-n/2, user_data);   \
                                                                \
        int i;                                                  \
        int h=0;                                                \
        int k=n/2;                                              \
        for (i=0; i<n; i++) {                                   \
            TYPE *a = &arr[h];                                  \
            TYPE *b = &arr[k];                                  \
            if (k==n || (h<n/2 && (IS_A_LT_B))) {               \
                res[i] = arr[h];                                \
                h++;                                            \
            } else {                                            \
                res[i] = arr[k];                                \
                k++;                                            \
            }                                                   \
        }                                                       \
        for (i=0; i<n; i++) {                                   \
            arr[i] = res[i];                                    \
        }                                                       \
    }                                                           \
}                                                               \
\
void FUNCNAME(TYPE *arr, int n) {                               \
    FUNCNAME ## _user_data (arr,n,NULL);                        \
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
    int i;
    if (start != NULL) {
        printf ("%s", start);
    }

    if (sep != NULL) {
        for (i=0; i<n-1; i++) {
            printf ("%d%s", arr[i], sep);
        }
    } else {
        for (i=0; i<n-1; i++) {
            printf ("%d", arr[i]);
        }
    }

    if (end != NULL) {
        printf ("%d%s", arr[i], end);
    } else {
        printf ("%d", arr[i]);
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
#define MEM_POOL_MIN_BIN_SIZE 1024u
typedef struct {
    uint32_t min_bin_size;
    uint32_t size;
    uint32_t used;
    void *base;

    uint32_t total_used;
    uint32_t num_bins;
} mem_pool_t;

struct _bin_info_t {
    void *base;
    uint32_t size;
    struct _bin_info_t *prev_bin_info;
};

typedef struct _bin_info_t bin_info_t;

enum alloc_opts {
    POOL_UNINITIALIZED,
    POOL_ZERO_INIT
};

#define mem_pool_push_struct(pool, type) mem_pool_push_size(pool, sizeof(type))
#define mem_pool_push_array(pool, n, type) mem_pool_push_size(pool, (n)*sizeof(type))
#define mem_pool_push_size(pool, size) mem_pool_push_size_full(pool, size, POOL_UNINITIALIZED)
void* mem_pool_push_size_full (mem_pool_t *pool, uint32_t size, enum alloc_opts opts)
{
    if (size == 0) return NULL;

    if (pool->used + size >= pool->size) {
        pool->num_bins++;
        int new_bin_size = MAX (MAX (MEM_POOL_MIN_BIN_SIZE, pool->min_bin_size), size);
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
    pool->used += size;
    pool->total_used += size;

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

void mem_pool_print (mem_pool_t *pool)
{
    uint32_t allocated = mem_pool_allocated(pool);
    printf ("Allocated: %u bytes\n", allocated);
    printf ("Available: %u bytes\n", pool->size-pool->used);
    printf ("Used: %u bytes (%.2f%%)\n", pool->total_used, ((double)pool->total_used*100)/allocated);
    uint64_t info_size = pool->num_bins*sizeof(bin_info_t);
    printf ("Info: %lu bytes (%.2f%%)\n", info_size, ((double)info_size*100)/allocated);

    // NOTE: This is the amount of space left empty in previous bins
    uint64_t left_empty;
    if (pool->num_bins>0)
        left_empty = (allocated - pool->size - sizeof(bin_info_t))- /*allocated except last bin*/
                     (pool->total_used - pool->used)-               /*total_used except last bin*/
                     (pool->num_bins-1)*sizeof(bin_info_t);         /*size used in bin_info_t*/
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
    uint32_t total_used;
} mem_pool_temp_marker_t;

mem_pool_temp_marker_t mem_pool_begin_temporary_memory (mem_pool_t *pool)
{
    mem_pool_temp_marker_t res;
    res.total_used = pool->total_used;
    res.used = pool->used;
    res.base = pool->base;
    res.pool = pool;
    return res;
}

void mem_pool_end_temporary_memory (mem_pool_temp_marker_t mrkr)
{
    if (mrkr.base != NULL) {
        bin_info_t *curr_info = (bin_info_t*)((uint8_t*)mrkr.pool->base + mrkr.pool->size);
        while (curr_info->base != mrkr.base) {
            void *to_free = curr_info->base;
            curr_info = curr_info->prev_bin_info;
            free (to_free);
            mrkr.pool->num_bins--;
        }
        mrkr.pool->size = curr_info->size;
        mrkr.pool->base = mrkr.base;
        mrkr.pool->used = mrkr.used;
        mrkr.pool->total_used = mrkr.total_used;
    } else {
        // NOTE: Here mrkr was created before the pool was initialized, so we
        // destroy everything.
        mem_pool_destroy (mrkr.pool);

        // This assumes pool wasn't bootstrapped after taking mrkr
        mrkr.pool->size = 0;
        mrkr.pool->base = NULL;
        mrkr.pool->used = 0;
        mrkr.pool->total_used = 0;
    }
}

// pom == pool or malloc
#define pom_push_struct(pool, type) pom_push_size(pool, sizeof(type))
#define pom_push_array(pool, n, type) pom_push_size(pool, (n)*sizeof(type))
#define pom_push_size(pool, size) (pool==NULL? malloc(size) : mem_pool_push_size(pool,size))

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
char* sh_expand (const char *str, mem_pool_t *pool)
{
    wordexp_t out;
    wordexp (str, &out, 0);
    char *res = collapse_str_arr (out.we_wordv, out.we_wordc, pool);
    wordfree (&out);
    return res;
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
    char *dir_path = sh_expand (path, NULL);

    // TODO: If writing fails, we will leave a blank file behind. We should make
    // a backup in case things go wrong.
    int file = open (dir_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
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

    free (dir_path);
    return failed;
}

char* full_file_read (mem_pool_t *pool, const char *path)
{
    bool success = true;
    char *dir_path = sh_expand (path, NULL);

    mem_pool_temp_marker_t mrk;
    if (pool != NULL) {
        mrk = mem_pool_begin_temporary_memory (pool);
    }

    char *loaded_data = NULL;
    struct stat st;
    if (stat(dir_path, &st) == 0) {
        loaded_data = (char*)pom_push_size (pool, st.st_size + 1);

        int file = open (dir_path, O_RDONLY);
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

    free (dir_path);
    return retval;
}

char* full_file_read_prefix (mem_pool_t *out_pool, const char *path, char **prefix, int len)
{
    mem_pool_t pool = {0};
    string_t pfx_s = {0};
    string_t path_s = str_new (path);
    char *dir_path = sh_expand (path, &pool);

    int status, i = 0;
    struct stat st;
    while ((status = (stat(dir_path, &st) == -1)) && i < len) {
        if (errno == ENOENT && prefix != NULL && *prefix != NULL) {
            str_set (&pfx_s, prefix[i]);
            assert (str_last(&pfx_s) == '/');
            str_cat (&pfx_s, &path_s);
            dir_path = sh_expand (str_data(&pfx_s), &pool);
            i++;
        } else {
            break;
        }
    }
    str_free (&pfx_s);
    str_free (&path_s);

    mem_pool_temp_marker_t mrk;
    if (out_pool != NULL) {
        mrk = mem_pool_begin_temporary_memory (out_pool);
    }

    bool success = true;
    char *loaded_data = NULL;
    if (status == 0) {
        loaded_data = (char*)pom_push_size (out_pool, st.st_size + 1);

        int file = open (dir_path, O_RDONLY);
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
    } else {
        success = false;
        printf ("Could not locate %s in any folder.\n", path);
    }

    char *retval = NULL;
    if (success) {
        retval = loaded_data;
    } else if (loaded_data != NULL) {
        if (out_pool != NULL) {
            mem_pool_end_temporary_memory (mrk);
        } else {
            free (loaded_data);
        }
    }

    mem_pool_destroy (&pool);
    return retval;
}

bool path_exists (char *path)
{
    bool retval = true;
    char *dir_path = sh_expand (path, NULL);

    struct stat st;
    int status;
    if ((status = stat(dir_path, &st)) == -1) {
        retval = false;
        if (errno != ENOENT) {
            printf ("Error checking existance of %s: %s\n", path, strerror(errno));
        }
    }
    free (dir_path);
    return retval;
}

// NOTE: Returns false if there was an error creating the directory.
bool ensure_dir_exists (char *path)
{
    bool retval = true;
    char *dir_path = sh_expand (path, NULL);

    struct stat st;
    int success = 0;
    if (stat(dir_path, &st) == -1 && errno == ENOENT) {
        success = mkdir (dir_path, 0777);
    }

    if (success == -1) {
        char *expl = strerror (errno);
        printf ("Could not create %s: %s\n", dir_path, expl);
        free (expl);
        retval = false;
    }

    free (dir_path);
    return retval;
}

// Checks if path exists (either as a file or directory). If it doesn't it tries
// to create all directories required for it to exist. If path ends in / then
// all components are checked, otherwise the last part after / is assumed to be
// a filename and is not created as a directory.
bool ensure_path_exists (const char *path)
{
    bool success = true;
    char *dir_path = sh_expand (path, NULL);

    char *c = dir_path;
    if (*c == '/') {
        c++;
    }

    struct stat st;
    if (stat(dir_path, &st) == -1) {
        if (errno == ENOENT) {
            while (*c && success) {
                while (*c && *c != '/') {
                    c++;
                }

                if (*c != '\0') {
                    *c = '\0';
                    if (stat(dir_path, &st) == -1 && errno == ENOENT) {
                        if (mkdir (dir_path, 0777) == -1) {
                            success = false;
                            printf ("Error creating %s: %s\n", dir_path, strerror (errno));
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

    free (dir_path);
    return success;
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

    char *res = (char*)mem_pool_push_size (pool, path_len+strlen(new_ext)+1);
    strcpy (res, path);
    strcpy (&res[i], new_ext);
    return res;
}

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

    char *res = (char*)mem_pool_push_size (pool, end_pos+1);
    memmove (res, path, end_pos);
    res[end_pos] = '\0';
    return res;
}

char* add_extension (mem_pool_t *pool, char *path, char *new_ext)
{
    size_t path_len = strlen(path);
    char *res = (char*)mem_pool_push_size (pool, path_len+strlen(new_ext)+2);
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


#define COMMON_H
#endif
