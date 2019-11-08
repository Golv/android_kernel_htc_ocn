/*
 * Copyright (C) 2010 - 2016 Novatek, Inc.
 *
 * $Revision: 8837 $
 * $Date: 2017-01-17 20:44:24 +0800 (週二, 17 一月 2017) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "nt36xxx.h"
#include "nt36xxx_mp_ctrlram.h"

#if NVT_TOUCH_MP

#define NORMAL_MODE 0x00
#define TEST_MODE_1 0x21
#define TEST_MODE_2 0x22
#define MP_MODE_CC 0x41
#define FREQ_HOP_DISABLE 0x66
#define FREQ_HOP_ENABLE 0x65

#define SHORT_TEST_CSV_FILE "/data/ShortTest.csv"
#define OPEN_TEST_CSV_FILE "/data/OpenTest.csv"
#define FW_RAWDATA_CSV_FILE "/data/FWMutualTest.csv"
#define FW_CC_CSV_FILE "/data/FWCCTest.csv"
#define NOISE_TEST_CSV_FILE "/data/NoiseTest.csv"


static const int32_t TOTAL_AFE_CNT = 32;
static const int32_t TOTAL_COL_CNT = 18;

static uint8_t RecordResult_Short[IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE] = {0};
static uint8_t RecordResult_Open[IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE] = {0};
#if NVT_TOUCH_MP_FW
static uint8_t RecordResult_FWMutual[IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE] = {0};
static uint8_t RecordResult_FW_CC[IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE] = {0};
static uint8_t RecordResult_FW_DiffMax[IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE] = {0};
static uint8_t RecordResult_FW_DiffMin[IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE] = {0};
#endif /* #if NVT_TOUCH_MP_FW */

static int32_t TestResult_Short = 0;
static int32_t TestResult_Open = 0;
#if NVT_TOUCH_MP_FW
static int32_t TestResult_FW_Rawdata = 0;
static int32_t TestResult_FWMutual = 0;
static int32_t TestResult_FW_CC = 0;
static int32_t TestResult_Noise = 0;
static int32_t TestResult_FW_DiffMax = 0;
static int32_t TestResult_FW_DiffMin = 0;
#endif /* #if NVT_TOUCH_MP_FW */

static int32_t RawData_Short[IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE] = {0};
static int32_t RawData_Open[IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE] = {0};
#if NVT_TOUCH_MP_FW
static int32_t RawData_Diff[IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE] = {0};
static int32_t RawData_Diff_Min[IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE] = {32767};
static int32_t RawData_Diff_Max[IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE] = {-32768};
static int32_t RawData_FWMutual[IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE] = {0};
static int32_t RawData_FW_CC[IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE] = {0};
#endif /* #if NVT_TOUCH_MP_FW */

struct test_cmd {
	uint32_t addr;
	uint8_t len;
	uint8_t data[64];
};

static int8_t nvt_mp_isInitialed = 0;
static struct proc_dir_entry *NVT_proc_selftest_entry = NULL;
static int8_t nvt_mp_test_result_printed = 0;

extern struct nvt_ts_data *ts;
extern int32_t CTP_I2C_READ(struct i2c_client *client, uint16_t address, uint8_t *buf, uint16_t len);
extern int32_t CTP_I2C_WRITE(struct i2c_client *client, uint16_t address, uint8_t *buf, uint16_t len);
extern void nvt_sw_reset_idle(void);
extern void nvt_bootloader_reset(void);
extern int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state);
extern int32_t nvt_clear_fw_status(void);
extern int32_t nvt_check_fw_status(void);
extern void nvt_change_mode(uint8_t mode);
extern int8_t nvt_get_fw_info(void);
extern uint8_t nvt_get_fw_pipe(void);
extern void nvt_read_mdata(uint32_t xdata_addr, uint32_t xdata_btn_addr);
extern void nvt_get_mdata(int32_t *buf, uint8_t *m_x_num, uint8_t *m_y_num);

typedef enum rawdata_type {
	E_RawdataType_Short,
	E_RawdataType_Open,
	E_RawdataType_Last
} rawdata_type_e;


/*******************************************************
Description:
	Novatek touchscreen self-test criteria print function.

return:
	n.a.
*******************************************************/
static void nvt_print_lmt_array(int32_t *array, int32_t x_ch, int32_t y_ch)
{
	int32_t i = 0;
	int32_t j = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	for (j = 0; j < y_ch; j++) {
		for(i = 0; i < x_ch; i++) {
			printk("%5d, ", array[j * x_ch + i]);
		}
		printk("\n");
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		printk("%5d, ", array[y_ch * x_ch + k]);
	}
	printk("\n");
#endif /* #if TOUCH_KEY_NUM > 0 */
}

static void nvt_print_criteria(void)
{
	NVT_LOG("++\n");

	//---PS_Config_Lmt_Short_Rawdata---
	printk("PS_Config_Lmt_Short_Rawdata_P: %5d\n", PS_Config_Lmt_Short_Rawdata_P);
	printk("PS_Config_Lmt_Short_Rawdata_N: %5d\n", PS_Config_Lmt_Short_Rawdata_N);
#if TOUCH_KEY_NUM > 0
	printk("PS_Config_Lmt_Key_Short_Rawdata_P: %5d\n", PS_Config_Lmt_Key_Short_Rawdata_P);
	printk("PS_Config_Lmt_Key_Short_Rawdata_N: %5d\n", PS_Config_Lmt_Key_Short_Rawdata_N);
#endif /* #if TOUCH_KEY_NUM > 0 */

	//---PS_Config_Lmt_Open_Rawdata---
	printk("PS_Config_Lmt_Open_Rawdata_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_Open_Rawdata_P, X_Channel, Y_Channel);
	printk("PS_Config_Lmt_Open_Rawdata_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_Open_Rawdata_N, X_Channel, Y_Channel);

#if NVT_TOUCH_MP_FW
	//---PS_Config_Lmt_FW_Rawdata---
	printk("PS_Config_Lmt_FW_Rawdata_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FW_Rawdata_P, X_Channel, Y_Channel);
	printk("PS_Config_Lmt_FW_Rawdata_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FW_Rawdata_N, X_Channel, Y_Channel);

	//---PS_Config_Lmt_FW_CC---
	printk("PS_Config_Lmt_FW_CC_P: %5d\n", PS_Config_Lmt_FW_CC_P);
	printk("PS_Config_Lmt_FW_CC_N: %5d\n", PS_Config_Lmt_FW_CC_N);
#if TOUCH_KEY_NUM > 0
	printk("PS_Config_Lmt_Key_FW_CC_P: %5d\n", PS_Config_Lmt_Key_FW_CC_P);
	printk("PS_Config_Lmt_Key_FW_CC_N: %5d\n", PS_Config_Lmt_Key_FW_CC_N);
#endif /* #if TOUCH_KEY_NUM > 0 */

	//---PS_Config_Lmt_FW_Diff---
	printk("PS_Config_Lmt_FW_Diff_P: %5d\n", PS_Config_Lmt_FW_Diff_P);
	printk("PS_Config_Lmt_FW_Diff_N: %5d\n", PS_Config_Lmt_FW_Diff_N);
#if TOUCH_KEY_NUM > 0
	printk("PS_Config_Lmt_Key_FW_Diff_P: %5d\n", PS_Config_Lmt_Key_FW_Diff_P);
	printk("PS_Config_Lmt_Key_FW_Diff_N: %5d\n", PS_Config_Lmt_Key_FW_Diff_N);
#endif /* #if TOUCH_KEY_NUM > 0 */
#endif /* #if NVT_TOUCH_MP_FW */

	NVT_LOG("--\n");
}

static int32_t nvt_save_rawdata_to_csv(int32_t *rawdata, uint8_t x_ch, uint8_t y_ch, const char *file_path, uint32_t offset)
{
	int32_t x = 0;
	int32_t y = 0;
	int32_t iArrayIndex = 0;
	struct file *fp = NULL;
	char *fbufp = NULL;
	mm_segment_t org_fs;
	int32_t write_ret = 0;
	uint32_t output_len = 0;
	loff_t pos = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
	int32_t keydata_output_offset = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	printk("%s:++\n", __func__);
	fbufp = (char *)kzalloc(8192, GFP_KERNEL);
	if (!fbufp) {
		NVT_ERR("kzalloc for fbufp failed!\n");
		return -ENOMEM;
	}

	for (y = 0; y < y_ch; y++) {
		for (x = 0; x < x_ch; x++) {
			iArrayIndex = y * x_ch + x;
			printk("%5d, ", rawdata[iArrayIndex]);
			snprintf(fbufp + iArrayIndex * 7 + y * 2, sizeof(fbufp), "%5d, ", rawdata[iArrayIndex]);
		}
		printk("\n");
		snprintf(fbufp + (iArrayIndex + 1) * 7 + y * 2, sizeof(fbufp), "\r\n");
	}
#if TOUCH_KEY_NUM > 0
	keydata_output_offset = y_ch * x_ch * 7 + y_ch * 2;
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = y_ch * x_ch + k;
		printk("%5d, ", rawdata[iArrayIndex]);
		snprintf(fbufp + keydata_output_offset + k * 7, sizeof(fbufp), "%5d, ", rawdata[iArrayIndex]);
	}
	printk("\n");
	snprintf(fbufp + y_ch * x_ch * 7 + y_ch * 2 + Key_Channel * 7, sizeof(fbufp), "\r\n");
#endif /* #if TOUCH_KEY_NUM > 0 */

	org_fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(file_path, O_RDWR | O_CREAT, 0644);
	if (fp == NULL || IS_ERR(fp)) {
		NVT_ERR("open %s failed\n", file_path);
		set_fs(org_fs);
		if (fbufp) {
			kfree(fbufp);
			fbufp = NULL;
		}
		return -1;
	}

#if TOUCH_KEY_NUM > 0
	output_len = y_ch * x_ch * 7 + y_ch * 2 + Key_Channel * 7 + 2;
#else
	output_len = y_ch * x_ch * 7 + y_ch * 2;
#endif /* #if TOUCH_KEY_NUM > 0 */
	pos = offset;
	write_ret = vfs_write(fp, (char __user *)fbufp, output_len, &pos);
	if (write_ret <= 0) {
		NVT_ERR("write %s failed\n", file_path);
		set_fs(org_fs);
		if (fp) {
			filp_close(fp, NULL);
			fp = NULL;
		}
		if (fbufp) {
			kfree(fbufp);
			fbufp = NULL;
		}
		return -1;
	}

	set_fs(org_fs);
	if (fp) {
		filp_close(fp, NULL);
		fp = NULL;
	}
	if (fbufp) {
		kfree(fbufp);
		fbufp = NULL;
	}

	printk("%s:--\n", __func__);
	return 0;
}

static int32_t nvt_polling_hand_shake_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 50;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		buf[0] = 0xFF;
		buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
		buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);

		if ((buf[1] == 0xA0) || (buf[1] == 0xA1))
			break;

		msleep(10);
	}

	if (i >= retry) {
		NVT_ERR("polling hand shake status failed, buf[1]=0x%02X\n", buf[1]);
		return -1;
	} else {
		return 0;
	}
}

static int8_t nvt_switch_FreqHopEnDis(uint8_t FreqHopEnDis)
{
	uint8_t buf[8] = {0};
	uint8_t retry = 0;
	int8_t ret = 0;

	NVT_LOG("++\n");

	for (retry = 0; retry < 5; retry++) {
		//---set xdata index to EVENT BUF ADDR---
		buf[0] = 0xFF;
		buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
		buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

		//---switch FreqHopEnDis---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = FreqHopEnDis;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);

		msleep(20);

		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);

		if (buf[1] == 0x00)
			break;

		msleep(10);
	}

	if (unlikely(retry == 5)) {
		NVT_ERR("switch FreqHopEnDis 0x%02X failed, buf[1]=0x%02X\n", FreqHopEnDis, buf[1]);
		ret = -1;
	}

	NVT_LOG("--\n");

	return ret;
}

#if NVT_TOUCH_MP_FW
static int32_t nvt_read_baseline(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
	int32_t key_idx = -1;
	int16_t xdata_btn_tmp[IC_KEY_CFG_SIZE] = {0};
#endif /* #if TOUCH_KEY_NUM > 0 */

	NVT_LOG("++\n");

	nvt_read_mdata(ts->mmap->BASELINE_ADDR, ts->mmap->BASELINE_BTN_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			xdata[iArrayIndex] =  (int16_t)xdata[iArrayIndex];
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		xdata_btn_tmp[k] = (int16_t)xdata[y_num * x_num + k];
	}
	key_idx = -1;
	for (k = 0; k < IC_KEY_CFG_SIZE; k++) {
		if (AIN_KEY[k] != 0xFF) {
			key_idx++;
			xdata[Y_Channel * X_Channel + AIN_KEY[k]] = xdata_btn_tmp[key_idx];
		}
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	printk("%s:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel, FW_RAWDATA_CSV_FILE, 0) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

	NVT_LOG("--\n");
	return 0;
}

static int32_t nvt_read_CC(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
	int32_t key_idx = -1;
	int16_t xdata_btn_tmp[IC_KEY_CFG_SIZE] = {0};
#endif /* #if TOUCH_KEY_NUM > 0 */

	NVT_LOG("++\n");

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			xdata[iArrayIndex] =  (int16_t)xdata[iArrayIndex];
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		xdata_btn_tmp[k] = (int16_t)xdata[y_num * x_num + k];
	}
	key_idx = -1;
	for (k = 0; k < IC_KEY_CFG_SIZE; k++) {
		if (AIN_KEY[k] != 0xFF) {
			key_idx++;
			xdata[Y_Channel * X_Channel + AIN_KEY[k]] = xdata_btn_tmp[key_idx];
		}
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	printk("%s:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel, FW_CC_CSV_FILE, 0) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

	NVT_LOG("--\n");
	return 0;
}

static void nvt_enable_noise_collect(int32_t frame_num)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

	//---enable short test---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x47;
	buf[2] = 0xAA;
	buf[3] = frame_num;
	buf[4] = 0x00;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 5);
}

static int32_t nvt_read_fw_noise(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
	int32_t frame_num = 0;
	uint32_t rawdata_diff_min_offset = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
	int32_t key_idx = -1;
	int16_t xdata_btn_tmp[IC_KEY_CFG_SIZE] = {0};
#endif /* #if TOUCH_KEY_NUM > 0 */

	NVT_LOG("++\n");

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		return -EAGAIN;
	}

	frame_num = PS_Config_Diff_Test_Frame / 10;
	if (frame_num <= 0)
		frame_num = 1;
	printk("%s: frame_num=%d\n", __func__, frame_num);
	nvt_enable_noise_collect(frame_num);
	// need wait PS_Config_Diff_Test_Frame * 8.3ms
	msleep(frame_num * 83);

	if (nvt_polling_hand_shake_status()) {
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			RawData_Diff_Max[iArrayIndex] = (int8_t)((xdata[iArrayIndex] >> 8) & 0xFF);
			RawData_Diff_Min[iArrayIndex] = (int8_t)(xdata[iArrayIndex] & 0xFF);
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		xdata_btn_tmp[k] = (int16_t)xdata[y_num * x_num + k];
	}
	key_idx = -1;
	for (k = 0; k < IC_KEY_CFG_SIZE; k++) {
		if (AIN_KEY[k] != 0xFF) {
			key_idx++;
			RawData_Diff_Max[Y_Channel * X_Channel + AIN_KEY[k]] = (int8_t)((xdata_btn_tmp[key_idx] >> 8) & 0xFF);
			RawData_Diff_Min[Y_Channel * X_Channel + AIN_KEY[k]] = (int8_t)(xdata_btn_tmp[key_idx] & 0xFF);
		}
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	printk("%s:RawData_Diff_Max:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(RawData_Diff_Max, X_Channel, Y_Channel, NOISE_TEST_CSV_FILE, 0) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

#if TOUCH_KEY_NUM > 0
	rawdata_diff_min_offset = Y_Channel * X_Channel * 7 + Y_Channel * 2 + Key_Channel * 7 + 2;
#else
	rawdata_diff_min_offset = Y_Channel * X_Channel * 7 + Y_Channel * 2;
#endif /* #if TOUCH_KEY_NUM > 0 */
	printk("%s:RawData_Diff_Min:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(RawData_Diff_Min, X_Channel, Y_Channel, NOISE_TEST_CSV_FILE, rawdata_diff_min_offset) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

	NVT_LOG("--\n");

	return 0;
}
#endif /* #if NVT_TOUCH_MP_FW */

static void nvt_enable_open_test(void)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

	//---enable short test---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x45;
	buf[2] = 0xAA;
	buf[3] = 0x02;
	buf[4] = 0x00;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 5);
}

static void nvt_enable_short_test(void)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

	//---enable short test---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x43;
	buf[2] = 0xAA;
	buf[3] = 0x02;
	buf[4] = 0x00;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 5);
}

static int32_t nvt_read_fw_open(int32_t *xdata)
{
	uint32_t raw_pipe_addr = 0;
	uint8_t *rawdata_buf = NULL;
	uint32_t x = 0;
	uint32_t y = 0;
	uint8_t buf[128] = {0};
#if TOUCH_KEY_NUM > 0
	uint32_t raw_btn_pipe_addr = 0;
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	NVT_LOG("++\n");

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		return -EAGAIN;
	}

	nvt_enable_open_test();

	if (nvt_polling_hand_shake_status()) {
		return -EAGAIN;
	}

#if TOUCH_KEY_NUM > 0
	rawdata_buf = (uint8_t *)kzalloc((IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE) * 2, GFP_KERNEL);
#else
	rawdata_buf = (uint8_t *)kzalloc(IC_X_CFG_SIZE * IC_Y_CFG_SIZE * 2, GFP_KERNEL);
#endif /* #if TOUCH_KEY_NUM > 0 */
	if (!rawdata_buf) {
		NVT_ERR("kzalloc for rawdata_buf failed!\n");
		return -ENOMEM;
	}

	if (nvt_get_fw_pipe() == 0)
		raw_pipe_addr = ts->mmap->RAW_PIPE0_ADDR;
	else
		raw_pipe_addr = ts->mmap->RAW_PIPE1_ADDR;

	for (y = 0; y < IC_Y_CFG_SIZE; y++) {
		//---change xdata index---
		buf[0] = 0xFF;
		buf[1] = (uint8_t)(((raw_pipe_addr + y * IC_X_CFG_SIZE * 2) >> 16) & 0xFF);
		buf[2] = (uint8_t)(((raw_pipe_addr + y * IC_X_CFG_SIZE * 2) >> 8) & 0xFF);
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);
		buf[0] = (uint8_t)((raw_pipe_addr + y * IC_X_CFG_SIZE * 2) & 0xFF);
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, IC_X_CFG_SIZE * 2 + 1);
		memcpy(rawdata_buf + y * IC_X_CFG_SIZE * 2, buf + 1, IC_X_CFG_SIZE * 2);
	}
#if TOUCH_KEY_NUM > 0
	if (nvt_get_fw_pipe() == 0)
		raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE0_ADDR;
	else
		raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE1_ADDR;

	//---change xdata index---
	buf[0] = 0xFF;
	buf[1] = (uint8_t)((raw_btn_pipe_addr >> 16) & 0xFF);
	buf[2] = (uint8_t)((raw_btn_pipe_addr >> 8) & 0xFF);
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);
	buf[0] = (uint8_t)(raw_btn_pipe_addr & 0xFF);
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, IC_KEY_CFG_SIZE * 2 + 1);
	memcpy(rawdata_buf + IC_Y_CFG_SIZE * IC_X_CFG_SIZE * 2, buf + 1, IC_KEY_CFG_SIZE * 2);
#endif /* #if TOUCH_KEY_NUM > 0 */

	for (y = 0; y < IC_Y_CFG_SIZE; y++) {
		for (x = 0; x < IC_X_CFG_SIZE; x++) {
			if ((AIN_Y[y] != 0xFF) && (AIN_X[x] != 0xFF)) {
				xdata[AIN_Y[y] * X_Channel + AIN_X[x]] = (int16_t)((rawdata_buf[(y * IC_X_CFG_SIZE + x) * 2] + 256 * rawdata_buf[(y * IC_X_CFG_SIZE + x) * 2 + 1]));
			}
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < IC_KEY_CFG_SIZE; k++) {
		if (AIN_KEY[k] != 0xFF)
			xdata[Y_Channel * X_Channel + AIN_KEY[k]] = (int16_t)(rawdata_buf[(IC_Y_CFG_SIZE * IC_X_CFG_SIZE + k) * 2] + 256 * rawdata_buf[(IC_Y_CFG_SIZE * IC_X_CFG_SIZE + k) * 2 + 1]);
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	if (rawdata_buf) {
		kfree(rawdata_buf);
		rawdata_buf = NULL;
	}

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);


	printk("%s:\n", __func__);
	// Save RawData to CSV file
	if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel, OPEN_TEST_CSV_FILE, 0) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

	NVT_LOG("--\n");

	return 0;
}

static int32_t nvt_read_fw_short(int32_t *xdata)
{
	uint32_t raw_pipe_addr = 0;
	uint8_t *rawdata_buf = NULL;
	uint32_t x = 0;
	uint32_t y = 0;
	uint8_t buf[128] = {0};
	int32_t iArrayIndex = 0;
#if TOUCH_KEY_NUM > 0
	uint32_t raw_btn_pipe_addr = 0;
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	NVT_LOG("++\n");

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		return -EAGAIN;
	}

	nvt_enable_short_test();

	if (nvt_polling_hand_shake_status()) {
		return -EAGAIN;
	}

#if TOUCH_KEY_NUM > 0
    rawdata_buf = (uint8_t *)kzalloc((X_Channel * Y_Channel + IC_KEY_CFG_SIZE) * 2, GFP_KERNEL);
#else
    rawdata_buf = (uint8_t *)kzalloc(X_Channel * Y_Channel * 2, GFP_KERNEL);
#endif /* #if TOUCH_KEY_NUM > 0 */
	if (!rawdata_buf) {
		NVT_ERR("kzalloc for rawdata_buf failed!\n");
		return -ENOMEM;
	}

	if (nvt_get_fw_pipe() == 0)
		raw_pipe_addr = ts->mmap->RAW_PIPE0_ADDR;
	else
		raw_pipe_addr = ts->mmap->RAW_PIPE1_ADDR;

	for (y = 0; y < Y_Channel; y++) {
		//---change xdata index---
		buf[0] = 0xFF;
		buf[1] = (uint8_t)(((raw_pipe_addr + y * X_Channel * 2) >> 16) & 0xFF);
		buf[2] = (uint8_t)(((raw_pipe_addr + y * X_Channel * 2) >> 8) & 0xFF);
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);
		buf[0] = (uint8_t)((raw_pipe_addr + y * X_Channel * 2) & 0xFF);
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, X_Channel * 2 + 1);
		memcpy(rawdata_buf + y * X_Channel * 2, buf + 1, X_Channel * 2);
	}
#if TOUCH_KEY_NUM > 0
	if (nvt_get_fw_pipe() == 0)
		raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE0_ADDR;
	else
		raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE1_ADDR;

    //---change xdata index---
	buf[0] = 0xFF;
	buf[1] = (uint8_t)((raw_btn_pipe_addr >> 16) & 0xFF);
	buf[2] = (uint8_t)((raw_btn_pipe_addr >> 8) & 0xFF);
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);
	buf[0] = (uint8_t)(raw_btn_pipe_addr & 0xFF);
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, IC_KEY_CFG_SIZE * 2 + 1);
	memcpy(rawdata_buf + Y_Channel * X_Channel * 2, buf + 1, IC_KEY_CFG_SIZE * 2);
#endif /* #if TOUCH_KEY_NUM > 0 */

	for (y = 0; y < Y_Channel; y++) {
		for (x = 0; x < X_Channel; x++) {
			iArrayIndex = y * X_Channel + x;
			xdata[iArrayIndex] = (int16_t)(rawdata_buf[iArrayIndex * 2] + 256 * rawdata_buf[iArrayIndex * 2 + 1]);
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < IC_KEY_CFG_SIZE; k++) {
		if (AIN_KEY[k] != 0xFF) {
			iArrayIndex = Y_Channel * X_Channel + k;
			xdata[Y_Channel * X_Channel + AIN_KEY[k]] = (int16_t) (rawdata_buf[iArrayIndex * 2] + 256 * rawdata_buf[iArrayIndex * 2 + 1]);
		}
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	if (rawdata_buf) {
		kfree(rawdata_buf);
		rawdata_buf = NULL;
	}

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	printk("%s:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel, SHORT_TEST_CSV_FILE, 0) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

	NVT_LOG("--\n");

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen raw data test function.

return:
	Executive outcomes. 0---passed. negative---failed.
*******************************************************/
static int32_t RawDataTest_Sub(int32_t rawdata[], uint8_t RecordResult[], uint8_t x_ch, uint8_t y_ch, int32_t Rawdata_Limit_Postive, int32_t Rawdata_Limit_Negative,
				int32_t Rawdata_Limit_Key_Postive, int32_t Rawdata_Limit_Key_Negative)
{
	int32_t i = 0;
	int32_t j = 0;
#if TOUCH_KEY_NUM > 0
    int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */
	int32_t iArrayIndex = 0;
	bool isPass = true;

	for (j = 0; j < y_ch; j++) {
		for (i = 0; i < x_ch; i++) {
			iArrayIndex = j * x_ch + i;

			RecordResult[iArrayIndex] = 0x00; // default value for PASS

			if(rawdata[iArrayIndex] > Rawdata_Limit_Postive)
				RecordResult[iArrayIndex] |= 0x01;

			if(rawdata[iArrayIndex] < Rawdata_Limit_Negative)
				RecordResult[iArrayIndex] |= 0x02;
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = y_ch * x_ch + k;

		RecordResult[iArrayIndex] = 0x00; // default value for PASS

		if(rawdata[iArrayIndex] > Rawdata_Limit_Key_Postive)
			RecordResult[iArrayIndex] |= 0x01;

		if(rawdata[iArrayIndex] < Rawdata_Limit_Key_Negative)
			RecordResult[iArrayIndex] |= 0x02;
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	//---Check RecordResult---
	for (j = 0; j < y_ch; j++) {
		for (i = 0; i < x_ch; i++) {
			if (RecordResult[j * x_ch + i] != 0) {
				isPass = false;
				break;
			}
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = y_ch * x_ch + k;
		if (RecordResult[iArrayIndex] != 0) {
			isPass = false;
			break;
		}
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	if (isPass == false) {
		return -1; // FAIL
	} else {
		return 0; // PASS
	}
}

/*******************************************************
Description:
	Novatek touchscreen raw data test for each single point function.

return:
	Executive outcomes. 0---passed. negative---failed.
*******************************************************/
static int32_t RawDataTest_SinglePoint_Sub(int32_t rawdata[], uint8_t RecordResult[], uint8_t x_ch, uint8_t y_ch, int32_t Rawdata_Limit_Postive[], int32_t Rawdata_Limit_Negative[])
{
	int32_t i = 0;
	int32_t j = 0;
#if TOUCH_KEY_NUM > 0
    int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */
	int32_t iArrayIndex = 0;
	bool isPass = true;

	for (j = 0; j < y_ch; j++) {
		for (i = 0; i < x_ch; i++) {
			iArrayIndex = j * x_ch + i;

			RecordResult[iArrayIndex] = 0x00; // default value for PASS

			if(rawdata[iArrayIndex] > Rawdata_Limit_Postive[iArrayIndex])
				RecordResult[iArrayIndex] |= 0x01;

			if(rawdata[iArrayIndex] < Rawdata_Limit_Negative[iArrayIndex])
				RecordResult[iArrayIndex] |= 0x02;
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = y_ch * x_ch + k;

		RecordResult[iArrayIndex] = 0x00; // default value for PASS

		if(rawdata[iArrayIndex] > Rawdata_Limit_Postive[iArrayIndex])
			RecordResult[iArrayIndex] |= 0x01;

		if(rawdata[iArrayIndex] < Rawdata_Limit_Negative[iArrayIndex])
			RecordResult[iArrayIndex] |= 0x02;
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	//---Check RecordResult---
	for (j = 0; j < y_ch; j++) {
		for (i = 0; i < x_ch; i++) {
			if (RecordResult[j * x_ch + i] != 0) {
				isPass = false;
				break;
			}
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = y_ch * x_ch + k;
		if (RecordResult[iArrayIndex] != 0) {
			isPass = false;
			break;
		}
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	if (isPass == false) {
		return -1; // FAIL
	} else {
		return 0; // PASS
	}
}

/*******************************************************
Description:
	Novatek touchscreen print self-test result function.

return:
	n.a.
*******************************************************/
void print_selftest_result(struct seq_file *m, int32_t TestResult, uint8_t RecordResult[], int32_t rawdata[], uint8_t x_len, uint8_t y_len)
{
	int32_t i = 0;
	int32_t j = 0;
	int32_t iArrayIndex = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	switch (TestResult) {
		case 0:
			seq_printf(m, " PASS!");
			seq_puts(m, "\n");
			if (!nvt_mp_test_result_printed)
				printk(" PASS!\n");
			break;

		case 1:
			seq_printf(m, " ERROR! Read Data FAIL!");
			seq_puts(m, "\n");
			if (!nvt_mp_test_result_printed)
				printk(" ERROR! Read Data FAIL!\n");
			goto end;
			break;

		case -1:
			seq_printf(m, " FAIL!");
			seq_puts(m, "\n");
			if (!nvt_mp_test_result_printed)
				printk(" FAIL!\n");
			break;
	}

	/* Print Test Result */
	seq_printf(m, "RecordResult:");
	seq_puts(m, "\n");
	if (!nvt_mp_test_result_printed)
		printk("RecordResult:\n");
	for (i = 0; i < y_len; i++) {
		for (j = 0; j < x_len; j++) {
			iArrayIndex = i * x_len + j;
			seq_printf(m, "0x%02X, ", RecordResult[iArrayIndex]);
			if (!nvt_mp_test_result_printed)
				printk("0x%02X, ", RecordResult[iArrayIndex]);
		}
		seq_puts(m, "\n");
		if (!nvt_mp_test_result_printed)
			printk("\n");
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = y_len * x_len + k;
		seq_printf(m, "0x%02X, ", RecordResult[iArrayIndex]);
		if (!nvt_mp_test_result_printed)
			printk("0x%02X, ", RecordResult[iArrayIndex]);
	}
	seq_puts(m, "\n");
	if (!nvt_mp_test_result_printed)
		printk("\n");
#endif /* #if TOUCH_KEY_NUM > 0 */
	seq_printf(m, "ReadData:");
	seq_puts(m, "\n");
	if (!nvt_mp_test_result_printed)
		printk("ReadData:\n");
	for (i = 0; i < y_len; i++) {
		for (j = 0; j < x_len; j++) {
			iArrayIndex = i * x_len + j;
			seq_printf(m, "%5d, ", rawdata[iArrayIndex]);
			if (!nvt_mp_test_result_printed)
				printk("%5d, ", rawdata[iArrayIndex]);
		}
		seq_puts(m, "\n");
		if (!nvt_mp_test_result_printed)
			printk("\n");
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = y_len * x_len + k;
		seq_printf(m, "%5d, ", rawdata[iArrayIndex]);
		if (!nvt_mp_test_result_printed)
			printk("%5d, ", rawdata[iArrayIndex]);
	}
	seq_puts(m, "\n");
	if (!nvt_mp_test_result_printed)
		printk("\n");
#endif /* #if TOUCH_KEY_NUM > 0 */
end:
	seq_printf(m, "\n");
	if (!nvt_mp_test_result_printed)
		printk("\n");
}

/*******************************************************
Description:
	Novatek touchscreen self-test sequence print show
	function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_show_selftest(struct seq_file *m, void *v)
{
	NVT_LOG("++\n");

	seq_printf(m, "Short Test");
	if (!nvt_mp_test_result_printed)
		printk("Short Test");
	print_selftest_result(m, TestResult_Short, RecordResult_Short, RawData_Short, X_Channel, Y_Channel);

	seq_printf(m, "Open Test");
	if (!nvt_mp_test_result_printed)
		printk("Open Test");
	print_selftest_result(m, TestResult_Open, RecordResult_Open, RawData_Open, X_Channel, Y_Channel);

#if NVT_TOUCH_MP_FW
	seq_printf(m, "FW Rawdata Test");
	if (!nvt_mp_test_result_printed)
		printk("FW Rawdata Test");
	if ((TestResult_FW_Rawdata == 0) || (TestResult_FW_Rawdata == 1)) {
		 print_selftest_result(m, TestResult_FWMutual, RecordResult_FWMutual, RawData_FWMutual, X_Channel, Y_Channel);
	} else { // TestResult_FW_Rawdata is -1
		seq_printf(m, " FAIL!");
		seq_puts(m, "\n");
		if (!nvt_mp_test_result_printed)
			printk(" FAIL!\n");
		if (TestResult_FWMutual == -1) {
			seq_printf(m, "FW Mutual");
			if (!nvt_mp_test_result_printed)
				printk("FW Mutual");
			print_selftest_result(m, TestResult_FWMutual, RecordResult_FWMutual, RawData_FWMutual, X_Channel, Y_Channel);
		}
		if (TestResult_FW_CC == -1) {
			seq_printf(m, "FW CC");
			if (!nvt_mp_test_result_printed)
				printk("FW CC");
			print_selftest_result(m, TestResult_FW_CC, RecordResult_FW_CC, RawData_FW_CC, X_Channel, Y_Channel);
		}
	}

	seq_printf(m, "Noise Test");
	if (!nvt_mp_test_result_printed)
		printk("Noise Test");
	if ((TestResult_Noise == 0) || (TestResult_Noise == 1)) {
		print_selftest_result(m, TestResult_FW_DiffMax, RecordResult_FW_DiffMax, RawData_Diff_Max, X_Channel, Y_Channel);
	} else { // TestResult_Noise is -1
		seq_printf(m, " FAIL!");
		seq_puts(m, "\n");
		if (!nvt_mp_test_result_printed)
			printk(" FAIL!\n");
		if (TestResult_FW_DiffMax == -1) {
			seq_printf(m, "FW Diff Max");
			if (!nvt_mp_test_result_printed)
				printk("FW Diff Max");
			print_selftest_result(m, TestResult_FW_DiffMax, RecordResult_FW_DiffMax, RawData_Diff_Max, X_Channel, Y_Channel);
		}
		if (TestResult_FW_DiffMin == -1) {
			seq_printf(m, "FW Diff Min");
			if (!nvt_mp_test_result_printed)
				printk("FW Diff Min");
			print_selftest_result(m, TestResult_FW_DiffMin, RecordResult_FW_DiffMin, RawData_Diff_Min, X_Channel, Y_Channel);
		}
	}
#endif /* #if NVT_TOUCH_MP_FW */

	nvt_mp_test_result_printed = 1;

	NVT_LOG("--\n");
    return 0;
}

/*******************************************************
Description:
	Novatek touchscreen self-test sequence print start
	function.

return:
	Executive outcomes. 1---call next function.
	NULL---not call next function and sequence loop
	stop.
*******************************************************/
static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

/*******************************************************
Description:
	Novatek touchscreen self-test sequence print next
	function.

return:
	Executive outcomes. NULL---no next and call sequence
	stop function.
*******************************************************/
static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

/*******************************************************
Description:
	Novatek touchscreen self-test sequence print stop
	function.

return:
	n.a.
*******************************************************/
static void c_stop(struct seq_file *m, void *v)
{
	return;
}

const struct seq_operations nvt_selftest_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_show_selftest
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_selftest open function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int32_t nvt_selftest_open(struct inode *inode, struct file *file)
{
	nvt_mp_isInitialed = 0;
	TestResult_Short = 0;
	TestResult_Open = 0;
#if NVT_TOUCH_MP_FW
	TestResult_FW_Rawdata = 0;
	TestResult_FWMutual = 0;
	TestResult_FW_CC = 0;
	TestResult_Noise = 0;
	TestResult_FW_DiffMax = 0;
	TestResult_FW_DiffMin = 0;
#endif /* #if NVT_TOUCH_MP_FW */

	NVT_LOG("++\n");

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	//---Print Test Criteria---
	nvt_print_criteria();

	if (nvt_switch_FreqHopEnDis(FREQ_HOP_DISABLE)) {
		mutex_unlock(&ts->lock);
		NVT_ERR("switch frequency hopping disable failed!\n");
		return -EAGAIN;
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		mutex_unlock(&ts->lock);
		NVT_ERR("check fw reset state failed!\n");
		return -EAGAIN;
	}

	msleep(100);

#if NVT_TOUCH_MP_FW
	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		NVT_ERR("clear fw status failed!\n");
		return -EAGAIN;
	}

	nvt_change_mode(MP_MODE_CC);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		NVT_ERR("check fw status failed!\n");
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		NVT_ERR("get fw info failed!\n");
		return -EAGAIN;
	}

	//---FW Rawdata Test---
	if (nvt_read_baseline(RawData_FWMutual) != 0) {
		TestResult_FWMutual = 1;
	} else {
		TestResult_FWMutual = RawDataTest_SinglePoint_Sub(RawData_FWMutual, RecordResult_FWMutual, X_Channel, Y_Channel,
												PS_Config_Lmt_FW_Rawdata_P, PS_Config_Lmt_FW_Rawdata_N);
	}
	if (nvt_read_CC(RawData_FW_CC) != 0) {
		TestResult_FW_CC = 1;
	} else {
		TestResult_FW_CC = RawDataTest_Sub(RawData_FW_CC, RecordResult_FW_CC, X_Channel, Y_Channel,
											PS_Config_Lmt_FW_CC_P, PS_Config_Lmt_FW_CC_N,
											PS_Config_Lmt_Key_FW_CC_P, PS_Config_Lmt_Key_FW_CC_N);
	}

	if ((TestResult_FWMutual == 1) || (TestResult_FW_CC == 1)) {
		TestResult_FW_Rawdata = 1;
	} else {
		if ((TestResult_FWMutual == -1) || (TestResult_FW_CC == -1))
			TestResult_FW_Rawdata = -1;
		else
			TestResult_FW_Rawdata = 0;
	}

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	//---Noise Test---
	if (nvt_read_fw_noise(RawData_Diff) != 0) {
		TestResult_Noise = 1;	// 1: ERROR
		TestResult_FW_DiffMax = 1;
		TestResult_FW_DiffMin = 1;
	} else {
		TestResult_FW_DiffMax = RawDataTest_Sub(RawData_Diff_Max, RecordResult_FW_DiffMax, X_Channel, Y_Channel,
											PS_Config_Lmt_FW_Diff_P, PS_Config_Lmt_FW_Diff_N,
											PS_Config_Lmt_Key_FW_Diff_P, PS_Config_Lmt_Key_FW_Diff_N);

		TestResult_FW_DiffMin = RawDataTest_Sub(RawData_Diff_Min, RecordResult_FW_DiffMin, X_Channel, Y_Channel,
											PS_Config_Lmt_FW_Diff_P, PS_Config_Lmt_FW_Diff_N,
											PS_Config_Lmt_Key_FW_Diff_P, PS_Config_Lmt_Key_FW_Diff_N);

		if ((TestResult_FW_DiffMax == -1) || (TestResult_FW_DiffMin == -1))
			TestResult_Noise = -1;
		else
			TestResult_Noise = 0;
	}
#endif /* #if NVT_TOUCH_MP_FW */

	//--Short Test---
	if (nvt_read_fw_short(RawData_Short) != 0) {
		TestResult_Short = 1; // 1:ERROR
	} else {
		//---Self Test Check --- // 0:PASS, -1:FAIL
		TestResult_Short = RawDataTest_Sub(RawData_Short, RecordResult_Short, X_Channel, Y_Channel,
											PS_Config_Lmt_Short_Rawdata_P, PS_Config_Lmt_Short_Rawdata_N,
											PS_Config_Lmt_Key_Short_Rawdata_P, PS_Config_Lmt_Key_Short_Rawdata_N);
	}

	//---Open Test---
	if (nvt_read_fw_open(RawData_Open) != 0) {
		TestResult_Open = 1;    // 1:ERROR
	} else {
		//---Self Test Check --- // 0:PASS, -1:FAIL
		TestResult_Open = RawDataTest_SinglePoint_Sub(RawData_Open, RecordResult_Open, X_Channel, Y_Channel,
											PS_Config_Lmt_Open_Rawdata_P, PS_Config_Lmt_Open_Rawdata_N);
	}

	//---Reset IC---
	nvt_bootloader_reset();

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");
	nvt_mp_test_result_printed = 0;
	return seq_open(file, &nvt_selftest_seq_ops);
}

static const struct file_operations nvt_selftest_fops = {
	.owner = THIS_MODULE,
	.open = nvt_selftest_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*******************************************************
Description:
	Novatek touchscreen MP function proc. file node
	initial function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_mp_proc_init(void)
{
	NVT_proc_selftest_entry = proc_create("nvt_selftest", 0444, NULL, &nvt_selftest_fops);
	if (NVT_proc_selftest_entry == NULL) {
		NVT_ERR("create /proc/nvt_selftest Failed!\n");
		return -1;
	} else {
		NVT_LOG("create /proc/nvt_selftest Succeeded!\n");
		return 0;
	}
}
#endif /* #if NVT_TOUCH_MP */
