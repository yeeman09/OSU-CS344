#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <inttypes.h>

#ifndef MAX_WORDS
#define MAX_WORDS 512
#endif

char *words[MAX_WORDS];
size_t wordsplit(char const *line);
void parsing(char* words, size_t words_len, char* new_words);
char * expand(char const *word);
void sigInt_handler(int sig) {};

int exit_status = 0; //$?
int bg_pid[50]; //$!
int bg_count = 0;
int spawnPid;
int childStatus;
int bgFlag = 0;


int main(int argc, char *argv[])
{
  
  struct sigaction SIGINT_old, SIGTSTP_old;

  struct sigaction ignore_action = {0};
  ignore_action.sa_handler = SIG_IGN;
  sigfillset(&ignore_action.sa_mask);
  ignore_action.sa_flags = 0;


  struct sigaction SIGINT_action = {0};
  SIGINT_action.sa_handler = sigInt_handler;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = 0;

  FILE *input = stdin;
  char *input_fn = "(stdin)";
  if (argc == 2) {
    input_fn = argv[1];
    input = fopen(input_fn, "re");
    if (!input) err(1, "%s", input_fn);
  } else if (argc > 2) {
    errx(1, "too many arguments");
  }


  char *line = NULL;
  size_t n = 0;

  for (;;) {
prompt:;

    /* Background Process Handling */
    int bgPid;
    int bgStatus;

    bgPid = waitpid(0, &bgStatus, WNOHANG | WUNTRACED);
    if (bgPid > 0) {
      if (WIFEXITED(bgStatus)) fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t)bgPid, WEXITSTATUS(bgStatus));
      else if (WIFSTOPPED(bgStatus)) {
        fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t)bgPid);
        kill(bgPid, SIGCONT);
      }
      else if (WIFSIGNALED(bgStatus)) fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t)bgPid, WTERMSIG(bgStatus));
    }


    /* TODO: prompt */
    if (input == stdin) {
      char* prompt = getenv("PS1");
      if (prompt != NULL) fprintf(stderr, "%s", prompt);
      else fprintf(stderr, "");

      sigaction(SIGINT, &SIGINT_action, &SIGINT_old);
      sigaction(SIGTSTP, &ignore_action, &SIGTSTP_old);
    }

    ssize_t line_len = getline(&line, &n, input);
    sigaction(SIGINT, &ignore_action, NULL);

    if (line_len < 0) {
      if (feof(input)) {
        exit(0);
        clearerr(input);
        fprintf(stderr, "\n");
        goto prompt;
      }

      else if (errno == EINTR){
        clearerr(input);
        fprintf(stderr, "\n");
        goto prompt;
      }
      err(1, "%s", input_fn);
    }

    size_t nwords = wordsplit(line);
    for (size_t i = 0; i < nwords; ++i) {
      //fprintf(stderr, "Word %zu: %s  -->  ", i, words[i]);
      char *exp_word = expand(words[i]);
      free(words[i]);
      words[i] = exp_word;
      //fprintf(stderr, "%s\n", words[i]);
    }


    //int bgFlag = 0;
    int non_builtin = 0;


    /* Parsing built-in commands here */
    for (int i = 0; i < nwords; ++i){

      if (strcmp(words[nwords - 1], "&") == 0) {
        bgFlag = 1;
        non_builtin = 1;
        //printf("Found a &.\n");
        words[nwords - 1] = NULL;
        nwords--;
        break;
      }

      /* Built-in: CD Command */
      else if (strcmp(words[0], "cd") == 0){

        if (nwords > 2) {
          fprintf(stderr, "too many arguments.\n");
          goto prompt;
        }

        else if (nwords == 2) {
          if (chdir(words[1]) == -1) {
            fprintf(stderr, "directory open failed.\n");
            goto prompt;
          }
        }

        else if (nwords == 1) {
          if (chdir(getenv("HOME")) == -1) {
            fprintf(stderr, "home directory open failed.\n");
            goto prompt;
          }
        }
      }

      /* Built-in: EXIT Command */
      else if (strcmp(words[0], "exit") == 0) {
        
        if (nwords > 2) {
          fprintf(stderr, "too many arguments.\n");
          goto prompt;
        }

        else if (nwords == 2) {
          for (int j = 0; words[1][j] != '\0'; j++){
            if (!isdigit(words[1][j])) {
              fprintf(stderr, "exit argument not an integer.\n");
              goto prompt;
            }
          }
          exit(atoi(words[1]));
        }

        else if (nwords == 1) {
          exit(exit_status);
        }
      }
      
      else non_builtin = 1;
      break;
    }

    /* END of Parsing Built-in Commands */


    /* Parsing non-built-in Commands */
    childStatus = 0;
    char* new_words[nwords];
    int new_len = 0;


    if (non_builtin == 1) {
      spawnPid = fork();
      switch(spawnPid){

        // case -1: fork fail
        case -1:
          perror("fork() failed.\n");
          exit(1);
          goto prompt;

        // case 0: child process
        case 0:

          sigaction(SIGINT, &SIGINT_old, NULL);
          sigaction(SIGTSTP, &SIGTSTP_old, NULL);


          /* Scan for redirection operator */
          for (int i = 0; i < nwords; ++i){

            if (strcmp(words[i], "<") == 0) {
              if (strcmp(words[i + 1], "") == 0) {
                fprintf(stderr, "input failed.\n");
                exit(1);
              }

              int input = open(words[i + 1], O_RDONLY);
              if (input == -1) {
                perror("Cannot open input file.\n");
                exit(1);
              }

              int input_result = dup2(input, 0);
              if (input_result == -1) {
                perror("Cannot assign input.\n");
                exit(1);
              }

              i++;
              fcntl(input, F_SETFD, FD_CLOEXEC);
              continue;
            }

            if (strcmp(words[i], ">") == 0) {
              if (strcmp(words[i + 1], "") == 0) {
                fprintf(stderr, "output failed.\n");
                exit(2);
              }

              int output = open(words[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0777);
              if (output == -1) {
                perror("Cannot open output file.\n");
                exit(2);
              }

              int output_result = dup2(output, 1);
              if (output_result == -1) {
                perror("Cannot assign output.\n");
                exit(2);
              }
              i++;
              fcntl(output, F_SETFD, FD_CLOEXEC);
              continue;
            }

            if (strcmp(words[i], ">>") == 0) {
              if (strcmp(words[i + 1], "") == 0) {
                fprintf(stderr, "append failed.\n");
                exit(3);
              }

              int append = open(words[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0777);
              if (append == -1) {
                perror("Cannot open file.\n");
                exit(3);
              }

              int append_result = dup2(append, 1);
              if (append_result == -1) {
                perror("Cannot append output.\n");
                exit(3);
              }
              i++;
              fcntl(append, F_SETFD, FD_CLOEXEC);
              continue;
            }

            else if (words[i]) {
              new_words[new_len] = words[i];
              new_len++;
            }
          }
        
        new_words[new_len] = NULL;
        //printf("Last word: %s\n", new_words[new_len - 1]);
        //printf("CHILD BG PID : %d\n", bg_pid);
        execvp(new_words[0], new_words);

        perror("non built in failed.\n");
        exit(EXIT_FAILURE);


      // parent process
      default:
        //printf("You got a spawn pid and bgFlag here: %d and %d\n", spawnPid, bgFlag);
        if (bgFlag == 1) {
          bg_pid[bg_count] = spawnPid;
          bg_count++;
          waitpid(spawnPid, &childStatus, WNOHANG);
          bgFlag = 0;
          
          //printf("BG_PID: %d\n", bg_pid);
          goto prompt;
        }
        else {
          spawnPid = waitpid(spawnPid, &childStatus, WUNTRACED);
          if (spawnPid >= 0){
            if (WIFEXITED(childStatus)) exit_status = WEXITSTATUS(childStatus);
            else if (WIFSIGNALED(childStatus)) exit_status = WTERMSIG(childStatus) + 128;
            else if (WIFSTOPPED(childStatus)) {
              kill(spawnPid, SIGCONT);
              fprintf(stderr, "Child process %d stopped. Continuing.\n", spawnPid);
              bg_pid[bg_count] = spawnPid;
              bg_count++;
            }
          }
        }
      }
    }
  }
}

char *words[MAX_WORDS] = {0};

/* Splits a string into words delimited by whitespace. Recognizes
 * comments as '#' at the beginning of a word, and backslash escapes.
 *
 * Returns number of words parsed, and updates the words[] array
 * with pointers to the words, each as an allocated string.
 */
size_t wordsplit(char const *line) {
  size_t wlen = 0;
  size_t wind = 0;

  char const *c = line;
  for (;*c && isspace(*c); ++c); /* discard leading space */

  for (; *c;) {
    if (wind == MAX_WORDS) break;
    /* read a word */
    if (*c == '#') break;
    for (;*c && !isspace(*c); ++c) {
      if (*c == '\\') ++c;
      void *tmp = realloc(words[wind], sizeof **words * (wlen + 2));
      if (!tmp) err(1, "realloc");
      words[wind] = tmp;
      words[wind][wlen++] = *c; 
      words[wind][wlen] = '\0';
    }
    ++wind;
    wlen = 0;
    for (;*c && isspace(*c); ++c);
  }
  return wind;
}


/* Find next instance of a parameter within a word. Sets
 * start and end pointers to the start and end of the parameter
 * token.
 */
char
param_scan(char const *word, char **start, char **end)
{
  static char *prev;
  if (!word) word = prev;
  
  char ret = 0;
  *start = NULL;
  *end = NULL;
  char *s = strchr(word, '$');
  if (s) {
    char *c = strchr("$!?", s[1]);
    if (c) {
      ret = *c;
      *start = s;
      *end = s + 2;
    }
    else if (s[1] == '{') {
      char *e = strchr(s + 2, '}');
      if (e) {
        ret = '{';
        *start = s;
        *end = e + 1;
      }
    }
  }
  prev = *end;
  return ret;
}

/* Simple string-builder function. Builds up a base
 * string by appending supplied strings/character ranges
 * to it.
 */
char *
build_str(char const *start, char const *end)
{
  static size_t base_len = 0;
  static char *base = 0;

  if (!start) {
    /* Reset; new base string, return old one */
    char *ret = base;
    base = NULL;
    base_len = 0;
    return ret;
  }
  /* Append [start, end) to base string 
   * If end is NULL, append whole start string to base string.
   * Returns a newly allocated string that the caller must free.
   */
  size_t n = end ? end - start : strlen(start);
  size_t newsize = sizeof *base *(base_len + n + 1);
  void *tmp = realloc(base, newsize);
  if (!tmp) err(1, "realloc");
  base = tmp;
  memcpy(base + base_len, start, n);
  base_len += n;
  base[base_len] = '\0';

  return base;
}


/* Expands all instances of $! $$ $? and ${param} in a string 
 * Returns a newly allocated string that the caller must free
 */
char *
expand(char const *word)
{
  char const *pos = word;
  char *start, *end;
  char c = param_scan(pos, &start, &end);
  build_str(NULL, NULL);
  build_str(pos, start);
  while (c) {
    /* BG PID */
    if (c == '!') {
      char* background_id = malloc(10);
      sprintf(background_id, "%jd", (intmax_t)bg_pid[bg_count - 1]);
      //printf("Recent bg pid: %s\n", background_id);
      build_str(background_id, NULL);
      free(background_id);
    }

    /* PID */
    else if (c == '$') {
      char* pid = malloc(10);
      sprintf(pid, "%jd", (intmax_t)getpid());
      build_str(pid, NULL);
      free(pid);
    }

    /* EXIT STATUS */
    else if (c == '?') {
      char* status = malloc(10);
      sprintf(status, "%d", exit_status);
      build_str(status, NULL);
      free(status);
    }

    else if (c == '{') {

      size_t length = end - 1 - (start + 2);
      char* string = malloc(length);
      strncpy(string, start + 2, length);
      //printf("%s", string);

      char* parameter;
      parameter = getenv(string);
      build_str(parameter, NULL);
      free(string);
    }
    pos = end;
    c = param_scan(pos, &start, &end);
    build_str(pos, start);
  }
  return build_str(start, NULL);
}


