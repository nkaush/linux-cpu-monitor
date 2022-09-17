#include "userapp.h"

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// This number of fibonaccis should take 10-15 seconds to compute
#define NUM_FACTORIALS 45

size_t factorial(size_t n) {
	if (n == 0) {
		return 1;
	} else {
		return n * factorial(n - 1);
	}
}

size_t fibonacci(size_t n) {
	if (n == 0) {
		return 0;
	} else if (n == 1) {
		return 1;
	} else {
		return fibonacci(n - 1) + fibonacci(n - 2);
	}
}

void register_application(void) {
	char cmd_buffer[32];
	memset(cmd_buffer, 0, sizeof(cmd_buffer));

	sprintf(cmd_buffer, "echo %d > /proc/mp1/status", getpid());

	printf("Registering application with: `%s`\n", cmd_buffer);
	system(cmd_buffer);
}

void read_cpu_time(void) {
	char* colon_ptr = NULL;
	char* lineptr = NULL;
	size_t buf_size = 0;
	int pid = -1;

	FILE* f = fopen("/proc/mp1/status", "r");

	while ( getline(&lineptr, &buf_size, f) != EOF ) {
		colon_ptr = strstr(lineptr, ":");
		*colon_ptr = '\0';
		sscanf(lineptr, "%d", &pid);
		*colon_ptr = ':';

		// if we can parse the pid and the pid is that of this app, print in color...
		if ( pid == getpid() ) {
			printf("\033[1m\033[33m%s\033[0m", lineptr);
		} else {
			printf("%s", lineptr);
		}
	}

	free(lineptr);
	fclose(f);
}

int main(int argc, char* argv[]) {
	register_application();
	char* fn_name = "factorial";
	size_t (*fn)(size_t) = factorial;

	if ( argc > 1 && !strcmp(argv[1], "fibonacci") ) {
		fn = fibonacci;
		fn_name = "fibonacci";
	}
	
	for (size_t i = 0; i <= NUM_FACTORIALS; ++i) {
		printf("%s(%zu) = %zu\n", fn_name, i, fn(i));
	}

	read_cpu_time();
}
