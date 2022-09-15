#include "userapp.h"

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// This number of factorials should take 10-15 seconds
#define NUM_FACTORIALS 45

size_t factorial(size_t n) {
	if (n == 0) {
		return 0;
	} else if (n == 1) {
		return 1;
	} else {
		return factorial(n - 1) + factorial(n - 2);
	}
}

void register_application(void) {
	char cmd_buffer[32] = { 0 };

	sprintf(cmd_buffer, "echo %d > /proc/mp1/status", getpid());
	system(cmd_buffer);
}

int main(int argc, char* argv[]) {
	register_application();
	
	for (size_t i = 0; i <= NUM_FACTORIALS; ++i) {
		printf("factorial(%zu) = %zu\n", i, factorial(i));
	}
}
