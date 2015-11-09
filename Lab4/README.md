####Installation:
Compile in Linux using 'make' within the directory.

####Usage:
Preferred:
Run
>$ mkdir tests_output
>$ ./testscript

This testscript simulates all 27 combinations of our schedulers, # of iterations and process types then stores thoses results in CSV within the appropriate directory.

Syntax:
>./test-sched <SCHEDULER> <# of ITERATIONS> <PROCESSTYPE>

<SCHEDULER>: SCHED_FIFO, SCHED_RR, SCHED_OTHER (DEFAULT)
<# OF ITERATIONS>: LOW (DEFAULT), MEDIUM, HIGH
<PROCESSTYPE>: CPU, IO, MIXED

