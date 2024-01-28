/*! \copyright 2018 - 2023 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file helpers.c
 *  \brief microhttpd helpers
 */
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <ctype.h>
#include "debug.h"
#include "helpers.h"

char *lower(char* s)
{
   char *tmp;
   for(tmp = s; *tmp != '\0'; ++tmp)
      *tmp = tolower((unsigned char) *tmp);
   return s;
}

char *string_find(char *string, uint32_t string_length, char *delimiter,
   uint32_t delimiter_length)
{
   char *cur = string, *end = string + string_length; 
   uint32_t match = 0;

   while(cur < end)
   {
      if(*cur == delimiter[match])
         ++match;

      if(match >= delimiter_length)
         return cur - (match - 1);
     
      ++cur;
   }

   return NULL;
}

void string_shift(char *string, uint32_t shift, uint32_t length)
{
   uint32_t offset;

   MH_ASSERT(length >= shift);

   for(offset = 0; offset < (length - shift); ++offset)
   {
      if((offset + shift) >= length)
         string[offset] = '\0';
      else
         string[offset] = string[offset + shift];
   }
}

char *string_chop(char **string, uint32_t *string_length, char *delimiter,
   uint32_t delimiter_length)
{
   char *start;
   uint32_t offset = 0;

   MH_ASSERT(NULL != string_length);
   MH_ASSERT(NULL != string);
   MH_ASSERT(NULL != delimiter);
   MH_ASSERT(NULL != *string);
   MH_ASSERT(*string_length > 0);
   MH_ASSERT(delimiter_length > 0);

   start = *string;

   while(offset < delimiter_length && *string_length > 0)
   {
      if (**string == delimiter[offset])
         ++offset;
      (*string)++;
      --(*string_length);
   }
 
   if(*string_length > 0)
   {
      *((*string) - delimiter_length) = '\0';
      return start;
   }

   *string = start;
   return NULL;
}

bool string_list_add(char *string, uint32_t string_length, char ***string_list, uint32_t *list_size)
{
   uint32_t idx = *list_size;

   *string_list = realloc(*string_list, (idx + 1) * sizeof(char *));
   if(NULL == *string_list)
      return false;

   (*string_list)[idx] = malloc(string_length + 1);
   if((*string_list)[idx] == NULL)
      return false;
   memcpy((*string_list)[idx], string, string_length);
   ((*string_list)[idx])[string_length] = '\0';
   
   ++(*list_size);
   return true;
}

void string_list_clear(char ***string_list, uint32_t *list_size)
{
   uint32_t idx;

   MH_ASSERT(NULL != list_size);
   MH_ASSERT(NULL != string_list);

   if(*string_list == NULL)
      return;

   MH_DBG("[%s] Clearing %"PRIu32" strings\n", __func__, *list_size);

   for(idx = 0; idx < *list_size; ++idx)
      free((*string_list)[idx]);
   free(*string_list);
   
   *string_list = NULL;
   *list_size = 0;
}
