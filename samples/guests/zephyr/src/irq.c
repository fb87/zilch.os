#include <zephyr/arch/cpu.h>

void z_soc_irq_init(void)
{
}

void z_soc_irq_enable(unsigned int irq)
{
    (void)irq;
}

void z_soc_irq_disable(unsigned int irq)
{
    (void)irq;
}

int z_soc_irq_is_enabled(unsigned int irq)
{
    (void)irq;
    return 0;
}

void z_soc_irq_priority_set(unsigned int irq, unsigned int priority, unsigned int flags)
{
    (void)irq;
    (void)priority;
    (void)flags;
}

unsigned int z_soc_irq_get_active(void)
{
    return 1023U;
}

void z_soc_irq_eoi(unsigned int irq)
{
    (void)irq;
}
