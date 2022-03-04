#include <iostream>
#include <opencv2/opencv.hpp>
#include "Viewer.h"

using namespace std;
using namespace cv;

#define GL_WIN_SIZE_X	1280
#define GL_WIN_SIZE_Y	960
#define TEXTURE_SIZE	512

#define DEFAULT_DISPLAY_MODE	DISPLAY_MODE_DEPTH

#define MIN_NUM_CHUNKS(data_size, chunk_size)	((((data_size)-1) / (chunk_size) + 1))
#define MIN_CHUNKS_SIZE(data_size, chunk_size)	(MIN_NUM_CHUNKS(data_size, chunk_size) * (chunk_size))

SampleViewer* SampleViewer::ms_self = NULL;


static unsigned char  nPalletIntsR[1282] = { 0, };
static unsigned char  nPalletIntsG[1282] = { 0, };
static unsigned char  nPalletIntsB[1282] = { 0, };

//0~1279(Red ~ Purple)
void CreateRainbowPallet()
{
	float fColorWeight;

	unsigned char r, g, b;
	for (int i=0; i<1282; ++i)
	{
		fColorWeight = (float)i / 1280.0f;

		if( (fColorWeight <= 1.0f) && (fColorWeight > 0.8f) )
		{
			r = (BYTE)(255 * ((fColorWeight - 0.8f) / 0.2f));
			g = 0;
			b = 255;
		}

		else if( (fColorWeight <= 0.8f) && (fColorWeight > 0.6f) )
		{
			r = 0;
			g = (BYTE)(255 * (1.0f - (fColorWeight - 0.6f) / 0.2f));
			b = 255;
		}

		else if( (fColorWeight <= 0.6f) && (fColorWeight > 0.4f) )
		{
			r = 0;
			g = 255;
			b = (BYTE)(255 * ((fColorWeight - 0.4f) / 0.2f));
		}

		else if( (fColorWeight <= 0.4f) && (fColorWeight > 0.2f) )
		{
			r = (BYTE)(255 * (1.0f - (fColorWeight - 0.2f) / 0.2f));
			g = 255;
			b = 0;
		}

		else if( (fColorWeight <= 0.2f) && (fColorWeight >= 0.0f) )
		{
			r = 255;
			g = (BYTE)(255 * ((fColorWeight - 0.0f) / 0.2f));
			b = 0;
		}

		else
		{
			r = 0;
			g = 0;
			b = 0;
		}

		nPalletIntsR[i] = r;
		nPalletIntsG[i] = g;
		nPalletIntsB[i] = b;
	}//for

}

SampleViewer::SampleViewer(const char* strSampleName)
{
	ms_self = this;

	m_nDataType = POINT_CLOUD;//data type
	m_nDisplayData = POINTCLOUD_Z;//Display data type

	m_bCameraInit = false;

	m_bDisplayBufAvailable[0] = false;
	m_bDisplayBufAvailable[1] = false;

	int nWidth = 640;
	int nHeight = 480;

	//Raw Depth Buffer Init
	for (int i = 0; i<2; ++i)//0:IR, 1:Depth
	{
		m_pCameraBuf[i] = NULL;
		m_pDisplayBuf[i] = NULL;
		m_pTriDisplayBuf[i] = NULL;

		m_pCameraBuf[i] = (unsigned short *)malloc(sizeof(uint16) * nWidth * nHeight);
		m_pDisplayBuf[i] = (unsigned short *)malloc(sizeof(uint16) * nWidth * nHeight);
		m_pTriDisplayBuf[i] = (unsigned short *)malloc(sizeof(uint16) * nWidth * nHeight);
	}

	//PCL Buffer Init
	m_pCameraPCLBuf = NULL;
	m_pDisplayPCLBuf = NULL;
	m_pTriDisplayPCLBuf = NULL;

	m_pCameraPCLBuf = (cePointCloud *)malloc(sizeof(cePointCloud)	* nWidth * nHeight);
	m_pDisplayPCLBuf = (cePointCloud *)malloc(sizeof(cePointCloud)	* nWidth * nHeight);
	m_pTriDisplayPCLBuf = (cePointCloud *)malloc(sizeof(cePointCloud)	* nWidth * nHeight);


	InitializeCriticalSection(&m_csLockDisplay[0]);
	InitializeCriticalSection(&m_csLockDisplay[1]);

	//create for rainbow color table - for display
	CreateRainbowPallet();

}

SampleViewer::~SampleViewer()
{
	//Buffer Free
	for (int i = 0; i<2; ++i)
	{
		if (m_pCameraBuf[i] != NULL)
		{
			free(m_pCameraBuf[i]);
			m_pCameraBuf[i] = NULL;
		}
		if (m_pDisplayBuf[i] != NULL)
		{
			free(m_pDisplayBuf[i]);
			m_pDisplayBuf[i] = NULL;
		}
		if (m_pTriDisplayBuf[i] != NULL)
		{
			free(m_pTriDisplayBuf[i]);
			m_pTriDisplayBuf[i] = NULL;
		}
	}

	free(m_pCameraPCLBuf);
	m_pCameraPCLBuf = NULL;

	free(m_pDisplayPCLBuf);
	m_pDisplayPCLBuf = NULL;

	free(m_pTriDisplayPCLBuf);
	m_pTriDisplayPCLBuf = NULL;

	DeleteCriticalSection(&m_csLockDisplay[0]);
	DeleteCriticalSection(&m_csLockDisplay[1]);
}


/*
*	@author			K.W.Yeom
*	@date			2013. 02. 08.
*
*	@func			Convert_To_RGB888()
*   @param			nValue - raw data, nColorTableIndex - Color Table Index
					fMinValue - Min Value
					fMaxValue - Max Value
					nColorTableIndex - color table index

*	@description	set color table
*/
inline void Convert_To_RGB888(const float fValue, unsigned short &nColorTableIndex, float fMinValue, float fMaxValue)
{
	if (fValue == 0.0f) //Invalide Pixel
	{
		nColorTableIndex = 1281;//Black
	}
	else if (fValue < fMinValue)
	{
		nColorTableIndex = 0;//Red
	}
	else if (fValue > fMaxValue)
	{
		nColorTableIndex = 1280;//Purple
	}
	else//Rainbow Color
	{
		//Red ~ Purple
		nColorTableIndex = (int)(((fValue - fMinValue) / (fMaxValue - fMinValue)) * 1280.0f);

	}
}

/*
*	@author			K.W.Yeom
*	@date			2013. 02. 08.
*
*	@func			Convert_To_GRAY8()
*   @param			nValue - raw data, nRGBData - Gray Buffer

*	@description	set gray level
*/
inline int Convert_To_GRAY8(const float fValue, BYTE &nGrayData, float fMinValue, float fMaxValue)
{
	/*float fColorWeight;
	if(fValue >= 0.0f)
	fColorWeight = (float)fValue / fMaxValue;
	else
	fColorWeight = (float)(fValue-fMinValue) / fMaxValue;*/

	if (fValue < fMinValue) nGrayData = 0;
	else if (fValue > fMaxValue) nGrayData = 255;
	else
	{
		nGrayData = (BYTE)(255.0f * (fValue - fMinValue) / (fMaxValue - fMinValue));
	}

	return TRUE;
}

//camera open
int SampleViewer::DeviceOpen()
{
	int nDevCount = 0;

	int nCamNum = -1;

	//Cubeeye Camera search
	vector<ceDevicePath> pDevicePath = m_objCubeEye.DeviceSearch();

	int nCameraCount = pDevicePath.size();

	if (nCameraCount < 1)
	{
		printf("Camera Search Fail, Checking usb connection.");
		m_bCameraInit = false;
	}
	else
	{
		//Control Camera Number(< nCameraCount(0 ~ 9))
		int nCameraNum = 0;

		//connect to first(0) Cubeeye camera
		int status = m_objCubeEye.Connect(pDevicePath[nCameraNum]);//Connect Camera

		if (status < 0)
		{
			printf("\nCamera Connect Fail, Checking usb connection.\n");

			m_bCameraInit = false;

			return -1;
		}
		else
		{
			printf("\nCamera - Initialize Success\n");

			char szDevName[256];			//Name
			char szProductNum[256];		//Product Number
			char szSerialNum[256];		//Sereial Number
			strcpy_s(szDevName, m_objCubeEye.pDevInfo.szDevName);
			strcpy_s(szProductNum, m_objCubeEye.pDevInfo.szProductName);
			strcpy_s(szSerialNum, m_objCubeEye.pDevInfo.szSerialNumber);

			//Get Max,Min Range Info.
			uint16 nMaxRange, nMInRange;
			m_objCubeEye.getDepthRange(nMaxRange, nMInRange);
			m_objCubeEye.setIROutputMode(1);
			printf("%s - %s, s/n:%s\n", szDevName, szProductNum, szSerialNum);

			m_bCameraInit = true;

		}
	}

	return 0;
}

//camera close
void SampleViewer::DeviceClose()
{
	if (m_bCameraInit == true)
	{
		m_objCubeEye.Stop();
		m_objCubeEye.Disconnect();

		m_bCameraInit = false;
	}

}

//raw data flip
bool SampleViewer::DisplayBufferFlip(unsigned short **pBuf1, unsigned short **pBuf2, bool bPrimaryBuffer/*=false*/)
{
	bool bFlipped = false;

	EnterCriticalSection(&m_csLockDisplay[0]);
	{
		if (bPrimaryBuffer || m_bDisplayBufAvailable[0])
		{
			unsigned short *pTmp[2];

			pTmp[0] = *pBuf1;
			pTmp[1] = *pBuf2;

			*pBuf1 = m_pTriDisplayBuf[0];
			*pBuf2 = m_pTriDisplayBuf[1];

			m_pTriDisplayBuf[0] = pTmp[0];
			m_pTriDisplayBuf[1] = pTmp[1];

			bFlipped = true;
		}
		m_bDisplayBufAvailable[0] = bPrimaryBuffer;
	}
	LeaveCriticalSection(&m_csLockDisplay[0]);

	return bFlipped;
}

//pcl data flip
bool SampleViewer::DisplayBufferFlip_PCL(cePointCloud **pBuf, bool bPrimaryBuffer/*=false*/)
{
	bool bFlipped = false;

	EnterCriticalSection(&m_csLockDisplay[1]);
	{
		if (bPrimaryBuffer || m_bDisplayBufAvailable[1])
		{
			cePointCloud *pTmp;

			pTmp = *pBuf;

			*pBuf = m_pTriDisplayPCLBuf;

			m_pTriDisplayPCLBuf = pTmp;

			bFlipped = true;
		}
		m_bDisplayBufAvailable[1] = bPrimaryBuffer;
	}
	LeaveCriticalSection(&m_csLockDisplay[1]);

	return bFlipped;
}

//Data Reading from device thread
unsigned __stdcall ReadDataThread(void*);

//data read thread
unsigned __stdcall ReadDataThread(void* pParam)
{
	SampleViewer *pApp = (SampleViewer *)pParam;

	while (pApp->m_bEnableReadDataThread)
	{
		//Read Raw Frame
		if (pApp->m_nDataType == DEPTH_IR)
		{
			if (pApp->m_objCubeEye.ReadDepthIRFrame(pApp->m_pCameraBuf[1], pApp->m_pCameraBuf[0], pApp->m_stFrameInfo) >= 0)
			{
				pApp->m_nFrameIndex = pApp->m_stFrameInfo.nFrameID;

				//Copy to Raw Depth/IR Buffer
				pApp->DisplayBufferFlip(&pApp->m_pCameraBuf[0], &pApp->m_pCameraBuf[1], true);
			}
		}
		else if (pApp->m_nDataType == POINT_CLOUD)
		{
			if (pApp->m_objCubeEye.ReadPCLFrame(pApp->m_pCameraPCLBuf, pApp->m_stFrameInfo) >= 0)
			{
				pApp->m_nFrameIndex = pApp->m_stFrameInfo.nFrameID;

				//Copy to RPCL Buffer
				pApp->DisplayBufferFlip_PCL(&pApp->m_pCameraPCLBuf, true);
			}
		}

		Sleep(1);

		pApp->CountDataReadTime();
	}

	return 0;
}

//Init GL, start stream & reading thread
int SampleViewer::init(int argc, char **argv)
{
	m_width		= VGA_WIDTH;
	m_height	= VGA_HEIGHT;

	// Texture map init
	m_nTexMapX = MIN_CHUNKS_SIZE(m_width, TEXTURE_SIZE);
	m_nTexMapY = MIN_CHUNKS_SIZE(m_height, TEXTURE_SIZE);
	m_pTexMap = new RGB888Pixel[m_nTexMapX * m_nTexMapY];

	//********** Data Read Start process *****************//

	//fps check timer start
	InitializeTimer();

	//Start Stream
	m_objCubeEye.Start();

	//Read thread Start
	m_bEnableReadDataThread = true;
	hThread = (HANDLE) _beginthreadex(NULL, 0, ReadDataThread, (void*) this, 0, (unsigned *) &dwThreadid);

	printf("Point Cloude Data Display Start\n");

	printf("Key 'Esc': exit\n");
	printf("Key   '1': Depth Data\n");
	printf("Key   '2': IR Data\n");
	printf("Key   '3': point cloud z data\n");
	printf("Key   '4': point cloud i  Data\n");

	//*****************************************************//

	return 1;

}

//display run
int SampleViewer::run()	//Does not return
{
	//glutMainLoop();

	return 1;
}

//Display callback thread
void SampleViewer::display()
{
	Sleep(1);

	if (m_nDataType == POINT_CLOUD)//Depth Stream Mode
	{
		if (DisplayBufferFlip_PCL(&m_pDisplayPCLBuf))//PCL Mode
		{
			RGB888Pixel* pTexRow = m_pTexMap + 0 * m_nTexMapX;

			//*********** Read Data Display ****************//

			register int x, y;

			///point cloud z display => color
			if (m_nDisplayData == POINTCLOUD_Z)
			{
				unsigned short nRGBIndex = 0;

				for (y = 0; y < VGA_HEIGHT; ++y)
				{
					RGB888Pixel* pTex = pTexRow + 0;//m_depthFrame.getCropOriginX();

					for (x = 0; x < VGA_WIDTH; ++x, ++pTex)
					{
						//Convert RGB888 Data
						Convert_To_RGB888(m_pDisplayPCLBuf[y*VGA_WIDTH + x].fZ, nRGBIndex, 0.1f, 4.0f);

						pTex->r = nPalletIntsR[nRGBIndex];
						pTex->g = nPalletIntsG[nRGBIndex];
						pTex->b = nPalletIntsB[nRGBIndex];

					}//for

					pTexRow += m_nTexMapX;
				}//for
			}

			//point cloud i display => gray
			else if (m_nDisplayData == POINTCLOUD_I)
			{
				unsigned char nGray8 = 0;

				for (y = 0; y < VGA_HEIGHT; ++y)
				{
					RGB888Pixel* pTex = pTexRow + 0;//m_depthFrame.getCropOriginX();

					for (x = 0; x < VGA_WIDTH; ++x, ++pTex)
					{
						//Convert Gray8 Data
						Convert_To_GRAY8(m_pDisplayPCLBuf[y*VGA_WIDTH + x].fI, nGray8, 0.0f, 0.5f);

						pTex->r = pTex->g = pTex->b = nGray8;

					}//for

					pTexRow += m_nTexMapX;
				}//for
			}//else if

		}//if
		else
		{
			return;
		}
	}//if

	else//Depth/IR mode
	{
		if (DisplayBufferFlip(&m_pDisplayBuf[IR], &m_pDisplayBuf[DEPTH]))
		{
			RGB888Pixel* pTexRow = m_pTexMap + 0 * m_nTexMapX;

			//*********** Read Data Display ****************//

			register int x, y;

			///raw depth display
			if (m_nDisplayData == DEPTH)
			{
				unsigned short nRGBIndex = 0;

				for (y = 0; y < VGA_HEIGHT; ++y)
				{
					RGB888Pixel* pTex = pTexRow + 0;//m_depthFrame.getCropOriginX();

					for (x = 0; x < VGA_WIDTH; ++x, ++pTex)
					{
						//Convert RGB888 Data
						Convert_To_RGB888(m_pDisplayBuf[DEPTH][y*VGA_WIDTH + x], nRGBIndex, 0.2f, 4000.0f);

						pTex->r = nPalletIntsR[nRGBIndex];
						pTex->g = nPalletIntsG[nRGBIndex];
						pTex->b = nPalletIntsB[nRGBIndex];

					}//for

					pTexRow += m_nTexMapX;
				}//for
			}

			//ir display
			else if (m_nDisplayData == IR)
			{
				unsigned char nGray8 = 0;

				for (y = 0; y < VGA_HEIGHT; ++y)
				{
					RGB888Pixel* pTex = pTexRow + 0;//m_depthFrame.getCropOriginX();

					for (x = 0; x < VGA_WIDTH; ++x, ++pTex)
					{
						//Convert Gray8 Data
						Convert_To_GRAY8(m_pDisplayBuf[IR][y*VGA_WIDTH + x], nGray8, 0.0f, 200.0f);
						pTex->r = pTex->g = pTex->b = nGray8;

					}//for

					pTexRow += m_nTexMapX;
				}//for
			}//else if
		}
		else
		{
			return;
		}
	}
	//******************************************//
	int nXRes = m_width;
	int nYRes = m_height;
}


void SampleViewer::InitializeTimer()
{
	m_dwPrevDisplayTimeTick = GetTickCount64();
	m_dwDisplayTimeTick = 1;
	m_dwDisplayTime = m_dwFDisplayTime = 10000;
	m_nDisplayCount = 1;
}

//Fps measurement
void SampleViewer::CountDataReadTime()
{
	m_dwDisplayTimeTick = GetTickCount64();

	m_dwDisplayTime += (m_dwDisplayTimeTick - m_dwPrevDisplayTimeTick);
	m_dwPrevDisplayTimeTick = m_dwDisplayTimeTick;

	m_nDisplayCount++;
	if(m_nDisplayCount >= 10)
	{
		m_dwFDisplayTime = m_dwDisplayTime;
		m_nDisplayCount = 0;
		m_dwDisplayTime = 0;
	}
}

//key check
void SampleViewer::onKey(unsigned char key, int /*x*/, int /*y*/)
{
	//IR
	if (m_nDataType != DEPTH_IR && m_bEnableReadDataThread == true)//Stop
	{
		//Thread stop
		m_bEnableReadDataThread = false;
		WaitForSingleObject(hThread, 500);
		CloseHandle(hThread);

		//camera stop
		m_objCubeEye.Stop();
	}

	m_nDataType = DEPTH_IR;
	m_nDisplayData = IR;

	Sleep(500);

	if (m_bEnableReadDataThread == false)//Start
	{
		//fps check timer start
		InitializeTimer();

		//Start Stream
		m_objCubeEye.Start();

		//Read thread Start
		m_bEnableReadDataThread = true;
		hThread = (HANDLE)_beginthreadex(NULL, 0, ReadDataThread, (void*) this, 0, (unsigned *)&dwThreadid);
	}

	printf("Raw IR data display\n");

}