#pragma once
#include <WinSock2.h>
#include <Windows.h>
#include "VzLPRClientSDK.h"

struct RTMP;

class CPushFlow
{
public://Global
	static int Init();
	static int Uninit();
	static void __stdcall GetRealDataCB(VzLPRClientHandle handle, void *pUserData, VZ_LPRC_DATA_TYPE eDataType, const VZ_LPRC_DATA_INFO *pData);

public:
	CPushFlow(void);
	virtual ~CPushFlow(void);

public:
	int connect(const char* ip, char* rtmpurl);
	int disconnect();
	int start();
	int stop();

private:
	RTMP* m_rtmp;
	int m_hclient;
	char* m_pHeadData;
	unsigned int m_headlen;
	char* m_buf;
	long begintime;
};
