/*! \copyright 2018 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file microhttpd_private.h
 *  \brief Private (non-exported) definitions for microhttpd
 */
#ifndef _MICROHTTPD_PRIVATE_H
#define _MICROHTTPD_PRIVATE_H

#include <stdint.h>
#include <stdbool.h>
#if defined(LWIP_SOCKET)
#include <lwip/sockets.h>
#else
#include <sys/socket.h>
#endif
#if !defined(MICROHTTPD_NO_NETINET_IN_H)
#include <netinet/in.h>
#endif
#include "microhttpd/microhttpd.h"

#define MICROHTTPD_SERVER_NAME               "microhttpd"
#define MICROHTTPD_MAX_SOURCE_ADDRESS_LENGTH 30
#define MICROHTTPD_MAX_QUEUED_CONNECTIONS    10
#define MICROHTTPD_MAX_HTTP_HEADER_OPTIONS   20
#define MICROHTTPD_MAX_HTTP_URI_PARAMS       20

struct md_client;
struct md_context;

typedef bool (*md_state_machine_function)(struct md_client *client, uint32_t *consumed, bool *error);

struct md_client
{
   struct md_context *ctx;

   int socket;
   struct sockaddr_in socket_info;
   char source_address[MICROHTTPD_MAX_SOURCE_ADDRESS_LENGTH];

   md_state_machine_function state;

   char *rx_buffer;
   uint32_t rx_buffer_size;
   uint32_t rx_size;

   /* HTTP Header */
   char **header_entries;
   uint32_t header_entry_count;
   char *operation, *uri, *http_version;
   char *uri_params[MICROHTTPD_MAX_HTTP_URI_PARAMS];
   uint32_t uri_param_count;

   /* POST */
   char *filename;
   char *post_boundary;
   char **post_header_entries;
   uint32_t post_header_entry_count;
   uint32_t content_length;
   uint32_t content_remaining;
   uint32_t post_header_length;
   uint32_t post_trailer_length;

   /* Linked list */
   struct md_client *next;
};

struct md_context
{
   tMicroHttpdParams params;
   bool running;
   int listen_socket;
   struct md_client *client_list;
};

void microhttpd_ResetState(struct md_client *client);

#endif /* _MICROHTTPD_PRIVATE_H */
