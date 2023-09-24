# \copyright 2018 Zorxx Software. All rights reserved.
# \license This file is released under the MIT License. See the LICENSE file for details.
# \file Makefile
# \brief microhttpd library build recipe 
TARGET := microhttpd

CC ?= gcc
AR ?= ar
RM ?= rm

CFLAGS := -fPIC -O3 -Wall -Werror -I.
#CDEFS += DEBUG

SRC = microhttpd.c helpers.c post.c client.c
HEADERS = microhttpd_private.h microhttpd.h

all: lib$(TARGET).a

lib$(TARGET).a: $(foreach src,$(SRC),$(src:.c=.o))
	$(info LINK $@)	
	@$(AR) rcs $@ $^

%.o: %.c
	$(info CC $^ -> $@)
	@$(CC) $(CFLAGS) $(foreach def,$(CDEFS),-D$(def)) -Iinclude -c $^ -o $@

clean:
	$(info CLEAN)	
	@$(RM) -f *.o lib$(TARGET).a

.PHONY: clean
