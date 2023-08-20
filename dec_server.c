#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>

static const char good_char[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

/* ERROR HANDLING */
void error(const char* msg) {

  perror(msg);
  exit(1);

}

/* ADDRESS STRUCT */
void setupAddressStruct(struct sockaddr_in* address, int portNumber){
  
  memset((char*) address, '\0', sizeof(*address));
  address->sin_family = AF_INET;
  address->sin_port = htons(portNumber);
  address->sin_addr.s_addr = INADDR_ANY;

}

/* SEND ALL */
int send_all(int socketFD, char* buffer, int length) {
  int total = 0;
  int bytesleft = length;
  int n;

  while (total < length){
    n = send(socketFD, buffer + total, bytesleft, 0);
    if (n == -1) {
      error("DEC LIENT - send all.");
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
      error("DEC CLIENT - send all.");
      return -1;
    }

    total += n;
    bytesleft -= n;
  }

  return 0;
}

void decrypting_text(char* decipher_buf, char* text_buf, char* key_buf, int length){

  // 5. DECRPYT
  memset(decipher_buf, '\0', sizeof(decipher_buf));

  // 5.1 Convert enctext to int
  int enc_int[length - 1];
  int idx = 0;

  for (int i = 0; i < length - 1; i++) {
    for (idx = 0; idx < 27; idx++) {
      if (text_buf[i] == good_char[idx]) enc_int[i] = idx;
    }
  }

  // 5.2 Convert key text to int
  int key_int[length - 1];
  idx = 0;
  
  for (int i = 0; i < length - 1; i++){
    for (idx = 0; idx < 27; idx++){
      if (key_buf[i] == good_char[idx]) key_int[i] = idx;
    }
  }

  // 5.3 plus then mod
  for (int i = 0; i < length - 1; i++){
    int good_char_idx = enc_int[i] - key_int[i];
    if (good_char_idx < 0) good_char_idx += 27;
    good_char_idx = good_char_idx % 27;
    decipher_buf[i] = good_char[good_char_idx];
  }
  decipher_buf[length] = '\0';
  //printf("decipher text: %s\n", decipher_buf);
}


int main(int argc, char* argv[]){

  int connectionSocket, charsRead, charsWritten;
  char buffer[1500];
  struct sockaddr_in serverAddress, clientAddress;
  socklen_t sizeOfClientInfo = sizeof(clientAddress);

  if (argc < 2) {
    fprintf(stderr, "USAGE: %s port.\n", argv[0]);
    exit(1);
  }

  int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (listenSocket < 0) error("ERROR opening socket.\n");

  setupAddressStruct(&serverAddress, atoi(argv[1]));

  if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) error("ERROR on binding.\n");

  listen(listenSocket, 5);

  while(1) {
    
    connectionSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &sizeOfClientInfo);
    if (connectionSocket < 0) error("ERROR on accept.\n");

    int spawnPid;
    char text_buffer[90000];
    char key_buffer[90000];
    char dec_text[90000];

    spawnPid = fork();
    switch(spawnPid){
      
      case -1:
        fprintf(stderr, "fork failed.");
        exit(1);

      case 0:
        // 1. Check if the message comes from the correct enc client
        // 1.1 Receive pair request
        char pair_key[4];
        memset(pair_key, '\0', sizeof(pair_key));
        int pair_rec = recv(connectionSocket, pair_key, sizeof(pair_key), 0);
        if (pair_rec < 0) error("DEC SERVER: pair key received failed.");
        if (strcmp(pair_key, "DEC") != 0) {
          char res[2] = "n";
          int pair_result = send(connectionSocket, res, sizeof(res), 0);
          if (pair_result < 0) error("DEC SERVER: pair result send failed");
        }
        // 1.2 Send pair result
        else {
          char res[2] = "y";
          int pair_result = send(connectionSocket, res, sizeof(res), 0);
          if (pair_result < 0) error("DEC SERVER: pair result send failed");
        }

        // 2. Get text length from client
        int text_length = 0;
        charsRead = recv(connectionSocket, &text_length, sizeof(text_length), 0);
        if (charsRead < 0) error("DEC SERVER ERROR reading from socket - text length.");
        //printf("DEC SERVER: text length: %d\n", text_length);

        // 3. Get enctext from client
        memset(text_buffer, '\0', sizeof(text_buffer));
        charsRead = recv_all(connectionSocket, text_buffer, text_length - 1);   // -1 not read the \n
        if (charsRead < 0) error("SERVER ERROR reading from socket");
        //printf("DEC SERVER enctxt buffer: %s\n", text_buffer);

        // 4. Get the key text from the client
        memset(key_buffer, '\0', sizeof(key_buffer));
        charsRead = recv_all(connectionSocket, key_buffer, text_length - 1);
        if (charsRead < 0) error("ERROR reading from socket - key text.");
        //printf("DEC SERVER key text buffer: %s\n", key_buffer);

        // 5. Encrpyt 
        decrypting_text(dec_text, text_buffer, key_buffer, text_length);

        // 6. Send back to client
        charsWritten = send_all(connectionSocket, dec_text, text_length);
        if (charsWritten < 0) error("SERVER: error writing to socket.");
    
        // Close the connection socket for this client
        close(connectionSocket); 
        exit(0);

      default:
        wait(NULL);
    }
  }
  // Close the listening socket
  close(listenSocket);
  return 0;
}

