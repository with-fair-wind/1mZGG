#include "ImageCode.h"

ImageCode::ImageCode(void)
{
}

ImageCode::~ImageCode(void)
{
}

bool ImageCode::ReadBmpFormatHeader(char *filename, ImgHeadBMP &head)
{
    FILE *fp = fopen(filename, "rb+");
    if (fp == NULL)
    {
        return false;
    }

    BITMAPFILEHEADER bmpFH;
    BITMAPINFOHEADER bmpIH;
    RGBQUAD pbmpRQ[256];
    char tmp[BMP_INFO_SIZE];
    fread(&bmpFH, 1, sizeof(BITMAPFILEHEADER), fp);
    fread(&bmpIH, 1, sizeof(BITMAPINFOHEADER), fp);
    //判断高度是否为负
    bmpIH.biHeight = (bmpIH.biHeight>0) ? bmpIH.biHeight : (-1 * bmpIH.biHeight);
    //判断是否存在颜色表
    int paleteSiz = (bmpIH.biBitCount <= 8) ? 256 : 0;//颜色表的条数
    if (paleteSiz != 0)
    {
        fread(pbmpRQ, 1, paleteSiz*sizeof(RGBQUAD), fp);
    }
    fread(tmp, 1, BMP_INFO_SIZE, fp);
    /// bmp附属信息头解码
    DecodeBmpHead(tmp, head);

    head.uiWidth = bmpIH.biWidth;
    head.uiHeight = bmpIH.biHeight;

    head.uiPixelColor = 1;/// 图像通道数
    head.uiPixelBit = bmpIH.biBitCount;/// 图像位数

    fclose(fp);

    return true;
}

bool ImageCode::ReadBmpFormat(char* filename, int iWidth, int iHeight, char* data, ImgHeadBMP &head)
{
	FILE *fp = fopen(filename, "rb+");
	if (fp == NULL)
	{
		return false;
	}

	BITMAPFILEHEADER bmpFH;
	BITMAPINFOHEADER bmpIH;
	RGBQUAD pbmpRQ[256];
	char tmp[BMP_INFO_SIZE];
	fread(&bmpFH, 1, sizeof(BITMAPFILEHEADER), fp);
	fread(&bmpIH, 1, sizeof(BITMAPINFOHEADER), fp);
	//判断高度是否为负
	bmpIH.biHeight = (bmpIH.biHeight>0) ? bmpIH.biHeight : (-1 * bmpIH.biHeight);
	//判断是否存在颜色表
	int paleteSiz = (bmpIH.biBitCount <= 8) ? 256 : 0;//颜色表的条数
	if (paleteSiz != 0)
	{
		fread(pbmpRQ, 1, paleteSiz*sizeof(RGBQUAD), fp);
	}
	fread(tmp, 1, BMP_INFO_SIZE, fp);
	/// bmp附属信息头解码
	DecodeBmpHead(tmp, head);

	head.uiWidth = bmpIH.biWidth;
	head.uiHeight = bmpIH.biHeight;

	if (head.uiWidth != iWidth || head.uiHeight != iHeight)
		return false;

	head.uiPixelColor = 1;/// 图像通道数
	head.uiPixelBit = bmpIH.biBitCount;/// 图像位数

	if (data != NULL)
	{
		fseek(fp, bmpFH.bfOffBits, 0);// 偏移量
		fread(data, bmpIH.biBitCount / 8, bmpIH.biWidth*bmpIH.biHeight, fp);
	}

	fclose(fp);

	return true;
}

void ImageCode::DecodeBmpHead(char* data1, ImgHeadBMP& head)
{
    memset(&head, 0, sizeof(ImgHeadBMP));
    unsigned char* data = (unsigned char*)data1;
	///bmp 图像附加信息头解码
	memcpy((void*)&head.uiTargetID, (void*)&data[5], 3);	// IDO
	memcpy((void*)&head.uiTeleID, (void*)&data[8], 2);	// IDS
    head.uiYear = 2000 + data[10];	// TYr
    head.uiMonth = data[11];	// TMo
    head.uiDay = data[12];	// TDy
	unsigned int uiTotalSec;
    memcpy((void*)&uiTotalSec, (void*)&data[13], 4);	// TSe
    double dTotalSec = (double)uiTotalSec / 10000.0;
    head.uiHour = floor(dTotalSec/3600);
    head.uiMinute = floor(dTotalSec/60) - head.uiHour * 60;
    head.uiSecond = (unsigned int)floor(dTotalSec) % 60;
    head.uiMSec = floor((dTotalSec - floor(dTotalSec)) * 1000.0);
    head.uiUSec = floor(((dTotalSec - floor(dTotalSec)) * 1000.0 - head.uiMSec) * 1000.0);
    memcpy((void*)&head.iZTCode, (void*)&data[17], 2);	// CS
	short sDeltaX, sDeltaY;
    memcpy((void*)&sDeltaX, (void*)&data[19], 2);	// DeX
    memcpy((void*)&sDeltaY, (void*)&data[21], 2);	// DeY
	double dDeltaX = (double)sDeltaX / 10;	
	double dDeltaY = (double)sDeltaY / 10;
	head.dPixelScaleX = (double)data[23];
    head.dPixelScaleY = (double)data[24];
    unsigned int uiAzi = 0, uiEle = 0;
//    memcpy((void*)&uiAzi, (void*)&data[25], 3);
//    memcpy((void*)&uiEle, (void*)&data[28], 3);
    uiAzi = (unsigned int)data[27] * 65536 + (unsigned int)data[26] * 256 + (unsigned int)data[25];
    uiEle = (unsigned int)data[30] * 65536 + (unsigned int)data[29] * 256 + (unsigned int)data[28];
    head.dAziDeg = (double)uiAzi / pow(2.0, 24.0) * 360.0;
    head.dEleDeg = (double)uiEle / pow(2.0, 24.0) * 360.0;
	unsigned int uiDist;
    memcpy((void*)&uiDist, (void*)&data[31], 4);
	head.dDist = (double)uiDist / 4;
    unsigned int uiFocal = 0;
    memcpy((void*)&uiFocal, (void*)&data[35], 3);
	head.dFocal = (double)uiFocal / 100;
    unsigned short uiFrameRate;
    memcpy((void*)&uiFrameRate, (void*)&data[38], 2);
	head.dFrameRate = (double)uiFrameRate / 100;
    unsigned short uiTemp;
    memcpy((void*)&uiTemp, (void*)&data[40], 2);
	head.dTemp = (double)uiTemp / 10;
    unsigned short uiHumidity;
    memcpy((void*)&uiHumidity, (void*)&data[42], 2);
	head.dHumidity = (double)uiHumidity / 100;
    unsigned short uiAtmosP;
    memcpy((void*)&uiAtmosP, (void*)&data[44], 2);
    head.dAtmosP = (double)uiAtmosP / 10 * 100;
    unsigned short uiWindSpeed;
    memcpy((void*)&uiWindSpeed, (void*)&data[46], 2);
	head.dWindSpeed = (double)uiWindSpeed / 10;
    head.dCL = (double)data[48] / 10;
    unsigned int uiExpTime = 0;
    memcpy((void*)&uiExpTime, (void*)&data[49], 3);
    head.dExposure = (double)uiExpTime / 10;
}

void ImageCode::EncodeBmpAtt(ImgHeadBMP head, char* data)
{
	memset(data, 0, BMP_INFO_SIZE);
	/// 图像头编码
	data[0] = 'B';
	data[1] = 'G';
	data[2] = 'Y';
	unsigned int iVYr = 9;
	unsigned int iVMo = 1;
	memcpy((void*)&data[3], (void*)&iVYr, 1);
	memcpy((void*)&data[4], (void*)&iVMo, 1);
	memcpy((void*)&data[5], (void*)&head.uiTargetID, 3);
	memcpy((void*)&data[8], (void*)&head.uiTeleID, 2);
	unsigned int uiYear2 = head.uiYear - 2000;
	memcpy((void*)&data[10], (void*)&uiYear2, 1);
	memcpy((void*)&data[11], (void*)&head.uiMonth, 1);
	memcpy((void*)&data[12], (void*)&head.uiDay, 1);
	unsigned int uiTotalSec = (unsigned int)(((double)head.uiHour * 3600.0 + (double)head.uiMinute * 60.0 + (double)head.uiSecond + (double)head.uiMSec * 0.001 + (double)head.uiUSec * 0.000001) * 10000.0);
	memcpy((void*)&data[13], (void*)&uiTotalSec, 4);
	memcpy((void*)&data[17], (void*)&head.iZTCode, 2);
	short sDeltaX = (short)(head.dDeltaX * 10);
	short sDeltaY = (short)(head.dDeltaY * 10);
	memcpy((void*)&data[19], (void*)&sDeltaX, 2);
	memcpy((void*)&data[21], (void*)&sDeltaY, 2);
	unsigned int uiPixelScaleX = (unsigned int)(head.dPixelScaleX);
	unsigned int uiPixelScaleY = (unsigned int)(head.dPixelScaleY);
	memcpy((void*)&data[23], (void*)&uiPixelScaleX, 1);
    memcpy((void*)&data[24], (void*)&uiPixelScaleY, 1);
	unsigned int uiAzi = (unsigned int)(head.dAziDeg / 360.0 * pow(2.0, 24.0));
	unsigned int uiEle = (unsigned int)(head.dEleDeg / 360.0 * pow(2.0, 24.0));
    memcpy((void*)&data[25], (void*)&uiAzi, 3);
    memcpy((void*)&data[28], (void*)&uiEle, 3);
    unsigned int uiDist = (unsigned int)(head.dDist * 4);
    memcpy((void*)&data[31], (void*)&uiDist, 4);
    unsigned int uiFocal = (unsigned int)(head.dFocal * 100);
    memcpy((void*)&data[35], (void*)&uiFocal, 3);
    unsigned int uiFrameRate = (unsigned int)(head.dFrameRate * 100);
    memcpy((void*)&data[38], (void*)&uiFrameRate, 2);
    unsigned int uiTemp = (unsigned int)(head.dTemp * 10);
    memcpy((void*)&data[40], (void*)&uiTemp, 2);
    unsigned int uiHumidity = (unsigned int)(head.dHumidity * 100);
    memcpy((void*)&data[42], (void*)&uiHumidity, 2);
    unsigned int uiAtmosP = (unsigned int)(head.dAtmosP / 100 * 10);
    memcpy((void*)&data[44], (void*)&uiAtmosP, 2);
    unsigned int uiWindSpeed = (unsigned int)(head.dWindSpeed * 10);
    memcpy((void*)&data[46], (void*)&uiWindSpeed, 2);
    unsigned int uiCL = (unsigned int)(head.dCL * 10);
    memcpy((void*)&data[48], (void*)&uiCL, 1);
    unsigned int uiExpTime = (unsigned int)(head.dExposure * 10);
    memcpy((void*)&data[49], (void*)&uiExpTime, 3);
    memset((void*)&data[52], 0, 11);
//    memcpy((void*)&data[24], (void*)&uiAzi, 3);
//    memcpy((void*)&data[27], (void*)&uiEle, 3);
//    unsigned int uiDist = (unsigned int)(head.dDist * 4);
//    memcpy((void*)&data[30], (void*)&uiDist, 4);
//    unsigned int uiFocal = (unsigned int)(head.dFocal * 100);
//    memcpy((void*)&data[34], (void*)&uiFocal, 3);
//    unsigned int uiFrameRate = (unsigned int)(head.dFrameRate * 100);
//    memcpy((void*)&data[37], (void*)&uiFrameRate, 2);
//    unsigned int uiTemp = (unsigned int)(head.dTemp * 10);
//    memcpy((void*)&data[39], (void*)&uiTemp, 2);
//    unsigned int uiHumidity = (unsigned int)(head.dHumidity * 100);
//    memcpy((void*)&data[41], (void*)&uiHumidity, 2);
//    unsigned int uiAtmosP = (unsigned int)(head.dAtmosP / 100 * 10);
//    memcpy((void*)&data[43], (void*)&uiAtmosP, 2);
//    unsigned int uiWindSpeed = (unsigned int)(head.dWindSpeed * 10);
//    memcpy((void*)&data[45], (void*)&uiWindSpeed, 2);
//    unsigned int uiCL = (unsigned int)(head.dCL * 10);
//    memcpy((void*)&data[47], (void*)&uiCL, 1);
//    unsigned int uiExpTime = (unsigned int)(head.dExposure * 10);
//    memcpy((void*)&data[48], (void*)&uiExpTime, 4);
//    memset((void*)&data[52], 0, 11);
}

void ImageCode::EncodeBmpFileHeader(ImgHeadBMP head, BITMAPFILEHEADER & bmpFH)
{
	bmpFH.bfType = 0x4d42; //BM
	unsigned long ulImgSiz = head.uiWidth * head.uiHeight * head.uiPixelColor * head.uiPixelBit/8;
	int paleteSiz = (head.uiPixelBit <= 8) ? 256 : 0;//颜色表的条数
	bmpFH.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + paleteSiz*sizeof(RGBQUAD) + BMP_INFO_SIZE;
	bmpFH.bfSize = bmpFH.bfOffBits + ulImgSiz;

	bmpFH.bfReserved1 = 0;
	bmpFH.bfReserved2 = 0;
}

void ImageCode::EncodeBmpInfoHeader(ImgHeadBMP head, BITMAPINFOHEADER & bmpIH)
{
	bmpIH.biSize = sizeof(BITMAPINFOHEADER);
	bmpIH.biWidth = head.uiWidth;
	bmpIH.biHeight = head.uiHeight;
	bmpIH.biBitCount = head.uiPixelBit;
    bmpIH.biCompression = 0L;    // BI_RGB
	bmpIH.biPlanes = 1;
	bmpIH.biSizeImage = head.uiWidth * head.uiHeight * head.uiPixelColor * head.uiPixelBit / 8;

	// 目标装备的水平/垂直解析度
	bmpIH.biXPelsPerMeter = 0;
	bmpIH.biYPelsPerMeter = 0;

	/// 位图文件实际应用的颜色数索引
	bmpIH.biClrUsed = (head.uiPixelBit <= 8) ? (unsigned int)pow(2.0, (int)head.uiPixelBit) : 0;

	/// 显示位图时，是否需要颜色数索引
	bmpIH.biClrImportant = 0;
}

void ImageCode::EncodeRGBQUAD(ImgHeadBMP head, RGBQUAD *pbmpRQ)
{
	int paleteSiz = (head.uiPixelBit <= 8) ? 256 : 0;//颜色表的条数
	for (int i = 0; i < paleteSiz; i++)
	{
        pbmpRQ[i].rgbBlue =(unsigned char) i;
        pbmpRQ[i].rgbGreen = (unsigned char)i;
        pbmpRQ[i].rgbRed = (unsigned char)i;
		pbmpRQ[i].rgbReserved = 0;
	}
}

bool ImageCode::WriteBmpFormat(char* filename, BITMAPFILEHEADER *pbmpFH,BITMAPINFOHEADER *pbmpIH,RGBQUAD *pbmpRQ,char* head, int size_head, char* data, int size_data)
{
	FILE *fp = fopen(filename, "wb+");
	if (fp == NULL)
	{
		return false;
	}
	//写bmp位图文件头
	fwrite(pbmpFH, 1, sizeof(BITMAPFILEHEADER), fp);
	//写bmp信息头
	fwrite(pbmpIH, 1, sizeof(BITMAPINFOHEADER), fp);
	//写颜色表
	int paleteSiz = (pbmpIH->biBitCount <= 8) ? 256 : 0;//颜色表的条数
	if (paleteSiz)
	{
		fwrite(pbmpRQ, 1, paleteSiz*sizeof(RGBQUAD), fp);
	}
	//写图像附属信息(30BYTE)
	if (head != NULL && size_head > 0)
	{
		fwrite(head, 1, size_head, fp);
	}

	fwrite(data, 1, size_data, fp);
	fclose(fp);

    return true;
}


