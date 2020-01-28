#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main () {
  FILE *f = fopen("test.txt", "rb");
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET);  /* same as rewind(f); */

  char *string = malloc(fsize + 1);
  fread(string, 1, fsize, f);

  printf("%s", string);
  return 0;
}
