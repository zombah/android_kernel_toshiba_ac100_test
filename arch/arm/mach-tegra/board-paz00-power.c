/*
 * Copyright (C) 2010-2011 NVIDIA, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>

#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/tps6586x.h>
#include <linux/io.h>

#include <mach/iomap.h>
#include <mach/irqs.h>

#include "board-paz00.h"
#include "pm.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)

static struct regulator_consumer_supply tps658621_sm0_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
};

static struct regulator_consumer_supply tps658621_sm1_supply[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL),
};

static struct regulator_consumer_supply tps658621_sm2_supply[] = {
	REGULATOR_SUPPLY("vdd_sm2", NULL),
};

static struct regulator_consumer_supply tps658621_ldo0_supply[] = {
	REGULATOR_SUPPLY("pex_clk", NULL),
};

static struct regulator_consumer_supply tps658621_ldo1_supply[] = {
	REGULATOR_SUPPLY("avdd_pll", NULL),
};

static struct regulator_consumer_supply tps658621_ldo2_supply[] = {
	REGULATOR_SUPPLY("vdd_rtc", NULL),
	REGULATOR_SUPPLY("vdd_aon", NULL),
};

static struct regulator_consumer_supply tps658621_ldo3_supply[] = {
	REGULATOR_SUPPLY("avdd_usb", NULL),
	REGULATOR_SUPPLY("avdd_usb_pll", NULL),
	REGULATOR_SUPPLY("avdd_lvds", NULL),
};

static struct regulator_consumer_supply tps658621_ldo4_supply[] = {
	REGULATOR_SUPPLY("avdd_osc", NULL),
};

static struct regulator_consumer_supply tps658621_ldo5_supply[] = {
	REGULATOR_SUPPLY("vddio_nand", NULL),
	REGULATOR_SUPPLY("vmmc", "sdhci-tegra.3"),
};

static struct regulator_consumer_supply tps658621_ldo6_supply[] = {
	REGULATOR_SUPPLY("avdd_vdac", NULL),
};

static struct regulator_consumer_supply tps658621_ldo7_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi", NULL),
};

static struct regulator_consumer_supply tps658621_ldo8_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi_pll", NULL),
};

static struct regulator_consumer_supply tps658621_ldo9_supply[] = {
	REGULATOR_SUPPLY("vdd_ddr_rx", NULL),
};

static struct tps6586x_settings sm0_config = {
	.sm_pwm_mode = PWM_DEFAULT_VALUE,
	.slew_rate = SLEW_RATE_3520UV_PER_SEC,
};

static struct tps6586x_settings sm1_config = {
	/*
	 * Current TPS6586x is known for having a voltage glitch if current load
	 * changes from low to high in auto PWM/PFM mode for CPU's Vdd line.
	 */
	.sm_pwm_mode = PWM_ONLY,
	.slew_rate = SLEW_RATE_3520UV_PER_SEC,
};

#define REGULATOR_INIT(_id, _minmv, _maxmv, on, config)				\
	{								\
		.constraints = {					\
			.min_uV = (_minmv)*1000,			\
			.max_uV = (_maxmv)*1000,			\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL |	\
					     REGULATOR_MODE_STANDBY),	\
			.valid_ops_mask = (REGULATOR_CHANGE_MODE |	\
					   REGULATOR_CHANGE_STATUS |	\
					   REGULATOR_CHANGE_VOLTAGE),	\
			.always_on = on,				\
			.apply_uV = 1,					\
		},							\
		.num_consumer_supplies = ARRAY_SIZE(tps658621_##_id##_supply),\
		.consumer_supplies = tps658621_##_id##_supply,		\
		.driver_data = config,					\
	}

static struct regulator_init_data sm0_data = REGULATOR_INIT(sm0,    725, 1300, true,  &sm0_config);
static struct regulator_init_data sm1_data = REGULATOR_INIT(sm1,    725, 1125, true,  &sm1_config);
static struct regulator_init_data sm2_data = REGULATOR_INIT(sm2,   3700, 3700, true,  NULL);
static struct regulator_init_data ldo0_data = REGULATOR_INIT(ldo0, 3300, 3300, false, NULL);
static struct regulator_init_data ldo1_data = REGULATOR_INIT(ldo1, 1100, 1100, true,  NULL);
static struct regulator_init_data ldo2_data = REGULATOR_INIT(ldo2,  725, 1275, false, NULL);
static struct regulator_init_data ldo3_data = REGULATOR_INIT(ldo3, 3300, 3300, true,  NULL);
static struct regulator_init_data ldo4_data = REGULATOR_INIT(ldo4, 1800, 1800, true,  NULL);
static struct regulator_init_data ldo5_data = REGULATOR_INIT(ldo5, 2850, 2850, true,  NULL);
static struct regulator_init_data ldo6_data = REGULATOR_INIT(ldo6, 1800, 1800, false, NULL);
static struct regulator_init_data ldo7_data = REGULATOR_INIT(ldo7, 3300, 3300, false, NULL);
static struct regulator_init_data ldo8_data = REGULATOR_INIT(ldo8, 1800, 1800, false, NULL);
static struct regulator_init_data ldo9_data = REGULATOR_INIT(ldo9, 2850, 2850, true,  NULL);

static struct tps6586x_rtc_platform_data rtc_data = {
	.irq = TEGRA_NR_IRQS + TPS6586X_INT_RTC_ALM1,
	.start = {
		.year = 2009,
		.month = 1,
		.day = 1,
	},
	.cl_sel = TPS6586X_RTC_CL_SEL_1_5PF /* use lowest (external 20pF cap) */
};

#define TPS_REG(_id, _data)			\
	{					\
		.id = TPS6586X_ID_##_id,	\
		.name = "tps6586x-regulator",	\
		.platform_data = _data,		\
	}

#define TPS_GPIO_FIXED_REG(_id, _data)		\
	{					\
		.id = _id,			\
		.name = "reg-fixed-voltage",	\
		.platform_data = _data,		\
	}

static struct tps6586x_subdev_info tps_devs[] = {
	TPS_REG(SM_0, &sm0_data),
	TPS_REG(SM_1, &sm1_data),
	TPS_REG(SM_2, &sm2_data),
	TPS_REG(LDO_0, &ldo0_data),
	TPS_REG(LDO_1, &ldo1_data),
	TPS_REG(LDO_2, &ldo2_data),
	TPS_REG(LDO_3, &ldo3_data),
	TPS_REG(LDO_4, &ldo4_data),
	TPS_REG(LDO_5, &ldo5_data),
	TPS_REG(LDO_6, &ldo6_data),
	TPS_REG(LDO_7, &ldo7_data),
	TPS_REG(LDO_8, &ldo8_data),
	TPS_REG(LDO_9, &ldo9_data),
/*	TPS_GPIO_FIXED_REG(0, &vdd_1v5),
	TPS_GPIO_FIXED_REG(1, &vdd_1v2),
	TPS_GPIO_FIXED_REG(2, &vdd_1v05),
	TPS_GPIO_FIXED_REG(3, &vdd_1v05_mode), */
	{
	 .id = 0,
	 .name = "tps6586x-rtc",
	 .platform_data = &rtc_data,
	 },
};

static struct tps6586x_platform_data tps_platform = {
	.irq_base	= TEGRA_NR_IRQS,
	.num_subdevs	= ARRAY_SIZE(tps_devs),
	.subdevs	= tps_devs,
	.gpio_base	= TEGRA_NR_GPIOS,
	.use_power_off	= false,
};

static struct i2c_board_info __initdata paz00_regulators[] = {
	{
		I2C_BOARD_INFO("tps6586x", 0x34),
		.irq		= INT_EXTERNAL_PMU,
		.platform_data	= &tps_platform,
	},
};

static void paz00_board_suspend(int lp_state, enum suspend_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_SUSPEND_BEFORE_CPU))
		tegra_console_uart_suspend();
}

static void paz00_board_resume(int lp_state, enum resume_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_RESUME_AFTER_CPU))
		tegra_console_uart_resume();
}

static struct tegra_suspend_platform_data paz00_suspend_data = {
	/*
	 * Check power on time and crystal oscillator start time
	 * for appropriate settings.
	 */
	.cpu_timer	= 2000,
	.cpu_off_timer	= 0,
	.suspend_mode	= TEGRA_SUSPEND_LP0,
	.core_timer	= 0x7e7e,
	.core_off_timer = 0x7f,
	.corereq_high	= false,
	.sysclkreq_high	= true,
	.board_suspend = paz00_board_suspend,
	.board_resume = paz00_board_resume,
};

int __init paz00_suspend_init(void)
{
	tegra_init_suspend(&paz00_suspend_data);
	return 0;
}

int __init paz00_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;

	/* configure the power management controller to trigger PMU
	 * interrupts when low */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	i2c_register_board_info(4, paz00_regulators, 1);

	regulator_has_full_constraints();

	return 0;
}
