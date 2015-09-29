#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/module.h>

int opened = 0;
int closed = 0;
int usedBuff = 0;
struct module *owner;

static ssize_t dread (struct file *, char __user *, size_t, loff_t *);
static ssize_t dwrite (struct file *, const char *, size_t, loff_t *);
static int dopen (struct inode *, struct file *);
static int drelease (struct inode *, struct file *);
static char device_buffer[2048];

static int dopen (struct inode *inode, struct file *file)
{
	opened++;
	printk(KERN_ALERT "Opened %d times \n", opened);
	return 0;
}
static int drelease (struct inode *inode, struct file *file)
{
	closed++;
	printk(KERN_ALERT "Closed %d times \n", closed);
	return 0;
}

static ssize_t dread (struct file *file, char __user *usbuff, size_t availablespace, loff_t *currentpos){
	copy_to_user(usbuff, device_buffer, availablespace);
	printk(KERN_ALERT "Reading! \n");
	return 0;
}

//Inspired by https://appusajeev.wordpress.com/2011/06/18/writing-a-linux-character-device-driver/
static ssize_t dwrite (struct file *file, const char *usbuff, size_t availablespace, loff_t *currentpos){
	int count = 0;
	copy_from_user(device_buffer+usedBuff, usbuff, availablespace);
	while (usbuff[count++]!= NULL);
	usedBuff += count;
	printk(KERN_ALERT "Successfully written %d bytes \n", count);
	return count;
}

//Inspired by http://www.tldp.org/LDP/lkmpg/2.4/html/c577.htm
struct file_operations fops = {
	.owner   = THIS_MODULE,
	.open    = dopen,
	.release = drelease,
	.read    = dread,
	.write   = dwrite
};

int scd_init(void)
{
	printk(KERN_ALERT "inside %s function\n",__FUNCTION__);
	register_chrdev(777, "dd", &fops);
	return 0;
}

int scd_exit(void)
{
	printk(KERN_ALERT "inside %s function\n",__FUNCTION__);
	unregister_chrdev(777, "dd");
	return 0;
}

module_init(scd_init);
module_exit(scd_exit);