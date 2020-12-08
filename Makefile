#----------------------------------------------------------#
#
# Makefile - P2
#
# Usage:
#	make all	Builds an executable product
#	make clean	Removes any build artifacts
#
# Author: Mitchell Rust
#
#----------------------------------------------------------#

#-------Defines options passed by make
        CFLAGS = -std=c99 -Wall

#-------Defines the name of the compiler to use
        CC = gcc

#---------------------------------------------------------#

#-------Target "all" builds executable and dependencies
all:    sender7035 receiver7035

#------Target "debug" builds debuggable binaries
debug:  CFLAGS += -g -O0
debug:  all

#-------Target sender builds executable and dependencies
sender7035: sender7035.o
	$(CC) $(CFLAGS) sender7035.o -o sender7035

#-------Target sender builds executable and dependencies
receiver7035: receiver7035.o
	$(CC) $(CFLAGS) receiver7035.o -o receiver7035

#-------Target "clean" removes artifacts from build
clean:
	rm -f sender7035.o receiver7035.o sender7035 receiver7035 *~