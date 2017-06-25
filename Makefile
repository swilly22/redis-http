ROOT=$(shell pwd)
# Flags for preprocessor
CPPFLAGS=-Wall -std=c99
LDFLAGS=-lm

#Flags for compiler
CFLAGS=-g -fPIC -O3 -I$(ROOT) -I$(ROOT)/src -I$(ROOT)/src/deps

MODULE_OBJ=$(ROOT)/src/module.o
MODULE_SO=$(ROOT)/redis-http.so

DEPS=$(ROOT)/src/deps/picohttpparser/picohttpparser.o $(ROOT)/src/deps/thpool/thpool.o
UTILS=$(ROOT)/src/rmutil/util.o

export

all: $(MODULE_SO)

$(MODULE_SO): $(MODULE_OBJ) $(DEPS) $(UTILS)
	$(CC) $^ -o$@ -shared $(LDFLAGS)

test:
	$(MAKE) -C pytest test

clean:
	$(RM) $(MODULE_OBJ) $(MODULE_SO) $(DEPS) $(UTILS)