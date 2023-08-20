#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>  // ssize_t
#include <sys/socket.h> // send(),recv()
#include <netdb.h>      // gethostbyname()


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
    fprintf(stderr, "CLIENT: ERROR, no such host\n"); 
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
      error("CLIENT - send all.");
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
      error("CLIENT - send all.");
      return -1;
    }

    total += n;
    bytesleft -= n;
  }

  return 0;
}

/* MAIN */
int main(int argc, char *argv[]) {

  //argv: 0 - ./enc_client 1-plaintext 2-key 3-port

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

  // 1. Plaintext 
  FILE* fp;
  fp = fopen(argv[1], "r");
  if (!fp) {
    error("Plaintext Open Error.\n");
    exit(1);
  }

  char c;
  while (1) {
    c = fgetc(fp);
    if (feof(fp)) break;

    if (!isupper(c)) {
      if (!isspace(c)){
        fprintf(stderr, "Invalid characters in plaintext.\n");
        exit(1);
      }
    }
  }
  int plaintext = open(argv[1], O_RDONLY);
  int plaintext_length;
  plaintext_length = lseek(plaintext, 0, SEEK_END);
  
  fclose(fp);
  close(plaintext);

  // 2. Key
  FILE *key;
  key = fopen(argv[2], "r");
  if (!key) {
    error("Key Open Error.\n");
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
  if (plaintext_length > key_length) {
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
    char pair_key[] = "ENC";
    int keySent = send(socketFD, pair_key, sizeof(pair_key), 0);
    if (keySent < 0) error("CLIENT pair key sent failed.");

  /* RECEIVE pair key from server */
    char pair_result[2];    // yes or no?
    int keyReceive = recv(socketFD, pair_result, sizeof(pair_result), 0);
    if (keyReceive < 0) error("CLIENT pair result received failed.");
    if (strcmp(pair_result, "y") != 0) error("CLIENT not correct client/server");


  /* SEND TEXT LENGTH to the server */
    int sentLength = send(socketFD, &plaintext_length, sizeof(plaintext_length), 0);
    //printf("sent length: %d\n", sentLength);
    //printf("CLIENT: plain text length: %d\n", plaintext_length);
    if (sentLength < 0) error("CLIENT - plaintext length: ERROR writing to socket");


  /* SEND PLAINTEXT to the server */
    FILE* plaintext_pointer;
    plaintext_pointer = fopen(argv[1], "r");
    char input_text[90000];
    memset(input_text, '\0', sizeof(input_text));
    fgets(input_text, sizeof(input_text), plaintext_pointer);
    input_text[strcspn(input_text, "\n")] = '\0';
    //printf("CLIENT Plaintext buffer: %s\n", input_text);

    // Send message to server
    charsWritten = send_all(socketFD, input_text, plaintext_length);
    if (charsWritten == -1) error("Client 1 - sending message error.");
    fclose(plaintext_pointer);


  /* SEND KEY */
    FILE* key_pointer;
    key_pointer = fopen(argv[2], "r");
    char key_gen[90000];
    memset(key_gen, '\0', sizeof(key_gen));
    fgets(key_gen, sizeof(key_gen), key_pointer);
    key_gen[strcspn(key_gen, "\n")] = '\0';
    //printf("CLIENT key buffer: %s\n", key_gen);

    // Send message to server
    charsWritten = send_all(socketFD, key_gen, plaintext_length);
    if (charsWritten == -1) error("Client 2 - sending KEY error.");
    fclose(key_pointer);

  /* SHUTDOWN for sending*/
    //shutdown(socketFD, SHUT_WR);

  /* RECEIVE Ciphertext */
    char ciphered[plaintext_length + 1];
    memset(ciphered, '\0', sizeof(ciphered));
    // Read data from the socket, leaving \0 at end
    charsRead = recv_all(socketFD, ciphered, plaintext_length - 1);
    if (charsRead == -1) error("Client 3 - sending message error.");
    //printf("Got cipher buffer: %s\n", ciphered);
    strcat(ciphered, "\n");

  /* SHUTDOWN for reading */
    //shutdown(socketFD, SHUT_RD);
    
  /* STDOUT */
  fflush(stdout);
  write(1, ciphered, plaintext_length);

  // Close the socket & Two opened files: fp (plaintext), key
  close(socketFD);
  return 0;
}
