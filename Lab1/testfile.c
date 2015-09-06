#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h> 
#include <stdio.h>
	
int main(){
printf("Test function! \n");
 int result;
 int number1=1, number2=2;
 syscall(319, number1, number2, &result);
 printf("%d + %d = %d\n", number1, number2, result);
return 0;
}
