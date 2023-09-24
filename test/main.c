/*! \copyright 2018 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file main.c
 *  \brief microhttpd test application 
 */
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <malloc.h>
#include <sys/time.h>
#include "microhttpd/microhttpd.h"

#define SERVER_PORT 8090

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define FILE_BUFFER_SIZE 2048
#define DBG printf

static void send_not_found(tMicroHttpdClient client, const char *uri);
static void handle_test(tMicroHttpdClient client, const char *uri,
   const char *param_list[], const uint32_t param_count, const char *source_address, void *cookie);
static void handle_ajax(tMicroHttpdClient client, const char *uri,
   const char *param_list[], const uint32_t param_count, const char *source_address, void *cookie);
static void handle_file(tMicroHttpdClient client, const char *uri,
   const char *param_list[], const uint32_t param_count, const char *source_address, void *cookie);
static tMicroHttpdGetHandlerEntry get_handler_list[] =
{
   { "/ajax", handle_ajax, NULL },
   { "/test", handle_test, NULL }
};

int main(int argc, char *argv[])
{
   tMicroHttpdParams params = {0};
   tMicroHttpdContext ctx;

   params.server_port = SERVER_PORT;
   params.process_timeout = 0;
   params.rx_buffer_size = 2048;
   params.get_handler_list = get_handler_list;
   params.get_handler_count = ARRAY_SIZE(get_handler_list);
   params.default_get_handler = handle_file;

   ctx = microhttpd_start(&params);
   if(NULL == ctx)
   {
      fprintf(stderr, "Failed to initialize microhttpd\n");
      return -1;
   }

   while(microhttpd_process(ctx) == 0);
   return 0;
}

/* ---------------------------------------------------------------------------------------------
 *
 */

static void send_not_found(tMicroHttpdClient client, const char *uri)
{
   char *content = malloc(strlen(uri) + 50);

   if(NULL == content)
      return;

   sprintf(content, "<html><title>Not Found</title>Not found: %s</html>", uri);
   microhttpd_send_response(client, 404, "text/html", strlen(content), NULL, content);
   free(content);
}

static void handle_test(tMicroHttpdClient client, const char *uri,
   const char *param_list[], const uint32_t param_count, const char *source_address, void *cookie)
{
   char content[] = "<html>Hello there!</html>";
   microhttpd_send_response(client, 200, "text/html", strlen(content), NULL, content); 
}

static void handle_ajax(tMicroHttpdClient client, const char *uri,
   const char *param_list[], const uint32_t param_count, const char *source_address, void *cookie)
{
   if(param_count == 0)
   {
      DBG("%s: No AJAX operation specified\n", __func__);
      send_not_found(client, uri);
   }

   if(param_count > 0 && strcmp(param_list[0], "update_time") == 0)
   {
      char content[40];
      struct timeval tv;
      time_t curtime;

      gettimeofday(&tv, NULL);
      curtime = tv.tv_sec;
      strftime(content, sizeof(content), "%m-%d-%Y %T", localtime(&curtime));
      DBG("%s: Sending time update (%s)\n", __func__, content);
      microhttpd_send_response(client, 200, "text/html", strlen(content), NULL, content);
      return;
   }

   DBG("%s: AJAX operation '%s' not found\n", __func__, param_list[0]);
   send_not_found(client, uri);
}

static void handle_file(tMicroHttpdClient client, const char *uri,
   const char *param_list[], const uint32_t param_count, const char *source_address, void *cookie)
{
   FILE *pFile;
   int32_t total_length, sent_length;
   char *content;
   bool failed = false;

   pFile = fopen(&uri[1], "rb");
   if(NULL == pFile)
   {
      send_not_found(client, uri);
      return;
   }

   fseek(pFile, 0, SEEK_END);
   total_length = ftell(pFile);
   DBG("%s: sending file, length %u\n", __func__, total_length);
   rewind(pFile);
   microhttpd_send_response(client, 200, "text/html", total_length, NULL, NULL);

   content = malloc(FILE_BUFFER_SIZE);
   if(NULL == content)
      failed = true;

   sent_length = 0;
   while(!failed && sent_length < total_length)
   {
      int32_t read_length = fread(content, 1, FILE_BUFFER_SIZE, pFile);
      if(read_length <= 0)
      {
         DBG("%s: failed to read from file\n", __func__);
         failed = true;
      }
      else
      {
         DBG("%s: sending %u bytes\n", __func__, total_length);
         microhttpd_send_data(client, read_length, content);
         sent_length += read_length;
      }
   }

   if(NULL != content)
      free(content);
   fclose(pFile);
}
