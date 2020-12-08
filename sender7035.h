// Includes for sender.c
#include "shared7035.h"
#include <unistd.h>

// Global constants
#define INTERVAL 3 // number of seconds for segment timer

// Function Declarations
void handleAlarm();
void setTimer();
void cancelTimer();
Header* listenForSegment();
Header* getDataHeader(char buff[]);
int sendSegment(Header *segP, bool isRetry);
void addNode(Header *segP);
void removeNodes(int segNum);
void retransmitSegments();
void retransmitRWASegment();
int getSegmentNumber();
void sendData();
void waitForNewWindow();
void endOfMessage();