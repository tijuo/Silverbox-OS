#include <util.h>
#include <types.h>
#include "init.h"

NON_NULL_PARAMS int memcmp(const void* m1, const void* m2, size_t num)
{
    size_t i;
    const char* mem1 = (const char*)m1;
    const char* mem2 = (const char*)m2;

    for(i = 0; i < num && mem1[i] == mem2[i]; i++)
        ;

    if(i == num)
        return 0;
    else
        return (mem1[i] > mem2[i] ? 1 : -1);
}

NON_NULL_PARAMS int strncmp(const char* str1, const char* str2, size_t num)
{
    size_t i;

    for(i = 0; i < num && str1[i] == str2[i] && str1[i]; i++)
        ;

    if(i == num)
        return 0;
    else
        return (str1[i] > str2[i] ? 1 : -1);
}

char* strstr(const char* str, const char* substr)
{
    register size_t i;

    if(str && substr) {
        for(; *str; str++) {
            for(i = 0; str[i] == substr[i] && substr[i]; i++)
                ;

            if(!substr[i])
                return str;
        }
    }

    return NULL;
}

size_t strlen(const char* s)
{
    if(!s)
        return 0;

    return (size_t)(strchr(s, '\0') - s);
}

char* strchr(const char* s, int c)
{
    if(s) {
        while(*s) {
            if(*s == c)
                return s;
            else
                s++;
        }

        return s;
    }

    return NULL;
}

