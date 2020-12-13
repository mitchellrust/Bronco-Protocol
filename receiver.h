// Includes for receiver.c
#include "shared.h"

// Global Constants
#define MAXWINSIZE 10 // max num of segments receiver can take at once

// Function Declarations
int sendSegment(Header* segP);
void acknowledgeSegments();
int getLatestSegment();
void acknowledgeRWASegment();
void writeMessage();
void addNodeInOrder(Header* segP);
