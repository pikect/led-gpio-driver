#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/kobject.h>
#include <linux/kthread.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("...");
MODULE_DESCRIPTION("Noone knows");
MODULE_VERSION("4.20");

static unsigned int gpioLED = 21;
module_param(gpioLED, uint, S_IRUGO);
MODULE_PARM_DESC(gpioLED, " GPIO LED number (default=21)");

static unsigned int blinkPeriod = 1000; //msleep uses ms
module_param(blinkPeriod, uint, S_IRUGO);
MODULE_PARM_DESC(blinkPeriod, " LED blink period in ms (min=1, default=1000, max=10000)");

static char ledName[7] = "ledXXX";
static bool ledOn = 0;
enum modes { OFF, ON, FLASH };
static enum modes mode = FLASH;


static ssize_t mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   switch(mode){
      case OFF:   return sprintf(buf, "off\n");
      case ON:    return sprintf(buf, "on\n");
      case FLASH: return sprintf(buf, "flash\n");
      default:    return sprintf(buf, "Module Error\n");
   }
}

static ssize_t mode_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count){
   // the count-1 is important as otherwise the \n is used in the comparison
   if (strncmp(buf,"on",count-1)==0) { mode = ON; }
   else if (strncmp(buf,"off",count-1)==0) { mode = OFF; }
   else if (strncmp(buf,"flash",count-1)==0) { mode = FLASH; }
   return count;
}

static ssize_t period_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%d\n", blinkPeriod);
}

static ssize_t period_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count){
   unsigned int period;
   sscanf(buf, "%du", &period);
   if ((period>1)&&(period<=10000)){
      blinkPeriod = period;
   }
   return period;
}

/**
 * Setup kobj_attribute attributes for blink period and blink mode
 */
static struct kobj_attribute period_attr = __ATTR(blinkPeriod, 0664, period_show, period_store);
static struct kobj_attribute mode_attr = __ATTR(mode, 0664, mode_show, mode_store);


static struct attribute *led_attrs[] = {
   &period_attr.attr,
   &mode_attr.attr,
   NULL,
};


static struct attribute_group attr_group = {
   .name  = ledName,
   .attrs = led_attrs,
};

static struct kobject *led_kobj;
static struct task_struct *task;


static int flash(void *arg){
   printk(KERN_INFO "LED: Thread has started running \n");
   while(!kthread_should_stop()){
      set_current_state(TASK_RUNNING);
      if (mode==FLASH) ledOn = !ledOn;
      else if (mode==ON) ledOn = true;
      else ledOn = false;
      gpio_set_value(gpioLED, ledOn);
      set_current_state(TASK_INTERRUPTIBLE);
      msleep(blinkPeriod/2);
   }
   printk(KERN_INFO "LED: Thread has run to completion \n");
   return 0;
}

/** LKM __init macro */
static int __init LED_init(void){
   int result = 0;

   printk(KERN_INFO "LED: Initializing the LED Module\n");
   sprintf(ledName, "led%d", gpioLED);

   led_kobj = kobject_create_and_add("led", kernel_kobj->parent); // kernel_kobj points to /sys/kernel
   if(!led_kobj){
      printk(KERN_ALERT "LED: failed to create kobject\n");
      return -ENOMEM;
   }

   result = sysfs_create_group(led_kobj, &attr_group);
   if(result) {
      printk(KERN_ALERT "LED: failed to create sysfs group\n");
      kobject_put(led_kobj);
      return result;
   }

   ledOn = true;
   gpio_request(gpioLED, "sysfs");
   gpio_direction_output(gpioLED, ledOn);
   gpio_export(gpioLED, false);  //second argument prevents the direction from being changed

   task = kthread_run(flash, NULL, "LED_thread");
   if(IS_ERR(task)){
      printk(KERN_ALERT "LED: failed to create the task\n");
      return PTR_ERR(task);
   }
   return result;
}

/** LKM __exit macro */
static void __exit LED_exit(void){
   kthread_stop(task);
   kobject_put(led_kobj);
   gpio_set_value(gpioLED, 0);
   gpio_unexport(gpioLED);
   gpio_free(gpioLED);
   printk(KERN_INFO "LED: Module exiting\n");
}

module_init(LED_init);
module_exit(LED_exit);