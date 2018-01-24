#include "PushFlow.h"
#include <stdio.h>

int main()
{
	int num = 10;

	CPushFlow::Init();
	
	CPushFlow* p = new CPushFlow[num];
	for(int i = 0; i< num; ++i)
	{
		char rtmpurl[256] = {0};
		sprintf(rtmpurl, "rtmp://192.168.34.40/live/test%02d", i+1);
		p[i].connect("192.168.3.87", rtmpurl);
		p[i].start();
		printf(rtmpurl);
		printf("\n");
	}

	CPushFlow f1;
	f1.connect("192.168.3.88", "rtmp://192.168.34.40/live/test");
	f1.start();
	CPushFlow f2;
	f2.connect("192.168.3.87", "rtmp://192.168.34.40/live/test87");
	f2.start();
	getchar();

	f1.stop();
	f2.stop();

	for(int i=0; i<num; ++i)
		p[i].stop();

	printf("stop\n");

	CPushFlow::Uninit();
	return 0;
}