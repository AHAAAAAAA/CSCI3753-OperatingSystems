#include <linux/kernel.h>
#include <linux/linkage.h>
asmlinkage long sys_Simple_add(int number1, int number2, int *result)
{
   *result=number1+number2;
   printk(KERN_ALERT "Adding %d to %d is %d \n", number1, number2, *result);

 return 0; 
}
