/*! \copyright 2018 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file microhttpd.h
 *  \brief microhttpd External Interface
 */
#ifndef _MICROHTTPD_H
#define _MICROHTTPD_H

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef void *tMicroHttpdContext;
typedef void *tMicroHttpdClient;

typedef void (*tMicroHttpdGetHandler)(tMicroHttpdClient client, const char *uri,
   const char *param_list[], const uint32_t param_count, const char *source_address, void *cookie);
typedef struct
{
   const char *uri;
   tMicroHttpdGetHandler handler;
   void *cookie;
} tMicroHttpdGetHandlerEntry;

typedef void (*tMicroHttpdPostHandler)(tMicroHttpdClient client, const char *uri,
   const char *param_list[], const uint32_t param_count, const char *data,
   const char *source_address, void *cookie);
typedef struct
{
   const char *uri;
   tMicroHttpdPostHandler handler;
   void *cookie;
} tMicroHttpdPostHandlerEntry;

typedef struct
{
   uint16_t server_port;
   uint32_t process_timeout; /* milliseconds */
   uint32_t rx_buffer_size;

   tMicroHttpdGetHandlerEntry *get_handler_list;
   uint32_t get_handler_count;
   tMicroHttpdGetHandler default_get_handler;
   void *default_get_handler_cookie;

   tMicroHttpdPostHandlerEntry *post_handler_list;
   uint32_t post_handler_count;
   tMicroHttpdPostHandler default_post_handler;
   void *default_post_handler_cookie;

} tMicroHttpdParams;

tMicroHttpdContext microhttpd_start(tMicroHttpdParams *params);
int microhttpd_process(tMicroHttpdContext context);
int microhttpd_send_response(tMicroHttpdClient client, uint16_t code, const char *contet_type,
   uint32_t content_length, const char *extra_header_options, const char *content);

#if defined(__cplusplus)
}
#endif

#endif /* _MICROHTTPD_H */
