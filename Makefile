DIR=.
BIN_DIR=$(DIR)/bin
SRC_DIR=$(DIR)/src
INCLUDE_DIR=$(DIR)/
OBJ_DIR=$(DIR)/obj
LIB_DIR=$(DIR)/lib
LIB=libcr.a

EXTENSION=cc
OBJS=$(patsubst $(SRC_DIR)/%.$(EXTENSION), $(OBJ_DIR)/%.o,$(wildcard $(SRC_DIR)/*.$(EXTENSION)))
DEPS=$(patsubst $(OBJ_DIR)/%.o, $(DEPS_DIR)/%.d, $(OBJS))

INCLUDE= 
		
CC=g++
AR= ar rcu
CFLAGS=-Wall -Werror -g 
#LDFLAGS= -L ./lib -lcr -pthread
LDFLAGS= -L ./lib -lcr -ldl

all:$(OBJS)
	$(AR) $(LIB_DIR)/$(LIB) $(OBJS)
	#$(CC) test/main.c -I./src $(INCLUDE) -o test/test $(LDFLAGS)

ex:$(LIB_DIR)/$(LIB) example/*.cc
	$(CC) example/test.cc -I./src $(CFLAGS) $(INCLUDE) -o example/test $(LDFLAGS)

test:$(LIB_DIR)/$(LIB)
	$(CC) test/main.c -I./src $(CFLAGS) $(INCLUDE) -o test/test $(LDFLAGS)

$(OBJ_DIR)/%.o:$(SRC_DIR)/%.$(EXTENSION) 
	$(CC) $< -o $@ -c $(CFLAGS) $(INCLUDE) 

rebuild:
	make clean
	make

clean:
	rm -rf $(OBJS) $(BIN_DIR)/* $(LIB_DIR)/*
