#include <stdio.h>
#include <string.h>
#include "tests.h"
#include <os/ostypes/string.h>

int test_init_string(void);
int test_str_slice(void);
int test_string_null_allocator(void);
int test_string_slicing(void);
int test_string_contains(void);
int test_string_compare(void);
int test_string_to_ascii_lowercase(void);
int test_string_to_ascii_uppercase(void);
int test_string_concat(void);
int test_string_find_substring(void);
int test_string_rfind_substring(void);
int test_string_find_any(void);
int test_string_rfind_any(void);

Test tests[] = {
    TEST(test_init_string),
    TEST(test_str_slice),
    TEST(test_string_null_allocator),
    TEST(test_string_slicing),
    TEST(test_string_contains),
    TEST(test_string_compare),
    TEST(test_string_to_ascii_lowercase),
    TEST(test_string_to_ascii_uppercase),
    TEST(test_string_concat),
    TEST(test_string_find_substring),
    TEST(test_string_rfind_substring),
    TEST(test_string_find_any),
    TEST(test_string_rfind_any),
    END_TESTS
};

int test_init_string(void) {
    String s;

    ASSERT_EQ(string_init(&s, 'a', 16, &GLOBAL_ALLOCATOR), &s);
    ASSERT_EQ(s.length, 16);

    string_destroy(&s);

    return 0;
}

int test_string_null_allocator(void) {
    String s;

    ASSERT_NULL(string_init(&s, 'X', 32, NULL));
    ASSERT_NULL(string_from_str(&s, STR("This should not work"), NULL));
    ASSERT_NULL(string_from_cstr(&s, "this shouldn't work either", NULL));

    return 0;
}

int test_str_slice(void) {
    ASSERT_ZERO(strcmp(STR("This is a test.").ptr, "This is a test."));
    ASSERT_ZERO(strcmp(STR("").ptr, ""));
    ASSERT_ZERO(STR("").length);
    ASSERT_EQ(STR("strings").length, strlen("strings"));

    return 0;
}

int test_string_slicing(void) {
    String s;

    ASSERT_EQ(string_from_cstr(&s, "Hello, world!", &GLOBAL_ALLOCATOR), &s);

    StrSlice slice = string_slice(&s, 2, 9);

    ASSERT_EQ(slice.length, 7);
    ASSERT_ZERO(strncmp(slice.ptr, "llo, wo", slice.length));

    string_destroy(&s);

    return 0;
}

int test_string_contains(void) {
    String s;

    ASSERT_NON_NULL(string_from_str(&s, STR("The quick brown fox jumps over the lazy dog."), &GLOBAL_ALLOCATOR));
    ASSERT_TRUE(string_contains(&s, 'h'));
    ASSERT_TRUE(string_contains(&s, 'o'));
    ASSERT_FALSE(string_contains(&s, '?'));
    ASSERT_FALSE(string_contains(&s, '\n'));
    ASSERT_FALSE(string_contains(&s, '\0'));

    string_destroy(&s);

    return 0;
}

int test_string_find_any(void) {
    String s;
    String needles1;
    String needles2;
    String needles3;
    String needles4;

    ASSERT_NON_NULL(string_from_str(&s, STR("The quick brown fox jumps over the lazy dog."), &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_str(&needles1, STR("aeiou"), &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_str(&needles2, STR("AK?/*"), &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_str(&needles3, STR("Grant it"), &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_str(&needles4, STR(""), &GLOBAL_ALLOCATOR));

    ASSERT_EQ(string_find_any(&s, &needles1), 2);
    ASSERT_EQ(string_find_any(&s, &needles2), -1);
    ASSERT_EQ(string_find_any(&s, &needles3), 3);
    ASSERT_EQ(string_find_any(&s, &needles4), -1);

    string_destroy(&needles4);
    string_destroy(&needles3);
    string_destroy(&needles2);
    string_destroy(&needles1);
    string_destroy(&s);

    return 0;
}

int test_string_rfind_any(void) {
    String s;
    String needles1;
    String needles2;
    String needles3;
    String needles4;
    
    ASSERT_NON_NULL(string_from_str(&s, STR("The quick brown fox jumps over the lazy dog."), &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_str(&needles1, STR("aeiou"), &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_str(&needles2, STR("AK?/*"), &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_str(&needles3, STR("Grant it"), &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_str(&needles4, STR(""), &GLOBAL_ALLOCATOR));

    ASSERT_EQ(string_rfind_any(&s, &needles1), 41);
    ASSERT_EQ(string_rfind_any(&s, &needles2), -1);
    ASSERT_EQ(string_rfind_any(&s, &needles3), 39);
    ASSERT_EQ(string_rfind_any(&s, &needles4), -1);

    string_destroy(&needles4);
    string_destroy(&needles3);
    string_destroy(&needles2);
    string_destroy(&needles1);
    string_destroy(&s);

    return 0;
}

int test_string_compare(void) {
    String s;
    String s2;
    String s3;
    String s4;

    ASSERT_NON_NULL(string_from_cstr(&s, "testing testing one two three. mic check one two one two", &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_cstr(&s2, "testing testing one two three. mic check one two one two", &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_cstr(&s3, "wonder", &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_cstr(&s4, "", &GLOBAL_ALLOCATOR));

    ASSERT_ZERO(string_compare(&s, &s));
    ASSERT_ZERO(string_compare(&s2, &s2));
    ASSERT_ZERO(string_compare(&s3, &s3));
    ASSERT_ZERO(string_compare(&s4, &s4));

    ASSERT_ZERO(string_compare(&s, &s2));
    ASSERT_ZERO(string_compare(&s2, &s));
    ASSERT_NON_ZERO(string_compare(&s, &s3));
    ASSERT_NON_ZERO(string_compare(&s, &s4));
    ASSERT_NON_ZERO(string_compare(&s3, &s4));

    string_destroy(&s4);
    string_destroy(&s3);
    string_destroy(&s2);
    string_destroy(&s);

    return 0;
}

int test_string_to_ascii_lowercase(void) {
    String s;
    String s2;
    String s3;
    String s4;
    String s5;

    string_from_cstr(&s, "i see you", &GLOBAL_ALLOCATOR);
    string_from_cstr(&s2, "I SEE YOU", &GLOBAL_ALLOCATOR);
    string_from_cstr(&s3, "There are 3 dogs in the back yard.", &GLOBAL_ALLOCATOR);
    string_from_cstr(&s4, "THERE ARE 3 DOGS IN THE BACK YARD.", &GLOBAL_ALLOCATOR);
    string_from_cstr(&s5, "there are 3 dogs in the back yard.", &GLOBAL_ALLOCATOR);

    ASSERT_NON_ZERO(string_compare(&s, &s2));
    ASSERT_NON_ZERO(string_compare(&s3, &s4));
    ASSERT_NON_ZERO(string_compare(&s4, &s5));
    ASSERT_NON_ZERO(string_compare(&s3, &s5));
    ASSERT_NON_ZERO(string_compare(&s, &s3));
    ASSERT_NON_ZERO(string_compare(&s2, &s4));
    ASSERT_NON_ZERO(string_compare(&s, &s5));

    string_to_ascii_lowercase(&s2);
    string_to_ascii_lowercase(&s3);
    string_to_ascii_lowercase(&s4);

    ASSERT_ZERO(string_compare(&s, &s2));
    ASSERT_ZERO(string_compare(&s3, &s4));
    ASSERT_ZERO(string_compare(&s4, &s5));
    ASSERT_ZERO(string_compare(&s3, &s5));
    ASSERT_NON_ZERO(string_compare(&s, &s3));
    ASSERT_NON_ZERO(string_compare(&s2, &s4));
    ASSERT_NON_ZERO(string_compare(&s, &s5));

    string_destroy(&s5);
    string_destroy(&s4);
    string_destroy(&s3);
    string_destroy(&s2);
    string_destroy(&s);

    return 0;
}

int test_string_to_ascii_uppercase(void) {
    String s;
    String s2;
    String s3;
    String s4;
    String s5;

    string_from_cstr(&s, "i see you", &GLOBAL_ALLOCATOR);
    string_from_cstr(&s2, "I SEE YOU", &GLOBAL_ALLOCATOR);
    string_from_cstr(&s3, "There are 3 dogs in the back yard.", &GLOBAL_ALLOCATOR);
    string_from_cstr(&s4, "THERE ARE 3 DOGS IN THE BACK YARD.", &GLOBAL_ALLOCATOR);
    string_from_cstr(&s5, "there are 3 dogs in the back yard.", &GLOBAL_ALLOCATOR);

    ASSERT_NON_ZERO(string_compare(&s, &s2));
    ASSERT_NON_ZERO(string_compare(&s3, &s4));
    ASSERT_NON_ZERO(string_compare(&s4, &s5));
    ASSERT_NON_ZERO(string_compare(&s3, &s5));
    ASSERT_NON_ZERO(string_compare(&s, &s3));
    ASSERT_NON_ZERO(string_compare(&s2, &s4));
    ASSERT_NON_ZERO(string_compare(&s, &s5));

    string_to_ascii_uppercase(&s);
    string_to_ascii_uppercase(&s2);
    string_to_ascii_uppercase(&s3);
    string_to_ascii_uppercase(&s4);
    string_to_ascii_uppercase(&s5);

    ASSERT_ZERO(string_compare(&s, &s2));
    ASSERT_ZERO(string_compare(&s3, &s4));
    ASSERT_ZERO(string_compare(&s4, &s5));
    ASSERT_ZERO(string_compare(&s3, &s5));
    ASSERT_NON_ZERO(string_compare(&s, &s3));
    ASSERT_NON_ZERO(string_compare(&s2, &s4));
    ASSERT_NON_ZERO(string_compare(&s, &s5));

    string_destroy(&s5);
    string_destroy(&s4);
    string_destroy(&s3);
    string_destroy(&s2);
    string_destroy(&s);

    return 0;
}

int test_string_concat(void) {
    String s;
    String s2;
    String s3;
    String s4;
    String s5;

    ASSERT_NON_NULL(string_from_cstr(&s, "This ", &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_cstr(&s2, "is ", &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_cstr(&s3, "a ", &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_cstr(&s4, "test.", &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_cstr(&s5, "This is a test.", &GLOBAL_ALLOCATOR));

    ASSERT_EQ(string_concat(&s, &s2), &s);
    ASSERT_EQ(string_concat(&s, &s3), &s);
    ASSERT_EQ(string_concat(&s, &s4), &s);

    ASSERT_ZERO(string_compare(&s, &s5));

    string_destroy(&s5);
    string_destroy(&s4);
    string_destroy(&s3);
    string_destroy(&s2);
    string_destroy(&s);

    return 0;
}

int test_string_find_substring(void) {
    String s;
    String s2;
    String s3;
    String s4;
    StrSlice slice;

    ASSERT_NON_NULL(string_from_cstr(&s, "testing testing one two three. mic check one two one two", &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_cstr(&s2, "two", &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_cstr(&s3, "wonder", &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_cstr(&s4, "", &GLOBAL_ALLOCATOR));

    slice = string_find_substring(&s, &s);

    ASSERT_EQ((char *)slice.ptr, (char *)s.ptr);
    ASSERT_EQ(slice.length, s.length);

    slice = string_find_substring(&s, &s2);

    ASSERT_EQ((char *)slice.ptr, (char *)&s.ptr[20]);

    ASSERT_EQ(slice.length, s2.length);

    ASSERT(SLICE_IS_NULL(string_rfind_substring(&s, &s3)));
    ASSERT(SLICE_IS_NULL(string_rfind_substring(&s, &s4)));

    string_destroy(&s4);
    string_destroy(&s3);
    string_destroy(&s2);
    string_destroy(&s);

    return 0;
}

int test_string_rfind_substring(void) {
    String s;
    String s2;
    String s3;
    String s4;
    StrSlice slice;

    ASSERT_NON_NULL(string_from_cstr(&s, "testing testing one two three. mic check one two one two", &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_cstr(&s2, "two", &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_cstr(&s3, "wonder", &GLOBAL_ALLOCATOR));
    ASSERT_NON_NULL(string_from_cstr(&s4, "", &GLOBAL_ALLOCATOR));

    slice = string_rfind_substring(&s, &s);

    ASSERT_EQ((char *)slice.ptr, (char *)s.ptr);
    ASSERT_EQ(slice.length, s.length);

    slice = string_rfind_substring(&s, &s2);

    ASSERT_EQ((char *)slice.ptr, (char *)&s.ptr[53]);

    ASSERT_EQ(slice.length, s2.length);

    ASSERT(SLICE_IS_NULL(string_rfind_substring(&s, &s3)));
    ASSERT(SLICE_IS_NULL(string_rfind_substring(&s, &s4)));

    string_destroy(&s4);
    string_destroy(&s3);
    string_destroy(&s2);
    string_destroy(&s);

    return 0;
}

int main(int argc, char *argv[]) {
    return tests_run(argc, argv, tests);
}
