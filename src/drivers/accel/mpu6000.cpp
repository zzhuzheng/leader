/*******************************Copyright (c)***************************
**
** Porject name:	LeaderUAV-Plus
** Created by:		zhuzheng<happyzhull@163.com>
** Created date:	2015/08/28
** Modified by:
** Modified date:
** Descriptions:
**
***********************************************************************/
#include "mpu6000.h"

namespace driver {

/*
  list of registers that will be checked in check_registers(). Note
  that MPUREG_PRODUCT_ID must be first in the list.
 */
const u8 mpu6000::_checked_registers[MPU6000_NUM_CHECKED_REGISTERS] = {
    MPUREG_PRODUCT_ID,
    MPUREG_PWR_MGMT_1,
    MPUREG_USER_CTRL,
    MPUREG_SMPLRT_DIV,
    MPUREG_CONFIG,
    MPUREG_GYRO_CONFIG,
    MPUREG_ACCEL_CONFIG,
    MPUREG_INT_ENABLE,
    MPUREG_INT_PIN_CFG
};


mpu6000::mpu6000(PCSTR devname, s32 devid) :
    device(devname, devid),
    _accel_reports(NULL),
    _accel_range_scale(0.0f),
    _accel_range_m_s2(0.0f),
    _gyro_reports(NULL),
    _gyro_range_scale(0.0f),
    _gyro_range_rad_s(0.0f),

	_dlpf_freq(MPU6000_DEFAULT_ONCHIP_FILTER_FREQ),
	_sample_rate(1000)

{
    // default accel scale factors
	_accel_scale.x_offset = 0;
	_accel_scale.x_scale  = 1.0f;
	_accel_scale.y_offset = 0;
	_accel_scale.y_scale  = 1.0f;
	_accel_scale.z_offset = 0;
	_accel_scale.z_scale  = 1.0f;

	// default gyro scale factors
	_gyro_scale.x_offset = 0;
	_gyro_scale.x_scale  = 1.0f;
	_gyro_scale.y_offset = 0;
	_gyro_scale.y_scale  = 1.0f;
	_gyro_scale.z_offset = 0;
	_gyro_scale.z_scale  = 1.0f;

	_params.name = "mpu6000_thread";
	_params.priority = 0;
	_params.stacksize = 2048;
	_params.func = (void *)thread::func;
	_params.parg = (thread *)this;

}

mpu6000::~mpu6000(void)
{

}


s32 mpu6000::probe(spi *pspi, gpio *gpio_cs)
{
	ASSERT((pspi != NULL) && (gpio_cs != NULL));

    _gpio_cs = gpio_cs;
    _spi = pspi;

	_gpio_cs->set_direction_output();
	_gpio_cs->set_value(true);

	s32 ret = 0;
	ret = init();
	if (ret) {
		ERR("%s: failed to init.\n", _devname);
        goto fail0;
	}

    return 0;

fail0:
    return -1;
}

s32 mpu6000::open(s32 flags)
{
	return 0;
}

s32 mpu6000::close(void)
{
    return 0;
}

s32 mpu6000::read_accel(u8 *buf, u32 size)
{
	u32 count = size / sizeof(accel_report);

	/* buffer must be large enough */
	if (count < 1)
		return -ENOSPC;

	/* if automatic measurement is not enabled, get a fresh measurement into the buffer */
	//if (_call_interval == 0) {
	//	_accel_reports->flush();
	//	measure();
	//}

	/* if no data, error (we could block here) */
	if (_accel_reports->empty())
		return -EAGAIN;

	//perf_count(_accel_reads);

	/* copy reports out of our buffer to the caller */
	accel_report *arp = reinterpret_cast<accel_report *>(buf);
	int transferred = 0;
	while (count--) {
		if (!_accel_reports->get(arp))
			break;
		transferred++;
		arp++;
	}

	/* return the number of bytes transferred */
	return (transferred * sizeof(accel_report));
}

s32 mpu6000::read_gyro(u8 *buf, u32 size)
{
	unsigned count = size / sizeof(gyro_report);

	/* buffer must be large enough */
	if (count < 1)
		return -ENOSPC;

	/* if automatic measurement is not enabled, get a fresh measurement into the buffer */
	//if (_call_interval == 0) {
	//	_gyro_reports->flush();
	//	measure();
	//}

	/* if no data, error (we could block here) */
	if (_gyro_reports->empty())
		return -EAGAIN;

	//perf_count(_gyro_reads);

	/* copy reports out of our buffer to the caller */
	gyro_report *grp = reinterpret_cast<gyro_report *>(buf);
	int transferred = 0;
	while (count--) {
		if (!_gyro_reports->get(grp))
			break;
		transferred++;
		grp++;
	}

	/* return the number of bytes transferred */
	return (transferred * sizeof(gyro_report));
}

void mpu6000::run(void *parg)
{
	for (;;)
	{
        mpu6000::measure();
        msleep(2);
	}
}

s32 mpu6000::init(void)
{
#if 0
	//等待上电稳定
	core::mdelay(10000);
    write_reg8(MPUREG_PWR_MGMT1, 0X80); 	// 复位MPU6050
    core::mdelay(1000);
    write_reg8(MPUREG_PWR_MGMT1, 0X00);  	// 唤醒MPU6050

    set_gyro_fsr(2000);                         //陀螺仪传感器,±2000dps
    set_accel_fsr(2);                           //加速度传感器,±2g
    set_sample_rate(50);                        //设置采样率50Hz
    write_reg8(MPUREG_INT_ENABLE, 0X00);    //关闭所有中断
    write_reg8(MPUREG_USER_CTRL, 0X00);     //I2C主模式关闭
    write_reg8(MPUREG_FIFO_ENABLE, 0X00);   //关闭FIFO
    write_reg8(MPUREG_INT_PIN_CFG, 0X80);   //INT引脚低电平有效
    u8 who_am_i = read_reg8(MPUREG_DEVICE_ID);
    core::mdelay(3000);
    if(who_am_i != MPU6050_ID) {
        // 器件ID不正确
        ERR("failed to read mpu6000 ID...\n");
        return -1;
    }
    write_reg8(MPUREG_PWR_MGMT1, 0X01);     //设置CLKSEL,PLL X轴为参考
    write_reg8(MPUREG_PWR_MGMT2, 0X00);     // 加速度与陀螺仪都工作
    set_sample_rate(200);                 	//设置采样率200Hz
#endif

	/* allocate basic report buffers */
	_accel_reports = new ringbuffer(2, sizeof(accel_report));
	if (_accel_reports == NULL) {
		goto out;
	}

	_gyro_reports = new ringbuffer(2, sizeof(gyro_report));
	if (_gyro_reports == NULL) {
		goto out;
	}

	if (reset() != 0) {
		ERR("%s: failed to reset.\n", _devname);
		goto out;
	}

	/* initialize offsets and scales */
	_accel_scale.x_offset = 0;
	_accel_scale.x_scale  = 1.0f;
	_accel_scale.y_offset = 0;
	_accel_scale.y_scale  = 1.0f;
	_accel_scale.z_offset = 0;
	_accel_scale.z_scale  = 1.0f;

	_gyro_scale.x_offset = 0;
	_gyro_scale.x_scale  = 1.0f;
	_gyro_scale.y_offset = 0;
	_gyro_scale.y_scale  = 1.0f;
	_gyro_scale.z_offset = 0;
	_gyro_scale.z_scale  = 1.0f;

	/* discard any stale data in the buffers */
	_accel_reports->flush();
	_gyro_reports->flush();

	measure();

	return 0;

out:
	return -1;
}


s32 mpu6000::reset(void)
{
	// if the mpu6000 is initialised after the l3gd20 and lsm303d
	// then if we don't do an irqsave/irqrestore here the mpu6000
	// frequenctly comes up in a bad state where all transfers
	// come as zero
	u8 tries = 5;
	while (--tries != 0) {
        taskDISABLE_INTERRUPTS();
		write_reg8(MPUREG_PWR_MGMT_1, BIT_H_RESET);
		core::mdelay(10);

		// Wake up device and select GyroZ clock. Note that the
		// MPU6000 starts up in sleep mode, and it can take some time
		// for it to come out of sleep
		write_checked_reg(MPUREG_PWR_MGMT_1, MPU_CLK_SEL_PLLGYROZ);
		core::mdelay(1);

		// Disable I2C bus (recommended on datasheet)
		write_checked_reg(MPUREG_USER_CTRL, BIT_I2C_IF_DIS);
		taskENABLE_INTERRUPTS();

		if (read_reg8(MPUREG_PWR_MGMT_1) == MPU_CLK_SEL_PLLGYROZ) {
			break;
		}
		//perf_count(_reset_retries);
		core::mdelay(2);
	}
	if (read_reg8(MPUREG_PWR_MGMT_1) != MPU_CLK_SEL_PLLGYROZ) {
        ERR("%s: failed to set mpu clk pll.\n", _devname);
		return -EIO;
	}

    /* look for a product ID we recognise */
    _product = read_reg8(MPUREG_PRODUCT_ID);
    // verify product revision
	if (MPU6000ES_REV_C4 == _product ||
		MPU6000ES_REV_C5 == _product ||
		MPU6000_REV_C4 == _product ||
		MPU6000_REV_C5 == _product ||
		MPU6000ES_REV_D6 == _product ||
		MPU6000ES_REV_D7 == _product ||
		MPU6000ES_REV_D8 == _product ||
		MPU6000_REV_D6 == _product ||
		MPU6000_REV_D7 == _product ||
		MPU6000_REV_D8 == _product ||
		MPU6000_REV_D9 == _product ||
		MPU6000_REV_D10 == _product) {
        DBG("%s: ID 0x%02x", _devname, _product);
        _checked_values[0] = _product;
	} else {
		DBG("%s: unexpected ID 0x%02x", _devname, _product);
        return -EIO;
	}
    
	core::mdelay(1);

	// SAMPLE RATE
	set_sample_rate(_sample_rate);
	core::mdelay(1);

	// FS & DLPF   FS=2000 deg/s, DLPF = 20Hz (low pass filter)
	// was 90 Hz, but this ruins quality and does not improve the
	// system response
	set_dlpf_filter(MPU6000_DEFAULT_ONCHIP_FILTER_FREQ);
	core::mdelay(1);
	// Gyro scale 2000 deg/s ()
	write_checked_reg(MPUREG_GYRO_CONFIG, BITS_FS_2000DPS);
	core::mdelay(1);

	// correct gyro scale factors
	// scale to rad/s in SI units
	// 2000 deg/s = (2000/180)*PI = 34.906585 rad/s
	// scaling factor:
	// 1/(2^15)*(2000/180)*PI
	_gyro_range_scale = (0.0174532 / 16.4);//1.0f / (32768.0f * (2000.0f / 180.0f) * M_PI_F);
	_gyro_range_rad_s = (2000.0f / 180.0f) * M_PI_F;

	set_accel_range(8);

	core::mdelay(1);

	// INT CFG => Interrupt on Data Ready
	//write_checked_reg(MPUREG_INT_ENABLE, BIT_RAW_RDY_EN);        // INT: Raw data ready
	//core::mdelay(1);
	//write_checked_reg(MPUREG_INT_PIN_CFG, BIT_INT_ANYRD_2CLEAR); // INT: Clear on any read
	//core::mdelay(1);

	// Oscillator set
	// write_reg(MPUREG_PWR_MGMT_1,MPU_CLK_SEL_PLLGYROZ);
	core::mdelay(1);

    return 0;
}

void mpu6000::measure(void)
{
    s32 ret = 0;
#pragma pack(push, 1)
	struct mpu_report_reg { /* Report conversation within the MPU6000 */
		u8		status;
		u8		accel_x[2];
		u8		accel_y[2];
		u8		accel_z[2];
		u8		temp[2];
		u8		gyro_x[2];
		u8		gyro_y[2];
		u8		gyro_z[2];
	} report_reg;
#pragma pack(pop)
	struct mpu_report {
        u8		status;
		s16		accel_x;
		s16		accel_y;
		s16		accel_z;
		s16		temp;
		s16		gyro_x;
		s16		gyro_y;
		s16		gyro_z;
	} report;

	/*
	 * Fetch the full set of measurements from the MPU6000 in one pass.
	 */
    //SPI_PEND(_spi);
    taskENTER_CRITICAL();
	ret = mpu6000::read_reg(MPUREG_INT_STATUS, (u8 *)&report_reg, sizeof(report_reg));
    taskEXIT_CRITICAL();
    //SPI_POST(_spi);
    if (ret < 0) {
        return;
    }

	/*
	 * Convert from big to little endian
	 */
	report.accel_x = s16_from_bytes(report_reg.accel_x);
	report.accel_y = s16_from_bytes(report_reg.accel_y);
	report.accel_z = s16_from_bytes(report_reg.accel_z);

	report.temp = s16_from_bytes(report_reg.temp);

	report.gyro_x = s16_from_bytes(report_reg.gyro_x);
	report.gyro_y = s16_from_bytes(report_reg.gyro_y);
	report.gyro_z = s16_from_bytes(report_reg.gyro_z);

	if (report.accel_x == 0 &&
	    report.accel_y == 0 &&
	    report.accel_z == 0 &&
	    report.temp == 0 &&
	    report.gyro_x == 0 &&
	    report.gyro_y == 0 &&
	    report.gyro_z == 0) {
		// all zero data - probably a SPI bus error
                // note that we don't call reset() here as a reset()
                // costs 20ms with interrupts disabled. That means if
                // the mpu6k does go bad it would cause a FMU failure,
                // regardless of whether another sensor is available,
		return;
	}


	/*
	 * Swap axes and negate y
	 */
	s16 accel_xt = report.accel_y;
	s16 accel_yt = ((report.accel_x == -32768) ? 32767 : -report.accel_x);

	s16 gyro_xt = report.gyro_y;
	s16 gyro_yt = ((report.gyro_x == -32768) ? 32767 : -report.gyro_x);

	/*
	 * Apply the swap
	 */
	report.accel_x = accel_xt;
	report.accel_y = accel_yt;
	report.gyro_x = gyro_xt;
	report.gyro_y = gyro_yt;

	/*
	 * Report buffers.
	 */
	accel_report	arb;
	gyro_report	    grb;

	/*
	 * Adjust and scale results to m/s^2.
	 */
	grb.timestamp = arb.timestamp = 0;//hrt_absolute_time();

	// report the error count as the sum of the number of bad
	// transfers and bad register reads. This allows the higher
	// level code to decide if it should use this sensor based on
	// whether it has had failures
    grb.error_count = arb.error_count = 0;//= perf_event_count(_bad_transfers) + perf_event_count(_bad_registers);

	/*
	 * 1) Scale raw value to SI units using scaling from datasheet.
	 * 2) Subtract static offset (in SI units)
	 * 3) Scale the statically calibrated values with a linear
	 *    dynamically obtained factor
	 *
	 * Note: the static sensor offset is the number the sensor outputs
	 * 	 at a nominally 'zero' input. Therefore the offset has to
	 * 	 be subtracted.
	 *
	 *	 Example: A gyro outputs a value of 74 at zero angular rate
	 *	 	  the offset is 74 from the origin and subtracting
	 *		  74 from all measurements centers them around zero.
	 */


	/* NOTE: Axes have been swapped to match the board a few lines above. */

	arb.x_raw = report.accel_x;
	arb.y_raw = report.accel_y;
	arb.z_raw = report.accel_z;

	f32 x_in_new = ((report.accel_x * _accel_range_scale) - _accel_scale.x_offset) * _accel_scale.x_scale;
	f32 y_in_new = ((report.accel_y * _accel_range_scale) - _accel_scale.y_offset) * _accel_scale.y_scale;
	f32 z_in_new = ((report.accel_z * _accel_range_scale) - _accel_scale.z_offset) * _accel_scale.z_scale;


	arb.x = 0;//_accel_filter_x.apply(x_in_new);
	arb.y = 0;//_accel_filter_y.apply(y_in_new);
	arb.z = 0;//_accel_filter_z.apply(z_in_new);

	// apply user specified rotation
	//rotate_3f(_rotation, arb.x, arb.y, arb.z);

	arb.scaling = _accel_range_scale;
	arb.range_m_s2 = _accel_range_m_s2;

	_last_temperature = (report.temp) / 361.0f + 35.0f;

	arb.temperature_raw = report.temp;
	arb.temperature = _last_temperature;

	grb.x_raw = report.gyro_x;
	grb.y_raw = report.gyro_y;
	grb.z_raw = report.gyro_z;

	float x_gyro_in_new = ((report.gyro_x * _gyro_range_scale) - _gyro_scale.x_offset) * _gyro_scale.x_scale;
	float y_gyro_in_new = ((report.gyro_y * _gyro_range_scale) - _gyro_scale.y_offset) * _gyro_scale.y_scale;
	float z_gyro_in_new = ((report.gyro_z * _gyro_range_scale) - _gyro_scale.z_offset) * _gyro_scale.z_scale;

	grb.x = 0;//_gyro_filter_x.apply(x_gyro_in_new);
	grb.y = 0;//_gyro_filter_y.apply(y_gyro_in_new);
	grb.z = 0;//_gyro_filter_z.apply(z_gyro_in_new);

	// apply user specified rotation
	//rotate_3f(_rotation, grb.x, grb.y, grb.z);

	grb.scaling = _gyro_range_scale;
	grb.range_rad_s = _gyro_range_rad_s;

	grb.temperature_raw = report.temp;
	grb.temperature = _last_temperature;

	_accel_reports->force(&arb);
	_gyro_reports->force(&grb);

    return;
}

s16 mpu6000::s16_from_bytes(u8 bytes[])
{
	union {
		u8    b[2];
		s16    w;
	} u;

	u.b[1] = bytes[0];
	u.b[0] = bytes[1];

	return u.w;
}

/*
 * 设置MPU6000陀螺仪传感器满量程范围
 * fsr:0,±250dps;1,±500dps;2,±1000dps;3,±2000dps
 * return:成功返回0
 */
s32 mpu6000::set_gyro_fsr(u16 fsr)
{
    s32 ret = 0;
    u8 data;

    switch (fsr) {
    case 250:
        data = (u8)(GYRO_FSR_250DPS << 3);
        break;
    case 500:
        data = (u8)(GYRO_FSR_500DPS << 3);
        break;
    case 1000:
        data = (u8)(GYRO_FSR_1000DPS << 3);
        break;
    case 2000:
        data = (u8)(GYRO_FSR_2000DPS << 3);
        break;
    default:
        return -1;
    }

    ret = write_reg8(MPUREG_GYRO_CONFIG, data);
    if (ret) {
        return -1;
    }

    return 0;
}

/*
 * 设置MPU6000加速度传感器满量程范围
 * fsr:0,±2g;1,±4g;2,±8g;3,±16g
 * return:成功返回0
 */
s32 mpu6000::set_accel_fsr(u8 fsr)
{
    s32 ret = 0;
    u8 data;

    switch (fsr) {
    case 2:
        data = (u8)(ACCEL_FSR_2G << 3);
        break;
    case 4:
        data = (u8)(ACCEL_FSR_4G << 3);
        break;
    case 8:
        data = (u8)(ACCEL_FSR_8G << 3);
        break;
    case 16:
        data = (u8)(ACCEL_FSR_16G << 3);
        break;
    default:
        return -1;
    }

    ret = write_reg8(MPUREG_ACCEL_CONFIG, data);
    if (ret) {
        return -1;
    }

    return 0;
}




/*
 * set the DLPF filter frequency. This affects both accel and gyro.
 */
void mpu6000::set_dlpf_filter(u16 frequency_hz)
{
	u8 filter;

	/*
	   choose next highest filter frequency available
	 */
	if (frequency_hz == 0) {
		_dlpf_freq = 0;
		filter = BITS_DLPF_CFG_2100HZ_NOLPF;
	} else if (frequency_hz <= 5) {
		_dlpf_freq = 5;
		filter = BITS_DLPF_CFG_5HZ;
	} else if (frequency_hz <= 10) {
		_dlpf_freq = 10;
		filter = BITS_DLPF_CFG_10HZ;
	} else if (frequency_hz <= 20) {
		_dlpf_freq = 20;
		filter = BITS_DLPF_CFG_20HZ;
	} else if (frequency_hz <= 42) {
		_dlpf_freq = 42;
		filter = BITS_DLPF_CFG_42HZ;
	} else if (frequency_hz <= 98) {
		_dlpf_freq = 98;
		filter = BITS_DLPF_CFG_98HZ;
	} else if (frequency_hz <= 188) {
		_dlpf_freq = 188;
		filter = BITS_DLPF_CFG_188HZ;
	} else if (frequency_hz <= 256) {
		_dlpf_freq = 256;
		filter = BITS_DLPF_CFG_256HZ_NOLPF2;
	} else {
		_dlpf_freq = 0;
		filter = BITS_DLPF_CFG_2100HZ_NOLPF;
	}
	write_checked_reg(MPUREG_CONFIG, filter);
}


/*
  set sample rate (approximate) - 1kHz to 5Hz, for both accel and gyro
*/
void mpu6000::set_sample_rate(u32 desired_sample_rate_hz)
{
	if (desired_sample_rate_hz == 0 ||
        desired_sample_rate_hz > MPU6000_GYRO_DEFAULT_RATE) {
		desired_sample_rate_hz = MPU6000_GYRO_DEFAULT_RATE;
	}

	u8 div = 1000 / desired_sample_rate_hz;
	if(div > 200)	 div = 200;
	if(div < 1)	 div = 1;
	write_checked_reg(MPUREG_SMPLRT_DIV, div - 1);
	_sample_rate = 1000 / div;
}

s32 mpu6000::set_accel_range(u32 max_g_in)
{
	// workaround for bugged versions of MPU6k (rev C)
	switch (_product) {
		case MPU6000ES_REV_C4:
		case MPU6000ES_REV_C5:
		case MPU6000_REV_C4:
		case MPU6000_REV_C5:
			write_checked_reg(MPUREG_ACCEL_CONFIG, 1 << 3);
			_accel_range_scale = (MPU6000_ONE_G / 4096.0f);
			_accel_range_m_s2 = 8.0f * MPU6000_ONE_G;
			return 0;
	}

	u8 afs_sel;
	float lsb_per_g;
	float max_accel_g;

	if (max_g_in > 8) { // 16g - AFS_SEL = 3
		afs_sel = 3;
		lsb_per_g = 2048;
		max_accel_g = 16;
	} else if (max_g_in > 4) { //  8g - AFS_SEL = 2
		afs_sel = 2;
		lsb_per_g = 4096;
		max_accel_g = 8;
	} else if (max_g_in > 2) { //  4g - AFS_SEL = 1
		afs_sel = 1;
		lsb_per_g = 8192;
		max_accel_g = 4;
	} else {                //  2g - AFS_SEL = 0
		afs_sel = 0;
		lsb_per_g = 16384;
		max_accel_g = 2;
	}

	write_checked_reg(MPUREG_ACCEL_CONFIG, afs_sel << 3);
	_accel_range_scale = (MPU6000_ONE_G / lsb_per_g);
	_accel_range_m_s2 = max_accel_g * MPU6000_ONE_G;

	return 0;
}

/**
 *  @brief      Read raw gyro data directly from the registers.
 *  @param[out] gyro        Raw data in hardware units.
 *  @return     0 if successful.
 *  @note		gyro_x gyro_y gyro_z = [-32768, 32767]
 */
s32 mpu6000::get_gyro_raw(s16 *gyro)
{
    u8 tmp[6];
	if (mpu6000::read_reg(MPUREG_RAW_GYRO, tmp, 6))
        return -1;
    gyro[0] = (tmp[0] << 8) | tmp[1];
    gyro[1] = (tmp[2] << 8) | tmp[3];
    gyro[2] = (tmp[4] << 8) | tmp[5];

    return 0;
}

/**
 *  @brief      Read raw accel data directly from the registers.
 *  @param[out] data        Raw data in hardware units.
 *  @return     0 if successful.
 *  @note		accel_x accel_y accel_z = [-32768, 32767]
 */
s32 mpu6000::get_accel_raw(s16 *accel)
{
    u8 tmp[6];
	if (mpu6000::read_reg(MPUREG_RAW_ACCEL, tmp, 6))
        return -1;

    accel[0] = (tmp[0] << 8) | tmp[1];
    accel[1] = (tmp[2] << 8) | tmp[3];
    accel[2] = (tmp[4] << 8) | tmp[5];

    return 0;
}

/**
 *  @brief      Read temperature data directly from the registers.
 *  @param[out] temperature	Data in q16 format.
 *  @return     0 if successful.
 */
s32 mpu6000::get_temperature(f32 *temperature)
{
    u8 tmp[2];
    s16 raw;
	if (mpu6000::read_reg(MPUREG_TEMPERATURE, tmp, 2))
        return -1;
    raw = (tmp[0]<<8) | tmp[1];
    //data[0] = (long)((35 + ((raw - (float)st.hw->temp_offset) / st.hw->temp_sens)) * 65536L);
	temperature[0] = 36.53 + ((double)raw)/340;
	return 0;
}

s32 mpu6000::calibrate_gyro(void)
{
	struct gyro_report gyro_report;
	u32 counter = 0;
	const u32 total_count = 5000;
	u32 err_count = 0;	/* determine gyro mean values */
	u32 good_count = 0;
	s32 size;
	s32 ret = 1;

	struct gyro_scale gyro_scale = { 
		0.0f,
		1.0f,
		0.0f,
		1.0f,
		0.0f,
		1.0f,
	};

	/* reset all offsets to zero and all scales to one */
	memcpy(&_gyro_scale, &gyro_scale, sizeof(gyro_scale));

	/* read the sensor up to 50x, stopping when we have 10 good values */
	for (counter = 0; counter < total_count; counter++) {
		/* now go get it */
		size = mpu6000::read_gyro((u8 *)&gyro_report, sizeof(gyro_report));
		if (size != sizeof(gyro_report)) {
			ERR("%s:　ERROR: READ 1");
			ret = -EIO;
			goto out;
		}

		gyro_scale.x_offset += gyro_report.x;
		gyro_scale.y_offset += gyro_report.y;
		gyro_scale.z_offset += gyro_report.z;
		good_count++;
	}

	if (good_count < 4000) {
		ret = -EIO;
		goto out;
	}
	gyro_scale.x_offset /= counter;
	gyro_scale.y_offset /= counter;
	gyro_scale.z_offset /= counter;
	
	memcpy(&_gyro_scale, &gyro_scale, sizeof(gyro_scale));

	INF("%s: gyro_scale: \n"
		"	X: x_offset = %f, x_scale = %f. \n"
		"	Y: y_offset = %f, y_scale = %f. \n"
		"	Z: z_offset = %f, z_scale = %f. \n", 
		_devname,
		_gyro_scale.x_offset, _gyro_scale.x_scale, 
		_gyro_scale.y_offset, _gyro_scale.y_scale, 
		_gyro_scale.z_offset, _gyro_scale.z_scale);

	/* TODO: save params */

	return 0;

out:
	return -1;
}

void mpu6000::write_checked_reg(u32 reg, u8 value)
{
	write_reg8(reg, value);
	for (u8 i = 0; i < MPU6000_NUM_CHECKED_REGISTERS; i++) {
		if (reg == _checked_registers[i]) {
			_checked_values[i] = value;
		}
	}
}


s32 mpu6000::read_reg8(u8 reg)
{
    s32 ret = 0;
    u8 data = 0;
    mpu6000::read_reg(reg, &data, 1);
    if (ret < 0) {
        return -1;
    }

    return ((s32)data);
}

s32 mpu6000::write_reg8(u8 reg, u8 data)
{
    s32 ret = 0;
    ret = mpu6000::write_reg(reg, &data, 1);
    if (ret < 0) {
        return -1;
    }

    return 0;
}

s32 mpu6000::read_reg(u8 reg, u8 *buf, u8 len)
{
    s32 ret = 0;
    reg = READ_CMD | reg;
    _gpio_cs->set_value(false);
    struct spi_msg msgs[] = {
        [0] = {
            .buf = &reg,
            .len = sizeof(reg),
            .flags = 0,
        },
        [1] = {
            .buf = buf,
            .len = len,
            .flags = SPI_M_RD,
        },
    };
    ret = _spi->transfer(&msgs[0]);
    if (ret < 0) {
        goto fail0;
    }
    ret = _spi->transfer(&msgs[1]);
    if (ret < 0) {
        goto fail1;
    }
    _gpio_cs->set_value(true);

    return 0;

fail1:
fail0:
    _gpio_cs->set_value(true);
    return -1;
}

s32 mpu6000::write_reg(u8 reg, u8 *buf, u8 len)
{
    s32 ret = 0;
    reg = WRITE_CMD & reg;
    _gpio_cs->set_value(false);
    struct spi_msg msgs[] = {
        [0] = {
            .buf = &reg,
            .len = sizeof(reg),
            .flags = 0,
        },
        [1] = {
            .buf = buf,
            .len = len,
            .flags = 0,
        },
    };
    ret = _spi->transfer(&msgs[0]);
    if (ret < 0) {
        goto fail0;
    }
    ret = _spi->transfer(&msgs[1]);
    if (ret < 0) {
        goto fail1;
    }
    _gpio_cs->set_value(true);

    return 0;

fail1:
fail0:
    _gpio_cs->set_value(true);
    return -1;
}


}

#if 0
//得到dmp处理后的数据(注意,本函数需要比较多堆栈,局部变量有点多)
//pitch:俯仰角 精度:0.1°   范围:-90.0° <---> +90.0°
//roll:横滚角  精度:0.1°   范围:-180.0°<---> +180.0°
//yaw:航向角   精度:0.1°   范围:-180.0°<---> +180.0°
//返回值:0,正常
//    其他,失败
s8 mpu6000::dmp_get_data(f32 *pitch, f32 *roll, f32 *yaw)
{
	s8 ret = 0;
	f32 q0=1.0f,q1=0.0f,q2=0.0f,q3=0.0f;
	unsigned long sensor_timestamp;
	s16 gyro[3], accel[3], sensors;
	u8 more;
	long quat[4];
	ret = dmp_read_fifo(gyro, accel, quat, &sensor_timestamp, &sensors,&more);
	if(ret) return -1;
	/* Gyro and accel data are written to the FIFO by the DMP in chip frame and hardware units.
	 * This behavior is convenient because it keeps the gyro and accel outputs of dmp_read_fifo and mpu_read_fifo consistent.
	**/
	/*if (sensors & INV_XYZ_GYRO )
	send_packet(PACKET_TYPE_GYRO, gyro);
	if (sensors & INV_XYZ_ACCEL)
	send_packet(PACKET_TYPE_ACCEL, accel); */
	/* Unlike gyro and accel, quaternions are written to the FIFO in the body frame, q30.
	 * The orientation is set by the scalar passed to dmp_set_orientation during initialization.
	**/
	if(sensors&INV_WXYZ_QUAT)
	{
		return -2;
	}
	q0 = quat[0] / q30;	// q30格式转换为浮点数
	q1 = quat[1] / q30;
	q2 = quat[2] / q30;
	q3 = quat[3] / q30;
	// 计算得到俯仰角/横滚角/航向角
	*pitch = asin(-2 * q1 * q3 + 2 * q0* q2)* 57.3;	// pitch
	*roll  = atan2(2 * q2 * q3 + 2 * q0 * q1, -2 * q1 * q1 - 2 * q2* q2 + 1)* 57.3;	// roll
	*yaw   = atan2(2*(q1*q2 + q0*q3),q0*q0+q1*q1-q2*q2-q3*q3) * 57.3;	//yaw

	return 0;
}
#endif

/***********************************************************************
** End of file
***********************************************************************/
