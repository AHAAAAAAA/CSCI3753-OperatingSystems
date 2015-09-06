Lab1
=============
Contact: Ahmed.Almutawa@colorado.edu

```
arch/x86/kernel/Simple_add.c
arch/x86/kernel/Makefile
arch/x86/syscalls/syscall_64.tbl
include/linux/syscalls.h
/var/log/syslog
/home/Users/(Username)/Desktop/testfile.c
```

* **Makefile**: Contains instructions on how to compile and build the system calls and kernel.
* **Simple_add.c**: Our systemcall program that adds two numbers together and does a KERNALERT.
* **syscall_64.tbl**: System call table, we added our new system call to this.
* **syscalls.h**: Header file that links syscalls to our test programs.
* **testfile.c**: Our test program to test the new syscalls.
* **syslog**: System log.

To run, just place the files according to the build tree above, compile the kernel in the terminal with:
```
sudo make -j2 CC="ccache gcc"
sudo make -j2 modules_install
sudo make -j2 install
```
then reboot into the new compiled kernel (Google this). Then just compile the testfile.c in the terminal with 
```
gcc testfile.c
```
and run the output
```
./a.out
```
