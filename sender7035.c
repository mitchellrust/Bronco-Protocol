#include "sender7035.h"

/**
 * Global Variables
 */
int receiversWindowSize = 0;  // # of segments receiver is prepared to accept
int sockfd;                   // file descriptor for open socket
struct sockaddr_in dest;      // destination (receiver) for sent segments
int nextSegmentNumber = -1;   // incremented segment number for sending data segments
Node* head;                   // pointer to first node in linked list of unacked segments
Header* rwaSeg;               // pointer to generic RWA segment
bool waitingForWindow = false;// waiting for new window from receiver

// Vars for receiving
int len;
struct sockaddr_in source; // source (receiver) for received segments

/**
 * Begin Program Execution
 */
int main(int argc, char *argv[]) {
   int destPort;              // Port number for destination, provided by args
   char* addr;                // Address for destination, provided by args
   struct in_addr destAddr;   // Converted IPv4 address for destination

   struct sockaddr_in recv;   // receiver object for receiving segments

   // Check for proper arguments
   if(argc != 3) {
      printf("Usage: sender <DESTINATION IP> <DESTINATION PORT>\n");
      fflush(stdout);
      return -1;
   }

   // Initialize timer handler
   signal(SIGALRM, handleAlarm);

   // Initialize global variables for sending segments
   addr = argv[1];
   destPort = atoi(argv[2]);
   sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
   if (sockfd < 0) {
      perror("Error - opening socket");
      fflush(stdout);
      return -1;
   }
   if (inet_aton(addr, &destAddr) < 0) {
      perror("Error - destination IPv4 address");
      fflush(stdout);
      return -1;
   }
   memset(&dest, 0, sizeof dest);
   dest.sin_family = AF_INET;
   dest.sin_addr = destAddr;
   dest.sin_port = htons(destPort);

   // Initialize receiver vars
   memset(&recv, 0, sizeof recv);
   recv.sin_family = AF_INET;
   recv.sin_addr.s_addr = htonl(INADDR_ANY);
   recv.sin_port = htons(0);
   if (bind(sockfd, (struct sockaddr *) &recv, sizeof(recv)) < 0) {
      perror("Error - binding to socket");
      fflush(stdout);
      return -1;
   }

   // Initialize global RWA segment
   rwaSeg = malloc(sizeof(struct Header));
   if (rwaSeg == NULL) { // malloc failed
      perror("Error - malloc for initial window request");
      fflush(stdout);
      return -1;
   }
   rwaSeg->segmentNumber = nextSegmentNumber + 1; // segment number of initial window request is same as first data segment
   rwaSeg->flags = RWA;
   rwaSeg->size = 0;

   printf("Sending initial RWA Segment.\n\n");
   printHeader(rwaSeg);
   fflush(stdout);

   // Send RWA segment
   int bytes = sendSegment(rwaSeg, false);
   if (bytes < 0) {
      perror("Error - sending initial RWA segment");
      fflush(stdout);
      return -1;
   }
   waitingForWindow = true;
   alarm(INTERVAL);
   while (receiversWindowSize <= 0) {
      receiversWindowSize = 0;
      Header* rwaResponse = listenForSegment();    // Await window size from receiver
      printHeader(rwaResponse);
      free(rwaResponse);
   }
   fflush(stdout);
   alarm(0); // response received, cancel timer
   waitingForWindow = false;

   printf("Acknowledgement received for initial data segment.\n\n");
   fflush(stdout);

   // Begin sending data
   sendData();

   // End of program, exit
   free(rwaSeg);
   return 0;
}

/**
 * STATE 1
 * 
 * Read data from stdin and send segments if able
 */
void sendData() {
   char buff[512];
   int count = 0;
   while((count = read(STDIN_FILENO, buff, 511)) != 0) {
      if (count < 0) {
         perror("Error - reading data from stdin");
         fflush(stdout);
         exit(-1);
      }

      buff[count] = '\0';

      // Check window size before continuing
      if (receiversWindowSize <= 0) {
         printf("Receiver is full, waiting for new window.\n\n");
         fflush(stdout);
         waitForNewWindow();
      }

      // Create and send data segment
      Header* dSeg = getDataHeader(buff);
      printf("Sending data segment %u.\n\n", dSeg->segmentNumber);
      fflush(stdout);
      int bytes = sendSegment(dSeg, false);
      if (bytes < 0) {
         perror("Error - sending data segment");
         fflush(stdout);
         exit(-1);
      }
   }

   // EOF reached
   endOfMessage();
}

/**
 * STATE 2
 * 
 * Wait for receiver to send a new window size
 * to signal readiness for more segments 
 */
void waitForNewWindow() {
   printf("Window full, sending RWA segment.\n\n");
   fflush(stdout);
   // Send RWA segment
   int bytes = sendSegment(rwaSeg, false);
   if (bytes < 0) {
      perror("Error - retransmitting RWA segment");
      fflush(stdout);
      exit(-1);
   }

   printf("Waiting for window advertisement.\n\n");
   fflush(stdout);

   waitingForWindow = true;
   alarm(INTERVAL);

   while (receiversWindowSize <= 0) {
      Header* response = listenForSegment(); // Await window size from receiver
      if (response->flags & ACK) {
         removeNodes(response->acknowledgement);
      }
      free(response);
   }

   alarm(0); // response received, cancel timer
   waitingForWindow = false;

   printf("Window advertisement received.\n\n");
   fflush(stdout);

   // Returns here to contine sending data
}

/**
 * STATE 3
 * 
 * Entire message has been transmitted, so wait for
 * all acknowledgements, retransmitting as necessary
 */
void endOfMessage() {
   alarm(INTERVAL);
   while (head != NULL) {
      printf("\n\nListening for all acknowledgements\n\n");
      fflush(stdout);
      Header* segResponse = listenForSegment();
      if (segResponse == NULL) {
         printf("HERE endOfMessage received was interuppted\n\n");
         fflush(stdout);
         free(segResponse);
         continue;
      }

      printHeader(segResponse);
      if (segResponse->flags & ACK) {
         printf("received ack for segment %u\n\n", segResponse->acknowledgement);
         fflush(stdout);
         removeNodes(segResponse->acknowledgement);
      }

      free(segResponse);
   }
   alarm(0);

   printf("Received acknowledgements for all segments, sending EOM.\n\n");
   fflush(stdout);

   // All segments have been acknowledged
   // Send EOM segment
   Header *eomSeg = malloc(sizeof(struct Header));
   if (eomSeg == NULL) { // malloc failed
      perror("Error - malloc for EOM segment");
      fflush(stdout);
      exit(-1);
   }
   eomSeg->segmentNumber = 0; // Arbitrary value, does not get checked in receiver
   eomSeg->flags = EOM;
   eomSeg->size = 0;

   printf("Sending EOM segment... ");
   fflush(stdout);
   int bytes = sendSegment(eomSeg, false);
   if (bytes < 0) {
      ; // no error checking for EOM segment
   }
   printf("done. Exiting.\n");
   fflush(stdout);
}

/**
 * Send a segment to the receiver and perform any
 * necessary updating of timers, receiversWindowSize,
 * and linked list of unacked segments
*/
int sendSegment(Header *segP, bool isRetry) {
   int bytes;

   bytes = sendto(sockfd, segP, sizeof(*segP), 0, (const struct sockaddr *) &dest, sizeof(dest));
   if (bytes < 0) {
      perror("Error - sending segment");
      fflush(stdout);
      return -1;
   }

   receiversWindowSize -= 1;

   if (!isRetry && segP->flags & DAT) {
      addNode(segP);
   }
   return bytes;
}

/**
 * Creates a data segment for sending to
 * the receiver, with the correct fields
 * specified.
*/
Header* getDataHeader(char buff[]) {
   Header *seg = malloc(sizeof(struct Header));
   if (seg == NULL) { // malloc failed
      perror("Error - malloc for data segment");
      fflush(stdout);
      exit(-1);
   }
   seg->segmentNumber = getSegmentNumber();
   seg->flags = DAT;
   seg->size = strlen(buff);
   strncpy(seg->data, buff, 512);
   return seg;
}

/** 
 * Listen for ACK segments from receiver to
 * acknowledge data received
*/
Header* listenForSegment() {
   Header *seg = malloc(sizeof(struct Header));
   if (seg == NULL) { // malloc failed
      perror("Error - malloc for receiving acknowledgement");
      fflush(stdout);
      exit(-1);
   }
   printf("Listening for segment... ");
   fflush(stdout);
   int n = recvfrom(sockfd, seg, sizeof(*seg), 0, (struct sockaddr *) &source, (socklen_t *) &len);
   if (n < 0) {
      perror("Error - receiving from socket");
      fflush(stdout);
      return NULL;
   } else {
      printf("segment received.\n\n");
      fflush(stdout);
   }
   receiversWindowSize = seg->window;
   return seg;
}

/**
 * Sends a previously sent segment to receiver
 * due to timer expiring and being unacked
*/
void retransmitSegments() {
   Node *currentNode = head;

   while (currentNode->next != NULL) {

      // Check window size before continuing
      if (receiversWindowSize <= 0) {
         printf("Receiver is full, waiting for new window before retransmitting more segments.\n\n");
         fflush(stdout);
         waitForNewWindow();
      }
      
      int bytes = sendSegment(currentNode->segment, true); // retransmit segment
      if (bytes < 0) {
         perror("Error - retransmitting segment");
         fflush(stdout);
      }
      currentNode = currentNode->next;
   }

   // Check window size before continuing
   if (receiversWindowSize <= 0) {
      printf("Receiver is full, waiting for new window before retransmitting more segments.\n\n");
      fflush(stdout);
      waitForNewWindow();
   }

   // Retransmit last segment in list
   printf("Retransmitting data segment %u\n\n", currentNode->segment->segmentNumber);
   fflush(stdout);
   int bytes = sendSegment(currentNode->segment, true);
   if (bytes < 0) {
      perror("Error - retransmitting last segment in list");
      fflush(stdout);
   }
}

/**
 * Sends a previously sent segment to receiver
 * due to timer expiring and being unacked
*/
void retransmitRWASegment() {
   rwaSeg->segmentNumber = 0; // doesn't matter
   rwaSeg->flags = RWA;
   rwaSeg->size = 0;
   int bytes = sendSegment(rwaSeg, true);
   if (bytes < 0) {
      perror("Error - retransmitting last segment in list");
      fflush(stdout);
   }
}

/**
 * Add's a node to the linked list of
 * nodes containing unacked segments
*/
void addNode(Header *segP) {
   Node *newNode = malloc(sizeof(struct Node));
   if (newNode == NULL) { // malloc failed
      perror("Error - malloc for adding new node");
      fflush(stdout);
      exit(-1);
   }

   newNode->segment = segP;
   newNode->next = NULL;

   // Base case: first node in list
   if (head == NULL) {
      head = newNode;
      return;
   }

   Node *lastNode = head;
   while (lastNode->next != NULL) { // traverse list to find end
      lastNode = lastNode->next;
   }
   lastNode->next = newNode;
}

/**
 * Free segment, then free node, for all nodes
 * before and up to the specified segment Number
*/
void removeNodes(int segNum) {
   printf("Removing segments up to and including %u\n\n", segNum);
   fflush(stdout);
   if (head != NULL && head->segment->segmentNumber > segNum) { // segment already acked and removed
      printf("Segment %u already acked, doing nothing.\n\n", segNum);
      fflush(stdout);
      return;
   }
   while (head != NULL) {
      printf("Head segment Number: %u\n\n", head->segment->segmentNumber);
      if (head->segment->segmentNumber <= segNum) {
         Node *temp = head;
         head = temp->next;
         free(temp->segment); // free segment
         free(temp);          // free node
      } else {
         break;
      }
   }
}

/**
 * Prints out the elements of a segment
*/
void printHeader(Header *segP) {
   printf("\nSegment Number: %u\n", segP->segmentNumber);
   printf("Ackowledgement: %u\n", segP->acknowledgement);
   printf("Flags: %u\n", segP->flags);
   printf("Window: %u\n", segP->window);
   printf("Data Size: %u\n", segP->size);
   printf("Data: %s\n\n", segP->data);
   fflush(stdout);
}

/**
 * Increments nextSegmentNumber and returns the value.
 * This is to ensure proper and uniform incrementing.
 */
int getSegmentNumber() {
   if (nextSegmentNumber == 65535) { // support segment number wrapping
      nextSegmentNumber = 0;
   } else {
      nextSegmentNumber += 1;
   }
   return nextSegmentNumber;
}

/**
 * Handler function for when SIGALRM is triggered
*/
void handleAlarm() {
   signal(SIGALRM, handleAlarm);
   if (waitingForWindow) {
      printf("\nAlarm triggered, retransmitting RWA segment.\n\n");
      fflush(stdout);
      retransmitRWASegment(); // retransmit RWA segment
   } else {
      printf("\nAlarm triggered, retransmitting data segments.\n\n");
      fflush(stdout);
      retransmitSegments(); // retransmit any unacked segments
   }
   alarm(INTERVAL);
}