/*
 * OMAP4 SMP source file. It contains platform specific fucntions
 * needed for the linux smp kernel.
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * Author:
 *      Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * Platform file needed for the OMAP4 SMP. This file is based on arm
 * realview smp platform.
 * * Copyright (c) 2002 ARM Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/device.h>
#include <linux/smp.h>
#include <linux/io.h>

#include <asm/cacheflush.h>
#include <asm/localtimer.h>
#include <asm/smp_scu.h>
#include <mach/hardware.h>
#include <mach/omap4-common.h>
#include <plat/clockdomain.h>

/*
 * GIC distibutor disable flag (omap4460)
 */
u32 disable_gd = 1;
/* SCU base address */
void __iomem *scu_base;
/*
 * Use SCU config register to count number of cores
 */
static inline unsigned int get_core_count(void)
{
	if (scu_base)
		return scu_get_core_count(scu_base);
	return 1;
}

static DEFINE_SPINLOCK(boot_lock);

static inline void disable_gic_distributor(void)
{
	writel_relaxed(0x0, gic_dist_base_addr + GIC_DIST_CTRL);
}

void __cpuinit platform_secondary_init(unsigned int cpu)
{
	trace_hardirqs_off();

	/* Enable NS access to SMP bit */
	omap4_secure_dispatcher(PPA_SERVICE_NS_SMP, 4, 0, 0, 0, 0, 0);

	/*
	 * If any interrupts are already enabled for the primary
	 * core (e.g. timer irq), then they will not have been enabled
	 * for us: do so
	 */
	gic_cpu_init(0, gic_cpu_base_addr);

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

int __cpuinit boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	struct clockdomain *cpu1_clkdm;
	static bool booted;
	/*
	 * Set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	/*
	 * Update the AuxCoreBoot0 with boot state for secondary core.
	 * omap_secondary_startup() routine will hold the secondary core till
	 * the AuxCoreBoot1 register is updated with cpu state
	 * A barrier is added to ensure that write buffer is drained
	 */
	omap_modify_auxcoreboot0(0x200, 0xfffffdff);
	flush_cache_all();
	smp_wmb();

	/*
	 * SGI isn't wakeup capable from low power states. This is
	 * known limitation and can be worked around by using software
	 * forced wake-up. After the wakeup, the CPU will restore it
	 * to hw_auto. This code also gets initialised but pm init code
	 * initialises the CPUx clockdomain to hw-auto mode
	 */
	if (booted) {
		cpu1_clkdm = clkdm_lookup("mpu1_clkdm");
		/*
		 * GIC distributor control register has changed between
		 * CortexA9 r1pX and r2pX. The Control Register secure
		 * banked version is now composed of 2 bits:
		 * bit 0 == Secure Enable
		 * bit 1 == Non-Secure Enable
		 * The Non-Secure banked register has not changed
		 * Because the ROM Code is based on the r1pX GIC, the CPU1
		 * GIC restoration will cause a problem to CPU0 Non-Secure SW.
		 * The workaround must be:
		 * 1) Before doing the CPU1 wakeup, CPU0 must disable
		 * the GIC distributor
		 * 2) CPU1 must re-enable the GIC distributor on
		 * it's wakeup path.
		 */
		if (cpu_is_omap446x()) {
			disable_gic_distributor();
			disable_gd = 0;
		}

		omap2_clkdm_wakeup(cpu1_clkdm);
		smp_cross_call(cpumask_of(cpu));
	} else {
		set_event();
		booted = true;
	}

	/*
	 * Now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	return 0;
}

static void __init wakeup_secondary(void)
{
	/*
	 * Write the address of secondary startup routine into the
	 * AuxCoreBoot1 where ROM code will jump and start executing
	 * on secondary core once out of WFE
	 * A barrier is added to ensure that write buffer is drained
	 */
	omap_auxcoreboot_addr(virt_to_phys(omap_secondary_startup));
	smp_wmb();

	/*
	 * Send a 'sev' to wake the secondary core from WFE.
	 * Drain the outstanding writes to memory
	 */
	dsb();
	set_event();
	mb();
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init smp_init_cpus(void)
{
	unsigned int i, ncores;

	/* Never released */
	scu_base = ioremap(OMAP44XX_SCU_BASE, SZ_256);
	BUG_ON(!scu_base);

	ncores = get_core_count();

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);
}

void __init smp_prepare_cpus(unsigned int max_cpus)
{
	unsigned int ncores = get_core_count();
	unsigned int cpu = smp_processor_id();
	int i;

	/* sanity check */
	if (ncores == 0) {
		printk(KERN_ERR
		       "OMAP4: strange core count of 0? Default to 1\n");
		ncores = 1;
	}

	if (ncores > NR_CPUS) {
		printk(KERN_WARNING
		       "OMAP4: no. of cores (%d) greater than configured "
		       "maximum of %d - clipping\n",
		       ncores, NR_CPUS);
		ncores = NR_CPUS;
	}
	smp_store_cpu_info(cpu);

	/*
	 * are we trying to boot more cores than exist?
	 */
	if (max_cpus > ncores)
		max_cpus = ncores;

	/*
	 * Initialise the present map, which describes the set of CPUs
	 * actually populated at the present time.
	 */
	for (i = 0; i < max_cpus; i++)
		set_cpu_present(i, true);

	if (max_cpus > 1) {
		/*
		 * Enable the local timer or broadcast device for the
		 * boot CPU, but only if we have more than one CPU.
		 */
		percpu_timer_setup();

		/*
		 * Initialise the SCU and wake up the secondary core using
		 * wakeup_secondary().
		 */
		scu_enable(scu_base);
		wakeup_secondary();
	}
}
