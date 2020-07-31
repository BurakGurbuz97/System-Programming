#include <linux/module.h>
#include <linux/init.h>      	
#include <linux/fs.h>         
#include <linux/uaccess.h>	
#include <linux/slab.h>		
#include <linux/cdev.h>		
#include <linux/device.h>
#include <linux/ioctl.h>

#define MAGIC 5813
#define MMIND_REMAINING _IO(MAGIC, 0)
#define MMIND_ENDGAME   _IO(MAGIC, 1)
#define MMIND_NEWGAME   _IOWR(MAGIC, 2, int)
#define MAXNR 2
#define MY_CHARDEV_NAME "Master Mind"
#define MEM_SIZE 4096

// to create device node
//sudo mknod -m 666 /dev/mastermind c 142 0


static char *memory = NULL;
static char *guess = NULL;
static char *result = NULL;
static char *mmind_number = NULL;
static int mmind_max_guesses = 10;
static int write_count = 0;

static dev_t first_dev = { 0 };
static unsigned int minor_count = 1;
static int major_number = 0, minor_number = 0;	
static struct cdev *my_char_dev = NULL;

module_param(major_number, int, S_IRUGO);
module_param(mmind_max_guesses, int, S_IRUGO);
module_param(mmind_number, charp , S_IRUGO);

static void toString(int number, char* numberArray){
	int i;
    for (i = 3; i > -1 ; i--) {
    	numberArray[i] = number % 10 + '0';
    	number = number / 10;
    }
}


//Assumes result_len = 16 
static void  evaluate(char* guess, char* secret, char* result) {
	int i = 0;
	int j = 0;
	int in_place = 0;
	int out_place = 0;
	int num = write_count + 1;

	for(; i < 4 ; i++){
		if (guess[i] == secret[i]) {
			in_place++;
			continue;
		} else {
			j = 0;
			for(; j < 4; j++) {
				if(guess[i] == secret[j]) {
					out_place++;
					break;
				}
			}
		}
	}


	for(i = 0; i < 4; i++)
		result[i] = guess[i];
	result[4] = ' ';
	result[5] = in_place + '0';
	result[6] = '+';
	result[7] = ' ';
	result[8] = out_place + '0';
	result[9] = '-';
	result[10] = ' ';

    result[14] = num % 10 + '0';
    num = num / 10;
    result[13] = num % 10 + '0';
    num = num / 10;
    result[12] = num % 10 + '0';
    num = num / 10;
    result[11] = num % 10 + '0';
    num = num / 10;
	result[15] = '\n';
}


static int my_dev_open(struct inode *inode, struct file *file)
{
	pr_info("Opening the device\n");
	pr_info("Major number = %u, Minor number: %u\n", imajor(inode), iminor(inode));
	return 0;
} 

static int my_dev_close(struct inode *inode, struct file * file)
{
	pr_info( "Closing the device\n");
	return 0;
}


static ssize_t my_dev_read
(struct file *file, char __user *buf, size_t num, loff_t *pos)
{
	ssize_t ret = -1;


	if(*pos >= (write_count * 16)) {
		return 0;
	}

	if (num > MEM_SIZE) {
		num = MEM_SIZE;
	}


	pr_info("Copying: \"%s\" to user-space\n", memory);

	ret = (ssize_t)copy_to_user(buf, memory, write_count * 16);

	pr_info("We wrote %d bytes to user-space\n", (int)(write_count * 16));

	*pos = *pos + (write_count * 16);
	return  write_count * 16;
}

static ssize_t my_dev_write
	(struct file *file, const char __user *buf, size_t num, loff_t *pos)
{
	int index = 0;
	int i;
	ssize_t ret = -1;

	if(num > MEM_SIZE) {
		pr_alert("invalid write length\n");
		return ret;
	}

	if(write_count == mmind_max_guesses) {
		pr_alert("sorry you ran out of guesses :/\n");
		return ret;
	}


	pr_info("starting computation\n");
	copy_from_user(guess, buf, num);

	evaluate(guess,mmind_number, result);
	pr_alert("%s\n", result);
	
	
	for(i = 16 * write_count; i < 16 * (write_count + 1); i++) {
		memory[i] = result[index];
		index++;
	}

	write_count++;
	return num;
} 

static long ioctl(struct file* file, unsigned int cmd, unsigned long arg) {
	
	switch (cmd) {
		case MMIND_REMAINING:
			return mmind_max_guesses - write_count;

		case MMIND_ENDGAME:
			write_count = 0;
			break;

		case MMIND_NEWGAME:
			pr_info("USER starts new game: %d", arg);
			toString(arg, mmind_number);
			write_count = 0;
			break;

		default:
			pr_alert("Invalid ioctl;");
			return -ENOTTY;
	}

	return 0;

}

//Set the file operations for the device driver
static const struct file_operations my_char_dev_fops = {
	.owner = THIS_MODULE,
	.open  = my_dev_open,
	.release = my_dev_close,
	.read  = my_dev_read,
	.write = my_dev_write,
	.unlocked_ioctl = ioctl,

 };

static int __init my_init(void)
{
    int ret_val = -1;
	pr_info("Allocating, Setting, and Registering char_dev\n");

	memory    = kmalloc(MEM_SIZE, GFP_KERNEL);
	guess     = kmalloc(sizeof(char) * 5, GFP_KERNEL);
	result    = kmalloc(sizeof(char) * 16, GFP_KERNEL);
	mmind_number = kmalloc(sizeof(char) * 5, GFP_KERNEL);
	strcpy(mmind_number, "4283");

	if (major_number) {
        first_dev = MKDEV(major_number, minor_number);
        ret_val = register_chrdev_region(first_dev, minor_count, MY_CHARDEV_NAME);
    } else {
        ret_val = alloc_chrdev_region(&first_dev, minor_number, minor_count, MY_CHARDEV_NAME);
        major_number = MAJOR(first_dev);
        minor_number = MINOR(first_dev);
    }


	if (0 != ret_val) {
		pr_emerg("Failed to register major and minor numbers!!\n");
		return -1;
	}

	//allocate cdev_t
	my_char_dev = cdev_alloc();

	if (NULL == my_char_dev) {
		pr_emerg("Failed to allocate character device!!\n");
		return -1;
	}

	//initialize the device
	cdev_init(my_char_dev, &my_char_dev_fops);

	//make the device work
	ret_val = cdev_add(my_char_dev, first_dev, minor_count);
	if (0 > ret_val) {
		pr_emerg("Failed to run the device	!!\n");
		return -1;
	}

	pr_info("Finished allocating resources and registering device\n");
	pr_info("Major number is: %d, and number of minor numbers is: %d\n", 
		major_number, minor_count);

	return 0;
}

static void __exit my_exit(void)
{
	pr_info("Deallocating and releasing char_dev\n");

	//release the device
	cdev_del(my_char_dev);

	//unregister the dev_t
	unregister_chrdev_region(first_dev, minor_count);
	kfree(memory);
	kfree(guess);
	kfree(result);
	
	pr_info("Finished deallocating char_dev and releasing resources\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Master Mind INC.");
MODULE_LICENSE("GPL v2");	
