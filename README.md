# CPU Time Monitor for Registered Applications

## Usage:

This kernel module is very simple to use. Users should simply write the pid of the
process they wish to minitor to the `/proc/mp1/status` file. This can be done in a 
terminal with the `echo :pid: > /proc/mp1/status` command where `:pid:` is a valid
process id. Use the `ps` bash command to find the process id of a running process.
Your shell will also print out the pid of a process if it is run in the background.
Users can monitor the cpu usage by reading from the same file. This can be done
in a terminal with `cat /proc/mp1/status`. Note that the kernel module must be 
loaded before registering apps as the file `/proc/mp1/status` will not exist 
if the module is not loaded. 

More Examples:
* terminal code:
    - registration: `echo :pid: > /proc/mp1/status`
    - monitoring: `cat /proc/mp1/status`
* C code:
    - registration: 
    ```c
    char* buf = NULL;
    FILE* f = fopen("/proc/mp1/status", "w"); 
    asprintf(&buf, "%d", getpid());
    fwrite(buf, strlen(buf), 1, f);
    fclose(f);
    free(buf);
    ```
    - monitoring: 
    ```c
	char* lineptr = NULL;
	size_t buf_size = 0;
	int pid = -1;
	FILE* f = fopen("/proc/mp1/status", "r");

	while ( getline(&lineptr, &buf_size, f) != EOF ) {
        // lineptr is of the form "<pid>: <cpu use>\n"
        printf("%s", lineptr);
	}

	free(lineptr);
	fclose(f);
    ```

## Installation:

Install the kernel module with `sudo insmod mp1.ko` and remove the module with `sudo rmmod mp1.ko`.

## Implementation

Under the hood, this module keeps track of registered processes with a kernel list. Every 5 seconds, for each process, the module with either update the CPU use of the process with the CPU use at the time of the update, or the module will remove the process from its internal list if the process is no longer valid (i.e. the process has finished). The module accomplishes this with a timer and a workqueue with a single worker thread. The module exposes an entry in the proc filesystem to allow users to register processes and retrieve CPU use time with write and read callbacks, respectively. 
