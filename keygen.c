#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int key_length;

int main(int argc, char* argv[]){

  /* 1. CHECK IF THE NUMBER OF ARGUMENTS ENTERED IS 2 */
  if (argc == 2) {
    key_length = atoi(argv[1]);
  } else if (argc > 2) {
    perror("too many arguments.\n");
  } else {
    perror("Please specify the length of the key you want to generate.\n");
  }

  /*2. GENERATE KEY */
  char keygen[key_length + 2];
  char good_char[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
  srand(time(NULL));

  int i = 0;
  for (i = 0; i < key_length; i++){
    int index = rand() % 27;
    keygen[i] = good_char[index];
    //printf("%c\n", good_char[index]);
    //printf("%c\n", keygen[i]);
  }

  keygen[i] = '\0';
  strcat(keygen, "\n");
  fputs(keygen, stdout);
  return 0;
}
