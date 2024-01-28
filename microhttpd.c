/*! \copyright 2018 - 2024 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file microhttpd.c
 *  \brief microhttpd Implementation 
 */
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include "debug.h"
#include "helpers.h"
#include "client.h"
#include "post.h"
#include "microhttpd_private.h"
#include "microhttpd/microhttpd.h"

/* Forward function declarations */
static int microhttpd_CreateListeningSocket(struct md_context *ctx);

static bool state_ParseHeader(struct md_client *client, uint32_t *consumed, bool *error);
static bool state_HeaderComplete(struct md_client *client, uint32_t *consumed, bool *error);
static bool state_HandleOperationGet(struct md_client *client, uint32_t *consumed, bool *error);
static bool state_HandleOperationUnsupported(struct md_client *client, uint32_t *consumed, bool *error);

static const char *RESPONSE_HEADER = "HTTP/1.1 %u\r\nServer: " MICROHTTPD_SERVER_NAME "\r\n"
   "Cache-control: no-cache\r\nPragma: no-cache\r\nAccept-Ranges: bytes\r\nContent-Length: %u\r\n";
static const char *CONTENT_TYPE_FIELD = "Content-Type: %s\r\n";

/* -------------------------------------------------------------------------------------------------
 * Exported Functions
 */

tMicroHttpdContext microhttpd_start(tMicroHttpdParams *params)
{
   struct md_context *ctx;

   if(params->rx_buffer_size == 0)
   {
      MH_DBG("[%s] Invalid receive buffer size\n", __func__);
      return NULL;
   }

   ctx = (struct md_context *) malloc(sizeof(*ctx));
   if(NULL == ctx)
   {
      MH_DBG("[%s] Failed to allocate context structure\n", __func__);
      return NULL; 
   }
   memset(ctx, 0, sizeof(*ctx));
   memcpy(&ctx->params, params, sizeof(ctx->params));

   if(microhttpd_CreateListeningSocket(ctx) != 0)
   {
      MH_DBG("[%s] Failed to create server listening socket on port %u\n",
         __func__, params->server_port);
      return NULL;
   }
   ctx->running = true;

   return (tMicroHttpdContext) ctx;
}

int microhttpd_process(tMicroHttpdContext context)
{
   struct md_context *ctx = (struct md_context *) context;
   int fd_max, nResult, i;
   uint32_t client_count = 0;
   fd_set fdRead;
   fd_set fdError;
   struct md_client *client, **client_list;
   struct timeval timeout, *pTimeout = NULL;

   MH_DBG("[%s]\n", __func__);

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

   /* Make a copy of the client list that was used to populate the upcoming select call. This
    *  is necessary since the client list can change (shrink) as select events are processed. */
   client_list = (struct md_client **) malloc(sizeof(struct md_client *) * client_count);
   client = ctx->client_list;
   for(i = 0; i < client_count; ++i)
   {
      client_list[i] = client;
      client = client->next;
   }

   MH_DBG("[%s] Waiting for %"PRIu32" clients\n", __func__, client_count);

   nResult = select(fd_max + 1, &fdRead, NULL, &fdError, pTimeout);
   if(nResult == 0)
   {
      free(client_list);
      return 0;  /* Nothing received within timeout */
   }
   if(nResult < 0)
   {
      MH_DBG("[%s] select failed (errno %d)\n", __func__, errno);
      /* Go through the list of clients and prune any closed sockets */
      for(i = 0; i < client_count; ++i)
      {
         if(fcntl(client_list[i]->socket, F_GETFD) != 0)
            microhttpd_RemoveClient(ctx, client_list[i]);
      }
      free(client_list);
      return -1;
   }

   /* First, process any data received from clients */
   for(i = 0; i < client_count; ++i)
   {
      client = client_list[i];
      if(FD_ISSET(client->socket, &fdError))
         microhttpd_HandleClientError(ctx, client);
      else if(FD_ISSET(client->socket, &fdRead))
         microhttpd_HandleClientReceive(ctx, client);
   }
   free(client_list);

   /* Finally, accept any new clients */
   if(FD_ISSET(ctx->listen_socket, &fdRead))
   {
      struct sockaddr_in info;
      socklen_t length = sizeof(info);
      int nSocket;

      nSocket = accept(ctx->listen_socket, (struct sockaddr *) &info, &length);
      if(nSocket < 0)
      {
         MH_DBG("[%s] Failed to accept client (%d)\n", __func__, nSocket);
      }
      else
         microhttpd_NewClient(ctx, nSocket, &info);
   }

   return 0; 
}

int microhttpd_send_data(tMicroHttpdClient client, uint32_t length, const char *content)
{
   struct md_client *c = (struct md_client *) client;
   const char *p = content;
   uint32_t sendLength;
   uint32_t totalSent = 0; 
   int32_t result;
   char var[MICROHTTPD_SSI_TAG_MAX_LENGTH];
   bool done;

   if(NULL == content)
      return -1;
   if(length == 0)
      length = strlen(content);
   if(length > MICROHTTPD_MAX_SEND_LENGTH)
   {
      MH_DBG("[%s] Send overflow\n", __func__);
      microhttpd_HandleClientError(c->ctx, client);
      return -1;
   }

   p = content;
   done = false;
   while(!done)
   {
      const char *a = strstr(p, "<!--#echo var=\"");
      if(NULL == a)
      {
         sendLength = strlen(p);
         done = true;
      }
      else
         sendLength = a - p;

      if(sendLength + totalSent > length) 
         sendLength = length - totalSent; 
      if(sendLength > 0)
      {
         result = send(c->socket, p, sendLength, 0);
         if(result != sendLength)
         {
            MH_DBG("[%s] Failed to send %"PRIu32" byte content (%"PRIi32")\n",
               __func__, sendLength, result);
            microhttpd_HandleClientError(c->ctx, client);
            return -1;
         }
      }
      totalSent += sendLength;

      if(done)
         continue;

      a += 15;
      const char *b = strstr(a, "\" -->");
      if(NULL == b)
      {
         MH_DBG("[%s] Unterminated SSI tag\n", __func__);
         microhttpd_HandleClientError(c->ctx, client);
         return -1;
      }
      strncpy(var, a, (b-a > 128) ? 128 : b-a);
      if(c->ctx->params.ssi_handler != NULL)
      {
         MH_DBG("[%s] Processing SSI (var=%s)\n", __func__, var);
         c->ctx->params.ssi_handler(c, var);
      }
      else
      {
         MH_DBG("[%s] SSI tag (var=%s) not implemented\n", __func__, var);
      }
      p = b+5;

      if(p[0] == '\0')
         done = true;
      if(totalSent >= length)
         done = true;
   }

   return 0;
}

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
      length += strlen(CONTENT_TYPE_FIELD) + strlen(content_type);
   tx = malloc(length);
   if(NULL == tx)
   {
      MH_DBG("[%s] Failed to allocate response buffer (%"PRIi32" bytes)\n", __func__, length);
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
      length += sprintf(&tx[length], CONTENT_TYPE_FIELD, content_type);
   length += sprintf(&tx[length], "\r\n");
   result = send(c->socket, tx, length, 0);
   free(tx);
   if(result != length)
   {
      MH_DBG("[%s] Failed to send %"PRIi32" byte header (%"PRIi32")\n", __func__, length, result);
      microhttpd_HandleClientError(c->ctx, client);
      return -1;
   }

   if(content_length > 0 && NULL != content)
      return microhttpd_send_data(client, content_length, content);
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
      MH_DBG("[%s] Failed to create listening socket\n", __func__);
   }
   else
   {
      int enable = 1;
      if(setsockopt(ctx->listen_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
      {
         MH_DBG("[%s] Failed to enable SO_REUSEADDR\n", __func__); /* Don't treat this as a fatal error */
      }

      sinAddress.sin_family = AF_INET;
      sinAddress.sin_addr.s_addr = htonl(INADDR_ANY);
      sinAddress.sin_port = htons(ctx->params.server_port);
      if(bind(ctx->listen_socket, (struct sockaddr*) &sinAddress, sizeof(sinAddress)) < 0)
      {
         MH_DBG("[%s] Error binding on port %u\n", __func__, ctx->params.server_port); 
      }
      else if(fcntl(ctx->listen_socket, F_SETFL, 
         fcntl(ctx->listen_socket, F_GETFL, 0) | O_NONBLOCK) != 0)
      {
         MH_DBG("[%s] Failed to set non-blocking mode on listening socket\n", __func__);
      }
      else if(listen(ctx->listen_socket, MICROHTTPD_MAX_QUEUED_CONNECTIONS))
      {
         MH_DBG("[%s] Failed to listen on server socket\n", __func__);
      }
      else
      {
         MH_DBG("[%s] Server running on port %u\n", __func__, ctx->params.server_port);
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
      MH_DBG("[%s] Header parsing complete (%"PRIu32" entries)\n", __func__, client->header_entry_count);
      client->state = state_HeaderComplete; /* Empty header entry found; header complete */
      *consumed = 2;
      return true;
   }
   MH_DBG("[%s] Found header option (length %"PRIu32")\n", __func__, length);

   if(!string_list_add(client->rx_buffer, length, &client->header_entries,
      &client->header_entry_count))
   {
      MH_DBG("[%s] Failed to allocate header entry list\n", __func__);
      *error = true;
      return false;
   }

   if(client->header_entry_count > 1)
      lower(client->header_entries[client->header_entry_count-1]);

   MH_DBG("[%s] Header option %"PRIu32": '%s'\n", __func__, client->header_entry_count,
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
      MH_DBG("[%s] No header entries\n", __func__);
      *error = true;
      return false;
   }

   /* Split-up the first header line into its three parts */
   offset = client->header_entries[0];
   remaining = strlen(offset);
   client->operation = string_chop(&offset, &remaining, " ", 1);
   MH_DBG("[%s] operation '%s'\n", __func__, client->operation);
   client->uri = string_chop(&offset, &remaining, " ", 1);
   MH_DBG("[%s] uri '%s'\n", __func__, client->uri);
   client->http_version = offset;
   MH_DBG("[%s] http version '%s'\n", __func__, client->http_version);

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
            MH_DBG("[%s] Final URI parameter %"PRIu32" '%s'\n", __func__, client->uri_param_count,
               client->uri_params[client->uri_param_count]);
            ++(client->uri_param_count);
            done = true;
         }
         else
         {
            MH_DBG("[%s] URI parameter %"PRIu32" '%s'\n", __func__, client->uri_param_count,
               client->uri_params[client->uri_param_count]);
            ++(client->uri_param_count);
         }
   
         if(client->uri_param_count >= ARRAY_SIZE(client->uri_params))
            done = true;
      }
      MH_DBG("[%s] Trimmed URI '%s'\n", __func__, client->uri);
   }

   if(memcmp(client->operation, "GET", 3) == 0)
      client->state = state_HandleOperationGet;
   else if(memcmp(client->operation, "POST", 4) == 0)
      client->state = state_HandleOperationPost;
   else
      client->state = state_HandleOperationUnsupported;

   return true;
}

static bool state_HandleOperationGet(struct md_client *client, uint32_t *consumed, bool *error)
{
   struct md_context *ctx = client->ctx;
   uint32_t idx, match_count = 0;

   MH_DBG("[%s] Searching %"PRIu32" GET operations\n", __func__, ctx->params.get_handler_count);
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
      MH_DBG("[%s] No matches found for URI '%s'\n", __func__, client->uri);
      if(ctx->params.default_get_handler != NULL)
      {
         MH_DBG("[%s] Calling default GET handler\n", __func__);
         ctx->params.default_get_handler((tMicroHttpdClient) client, client->uri,
            (const char **) client->uri_params, client->uri_param_count,
            client->source_address, ctx->params.default_get_handler_cookie);
      }
   }

   MH_DBG("[%s] GET finished\n", __func__);
   microhttpd_ResetState(client);
   return true;
}

static bool state_HandleOperationUnsupported(struct md_client *client, uint32_t *consumed, bool *error)
{
   MH_DBG("[%s] Unsupported HTTP operation '%s'\n", __func__, client->operation);
   microhttpd_ResetState(client);
   return true;
}

