#include "main.h"

#include <stdio.h>

int main(int argc, char** argv)
{
	printf("Testing %d\n", test(9, 10));
	return 0;
}

int test(int a, int b)
{
	return a + b;
}
