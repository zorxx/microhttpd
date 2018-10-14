# \copyright 2018 Zorxx Software. All rights reserved.
# \license This file is released under the MIT License. See the LICENSE file for details.
# \file Makefile
# \brief microhttpd test application build recipe 
TARGET := microhttpd

CC ?= gcc
AR ?= ar
RM ?= rm

CFLAGS := -O3 -Wall -Werror -I..
CDEFS :=
LDFLAGS := -L..
LIBS := microhttpd

SRC := main.c

all: $(TARGET)

$(TARGET): $(foreach src,$(SRC),$(src:.c=.o))
	$(info LINK $@)
	@$(CC) $(LDFLAGS) -Wl,--start-group $(foreach lib,$(LIBS),-l$(lib)) $^ -Wl,--end-group -o $@

%.o: %.c
	$(info CC $^ -> $@)
	@$(CC) $(CFLAGS) $(foreach def,$(CDEFS),-D$(def)) -c $^ -o $@

clean:
	$(info CLEAN)	
	@$(RM) -f *.o $(TARGET)

.PHONY: clean
