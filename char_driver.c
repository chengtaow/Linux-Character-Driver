#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/spinlock_types.h>
#include <linux/sched.h>

#define DEVICE "/dev/mycdrv"

#define ramdisk_size (size_t) (16 * PAGE_SIZE)
//data access directions
#define REGULAR 0
#define REVERSE 1

//NUM_DEVICES defaults to 3 unless specified during insmod
static int NUM_DEVICES = 3;
module_param(NUM_DEVICES, int, S_IRUGO);//set parameters

#define CDRV_IOC_MAGIC 'Z'
#define ASP_CHGACCDIR _IOW(CDRV_IOC_MAGIC, 1, int)

struct ASP_mycdrv {
	struct list_head list;
	struct cdev dev;
	char *ramdisk;
	struct semaphore sem;
	int mode;
};

/*struct ASP_mycdrv {
	struct cdev dev;
	struct semaphore sem;
	int length;
	int direction;
	unsigned char buffer[50];
};*/
static dev_t devno;
static struct ASP_mycdrv *ASP_mycdrv;
struct class *cls;


static int cdrv_open(struct inode *inode, struct file *filp)
{
	struct ASP_mycdrv *mycdrv;
	mycdrv = container_of(inode->i_cdev,struct ASP_mycdrv,dev);
	filp->private_data = mycdrv;
	down_interruptible(&mycdrv ->sem);
	mycdrv -> mode = REGULAR;
	up(&mycdrv -> sem);
	return 0;
}

static int cdrv_release(struct inode *inode,struct file *filp)
{
	struct ASP_mycdrv *mycdrv;
	mycdrv = container_of(inode->i_cdev,struct ASP_mycdrv,dev);
	filp->private_data = mycdrv;
	return 0;
}


static ssize_t cdrv_read(struct file *filp, char __user *buf, size_t lbuf, loff_t *ppos)
{
	int nbytes = 0;
	struct ASP_mycdrv *mycdrv = filp->private_data;
	down_interruptible(&mycdrv ->sem);
	if ((lbuf + *ppos) > ramdisk_size) {
		pr_info("trying to read past end of device,"
		"aborting because this is just a stub!\n");
	return 0;
	}
	if(mycdrv->mode == REGULAR){
		nbytes = lbuf - copy_to_user(buf, mycdrv->ramdisk + *ppos, lbuf);
		*ppos += nbytes;
	}
	else if(mycdrv->mode == REVERSE){
		nbytes = lbuf - copy_to_user(buf, mycdrv->ramdisk + *ppos - lbuf, lbuf);
		*ppos -= nbytes;
	}
	pr_info("\n READING function, nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
	up(&mycdrv -> sem);
	return nbytes;
}


static ssize_t cdrv_write(struct file *filp, const char __user *buf, size_t lbuf, loff_t *ppos)
{
	int nbytes = 0, i;
	struct ASP_mycdrv *mycdrv = filp->private_data;
	down_interruptible(&mycdrv ->sem);
	if ((lbuf + *ppos) > ramdisk_size) {
	pr_info("trying to read past end of device,"
		"aborting because this is just a stub!\n");
	up(&mycdrv -> sem);
	return 0;
	}
	if(mycdrv->mode == REGULAR){
		pr_info("haha.\n");
		nbytes = lbuf - copy_from_user(mycdrv->ramdisk + *ppos, buf, lbuf);
		*ppos += nbytes;
	}
	else if(mycdrv->mode == REVERSE){
		char sbuf[lbuf];
		for(i=0;i<lbuf;i++){
			sbuf[i] = buf[lbuf-i];
		}
		nbytes = lbuf - copy_from_user(mycdrv->ramdisk + *ppos - lbuf, sbuf, lbuf);
		*ppos -= nbytes;
	}
	
	pr_info("\n WRITING function, nbytes=%d, pos=%d, *ppos: %lld\n", nbytes, (int)*ppos, *ppos);
	up(&mycdrv -> sem);
	return nbytes;
}

static loff_t cdrv_llseek(struct file *filp,loff_t offset,int orig)
{
	loff_t testpos = 0;
	struct ASP_mycdrv *mycdrv = filp->private_data;
	down_interruptible(&mycdrv ->sem);
	switch(orig){
		case 0: // SEEKSET 
			testpos = offset;
			break;
		case 1: // SEEKCUR
			testpos = filp ->f_pos + offset;
			break;
		case 2: // SEEKEND
			testpos = ramdisk_size + offset;
			break;
		default: 
			return -EINVAL;
	}
	testpos = testpos < ramdisk_size ? testpos : ramdisk_size;
	testpos = testpos >= 0 ? testpos : 0;
	filp->f_pos = testpos;
	pr_info("Seeking to pos=%ld\n", (long)testpos);
	up(&mycdrv -> sem);
	return testpos;
}

long cdrv_unlocked_ioctl(struct file *filp, unsigned int cmd,unsigned long arg)
{
	struct ASP_mycdrv *mycdrv = filp->private_data;
	int retval = 0;
	if (_IOC_TYPE(cmd) != CDRV_IOC_MAGIC) {
		pr_info("Invalid magic number\n");
		return -ENOTTY;
	}
	if ( !((arg == REGULAR) or ( arg == REVERSE) )) {
		pr_info("Invalid cmd\n");
		return -ENOTTY;
	}
	down_interruptible(&mycdrv ->sem);
	switch(cmd){
		case ASP_CHGACCDIR:
			pr_info("Ha.\n");
			if (arg == 0){
                        	mycdrv -> mode = 0;
				pr_info("Haha.\n");
				break;
			}
			if (arg == 1){
				mycdrv -> mode = 1;
                        	break;
			}
		default:
			return -EFAULT;
	}
	up(&mycdrv -> sem);
	return retval;
}


static const struct file_operations fifo_operations = {
	.owner = THIS_MODULE,
	.open = cdrv_open,
	.read = cdrv_read,
	.write = cdrv_write,
	.llseek = cdrv_llseek,
	.unlocked_ioctl = cdrv_unlocked_ioctl,
	.release = cdrv_release,
};


int __init cdrv_init(void)
{
	int i, n, retval;
	ASP_mycdrv = kmalloc(NUM_DEVICES * sizeof(struct ASP_mycdrv), GFP_KERNEL);
	memset(ASP_mycdrv, 0, NUM_DEVICES * sizeof(struct ASP_mycdrv));	
	if(!ASP_mycdrv)
		return -ENOMEM;
	retval = alloc_chrdev_region(&devno,0,NUM_DEVICES,DEVICE);
	if(retval < 0){
		printk("Fail to register device!\n");
		goto err_register_chrdev_region;
	}


	cls = class_create(THIS_MODULE, DEVICE);
	if(IS_ERR(cls)){
		retval = PTR_ERR(cls);
		goto err_class_create;
	}
	printk("%d devices have been created!\n",NUM_DEVICES);

	for(n = 0;n < NUM_DEVICES;n ++){
		cdev_init(&ASP_mycdrv[n].dev, &fifo_operations);
		retval = cdev_add(&ASP_mycdrv[n].dev, devno + n, 1);
		if (retval < 0)
			goto err_cdev_add;
		sema_init(&ASP_mycdrv[n].sem, 1);
		ASP_mycdrv[n].ramdisk = kmalloc(ramdisk_size, GFP_KERNEL);
		memset(ASP_mycdrv[n].ramdisk, 0, ramdisk_size);
		ASP_mycdrv[n].mode = REGULAR;
		device_create(cls, NULL, devno + n, NULL, "mycdrv%d",n);

	}
	printk("mycdrv has been registrated to system!\n");
	return 0;


	err_register_chrdev_region:
		return retval;

	err_class_create:
		unregister_chrdev_region(devno, NUM_DEVICES);
		return retval;
	
	err_cdev_add:
		for(i = 0;i < n;i ++){
			cdev_del(&ASP_mycdrv[i].dev);
		}
		return retval;
}

void __exit cdrv_exit(void)
{
	int i;
	for(i = 0;i < NUM_DEVICES;i ++){
		device_destroy(cls, devno + i);
	}

	class_destroy(cls);

	for(i = 0;i < NUM_DEVICES;i ++){
		cdev_del(&ASP_mycdrv[i].dev);
	}
	
	kfree(ASP_mycdrv);
	unregister_chrdev_region(devno, NUM_DEVICES);

	return;
}

module_init(cdrv_init);
module_exit(cdrv_exit);

MODULE_AUTHOR("Chengtao Wang");
MODULE_LICENSE("GPL v2");
