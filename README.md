# microhttpd
micro-sized HTTP server

## Overview
There are lots of small web server implementations. microhttpd is designed to have a small code footprint, minimal dependencies, and sufficient configuration to support a wide-variety of applications.

## Features
microhttpd has the following distinguishing features:

- **No threads required, multiple clients supported**\
The microhttpd API provides a function that blocks, waiting for any events to accept new clients or receive data from existing clients. This design makes microhttpd suitable for threaded applications, as well as single-loop applications.
- **POSIX sockets compliant**\
The only features required of the build environment is the standard C library and POSIX (BSD) sockets.
- **Event/callback customization**\
User application entrypoints for servicing HTTP events are all implemented by callback functions. The user application defines functions to handle GET/POST operations for specific URIs and microhttpd invokes the proper callback.
- **No filesystem dependencies**\
Most HTTP servers are designed to serve files from a filesystem; but this isn't useful for embedded applications. The microhttpd library provides no file serving to break this unnecessary dependency. It's trivial to implement a `tMicroHttpGetHandler` to serve files from a filesystem, if desired.

## Usage Example
The following example is a minimal application

```c
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

```
