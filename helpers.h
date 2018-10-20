/*! \copyright 2018 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file helpers.h
 *  \brief microhttpd helper interface
 */
#ifndef MICROHTTPD_HELPERS_H
#define MICROHTTPD_HELPERS_H

#include <stdint.h>
#include <stdbool.h>

#define MAX(x, y) (x) > (y) ? (x) : (y)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

char *lower(char* s);
char *string_find(char *string, uint32_t string_length, char *delimiter,
   uint32_t delimiter_length);
void string_shift(char *string, uint32_t shift, uint32_t length);
char *string_chop(char **string, uint32_t *string_length, char *delimiter,
   uint32_t delimiter_length);

bool string_list_add(char *string, uint32_t string_length, char ***string_list,
   uint32_t *list_size);
void string_list_clear(char ***string_list, uint32_t *list_size);

#endif /* MICROHTTPD_HELPERS_H */
