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
all:    sender receiver

#------Target "debug" builds debuggable binaries
debug:  CFLAGS += -g -O0
debug:  all

#-------Target sender builds executable and dependencies
sender: sender.o
	$(CC) $(CFLAGS) sender.o -o sender7035

#-------Target sender builds executable and dependencies
receiver: receiver.o
	$(CC) $(CFLAGS) receiver.o -o receiver7035

#-------Target "clean" removes artifacts from build
clean:
	rm -f sender.o receiver.o sender7035 receiver7035 *~
