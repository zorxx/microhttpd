/*! \copyright 2018 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file microhttpd_private.h
 *  \brief Private (non-exported) definitions for microhttpd
 */
#ifndef _MICROHTTPD_PRIVATE_H
#define _MICROHTTPD_PRIVATE_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include "microhttpd.h"

#define MICROHTTPD_SERVER_NAME               "microhttpd"
#define MICROHTTPD_MAX_SOURCE_ADDRESS_LENGTH 30
#define MICROHTTPD_MAX_QUEUED_CONNECTIONS    5
#define MICROHTTPD_MAX_HTTP_HEADER_OPTIONS   20
#define MICROHTTPD_MAX_HTTP_URI_PARAMS       20

#define MAX(x, y) (x) > (y) ? (x) : (y)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef enum
{
   MD_OP_GET,
   MD_OP_POST
} md_operation_t;

typedef struct
{
   int socket;
   struct sockaddr_in socket_info;
   char source_address[MICROHTTPD_MAX_SOURCE_ADDRESS_LENGTH];
   char *rx_buffer;
   uint32_t rx_buffer_size;
   uint32_t rx_size;

   /* Parsed request */
   char *operation, *uri, *http_version;
   char *options[MICROHTTPD_MAX_HTTP_HEADER_OPTIONS];
   uint32_t option_count;
   char *uri_params[MICROHTTPD_MAX_HTTP_URI_PARAMS];
   uint32_t uri_param_count;

   /* Linked list */
   void *prev;
   void *next;
} md_client_t;

typedef struct
{
   tMicroHttpdParams params;
   bool running;
   int listen_socket;
   md_client_t *client_list;
} md_context_t;

#endif /* _MICROHTTPD_PRIVATE_H */
