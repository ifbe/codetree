#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <termios.h>




//out1:		pitch,yaw,roll
//out2:		basespeed
extern float ebase[3];
extern int thresholdspeed[4];

//............
static struct termios stored_settings;




//new thread
static void* keyboardthread(void* in)
{
	int ret;

	printf("6s left\n");
	sleep(1);

	printf("5s left\n");
	sleep(1);

	printf("4s left\n");
	sleep(1);

	printf("3s left\n");
	sleep(1);

	printf("2s left\n");
	sleep(1);

	printf("1s left\n");
	sleep(1);

	printf("now go\n");
	tcdrain(0);
	tcflush(0, TCIFLUSH);

	while(1)
	{
		ret=getchar();
		if(ret=='\033')
		{
			getchar();	//'['
			ret=getchar();

			if(ret==65)
			{
				if(thresholdspeed[0] < 500)
				{
					thresholdspeed[0]+=10;
					thresholdspeed[1]+=10;
					thresholdspeed[2]+=10;
					thresholdspeed[3]+=10;
					printf("threshold=%d\n",thresholdspeed[0]);
				}
			}
			else if(ret==66)
			{
				if(thresholdspeed[0]>0)
				{
					thresholdspeed[0]-=10;
					thresholdspeed[1]-=10;
					thresholdspeed[2]-=10;
					thresholdspeed[3]-=10;
					printf("threshold=%d\n",thresholdspeed[0]);
				}
			}
		}
	}
}




int initcontrol()
{
	int ret;
	struct termios new_settings;
	pthread_t control=0;

	//control thread
	ret = pthread_create (&control, NULL, keyboardthread, NULL);
	if (ret != 0)
	{
		printf("fail@pthread_create\n");
		return 0;
	}

	//
	tcgetattr(0,&stored_settings);
	new_settings = stored_settings;
	new_settings.c_lflag &= (~ICANON);
	new_settings.c_lflag &= (~ECHO);
	new_settings.c_cc[VTIME] = 0;
	new_settings.c_cc[VMIN] = 1;
	tcsetattr(0,TCSANOW,&new_settings);

	//angle
	ebase[0]=0;
	ebase[1]=0;
	ebase[2]=0;

        //speed
	thresholdspeed[0]=0;
	thresholdspeed[1]=0;
	thresholdspeed[2]=0;
	thresholdspeed[3]=0;

	return 1;
}

void killcontrol()
{
	tcsetattr(0,TCSANOW,&stored_settings);
}
