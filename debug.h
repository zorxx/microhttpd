/*! \copyright 2018 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file debug.h
 *  \brief microhttpd Debug 
 */
#ifndef _MICROHTTPD_DEBUG_H
#define _MICROHTTPD_DEBUG_H

#include <stdio.h>
#include <assert.h>

#if defined(DEBUG)
#define DBG printf
#define ASSERT assert
#else
#define DBG(...)
#define ASSERT(x)
#endif

#endif /* _MICROHTTPD_DEBUG_H */
