#define _BSD_SOURCE

// Includes for sender.c and receiver.c
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h>

// Includes for shared.h
#include <stdint.h>

#define DEBUG true

#define DAT 0x80
#define ACK 0x40
#define RWA 0x20
#define EOM 0x10

// Bronco Protocol Header required
// for every transmission
typedef struct Header
{
    u_int16_t segmentNumber;
    u_int16_t acknowledgement;

    unsigned char flags; // 8 bits

    u_int8_t window;
    u_int16_t size;
    char data[512];
} Header;

// Node in linked list of segments
typedef struct Node
{
    Header* segment;    // pointer to segment this node references
    struct Node* next;  // pointer to next node in list
} Node;

// Function Declarations
void printHeader(Header *segP);