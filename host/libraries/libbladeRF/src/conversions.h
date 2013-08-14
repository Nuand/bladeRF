#ifndef CONVERSIONS_H__
#define CONVERSIONS_H__

unsigned int str2uint(const char *str, unsigned int min, unsigned int max, bool *ok);
unsigned int str2uint64(const char *str, uint64_t min, uint64_t max, bool *ok);

#endif
