/*
 * OMAP4 Power Management Routines
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Rajendra Nayak <rnayak@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>

#include <plat/powerdomain.h>
#include <plat/clockdomain.h>
#include <plat/serial.h>
#include <plat/common.h>
#include <plat/usb.h>
#include <plat/display.h>
#include <plat/smartreflex.h>
#include <plat/voltage.h>
#include <plat/prcm.h>
#include <plat/dma.h>
#include <mach/omap4-common.h>
#include <mach/omap4-wakeupgen.h>

#include "mach/omap_hsi.h" 


#include "prm.h"
#include "pm.h"
#include "cm.h"
#include "cm-regbits-44xx.h"
#include "prm-regbits-44xx.h"
#include "clock.h"

void *so_ram_address;

struct power_state {
	struct powerdomain *pwrdm;
	u32 next_state;
#ifdef CONFIG_SUSPEND
	u32 saved_state;
	u32 saved_logic_state;
#endif
	struct list_head node;
};

static LIST_HEAD(pwrst_list);
static struct powerdomain *mpu_pwrdm;


static struct powerdomain *mpu_pwrdm, *cpu0_pwrdm, *cpu1_pwrdm;

static struct powerdomain *core_pwrdm, *per_pwrdm;

static struct voltagedomain *vdd_mpu, *vdd_iva, *vdd_core;

#define MAX_IOPAD_LATCH_TIME 1000

/**
 * omap4_device_off_set_state -
 * When Device OFF is enabled, Device is allowed to perform
 * transition to off mode as soon as all power domains in MPU, IVA
 * and CORE voltage are in OFF or OSWR state (open switch retention)
 */
void omap4_device_off_set_state(u8 enable)
{
	if (enable)
		prm_write_mod_reg(0x1 << OMAP4430_DEVICE_OFF_ENABLE_SHIFT,
		OMAP4430_PRM_DEVICE_MOD, OMAP4_PRM_DEVICE_OFF_CTRL_OFFSET);
	else
		prm_write_mod_reg(0x0 << OMAP4430_DEVICE_OFF_ENABLE_SHIFT,
		OMAP4430_PRM_DEVICE_MOD, OMAP4_PRM_DEVICE_OFF_CTRL_OFFSET);
}

/**
 * omap4_device_off_read_prev_state:
 * returns 1 if the device hit OFF mode
 * This is API to check whether OMAP is waking up from device OFF mode.
 * There is no other status bit available for SW to read whether last state
 * entered was device OFF. To work around this, CORE PD, RFF context state
 * is used which is lost only when we hit device OFF state
 */
u32 omap4_device_off_read_prev_state(void)
{
	u32 reg = 0;

	reg = prm_read_mod_reg(core_pwrdm->prcm_offs,
					core_pwrdm->context_offset);
	reg = (reg >> 0x1) & 0x1;
	return reg;
}

/**
 * omap4_device_off_read_prev_state:
 * returns 1 if the device next state is OFF
 * This is API to check whether OMAP is programmed for device OFF
 */
u32 omap4_device_off_read_next_state(void)
{
	return prm_read_mod_reg(OMAP4430_PRM_DEVICE_MOD,
			OMAP4_PRM_DEVICE_OFF_CTRL_OFFSET)
			& OMAP4430_DEVICE_OFF_ENABLE_MASK;
}

int omap4_can_sleep(void)
{
	if (!omap_uart_can_sleep())
		return 0;
	return 1;
}

void omap4_trigger_ioctrl(void)
{
	int i = 0;

	/* Trigger WUCLKIN enable */
	prm_rmw_mod_reg_bits(OMAP4430_WUCLK_CTRL_MASK, OMAP4430_WUCLK_CTRL_MASK,
		OMAP4430_PRM_DEVICE_MOD, OMAP4_PRM_IO_PMCTRL_OFFSET);

	omap_test_timeout(
		(prm_read_mod_reg(OMAP4430_PRM_DEVICE_MOD,
				OMAP4_PRM_IO_PMCTRL_OFFSET)
			& OMAP4430_WUCLK_STATUS_MASK),
		MAX_IOPAD_LATCH_TIME, i);
	/* Trigger WUCLKIN disable */
	prm_rmw_mod_reg_bits(OMAP4430_WUCLK_CTRL_MASK, 0x0,
		OMAP4430_PRM_DEVICE_MOD, OMAP4_PRM_IO_PMCTRL_OFFSET);
	return;
}

/* This sets pwrdm state (other than mpu & core. Currently only ON &
 * RET are supported. Function is assuming that clkdm doesn't have
 * hw_sup mode enabled. */
int omap4_set_pwrdm_state(struct powerdomain *pwrdm, u32 state)
{
	u32 cur_state;
	int sleep_switch = 0;
	int ret = 0;

	if (pwrdm == NULL || IS_ERR(pwrdm))
		return -EINVAL;

	while (!(pwrdm->pwrsts & (1 << state))) {
		if (state == PWRDM_POWER_OFF)
			return ret;
		state--;
	}

	cur_state = pwrdm_read_next_pwrst(pwrdm);
	if (cur_state == state)
		return ret;

	if (pwrdm_read_pwrst(pwrdm) < PWRDM_POWER_ON) {
		if ((pwrdm_read_pwrst(pwrdm) > state) &&
			(pwrdm->flags & PWRDM_HAS_LOWPOWERSTATECHANGE)) {
				ret = pwrdm_set_next_pwrst(pwrdm, state);
				pwrdm_set_lowpwrstchange(pwrdm);
				pwrdm_wait_transition(pwrdm);
				pwrdm_state_switch(pwrdm);
				return ret;
		}
		omap2_clkdm_wakeup(pwrdm->pwrdm_clkdms[0]);
		sleep_switch = 1;
		pwrdm_wait_transition(pwrdm);
	}

	ret = pwrdm_set_next_pwrst(pwrdm, state);
	if (ret) {
		printk(KERN_ERR "Unable to set state of powerdomain: %s\n",
		       pwrdm->name);
		goto err;
	}

	if (sleep_switch) {
		if (pwrdm->pwrdm_clkdms[0]->flags & CLKDM_CAN_ENABLE_AUTO)
			omap2_clkdm_allow_idle(pwrdm->pwrdm_clkdms[0]);
		else
			omap2_clkdm_sleep(pwrdm->pwrdm_clkdms[0]);
		pwrdm_wait_transition(pwrdm);
		pwrdm_state_switch(pwrdm);
	}

err:
	return ret;
}
/* This is a common low power function called from suspend and
 * cpuidle
 */
void omap4_enter_sleep(unsigned int cpu, unsigned int power_state)
{
	int cpu0_next_state = PWRDM_POWER_ON;
	int per_next_state = PWRDM_POWER_ON;
	int core_next_state = PWRDM_POWER_ON;
	int mpu_next_state = PWRDM_POWER_ON;

	pwrdm_clear_all_prev_pwrst(cpu0_pwrdm);
	pwrdm_clear_all_prev_pwrst(mpu_pwrdm);
	pwrdm_clear_all_prev_pwrst(core_pwrdm);
	pwrdm_clear_all_prev_pwrst(per_pwrdm);



	cpu0_next_state = pwrdm_read_next_pwrst(cpu0_pwrdm);
	per_next_state = pwrdm_read_next_pwrst(per_pwrdm);
	core_next_state = pwrdm_read_next_pwrst(core_pwrdm);
	mpu_next_state = pwrdm_read_next_pwrst(mpu_pwrdm);

	if (mpu_next_state < PWRDM_POWER_ON) {
		/* Disable SR for MPU VDD */
		omap_smartreflex_disable(vdd_mpu);
		/* Enable AUTO RET for mpu */
		if (!omap4_device_off_read_next_state())
			prm_rmw_mod_reg_bits(OMAP4430_AUTO_CTRL_VDD_MPU_L_MASK,
			0x2 << OMAP4430_AUTO_CTRL_VDD_MPU_L_SHIFT,
			OMAP4430_PRM_DEVICE_MOD, OMAP4_PRM_VOLTCTRL_OFFSET);
	}

	if (core_next_state < PWRDM_POWER_ON) {
		/*
		 * NOTE: IVA can hit RET outside of cpuidle and
		 * hence this is not the right (optimal) place
		 * to enable IVA AUTO RET. But since enabling AUTO
		 * RET requires SR to be disabled, its done here
		 * for now. Needs a relook to see if this can be
		 * optimized.
		 */
		/* Disable SR for CORE and IVA VDD*/
		omap_smartreflex_disable(vdd_iva);
		omap_smartreflex_disable(vdd_core);

	
	 if (omap4_device_off_read_next_state()) {
		omap_uart_prepare_idle(0);
		omap_uart_prepare_idle(1);
		omap_uart_prepare_idle(2);
		omap_uart_prepare_idle(3);
		omap2_gpio_prepare_for_idle(0);
	}

  if (omap4_device_off_read_next_state())
   omap2_dma_context_save();
		omap4_trigger_ioctrl();

		if (!omap4_device_off_read_next_state()) {
			/* Enable AUTO RET for IVA and CORE */
			prm_rmw_mod_reg_bits(OMAP4430_AUTO_CTRL_VDD_IVA_L_MASK,
			0x2 << OMAP4430_AUTO_CTRL_VDD_IVA_L_SHIFT,
			OMAP4430_PRM_DEVICE_MOD, OMAP4_PRM_VOLTCTRL_OFFSET);
			prm_rmw_mod_reg_bits(OMAP4430_AUTO_CTRL_VDD_CORE_L_MASK,
			0x2 << OMAP4430_AUTO_CTRL_VDD_CORE_L_SHIFT,
			OMAP4430_PRM_DEVICE_MOD, OMAP4_PRM_VOLTCTRL_OFFSET);
		}
	}

	/* FIXME  This call is not needed now for retention support and global
	 * suspend resume support. All the required actions are taken based
	 * the connect disconnect events.
	 * This call will be required for offmode support to save and restore
	 * context in the idle path oddmode support only.
	*/
	if (omap4_device_off_read_next_state()) {
		omap4_prcm_prepare_off();
		/* Save the device context to SAR RAM */
		omap4_sar_save();
		omap4_sar_overwrite();
	}

	omap4_enter_lowpower(cpu, power_state);

	if (omap4_device_off_read_prev_state()) {
		omap4_prcm_resume_off();
#ifdef CONFIG_PM_DEBUG
		omap4_device_off_counter++;
#endif
	}


	/* FIXME  This call is not needed now for retention support and global
	 * suspend resume support. All the required actions are taken based
	 * the connect disconnect events.
	 * This call will be required for offmode support to save and restore
	 * context in the idle path oddmode support only.
	*/

	if (core_next_state < PWRDM_POWER_ON) {
		if (!omap4_device_off_read_next_state()) {
			/* Disable AUTO RET for IVA and CORE */
			prm_rmw_mod_reg_bits(OMAP4430_AUTO_CTRL_VDD_IVA_L_MASK,
			0x0,
			OMAP4430_PRM_DEVICE_MOD, OMAP4_PRM_VOLTCTRL_OFFSET);
			prm_rmw_mod_reg_bits(OMAP4430_AUTO_CTRL_VDD_CORE_L_MASK,
			0x0,
			OMAP4430_PRM_DEVICE_MOD, OMAP4_PRM_VOLTCTRL_OFFSET);
		}



	
	 if (omap4_device_off_read_next_state()){
		omap2_gpio_resume_after_idle(0);
		omap_uart_resume_idle(0);
		omap_uart_resume_idle(1);
		omap_uart_resume_idle(2);
		omap_uart_resume_idle(3);
	}
  if (omap4_device_off_read_next_state())
   omap2_dma_context_restore();
		
		
		omap_hsi_resume_idle();
		

		/* Enable SR for IVA and CORE */
		omap_smartreflex_enable(vdd_iva);
		omap_smartreflex_enable(vdd_core);
	}

	if (mpu_next_state < PWRDM_POWER_ON) {
		if (!omap4_device_off_read_next_state())
			/* Disable AUTO RET for mpu */
			prm_rmw_mod_reg_bits(OMAP4430_AUTO_CTRL_VDD_MPU_L_MASK,
			0x0,
			OMAP4430_PRM_DEVICE_MOD, OMAP4_PRM_VOLTCTRL_OFFSET);
		/* Enable SR for MPU VDD */
		omap_smartreflex_enable(vdd_mpu);
	}

	return;
}


static irqreturn_t prcm_interrupt_handler (int irq, void *dev_id)
{
	u32 irqenable_mpu, irqstatus_mpu;

	irqenable_mpu = prm_read_mod_reg(OMAP4430_PRM_OCP_SOCKET_MOD,
					 OMAP4_PRM_IRQENABLE_MPU_OFFSET);
	irqstatus_mpu = prm_read_mod_reg(OMAP4430_PRM_OCP_SOCKET_MOD,
					 OMAP4_PRM_IRQSTATUS_MPU_OFFSET);

	/* Check if a IO_ST interrupt */
	if (irqstatus_mpu & OMAP4430_IO_ST_MASK) {
		/* Re-enable UART3 */
		omap_writel(0x2, 0x4A009550);
		omap_writel(0xD, 0x48020054);

		/* usbhs remote wakeup */
		usbhs_wakeup();
		omap4_trigger_ioctrl();
	}

	/* Clear the interrupt */
	irqstatus_mpu &= irqenable_mpu;
	prm_write_mod_reg(irqstatus_mpu, OMAP4430_PRM_OCP_SOCKET_MOD,
					OMAP4_PRM_IRQSTATUS_MPU_OFFSET);

	return IRQ_HANDLED;
}

#ifdef CONFIG_SUSPEND
extern int twl_i2c_write_u8(unsigned char mod_no, unsigned char value, unsigned char reg);
static int omap4_pm_prepare(void)
{
	//GPADC_CTRL 
	twl_i2c_write_u8(0x0E, 0x00, 0x2E);
	//TOGGLE1
	twl_i2c_write_u8(0x0E, 0x51, 0x90);
	//MISC2
	twl_i2c_write_u8(0x0D, 0x00, 0xE5);

	//OFF VCXIO  100uA
	twl_i2c_write_u8(0x0D, 0x01, 0x90);
	twl_i2c_write_u8(0x0D, 0x01, 0x91);

	//VDAC
	twl_i2c_write_u8(0x0D, 0x01, 0x94);
	twl_i2c_write_u8(0x0D, 0x01, 0x95);

	// VMMC
#if defined(CONFIG_MACH_LGE_VMMC_ALWAYSON_FORCED) || defined(CONFIG_MACH_LGE_MMC_ALWAYSON)	
	//removed
#else
	#if defined (CONFIG_MACH_LGE_VMMC_AUTO_OFF)
//none
	#else

	twl_i2c_write_u8(0x0D, 0x01, 0x98);
	twl_i2c_write_u8(0x0D, 0x01, 0x99);
	twl_i2c_write_u8(0x0D, 0x21, 0x9A);
	#endif

#endif	

 	//Logan_Test - put MOD, CON into DEVOFF mode
	twl_i2c_write_u8(0x0D, 0x06, 0x25);	

	return 0;
}

/* Revert the defines after the GPIO glitch errata is applied */

#define OMAP4_GPIO_DATAOUT		0x013c
#define OMAP4_GPIO_OE			0x0134

static unsigned int padconf[128][3];
static int padindex = 0;
int bank_gpio_no =0;
int bank_no =0;	

/* Save the current register value into padconf[][]
 * then update the register with the given value in the parameter. */
static void pad_save(unsigned int addr, int gpio_no)
{
	
	
	int nwp_padconf;
	int bank_base_addr[6]={0x4a310000,0x48055000,0x48057000,0x48059000,0x4805b000,0x4805d000};
	

	bank_gpio_no=(gpio_no%32);
	bank_no=(int)(gpio_no/32);

	if (omap_readl(bank_base_addr[bank_no] + OMAP4_GPIO_DATAOUT) & (1 << bank_gpio_no)) 
	{

		padconf[padindex][0] = addr;
		padconf[padindex][1] = (u32) omap_readw(addr);
		padconf[padindex][2] = gpio_no;

		nwp_padconf = omap_readw(addr);

		/* Enable pullupdown */
		nwp_padconf |= 1 << 3;
		omap_writew(nwp_padconf, addr);
		
		/* Enable INPUT */
		nwp_padconf = nwp_padconf | 1 << 8;
		omap_writew(nwp_padconf, addr);
		
		/* Enable PU */
		nwp_padconf = nwp_padconf | 1 << 4;
		omap_writew(nwp_padconf, addr);

		/* Disable output on GPIO */
		omap_writel(omap_readl(bank_base_addr[bank_no] + OMAP4_GPIO_OE) | (1 << bank_gpio_no),bank_base_addr[bank_no] + OMAP4_GPIO_OE);

		/* Put the pad in safe mode */
		omap_writew(nwp_padconf | 0x7, addr);


		/*
		 * Strangely the delay seems to be needed even after programming
		 * the bits.
		 */
		
		padindex ++;		
		
	}	

}
static void Cosmo_padconf_restore()
{
	int j;

	if(padindex==0)
		return;
	
	for(j=0;j<padindex;j++)
	{
		//hard coding
		gpio_direction_output(padconf[j][2], 1);
   		gpio_set_value(padconf[j][2],1);
		omap_writew((u16) padconf[j][1], padconf[j][0]);
	}

	padindex = 0;
}

static void Cosmo_padconf_save()
{

	if(padindex==0)
	{
		pad_save(	0x4A100062	,	41	);	// HDMI_LS_OE (?)
		pad_save(	0x4A100066	,	43	);	// 3D_BOOST_EN
		pad_save(	0x4A10007E	,	55	);	// IFX_USB_VBUS_EN
		pad_save(	0x4A100094	,	103	);	// SWB_VIO_EN
		pad_save(	0x4A100096	,	104	);	// PROXI_LDO_EN
		pad_save(	0x4A1000BE	,	82	);	// FRONT_KEY_LED_EN
		pad_save(	0x4A1000C0	,	83	);	// CHG_EN_SET
		pad_save(	0x4A1000DE	,	98	);	// LCD_CP_EN
		//pad_save(	0x4A1000E0	,	99	);	// CAM_SUBPM_EN	
		pad_save(	0x4A100112	,	120	);	// IPC_MRDY
		pad_save(	0x4A100116	,	122	);	// OMAP_SEND
		pad_save(	0x4A100120	,	127	);	// AUD_PWRON 
		pad_save(	0x4A10013E	,	140	);	// GPS_LNA_SD 
		pad_save(	0x4A100146	,	144	);	// 3D_LCD_EN
		pad_save(	0x4A10016E	,	164	);	// RESET_PMU_N
		pad_save(	0x4A100170	,	165	);	// USIF1_SW
		pad_save(	0x4A100172	,	166	);	// BT_EN
		pad_save(	0x4A100176	,	168	);	// WLAN_EN
		pad_save(	0x4A1001D2	,	190	);	// 3D_LCD_BANK_SEL
		pad_save(	0x4A1001D4	,	191	);	// FLASH_EN
	}	

}


int offmode_enter=0;

int mpu_m3_clkctrl =0;
int mpu_m3_clkctrl_count = 0;
static int omap4_pm_suspend(void)
{
	struct power_state *pwrst;
	int state;
	u32 cpu_id = 0;
	
	u32 cpu1_state;
	
	u16 uart2_cts_config, uart_sysc_config;

	uart2_cts_config = omap_readw(0x4A100118);
	uart_sysc_config = omap_readl(0x4806C054);
	omap_writew(0x4100, 0x4A100118); // set uart2_cts
	omap_writel(uart_sysc_config | 0x00000004, 0x4806C054); // set uart2_wakeup_enable
	

	
	Cosmo_padconf_save();
	omap_writel(0x00000009,0x4A307508);
	
	/*
	 * Wakeup timer from suspend
	 */
	if (wakeup_timer_seconds || wakeup_timer_milliseconds)
		omap2_pm_wakeup_on_timer(wakeup_timer_seconds,
					 wakeup_timer_milliseconds);
#ifdef CONFIG_PM_DEBUG
	pwrdm_pre_transition();
#endif

	/*
	 * Clear all wakeup sources and keep
	 * only Debug UART, Keypad, HSI and GPT1 interrupt
	 * as a wakeup event from MPU/Device OFF
	 */
	omap4_wakeupgen_clear_all(cpu_id);
	omap4_wakeupgen_set_interrupt(cpu_id, OMAP44XX_IRQ_UART3);
	omap4_wakeupgen_set_interrupt(cpu_id, OMAP44XX_IRQ_KBD_CTL);
	omap4_wakeupgen_set_interrupt(cpu_id, OMAP44XX_IRQ_GPT1);
	omap4_wakeupgen_set_interrupt(cpu_id, OMAP44XX_IRQ_PRCM);
	omap4_wakeupgen_set_interrupt(cpu_id, OMAP44XX_IRQ_SYS_1N);

#if defined(CONFIG_OMAP_HSI)
	omap4_wakeupgen_set_interrupt(cpu_id, OMAP44XX_IRQ_HSI_P1);
#endif

	omap4_wakeupgen_set_interrupt(cpu_id, OMAP44XX_IRQ_GPIO4);
	omap4_wakeupgen_set_interrupt(cpu_id, OMAP44XX_IRQ_SYS_2N);
	struct irq_desc *desc = irq_to_desc(OMAP44XX_IRQ_SYS_1N);
	desc->chip->unmask(OMAP44XX_IRQ_SYS_1N);


	omap4_wakeupgen_set_interrupt(cpu_id, OMAP44XX_IRQ_UART2); // WAKEUP from BT(UART2)

#ifdef CONFIG_ENABLE_L3_ERRORS
	/* Allow the L3 errors to be logged */
	omap4_wakeupgen_set_interrupt(cpu_id, OMAP44XX_IRQ_L3_DBG);
	omap4_wakeupgen_set_interrupt(cpu_id, OMAP44XX_IRQ_L3_APP);
#endif

	/* Read current next_pwrsts */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		pwrst->saved_state = pwrdm_read_next_pwrst(pwrst->pwrdm);
		pwrst->saved_logic_state = pwrdm_read_logic_retst(pwrst->pwrdm);
	}

	/* program all powerdomains to sleep */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		pwrdm_clear_all_prev_pwrst(pwrst->pwrdm);
		/*
		 * While attempting a system wide suspend, all non core cpu;s
		 * are already offlined using the cpu hotplug callback.
		 * Hence we do not have to program non core cpu (cpu1) target
		 * state in the omap4_pm_suspend function
		 */
		if (strcmp(pwrst->pwrdm->name, "cpu1_pwrdm")) {

#ifdef CONFIG_OMAP_ALLOW_OSWR
			if ((pwrst->pwrdm->pwrsts_logic_ret == PWRSTS_OFF_RET)
			 && (omap_rev() >= OMAP4430_REV_ES2_1))
				pwrdm_set_logic_retst(pwrst->pwrdm,
							PWRDM_POWER_OFF);
#endif
			if (omap4_set_pwrdm_state(pwrst->pwrdm,
							pwrst->next_state))
				goto restore;
		}
	}

	omap_uart_prepare_suspend();
	
	omap_hsi_prepare_suspend(); 
	

	offmode_enter=1;
	/* Enable Device OFF */
	if (enable_off_mode)
		omap4_device_off_set_state(1);


	omap4_enter_sleep(0, PWRDM_POWER_OFF);

	/* Disable Device OFF state*/
	if (enable_off_mode)
		omap4_device_off_set_state(0);

	printk("count=%d, pre_mpu_m3_clkctrl=0x%x, CM_MPU_M3_MPU_M3_CLKCTRL: 0x%x\n", mpu_m3_clkctrl_count, mpu_m3_clkctrl, omap_readl(0x4A008920));
	mpu_m3_clkctrl_count=0;
           
restore:
	/* Print the previous power domain states */
	pr_info("Read Powerdomain states as ...\n");
	pr_info("0 : OFF, 1 : RETENTION, 2 : ON-INACTIVE, 3 : ON-ACTIVE\n");
	list_for_each_entry(pwrst, &pwrst_list, node) {
		state = pwrdm_read_prev_pwrst(pwrst->pwrdm);
		if (state == -EINVAL) {
			state = pwrdm_read_pwrst(pwrst->pwrdm);
			pr_info("Powerdomain (%s) is in state %d\n",
				pwrst->pwrdm->name, state);
		} else {
			pr_info("Powerdomain (%s) entered state %d\n",
				pwrst->pwrdm->name, state);
		}
	}

	/* restore next_pwrsts */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		if (strcmp(pwrst->pwrdm->name, "cpu1_pwrdm")) {
			omap4_set_pwrdm_state(pwrst->pwrdm, pwrst->saved_state);
			pwrdm_set_logic_retst(pwrst->pwrdm,
						pwrst->saved_logic_state);
		}
	}

#ifdef CONFIG_PM_DEBUG
	pwrdm_post_transition();
#endif
	
	/*
	 * Enable all wakeup sources post wakeup
	 */
	omap4_wakeupgen_set_all(cpu_id);

	omap_writew(uart2_cts_config, 0x4A100118); // restore uart2_cts
	omap_writel(uart_sysc_config, 0x4806C054); // restore uart2_wakeup_enable

	
	Cosmo_padconf_restore();

	return 0;
}

static int omap4_pm_enter(suspend_state_t suspend_state)
{
	int ret = 0;

	switch (suspend_state) {
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		ret = omap4_pm_suspend();
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static void omap4_pm_finish(void)
{
	return;
}

static int omap4_pm_begin(suspend_state_t state)
{
	disable_hlt();
	return 0;
}

static void omap4_pm_end(void)
{
	enable_hlt();
	return;
}

static struct platform_suspend_ops omap_pm_ops = {
	.begin		= omap4_pm_begin,
	.end		= omap4_pm_end,
	.prepare	= omap4_pm_prepare,
	.enter		= omap4_pm_enter,
	.finish		= omap4_pm_finish,
	.valid		= suspend_valid_only_mem,
};
#endif /* CONFIG_SUSPEND */

/*
 * Enable hw supervised mode for all clockdomains if it's
 * supported. Initiate sleep transition for other clockdomains, if
 * they are not used
 */
static int __attribute__ ((unused)) __init
		clkdms_setup(struct clockdomain *clkdm, void *unused)
{
	if (clkdm->flags & CLKDM_CAN_ENABLE_AUTO)
		omap2_clkdm_allow_idle(clkdm);
	else if (clkdm->flags & CLKDM_CAN_FORCE_SLEEP &&
			atomic_read(&clkdm->usecount) == 0)
		omap2_clkdm_sleep(clkdm);
	return 0;
}


static int __init pwrdms_setup(struct powerdomain *pwrdm, void *unused)
{
	struct power_state *pwrst;

	if (!pwrdm->pwrsts)
		return 0;

	pwrst = kmalloc(sizeof(struct power_state), GFP_ATOMIC);
	if (!pwrst)
		return -ENOMEM;
	pwrst->pwrdm = pwrdm;
	pwrst->next_state = PWRDM_POWER_RET;
	list_add(&pwrst->node, &pwrst_list);

	return omap4_set_pwrdm_state(pwrst->pwrdm, pwrst->next_state);
}

/**
 * omap4_pm_off_mode_enable :
 *	Program all powerdomain to OFF
 */
void omap4_pm_off_mode_enable(int enable)
{
	struct power_state *pwrst;
	u32 state;
	u32 logic_state;

	if (enable) {
		state = PWRDM_POWER_OFF;
		logic_state = PWRDM_POWER_OFF;
	} else {
		state = PWRDM_POWER_RET;
		logic_state = PWRDM_POWER_RET;
	}

	list_for_each_entry(pwrst, &pwrst_list, node) {
		if (!plist_head_empty(&pwrst->pwrdm->wakeuplat_dev_list)) {
			pr_crit("Device is holding contsraint on %s\n",
				 pwrst->pwrdm->name);
			continue;
		}
		pwrdm_set_logic_retst(pwrst->pwrdm, logic_state);
		if ((state == PWRDM_POWER_OFF) &&
			!(pwrst->pwrdm->pwrsts & (1 << state)))
			pwrst->next_state = PWRDM_POWER_RET;
		else
			pwrst->next_state = state;
		omap4_set_pwrdm_state(pwrst->pwrdm, state);
	}
}

static void __init prcm_setup_regs(void)
{
	struct clk *dpll_abe_ck, *dpll_core_ck, *dpll_iva_ck;
	struct clk *dpll_mpu_ck, *dpll_per_ck, *dpll_usb_ck;
	struct clk *dpll_unipro_ck;

	/*Enable all the DPLL autoidle */
	dpll_abe_ck = clk_get(NULL, "dpll_abe_ck");
	omap3_dpll_allow_idle(dpll_abe_ck);
	dpll_core_ck = clk_get(NULL, "dpll_core_ck");
	omap3_dpll_allow_idle(dpll_core_ck);
	dpll_iva_ck = clk_get(NULL, "dpll_iva_ck");
	omap3_dpll_allow_idle(dpll_iva_ck);
	dpll_mpu_ck = clk_get(NULL, "dpll_mpu_ck");
	omap3_dpll_allow_idle(dpll_mpu_ck);
	dpll_per_ck = clk_get(NULL, "dpll_per_ck");
	omap3_dpll_allow_idle(dpll_per_ck);
	dpll_usb_ck = clk_get(NULL, "dpll_usb_ck");
	omap3_dpll_allow_idle(dpll_usb_ck);
	dpll_unipro_ck = clk_get(NULL, "dpll_unipro_ck");
	omap3_dpll_allow_idle(dpll_unipro_ck);

	/* Enable autogating for all DPLL post dividers */
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUT_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD, OMAP4_CM_DIV_M2_DPLL_MPU_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT1_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M4_DPLL_IVA_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT2_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M5_DPLL_IVA_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUT_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M2_DPLL_CORE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUTHIF_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M3_DPLL_CORE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT1_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M4_DPLL_CORE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT2_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M5_DPLL_CORE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT3_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M6_DPLL_CORE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT4_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M7_DPLL_CORE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUT_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD, OMAP4_CM_DIV_M2_DPLL_PER_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUTX2_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD,	OMAP4_CM_DIV_M2_DPLL_PER_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUTHIF_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD,	OMAP4_CM_DIV_M3_DPLL_PER_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT1_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD,	OMAP4_CM_DIV_M4_DPLL_PER_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT2_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD,	OMAP4_CM_DIV_M5_DPLL_PER_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT3_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD,	OMAP4_CM_DIV_M6_DPLL_PER_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT4_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD,	OMAP4_CM_DIV_M7_DPLL_PER_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUT_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M2_DPLL_ABE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUTX2_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M2_DPLL_ABE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUTHIF_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M3_DPLL_ABE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUT_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD,	OMAP4_CM_DIV_M2_DPLL_USB_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKDCOLDO_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD, OMAP4_CM_CLKDCOLDO_DPLL_USB_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUTX2_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD,	OMAP4_CM_DIV_M2_DPLL_UNIPRO_OFFSET);

	/* Enable IO_ST interrupt */
	prm_rmw_mod_reg_bits(OMAP4430_IO_ST_MASK, OMAP4430_IO_ST_MASK,
		OMAP4430_PRM_OCP_SOCKET_MOD, OMAP4_PRM_IRQENABLE_MPU_OFFSET);

	/* Enable GLOBAL_WUEN */
	prm_rmw_mod_reg_bits(OMAP4430_GLOBAL_WUEN_MASK, OMAP4430_GLOBAL_WUEN_MASK,
		OMAP4430_PRM_DEVICE_MOD, OMAP4_PRM_IO_PMCTRL_OFFSET);

	/*
	 * Errata ID: i608 Impacted OMAP4430 ES 1.0,2.0,2.1,2.2
	 * On OMAP4, Retention-Till-Access Memory feature is not working
	 * reliably and hardware recommondation is keep it disabled by
	 * default
	 */
	prm_rmw_mod_reg_bits(OMAP4430_DISABLE_RTA_EXPORT_MASK,
		0x1 << OMAP4430_DISABLE_RTA_EXPORT_SHIFT,
		OMAP4430_PRM_DEVICE_MOD, OMAP4_PRM_SRAM_WKUP_SETUP_OFFSET);
	prm_rmw_mod_reg_bits(OMAP4430_DISABLE_RTA_EXPORT_MASK,
		0x1 << OMAP4430_DISABLE_RTA_EXPORT_SHIFT,
		OMAP4430_PRM_DEVICE_MOD, OMAP4_PRM_LDO_SRAM_CORE_SETUP_OFFSET);
	prm_rmw_mod_reg_bits(OMAP4430_DISABLE_RTA_EXPORT_MASK,
		0x1 << OMAP4430_DISABLE_RTA_EXPORT_SHIFT,
		OMAP4430_PRM_DEVICE_MOD, OMAP4_PRM_LDO_SRAM_MPU_SETUP_OFFSET);
	prm_rmw_mod_reg_bits(OMAP4430_DISABLE_RTA_EXPORT_MASK,
		0x1 << OMAP4430_DISABLE_RTA_EXPORT_SHIFT,
		OMAP4430_PRM_DEVICE_MOD, OMAP4_PRM_LDO_SRAM_IVA_SETUP_OFFSET);

	/* Toggle CLKREQ in RET and OFF states */
	prm_write_mod_reg(0x2, OMAP4430_PRM_DEVICE_MOD, OMAP4_PRM_CLKREQCTRL_OFFSET);

	/*
	 * De-assert PWRREQ signal in Device OFF state
	 *	0x3: PWRREQ is de-asserted if all voltage domain are in
	 *	OFF state. Conversely, PWRREQ is asserted upon any
	 *	voltage domain entering or staying in ON or SLEEP or
	 *	RET state.
	 */
	prm_write_mod_reg(0x3, OMAP4430_PRM_DEVICE_MOD,
				OMAP4_PRM_PWRREQCTRL_OFFSET);
}

/**
 * prcm_clear_statdep_regs :
 * Clear Static dependencies for the modules
 * Refer TRM section 3.1.1.1.7.1 for more information
 */
static void __init prcm_clear_statdep_regs(void)
{
#ifdef CONFIG_OMAP4_KEEP_STATIC_DEPENDENCIES
	pr_info("%s: Keep static depndencies\n", __func__);
	return;
#else
	u32 reg;

	pr_info("%s: Clearing static depndencies\n", __func__);

	/*
	 * REVISIT: Seen issue with MPU/DSP -> L3_2 and L4CFG. Keeping
	 * it enabled.
	 */
	/* MPU towards EMIF, L3_2 and L4CFG clockdomains */
	 /*
	  * REVISIT: Issue seen with Ducati towards EMIF, L3_2, L3_1,
	  * L4CFG and L4WKUP static
	  * dependency. Keep it enabled as of now.
	  */


	/* Ducati towards EMIF, L3_2, L3_1, L4CFG and L4WKUP clockdomains */
	reg = OMAP4430_MEMIF_STATDEP_MASK | OMAP4430_L3_1_STATDEP_MASK
		| OMAP4430_L3_2_STATDEP_MASK | OMAP4430_L4CFG_STATDEP_MASK
		| OMAP4430_L4WKUP_STATDEP_MASK;
	cm_rmw_mod_reg_bits(reg, 0, OMAP4430_CM2_CORE_MOD,
		OMAP4_CM_DUCATI_STATICDEP_OFFSET);

	/* SDMA towards EMIF, L3_2, L3_1, L4CFG, L4WKUP, L3INIT
	 * and L4PER clockdomains
	*/
	reg = OMAP4430_MEMIF_STATDEP_MASK | OMAP4430_L3_1_STATDEP_MASK
		| OMAP4430_L3_2_STATDEP_MASK | OMAP4430_L4CFG_STATDEP_MASK
		| OMAP4430_L4WKUP_STATDEP_MASK | OMAP4430_L4PER_STATDEP_MASK
		| OMAP4430_L3INIT_STATDEP_MASK;
	cm_rmw_mod_reg_bits(reg, 0, OMAP4430_CM2_CORE_MOD,
		OMAP4_CM_SDMA_STATICDEP_OFFSET);

	/* C2C towards EMIF clockdomains */
	cm_rmw_mod_reg_bits(OMAP4430_MEMIF_STATDEP_MASK, 0,
		OMAP4430_CM2_CORE_MOD, OMAP4_CM_D2D_STATICDEP_OFFSET);

	/* C2C_STATICDEP_RESTORE towards EMIF clockdomains */
	cm_rmw_mod_reg_bits(OMAP4430_MEMIF_STATDEP_MASK, 0,
			OMAP4430_CM2_RESTORE_MOD,
			OMAP4_CM_D2D_STATICDEP_RESTORE_OFFSET);

	 /* SDMA_RESTORE towards EMIF, L3_1, L4_CFG,L4WKUP clockdomains */
	reg = OMAP4430_MEMIF_STATDEP_MASK | OMAP4430_L3_1_STATDEP_MASK
		| OMAP4430_L4CFG_STATDEP_MASK | OMAP4430_L4WKUP_STATDEP_MASK;
	cm_rmw_mod_reg_bits(reg, 0, OMAP4430_CM2_RESTORE_MOD,
		OMAP4_CM_SDMA_STATICDEP_RESTORE_OFFSET);
#endif
};

/**
 * omap4_pm_init - Init routine for OMAP4 PM
 *
 * Initializes all powerdomain and clockdomain target states
 * and all PRCM settings.
 */
static int __init omap4_pm_init(void)
{
	int ret;
	int ram_addr;

	if (!cpu_is_omap44xx())
		return -ENODEV;

	pr_err("Power Management for TI OMAP4.\n");


#ifdef CONFIG_PM
	prcm_setup_regs();
	prcm_clear_statdep_regs();

	ret = request_irq(OMAP44XX_IRQ_PRCM,
			  (irq_handler_t)prcm_interrupt_handler,
			  IRQF_DISABLED, "prcm", NULL);
	if (ret) {
		printk(KERN_ERR "request_irq failed to register for 0x%x\n",
		       OMAP44XX_IRQ_PRCM);
		goto err2;
	}

	so_ram_address = (void *)dma_alloc_so_coherent(NULL, 1024,
			(dma_addr_t *)&ram_addr, GFP_KERNEL);

	if (!so_ram_address) {
		printk ("omap4_pm_init: failed to allocate SO mem.\n");
		return -ENOMEM;
	}

	ret = pwrdm_for_each(pwrdms_setup, NULL);
	if (ret) {
		pr_err("Failed to setup powerdomains\n");
		goto err2;
	}

	mpu_pwrdm = pwrdm_lookup("mpu_pwrdm");
	if (!mpu_pwrdm) {
		printk(KERN_ERR "Failed to get lookup for MPU pwrdm's\n");
		goto err2;
	}

	(void) clkdm_for_each(clkdms_setup, NULL);

	/* Get handles for VDD's for enabling/disabling SR */
	vdd_mpu = omap_voltage_domain_get("mpu");
	if (IS_ERR(vdd_mpu)) {
		printk(KERN_ERR "Failed to get handle for VDD MPU\n");
		goto err2;
	}

	vdd_iva = omap_voltage_domain_get("iva");
	if (IS_ERR(vdd_iva)) {
		printk(KERN_ERR "Failed to get handle for VDD IVA\n");
		goto err2;
	}

	vdd_core = omap_voltage_domain_get("core");
	if (IS_ERR(vdd_core)) {
		printk(KERN_ERR "Failed to get handle for VDD CORE\n");
		goto err2;
	}

	omap4_mpuss_init();
#endif

#ifdef CONFIG_SUSPEND
	suspend_set_ops(&omap_pm_ops);
#endif /* CONFIG_SUSPEND */

	cpu0_pwrdm = pwrdm_lookup("cpu0_pwrdm");
	core_pwrdm = pwrdm_lookup("core_pwrdm");
	per_pwrdm = pwrdm_lookup("l4per_pwrdm");
	omap4_idle_init();
	omap4_trigger_ioctrl();

err2:
	return ret;
}
late_initcall(omap4_pm_init);
