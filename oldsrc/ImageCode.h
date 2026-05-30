#ifndef IMAGECODE_H
#define IMAGECODE_H

#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include <string.h>

#define BMP_INFO_SIZE  60

#pragma pack(push)
#pragma pack(push,1)

typedef struct BITMAPFILEHEADER
{
    u_int16_t bfType;
    u_int32_t bfSize;
    u_int16_t bfReserved1;
    u_int16_t bfReserved2;
    u_int32_t bfOffBits;
}BITMAPFILEHEADER;

typedef struct BITMAPINFOHEADER
{
    u_int32_t biSize;
    u_int32_t biWidth;
    u_int32_t biHeight;
    u_int16_t biPlanes;
    u_int16_t biBitCount;
    u_int32_t biCompression;
    u_int32_t biSizeImage;
    u_int32_t biXPelsPerMeter;
    u_int32_t biYPelsPerMeter;
    u_int32_t biClrUsed;
    u_int32_t biClrImportant;
}BITMAPINFODEADER;

typedef struct RGBQUAD
{
    unsigned char rgbBlue;
    unsigned char rgbGreen;
    unsigned char rgbRed;
    unsigned char rgbReserved;
}RGBQUAD;

#pragma pack(pop)

struct ImgHeadBMP
{
	unsigned int uiTargetID;	// 目标编号
	unsigned int uiTeleID;	// 站址编号
	unsigned int uiYear; // 时间信息
	unsigned int uiMonth;
	unsigned int uiDay;
	unsigned int uiHour;
	unsigned int uiMinute;
	unsigned int uiSecond;
	unsigned int uiMSec;
	unsigned int uiUSec;
	int iZTCode;//状态码
	double dDeltaX;	// X方向固定偏差,单位pixel
	double dDeltaY;	// Y方向固定偏差,单位pixel
	double dPixelScaleX;	// μm
	double dPixelScaleY;	// μm
	double dAziDeg;	// °
	double dEleDeg;	// °
	double dDist;	// 距离（0）
    double dFocal; // 焦距，单位mm
    double dFrameRate; // 相机工作帧频
    double dExposure; // 相机积分时间，单位ms
    unsigned int uiWidth; // CCD水平有效像素个数
    unsigned int uiHeight; // CCD垂直有效像素个数
    unsigned int uiPixelColor; // 图像通道数 1，3
    unsigned int uiPixelBit; // 图像位数 8，16
	unsigned int uiBitDepth;//图像有效位数
	double dTemp;	// ℃
	double dHumidity;	// 0~1
	double dAtmosP;	// Pa
	double dWindSpeed;	// m/s
	double dCL;	// 大气相干长度 cm
};

class  ImageCode
{
public:
	ImageCode(void);
	~ImageCode(void);

	static bool WriteBmpFormat(char* filename, BITMAPFILEHEADER *pbmpFH,BITMAPINFOHEADER *pbmpIH,RGBQUAD *pbmpRQ,char* head, int size_head, char* data, int size_data);

    static bool ReadBmpFormatHeader(char* filename, ImgHeadBMP &head);

    static bool ReadBmpFormat(char* filename, int iWidth, int iHeight, char* data, ImgHeadBMP &head);

    static void DecodeBmpHead(char* data, ImgHeadBMP& head);

    static void EncodeBmpAtt(ImgHeadBMP head, char* data);

    static void EncodeBmpInfoHeader(ImgHeadBMP head, BITMAPINFOHEADER & bmpIH);

    static void EncodeRGBQUAD(ImgHeadBMP head, RGBQUAD * pbmpRQ);

    static void EncodeBmpFileHeader(ImgHeadBMP head, BITMAPFILEHEADER & bmpFH);
};


#endif // IMAGECODE_H
