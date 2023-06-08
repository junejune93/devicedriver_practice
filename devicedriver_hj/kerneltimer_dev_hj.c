#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/fs.h>          
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/errno.h>       
#include <linux/types.h>       
#include <linux/sched.h>       
#include <linux/fcntl.h>       
#include <linux/gpio.h>
#include <linux/slab.h>
#include "kerneltimer_dev_hj.h"

#define   LEDKEY_DEV_NAME            "keyled_dev_hj"
#define   LEDKEY_DEV_MAJOR            240      
#define DEBUG 1
#define IMX_GPIO_NR(bank, nr)       (((bank) - 1) * 32 + (nr))

DECLARE_WAIT_QUEUE_HEAD(WaitQueue_Read);

static int sw_irq[8] = {0,};
static char sw_no = 0;

typedef struct{
	struct timer_list timer;
	unsigned long led;
	int time_val;
} __attribute__ ((packed)) KERNEL_TIMER_MANAGER;

void kerneltimer_registertimer(KERNEL_TIMER_MANAGER*,unsigned long);

static int led[] = {
	IMX_GPIO_NR(1, 16),   //16
	IMX_GPIO_NR(1, 17),	  //17
	IMX_GPIO_NR(1, 18),   //18
	IMX_GPIO_NR(1, 19),   //19
};

static int key[] = {
	IMX_GPIO_NR(1, 20),
	IMX_GPIO_NR(1, 21),
	IMX_GPIO_NR(4, 8),
	IMX_GPIO_NR(4, 9),
	IMX_GPIO_NR(4, 5),
	IMX_GPIO_NR(7, 13),
	IMX_GPIO_NR(1, 7),
	IMX_GPIO_NR(1, 8),
};
irqreturn_t sw_isr(int irq, void* dev_id){
	int i;
//	KERNEL_TIMER_MANAGER* pSTimer = (KERNEL_TIMER_MANAGER*)dev_id;
	sw_no = 0;
	for(i=0; i<ARRAY_SIZE(key);i++){
		if(irq == sw_irq[i]){
			sw_no = i+1;
			break;
		}
	}   
	printk("IRQ : %d, SW_NO : %d\n",irq,sw_no);
	wake_up_interruptible(&WaitQueue_Read);
	return IRQ_HANDLED;
}
static int led_init(void)
{
	int ret = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(led); i++) {
		ret = gpio_request(led[i], "gpio led");
		if(ret<0){
			printk("#### FAILED Request gpio %d. error : %d \n", led[i], ret);
		} 
		else
			gpio_direction_output(led[i], 0);  //0:led off
	}
	return ret;
}
static int kkey_init(void)
{
	int ret = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(key); i++) {
		sw_irq[i] = gpio_to_irq(key[i]);
	}
return ret;
}
static void led_exit(void)
{
  int i;
  for (i = 0; i < ARRAY_SIZE(led); i++){
	  gpio_free(led[i]);
  }
 }
static void key_exit(void)
{
  int i;
  for (i = 0; i < ARRAY_SIZE(key); i++){
	  gpio_free(key[i]);
  }
}
static unsigned int ledkey_poll(struct file * filp, poll_table * wait){
	unsigned int mask = 0;
	printk("_key : %ld/n",(wait->_key & POLLIN));
	if(wait->_key & POLLIN){
		poll_wait(filp,&WaitQueue_Read,wait);
	}   
	if(sw_no > 0){ 
		mask = POLLIN;
	}   
	return mask;
}
int irq_init(struct file * filp){

	int i;
	int result=0;
	char* sw_name[8]={"key1","key2","key3","key4","key5","key6","key7","key8"};
	result = kkey_init();
	if(result < 0){
		return result;
	}
	for(i = 0; i < ARRAY_SIZE(sw_irq);i++){
		result = request_irq(sw_irq[i],sw_isr,IRQF_TRIGGER_RISING,sw_name[i],NULL);
		if(result){
			printk("#### FAILED Request IRQ %d, error : %d\n",sw_irq[i],result);
			break;
		}
	}
	return result;
}

static void led_write(char data)
{
	int i;
	for(i = 0; i < ARRAY_SIZE(led); i++){
		gpio_set_value(led[i], (data >> i ) & 0x01);
	}
#if DEBUG
	printk("#### %s, data = %d\n", __FUNCTION__, data);
#endif
}
void kerneltimer_timeover(unsigned long arg)
{
	KERNEL_TIMER_MANAGER* pdata = NULL;
	if( arg )
	{
		pdata = ( KERNEL_TIMER_MANAGER *)arg;
		led_write(pdata->led & 0x0f);
#if DEBUG
		printk("led : %#04x\n",(unsigned int)(pdata->led & 0x0000000f));
#endif
		pdata->led = ~(pdata->led);
		kerneltimer_registertimer( pdata, pdata->time_val);
	}
}
void kerneltimer_registertimer(KERNEL_TIMER_MANAGER *pdata, unsigned long timeover)
{
	init_timer( &(pdata->timer) );
	pdata->timer.expires = get_jiffies_64() + timeover;  //10ms *100 = 1sec
	pdata->timer.data    = (unsigned long)pdata ;
	pdata->timer.function = kerneltimer_timeover;
	add_timer( &(pdata->timer) );
}
static void key_read(char * key_data)
{
	int i;
	char data=0;
	for(i=0;i<ARRAY_SIZE(key);i++)
	{
		if(gpio_get_value(key[i]))
		{
			data = i+1;
			break;
		}
	}
#if DEBUG
	printk("#### %s, data = %d\n", __FUNCTION__, data);
#endif
	*key_data = data;
	return;
}
static int ledkey_open (struct inode *inode, struct file *filp)
{
	int num0 = MAJOR(inode->i_rdev); 
	int num1 = MINOR(inode->i_rdev); 
	KERNEL_TIMER_MANAGER* pSTimer;
	pSTimer = (KERNEL_TIMER_MANAGER*)kmalloc(sizeof(KERNEL_TIMER_MANAGER),GFP_KERNEL);
	printk( "call open -> major : %d\n", num0 );
	printk( "call open -> minor : %d\n", num1 );

	memset(pSTimer,0x00,sizeof(KERNEL_TIMER_MANAGER));
	filp->private_data = (void*)pSTimer;
	kerneltimer_registertimer(pSTimer,pSTimer->time_val);
	irq_init(filp);
	return 0;
}

static ssize_t ledkey_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	int ret;
	if(!(filp->f_flags & O_NONBLOCK)){
		if(sw_no == 0)
			interruptible_sleep_on(&WaitQueue_Read);
	}
	key_read(&sw_no);
	ret = copy_to_user(buf,&sw_no,count);
	sw_no = 0;
	if(ret < 0)
		return -ENOMEM;
	return count;
}

static ssize_t ledkey_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	KERNEL_TIMER_MANAGER* pSTimer =(KERNEL_TIMER_MANAGER*)filp->private_data;	
	int ret;
	//	get_user(kbuf,buf);
	ret = copy_from_user(&pSTimer->led,buf,count);
	led_write(pSTimer->led);
	return count;
	//	return -EFAULT;
}
static void ledkey_free(void){
	int i;
	for(i=0;i<ARRAY_SIZE(led);i++){
		gpio_free(led[i]);
	}
	for(i=0;i<ARRAY_SIZE(key); i++){
		free_irq(sw_irq[i],NULL);
	}
}
static long ledkey_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{

	keyled_data ctrl_info;
	KERNEL_TIMER_MANAGER* pSTimer = (KERNEL_TIMER_MANAGER*)filp->private_data;
	int err, size,result;
	if( _IOC_TYPE( cmd ) != TIMER_MAGIC ) return -EINVAL;
	if( _IOC_NR( cmd ) >= TIMER_MAXNR ) return -EINVAL;

	size = _IOC_SIZE( cmd );
	if( size )
	{
		err = 0;
		if( _IOC_DIR( cmd ) & _IOC_READ )
			err = access_ok( VERIFY_WRITE, (void *) arg, size );
		else if( _IOC_DIR( cmd ) & _IOC_WRITE )
			err = access_ok( VERIFY_READ , (void *) arg, size );
		if( !err ) return err;
	}
	switch( cmd )
	{
		case TIMER_START :
			if(!timer_pending(&(pSTimer->timer)))
				add_timer(&(pSTimer->timer));	
			break;
		case TIMER_STOP :
			if(timer_pending(&(pSTimer->timer)))
				del_timer(&(pSTimer->timer));
			break;
		case TIMER_VALUE :
			result = copy_from_user(&ctrl_info,(void*)arg,size);
			pSTimer->time_val = (int)ctrl_info.timer_val;
			break;
		default:
			err =-E2BIG;
			break;
	}	
	return err;
}

static int ledkey_release (struct inode *inode, struct file *filp)
{
	KERNEL_TIMER_MANAGER* pSTimer = (KERNEL_TIMER_MANAGER*)filp->private_data;
	printk( "call release \n" );
	led_write(0);
	ledkey_free();
	if(timer_pending(&(pSTimer->timer)))
		del_timer(&(pSTimer->timer));
	if(pSTimer != NULL){
		kfree(pSTimer);
	}

	return 0;
}

struct file_operations ledkey_fops =
{
	.owner    = THIS_MODULE,
	.read     = ledkey_read,     
	.write    = ledkey_write,    
	.unlocked_ioctl = ledkey_ioctl,
	.open     = ledkey_open,     
	.release  = ledkey_release,
	.poll = ledkey_poll,
};

static int ledkey_init(void)
{
	int result;

	printk( "call ledkey_init \n" );    

	result = register_chrdev( LEDKEY_DEV_MAJOR, LEDKEY_DEV_NAME, &ledkey_fops);
	if (result < 0) return result;

	led_init();
	kkey_init();
	return 0;
}

static void ledkey_exit(void)
{
	printk( "call ledkey_exit \n" );    
	unregister_chrdev( LEDKEY_DEV_MAJOR, LEDKEY_DEV_NAME );
	led_exit();
	key_exit();
}

module_init(ledkey_init);
module_exit(ledkey_exit);

MODULE_LICENSE("Dual BSD/GPL");
