#ifndef STRING_H
#define STRING_H

// Standard string comparison helper since we do not have standard library
static int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// String prefix comparison helper
static int strncmp(const char *s1, const char *s2, int n) {
    while (n--) {
        if (*s1 != *s2++) {
            return *(const unsigned char*)s1 - *(const unsigned char*)--s2;
        }
        if (*s1++ == 0) {
            break;
        }
    }
    return 0;
}

static int strlen_simple(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

#endif // STRING_H