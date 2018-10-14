/*! \copyright 2018 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file microhttpd.c
 *  \brief microhttpd Implementation 
 */
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "debug.h"
#include "microhttpd_private.h"
#include "microhttpd.h"

// Forward function declarations
static int microhttpd_CreateListeningSocket(md_context_t *ctx);
static int microhttpd_NewClient(md_context_t *ctx, int nSocket, struct sockaddr_in *socket_info);
static int microhttpd_RemoveClient(md_context_t *ctx, md_client_t *client);
static int microhttpd_HandleClientReceive(md_context_t *ctx, md_client_t *client);
static int microhttpd_HandleClientError(md_context_t *ctx, md_client_t *client);

static char *http_header_chop(char **header, uint32_t *header_length, char *delimiter, uint32_t delimiter_length);
static int microhttpd_HandleReceivedHeader(md_context_t *ctx, md_client_t *client, uint32_t header_length);
static int microhttpd_HandleGetOperation(md_context_t *ctx, md_client_t *client);
static int microhttpd_HandlePostOperation(md_context_t *ctx, md_client_t *client);

/* -------------------------------------------------------------------------------------------------
 * Exported Functions
 */

tMicroHttpdContext microhttpd_start(tMicroHttpdParams *params)
{
   md_context_t *ctx;

   if(params->rx_buffer_size == 0)
   {
      DBG("%s: Invalid receive buffer size\n", __func__);
      return NULL;
   }

   ctx = (md_context_t *) malloc(sizeof(*ctx));
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
   md_context_t *ctx = (md_context_t *) context;
   int fd_max, nResult;
   uint32_t client_count = 0;
   fd_set fdRead;
   fd_set fdError;
   md_client_t *client;
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
   for(client = ctx->client_list; client != NULL; client = (md_client_t *) client->next)
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
   for(client = ctx->client_list; client != NULL; client =(md_client_t *) client->next)
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

int microhttpd_send_response(tMicroHttpdClient client, uint16_t code, const char *contet_type,
   uint32_t content_length, const char *extra_header_options, const char *content)
{
   md_client_t *c = (md_client_t *) client;
   char *tx;
   ssize_t length, result;

   length = strlen(RESPONSE_HEADER) + 20;
   if(NULL != extra_header_options)
      length += strlen(extra_header_options);
   tx = malloc(length);
   if(NULL == tx)
   {
      DBG("%s: Failed to allocate response buffer (%ld bytes)\n", __func__, length);
      return -1;
   }
   
   /* Build and send header */
   length = sprintf(tx, RESPONSE_HEADER, code, content_length);
   if(NULL != extra_header_options)
   {
      strcpy(&tx[length], extra_header_options); 
      length += strlen(extra_header_options);
   }
   strcpy(&tx[length], "\r\n");
   length += 2;
   result = send(c->socket, tx, length, 0);
   if(result != length)
   {
      DBG("%s: Failed to send %ld byte header (%ld)\n", __func__, length, result);
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
         DBG("%s: Failed to send %u byte content (%ld)\n", __func__, content_length, result);
         /* TODO: close connection? */
         return -1;
      }
   }

   return 0;
}

/* -------------------------------------------------------------------------------------------------
 * Private Helper Functions
 */

static int microhttpd_CreateListeningSocket(md_context_t *ctx)
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

static int microhttpd_NewClient(md_context_t *ctx, int nSocket, struct sockaddr_in *socket_info)
{
   md_client_t *client;
   uint8_t *addr = (uint8_t *) &socket_info->sin_addr.s_addr;
   uint16_t port = ntohs(socket_info->sin_port);

   client = (md_client_t *) malloc(sizeof(*client));
   if(NULL == client)
      return -1;
   snprintf(client->source_address, sizeof(client->source_address) - 1, 
      "%u.%u.%u.%u:%u", addr[0], addr[1], addr[2], addr[3], port);
   DBG("%s: New client connected from %s\n", __func__, client->source_address);

   client->socket = nSocket;
   memcpy(&client->socket_info, socket_info, sizeof(client->socket_info));
   client->rx_buffer_size = ctx->params.rx_buffer_size;
   client->rx_buffer = malloc(client->rx_buffer_size);
   if(client->rx_buffer == NULL)
   {
      DBG("%s: Failed to allocate receive buffer\n", __func__);
      free(client);
      return -1;
   }

   client->next = ctx->client_list;
   if(ctx->client_list != NULL)
      client->prev = ctx->client_list->prev;
   else
   {
      ctx->client_list = client;
      client->prev = NULL;
   }

   return 0;
}

static int microhttpd_RemoveClient(md_context_t *ctx, md_client_t *client)
{
   close(client->socket);

   if(client->prev != NULL)
      ((md_client_t *) client->prev)->next = client->next;
   else
      ctx->client_list = client->next; /* Remove list head */
   if(client->next != NULL)
      ((md_client_t *) client->next)->prev = client->prev;

   free(client->rx_buffer);
   free(client);
   return 0;
}

static int microhttpd_HandleClientReceive(md_context_t *ctx, md_client_t *client)
{
   ssize_t space_left = client->rx_buffer_size - client->rx_size;
   ssize_t length;
   char *offset;

   if(space_left <= 0)
   {
      DBG("%s: Invalid space remaining (%ld)\n", __func__, space_left);
      return microhttpd_RemoveClient(ctx, client);
   }
   length = read(client->socket, &client->rx_buffer[client->rx_size], space_left); 
   if(length <= 0)
   {
      DBG("%s: Read failed (%ld)\n", __func__, length);
      return microhttpd_RemoveClient(ctx, client);
   }
   client->rx_size += length;

   offset = strstr((char *)client->rx_buffer, "\r\n\r\n");
   if(offset != NULL)
      microhttpd_HandleReceivedHeader(ctx, client, offset - client->rx_buffer + 4);
   else
   {
      DBG("%s: Partial header '%s'\n", __func__, client->rx_buffer);
   }

   return 0;
}

static int microhttpd_HandleClientError(md_context_t *ctx, md_client_t *client)
{
   DBG("%s: Socket error\n", __func__);
   return microhttpd_RemoveClient(ctx, client);
}

/* -------------------------------------------------------------------------------------------------
 * HTTP Operations 
 */

static char *http_header_chop(char **header, uint32_t *header_length,
   char *delimiter, uint32_t delimiter_length)
{
   char *start = *header;
   uint32_t offset = 0;

   ASSERT(header_length > 0);
   ASSERT(delimiter_length > 0);
   ASSERT(*header != NULL);
   ASSERT(delimiter != NULL);

   while(offset < delimiter_length && *header_length > 0)
   {
      if (**header == delimiter[offset])
         ++offset;
      (*header)++;
      --(*header_length);
   }
 
   if(*header_length > 0)
   {
      *((*header) - delimiter_length) = '\0';
      return start;
   }

   return NULL;
}

static int microhttpd_HandleReceivedHeader(md_context_t *ctx, md_client_t *client, uint32_t header_length)
{
   char *offset = client->rx_buffer;
   uint32_t remaining = header_length;
   bool done;

   DBG("%s: Header length %u (buffer total %u)\n", __func__, header_length, client->rx_size);

   /* Parse the operaton, URI and HTTP version header fields */
   client->operation = http_header_chop(&offset, &remaining, " ", 1);
   client->uri = http_header_chop(&offset, &remaining, " ", 1);
   client->http_version = http_header_chop(&offset, &remaining, "\r\n", 2);
   DBG("%s: operation '%s', uri '%s', version '%s'\n", __func__,
      client->operation, client->uri, client->http_version);

   /* Parse all header options */
   done = false;
   client->option_count = 0;
   while(!done)
   {
      client->options[client->option_count] = http_header_chop(&offset, &remaining, "\r\n", 2);
      if(client->options[client->option_count] == NULL
      || strlen(client->options[client->option_count]) == 0)
         done = true;
      else
      {
         DBG("%s: option %u '%s'\n", __func__, client->option_count,
            client->options[client->option_count]);
         ++(client->option_count);
      }
   
      if(client->option_count >= ARRAY_SIZE(client->options))
         done = true;
   }

   /* Parse parameters in the URI */
   offset = strchr(client->uri, '?');
   if(NULL != offset)
   {
      *offset = '\0';
      ++offset;
      remaining = strlen(offset);
      done = false;
      client->uri_param_count = 0;
      while(!done)
      {
         client->uri_params[client->uri_param_count] = http_header_chop(&offset, &remaining, "&", 1);
         if(client->uri_params[client->uri_param_count] == NULL
         || strlen(client->uri_params[client->uri_param_count]) == 0)
            done = true;
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
      microhttpd_HandleGetOperation(ctx, client); 
   else if(memcmp(client->operation, "POST", 4) == 0)
      microhttpd_HandlePostOperation(ctx, client); 
   else
   {
      DBG("%s: Unsupported HTTP operation '%s'\n", __func__, client->operation);
   }

   if(header_length != client->rx_size)
   {
      DBG("%s: %d leftover bytes in receive buffer\n", __func__, client->rx_size - header_length);
      /* TODO: shift buffer */
   }
   else
      client->rx_size = 0;   
   return 0;
}

static int microhttpd_HandleGetOperation(md_context_t *ctx, md_client_t *client)
{
   uint32_t idx, match_count = 0; 

   DBG("%s: Searching %u GET operations\n", __func__, ctx->params.get_handler_count);
   for(idx = 0; idx < ctx->params.get_handler_count; ++idx)
   {
      tMicroHttpdGetHandlerEntry *entry = &ctx->params.get_handler_list[idx];
      
      if(strcmp(client->uri, entry->uri) == 0)
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

   return 0;
}

static int microhttpd_HandlePostOperation(md_context_t *ctx, md_client_t *client)
{
   /* TODO */
   return 0;
}
