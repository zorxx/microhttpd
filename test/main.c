/*! \copyright 2018 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file main.c
 *  \brief microhttpd test application 
 */
#include <stdio.h>
#include <string.h>
#include "microhttpd.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

static void handle_root(tMicroHttpdClient client, const char *uri,
   const char *param_list[], const uint32_t param_count, const char *source_address, void *cookie);
static void handle_not_found(tMicroHttpdClient client, const char *uri,
   const char *param_list[], const uint32_t param_count, const char *source_address, void *cookie);
static tMicroHttpdGetHandlerEntry get_handler_list[] =
{
   { "/", handle_root, NULL }
};

int main(int argc, char *argv[])
{
   tMicroHttpdParams params = {0};
   void *ctx;

   params.server_port = 80;
   params.process_timeout = 0;
   params.rx_buffer_size = 2048;
   params.get_handler_list = get_handler_list;
   params.get_handler_count = ARRAY_SIZE(get_handler_list);
   params.default_get_handler = handle_not_found;

   ctx = microhttpd_start(&params);
   if(NULL == ctx)
   {
      fprintf(stderr, "Failed to initialize microhttpd\n");
      return -1;
   }

   while(microhttpd_process(ctx) == 0);
   return 0;
}

/* ------------------------------------------------------------------------------------------------------------------
 *
 */

static void handle_root(tMicroHttpdClient client, const char *uri,
   const char *param_list[], const uint32_t param_count, const char *source_address, void *cookie)
{
   char content[] = "<html>Hello there!</html>";
   microhttpd_send_response(client, 200, "text/html", strlen(content), NULL, content); 
}

static void handle_not_found(tMicroHttpdClient client, const char *uri,
   const char *param_list[], const uint32_t param_count, const char *source_address, void *cookie)
{
   char content[200];
   snprintf(content, sizeof(content), "<html><title>Not Found</title>Not found: %s</html>", uri);
   microhttpd_send_response(client, 404, "text/html", strlen(content), NULL, content); 
}
