/*! \copyright 2018 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file microhttpd.c
 *  \brief microhttpd Implementation 
 */
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include "debug.h"
#include "helpers.h"
#include "client.h"
#include "post.h"
#include "microhttpd_private.h"
#include "microhttpd.h"

// Forward function declarations
static int microhttpd_CreateListeningSocket(struct md_context *ctx);

static bool state_ParseHeader(struct md_client *client, uint32_t *consumed, bool *error);
static bool state_HeaderComplete(struct md_client *client, uint32_t *consumed, bool *error);
static bool state_HandleOperationGet(struct md_client *client, uint32_t *consumed, bool *error);
static bool state_HandleOperationUnsupported(struct md_client *client, uint32_t *consumed, bool *error);

/* -------------------------------------------------------------------------------------------------
 * Exported Functions
 */

tMicroHttpdContext microhttpd_start(tMicroHttpdParams *params)
{
   struct md_context *ctx;

   if(params->rx_buffer_size == 0)
   {
      DBG("%s: Invalid receive buffer size\n", __func__);
      return NULL;
   }

   ctx = (struct md_context *) malloc(sizeof(*ctx));
   if(NULL == ctx)
   {
      DBG("%s: Failed to allocate context structure\n", __func__);
      return NULL; 
   }
   memset(ctx, 0, sizeof(*ctx));
   memcpy(&ctx->params, params, sizeof(ctx->params));

   if(microhttpd_CreateListeningSocket(ctx) != 0)
   {
      DBG("%s: Failed to create server listening socket on port %u\n",
         __func__, params->server_port);
      return NULL;
   }
   ctx->running = true;

   return (tMicroHttpdContext) ctx;
}

int microhttpd_process(tMicroHttpdContext context)
{
   struct md_context *ctx = (struct md_context *) context;
   int fd_max, nResult;
   uint32_t client_count = 0;
   fd_set fdRead;
   fd_set fdError;
   struct md_client *client;
   struct timeval timeout, *pTimeout = NULL;

   DBG("%s\n", __func__);

   if(!ctx->running)
     return -1;

   if(ctx->params.process_timeout > 0)
   {
      timeout.tv_sec = ctx->params.process_timeout / 1000;
      timeout.tv_usec = (ctx->params.process_timeout % 1000) * 1000;
      pTimeout = &timeout;
   }
   
   FD_ZERO(&fdRead);
   FD_SET(ctx->listen_socket, &fdRead);
   FD_SET(ctx->listen_socket, &fdError);
   fd_max = ctx->listen_socket;
   for(client = ctx->client_list; client != NULL; client = (struct md_client *) client->next)
   {
      fd_max = MAX(fd_max, client->socket);
      FD_SET(client->socket, &fdRead);
      FD_SET(client->socket, &fdError);
      ++client_count;
   }

   DBG("%s: Waiting for %u clients\n", __func__, client_count);

   nResult = select(fd_max + 1, &fdRead, NULL, &fdError, pTimeout);
   if(nResult == 0)
      return 0;  /* Nothing received within timeout */
   if(nResult < 0)
   {
      DBG("%s: select failed (%d)\n", __func__, nResult); 
      return -1;
   }

   /* First, process any data received from clients */
   for(client = ctx->client_list; client != NULL; client =(struct md_client *) client->next)
   {
      if(FD_ISSET(client->socket, &fdError))
         microhttpd_HandleClientError(ctx, client);
      else if(FD_ISSET(client->socket, &fdRead))
         microhttpd_HandleClientReceive(ctx, client);
   }

   /* Finally, accept any new clients */
   if(FD_ISSET(ctx->listen_socket, &fdRead))
   {
      struct sockaddr_in info;
      socklen_t length = sizeof(info);
      int nSocket;

      nSocket = accept(ctx->listen_socket, (struct sockaddr *) &info, &length);
      if(nSocket < 0)
      {
         DBG("%s: Failed to accept client (%d)\n", __func__, nSocket);
      }
      else
         microhttpd_NewClient(ctx, nSocket, &info);
   }

   return 0; 
}

static const char *RESPONSE_HEADER = "HTTP/1.1 %u\r\nServer: " MICROHTTPD_SERVER_NAME "\r\n"
   "Cache-control: no-cache\r\nPragma: no-cache\r\nAccept-Ranges: bytes\r\nContent-Length: %u\r\n";

int microhttpd_send_response(tMicroHttpdClient client, uint16_t code, const char *content_type,
   uint32_t content_length, const char *extra_header_options, const char *content)
{
   struct md_client *c = (struct md_client *) client;
   char *tx;
   int32_t length, result;

   length = strlen(RESPONSE_HEADER) + 20;
   if(NULL != extra_header_options)
      length += strlen(extra_header_options);
   if(content_type != NULL)
      length += strlen(content_type) + 20;
   tx = malloc(length);
   if(NULL == tx)
   {
      DBG("%s: Failed to allocate response buffer (%d bytes)\n", __func__, length);
      return -1;
   }
   
   /* Build and send header */
   length = sprintf(tx, RESPONSE_HEADER, code, content_length);
   if(NULL != extra_header_options)
   {
      strcpy(&tx[length], extra_header_options); 
      length += strlen(extra_header_options);
   }
   if(content_type != NULL)
      length += sprintf(&tx[length], "Content-Type: %s\r\n", content_type);
   strcpy(&tx[length], "\r\n");
   length += 2;
   result = send(c->socket, tx, length, 0);
   if(result != length)
   {
      DBG("%s: Failed to send %d byte header (%d)\n", __func__, length, result);
      free(tx);
      /* TODO: close connection? */
      return -1;
   }
   free(tx);

   /* Send any content */
   if(content_length > 0 && NULL != content)
   {
      result = send(c->socket, content, content_length, 0);
      if(result != content_length)
      {
         DBG("%s: Failed to send %u byte content (%d)\n", __func__, content_length, result);
         /* TODO: close connection? */
         return -1;
      }
   }

   return 0;
}

/* -------------------------------------------------------------------------------------------------
 * Common Functions
 */

void microhttpd_ResetState(struct md_client *client)
{
   string_list_clear(&client->header_entries, &client->header_entry_count);
   string_list_clear(&client->post_header_entries, &client->post_header_entry_count);
   client->state = state_ParseHeader;
}

/* -------------------------------------------------------------------------------------------------
 * Private Helper Functions
 */

static int microhttpd_CreateListeningSocket(struct md_context *ctx)
{
   struct sockaddr_in sinAddress = {0};
   int result = -1;

   ctx->listen_socket = socket(AF_INET, SOCK_STREAM, 0);
   if(ctx->listen_socket < 0)
   {
      DBG("%s: Failed to create listening socket\n", __func__);
   }
   else
   {
      int enable = 1;
      if(setsockopt(ctx->listen_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
      {
         DBG("%s: Failed to enable SO_REUSEADDR\n", __func__); /* Don't treat this as a fatal error */
      }

      sinAddress.sin_family = AF_INET;
      sinAddress.sin_addr.s_addr = htonl(INADDR_ANY);
      sinAddress.sin_port = htons(ctx->params.server_port);
      if(bind(ctx->listen_socket, (struct sockaddr*) &sinAddress, sizeof(sinAddress)) < 0)
      {
         DBG("%s: Error binding on port %u\n", __func__, ctx->params.server_port); 
      }
      else if(fcntl(ctx->listen_socket, F_SETFL, 
         fcntl(ctx->listen_socket, F_GETFL, 0) | O_NONBLOCK) != 0)
      {
         DBG("%s: Failed to set non-blocking mode on listening socket\n", __func__);
      }
      else if(listen(ctx->listen_socket, MICROHTTPD_MAX_QUEUED_CONNECTIONS))
      {
         DBG("%s: Failed to listen on server socket\n", __func__);
      }
      else
      {
         DBG("%s: Server running on port %u\n", __func__, ctx->params.server_port);
         result = 0;
      }

      if(result != 0)
      {
         close(ctx->listen_socket);
         ctx->listen_socket = -1;
      }
   }

   return result;
}

/* -------------------------------------------------------------------------------------------------
 * States 
 */

static bool state_ParseHeader(struct md_client *client, uint32_t *consumed, bool *error)
{
   uint32_t length;
   char *offset;

   offset = string_find(client->rx_buffer, client->rx_size, "\r\n", 2);
   if(offset == NULL)
      return false;  /* Header entry delimiter not found; need more rx data */

   length = offset - client->rx_buffer;
   if(0 == length)
   {
      DBG("%s: Header parsing complete (%u entries)\n", __func__, client->header_entry_count);
      client->state = state_HeaderComplete; /* Empty header entry found; header complete */
      *consumed = 2;
      return true;
   }
   DBG("%s: Found header option (length %u)\n", __func__, length);

   if(!string_list_add(client->rx_buffer, length, &client->header_entries,
      &client->header_entry_count))
   {
      DBG("%s: Failed to allocate header entry list\n", __func__);
      *error = true;
      return false;
   }

   if(client->header_entry_count > 1)
      lower(client->header_entries[client->header_entry_count-1]);

   DBG("%s: Header option %u: '%s'\n", __func__, client->header_entry_count,
      client->header_entries[client->header_entry_count-1]);

   *consumed = length + 2;
   return true;
}

static bool state_HeaderComplete(struct md_client *client, uint32_t *consumed, bool *error)
{
   char *offset;
   uint32_t remaining;

   if(client->header_entry_count == 0)
   {
      DBG("%s: No header entries\n", __func__);
      *error = true;
      return false;
   }

   /* Split-up the first header line into its three parts */
   offset = client->header_entries[0];
   remaining = strlen(offset);
   client->operation = string_chop(&offset, &remaining, " ", 1);
   DBG("%s: operation '%s'\n", __func__, client->operation);
   client->uri = string_chop(&offset, &remaining, " ", 1);
   DBG("%s: uri '%s'\n", __func__, client->uri);
   client->http_version = offset;
   DBG("%s: http version '%s'\n", __func__, client->http_version);

   /* Parse parameters in the URI */
   offset = strchr(client->uri, '?');
   if(NULL != offset)
   {
      bool done = false;

      *offset = '\0';
      ++offset;
      remaining = strlen(offset);
      client->uri_param_count = 0;
      while(!done)
      {
         client->uri_params[client->uri_param_count] = string_chop(&offset, &remaining, "&", 1);
         if(client->uri_params[client->uri_param_count] == NULL
         || strlen(client->uri_params[client->uri_param_count]) == 0)
         {
            client->uri_params[client->uri_param_count] = offset;
            DBG("%s: Final URI parameter %u '%s'\n", __func__, client->uri_param_count,
               client->uri_params[client->uri_param_count]);
            ++(client->uri_param_count);
            done = true;
         }
         else
         {
            DBG("%s: URI parameter %u '%s'\n", __func__, client->uri_param_count,
               client->uri_params[client->uri_param_count]);
            ++(client->uri_param_count);
         }
   
         if(client->uri_param_count >= ARRAY_SIZE(client->uri_params))
            done = true;
      }
      DBG("%s: Trimmed URI '%s'\n", __func__, client->uri);
   }

   if(memcmp(client->operation, "GET", 3) == 0)
      client->state = state_HandleOperationGet;
   else if(memcmp(client->operation, "POST", 4) == 0)
      client->state = state_HandleOperationPost;
   else
   {
      DBG("%s: Unsupported HTTP operation '%s'\n", __func__, client->operation);
      client->state = state_HandleOperationUnsupported;
   }

   return true;
}

static bool state_HandleOperationGet(struct md_client *client, uint32_t *consumed, bool *error)
{
   struct md_context *ctx = client->ctx;
   uint32_t idx, match_count = 0;

   DBG("%s: Searching %u GET operations\n", __func__, ctx->params.get_handler_count);
   for(idx = 0; idx < ctx->params.get_handler_count; ++idx)
   {
      tMicroHttpdGetHandlerEntry *entry = &ctx->params.get_handler_list[idx];

      if(memcmp(entry->uri, client->uri, strlen(entry->uri)) == 0)
      {
         entry->handler((tMicroHttpdClient) client, client->uri,
            (const char **) client->uri_params, client->uri_param_count,
            client->source_address, entry->cookie);
         ++match_count;
      }
   }

   if(0 == match_count)
   {
      DBG("%s: No matches found for URI '%s'\n", __func__, client->uri);
      if(ctx->params.default_get_handler != NULL)
      {
         DBG("%s: Calling default GET handler\n", __func__);
         ctx->params.default_get_handler((tMicroHttpdClient) client, client->uri,
            (const char **) client->uri_params, client->uri_param_count,
            client->source_address, ctx->params.default_get_handler_cookie);
      }
   }

   DBG("%s: GET finished\n", __func__);
   microhttpd_ResetState(client);

   return true;
}

static bool state_HandleOperationUnsupported(struct md_client *client, uint32_t *consumed, bool *error)
{
   DBG("%s: TODO\n", __func__);
   microhttpd_ResetState(client);
   return true;
}

