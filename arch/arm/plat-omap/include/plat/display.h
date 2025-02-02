/*
 * linux/include/asm-arm/arch-omap/display.h
 *
 * Copyright (C) 2008 Nokia Corporation
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ASM_ARCH_OMAP_DISPLAY_H
#define __ASM_ARCH_OMAP_DISPLAY_H

#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <asm/atomic.h>
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <linux/hrtimer.h>


#define DISPC_IRQ_FRAMEDONE		(1 << 0)
#define DISPC_IRQ_VSYNC			(1 << 1)
#define DISPC_IRQ_EVSYNC_EVEN		(1 << 2)
#define DISPC_IRQ_EVSYNC_ODD		(1 << 3)
#define DISPC_IRQ_ACBIAS_COUNT_STAT	(1 << 4)
#define DISPC_IRQ_PROG_LINE_NUM		(1 << 5)
#define DISPC_IRQ_GFX_FIFO_UNDERFLOW	(1 << 6)
#define DISPC_IRQ_GFX_END_WIN		(1 << 7)
#define DISPC_IRQ_PAL_GAMMA_MASK	(1 << 8)
#define DISPC_IRQ_OCP_ERR		(1 << 9)
#define DISPC_IRQ_VID1_FIFO_UNDERFLOW	(1 << 10)
#define DISPC_IRQ_VID1_END_WIN		(1 << 11)
#define DISPC_IRQ_VID2_FIFO_UNDERFLOW	(1 << 12)
#define DISPC_IRQ_VID2_END_WIN		(1 << 13)
#define DISPC_IRQ_SYNC_LOST		(1 << 14)
#define DISPC_IRQ_SYNC_LOST_DIGIT	(1 << 15)
#define DISPC_IRQ_WAKEUP		(1 << 16)
#define DISPC_IRQ_SYNC_LOST_2		(1 << 17)
#define DISPC_IRQ_VSYNC2		(1 << 18)
#define DISPC_IRQ_ACBIAS_COUNT_STAT2	(1 << 21)
#define DISPC_IRQ_VID3_END_WIN		(1 << 19)
#define DISPC_IRQ_VID3_FIFO_UNDERFLOW	(1 << 20)
#define DISPC_IRQ_FRAMEDONE2		(1 << 22)
#define DISPC_IRQ_FRAMEDONE_WB		(1 << 23)
#define DISPC_IRQ_FRAMEDONE_DIG		(1 << 24)
#define DISPC_IRQ_WB_BUF_OVERFLOW	(1 << 25)

struct omap_dss_device;
struct omap_overlay_manager;

enum omap_display_type {
	OMAP_DISPLAY_TYPE_NONE		= 0,
	OMAP_DISPLAY_TYPE_DPI		= 1 << 0,
	OMAP_DISPLAY_TYPE_DBI		= 1 << 1,
	OMAP_DISPLAY_TYPE_SDI		= 1 << 2,
	OMAP_DISPLAY_TYPE_DSI		= 1 << 3,
	OMAP_DISPLAY_TYPE_VENC		= 1 << 4,
	OMAP_DISPLAY_TYPE_HDMI		= 1 << 5,
};

enum omap_plane {
	OMAP_DSS_GFX	= 0,
	OMAP_DSS_VIDEO1	= 1,
	OMAP_DSS_VIDEO2	= 2,
	OMAP_DSS_VIDEO3	= 3,
	OMAP_DSS_WB 	= 4,
};

enum omap_channel {
	OMAP_DSS_CHANNEL_LCD	= 0,
	OMAP_DSS_CHANNEL_DIGIT	= 1,
	OMAP_DSS_CHANNEL_LCD2	= 2,
};

enum omap_color_mode {
	OMAP_DSS_COLOR_CLUT1		= 1 << 0,  /* BITMAP 1 */
	OMAP_DSS_COLOR_CLUT2		= 1 << 1,  /* BITMAP 2 */
	OMAP_DSS_COLOR_CLUT4		= 1 << 2,  /* BITMAP 4 */
	OMAP_DSS_COLOR_CLUT8		= 1 << 3,  /* BITMAP 8 */

	/* also referred to as RGB 12-BPP, 16-bit container  */
	OMAP_DSS_COLOR_RGB12U		= 1 << 4,  /* xRGB12-4444 */
	OMAP_DSS_COLOR_ARGB16		= 1 << 5,  /* ARGB16-4444 */
	OMAP_DSS_COLOR_RGB16		= 1 << 6,  /* RGB16-565 */

	/* also referred to as RGB 24-BPP, 32-bit container */
	OMAP_DSS_COLOR_RGB24U		= 1 << 7,  /* xRGB24-8888 */
	OMAP_DSS_COLOR_RGB24P		= 1 << 8,  /* RGB24-888 */
	OMAP_DSS_COLOR_YUV2		= 1 << 9,  /* YUV2 4:2:2 co-sited */
	OMAP_DSS_COLOR_UYVY		= 1 << 10, /* UYVY 4:2:2 co-sited */
	OMAP_DSS_COLOR_ARGB32		= 1 << 11, /* ARGB32-8888 */
	OMAP_DSS_COLOR_RGBA32		= 1 << 12, /* RGBA32-8888 */

	/* also referred to as RGBx 32 in TRM */
	OMAP_DSS_COLOR_RGBX24		= 1 << 13, /* RGBx24-8888 */
	OMAP_DSS_COLOR_NV12		= 1 << 14, /* NV12 format: YUV 4:2:0 */

	/* also referred to as RGBA12-4444 in TRM */
	OMAP_DSS_COLOR_RGBA16		= 1 << 15, /* RGBA16-4444 */

	OMAP_DSS_COLOR_RGBX12		= 1 << 16, /* RGBx12-4444 */
	OMAP_DSS_COLOR_ARGB16_1555	= 1 << 17, /* ARGB16-1555 */

	/* also referred to as xRGB16-555 in TRM */
	OMAP_DSS_COLOR_XRGB15		= 1 << 18, /* xRGB15-1555 */

	OMAP_DSS_COLOR_GFX_OMAP2 =
		OMAP_DSS_COLOR_CLUT1 | OMAP_DSS_COLOR_CLUT2 |
		OMAP_DSS_COLOR_CLUT4 | OMAP_DSS_COLOR_CLUT8 |
		OMAP_DSS_COLOR_RGB12U | OMAP_DSS_COLOR_RGB16 |
		OMAP_DSS_COLOR_RGB24U | OMAP_DSS_COLOR_RGB24P,

	OMAP_DSS_COLOR_VID_OMAP2 =
		OMAP_DSS_COLOR_RGB16 | OMAP_DSS_COLOR_RGB24U |
		OMAP_DSS_COLOR_RGB24P | OMAP_DSS_COLOR_YUV2 |
		OMAP_DSS_COLOR_UYVY,

	OMAP_DSS_COLOR_GFX_OMAP3 =
		OMAP_DSS_COLOR_CLUT1 | OMAP_DSS_COLOR_CLUT2 |
		OMAP_DSS_COLOR_CLUT4 | OMAP_DSS_COLOR_CLUT8 |
		OMAP_DSS_COLOR_RGB12U | OMAP_DSS_COLOR_ARGB16 |
		OMAP_DSS_COLOR_RGB16 | OMAP_DSS_COLOR_RGB24U |
		OMAP_DSS_COLOR_RGB24P | OMAP_DSS_COLOR_ARGB32 |
		OMAP_DSS_COLOR_RGBA32,

	OMAP_DSS_COLOR_VID1_OMAP3 =
		OMAP_DSS_COLOR_RGB12U | OMAP_DSS_COLOR_RGB16 |
		OMAP_DSS_COLOR_RGB24U | OMAP_DSS_COLOR_RGB24P |
		OMAP_DSS_COLOR_YUV2 | OMAP_DSS_COLOR_UYVY,

	OMAP_DSS_COLOR_VID2_OMAP3 =
		OMAP_DSS_COLOR_RGB12U | OMAP_DSS_COLOR_ARGB16 |
		OMAP_DSS_COLOR_RGB16 | OMAP_DSS_COLOR_RGB24U |
		OMAP_DSS_COLOR_RGB24P | OMAP_DSS_COLOR_YUV2 |
		OMAP_DSS_COLOR_UYVY | OMAP_DSS_COLOR_ARGB32 |
		OMAP_DSS_COLOR_RGBA32,

	OMAP_DSS_COLOR_GFX_OMAP4 =
		OMAP_DSS_COLOR_CLUT1 | OMAP_DSS_COLOR_CLUT2 |
		OMAP_DSS_COLOR_CLUT4 | OMAP_DSS_COLOR_CLUT8 |
		OMAP_DSS_COLOR_RGB12U | OMAP_DSS_COLOR_ARGB16 |
		OMAP_DSS_COLOR_RGB16 | OMAP_DSS_COLOR_RGB24U |
		OMAP_DSS_COLOR_RGB24P | OMAP_DSS_COLOR_ARGB32 |
		OMAP_DSS_COLOR_RGBA32 | OMAP_DSS_COLOR_XRGB15 |
		OMAP_DSS_COLOR_RGBX24 | OMAP_DSS_COLOR_RGBX12 |
		OMAP_DSS_COLOR_ARGB16_1555 | OMAP_DSS_COLOR_RGBA16,

	OMAP_DSS_COLOR_VID_OMAP4 =
		OMAP_DSS_COLOR_NV12 | OMAP_DSS_COLOR_RGBA16 |
		OMAP_DSS_COLOR_RGBX12 | OMAP_DSS_COLOR_ARGB16_1555 |
		OMAP_DSS_COLOR_XRGB15 |
		OMAP_DSS_COLOR_RGB12U | OMAP_DSS_COLOR_ARGB16 |
		OMAP_DSS_COLOR_RGB16 | OMAP_DSS_COLOR_RGB24U |
		OMAP_DSS_COLOR_RGB24P | OMAP_DSS_COLOR_YUV2 |
		OMAP_DSS_COLOR_UYVY | OMAP_DSS_COLOR_ARGB32 |
		OMAP_DSS_COLOR_RGBA32 | OMAP_DSS_COLOR_RGBX24,
};

enum omap_lcd_display_type {
	OMAP_DSS_LCD_DISPLAY_STN,
	OMAP_DSS_LCD_DISPLAY_TFT,
};

enum omap_dss_load_mode {
	OMAP_DSS_LOAD_CLUT_AND_FRAME	= 0,
	OMAP_DSS_LOAD_CLUT_ONLY		= 1,
	OMAP_DSS_LOAD_FRAME_ONLY	= 2,
	OMAP_DSS_LOAD_CLUT_ONCE_FRAME	= 3,
};

enum omap_dss_trans_key_type {
	OMAP_DSS_COLOR_KEY_GFX_DST = 0,
	OMAP_DSS_COLOR_KEY_VID_SRC = 1,
};

enum omap_rfbi_te_mode {
	OMAP_DSS_RFBI_TE_MODE_1 = 1,
	OMAP_DSS_RFBI_TE_MODE_2 = 2,
};

enum omap_panel_config {
	OMAP_DSS_LCD_IVS		= 1 << 0,
	OMAP_DSS_LCD_IHS		= 1 << 1,
	OMAP_DSS_LCD_IPC		= 1 << 2,
	OMAP_DSS_LCD_IEO		= 1 << 3,
	OMAP_DSS_LCD_RF			= 1 << 4,
	OMAP_DSS_LCD_ONOFF		= 1 << 5,
	OMAP_DSS_LCD_TFT		= 1 << 20,
};

enum omap_dss_venc_type {
	OMAP_DSS_VENC_TYPE_COMPOSITE,
	OMAP_DSS_VENC_TYPE_SVIDEO,
};

enum omap_display_caps {
	OMAP_DSS_DISPLAY_CAP_MANUAL_UPDATE	= 1 << 0,
	OMAP_DSS_DISPLAY_CAP_TEAR_ELIM		= 1 << 1,
};

enum omap_dss_color_conv_type {
	OMAP_DSS_COLOR_CONV_BT601_5_LR = 0,
	OMAP_DSS_COLOR_CONV_BT601_5_FR = 0,
	OMAP_DSS_COLOR_CONV_BT709,
};

enum omap_dss_update_mode {
	OMAP_DSS_UPDATE_DISABLED = 0,
	OMAP_DSS_UPDATE_AUTO,
	OMAP_DSS_UPDATE_MANUAL,
};

enum omap_dss_display_state {
	OMAP_DSS_DISPLAY_DISABLED = 0,
	OMAP_DSS_DISPLAY_ACTIVE,
	OMAP_DSS_DISPLAY_SUSPENDED,
	OMAP_DSS_DISPLAY_TRANSITION,
	
	OMAP_DSS_DISPLAY_ACTIVE_NO_DRAW,
};

enum omap_dss_reset_phase {
	OMAP_DSS_RESET_OFF = 1,
	OMAP_DSS_RESET_ON = 2,
	OMAP_DSS_RESET_BOTH = 3,
};

/* XXX perhaps this should be removed */
enum omap_dss_overlay_managers {
	OMAP_DSS_OVL_MGR_LCD,
	OMAP_DSS_OVL_MGR_TV,
	OMAP_DSS_OVL_MGR_LCD2,
};

enum omap_dss_rotation_type {
	OMAP_DSS_ROT_DMA = 0,
	OMAP_DSS_ROT_VRFB = 1,
	OMAP_DSS_ROT_TILER = 2,
};

/* clockwise rotation angle */
enum omap_dss_rotation_angle {
	OMAP_DSS_ROT_0		= 0,
	OMAP_DSS_ROT_90		= 1,
	OMAP_DSS_ROT_180	= 2,
	OMAP_DSS_ROT_270	= 3,
};

enum omap_overlay_caps {
	OMAP_DSS_OVL_CAP_SCALE = 1 << 0,
	OMAP_DSS_OVL_CAP_DISPC = 1 << 1,
};

enum omap_overlay_manager_caps {
	OMAP_DSS_OVL_MGR_CAP_DISPC = 1 << 0,
};

enum omap_overlay_zorder {
	OMAP_DSS_OVL_ZORDER_0	= 0x0,
	OMAP_DSS_OVL_ZORDER_1	= 0x1,
	OMAP_DSS_OVL_ZORDER_2	= 0x2,
	OMAP_DSS_OVL_ZORDER_3	= 0x3,
};

/* Writeback*/
enum omap_writeback_source {
	OMAP_WB_LCD_1_MANAGER	= 0,
	OMAP_WB_LCD_2_MANAGER	= 1,
	OMAP_WB_TV_MANAGER	= 2,
	OMAP_WB_OVERLAY0	= 3,
	OMAP_WB_OVERLAY1	= 4,
	OMAP_WB_OVERLAY2	= 5,
	OMAP_WB_OVERLAY3	= 6,
};

enum omap_writeback_capturemode {
	OMAP_WB_CAPTURE_ALL	= 0x0,
	OMAP_WB_CAPTURE_1	= 0x1,
	OMAP_WB_CAPTURE_1_OF_2	= 0x2,
	OMAP_WB_CAPTURE_1_OF_3	= 0x3,
	OMAP_WB_CAPTURE_1_OF_4	= 0x4,
	OMAP_WB_CAPTURE_1_OF_5	= 0x5,
	OMAP_WB_CAPTURE_1_OF_6	= 0x6,
	OMAP_WB_CAPTURE_1_OF_7	= 0x7,
};

enum device_n_buffer_type {
	OMAP_FLAG_IDEV = 1,	/* interlaced device */
	OMAP_FLAG_IBUF = 2,	/* sequentially interlaced buffer */
	OMAP_FLAG_ISWAP = 4,	/* bottom-top interlacing */

	PBUF_PDEV	= 0,
	PBUF_IDEV	= OMAP_FLAG_IDEV,
	PBUF_IDEV_SWAP	= OMAP_FLAG_IDEV | OMAP_FLAG_ISWAP,
	IBUF_IDEV	= OMAP_FLAG_IBUF | OMAP_FLAG_IDEV,
	IBUF_IDEV_SWAP	= OMAP_FLAG_IBUF | OMAP_FLAG_IDEV | OMAP_FLAG_ISWAP,
	IBUF_PDEV	= OMAP_FLAG_IBUF,
	IBUF_PDEV_SWAP	= OMAP_FLAG_IBUF | OMAP_FLAG_ISWAP,
};

/* Stereoscopic Panel types
 * row, column, overunder, sidebyside options
 * are with respect to native scan order
*/
enum s3d_disp_type {
	S3D_DISP_NONE = 0,
	S3D_DISP_FRAME_SEQ,
	S3D_DISP_ROW_IL,
	S3D_DISP_COL_IL,
	S3D_DISP_PIX_IL,
	S3D_DISP_CHECKB,
	S3D_DISP_OVERUNDER,
	S3D_DISP_SIDEBYSIDE,
};

/* Subsampling direction is based on native panel scan order.
*/
enum s3d_disp_sub_sampling {
	S3D_DISP_SUB_SAMPLE_NONE = 0,
	S3D_DISP_SUB_SAMPLE_V,
	S3D_DISP_SUB_SAMPLE_H,
};

/* Indicates if display expects left view first followed by right or viceversa
 * For row interlaved displays, defines first row view
 * For column interleaved displays, defines first column view
 * For checkerboard, defines first pixel view
 * For overunder, defines top view
 * For sidebyside, defines west view
*/
enum s3d_disp_order {
	S3D_DISP_ORDER_L = 0,
	S3D_DISP_ORDER_R = 1,
};

/* Indicates current view
 * Used mainly for displays that need to trigger a sync signal
*/
enum s3d_disp_view {
	S3D_DISP_VIEW_L = 0,
	S3D_DISP_VIEW_R,
};

/* RFBI */

struct rfbi_timings {
	int cs_on_time;
	int cs_off_time;
	int we_on_time;
	int we_off_time;
	int re_on_time;
	int re_off_time;
	int we_cycle_time;
	int re_cycle_time;
	int cs_pulse_width;
	int access_time;

	int clk_div;

	u32 tim[5];		/* set by rfbi_convert_timings() */

	int converted;
};

void omap_rfbi_write_command(const void *buf, u32 len);
void omap_rfbi_read_data(void *buf, u32 len);
void omap_rfbi_write_data(const void *buf, u32 len);
void omap_rfbi_write_pixels(const void __iomem *buf, int scr_width,
		u16 x, u16 y,
		u16 w, u16 h);
int omap_rfbi_enable_te(bool enable, unsigned line);
int omap_rfbi_setup_te(enum omap_rfbi_te_mode mode,
		unsigned hs_pulse_time, unsigned vs_pulse_time,
		int hs_pol_inv, int vs_pol_inv, int extif_div);

/* DSI */
enum omap_dsi_index {
	DSI1 = 0,
	DSI2 = 1,
};

enum omap_dsi_mode {
	OMAP_DSI_MODE_CMD = 0,
	OMAP_DSI_MODE_VIDEO = 1,
};
void dsi_bus_lock(enum omap_dsi_index ix);
void dsi_bus_unlock(enum omap_dsi_index ix);
int dsi_vc_dcs_write(enum omap_dsi_index ix, int channel,
		u8 *data, int len);
int dsi_vc_dcs_write_0(enum omap_dsi_index ix, int channel,
		u8 dcs_cmd);
int dsi_vc_dcs_write_1(enum omap_dsi_index ix, int channel,
		u8 dcs_cmd, u8 param);
int dsi_vc_dcs_write_nosync(enum omap_dsi_index ix, int channel,
		u8 *data, int len);
int dsi_vc_dcs_read(enum omap_dsi_index ix, int channel,
		u8 dcs_cmd, u8 *buf, int buflen);
int dsi_vc_dcs_read_1(enum omap_dsi_index ix, int channel,
		u8 dcs_cmd, u8 *data);
int dsi_vc_dcs_read_2(enum omap_dsi_index ix, int channel,
		u8 dcs_cmd, u8 *data1, u8 *data2);
int dsi_vc_set_max_rx_packet_size(enum omap_dsi_index ix,
		int channel, u16 len);
int dsi_vc_send_null(enum omap_dsi_index ix, int channel);
int dsi_vc_send_bta_sync(enum omap_dsi_index ix, int channel);


int dsi_enable_dcs_cmd(struct omap_dss_device *dssdev,enum omap_dsi_index ix);
int dsi_disable_dcs_cmd(struct omap_dss_device *dssdev,enum omap_dsi_index ix);


/* Board specific data */
#define PWM2ON		0x03
#define PWM2OFF		0x04
#define TOGGLE3		0x92
#define DSI1_GPIO_27 27
#define DSI2_GPIO_59 59
#define DSI2_GPIO_104 104
#define DSI1_GPIO_102 102
#define HDMI_GPIO_60 60
#define HDMI_GPIO_41 41
#define DLP_4430_GPIO_40	40
#define DLP_4430_GPIO_44	44
#define DLP_4430_GPIO_45	45
#define DLP_4430_GPIO_59	59

#define PROGRESSIVE		0
#define INTERLACED		1

struct omap_dss_board_info {
	int (*get_last_off_on_transaction_id)(struct device *dev);
	int num_devices;
	struct omap_dss_device **devices;
	struct omap_dss_device *default_device;
};
extern void omap_display_init(struct omap_dss_board_info *board_data);

struct omap_display_platform_data{
	char name[16];
	int hwmod_count;
	struct omap_dss_board_info *board_data;
	int (*device_enable)(struct platform_device *pdev);
	int (*device_shutdown)(struct platform_device *pdev);
	int (*device_idle)(struct platform_device *pdev);
};

struct omap_video_timings {
	/* Unit: pixels */
	u16 x_res;
	/* Unit: pixels */
	u16 y_res;
	/* Unit: KHz */
	u32 pixel_clock;
	/* Unit: pixel clocks */
	u16 hsw;	/* Horizontal synchronization pulse width */
	/* Unit: pixel clocks */
	u16 hfp;	/* Horizontal front porch */
	/* Unit: pixel clocks */
	u16 hbp;	/* Horizontal back porch */
	/* Unit: line clocks */
	u16 vsw;	/* Vertical synchronization pulse width */
	/* Unit: line clocks */
	u16 vfp;	/* Vertical front porch */
	/* Unit: line clocks */
	u16 vbp;	/* Vertical back porch */
	u16 hsa;
	u16 vsa;
};

/* Weight coef are set as value * 1000 (if coef = 1 it is set to 1000) */
struct omap_dss_color_weight_coef {
	int rr, rg, rb;
	int gr, gg, gb;
	int br, bg, bb;
};

struct omap_dss_yuv2rgb_conv {
	enum omap_dss_color_conv_type type;
	struct omap_dss_color_weight_coef *weight_coef;
	bool dirty;
};

/* Weight coef are set as value * 1000 (if coef = 1 it is set to 1000) */
#ifdef CONFIG_MACH_LGE_COSMO_DOMASTIC
#define LGE_FW_TDMB
#endif // CONFIG_MACH_LGE_COSMO_DOMASTIC

#ifdef LGE_FW_TDMB
struct omap_ccs_matrix {
	s16 ry;
	s16 rcr;
	s16 rcb;
	s16 gy;
	s16 gcr;
	s16 gcb;
	s16 by;
	s16 bcr;
	s16 bcb;	
};
#endif

#ifdef CONFIG_OMAP2_DSS_VENC
/* Hardcoded timings for tv modes. Venc only uses these to
 * identify the mode, and does not actually use the configs
 * itself. However, the configs should be something that
 * a normal monitor can also show */
extern const struct omap_video_timings omap_dss_pal_timings;
extern const struct omap_video_timings omap_dss_ntsc_timings;
#endif


enum omap_dss_overlay_s3d_type {
	omap_dss_overlay_s3d_none = 0,
	omap_dss_overlay_s3d_top_bottom = 1,
	omap_dss_overlay_s3d_side_by_side = 2,
	omap_dss_overlay_s3d_interlaced = 4,
};

enum omapdss_completion_status {
	DSS_COMPLETION_PROGRAMMED = 0,
	DSS_COMPLETION_DISPLAYED = 4,
	DSS_COMPLETION_CHANGED_SET,
	DSS_COMPLETION_CHANGED_CACHE,
	DSS_COMPLETION_RELEASED = 8,
	DSS_COMPLETION_ECLIPSED_SET,
	DSS_COMPLETION_ECLIPSED_CACHE,
	DSS_COMPLETION_ECLIPSED_SHADOW,
	DSS_COMPLETION_TORN,
};

struct omapdss_ovl_cb {
	/* optional callback method */
	void (*fn)(void *data, int id, int status);
	void *data;
};

struct omap_overlay_info {
	bool enabled;

	u32 paddr;
	void __iomem *vaddr;
	u16 screen_width;
	u16 width;
	u16 height;
	enum omap_color_mode color_mode;
	struct omap_dss_yuv2rgb_conv yuv2rgb_conv;
	u8 rotation;
	enum omap_dss_rotation_type rotation_type;
	bool mirror;

	int status;
	char  manager[16];

	u16 pos_x;
	u16 pos_y;
	u16 out_width;	/* if 0, out_width == width */
	u16 out_height;	/* if 0, out_height == height */
	u8 global_alpha;
	u16 min_x_decim, max_x_decim, min_y_decim, max_y_decim;
	enum omap_overlay_zorder zorder;
	u32 p_uv_addr;	/* for NV12 format */
	enum device_n_buffer_type field;
	u16 pic_height;	/* required for interlacing with cropping */
	bool out_wb; /* true when this overlay only feeds wb pipeline */
	
	enum omap_dss_overlay_s3d_type s3d_type;
	bool pre_mult_alpha;

	struct omapdss_ovl_cb cb;
};

#define DSI1_GPIO_27 27
#define DSI2_GPIO_59 59
#define DSI2_GPIO_104 104
#define DSI1_GPIO_102 102
#define HDMI_GPIO_60 60
#define HDMI_GPIO_41 41
#define DLP_4430_GPIO_40        40
#define DLP_4430_GPIO_44        44
#define DLP_4430_GPIO_45        45
#define DLP_4430_GPIO_59        59

struct omap_overlay {
	struct kobject kobj;
	struct list_head list;

	/* static fields */
	const char *name;
	int id;
	enum omap_color_mode supported_modes;
	enum omap_overlay_caps caps;

	/* dynamic fields */
	struct omap_overlay_manager *manager;
	struct omap_overlay_info info;

	/* if true, info has been changed, but not applied() yet */
	bool info_dirty;

	/* if true, overlay resource is used by an instance of a driver*/
	bool in_use;
	struct mutex lock;

	int (*set_manager)(struct omap_overlay *ovl,
		struct omap_overlay_manager *mgr);
	int (*unset_manager)(struct omap_overlay *ovl);

	int (*set_overlay_info)(struct omap_overlay *ovl,
			struct omap_overlay_info *info);
	void (*get_overlay_info)(struct omap_overlay *ovl,
			struct omap_overlay_info *info);

	int (*wait_for_go)(struct omap_overlay *ovl);
};

struct omap_overlay_manager_info {
	u32 default_color;

	enum omap_dss_trans_key_type trans_key_type;
	u32 trans_key;
	bool trans_enabled;

	bool alpha_enabled;

	struct omapdss_ovl_cb cb;
};

struct omap_overlay_manager {
	struct kobject kobj;
	struct list_head list;

	/* static fields */
	const char *name;
	int id;
	enum omap_overlay_manager_caps caps;
	int num_overlays;
	struct omap_overlay **overlays;
	enum omap_display_type supported_displays;

	/* dynamic fields */
	struct omap_dss_device *device;
	struct omap_overlay_manager_info info;

	bool device_changed;
	/* if true, info has been changed but not applied() yet */
	bool info_dirty;

	int (*set_device)(struct omap_overlay_manager *mgr,
		struct omap_dss_device *dssdev);
	int (*unset_device)(struct omap_overlay_manager *mgr);

	int (*set_manager_info)(struct omap_overlay_manager *mgr,
			struct omap_overlay_manager_info *info);
	void (*get_manager_info)(struct omap_overlay_manager *mgr,
			struct omap_overlay_manager_info *info);

	int (*apply)(struct omap_overlay_manager *mgr);
	int (*wait_for_go)(struct omap_overlay_manager *mgr);
	int (*wait_for_vsync)(struct omap_overlay_manager *mgr);

	int (*enable)(struct omap_overlay_manager *mgr);
	int (*disable)(struct omap_overlay_manager *mgr);
};

enum omap_writeback_source_type {
	OMAP_WB_SOURCE_OVERLAY	= 0,
	OMAP_WB_SOURCE_MANAGER	= 1
};


struct omap_writeback_info {
		bool					enabled;
		bool					info_dirty;
		enum omap_writeback_source		source;
		enum omap_writeback_source_type 	source_type;
		unsigned long				width;
		unsigned long				height;
		unsigned long				out_width;
		unsigned long				out_height;
		enum omap_color_mode			dss_mode;
		enum omap_writeback_capturemode 	capturemode;
		unsigned long				paddr;
		/* NV12 support*/
		unsigned long				puv_addr;
		unsigned int				line_skip;

};

struct omap_writeback {
	struct kobject kobj;
	struct list_head list;
	bool								enabled;
	bool								info_dirty;
	bool								first_time;
	/* mutex to control access to wb data */
	struct mutex lock;
	struct omap_writeback_info info;
	bool (*check_wb)(struct omap_writeback *wb);

	int (*set_wb_info)(struct omap_writeback *wb, struct omap_writeback_info *info);
	void (*get_wb_info)(struct omap_writeback *wb, struct omap_writeback_info *info);

};

struct s3d_disp_info {
	enum s3d_disp_type type;
	enum s3d_disp_sub_sampling sub_samp;
	enum s3d_disp_order order;
	/* Gap between left and right views
	 * For over/under units are lines
	 * For sidebyside units are pixels
	  *For other types ignored*/
	unsigned int gap;
};

/* info for delayed update */
struct omap_dss_sched_update {
	u16 x, y, w, h;		/* update window */
	struct work_struct work;
	bool scheduled;		/* scheduled */
	bool waiting;		/* waiting to update */
};

struct omap_dss_device {
	struct device dev;

	enum omap_display_type type;

	union {
		struct {
			u8 data_lines;
		} dpi;

		struct {
			u8 channel;
			u8 data_lines;
		} rfbi;

		struct {
			u8 datapairs;
		} sdi;

		struct {
			u8 clk_lane;
			u8 clk_pol;
			u8 data1_lane;
			u8 data1_pol;
			u8 data2_lane;
			u8 data2_pol;
			u8 num_data_lanes;
			struct {
				u16 regn;
				u16 regm;
				u16 regm_dispc;
				u16 regm_dsi;

				u16 lp_clk_div;

				u16 lck_div;
				u16 pck_div;
			} div;

			bool ext_te;
			u8 ext_te_gpio;
                        enum omap_dsi_mode mode;
 
                        struct {
                                u16 hbp;
                                u16 hfp;
                                u16 hsa;
                                u16 vbp;
                                u16 vfp;
                                u16 vsa;
                        } timings;
		} dsi;

		struct {
			enum omap_dss_venc_type type;
			bool invert_polarity;
		} venc;
	} phy;

	struct {
		struct omap_video_timings timings;

		int acbi;	/* ac-bias pin transitions per interrupt */
		/* Unit: line clocks */
		int acb;	/* ac-bias pin frequency */

		enum omap_panel_config config;
		struct s3d_disp_info s3d_info;
	} panel;

	struct {
		u8 pixel_size;
		struct rfbi_timings rfbi_timings;
	} ctrl;

	int reset_gpio;

	int max_backlight_level;

	const char *name;

	/* used to match device to driver */
	const char *driver_name;

	void *data;

	struct omap_dss_driver *driver;

	/* helper variable for driver suspend/resume */
	bool activate_after_resume;

	enum omap_display_caps caps;

	struct omap_overlay_manager *manager;
	struct omap_writeback *wb_manager;

	enum omap_dss_display_state state;
	enum omap_channel channel;

	struct blocking_notifier_head notifier;

	/* support for scheduling subsequent update */
	struct omap_dss_sched_update sched_update;

#ifdef LGE_FW_TDMB
	int (*set_ccs)(struct omap_dss_device * dssdev, struct omap_ccs_matrix * ccs_info);
#endif

	/* platform specific  */
	int (*platform_enable)(struct omap_dss_device *dssdev);
	void (*platform_disable)(struct omap_dss_device *dssdev);
	int (*set_backlight)(struct omap_dss_device *dssdev, int level);
	int (*get_backlight)(struct omap_dss_device *dssdev);
};

struct omap_dss_driver {
	struct device_driver driver;

	int (*probe)(struct omap_dss_device *);
	void (*remove)(struct omap_dss_device *);

	int (*enable)(struct omap_dss_device *display);
	void (*disable)(struct omap_dss_device *display);
	int (*suspend)(struct omap_dss_device *display);
	int (*resume)(struct omap_dss_device *display);
	int (*run_test)(struct omap_dss_device *display, int test);

	int (*set_update_mode)(struct omap_dss_device *dssdev,
			enum omap_dss_update_mode);
	enum omap_dss_update_mode (*get_update_mode)(
			struct omap_dss_device *dssdev);

	int (*update)(struct omap_dss_device *dssdev,
			       u16 x, u16 y, u16 w, u16 h);
	int (*sched_update)(struct omap_dss_device *dssdev,
			       u16 x, u16 y, u16 w, u16 h);
	int (*sync)(struct omap_dss_device *dssdev);

	int (*enable_te)(struct omap_dss_device *dssdev, bool enable);
	int (*get_te)(struct omap_dss_device *dssdev);

	u8 (*get_rotate)(struct omap_dss_device *dssdev);
	int (*set_rotate)(struct omap_dss_device *dssdev, u8 rotate);

	bool (*get_mirror)(struct omap_dss_device *dssdev);
	int (*set_mirror)(struct omap_dss_device *dssdev, bool enable);

	int (*memory_read)(struct omap_dss_device *dssdev,
			void *buf, size_t size,
			u16 x, u16 y, u16 w, u16 h);

	void (*get_resolution)(struct omap_dss_device *dssdev,
			u16 *xres, u16 *yres);
	void (*get_dimension)(struct omap_dss_device *dssdev,
			u32 *width, u32 *height);
	int (*get_recommended_bpp)(struct omap_dss_device *dssdev);

	int (*check_timings)(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings);
	void (*set_timings)(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings);
	void (*get_timings)(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings);

	int (*set_wss)(struct omap_dss_device *dssdev, u32 wss);
	u32 (*get_wss)(struct omap_dss_device *dssdev);

	/*
	 * used for sysfs control for panels that are not fully enabled
	 * when powered on
	 */
	bool (*smart_is_enabled)(struct omap_dss_device *dssdev);
	int (*smart_enable)(struct omap_dss_device *display);

/*HDMI specific */
	void (*get_edid)(struct omap_dss_device *dssdev);
	void (*set_custom_edid_timing_code)(struct omap_dss_device *dssdev,
			int mode, int code);
	int (*hpd_enable)(struct omap_dss_device *dssdev);
	/* resets display.  returns status (of reenabling the display).*/
	int (*reset)(struct omap_dss_device *dssdev,
					enum omap_dss_reset_phase phase);
	
	int (*hpd_disable)(struct omap_dss_device *dssdev);

	/* S3D specific */
	/* Used for displays that can switch 3D mode on/off
	3D only displays should return non-zero value when trying to disable */
	int (*enable_s3d)(struct omap_dss_device *dssdev, bool enable);
	/* 3D only panels should return true always */
	bool (*get_s3d_enabled)(struct omap_dss_device *dssdev);
	/* Only used for frame sequential displays*/
	int (*set_s3d_view)(struct omap_dss_device *dssdev, enum s3d_disp_view view);
	/*Some displays may accept multiple 3D packing formats (like HDMI)
	 *hence we add capability to choose the most optimal one given a source
	 *Returns non-zero if the type was not supported*/
	int (*set_s3d_disp_type)(struct omap_dss_device *dssdev, struct s3d_disp_info *info);
	int (*get_s3d_disp_type)(struct omap_dss_device *dssdev, struct s3d_disp_info *info);
};

struct pico_platform_data {
	u8 gpio_intr;
};

int omap_dss_register_driver(struct omap_dss_driver *);
void omap_dss_unregister_driver(struct omap_dss_driver *);

int omap_dss_register_device(struct omap_dss_device *);
void omap_dss_unregister_device(struct omap_dss_device *);

void omap_dss_get_device(struct omap_dss_device *dssdev);
void omap_dss_put_device(struct omap_dss_device *dssdev);
#define for_each_dss_dev(d) while ((d = omap_dss_get_next_device(d)) != NULL)
struct omap_dss_device *omap_dss_get_next_device(struct omap_dss_device *from);
struct omap_dss_device *omap_dss_find_device(void *data,
		int (*match)(struct omap_dss_device *dssdev, void *data));

int omap_dss_start_device(struct omap_dss_device *dssdev);
void omap_dss_stop_device(struct omap_dss_device *dssdev);

/* the event id of the event that occurred is passed in as the second arg
 * to the notifier function, and the dssdev is passed as the third.
 */
enum omap_dss_event {
	OMAP_DSS_SIZE_CHANGE
	/* possibly add additional events, like hot-plug connect/disconnect */
};

void omap_dss_notify(struct omap_dss_device *dssdev, enum omap_dss_event evt);
void omap_dss_add_notify(struct omap_dss_device *dssdev,
			struct notifier_block *nb);
void omap_dss_remove_notify(struct omap_dss_device *dssdev,
			struct notifier_block *nb);


int omap_dss_get_num_overlay_managers(void);
struct omap_overlay_manager *omap_dss_get_overlay_manager(int num);
bool dss_ovl_manually_updated(struct omap_overlay *ovl);
static inline bool dssdev_manually_updated(struct omap_dss_device *dev)
{
	return dev->caps & OMAP_DSS_DISPLAY_CAP_MANUAL_UPDATE &&
		dev->driver->get_update_mode(dev) != OMAP_DSS_UPDATE_AUTO;
}

int omap_dss_get_num_overlays(void);
struct omap_overlay *omap_dss_get_overlay(int num);
struct omap_writeback *omap_dss_get_wb(int num);

void omapdss_default_get_resolution(struct omap_dss_device *dssdev,
			u16 *xres, u16 *yres);
int omapdss_default_get_recommended_bpp(struct omap_dss_device *dssdev);
bool dispc_go_busy(enum omap_channel channel);
void dispc_go(enum omap_channel channel);
typedef void (*omap_dispc_isr_t) (void *arg, u32 mask);
int omap_dispc_register_isr(omap_dispc_isr_t isr, void *arg, u32 mask);
int omap_dispc_unregister_isr(omap_dispc_isr_t isr, void *arg, u32 mask);

bool dispc_go_busy(enum omap_channel channel);
void dispc_go(enum omap_channel channel);

int omap_dispc_wait_for_irq_timeout(u32 irqmask, unsigned long timeout);
int omap_dispc_wait_for_irq_interruptible_timeout(u32 irqmask,
			unsigned long timeout);

#define to_dss_driver(x) container_of((x), struct omap_dss_driver, driver)
#define to_dss_device(x) container_of((x), struct omap_dss_device, dev)

int omapdss_display_enable(struct omap_dss_device *dssdev);
void omapdss_display_disable(struct omap_dss_device *dssdev);

void omapdss_dsi_vc_enable_hs(enum omap_dsi_index ix, int channel,
			bool enable);
int omapdss_dsi_enable_te(struct omap_dss_device *dssdev, bool enable);

int omap_dsi_sched_update_lock(struct omap_dss_device *dssdev,
					u16 x, u16 y, u16 w, u16 h);
int omap_dsi_prepare_update(struct omap_dss_device *dssdev,
			u16 *x, u16 *y, u16 *w, u16 *h,
			bool enlarge_update_area);
int omap_dsi_update(struct omap_dss_device *dssdev,
			int channel,
			u16 x, u16 y, u16 w, u16 h,
			void (*callback)(int, void *), void *data);

int omapdss_dsi_display_enable(struct omap_dss_device *dssdev);
void omapdss_dsi_display_disable(struct omap_dss_device *dssdev);
bool omap_dsi_recovery_state(enum omap_dsi_index ix);

int omapdss_dpi_display_enable(struct omap_dss_device *dssdev);
void omapdss_dpi_display_disable(struct omap_dss_device *dssdev);
void dpi_set_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings);
int dpi_check_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings);

int omapdss_sdi_display_enable(struct omap_dss_device *dssdev);
void omapdss_sdi_display_disable(struct omap_dss_device *dssdev);

int omapdss_rfbi_display_enable(struct omap_dss_device *dssdev);
void omapdss_rfbi_display_disable(struct omap_dss_device *dssdev);
int omap_rfbi_prepare_update(struct omap_dss_device *dssdev,
			u16 *x, u16 *y, u16 *w, u16 *h);
int omap_rfbi_update(struct omap_dss_device *dssdev,
			u16 x, u16 y, u16 w, u16 h,
			void (*callback)(void *), void *data);

void change_base_address(int id, u32 p_uv_addr);
bool is_hdmi_interlaced(void);

struct cosmo_panel_data {
		struct backlight_device *bldev;

		unsigned long	hw_guard_end;	/* next value of jiffies when we can
						 * issue the next sleep in/out command
						 */
		unsigned long	hw_guard_wait;	/* max guard time in jiffies */

		struct omap_dss_device *dssdev;

		bool enabled;
		u8 rotate;
		bool mirror;

		bool te_enabled;
		bool use_ext_te;
		struct completion te_completion;

		bool use_dsi_bl;

		bool intro_printed;

		struct workqueue_struct *esd_wq;
		struct delayed_work esd_work;

		struct i2c_client *barrier_i2c[2];
		struct hrtimer	   timer;

		bool barrier_enabled;

};


#endif
