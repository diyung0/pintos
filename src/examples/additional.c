#include <syscall.h>
#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char *argv[])
{
  if (argc != 5) {
    printf("Usage: additional [num1] [num2] [num3] [num4]\n");
    return EXIT_FAILURE;
  }

  int num1 = atoi(argv[1]);
  int num2 = atoi(argv[2]);
  int num3 = atoi(argv[3]);
  int num4 = atoi(argv[4]);

  int fib_val = fibonacci(num1);
  int max_val = max_of_four_int(num1, num2, num3, num4);

  printf("%d %d\n", fib_val, max_val);
  return EXIT_SUCCESS;
}
