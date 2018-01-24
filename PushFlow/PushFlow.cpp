#include "PushFlow.h"

#include "sps_decode.h"
#include "librtmp/rtmp_sys.h"
#include "librtmp/log.h"

#pragma comment(lib, "VzLPRSDK.lib")
#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "librtmp.lib")

#define BUFSIZE (1024*500)
#define SAFEFREE(handle,func) if(handle!=NULL) {func(handle);handle=NULL;}

int CPushFlow::Init()
{
	if(VzLPRClient_Setup() == -1)
		return -1;

	return 0;
}

int CPushFlow::Uninit()
{
	VzLPRClient_Cleanup();

	return 0;
}

CPushFlow::CPushFlow(void)
{
	m_rtmp = NULL;
	m_hclient = NULL;
	m_headlen = 0; 
	begintime = 0;
	m_buf = (char*)malloc(BUFSIZE);
	memset(m_buf, 0, BUFSIZE);
	m_pHeadData = (char*)malloc(BUFSIZE);
	memset(m_pHeadData, 0, BUFSIZE);
}

CPushFlow::~CPushFlow(void)
{
}

int CPushFlow::connect(const char* ip, char* rtmpurl)
{
	int res = 0;

	m_rtmp =  RTMP_Alloc();
	if(m_rtmp == NULL)
		goto RET;
	RTMP_Init(m_rtmp);
	if(rtmpurl==NULL || (res = RTMP_SetupURL(m_rtmp, rtmpurl))!=1)
		goto RET;
	//if unable,the AMF command would be 'play' instead of 'publish'
	RTMP_EnableWrite(m_rtmp);	
	if((res = RTMP_Connect(m_rtmp, NULL))!=1)
		goto RET;
	if((res = RTMP_ConnectStream(m_rtmp,0))!=1)
		goto RET;

	if(ip==NULL || (m_hclient = VzLPRClient_Open(ip,80,"admin","admin"))==NULL)
		goto RET;

RET:
	if(m_hclient==NULL || m_rtmp==NULL)
	{
		disconnect();
		return -1;
	}
	else
		return 0;
}

int CPushFlow::disconnect()
{
	stop();
	
	return 0;
}

int CPushFlow::start()
{
	return VzLPRClient_SetRealDataCallBack(m_hclient, GetRealDataCB, this);
}

int CPushFlow::stop()
{
	SAFEFREE(m_hclient,VzLPRClient_Close);
	RTMP_Close(m_rtmp);
	SAFEFREE(m_rtmp,RTMP_Free);

	return 0;
}

void __stdcall CPushFlow::GetRealDataCB(VzLPRClientHandle handle, void *pUserData, VZ_LPRC_DATA_TYPE eDataType, const VZ_LPRC_DATA_INFO *pData)
{
 	int spshead=0, ppshead=0, naltail_pos=0;
 	int spslen=0, ppslen=0;
 	int index = 0, i =0;
 	int res;
 	CPushFlow* pushflow = (CPushFlow*)pUserData;
	RTMPPacket packet = {0};
	int width, heigh, fps;

	if(pData->uDataSize > BUFSIZE-100)
		printf("OFF....\n");

	if(pushflow==NULL)
		return;
	switch(eDataType)
	{
	case VZ_LPRC_DATA_ENC_VIDEO:
		//if(pushflow->m_headlen == 0)
		{
			while(spshead<pData->uDataSize)
			{  
				if(pData->pBuffer[spshead++] == 0x00 && pData->pBuffer[spshead++] == 0x00) 
				{
					if(pData->pBuffer[spshead++] == 0x01)
						goto gotnal_head;
					else 
					{
						spshead--;		
						if(pData->pBuffer[spshead++] == 0x00 && pData->pBuffer[spshead++] == 0x01)
							goto gotnal_head;
						else
							continue;
					}
				}
				else 
					continue;
			}
gotnal_head:
			naltail_pos = spshead;  
			while (naltail_pos<pData->uDataSize)  
			{  
				if(pData->pBuffer[naltail_pos++] == 0x00 && pData->pBuffer[naltail_pos++] == 0x00 )
				{  
					if(pData->pBuffer[naltail_pos++] == 0x01)
					{
						spslen = (naltail_pos-3)-spshead;
						break;
					}
					else
					{
						naltail_pos--;
						if(pData->pBuffer[naltail_pos++] == 0x00 && pData->pBuffer[naltail_pos++] == 0x01)
						{	
							spslen = (naltail_pos-4)-spshead;
							break;
						}
					}
				}  
			}
			if(naltail_pos==pData->uDataSize)
			{
				naltail_pos = 0;
				goto COMMON;
			}

			ppshead = naltail_pos; 
			while (naltail_pos<pData->uDataSize)  
			{  
				if(pData->pBuffer[naltail_pos++] == 0x00 && pData->pBuffer[naltail_pos++] == 0x00 )
				{  
					if(pData->pBuffer[naltail_pos++] == 0x01)
					{
						ppslen = (naltail_pos-3)-ppshead;
						break;
					}
					else
					{
						naltail_pos--;
						if(pData->pBuffer[naltail_pos++] == 0x00 && pData->pBuffer[naltail_pos++] == 0x01)
						{	
							ppslen = (naltail_pos-4)-ppshead;
							break;
						}
					}
				}  
			}

			i = 0;
			pushflow->m_pHeadData[i++] = 0x17;
			pushflow->m_pHeadData[i++] = 0x00;

			pushflow->m_pHeadData[i++] = 0x00;
			pushflow->m_pHeadData[i++] = 0x00;
			pushflow->m_pHeadData[i++] = 0x00;

			/*AVCDecoderConfigurationRecord*/
			pushflow->m_pHeadData[i++] = 0x01;
			pushflow->m_pHeadData[i++] = pData->pBuffer[spshead+1];
			pushflow->m_pHeadData[i++] = pData->pBuffer[spshead+2];
			pushflow->m_pHeadData[i++] = pData->pBuffer[spshead+3];
			pushflow->m_pHeadData[i++] = 0xff;

			/*sps*/
			pushflow->m_pHeadData[i++]   = 0xe1;
			pushflow->m_pHeadData[i++] = (spslen >> 8) & 0xff;
			pushflow->m_pHeadData[i++] = spslen & 0xff;
			memcpy(pushflow->m_pHeadData+i,pData->pBuffer+spshead,spslen);
			//h264_decode_sps((BYTE*)&pushflow->m_pHeadData[i],spslen,width,heigh,fps);
			i +=  spslen;
			
			/*pps*/
			pushflow->m_pHeadData[i++]   = 0x01;
			pushflow->m_pHeadData[i++] = (ppslen >> 8) & 0xff;
			pushflow->m_pHeadData[i++] = (ppslen) & 0xff;
			memcpy(pushflow->m_pHeadData+i,pData->pBuffer+ppshead,ppslen);
			i += ppslen;

			pushflow->m_headlen = 16+spslen+ppslen;
			printf("a pps nalu\n");
		}

COMMON:
		index = 0;
		pushflow->m_buf[index++] = pData->uIsKeyFrame ? 0x17 : 0x27;
		pushflow->m_buf[index++] = 0x01;
		pushflow->m_buf[index++] = 0x00;
		pushflow->m_buf[index++] = 0x00;
		pushflow->m_buf[index++] = 0x00;
		pushflow->m_buf[index++] = (pData->uDataSize-naltail_pos)>>24 &0xff;  
		pushflow->m_buf[index++] = (pData->uDataSize-naltail_pos)>>16 &0xff;  
		pushflow->m_buf[index++] = (pData->uDataSize-naltail_pos)>>8 &0xff;  
		pushflow->m_buf[index++] = (pData->uDataSize-naltail_pos)&0xff;
		memcpy(&pushflow->m_buf[index],pData->pBuffer+naltail_pos,pData->uDataSize-naltail_pos); 

		if(pData->uIsKeyFrame)
		{
			if(pushflow->begintime==0)
				pushflow->begintime= RTMP_GetTime();
			packet.m_packetType = RTMP_PACKET_TYPE_VIDEO;
			packet.m_nBodySize = pushflow->m_headlen;
			packet.m_nChannel = 0x04;
			packet.m_nTimeStamp = RTMP_GetTime() - pushflow->begintime;
			packet.m_hasAbsTimestamp = 0;
			packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
			packet.m_nInfoField2 = pushflow->m_rtmp->m_stream_id;
			packet.m_body = pushflow->m_pHeadData;

			res = RTMP_SendPacket(pushflow->m_rtmp,&packet,FALSE);
		}

		if(pushflow->begintime==0)
			pushflow->begintime= RTMP_GetTime();
		packet.m_packetType = RTMP_PACKET_TYPE_VIDEO;
		packet.m_nBodySize = 9+pData->uDataSize-naltail_pos;
		packet.m_nChannel = 0x04;
		packet.m_nTimeStamp = RTMP_GetTime() - pushflow->begintime;
		packet.m_hasAbsTimestamp = 0;
		packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
		packet.m_nInfoField2 = pushflow->m_rtmp->m_stream_id;
		packet.m_body = pushflow->m_buf;

		res = RTMP_SendPacket(pushflow->m_rtmp,&packet,TRUE);
		//printf("bodylen = %d\n", 9+pData->uDataSize-naltail_pos);
		break;
		
	default:
		break;
	}
}