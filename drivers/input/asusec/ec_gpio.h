#include <mach/irqs.h>
#include <asm-generic/gpio.h>

#define TEGRA_NR_GPIOS		INT_GPIO_NR

#if 0
static inline int gpio_to_irq(unsigned int gpio)
{
	/* SOC gpio */
	if (gpio < TEGRA_NR_GPIOS)
		return INT_GPIO_BASE + gpio;

	/* For non soc gpio, the external peripheral driver need to
	 * provide the implementation */
	return __gpio_to_irq(gpio);
}
#endif /* TODO: fix use of custom gpio_to_irq() */

static inline int irq_to_gpio(unsigned int irq)
{
	/* SOC gpio */
	if ((irq >= INT_GPIO_BASE) && (irq < INT_GPIO_BASE + INT_GPIO_NR))
		return irq - INT_GPIO_BASE;

	/* we don't supply reverse mappings for non-SOC gpios */
	return -EIO;
}

