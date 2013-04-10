#include <string.h>

int main(int argc, char* argv[]) {
  char dst[100];
  char src[100];

  strcpy(src, "foobar");

  memset(dst, 0, sizeof(dst));
  strlcpy(dst, src, 4);
  if (strcmp(dst, "foo")) {
    puts("fail - 4");
    return 1;
  }

  memset(dst, 0, sizeof(dst));
  strlcpy(dst, src, 6);
  if (strcmp(dst, "fooba")) {
    puts("fail - 6");
    return 1;
  }

  memset(dst, 0, sizeof(dst));
  strlcpy(dst, src, 7);
  if (strcmp(dst, "foobar")) {
    puts("fail - 7");
    return 1;
  }

  memset(dst, 0, sizeof(dst));
  strlcpy(dst, src, 8);
  if (strcmp(dst, "foobar")) {
    puts("fail - 8");
    return 1;
  }

  puts("ok");
  return 0;
}
