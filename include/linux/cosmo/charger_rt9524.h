/*
 * Charging IC driver (RT9524)
 *
 * Copyright (C) 2010 LGE, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#define CHG_EN_SET_N_OMAP 		83
#define CHG_STATUS_N_OMAP 		51

#define OMAP_SEND				122  //for fuel gauge reset on CP.

//#define DEBUG

#ifdef DEBUG
#define D(fmt, args...) printk(fmt " :: file=%s, func=%s, line=%d\n", ##args, __FILE__, __func__, __LINE__ )
//#define D(fmt, args...) printk(fmt, ##args)
#else
#define D(fmt, args...)
#endif

#define BAT_TEMP_OVER

#define TEMP_LOW_NO_BAT			-200
#define TEMP_LOW_DISCHARGING	0
#define TEMP_HIGH_DISCHARGING	520


#define RECHARGING_BAT_SOC_CON		99

#define RECHARGING_BAT_VOLT_LOW		4150
#define RECHARGING_BAT_VOLT_HIGH	4193


#define OVER_RANGE_TEMP_CHARGING_VOLT_LOW	3790
#define OVER_RANGE_TEMP_CHARGING_VOLT_HIGH	4020


#if defined(CONFIG_LG_FW_MAX17043_FUEL_GAUGE)
#define SAFE_SHUTDOWN_SOC_CON	5
#define SHUTDOWN_SOC_CON	1
#endif


typedef enum {
	FACTORY_CHARGER_ENABLE,
	FACTORY_CHARGER_DISABLE,
}charge_factory_cmd;


typedef enum {
	CHARGER_DISABLE,
	BATTERY_NO_CHARGER,
	CHARGER_NO_BATTERY,
	CHARGER_AND_BATTERY,
}charge_enable_state_t ;

typedef enum {
	RECHARGING_WAIT_UNSET,
	RECHARGING_WAIT_SET,
	RECHARGING_CHARGING_SET,
}recharging_state_t;

typedef enum {
	CHARGER_LOGO_STATUS_UNKNOWN,
	CHARGER_LOGO_STATUS_STARTED,
	CHARGER_LOGO_STATUS_END,
}charger_logo_state_t;


/* Function Prototype */

enum power_supply_type get_charging_ic_status(void);

void charging_ic_active_default(void);
void charging_ic_set_ta_mode(void);
void charging_ic_set_usb_mode(void);
void charging_ic_set_factory_mode(void);
void charging_ic_deactive(void);
int get_temp(void);
void set_modem_alive(int alive);

typedef enum {
	CHARG_FSM_CAUSE_ANY = 0,
	CHARG_FSM_CAUSE_CHARGING_TIMER_EXPIRED,
}charger_fsm_cause;
void charger_fsm(charger_fsm_cause reason);


int twl6030battery_temperature(void);
int get_bat_soc(void);
int get_bat_volt(void);
struct delayed_work* get_charger_work(void);

void charger_schedule_delayed_work(struct delayed_work *work, unsigned long delay);

/// max17043 fuel gauge..
void set_boot_charging_mode(int charging_mode);
