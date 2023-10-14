/*! \copyright 2018 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file post.c
 *  \brief microhttpd POST Implementation 
 */
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "debug.h"
#include "helpers.h"
#include "post.h"

static bool state_HandlePostHeader(struct md_client *client, uint32_t *consumed, bool *error);
static bool state_HandlePostHeaderComplete(struct md_client *client, uint32_t *consumed, bool *error);
static bool state_HandlePostData(struct md_client *client, uint32_t *consumed, bool *error);

/* ------------------------------------------------------------------------------------------
 * Exported Functions
 */

bool state_HandleOperationPost(struct md_client *client, uint32_t *consumed, bool *error)
{
   uint32_t idx, content_length = 0;
   bool found;

   for(idx = 0, found = false; idx < client->header_entry_count && !found; ++idx)
   {
      char *option = client->header_entries[idx];
      if(memcmp(option, "content-length: ", 16) == 0)
      {
         content_length = strtoul(&option[16], NULL, 10);
         found = true;
      }
   }

   client->content_length = content_length;
   client->content_remaining = content_length;
   client->state = state_HandlePostHeader;
   return true;
}

/* ------------------------------------------------------------------------------------------
 * Private Functions 
 */

static bool state_HandlePostHeader(struct md_client *client, uint32_t *consumed, bool *error)
{
   uint32_t length;
   char *offset;

   offset = string_find(client->rx_buffer, client->rx_size, "\r\n", 2);
   if(offset == NULL)
      return false;  /* Header entry delimiter not found; need more rx data */

   length = offset - client->rx_buffer;
   if(0 == length)
   {
      DBG("%s: Header parsing complete (%"PRIu32" entries)\n", __func__, client->header_entry_count);
      client->state = state_HandlePostHeaderComplete; /* Empty header entry found; header complete */

      client->content_remaining -= 2;
      *consumed = 2;
      return true;
   }
   DBG("%s: Found header option (length %"PRIu32")\n", __func__, length);

   if(!string_list_add(client->rx_buffer, length, &client->post_header_entries,
      &client->post_header_entry_count))
   {
      DBG("%s: Failed to add entry to post header list\n", __func__);
      *error = true;
      return false;
   }

   DBG("%s: Header option %"PRIu32": '%s'\n", __func__, client->post_header_entry_count,
      client->post_header_entries[client->post_header_entry_count - 1]);

   client->content_remaining -= length + 2;
   *consumed = length + 2;
   return true;
}

static bool state_HandlePostHeaderComplete(struct md_client *client, uint32_t *consumed, bool *error)
{
   struct md_context *ctx = client->ctx;
   uint32_t idx;
   bool found;

   client->post_boundary = NULL;
   for(idx = 0, found = false; idx < client->header_entry_count && !found; ++idx)
   {
      client->post_boundary = strstr(client->header_entries[idx], "boundary=");
      if(NULL != client->post_boundary)
      {
         client->post_boundary += 9;
         DBG("%s: boundary is '%s'\n", __func__, client->post_boundary); 
         found = true;
      }
   }

   client->filename = NULL;
   for(idx = 0, found = false; idx < client->post_header_entry_count && !found; ++idx)
   {
      client->filename = strstr(client->post_header_entries[idx], "filename=\"");
      if(client->filename != NULL)
      {
         char *end;
         client->filename += 10;
         end = strchr(client->filename, '\"');
         if(end != NULL)
            *end = '\0';
         DBG("%s: POST filename is %s\n", __func__, client->filename);
         found = true;
      }
   }

   client->post_header_length = client->content_length - client->content_remaining;
   client->post_trailer_length = strlen(client->post_boundary);
   if(client->content_length < (client->post_header_length + client->post_trailer_length))
   {
      DBG("%s: Invalid post data length (total %"PRIu32", header %"PRIu32", footer %"PRIu32"\n", __func__,
         client->content_length, client->post_header_length, client->post_trailer_length);
   }
   else
   {
      client->content_length -= (client->post_header_length + client->post_trailer_length);
      DBG("%s: POST data lengths (total %"PRIu32", header %"PRIu32", footer %"PRIu32"\n", __func__,
         client->content_length, client->post_header_length, client->post_trailer_length);
   }

   if(ctx->params.post_handler != NULL) /* POST start handler */
   {
      ctx->params.post_handler((tMicroHttpdClient) client, client->uri, client->filename,
         (const char **) client->uri_params, client->uri_param_count, client->source_address,
         ctx->params.post_handler_cookie, true, false, NULL, 0, client->content_length);
   }

   client->state = state_HandlePostData;
   return true;
}

static bool state_HandlePostData(struct md_client *client, uint32_t *consumed, bool *error)
{
   struct md_context *ctx = client->ctx;
   uint32_t handled_length, data_length;

   handled_length = client->content_remaining;
   if(handled_length > client->rx_size)
      handled_length = client->rx_size;
      
   client->content_remaining -= handled_length; 
   DBG("%s: POST total length %"PRIu32", current length %"PRIu32", remaining length %"PRIu32"\n",
      __func__, client->content_length, handled_length, client->content_remaining);

   data_length = handled_length;
   if(client->content_remaining < client->post_trailer_length)
      data_length -= client->post_trailer_length - client->content_remaining;
   if(data_length > 0 && ctx->params.post_handler != NULL)
   {
      DBG("%s: Sending %"PRIu32" bytes of data to application\n", __func__, data_length);
      ctx->params.post_handler((tMicroHttpdClient) client, client->uri, client->filename,
         (const char **) client->uri_params, client->uri_param_count,
         client->source_address, ctx->params.post_handler_cookie,
         false, false, client->rx_buffer, data_length, client->content_length);
   }

   *consumed = handled_length; 
   if(0 == client->content_remaining)
   {
      DBG("%s: POST finished\n", __func__);

      /* Post complete handler */
      if(ctx->params.post_handler != NULL)
      {
         ctx->params.post_handler((tMicroHttpdClient) client, client->uri, client->filename,
            (const char **) client->uri_params, client->uri_param_count, client->source_address,
            ctx->params.post_handler_cookie, false, true, NULL, 0, client->content_length);
      }

      microhttpd_ResetState(client);
      return true;
   }

   return false; /* need more rx data */
}
