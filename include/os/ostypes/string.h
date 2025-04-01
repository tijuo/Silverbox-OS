#ifndef OS_TYPES_STRING_H
#define OS_TYPES_STRING_H

#include "slice.h"
#include <os/allocator.h>
#include <stdbool.h>

typedef SLICE(const char) StrSlice;

#define STR(s)              (StrSlice) { .ptr = (s), .length = sizeof(s) - 1 }

typedef struct {
    char *ptr;
    size_t length;
    Allocator allocator;
} String;

#define STRING_AS_STR(s)    (StrSlice) { .ptr = (s).ptr, .length = (s).length }

static inline const char *string_as_cstr(const String *s) { return (const char *)s->ptr; }
static inline const char *str_slice_as_cstr(const StrSlice *s) { return (const char *)s->ptr; }

String *string_from_str(String *string, StrSlice str_slice, Allocator *allocator);
String *string_from_sequence(String *string, const char *s, size_t length, Allocator *allocator);
String *string_from_cstr(String *string, const char *s, Allocator *allocator);
String *string_init(String *string, char fill, size_t length, Allocator *allocator);
String *string_clone(String *string, String *new_string);
String *string_concat(String *augend, const String *addend);
String *string_substring(String *string, String *substring, int start, int end);
StrSlice string_slice(const String *string, int start, int end);
StrSlice string_find_substring(const String *haystack, const String *needle);
StrSlice string_rfind_substring(const String *haystack, const String *needle);

int string_compare(const String *str1, const String *str2);
int string_find_any(const String *haystack, const String *needles);
int string_rfind_any(const String *haystack, const String *needles);
int string_ascii_count(const String *haystack, const String *needles);
void string_transform(String *string, char (*func)(String *, int));
void string_to_ascii_uppercase(String *string);
void string_to_ascii_lowercase(String *string);
bool string_contains(const String *haystack, char needle);
void string_destroy(String *string);

int str_slice_compare(StrSlice s1, StrSlice s2);

#define string_cstr(s)  _Generic((s),\
    String*: string_as_cstr,\
    StrSlice*: str_slice_as_cstr)(s)
#endif /* OS_TYPES_STRING_H */