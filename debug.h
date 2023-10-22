/*! \copyright 2018 - 2023 Zorxx Software. All rights reserved.
 *  \license This file is released under the MIT License. See the LICENSE file for details.
 *  \file debug.h
 *  \brief microhttpd debug
 */
#ifndef _MICROHTTPD_DEBUG_H
#define _MICROHTTPD_DEBUG_H

#include <stdio.h>
#include <assert.h>

#if defined(DEBUG)
#define MH_DBG printf
#define MH_ASSERT(x) assert(x)
#else
#define MH_DBG(...)
#define MH_ASSERT(x)
#endif

#endif /* _MICROHTTPD_DEBUG_H */
