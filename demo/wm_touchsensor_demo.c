
#include <string.h>
#include "wm_include.h"
#include "wm_demo.h"
#include "wm_touchsensor.h"
#include "wm_regs.h"
#include "wm_irq.h"
#include "wm_cpu.h"
#include "wm_gpio.h"

#if DEMO_TOUCHSENSOR
#define KEY_PRESS_QUEUE_LENGTH  (32)
tls_os_queue_t *key_press_queue = NULL;
#define KEY_PRESS_TASK_STACK  512
u32 key_press_stack[KEY_PRESS_TASK_STACK];

void key_press_callback(u32 status)
{
	if (key_press_queue)
	{
		tls_os_queue_send(key_press_queue,(void *)status,0);
	}
}

void key_press_task_hdl(void)
{
	u32 *msg;
	u32 value = 0;
	int i = 0;
	for(;;)
	{
		tls_os_queue_receive(key_press_queue,(void * *)&msg,0,0);
		value = (u32)msg;
		for (i = 0; i < 15; i++)
		{
			if (value&(1<<i))
			{
				wm_printf("key %d\r\n is pressed\r\n", (i+1));
			}
		}
	}
}

/*test code
You can call this function in function UserMain in main.c and when you touch the pad,you can find uart log printing from function tls_touchsensor_irq_handler.
If the corresponding bit is set, this touch sensor has been touched.
You can register your callback if need.
*/
void key_init(int touchnum)
{
	int i = 0;
	enum tls_io_name touchio[15] = {WM_IO_PA_07, WM_IO_PA_09, WM_IO_PA_10,  /*touch 1-3*/ 
									WM_IO_PB_00, WM_IO_PB_01, WM_IO_PB_02, /*touch 4-6*/ 
									WM_IO_PB_03, WM_IO_PB_04, WM_IO_PB_05, /*touch 7-9*/ 
									WM_IO_PB_06, WM_IO_PB_07, WM_IO_PB_08, /*touch 10-12*/ 
									WM_IO_PB_09, WM_IO_PA_12, WM_IO_PA_14};/*touch 13-15*/ 

	/*deinit all touch sensor*/
	for (i = 15; i > 0; i--)
	{
		tls_touchsensor_deinit(i);
		tls_gpio_cfg(touchio[i-1],WM_GPIO_DIR_OUTPUT, WM_GPIO_ATTR_FLOATING);
	}

	for (i = 0; i < 15; i++)
	{
		if ((touchnum == 0) || (touchnum&(1<<i)))
		{
			/*1)configure touch sensor IO multiplex.*/
			wm_touch_sensor_config(touchio[i]); 

			/*2)set threshold*/
			tls_touchsensor_threshold_config(i+1, 120);

			/*3)init touch sensor's parameter, all touch sensor only can use the same configure parameter.*/
			tls_touchsensor_init_config(i+1,16,16,1);

			/*4)enable touch sensor's irq , only enable specified irq bit, you can find the pad is touched by irq.*/
			tls_touchsensor_irq_enable(i+1);
		}
	}
		
}

void key_press_init(int touchnum)
{
	if (key_press_queue == NULL)
	{
		tls_os_queue_create(&key_press_queue,KEY_PRESS_QUEUE_LENGTH);
		tls_os_task_create(NULL,NULL,key_press_task_hdl,NULL, &key_press_stack[0],KEY_PRESS_TASK_STACK*4,50,0);
	}

	tls_touchsensor_irq_register(key_press_callback);
	key_init(touchnum);
}

int demo_touchsensor(int touchnum)
{
	if ((touchnum <= 65535) && ((touchnum == 0) || (touchnum & 1)))
	{
		wm_printf("test touch:%x\r\n", touchnum);		
		key_press_init(touchnum);
	}
	else
	{
		wm_printf("Usage: t-touch(0),t-touch(1),t-touch(3),t-touch(5)...t-touch(65535)\r\n");
		wm_printf(" 1)If you enable only one touch sensor, then touch sensor 1 must be used firstly.\r\n");
		wm_printf(" 2)You can set touchnum == 0 and enable all touch sensor.\r\n");			
	}
	return 0;
}

#endif

