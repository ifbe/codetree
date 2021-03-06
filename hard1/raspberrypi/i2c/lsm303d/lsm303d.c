#define BYTE unsigned char
#include<stdio.h>
#include<string.h>
#include<errno.h>
#include<fcntl.h>
#include<unistd.h>
#include<linux/i2c.h>
#include<linux/i2c-dev.h>

//
static unsigned char reg[12];

//
float measure[6];




int initlsm303d()
{
/*
        write(0x57, CTRL_REG1);  // 0x57 = ODR=50hz, all accel axes on
        write((3<<6)|(0<<3), CTRL_REG2);  // set full-scale
        write(0x00, CTRL_REG3);  // no interrupt
        write(0x00, CTRL_REG4);  // no interrupt
        write((4<<2), CTRL_REG5);  // 0x10 = mag 50Hz output rate
        write(MAG_SCALE_2, CTRL_REG6); //magnetic scale = +/-1.3Gauss
        write(0x00, CTRL_REG7);  // 0x00 = continouous conversion mode
*/
	//LSM303_CTRL1
	reg[0]=0x77;
	systemi2c_write(0x1d,0x20,reg,1);
	usleep(1000);

	//LSM303_CTRL2
	reg[0]=0x00;
	systemi2c_write(0x1d,0x21,reg,1);
	usleep(1000);

	//LSM303_CTRL3
	reg[0]=0x4;
	systemi2c_write(0x1d,0x22,reg,1);
	usleep(1000);

	//LSM303_CTRL4
	reg[0]=0x4;
	systemi2c_write(0x1d,0x23,reg,1);
	usleep(1000);

	//LSM303_CTRL5
	reg[0]=0xf4;
	systemi2c_write(0x1d,0x24,reg,1);
	usleep(1000);

	//LSM303_CTRL6
	reg[0]=0x20;
	systemi2c_write(0x1d,0x25,reg,1);
	usleep(1000);

	//LSM303_CTRL7
	reg[0]=0x0;
	systemi2c_write(0x1d,0x26,reg,1);
	usleep(1000);




	//
	return 1;
}
void killlsm303d()
{
}




int readlsm303d()
{
	int temp;

	//accel
	systemi2c_read(0x1d, 0x28, reg, 1);
	systemi2c_read(0x1d, 0x29, reg+1, 1);
	systemi2c_read(0x1d, 0x2a, reg+2, 1);
	systemi2c_read(0x1d, 0x2b, reg+3, 1);
	systemi2c_read(0x1d, 0x2c, reg+4, 1);
	systemi2c_read(0x1d, 0x2d, reg+5, 1);

	//mag
	systemi2c_read(0x1d, 0x8, reg+6, 1);
	systemi2c_read(0x1d, 0x9, reg+7, 1);
	systemi2c_read(0x1d, 0xa, reg+8, 1);
	systemi2c_read(0x1d, 0xb, reg+9, 1);
	systemi2c_read(0x1d, 0xc, reg+10, 1);
	systemi2c_read(0x1d, 0xd, reg+11, 1);
/*
	for(temp=0;temp<12;temp++)
	{
		printf("%.2x ",reg[temp]);
	}
	printf("\n");
*/




	//
	temp = *(short*)(reg+0x0);
	measure[0] = temp * 9.8 / 16384;

	temp=*(short*)(reg+0x2);
	measure[1] = temp * 9.8 / 16384;

	temp=*(short*)(reg+0x4);
	measure[2] = temp * 9.8 / 16384;

	//
	temp=*(short*)(reg+0x6);
	measure[3] = temp;

	temp=*(short*)(reg+0x8);
	measure[4] = temp;

	temp=*(short*)(reg+10);
	measure[5] = temp;




	printf("%1.1f	%1.1f	%1.1f	%1.1f	%1.1f	%1.1f\n",
		measure[0],
		measure[1],
		measure[2],
		measure[3],
		measure[4],
		measure[5]
	);

}

void main()
{
	systemi2c_init();
	initlsm303d();

	while(1)
	{
		readlsm303d();
	}
}
