/**
 * @file bmp.c
 *
 * @author     James Strawson
 * @date       3/14/2018
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <rc/i2c.h>
#include <rc/bmp.h>
#include <rc/time.h>
#include <rc/model.h>
#include "bmp_defs.h"

#define BMP_BUS 2

// local struct for calibration data
typedef struct bmp280_cal_t{
	uint16_t dig_T1;
	int16_t  dig_T2;
	int16_t  dig_T3;
	uint16_t dig_P1;
	int16_t  dig_P2;
	int16_t  dig_P3;
	int16_t  dig_P4;
	int16_t  dig_P5;
	int16_t  dig_P6;
	int16_t  dig_P7;
	int16_t  dig_P8;
	int16_t  dig_P9;
	double sea_level_pa;
}bmp280_cal_t;

// global variables
static bmp280_cal_t rc_bmp280_cal;
// init_flag: 0 = off, 1 = I2C mode, 2 = IIO mode
static int rc_bmp280_init_flag = 0; 
static char rc_iio_base_path[128];
// file descriptors for the IIO channels, kept open between reads
static int rc_iio_temp_fd = -1;
static int rc_iio_press_fd = -1;

// reads an IIO/sysfs attribute as a double. sysfs regenerates the value on
// every read at offset 0, so the fd can stay open and be reused with pread
static int __iio_read_double(int fd, double* out)
{
	char buf[64];
	char* end;
	ssize_t n = pread(fd, buf, sizeof(buf)-1, 0);
	if(n<=0) return -1;
	buf[n] = '\0';
	*out = strtod(buf, &end);
	if(end==buf) return -1;
	return 0;
}

static void __iio_close(void)
{
	if(rc_iio_temp_fd>=0)  close(rc_iio_temp_fd);
	if(rc_iio_press_fd>=0) close(rc_iio_press_fd);
	rc_iio_temp_fd = -1;
	rc_iio_press_fd = -1;
}

// best effort: write an integer to an IIO attribute. returns 0 on success
static int __iio_write_int(const char* attr, int value)
{
	char path[256];
	FILE* f;
	int ret;
	snprintf(path, sizeof(path), "%s%s", rc_iio_base_path, attr);
	f = fopen(path, "w");
	if(!f) return -1;
	ret = (fprintf(f, "%d", value) < 0) ? -1 : 0;
	if(fclose(f)!=0) ret = -1; // the driver reports invalid values on close/flush
	return ret;
}

// looks for a barometer exposed by the kernel IIO subsystem and opens its
// temperature and pressure channels. returns 0 if a usable device was found.
static int __iio_init(rc_bmp_oversample_t oversample)
{
	int i, n, ratio;
	char path[256];
	char name_buf[64];
	double tmp;
	FILE* f;

	__iio_close();

	for(i=0; i<16; i++){
		snprintf(path, sizeof(path), "/sys/bus/iio/devices/iio:device%d/name", i);
		f = fopen(path, "r");
		if(!f) continue;
		name_buf[0] = '\0';
		if(fgets(name_buf, sizeof(name_buf), f)==NULL) name_buf[0] = '\0';
		fclose(f);
		if(strstr(name_buf, "bmp")==NULL) continue;

		snprintf(rc_iio_base_path, sizeof(rc_iio_base_path), "/sys/bus/iio/devices/iio:device%d/", i);

		snprintf(path, sizeof(path), "%sin_temp_input", rc_iio_base_path);
		rc_iio_temp_fd = open(path, O_RDONLY);
		snprintf(path, sizeof(path), "%sin_pressure_input", rc_iio_base_path);
		rc_iio_press_fd = open(path, O_RDONLY);

		// make sure both channels really deliver data before committing
		if(rc_iio_temp_fd>=0 && rc_iio_press_fd>=0 &&
				__iio_read_double(rc_iio_temp_fd, &tmp)==0 &&
				__iio_read_double(rc_iio_press_fd, &tmp)==0){
			break;
		}
		fprintf(stderr,"WARNING: in rc_bmp_init, %s looks like a barometer but its channels can't be read\n", rc_iio_base_path);
		__iio_close();
	}
	if(rc_iio_temp_fd<0 || rc_iio_press_fd<0) return -1;

	// the oversample enum holds the BMP280 register bits (n<<2, n=1..5)
	// which map to a ratio of 1,2,4,8,16. Temperature stays at x1 like in the
	// I2C path. The filter setting has no equivalent here and is ignored.
	// Failing to change these only means the driver defaults stay in effect.
	n = ((int)oversample>>2) & 0x07;
	ratio = (n>=1 && n<=5) ? (1<<(n-1)) : 1;
	if(__iio_write_int("in_pressure_oversampling_ratio", ratio)<0){
		fprintf(stderr,"WARNING: in rc_bmp_init, couldn't set pressure oversampling through IIO, using driver default\n");
	}
	__iio_write_int("in_temp_oversampling_ratio", 1); // optional attribute, ignore failure
	return 0;
}

int rc_bmp_init(rc_bmp_oversample_t oversample, rc_bmp_filter_t filter)
{
	int i;

	// on boards where the kernel owns the barometer (AI-64) it is exposed
	// through IIO. Everywhere else fall through to the I2C driver below.
	if(__iio_init(oversample)==0){
		rc_bmp280_cal.sea_level_pa = DEFAULT_SEA_LEVEL_PA;
		rc_bmp280_init_flag = 2; // IIO flag
		return 0;
	}
	if(rc_model()==MODEL_BB_AI64){
		fprintf(stderr,"ERROR in rc_bmp_init: no IIO barometer found\n");
		fprintf(stderr,"check that the robotics cape overlay is loaded and the kernel driver bound to it\n");
		return -1;
	}

	// make sure the bus is not currently in use by another thread
	// do not proceed to prevent interfering with that process
	uint8_t buf[24];
	uint8_t c;
	
	if(rc_i2c_get_lock(BMP_BUS)){
		fprintf(stderr,"WARNING in rc_bmp_init, i2c bus claimed by another thread\n");
		fprintf(stderr,"Continuing anyway.\n");
	}
	
	// initialize the bus
	if(rc_i2c_init(BMP_BUS, BMP280_ADDR)<0){
		fprintf(stderr,"ERROR: in rc_bmp_init failed to initialize i2c bus\n");
		return -1;
	}
	
	// claiming the bus does no guarantee other code will not interfere
	// with this process, but best to claim it so other code can check
	// like we did above
	rc_i2c_lock_bus(BMP_BUS);
	
	// reset the barometer
	if(rc_i2c_write_byte(BMP_BUS, BMP280_RESET_REG, BMP280_RESET_WORD)<0){
		fprintf(stderr,"ERROR: in rc_bmp_init failed to send reset byte to barometer\n");
		rc_i2c_unlock_bus(BMP_BUS);
		return -1;
	}
	
	// check the chip ID register
	if(rc_i2c_read_byte(BMP_BUS, BMP280_CHIP_ID_REG, &c)<0){
		fprintf(stderr,"ERROR: in rc_bmp_init, failed to read chip_id register\n");
		rc_i2c_unlock_bus(BMP_BUS);
		return -1;
	}
	if(c != BMP280_CHIP_ID){
		fprintf(stderr,"ERROR: in rc_bmp_init, barometer returned wrong chip_id\n");
		fprintf(stderr,"received: %x  expected: %x\n", c, BMP280_CHIP_ID_REG);
		rc_i2c_unlock_bus(BMP_BUS);
		return -1;
	}
	
	// set up the bmp measurement control register settings
	// no temperature oversampling,  normal continuous read mode
	c = BMP_MODE_NORMAL;
	c |= BMP_TEMP_OVERSAMPLE_1;
	c |= oversample;
	
	// write the measurement control register
	if(rc_i2c_write_byte(BMP_BUS,BMP280_CTRL_MEAS,c)<0){
		fprintf(stderr,"ERROR: in rc_bmp_init, can't write to measurement control register\n");
		rc_i2c_unlock_bus(BMP_BUS);
		return -1;
	}
	
	// set up the filter config register
	c = BMP280_TSB_0;	// minimal sleep delay between samples
	c |= filter;		// user selectable filter coefficient
	if(rc_i2c_write_byte(BMP_BUS,BMP280_CONFIG,c)<0){
		fprintf(stderr,"ERROR: in rc_bmp_init, failed to write to bmp_config register\n");
		rc_i2c_unlock_bus(BMP_BUS);
		return -1;
	}
	
	// keep checking the status register untill the NVM calibration is ready
	// after a short wait
	i = 0;
	c = BMP280_IM_UPDATE_STATUS;
	do{
		rc_usleep(20000);
		if(rc_i2c_read_byte(BMP_BUS, BMP280_STATUS_REG	, &c)<0){
			fprintf(stderr,"ERROR: in rc_bmp_init can't read status byte from barometer\n");
			rc_i2c_unlock_bus(BMP_BUS);
			return -1;
		}
		if(i>10){
			fprintf(stderr,"ERROR: in rc_bmp_init factory NVM calibration not available yet\n");
			rc_i2c_unlock_bus(BMP_BUS);
			return -1;
		}
		i++;
	}while(c&BMP280_IM_UPDATE_STATUS);
	
	// retrieve the factory NVM calibration data all in one go
	if(rc_i2c_read_bytes(BMP_BUS,BMP280_DIG_T1,24,buf)<0){
		fprintf(stderr,"ERROR: in rc_bmp_init failed to load factory calibration registers\n");
		rc_i2c_unlock_bus(BMP_BUS);
		return -1;
	}
	
	// save calibration in useful format
	rc_bmp280_cal.dig_T1 = (uint16_t) ((buf[1] << 8) | buf [0]);
	rc_bmp280_cal.dig_T2 = (uint16_t) ((buf[3] << 8) | buf [2]);
	rc_bmp280_cal.dig_T3 = (uint16_t) ((buf[5] << 8) | buf [4]);
	rc_bmp280_cal.dig_P1 = (uint16_t) ((buf[7] << 8) | buf [6]);
	rc_bmp280_cal.dig_P2 = (uint16_t) ((buf[9] << 8) | buf [8]);
	rc_bmp280_cal.dig_P3 = (uint16_t) ((buf[11] << 8) | buf [10]);
	rc_bmp280_cal.dig_P4 = (uint16_t) ((buf[13] << 8) | buf [12]);
	rc_bmp280_cal.dig_P5 = (uint16_t) ((buf[15] << 8) | buf [14]);
	rc_bmp280_cal.dig_P6 = (uint16_t) ((buf[17] << 8) | buf [16]);
	rc_bmp280_cal.dig_P7 = (uint16_t) ((buf[19] << 8) | buf [18]);
	rc_bmp280_cal.dig_P8 = (uint16_t) ((buf[21] << 8) | buf [20]);
	rc_bmp280_cal.dig_P9 = (uint16_t) ((buf[23] << 8) | buf [22]);
	
	// use default pressure for now unless user sets it otherwise
	rc_bmp280_cal.sea_level_pa = DEFAULT_SEA_LEVEL_PA;
	
	// release control of the bus
	rc_i2c_unlock_bus(BMP_BUS);
	
	// wait for bmp to finish it's internal initialization
	rc_usleep(50000);
	rc_bmp280_init_flag=1;
	return 0;
}



int rc_bmp_power_off(void)
{
	// IIO power off block
	if(rc_bmp280_init_flag == 2) {
		__iio_close();
		rc_bmp280_init_flag = 0;
		return 0;
	}

	// I2C power off block
	if(rc_bmp280_init_flag == 1) {
		// make sure the bus is not currently in use by another thread
		// do not proceed to prevent interfering with that process
		if(rc_i2c_get_lock(BMP_BUS)){
			fprintf(stderr,"WARNING: in rc_bmp_power_off i2c bus claimed by another thread\n");
			fprintf(stderr,"Continuing anyway.\n");
		}
		// set the i2c address
		if(rc_i2c_set_device_address(BMP_BUS, BMP280_ADDR)<0){
			fprintf(stderr,"ERROR: in rc_bmp_power_off failed to set the i2c device address\n");
			rc_i2c_unlock_bus(BMP_BUS);
			return -1;
		}
		// write the measurement control register to go into sleep mode
		if(rc_i2c_write_byte(BMP_BUS,BMP280_CTRL_MEAS,BMP_MODE_SLEEP)<0){
			fprintf(stderr,"ERROR: in rc_bmp_power_off cannot write bmp control mode register\n");
			rc_i2c_unlock_bus(BMP_BUS);
			return -1;
		}
		
		// release control of the bus
		rc_i2c_unlock_bus(BMP_BUS);
		rc_bmp280_init_flag=0;
		return 0;
	}
	return 0;
}

int rc_bmp_read(rc_bmp_data_t* data)
{
	// sanity checks
	if(rc_bmp280_init_flag==0){
		fprintf(stderr,"ERROR in rc_bmp_read, call rc_bmp_init first\n");
		return -1;
	}
	if(data==NULL){
		fprintf(stderr, "ERROR in rc_bmp_read, received NULL pointer\n");
		return -1;
	}

	// IIO read block
	if(rc_bmp280_init_flag == 2) {
		double temp_raw, press_raw;

		// in_temp_input is in milli degrees C, in_pressure_input in kPa
		if(__iio_read_double(rc_iio_temp_fd, &temp_raw)<0){
			fprintf(stderr,"ERROR in rc_bmp_read, failed to read IIO temperature\n");
			return -1;
		}
		if(__iio_read_double(rc_iio_press_fd, &press_raw)<0){
			fprintf(stderr,"ERROR in rc_bmp_read, failed to read IIO pressure\n");
			return -1;
		}
		data->temp_c = temp_raw / 1000.0;
		data->pressure_pa = press_raw * 1000.0;
		data->alt_m = 44330.0 * (1.0 - pow((data->pressure_pa / rc_bmp280_cal.sea_level_pa), 0.1903));
		return 0;
	}

	// I2C read block
	if(rc_bmp280_init_flag == 1) {
		int64_t var1, var2, var3, var4, t_fine, T, p;
		uint8_t raw[6];
		int32_t adc_P, adc_T;
		
		// check claim bus state to avoid stepping on IMU reads
		if(rc_i2c_get_lock(BMP_BUS)){
			fprintf(stderr,"WARNING: in rc_bmp_read, i2c bus is claimed by another thread, aborting\n");
			return -1;
		}
		
		// claim bus for ourselves and set the device address
		rc_i2c_lock_bus(BMP_BUS);
		if(rc_i2c_set_device_address(BMP_BUS, BMP280_ADDR)<0){
			fprintf(stderr,"ERROR: in rc_bmp_read, failed to set the i2c device address\n");
			rc_i2c_unlock_bus(BMP_BUS);
			return -1;
		}
		
		// if new data is ready, read it in
		if(rc_i2c_read_bytes(BMP_BUS,BMP280_PRESSURE_MSB,6,raw)<0){
			fprintf(stderr,"ERROR: in rc_bmp_read, failed to read barometer data registers\n");
			rc_i2c_unlock_bus(BMP_BUS);
			return -1;
		}
		rc_i2c_unlock_bus(BMP_BUS);
		
		// run the numbers, thanks to Bosch for putting this code in their datasheet
		adc_P = (raw[0] << 12)| (raw[1] << 4)|(raw[2] >> 4);
		adc_T = (raw[3] << 12)| (raw[4] << 4)|(raw[5] >> 4);

		var1  = ((((adc_T>>3) - ((int32_t)rc_bmp280_cal.dig_T1 <<1))) *
						((int32_t)rc_bmp280_cal.dig_T2)) >> 11;
		var2  = (((((adc_T>>4) - ((int32_t)rc_bmp280_cal.dig_T1)) *
						((adc_T>>4) - ((int32_t)rc_bmp280_cal.dig_T1))) >> 12) *
						((int32_t)rc_bmp280_cal.dig_T3)) >> 14;
		
		t_fine = var1 + var2;
		
		T  = (t_fine * 5 + 128) >> 8;
		data->temp_c =  T/100.0;
		
		var3 = ((int64_t)t_fine) - 128000;
		var4 = var3 * var3 * (int64_t)rc_bmp280_cal.dig_P6;
		var4 = var4 + ((var3*(int64_t)rc_bmp280_cal.dig_P5)<<17);
		var4 = var4 + (((int64_t)rc_bmp280_cal.dig_P4)<<35);
		var3 = ((var3 * var3 * (int64_t)rc_bmp280_cal.dig_P3)>>8) +
		   			((var3 * (int64_t)rc_bmp280_cal.dig_P2)<<12);
		var3 = (((((int64_t)1)<<47)+var3))*((int64_t)rc_bmp280_cal.dig_P1)>>33;
		
		// avoid exception caused by division by zero
		if(var3==0){
			fprintf(stderr,"ERROR in rc_bmp_read, invalid data read\n");
			return -1;
		}
		
		p = 1048576 - adc_P;
		p = (((p<<31) - var4)*3125) / var3;
		var3 = (((int64_t)rc_bmp280_cal.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
		var4 = (((int64_t)rc_bmp280_cal.dig_P8) * p) >> 19;
		
		p = ((p + var3 + var4) >> 8) + (((int64_t)rc_bmp280_cal.dig_P7) << 4);
		data->pressure_pa = p/256.0;
		
		data->alt_m = 44330.0*(1.0 - pow((data->pressure_pa/rc_bmp280_cal.sea_level_pa), 0.1903));
		
		return 0;
	}

	return -1;
}

int rc_bmp_set_sea_level_pressure_pa(double pa)
{
	// sanity checks
	if(rc_bmp280_init_flag==0){
		fprintf(stderr, "ERROR in rc_set_sea_level_pressure_pa, call rc_bmp_init first\n");
		return -1;
	}
	if(pa<80000 || pa>120000){
		fprintf(stderr,"ERROR: in rc_set_sea_level_pressure_pa, Please enter a reasonable\n");
		fprintf(stderr," pressure between 80,000 & 120,000 pascals.\n");
		return -1;
	}
	rc_bmp280_cal.sea_level_pa = pa;
	return 0;
}
