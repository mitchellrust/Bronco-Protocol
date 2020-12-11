#include "receiver7035.h"
#include <errno.h>

/**
 * Global Variables
 */
int windowSize = MAXWINSIZE;  // initialize window size to maximum
Node* head;                   // pointer to first node in linked list of unacked segments
struct itimerval timer;       // timer for acking segments

// Vars for sending
int sockfd;
struct sockaddr_in source;
socklen_t sourceLen;

int main(int argc, char *argv[]) {
   // Vars for receiving
   int destPort;
   struct sockaddr_in recv;

   if(argc != 2) {
      printf("Usage: receiver <LISTEN PORT>\n");
      return -1;
   }

   // Initialize receiver vars
   destPort = atoi(argv[1]);
   sockfd = socket(PF_INET, SOCK_DGRAM, 0);
   if (sockfd < 0) {
      perror("Error - opening receiver socket");
      return -1;
   }
   memset(&recv, 0, sizeof recv);
   recv.sin_family = AF_INET;
   recv.sin_addr.s_addr = INADDR_ANY;
   recv.sin_port = htons(destPort);
   if (bind(sockfd, (struct sockaddr *) &recv, sizeof(recv)) < 0) {
      perror("Error - binding to receiver socket");
      return -1;
   }
   sourceLen = sizeof(source);

   // Initialize timer
   signal(SIGALRM, acknowledgeSegments);
   timer.it_value.tv_sec = 3;
   timer.it_value.tv_usec = 0;
   timer.it_interval.tv_sec = 3;
   timer.it_interval.tv_usec = 0;
   setitimer(ITIMER_REAL, &timer, NULL);

   // Listen for segments
   bool done = false;
   while (!done) {
      while (windowSize > 0) {
         Header *seg = malloc(sizeof(struct Header));
         if (seg == NULL) { // malloc failed
            perror("Error - malloc for receiving");
            return -1;
         }

         if (DEBUG) {
            printf("Listening for new segment...\n");
            fflush(stdout);
         }

         int n = recvfrom(sockfd, seg, sizeof(*seg), 0, (struct sockaddr *) &source, (socklen_t *) &sourceLen);
         if (n < 0) {
            perror("Error - receiving from socket");
         }

         windowSize -= 1; // decrement window size

         if (seg->flags & DAT) { // Data segment received
            if (DEBUG) {
               printf("DAT segment received.\n\n");
               printHeader(seg);
            }
            addNodeInOrder(seg);
         } else if (seg->flags & RWA) { // Window Advertisement requested
            if (DEBUG) {
               printf("RWA segment received.\n\n");
               printHeader(seg);
            }
            acknowledgeRWASegment();
            free(seg);
         } else if (seg->flags & EOM) { // No more data segments to come, end execution
            if (DEBUG) {
               printf("EOM segment received.\n\n");
               printHeader(seg);
            }
            timer.it_value.tv_sec = 0;
            setitimer(ITIMER_REAL, &timer, NULL);
            free(seg);
            done = true;
            break;
         } else { // Something happened, error handle here
            if (DEBUG) {
               printf("Unknown packet received. Printing contents:\n\n");
               printHeader(seg);
            }
            free(seg);
         }      
      }
      if (done) {
         break;
      }
      windowSize = MAXWINSIZE;
   }

   // write message to stdout
   writeMessage();

   // End of progam, exit
   return 0;
}

void acknowledgeSegments() {
   int segNum = getLatestSegment();
   if (segNum < 0) {
      return; // no segments to acknowledge
   }

   Header *seg = malloc(sizeof(struct Header));
   if (seg == NULL) { // malloc failed
      perror("Error - malloc for sending acknowledgement");
      return;
   }

   if (DEBUG) {
      printf("Timer expired, acknowledging segments up to and including %u\n\n", segNum);
      fflush(stdout);
   }

   seg->acknowledgement = segNum;
   seg->flags = ACK;
   seg->window = windowSize;
   int bytes = sendSegment(seg);
   if (bytes < 0) {
      ; // error handling done in sender
   }
   free(seg);
}

void acknowledgeRWASegment() {
   Header *seg = malloc(sizeof(struct Header));
   if (seg == NULL) { // malloc failed
      perror("Error - malloc for sending acknowledgement");
      return;
   }

   seg->flags = 0;
   seg->window = windowSize;

   // check if I can acknowledge any segments
   int lastSegNum = getLatestSegment();
   if (lastSegNum >= 0) { // I can acknowledge segments
      seg->flags = ACK;
      seg->acknowledgement = lastSegNum;
      if (DEBUG) {
         printf("Acknowledging RWA segment and DAT segments up to %u.\n\n", lastSegNum);
         fflush(stdout);
      }
   } else if (DEBUG) { // There are no segments to acknowledge
      printf("Acknowledging RWA segment.\n\n");
      fflush(stdout);
   }

   int bytes = sendSegment(seg);
   if (bytes < 0) {
      ; // error handling done in sender
   }
   free(seg);
}

int sendSegment(Header *segP) {
   int bytes;
   sourceLen = sizeof(source);
   bytes = sendto(sockfd, segP, sizeof(*segP), 0, (const struct sockaddr *) &source, sourceLen);
   if (bytes < 0) {
      perror("Error - sending segment");
   }
   return bytes;
}

/**
 * Get's the latest segment number that can be acked
*/
int getLatestSegment() {
   // Base case: first node in list
   if (head == NULL) {
      return -1; // no nodes
   }
   if (head->next == NULL) {
      return head->segment->segmentNumber; // head is only node so it can be acked
   }

   Node *currentNode = head;
   u_int16_t prevSegNum = head->segment->segmentNumber;
   while (currentNode->next != NULL && currentNode->next->segment->segmentNumber == prevSegNum + 1) { // traverse list to find end
      prevSegNum = currentNode->next->segment->segmentNumber;
      currentNode = currentNode->next;
   }
   return currentNode->segment->segmentNumber;
}

/**
 * Add's a data node to the linked list of
 * nodes, in correct sending order
*/
void addNodeInOrder(Header *segP) {
   u_int16_t segNum = segP->segmentNumber;
   Node *newNode = malloc(sizeof(struct Node));
   if (newNode == NULL) { // malloc failed
      perror("Error - malloc for new node in list");
      return;
   }

   newNode->segment = segP;

   // Base case: first node in list
   if (head == NULL || head->segment->segmentNumber >= segNum) {
      if (head != NULL && head->segment->segmentNumber == segNum) { // duplicate segment number, discard node
         if (DEBUG) {
            printf("Received duplicate segment %u, discarding.\n\n", segNum);
            fflush(stdout);
         }
         free(newNode);
         return;
      }
      newNode->next = head;
      head = newNode;
      return;
   }

   Node *currentNode = head;
   while (currentNode->next != NULL && currentNode->next->segment->segmentNumber < segNum) { // traverse list to find entry point
      currentNode = currentNode->next;
   }

   if (currentNode->next && currentNode->next->segment->segmentNumber == segNum) { // duplicate segment number, discard node
      if (DEBUG) {
         printf("Received duplicate segment %u, discarding.\n\n", segNum);
         fflush(stdout);
      }
      free(newNode);
      return;
   }

   newNode->next = currentNode->next;
   currentNode->next = newNode;
   return;
}

/**
 * Finished receiving segments, so write
 * the message to stdout
*/
void writeMessage() {
   if (head == NULL) {
      return;
   }
   Node *currentNode = head;
   int bytes;
   while (currentNode->next != NULL) { // traverse list to find entry point
      bytes = write(STDOUT_FILENO, currentNode->segment->data, strlen(currentNode->segment->data));
      if (bytes < 0) {
         perror("Error - writing data to stdout");
         exit(-1);
      }
      currentNode = currentNode->next;
   }

   // write last node
   bytes = write(STDOUT_FILENO, currentNode->segment->data, strlen(currentNode->segment->data));
   if (bytes < 0) {
      perror("Error - writing data to stdout");
      exit(-1);
   }
}

void printHeader(Header *segP) {
   printf("Segment Number: %u\n", segP->segmentNumber);
   printf("Ackowledgement: %u\n", segP->acknowledgement);
   printf("Flags: %u\n", segP->flags);
   printf("Window: %u\n", segP->window);
   printf("Data Size: %u\n", segP->size);
   printf("Data: %s\n\n\n", segP->data);
   fflush(stdout);
}
