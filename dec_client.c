#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>  // ssize_t
#include <sys/socket.h> // send(),recv()
#include <netdb.h>      // gethostbyname()

/* THIS IS DECRYPT PROGRAM ! */

/* ERROR HANDLING */
void error(const char *msg) { 
  perror(msg); 
} 



/* Set up the address struct */
void setupAddressStruct(struct sockaddr_in* address, 
                        int portNumber, 
                        char* hostname){
 
  // Clear out the address struct
  memset((char*) address, '\0', sizeof(*address)); 

  // The address should be network capable
  address->sin_family = AF_INET;

  // Store the port number
  address->sin_port = htons(portNumber);

  // Get the DNS entry for this host name
  struct hostent* hostInfo = gethostbyname(hostname); 
  if (hostInfo == NULL) { 
    fprintf(stderr, "DEC CLIENT: ERROR, no such host\n"); 
    exit(0); 
  }

  // Copy the first IP address from the DNS entry to sin_addr.s_addr
  memcpy((char*) &address->sin_addr.s_addr, 
        hostInfo->h_addr_list[0],
        hostInfo->h_length);
}

/* SEND ALL */
int send_all(int socketFD, char* buffer, int length) {
  int total = 0;
  int bytesleft = length;
  int n;

  while (total < length){
    n = send(socketFD, buffer + total, bytesleft, 0);
    if (n == -1) {
      error("DEC CLIENT - send all.");
      return -1;
    }

    total += n;
    bytesleft -= n;
  }

  return 0;
}

/* RECV ALL */
int recv_all(int socketFD, char* buffer, int length) {
  int total = 0;
  int bytesleft = length;
  int n;

  while (total < length){
    n = recv(socketFD, buffer + total, bytesleft, 0);
    if (n == -1) {
      error("DEC CLIENT - RECV all.");
      return -1;
    }

    total += n;
    bytesleft -= n;
  }

  return 0;
}

/* MAIN */
int main(int argc, char *argv[]) {

  //argv: 0 - ./enc_client 1-enctext 2-key 3-port

  int socketFD, portNumber, charsWritten, charsRead;
  struct sockaddr_in serverAddress;
  char buffer[2000];

  // Check usage & args
  if (argc < 4) { 
    fprintf(stderr,"USAGE: %s hostname port\n", argv[0]); 
    exit(0); 
  }

  /* TODO: Check bad characters */
  char good_char[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

  // 1. Ciphered text 
  FILE* fp;
  fp = fopen(argv[1], "r");
  if (!fp) {
    error("CLIENT Ciphered Open Error.\n");
    exit(1);
  }

  char c;
  while (1) {
    c = fgetc(fp);
    if (feof(fp)) break;

    if (!isupper(c)) {
      if (!isspace(c)){
        fprintf(stderr, "Invalid characters in enctext.\n");
        exit(1);
      }
    }
  }
  int enctext_fd = open(argv[1], O_RDONLY);
  int enctext_length;
  enctext_length = lseek(enctext_fd, 0, SEEK_END);
  
  fclose(fp);
  close(enctext_fd);

  // 2. Key
  FILE *key;
  key = fopen(argv[2], "r");
  if (!key) {
    error("CLIENT Key Open Error.\n");
    exit(1);
  }

  char key_c;
  while (1) {
    key_c = fgetc(key);
    if (feof(key)) break;

    if (!isupper(key_c)){
      if (!isspace(key_c)) {
        fprintf(stderr, "Invalid characters in key.\n");
        exit(1);
      }
    }
  }

  int key_text = open(argv[2], O_RDONLY);
  int key_length;
  key_length = lseek(key_text, 0, SEEK_END);
  
  fclose(key);
  close(key_text);


  /* TODO: Check length */
  if (enctext_length > key_length) {
    fprintf(stderr, "key too short.\n");
    exit(1);
  }

  // Create a socket
  socketFD = socket(AF_INET, SOCK_STREAM, 0); 
  if (socketFD < 0){
    error("CLIENT: ERROR opening socket");
  }

  // Set up the server address struct
  setupAddressStruct(&serverAddress, atoi(argv[3]), "localhost");

  // Connect to server
  if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
    error("CLIENT: ERROR connecting");
  }

  /* SEND server pair key */
    char pair_key[] = "DEC";
    int keySent = send(socketFD, pair_key, sizeof(pair_key), 0);
    if (keySent < 0) error("DEC CLIENT pair key sent failed.");

  /* RECEIVE pair key from server */
    char pair_result[2];    // yes or no?
    int keyReceive = recv(socketFD, pair_result, sizeof(pair_result), 0);
    if (keyReceive < 0) error("DEC CLIENT pair result received failed.");
    if (strcmp(pair_result, "y") != 0) error("DEC CLIENT not correct client/server");


  /* SEND TEXT LENGTH to the server */
    int sentLength = send(socketFD, &enctext_length, sizeof(enctext_length), 0);
    //printf("sent length: %d\n", sentLength);
    //printf("DEC CLIENT: enc text length: %d\n", enctext_length);
    if (sentLength < 0) error("DEC CLIENT - enctext length: ERROR writing to socket");


  /* SEND ENCTEXT to the server */
    FILE* enctext_pointer;
    enctext_pointer = fopen(argv[1], "r");
    char enctext[90000];
    memset(enctext, '\0', sizeof(enctext));
    fgets(enctext, sizeof(enctext), enctext_pointer);
    enctext[strcspn(enctext, "\n")] = '\0';
    //printf("DEC CLIENT enctext buffer: %s\n", enctext);

    // Send message to server
    charsWritten = send_all(socketFD, enctext, enctext_length);
    if (charsWritten == -1) error("DEC Client 1 - sending message error.");
    fclose(enctext_pointer);


  /* SEND KEY */
    FILE* key_pointer;
    key_pointer = fopen(argv[2], "r");
    char key_gen[90000];
    memset(key_gen, '\0', sizeof(key_gen));
    fgets(key_gen, sizeof(key_gen), key_pointer);
    key_gen[strcspn(key_gen, "\n")] = '\0';
    //printf("DEC CLIENT key buffer: %s\n", key_gen);

    // Send message to server
    charsWritten = send_all(socketFD, key_gen, enctext_length);
    if (charsWritten == -1) error("DEC Client 2 - sending KEY error.");
    fclose(key_pointer);

  /* SHUTDOWN for sending*/
    //shutdown(socketFD, SHUT_WR);

  /* RECEIVE DE-Ciphertext */
    char de_ciphered[enctext_length + 1];
    memset(de_ciphered, '\0', sizeof(de_ciphered));
    // Read data from the socket, leaving \0 at end
    charsRead = recv_all(socketFD, de_ciphered, enctext_length - 1);
    if (charsRead == -1) error("Client 3 - sending message error.");
    //printf("Got decipher buffer: %s\n", de_ciphered);
    strcat(de_ciphered, "\n");

  /* SHUTDOWN for reading */
    //shutdown(socketFD, SHUT_RD);
    
  /* STDOUT */
  fflush(stdout);
  write(1, de_ciphered, enctext_length);

  // Close the socket & Two opened files: fp (enctext), key
  close(socketFD);
  return 0;
}
