

#ifndef TEAVPN2__GLOBAL__HELPERS__ARENA_H
#define TEAVPN2__GLOBAL__HELPERS__ARENA_H

void ar_init(char *ar, size_t ar_size);
size_t ar_unused_size();
void *ar_alloc(size_t len);
void *ar_strdup(const char *str);
void *ar_strndup(const char *str, size_t inlen);

#endif /* #ifndef TEAVPN2__GLOBAL__HELPERS__ARENA_H */
