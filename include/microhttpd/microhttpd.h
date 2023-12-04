/*! \copyright 2018 - 2023 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file microhttpd.h
 *  \brief microhttpd External Interface
 */
#ifndef _MICROHTTPD_H
#define _MICROHTTPD_H

#include <stdint.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define HTTP_CONTINUE            100
#define HTTP_OK                  200
#define HTTP_CREATED             201
#define HTTP_ACCEPTED            202
#define HTTP_URI_FOUND           302
#define HTTP_TEMPORARY_REDIRECT  307
#define HTTP_PERMANENT_REDIRECT  308
#define HTTP_BAD_REQUEST         400
#define HTTP_UNAUTHORIZED        401
#define HTTP_FORBIDDEN           403
#define HTTP_NOT_FOUND           404

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

typedef void (*tMicroHttpdPostHandler)(tMicroHttpdClient client, const char *uri, const char *filename,
   const char *param_list[], const uint32_t param_count, const char *source_address, void *cookie,
   bool start, bool finish, const char *data, const uint32_t data_length, const uint32_t total_length);

typedef struct
{
   uint16_t server_port;
   uint32_t process_timeout; /* milliseconds */
   uint32_t rx_buffer_size;

   /* GET */
   tMicroHttpdGetHandlerEntry *get_handler_list;
   uint32_t get_handler_count;
   tMicroHttpdGetHandler default_get_handler;
   void *default_get_handler_cookie;

   /* POST */
   tMicroHttpdPostHandler post_handler;
   void *post_handler_cookie;

} tMicroHttpdParams;

tMicroHttpdContext microhttpd_start(tMicroHttpdParams *params);
int microhttpd_process(tMicroHttpdContext context);

int microhttpd_send_response(tMicroHttpdClient client, uint16_t code, const char *content_type,
   uint32_t content_length, const char *extra_header_options, const char *content);
int microhttpd_send_data(tMicroHttpdClient client, uint32_t length, const char *content);

#if defined(__cplusplus)
}
#endif

#endif /* _MICROHTTPD_H */
