#include <iostream>
#include "CubeEye.h"
#include "CubeEyeDef.h"
#include "opencv2/opencv.hpp"
#include "Viewer.h"

using namespace std;
using namespace cv;
using namespace CUBE_EYE;

inline int Convert_To_GRAY8(const float fValue, BYTE& nGrayData, float fMinValue, float fMaxValue)
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

int main()
{
	SampleViewer cube("test");

	if (cube.DeviceOpen() == 0){	printf("Device Open Success.\n");}
	else{							printf("Device Open Fail.\n");return 1;}

	cube.m_objCubeEye.Start();
	cube.m_bEnableReadDataThread = true;

	cube.m_nDataType = 0; //IR
	cube.m_nDisplayData = 0;//IR
	VideoWriter writer;
	double fps = 50.0;
	double time = 30 * fps;  // 20초 동안 촬영
	int cnt=0;
	try
	{
		writer.open("Intensity.avi", VideoWriter::fourcc('M', 'J', 'P', 'G'), fps, Size(VGA_WIDTH, VGA_HEIGHT), 0);
		if (!writer.isOpened())
		{
			cout << "동영상을 저장하기 위한 초기화 작업 중 에러 발생" << endl;
			throw 1;
		}
		while (cube.m_bEnableReadDataThread)
		{
			//Read Raw Frame
			if (cube.m_objCubeEye.ReadDepthIRFrame(cube.m_pCameraBuf[1], cube.m_pCameraBuf[0], cube.m_stFrameInfo) >= 0)
			{
				//cube.m_nFrameIndex = cube.m_stFrameInfo.nFrameIndex;
				//Copy to Raw Depth/IR Buffer
				cube.DisplayBufferFlip(&cube.m_pCameraBuf[0], &cube.m_pCameraBuf[1], true);

				if (cube.DisplayBufferFlip(&cube.m_pDisplayBuf[IR], &cube.m_pDisplayBuf[DEPTH]))
				{
					//*********** Read Data Display ****************//
					register int x, y;
					//ir display
					if (cube.m_nDisplayData == IR)
					{
						Mat srcimg;
						srcimg.create(VGA_HEIGHT, VGA_WIDTH, CV_8UC1);
						unsigned char nGray8 = 0;

						for (y = 0; y < VGA_HEIGHT; ++y)
						{
							for (x = 0; x < VGA_WIDTH; ++x)
							{
								//Convert Gray8 Data
								Convert_To_GRAY8(cube.m_pDisplayBuf[IR][y * VGA_WIDTH + x], nGray8, 0.0f, 255.0f);
								srcimg.at<BYTE>(y, x) = nGray8;
							}//for
						}//for
						writer.write(srcimg);
						imshow("IR_img", srcimg);
					}
				}
			}
			int key = waitKey(50);
			if (key == 27) {
				writer.release();
				cube.DeviceClose();
				destroyAllWindows();
				break;
			}
			if (cnt > time) {
				break;
			}
			cnt += 1;
		}
	}catch (bool e){
		cout << "[error]camera close : "<< endl;
	}
}
