#ifndef SENSOR_H
#define SENSOR_H

/* Use 'n' as magic number */
#define SENSOR_IOC_M			's'

/* IOCTLs for sensor*/
#define SENSOR_IOC_CALIBRATE _IO(SENSOR_IOC_M, 0x01)

#define CALI_NUM 4
#define SAMPLE_NUM 50

#define CALI_OFFSET_FILE "/data/misc/.gsensor_cali"

struct cali{
	int xoffset_p;
	int yoffset_p;
	int zoffset_p;
	int xoffset_n;
	int yoffset_n;
	int zoffset_n;

	int valid;
};

struct sample
{
    short x;
    short y;
    short z;
};

struct sample gsensor_cali_apply(struct sample data);
void gsensor_cali_conduct();

void gsensor_cali_init(int lsg, struct sample (*read_data)(), void (*read_init)(), void (*read_deinit)());

int gsensor_cali_get_offset();

#endif
