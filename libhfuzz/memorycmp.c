#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "libhfcommon/common.h"
#include "libhfcommon/util.h"
#include "libhfuzz/instrument.h"

__attribute__((visibility("default"))) __attribute__((used))
const char* const LIBHFUZZ_module_memorycmp = "LIBHFUZZ_module_memorycmp";

static inline int HF_strcmp(const char* s1, const char* s2, uintptr_t addr) {
    size_t i;
    for (i = 0; s1[i] == s2[i]; i++) {
        if (s1[i] == '\0' || s2[i] == '\0') {
            break;
        }
    }

    instrumentUpdateCmpMap(addr, i);
    instrumentAddConstMem(s1, strlen(s1), /* check_if_ro= */ true);
    instrumentAddConstMem(s2, strlen(s2), /* check_if_ro= */ true);

    return ((int)s1[i] - (int)s2[i]);
}

static inline int HF_strcasecmp(const char* s1, const char* s2, uintptr_t addr) {
    size_t i;
    for (i = 0; tolower((unsigned char)s1[i]) == tolower((unsigned char)s2[i]); i++) {
        if (s1[i] == '\0' || s2[i] == '\0') {
            break;
        }
    }

    instrumentUpdateCmpMap(addr, i);
    instrumentAddConstMem(s1, strlen(s1), /* check_if_ro= */ true);
    instrumentAddConstMem(s2, strlen(s2), /* check_if_ro= */ true);

    return (tolower((unsigned char)s1[i]) - tolower((unsigned char)s2[i]));
}

static inline int HF_strncmp(const char* s1, const char* s2, size_t n, uintptr_t addr) {
    size_t i;
    for (i = 0; i < n; i++) {
        if ((s1[i] != s2[i]) || s1[i] == '\0' || s2[i] == '\0') {
            break;
        }
    }

    instrumentUpdateCmpMap(addr, i);
    instrumentAddConstMem(s1, strnlen(s1, n), /* check_if_ro= */ true);
    instrumentAddConstMem(s2, strnlen(s2, n), /* check_if_ro= */ true);

    if (i == n) {
        return 0;
    }
    return ((int)s1[i] - (int)s2[i]);
}

static inline int HF_strncasecmp(const char* s1, const char* s2, size_t n, uintptr_t addr) {
    size_t i;
    for (i = 0; i < n; i++) {
        if ((tolower((unsigned char)s1[i]) != tolower((unsigned char)s2[i])) || s1[i] == '\0' ||
            s2[i] == '\0') {
            break;
        }
    }

    instrumentUpdateCmpMap(addr, i);
    instrumentAddConstMem(s1, strnlen(s1, n), /* check_if_ro= */ true);
    instrumentAddConstMem(s2, strnlen(s2, n), /* check_if_ro= */ true);

    if (i == n) {
        return 0;
    }
    return tolower((unsigned char)s1[i]) - tolower((unsigned char)s2[i]);
}

static inline char* HF_strstr(const char* haystack, const char* needle, uintptr_t addr) {
    size_t needle_len = strlen(needle);
    if (needle_len == 0) {
        return (char*)haystack;
    }

    const char* h = haystack;
    for (; (h = __builtin_strchr(h, needle[0])) != NULL; h++) {
        if (HF_strncmp(h, needle, needle_len, addr) == 0) {
            return (char*)h;
        }
    }
    return NULL;
}

static inline char* HF_strcasestr(const char* haystack, const char* needle, uintptr_t addr) {
    size_t needle_len = strlen(needle);
    for (size_t i = 0; haystack[i]; i++) {
        if (HF_strncasecmp(&haystack[i], needle, needle_len, addr) == 0) {
            return (char*)(&haystack[i]);
        }
    }
    return NULL;
}

static inline int HF_memcmp(const void* m1, const void* m2, size_t n, uintptr_t addr) {
    const unsigned char* s1 = (const unsigned char*)m1;
    const unsigned char* s2 = (const unsigned char*)m2;

    size_t i;
    for (i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            break;
        }
    }

    instrumentUpdateCmpMap(addr, i);
    instrumentAddConstMem(m1, n, /* check_if_ro= */ true);
    instrumentAddConstMem(m2, n, /* check_if_ro= */ true);

    if (i == n) {
        return 0;
    }
    return ((int)s1[i] - (int)s2[i]);
}

static inline void* HF_memmem(const void* haystack, size_t haystacklen, const void* needle,
    size_t needlelen, uintptr_t addr) {
    if (needlelen > haystacklen) {
        return NULL;
    }
    if (needlelen == 0) {
        return (void*)haystack;
    }

    const char* h = haystack;
    for (size_t i = 0; i <= (haystacklen - needlelen); i++) {
        if (HF_memcmp(&h[i], needle, needlelen, addr) == 0) {
            return (void*)(&h[i]);
        }
    }
    return NULL;
}

static inline char* HF_strcpy(char* dest, const char* src, uintptr_t addr) {
    size_t len = strlen(src);
    if (len > 0) {
        uint32_t level = (sizeof(len) * 8) - __builtin_clzl(len);
        instrumentUpdateCmpMap(addr, level);
    }

    return __builtin_memcpy(dest, src, len + 1);
}

/* Define a weak function x, as well as __wrap_x pointing to x */
#define XVAL(x) x
#define HF_WEAK_WRAP(ret, func, ...) \
    _Pragma(HF__XSTR(weak func = __wrap_##func)) XVAL(ret) XVAL(__wrap_##func)(__VA_ARGS__)

/* Typical libc wrappers */
HF_WEAK_WRAP(int, strcmp, const char* s1, const char* s2) {
    return HF_strcmp(s1, s2, (uintptr_t)__builtin_return_address(0));
}
void __sanitizer_weak_hook_strcmp(
    uintptr_t pc, const char* s1, const char* s2, int result HF_ATTR_UNUSED) {
    HF_strcmp(s1, s2, pc);
}
HF_WEAK_WRAP(int, strcasecmp, const char* s1, const char* s2) {
    return HF_strcasecmp(s1, s2, (uintptr_t)__builtin_return_address(0));
}
void __sanitizer_weak_hook_strcasecmp(
    uintptr_t pc, const char* s1, const char* s2, int result HF_ATTR_UNUSED) {
    HF_strcasecmp(s1, s2, pc);
}
HF_WEAK_WRAP(int, strncmp, const char* s1, const char* s2, size_t n) {
    return HF_strncmp(s1, s2, n, (uintptr_t)__builtin_return_address(0));
}
void __sanitizer_weak_hook_strncmp(
    uintptr_t pc, const char* s1, const char* s2, size_t n, int result HF_ATTR_UNUSED) {
    HF_strncmp(s1, s2, n, pc);
}
HF_WEAK_WRAP(int, strncasecmp, const char* s1, const char* s2, size_t n) {
    return HF_strncasecmp(s1, s2, n, (uintptr_t)__builtin_return_address(0));
}
void __sanitizer_weak_hook_strncasecmp(
    uintptr_t pc, const char* s1, const char* s2, size_t n, int result HF_ATTR_UNUSED) {
    HF_strncasecmp(s1, s2, n, pc);
}
HF_WEAK_WRAP(char*, strstr, const char* haystack, const char* needle) {
    return HF_strstr(haystack, needle, (uintptr_t)__builtin_return_address(0));
}
void __sanitizer_weak_hook_strstr(
    uintptr_t pc, const char* haystack, const char* needle, char* result HF_ATTR_UNUSED) {
    HF_strstr(haystack, needle, pc);
}
HF_WEAK_WRAP(char*, strcasestr, const char* haystack, const char* needle) {
    return HF_strcasestr(haystack, needle, (uintptr_t)__builtin_return_address(0));
}
void __sanitizer_weak_hook_strcasestr(
    uintptr_t pc, const char* haystack, const char* needle, char* result HF_ATTR_UNUSED) {
    HF_strcasestr(haystack, needle, pc);
}
HF_WEAK_WRAP(int, memcmp, const void* m1, const void* m2, size_t n) {
    return HF_memcmp(m1, m2, n, (uintptr_t)__builtin_return_address(0));
}
void __sanitizer_weak_hook_memcmp(
    uintptr_t pc, const void* m1, const void* m2, size_t n, int result HF_ATTR_UNUSED) {
    HF_memcmp(m1, m2, n, pc);
}
HF_WEAK_WRAP(int, bcmp, const void* m1, const void* m2, size_t n) {
    return HF_memcmp(m1, m2, n, (uintptr_t)__builtin_return_address(0));
}
void __sanitizer_weak_hook_bcmp(
    uintptr_t pc, const void* m1, const void* m2, size_t n, int result HF_ATTR_UNUSED) {
    HF_memcmp(m1, m2, n, pc);
}
HF_WEAK_WRAP(
    void*, memmem, const void* haystack, size_t haystacklen, const void* needle, size_t needlelen) {
    return HF_memmem(
        haystack, haystacklen, needle, needlelen, (uintptr_t)__builtin_return_address(0));
}
void __sanitizer_weak_hook_memmem(uintptr_t pc, const void* haystack, size_t haystacklen,
    const void* needle, size_t needlelen, void* result HF_ATTR_UNUSED) {
    HF_memmem(haystack, haystacklen, needle, needlelen, pc);
}
HF_WEAK_WRAP(char*, strcpy, char* dest, const char* src) {
    return HF_strcpy(dest, src, (uintptr_t)__builtin_return_address(0));
}
void __sanitizer_weak_hook_strcpy(
    uintptr_t pc, char* dest, const char* src, char* result HF_ATTR_UNUSED) {
    HF_strcpy(dest, src, pc);
}

/*
 * Apache's httpd wrappers
 */
HF_WEAK_WRAP(int, ap_cstr_casecmp, const char* s1, const char* s2) {
    return HF_strcasecmp(s1, s2, (uintptr_t)__builtin_return_address(0));
}

HF_WEAK_WRAP(int, ap_cstr_casecmpn, const char* s1, const char* s2, size_t n) {
    return HF_strncasecmp(s1, s2, n, (uintptr_t)__builtin_return_address(0));
}

HF_WEAK_WRAP(const char*, ap_strcasestr, const char* s1, const char* s2) {
    return HF_strcasestr(s1, s2, (uintptr_t)__builtin_return_address(0));
}

HF_WEAK_WRAP(int, apr_cstr_casecmp, const char* s1, const char* s2) {
    return HF_strcasecmp(s1, s2, (uintptr_t)__builtin_return_address(0));
}

HF_WEAK_WRAP(int, apr_cstr_casecmpn, const char* s1, const char* s2, size_t n) {
    return HF_strncasecmp(s1, s2, n, (uintptr_t)__builtin_return_address(0));
}

/*
 * *SSL wrappers
 */
HF_WEAK_WRAP(int, CRYPTO_memcmp, const void* m1, const void* m2, size_t len) {
    return HF_memcmp(m1, m2, len, (uintptr_t)__builtin_return_address(0));
}

HF_WEAK_WRAP(int, OPENSSL_memcmp, const void* m1, const void* m2, size_t len) {
    return HF_memcmp(m1, m2, len, (uintptr_t)__builtin_return_address(0));
}

HF_WEAK_WRAP(int, OPENSSL_strcasecmp, const char* s1, const char* s2) {
    return HF_strcasecmp(s1, s2, (uintptr_t)__builtin_return_address(0));
}

HF_WEAK_WRAP(int, OPENSSL_strncasecmp, const char* s1, const char* s2, size_t len) {
    return HF_strncasecmp(s1, s2, len, (uintptr_t)__builtin_return_address(0));
}

HF_WEAK_WRAP(int32_t, memcmpct, const void* s1, const void* s2, size_t len) {
    return HF_memcmp(s1, s2, len, (uintptr_t)__builtin_return_address(0));
}

/*
 * libXML wrappers
 */
HF_WEAK_WRAP(int, xmlStrncmp, const char* s1, const char* s2, int len) {
    if (len <= 0) {
        return 0;
    }
    if (s1 == s2) {
        return 0;
    }
    if (s1 == NULL) {
        return -1;
    }
    if (s2 == NULL) {
        return 1;
    }
    return HF_strncmp(s1, s2, (size_t)len, (uintptr_t)__builtin_return_address(0));
}

HF_WEAK_WRAP(int, xmlStrcmp, const char* s1, const char* s2) {
    if (s1 == s2) {
        return 0;
    }
    if (s1 == NULL) {
        return -1;
    }
    if (s2 == NULL) {
        return 1;
    }
    return HF_strcmp(s1, s2, (uintptr_t)__builtin_return_address(0));
}

HF_WEAK_WRAP(int, xmlStrEqual, const char* s1, const char* s2) {
    if (s1 == s2) {
        return 1;
    }
    if (s1 == NULL) {
        return 0;
    }
    if (s2 == NULL) {
        return 0;
    }
    if (HF_strcmp(s1, s2, (uintptr_t)__builtin_return_address(0)) == 0) {
        return 1;
    }
    return 0;
}

HF_WEAK_WRAP(int, xmlStrcasecmp, const char* s1, const char* s2) {
    if (s1 == s2) {
        return 0;
    }
    if (s1 == NULL) {
        return -1;
    }
    if (s2 == NULL) {
        return 1;
    }
    return HF_strcasecmp(s1, s2, (uintptr_t)__builtin_return_address(0));
}

HF_WEAK_WRAP(int, xmlStrncasecmp, const char* s1, const char* s2, int len) {
    if (len <= 0) {
        return 0;
    }
    if (s1 == s2) {
        return 0;
    }
    if (s1 == NULL) {
        return -1;
    }
    if (s2 == NULL) {
        return 1;
    }
    return HF_strncasecmp(s1, s2, (size_t)len, (uintptr_t)__builtin_return_address(0));
}

HF_WEAK_WRAP(const char*, xmlStrstr, const char* haystack, const char* needle) {
    if (haystack == NULL) {
        return NULL;
    }
    if (needle == NULL) {
        return NULL;
    }
    return HF_strstr(haystack, needle, (uintptr_t)__builtin_return_address(0));
}

HF_WEAK_WRAP(const char*, xmlStrcasestr, const char* haystack, const char* needle) {
    if (haystack == NULL) {
        return NULL;
    }
    if (needle == NULL) {
        return NULL;
    }
    return HF_strcasestr(haystack, needle, (uintptr_t)__builtin_return_address(0));
}

/*
 * Samba wrappers
 */
HF_WEAK_WRAP(int, memcmp_const_time, const void* s1, const void* s2, size_t n) {
    return HF_memcmp(s1, s2, n, (uintptr_t)__builtin_return_address(0));
}

HF_WEAK_WRAP(bool, strcsequal, const void* s1, const void* s2) {
    if (s1 == s2) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }
    return (HF_strcmp(s1, s2, (uintptr_t)__builtin_return_address(0)) == 0);
}

/*
 * LittleCMS wrappers
 */
HF_WEAK_WRAP(int, cmsstrcasecmp, const void* s1, const void* s2) {
    return HF_strcasecmp(s1, s2, (uintptr_t)__builtin_return_address(0));
}
