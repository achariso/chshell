#SHELL := /bin/bash

# ==================================================
# COMMANDS

CC = gcc
RM = rm -f

# ==================================================
# DIRECTORIES

SRC = src
LIB = lib
BIN = bin

# ==================================================
# TARGETS

EXEC = chshell


# ==================================================
# COMPILATION

all: compile_libs compile_chshell

compile_chshell: $(EXEC)

# -- add any dependencies here
%: $(SRC)/%.c
	$(CC) $< -o $(BIN)/$@ $(LIB)/termcap/bin/libtermcap.a -I$(LIB)

clean: clean_libs
	$(RM) $(SRC)/*~ *~

purge: clean
	$(RM) $(addprefix $(BIN)/, $(EXEC))

# ==================================================
# DEPENDENCIES

# Compile
compile_termcap: 
	@cd ./$(LIB)/termcap/src && sh ./configure && make && cp ./libtermcap.a ../bin/

compile_libs: compile_termcap

# Clean
clean_termcap:
	@cd ./$(LIB)/termcap/src && make clean && cd ../bin && rm -f libtermcap.a

clean_libs: clean_termcap

