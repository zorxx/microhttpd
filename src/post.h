/*! \copyright 2018 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file post.h
 *  \brief microhttpd POST interface
 */
#ifndef _MICROHTTPD_POST_H
#define _MICROHTTPD_POST_H

#include <stdint.h>
#include <stdbool.h>
#include "microhttpd_private.h"

bool state_HandleOperationPost(struct md_client *client, uint32_t *consumed, bool *error);

#endif /* _MICROHTTPD_POST_H */
