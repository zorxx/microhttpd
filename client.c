/*! \copyright 2018 - 2024 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file client.c
 *  \brief microhttpd client implementation
 */
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "debug.h"
#include "helpers.h"
#include "client.h"

int microhttpd_NewClient(struct md_context *ctx, int nSocket, struct sockaddr_in *socket_info)
{
   struct md_client *client;
   uint8_t *addr = (uint8_t *) &socket_info->sin_addr.s_addr;
   uint16_t port = ntohs(socket_info->sin_port);

   client = (struct md_client *) malloc(sizeof(*client));
   if(NULL == client)
      return -1;
   memset(client, 0, sizeof(*client));
   snprintf(client->source_address, sizeof(client->source_address) - 1, 
      "%u.%u.%u.%u:%u", addr[0], addr[1], addr[2], addr[3], port);
   MH_DBG("[%s] New client connected from %s\n", __func__, client->source_address);

   client->socket = nSocket;
   memcpy(&client->socket_info, socket_info, sizeof(client->socket_info));
   client->rx_buffer_size = ctx->params.rx_buffer_size;
   client->rx_buffer = malloc(client->rx_buffer_size);
   if(client->rx_buffer == NULL)
   {
      MH_DBG("[%s] Failed to allocate receive buffer\n", __func__);
      free(client);
      return -1;
   }

   client->ctx = ctx;
   microhttpd_ResetState(client);

   client->next = ctx->client_list; /* Always add to the head of the list */
   ctx->client_list = client;

   return 0;
}

int microhttpd_RemoveClient(struct md_context *ctx, struct md_client *client)
{
   struct md_client *cur, *prev;
   int found = 0;

   close(client->socket);

   for(prev = NULL, cur = ctx->client_list; !found && cur != NULL; prev = cur, cur = cur->next)
   {
      if(cur == client)
      {
         if(prev != NULL)
            prev->next = cur->next;
         else
            ctx->client_list = cur->next;
         found = 1;
      }
   }

   if(!found)
   {
      MH_DBG("[%s] Client not found in list\n", __func__);
      return -1;
   }
   MH_DBG("[%s] Client removed\n", __func__);

   microhttpd_ResetState(client);
   free(client->rx_buffer);
   free(client);
   return 0;
}

int microhttpd_HandleClientReceive(struct md_context *ctx, struct md_client *client)
{
   int32_t space_left = client->rx_buffer_size - client->rx_size;
   int32_t length;
   uint32_t consumed;
   bool error, cont;

   if(space_left <= 0)
   {
      MH_DBG("[%s] Invalid space remaining (%"PRIi32")\n", __func__, space_left);
      return microhttpd_RemoveClient(ctx, client);
   }
   MH_DBG("[%s] Receive at offset %"PRIu32", %"PRIu32" bytes remaining\n",
      __func__, client->rx_size, space_left);
   length = read(client->socket, &client->rx_buffer[client->rx_size], space_left); 
   if(length <= 0)
   {
      MH_DBG("[%s] Read failed (%"PRIi32")\n", __func__, length);
      return microhttpd_RemoveClient(ctx, client);
   }
   client->rx_size += length;
   MH_DBG("[%s] Received %"PRIu32" bytes (total now %"PRIu32")\n", __func__, length, client->rx_size);

   cont = true;
   do
   {
      consumed = 0;
      error = false;
      cont = client->state(client, &consumed, &error);

      if(error)
      {
         MH_DBG("[%s] State machine error\n", __func__);
         return microhttpd_RemoveClient(ctx, client);
      }

      if(consumed > 0)
      {
         if(client->rx_size < consumed)
         {
            MH_DBG("[%s] Rx buffer underrun (consumed %"PRIu32" of %"PRIu32" bytes)\n",
               __func__, consumed, client->rx_size);
            return microhttpd_RemoveClient(ctx, client);
         }
         
         string_shift(client->rx_buffer, consumed, client->rx_size);
         client->rx_size -= consumed;
      }
   } while(cont);

   return 0;
}

int microhttpd_HandleClientError(struct md_context *ctx, struct md_client *client)
{
   MH_DBG("[%s]\n", __func__);
   close(client->socket);
   return microhttpd_RemoveClient(ctx, client);
}
