#include <linux/clk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>

#include <asm/gpio.h>
#include <asm/ioctl.h>
#include <asm/uaccess.h>
#include <linux/delay.h>

#include <linux/input.h>
#include <linux/debugfs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/gpio.h>

#include <linux/workqueue.h>

#include <mach/clk.h>
#include <mach/board-cardhu-misc.h>

#include "dsp.h"

#define MAX_RETRY (5)

#define DSP_POWER_1V8_EN_GPIO_TF201 TEGRA_GPIO_PU5
#define DSP_POWER_1V8_EN_GPIO_TF201X TEGRA_GPIO_PP3

struct fm34_chip {
	struct i2c_client	*client;

	struct clk *clk_cdev1;
	struct clk *clk_out1;
};

static int fm34_i2c_retry(struct i2c_client *fm34_i2c_client, u8* parameter, size_t size)
{
	int ret, i;

	for (i = 0; i < MAX_RETRY; i++) {
		ret = i2c_master_send(fm34_i2c_client, parameter, size);
		if (ret > 0)
			return 0;

		msleep(5);
	}

	return ret;
}

static void fm34_power_switch(int state)
{
	unsigned dsp_1v8_power_control;
	u32 project_info = tegra3_get_project_id();

	if (project_info == TEGRA3_PROJECT_TF201)
		dsp_1v8_power_control = DSP_POWER_1V8_EN_GPIO_TF201;
	else if (project_info == TEGRA3_PROJECT_TF300T)
		dsp_1v8_power_control = DSP_POWER_1V8_EN_GPIO_TF201X;
	else
		return;

	gpio_set_value(dsp_1v8_power_control, state);
}

static void fm34_reset_DSP(void)
{
	gpio_set_value(TEGRA_GPIO_PO3, 0);
	msleep(10);

	gpio_set_value(TEGRA_GPIO_PO3, 1);
	msleep(100);
}

static int fm34_config_DSP(struct fm34_chip *data)
{
	int ret;
	int input_config_length;
	u8 *input_config_table;

	struct i2c_msg msg[3];
	u8 buf1 = 0xC0;

	fm34_power_switch(1);

	//reset dsp
	fm34_reset_DSP();

	gpio_set_value(TEGRA_GPIO_PBB6, 1); // Enable DSP Bypass
	msleep(20);

	/* Write register */
	msg[0].addr = data->client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &buf1;

	ret = i2c_transfer(data->client->adapter, msg, 1);
	if(ret < 0){
		//FM34_INFO("DSP NOack, Failed to read 0x%x: %d\n", buf1, ret);
		msleep(50);
		fm34_reset_DSP();
		return ret;
	}

	if (tegra3_get_project_id() == TEGRA3_PROJECT_TF700T) {
		FM34_INFO("Load TF700T DSP parameters\n");
		input_config_length = sizeof(input_parameter_TF700T);
		input_config_table = (u8 *)input_parameter_TF700T;
	} else if (tegra3_get_project_id() == TEGRA3_PROJECT_TF201) {
		input_config_length = sizeof(input_parameter_TF201);
		input_config_table = (u8 *)input_parameter_TF201;
	} else {
		FM34_INFO("Load  DSP parameters\n");
		input_config_length= sizeof(input_parameter);
		input_config_table= (u8 *)input_parameter;
	}

	ret = fm34_i2c_retry(data->client, input_config_table, input_config_length);
	FM34_INFO("input_config_length = %d\n", input_config_length);

	msleep(100);
	gpio_set_value(TEGRA_GPIO_PBB6, 0);

	return ret;
}

// taken from tegra_asoc_utils_init
static int fm34_clk_init(struct fm34_chip *data)
{
	struct device *dev = &data->client->dev;
	int ret;

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
	data->clk_cdev1 = clk_get_sys(NULL, "cdev1");
#else
	data->clk_cdev1 = clk_get_sys("extern1", NULL);
#endif
	if (IS_ERR(data->clk_cdev1)) {
		dev_err(dev, "Can't retrieve clk cdev1\n");
		return PTR_ERR(data->clk_cdev1);
	}

	ret = clk_enable(data->clk_cdev1);
	if (ret) {
		dev_err(dev, "Can't enable clk cdev1/extern1");
		clk_put(data->clk_cdev1);
		return ret;
	}

#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
	data->clk_out1 = clk_get_sys("clk_out_1", "extern1");
	if (IS_ERR(data->clk_out1)) {
		dev_err(dev, "Can't retrieve clk out1\n");
		clk_put(data->clk_cdev1);
		return PTR_ERR(data->clk_out1);
	}

	ret = clk_enable(data->clk_out1);
	if (ret) {
		dev_err(dev, "Can't enable clk out1");
		clk_put(data->clk_out1);
		clk_put(data->clk_cdev1);
		return ret;
	}
#endif

	return 0;
}

static int fm34_gpio_init(void)
{
	int rc = 0;

	//config RST# pin, default HIGH.
	rc = gpio_request(TEGRA_GPIO_PO3, "fm34_reset");
	if (rc) {
		FM34_ERR("gpio_request failed for input %d\n", TEGRA_GPIO_PO3);
	}

	rc = gpio_direction_output(TEGRA_GPIO_PO3, 1);
	if (rc) {
		FM34_ERR("gpio_direction_output failed for input %d\n", TEGRA_GPIO_PO3);
	}

	//config PWDN# pin, default HIGH.
	rc = gpio_request(TEGRA_GPIO_PBB6, "fm34_bypass");
	if (rc) {
		FM34_ERR("gpio_request failed for input %d\n", TEGRA_GPIO_PBB6);
	}

	rc = gpio_direction_output(TEGRA_GPIO_PBB6, 0);
	if (rc) {
		FM34_ERR("gpio_direction_output failed for input %d\n", TEGRA_GPIO_PBB6);
	}

	return 0;
}

static void fm34_regulator_get(void)
{
	unsigned dsp_1v8_power_control;
	int ret = 0;
	u32 project_info = tegra3_get_project_id();

	if(project_info == TEGRA3_PROJECT_TF201)
		dsp_1v8_power_control = DSP_POWER_1V8_EN_GPIO_TF201;
	else if(project_info == TEGRA3_PROJECT_TF300T)
		dsp_1v8_power_control = DSP_POWER_1V8_EN_GPIO_TF201X;
	else
		return;

	//Request dsp power 1.8V
	ret = gpio_request(dsp_1v8_power_control, "dsp_power_1v8_en");
	if (ret < 0)
		pr_err("%s: gpio_request failed for gpio %s\n",
			__func__, "DSP_POWER_1V8_EN_GPIO");

	gpio_direction_output(dsp_1v8_power_control, 0);
}

static int fm34_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct fm34_chip *data;
	int ret;

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	i2c_set_clientdata(client, data);
	data->client = client;

	ret = fm34_clk_init(data);
	if (ret)
		return ret;

	//Request dsp power 1.8V
	fm34_regulator_get();

	fm34_gpio_init();

	fm34_config_DSP(data);

	pr_info("%s()\n", __func__);

	return 0;
}

// taken from tegra_asoc_utils_fini
static void fm34_clk_remove(struct fm34_chip *data)
{
#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
	if (!IS_ERR(data->clk_out1))
		clk_put(data->clk_out1);
#endif

	clk_put(data->clk_cdev1);
	/* Just to make sure that clk_cdev1 should turn off in case if it is
	 * switched on by some codec whose hw switch is not registered.*/
	if (tegra_is_clk_enabled(data->clk_cdev1))
		clk_disable(data->clk_cdev1);
}

static int fm34_remove(struct i2c_client *client)
{
	struct fm34_chip *data = i2c_get_clientdata(client);

	fm34_clk_remove(data);
	kfree(data);

	return 0;
}

static int fm34_suspend(struct device *dev)
{
	gpio_set_value(TEGRA_GPIO_PBB6, 0); /* Bypass DSP */

	fm34_power_switch(0);

	return 0;
}

static int fm34_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fm34_chip *data = i2c_get_clientdata(client);

	fm34_config_DSP(data);

	return 0;
}

static const struct i2c_device_id fm34_id[] = {
	{ "dsp_fm34", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fm34_id);

static const struct dev_pm_ops fm34_dev_pm_ops = {
	.suspend	= fm34_suspend,
	.resume		= fm34_resume,
};

static struct i2c_driver fm34_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name	= "dsp_fm34",
		.pm	= &fm34_dev_pm_ops,
	},
	.probe		= fm34_probe,
	.remove		= fm34_remove,
	.id_table	= fm34_id,
};
module_i2c_driver(fm34_driver);

