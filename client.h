/*! \copyright 2018 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file client.h
 *  \brief microhttpd client interface
 */
#ifndef _MICROHTTPD_CLIENT_H
#define _MICROHTTPD_CLIENT_H

#include "microhttpd_private.h"

int microhttpd_NewClient(struct md_context *ctx, int nSocket, struct sockaddr_in *socket_info);
int microhttpd_RemoveClient(struct md_context *ctx, struct md_client *client);
int microhttpd_HandleClientReceive(struct md_context *ctx, struct md_client *client);
int microhttpd_HandleClientError(struct md_context *ctx, struct md_client *client);

#endif /* _MICROHTTPD_CLIENT_H */
