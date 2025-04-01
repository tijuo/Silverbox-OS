#include <os/ostypes/string.h>
#include <os/allocator.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

static char to_ascii_upper_char(String *s, int i) {
    return toupper(s->ptr[i]);
}

static char to_ascii_lower_char(String *s, int i) {
    return tolower(s->ptr[i]);
}

/** Create an owned string from a string slice.
 * 
 * @param string The new owned string.
 * @param str_slice The string slice.
 * @param allocator The allocator to use for the owned string. If `NULL`, then use the global allocator. 
 * @return The new owned string. `NULL`, on failure.
 * 
 */
String *string_from_str(String *string, StrSlice str_slice, Allocator *allocator) {
    string->allocator = allocator ? *allocator : NULL_ALLOCATOR;

    ByteSlice new_slice = allocator_alloc(&string->allocator, str_slice.length + 1, sizeof(size_t));

    if(!new_slice.ptr) {
        return NULL;
    }

    strcpy((char *)new_slice.ptr, str_slice.ptr);

    string->ptr = (char *)new_slice.ptr;
    string->length = str_slice.length;

    return string;
}

/** Create an owned string from a sequence of chars of known length.
 * 
 * @param string The new owned string.
 * @param s The sequence.
 * @param length The length of the sequence, in bytes.
 * @param allocator The allocator to use for the owned string. If `NULL`, then use the global allocator. 
 * @return The new owned string. `NULL`, on failure.
 * 
 */
String *string_from_sequence(String *string, const char *s, size_t length, Allocator *allocator) {
    string->allocator = allocator ? *allocator : NULL_ALLOCATOR;
    string->length = length;

    ByteSlice new_slice = allocator_alloc(&string->allocator, length + 1, sizeof(size_t));

    if(!new_slice.ptr) {
        return NULL;
    }

    memcpy(new_slice.ptr, s, length);
    string->ptr = (char *)new_slice.ptr;
    string->ptr[string->length] = '\0';

    return string;
}

/** Create an owned string from a null-terminated C string.
 * 
 * @param string The new owned string.
 * @param s The C string.
 * @param allocator The allocator to use for the owned string. If `NULL`, then use the global allocator. 
 * @return The new owned string. `NULL`, on failure.
 * 
 */
String *string_from_cstr(String *string, const char *s, Allocator *allocator) {
    return string_from_sequence(string, s, strlen(s), allocator);
}

/** Create an owned string from a string slice.
 * 
 * @param string The new owned string.
 * @param str_slice The string slice.
 * @param allocator The allocator to use for the owned string. If `NULL`, then use the global allocator. 
 * @return The new owned string. `NULL`, on failure.
 * 
 */
String *string_init(String *string, char fill, size_t length, Allocator *allocator) {
    string->allocator = allocator ? *allocator : NULL_ALLOCATOR;
    string->length = length;
    
    ByteSlice new_slice = allocator_alloc(&string->allocator, length + 1, sizeof(size_t));

    if(!new_slice.ptr) {
        return NULL;
    }

    memset(new_slice.ptr, fill, length);

    string->ptr = (char *)new_slice.ptr;
    string->ptr[length] = '\0';

    return string;
}

/**
 * Make a deep copy of a string.
 * 
 * @param new_string The cloned string.
 * @param string The string to be cloned.
 * @return The cloned string. `NULL`, on failure.
 */
String *string_clone(String *string, String *new_string) {
    if(string_from_str(new_string, STRING_AS_STR(*string), &string->allocator) == NULL){
        return NULL;
    }

    new_string->ptr[new_string->length] = '\0';

    return new_string;
}

String *string_concat(String *augend, const String *addend) {
    ByteSlice new_slice = allocator_resize(&augend->allocator, augend->ptr, augend->length + 1, augend->length + addend->length + 1);

    if(!new_slice.ptr) {
        return NULL;
    }

    strcpy((char *)&new_slice.ptr[augend->length], addend->ptr);
    augend->ptr = (char *)new_slice.ptr;
    augend->length = new_slice.length - 1;

    return augend;
}

String *string_substring(String *string, String *substring, int start, int end) {
    StrSlice slice = string_slice(string, start, end);
    *substring = (String){ .ptr=(void *)slice.ptr, .length = slice.length, .allocator = string->allocator };
    return substring;
}

StrSlice string_slice(const String *string, int start, int end) {
    size_t start_index;
    size_t end_index;

    if(start < 0) {
        start_index = string->length + (size_t)start;
    } else {
        start_index = (size_t)start;
    }

    if(end < 0) {
        end_index = string->length + (size_t)end;
    } else {
        end_index = (size_t)end;
    }

    start_index = (start_index >= string->length) ? string->length : start_index;
    end_index = (end_index >= string->length) ? string->length : end_index;

    return (StrSlice){ .ptr=&string->ptr[start_index], .length = (end_index <= start_index) ?  0 : (end_index - start_index) };
}

int string_find_any(const String *haystack, const String *needles) {
    const char *p = strpbrk(haystack->ptr, needles->ptr);
    return p ? (int)(p - haystack->ptr) : -1;
}

int string_rfind_any(const String *haystack, const String *needles) {
    const char *start = &haystack->ptr[0];

    for(const char *ptr=&haystack->ptr[haystack->length]; ptr != start; ptr--) {
        if(string_contains(needles, *(ptr-1))) {
            return (int)(ptr - start - 1);
        }
    }

    return -1;
}

StrSlice string_find_substring(const String *haystack, const String *needle) {
    const char *p = strstr(haystack->ptr, needle->ptr);
    
    if(p) {
        return SLICE_FROM_ARRAY(StrSlice, p, needle->length);
    }

    return NULL_SLICE(StrSlice);
}

StrSlice string_rfind_substring(const String *haystack, const String *needle) {
    if(haystack->length >= needle->length && needle->length > 0) {
        for(int i=(int)(haystack->length-needle->length); i >= 0; i--) {
            if(strncmp(&haystack->ptr[i], needle->ptr, needle->length) == 0) {
                return SLICE_FROM_ARRAY(StrSlice, &haystack->ptr[i], needle->length);
            }
        }
    }

    return NULL_SLICE(StrSlice);
}

int string_ascii_count(const String *haystack, const String *needles) {
    int count = 0;
    const char *end = &haystack->ptr[haystack->length];

    for(const char *ptr=&haystack->ptr[0]; ptr != end && count != INT_MAX; ptr++) {
        if(string_contains(needles, *ptr)) {
            count += 1;
        }
    }

    return count;
}

void string_transform(String *string, char (*func)(String *, int)) {
    char *start = &string->ptr[0];
    char *end = &string->ptr[string->length];

    for(char *ptr=start; ptr != end; ptr++) {
        *ptr = func(string, ptr - start);
    }
}

void string_to_ascii_uppercase(String *string) {
    string_transform(string, to_ascii_upper_char);
}

void string_to_ascii_lowercase(String *string) {
    string_transform(string, to_ascii_lower_char);
}

bool string_contains(const String *haystack, char needle) {
    char *s=strchr(haystack->ptr, needle);
    return s != NULL && s != &haystack->ptr[haystack->length];
}

void string_destroy(String *string) {
    allocator_free(&string->allocator, string->ptr);
}

int string_compare(const String *str1, const String *str2) {
    return strcmp(str1->ptr, str2->ptr);
}

int str_slice_compare(StrSlice s1, StrSlice s2) {
    return strcmp(s1.ptr, s2.ptr);
}