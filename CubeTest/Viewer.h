/*****************************************************************************
*                                                                            *
*  CubeEye Simple Viewer                                                      *
*  Copyright (C) 2014 Meerecompany Ltd.                                        *
*                                                                            *
*****************************************************************************/

#ifndef _SAMPLE_VIEWER_H_
#define _SAMPLE_VIEWER_H_


//#include "stdafx.h"
#include "CubeEyeDef.h"
#include "CubeEye.h"

using namespace CUBE_EYE;


static const uint16 VGA_WIDTH = 640;
static const uint16 VGA_HEIGHT = 480;

#define IR				0
#define DEPTH			1
#define POINTCLOUD_Z	2
#define POINTCLOUD_I	3

//RGB 888
typedef struct _RGB888Pixel
{
	/* Red value of this pixel. */
	unsigned char r;
	/* Green value of this pixel. */
	unsigned char g;
	/* Blue value of this pixel. */
	unsigned char b;
} RGB888Pixel;

class SampleViewer
{
public:
	SampleViewer(const char* strSampleNam);
	~SampleViewer();

	CCubeEye			m_objCubeEye;

	bool				m_bCameraInit;

	ceDeviceInfo		m_pDevInfo;		//device info
	ceFrameInfo			m_stFrameInfo;	//Frame info

	long				m_nFrameIndex;

	unsigned short* m_pCameraBuf[2];
	unsigned short* m_pDisplayBuf[2];
	unsigned short* m_pTriDisplayBuf[2];

	cePointCloud* m_pCameraPCLBuf;
	cePointCloud* m_pDisplayPCLBuf;
	cePointCloud* m_pTriDisplayPCLBuf;

	virtual bool		DisplayBufferFlip(unsigned short** pBuf1, unsigned short** pBuf2, bool bPrimaryBuffer = false);
	virtual bool		DisplayBufferFlip_PCL(cePointCloud** pBuf1, bool bPrimaryBuffer = false);

	virtual int			init(int argc, char** argv);
	virtual int			run();	//Does not return

	int					m_nDevOpened;
	int					m_nDataType;//0;Depth/IR, 1:Point Cloud
	int					m_nDisplayData;

	bool				m_bEnableReadDataThread;
	CRITICAL_SECTION	m_csLockDisplay[2];
	bool				m_bDisplayBufAvailable[2];
	int					DeviceOpen();
	void				DeviceClose();

	DWORD				m_dwDisplayTimeTick, m_dwPrevDisplayTimeTick;
	DWORD				m_dwDisplayTime, m_dwFDisplayTime;
	int					m_nDisplayCount;

	virtual void		InitializeTimer();
	virtual void		CountDataReadTime();


protected:
	virtual void display();
	virtual void onKey(unsigned char key, int x, int y);

	HANDLE	hThread;
	DWORD	dwThreadid;


private:
	SampleViewer(const SampleViewer&);
	SampleViewer& operator=(SampleViewer&);

	static SampleViewer* ms_self;
	//static void glut_Idle();
	//static void glut_Display();
	//static void glut_Keyboard(unsigned char key, int x, int y);
	char			m_strSampleName[256];
	unsigned int	m_nTexMapX;
	unsigned int	m_nTexMapY;

	RGB888Pixel* m_pTexMap;
	int			m_width;
	int			m_height;
};


#endif // _SAMPLE_VIEWER_H_
