#pragma OPENCL EXTENSION cl_khr_fp64:enable

__kernel void kernel_Hist16(__global unsigned short* image_ptr, __global unsigned int* histogram)
{
	int gid = (int)get_global_id(0);
	uint x = image_ptr[gid];
	atom_inc(&histogram[x]);
}

__kernel void kernel_Short2Byte(__global unsigned short* image_ptr, int min0, int max0, float diff, __global unsigned char* dst_img)
{
	int gid = (int)get_global_id(0);
	if (image_ptr[gid] <= min0)
	{
		dst_img[gid] = 0;
	}
	else if (image_ptr[gid] >= max0)
	{
		dst_img[gid] = 255;
	}
	else 
	{
		float rate = (float)(image_ptr[gid] - min0) / diff;
		dst_img[gid] = (int)(rate * 255);
	}
}

__kernel void kernel_AvgGroupSum16(__global ushort* src, __global double* avgsum, __local double* lavgsum, uint nitems) 
{
	uint count = nitems / get_global_size(0);
	uint idx = get_global_id(0);
	uint stride = get_global_size(0);
	double pavg = 0;
	for (int n = 0; n < count; n++, idx += stride) 
	{
		pavg += src[idx];
	}
	barrier(CLK_LOCAL_MEM_FENCE);
	if (get_local_id(0) == 0) 
	{
		lavgsum[0] = 0;
	}
	for (int n = 0; n < get_local_size(0); n++) 
	{
		barrier(CLK_LOCAL_MEM_FENCE);
		if (get_local_id(0) == n) 
		{
			lavgsum[0] += pavg;
		}
	}
	barrier(CLK_LOCAL_MEM_FENCE);
	if (get_local_id(0) == 0) 
	{
		avgsum[get_group_id(0)] = lavgsum[0];
	}
}

__kernel void kernel_AvgGroupSum8(__global uchar* src, __global double* avgsum, __local double* lavgsum, uint nitems)
{
	uint count = nitems / get_global_size(0);
	uint idx = get_global_id(0);
	uint stride = get_global_size(0);
	double pavg = 0;
	for (int n = 0; n < count; n++, idx += stride)
	{
		pavg += src[idx];
	}
	barrier(CLK_LOCAL_MEM_FENCE);
	if (get_local_id(0) == 0)
	{
		lavgsum[0] = 0;
	}
	for (int n = 0; n < get_local_size(0); n++)
	{
		barrier(CLK_LOCAL_MEM_FENCE);
		if (get_local_id(0) == n)
		{
			lavgsum[0] += pavg;
		}
	}
	barrier(CLK_LOCAL_MEM_FENCE);
	if (get_local_id(0) == 0)
	{
		avgsum[get_group_id(0)] = lavgsum[0];
	}
}

__kernel void kernel_VarGroupSum16(__global ushort* src, double dAvg, __global double* varsum, __local double* lvarsum, uint nitems)
{
	uint count = nitems / get_global_size(0);
	uint idx = get_global_id(0);
	uint stride = get_global_size(0);
	double pvar = 0;
	for (int n = 0; n < count; n++, idx += stride)
	{
		pvar += (src[idx] - dAvg) * (src[idx] - dAvg);
	}
	barrier(CLK_LOCAL_MEM_FENCE);
	if (get_local_id(0) == 0)
	{
		lvarsum[0] = 0;
	}
	for (int n = 0; n < get_local_size(0); n++)
	{
		barrier(CLK_LOCAL_MEM_FENCE);
		if (get_local_id(0) == n)
		{
			lvarsum[0] += pvar;
		}
	}
	barrier(CLK_LOCAL_MEM_FENCE);
	if (get_local_id(0) == 0)
	{
		varsum[get_group_id(0)] = lvarsum[0];
	}
}

__kernel void kernel_VarGroupSum8(__global uchar* src, double dAvg, __global double* varsum, __local double* lvarsum, uint nitems)
{
	uint count = nitems / get_global_size(0);
	uint idx = get_global_id(0);
	uint stride = get_global_size(0);
	double pvar = 0;
	for (int n = 0; n < count; n++, idx += stride)
	{
		pvar += (src[idx] - dAvg) * (src[idx] - dAvg);
	}
	barrier(CLK_LOCAL_MEM_FENCE);
	if (get_local_id(0) == 0)
	{
		lvarsum[0] = 0;
	}
	for (int n = 0; n < get_local_size(0); n++)
	{
		barrier(CLK_LOCAL_MEM_FENCE);
		if (get_local_id(0) == n)
		{
			lvarsum[0] += pvar;
		}
	}
	barrier(CLK_LOCAL_MEM_FENCE);
	if (get_local_id(0) == 0)
	{
		varsum[get_group_id(0)] = lvarsum[0];
	}
}

__kernel void kernel_DoubleGlobalSum(__global double* gsum)
{
	for (int n = 1; n < get_global_size(0); n++)
	{
		barrier(CLK_GLOBAL_MEM_FENCE);
		if (get_global_id(0) == n)
		{
			gsum[0] += gsum[n];
		}
	}
	barrier(CLK_GLOBAL_MEM_FENCE);
}

__kernel void kernel_MinMaxGroup16(__global ushort* src, __global double* gmin, __global double* gmax,
	__local double* lmin, __local double* lmax, uint nitems) 
{
	uint count = nitems / get_global_size(0);
	uint idx = get_global_id(0);
	uint stride = get_global_size(0);
	double pmin = 100000.0;
	double pmax = 0;
	for (int n = 0; n < count; n++, idx += stride) 
	{
		pmin = (pmin < src[idx] ? pmin : src[idx]);
		pmax = (pmax > src[idx] ? pmax : src[idx]);
	}
	barrier(CLK_LOCAL_MEM_FENCE);
	if (get_local_id(0) == 0) 
	{
		lmin[0] = 100000.0;
		lmax[0] = 0;
	}
	for (int n = 0; n < get_local_size(0); n++)
	{
		barrier(CLK_LOCAL_MEM_FENCE);
		if (get_local_id(0) == n) 
		{
			lmin[0] = (lmin[0] < pmin ? lmin[0] : pmin);
			lmax[0] = (lmax[0] > pmax ? lmax[0] : pmax);
		}
	}
	barrier(CLK_LOCAL_MEM_FENCE);
	if (get_local_id(0) == 0) 
	{
		gmin[get_group_id(0)] = lmin[0];
		gmax[get_group_id(0)] = lmax[0];
	}	
}

__kernel void kernel_MinMaxGroup8(__global uchar* src, __global double* gmin, __global double* gmax,
	__local double* lmin, __local double* lmax, uint nitems)
{
	uint count = nitems / get_global_size(0);
	uint idx = get_global_id(0);
	uint stride = get_global_size(0);
	double pmin = 100000.0;
	double pmax = 0;
	for (int n = 0; n < count; n++, idx += stride)
	{
		pmin = (pmin < src[idx] ? pmin : src[idx]);
		pmax = (pmax > src[idx] ? pmax : src[idx]);
	}
	barrier(CLK_LOCAL_MEM_FENCE);
	if (get_local_id(0) == 0)
	{
		lmin[0] = 100000.0;
		lmax[0] = 0;
	}
	for (int n = 0; n < get_local_size(0); n++)
	{
		barrier(CLK_LOCAL_MEM_FENCE);
		if (get_local_id(0) == n)
		{
			lmin[0] = (lmin[0] < pmin ? lmin[0] : pmin);
			lmax[0] = (lmax[0] > pmax ? lmax[0] : pmax);
		}
	}
	barrier(CLK_LOCAL_MEM_FENCE);
	if (get_local_id(0) == 0)
	{
		gmin[get_group_id(0)] = lmin[0];
		gmax[get_group_id(0)] = lmax[0];
	}
}

__kernel void kernel_DoubleGlobalMinMax(__global double* gmin, __global double* gmax)
{
	for (int n = 1; n < get_global_size(0); n++)
	{
		barrier(CLK_GLOBAL_MEM_FENCE);
		if (get_global_id(0) == n)
		{
			gmin[0] = (gmin[0] < gmin[n] ? gmin[0] : gmin[n]);
			gmax[0] = (gmax[0] > gmax[n] ? gmax[0] : gmax[n]);
		}
	}
	barrier(CLK_GLOBAL_MEM_FENCE);
}

__kernel void kernel_GaussianFilter16(__global ushort* pausDataIn, __global ushort* pausDataOut,
	int iWidth, int iHeight, float fSigma, int iHSize)
{
	int i, j, offset;
	int x, y;
	float summ, sumg, facg;

	x = get_global_id(0);
	y = get_global_id(1);

	offset = iHSize / 2;

	if ((y - offset <= 0) || (y + offset >= iHeight) || (x - offset <= 0) || (x + offset >= iWidth))
	{
		pausDataOut[y * iWidth + x] = pausDataIn[y * iWidth + x];
	}
	else
	{
		summ = 0;
		sumg = 0;
		facg = 0;
		for (j = -offset; j <= offset; j++)
		{
			for (i = -offset; i <= offset; i++)
			{
				facg = (1.0 / (2 * 3.1415926*fSigma*fSigma)) * exp(-((j*j + i*i) / (2 * fSigma*fSigma)));
				summ += pausDataIn[(y + j)*iWidth + x + i] * facg;
				sumg += facg;
			}
		}
		pausDataOut[y * iWidth + x] = (ushort)(summ / sumg);
	}
}

__kernel void kernel_GaussianFilter8(__global uchar* paucDataIn, __global uchar* paucDataOut,
	int iWidth, int iHeight, float fSigma, int iHSize)
{
	int i, j, offset;
	int x, y;
	float summ, sumg, facg;

	x = get_global_id(0);
	y = get_global_id(1);

	offset = iHSize / 2;

	if ((y - offset <= 0) || (y + offset >= iHeight) || (x - offset <= 0) || (x + offset >= iWidth))
	{
		paucDataOut[y * iWidth + x] = paucDataIn[y * iWidth + x];
	}
	else
	{
		summ = 0;
		sumg = 0;
		facg = 0;
		for (j = -offset; j <= offset; j++)
		{
			for (i = -offset; i <= offset; i++)
			{
				facg = (1.0 / (2 * 3.1415926*fSigma*fSigma)) * exp(-((j*j + i*i) / (2 * fSigma*fSigma)));
				summ += paucDataIn[(y + j)*iWidth + x + i] * facg;
				sumg += facg;
			}
		}
		paucDataOut[y * iWidth + x] = (uchar)(summ / sumg);
	}
}

__kernel void kernel_ReduceSaturation16(__global ushort* src, __global ushort* dst, float avr, float std, float fRatioStd)
{
	int gid = (int)get_global_id(0);
//	dst[gid] = src[gid] < avr - fRatioStd * std ? (ushort)(avr - fRatioStd * std) : src[gid];
	dst[gid] = src[gid] >= avr + fRatioStd * std ? (ushort)(avr + fRatioStd * std) : src[gid];
}

__kernel void kernel_ReduceSaturation8(__global uchar* src, __global uchar* dst, float avr, float std, float fRatioStd)
{
	int gid = (int)get_global_id(0);
	dst[gid] = src[gid] >= avr + fRatioStd * std ? (uchar)(avr + fRatioStd * std)  : src[gid];
}

__kernel void kernel_SubFrame16(__global ushort* src1, __global ushort* src2, __global ushort* dst, int iWidth, int iHeight, int iCenterShiftX12, int iCenterShiftY12)
{
	int x = get_global_id(0);
	int y = get_global_id(1);

	if (x + iCenterShiftX12 >= 0 && x + iCenterShiftX12 < iWidth
		&& y + iCenterShiftY12 >= 0 && y + iCenterShiftY12 < iHeight)
	{
		ushort ussrc1 = src1[y * iWidth + x];
		ushort ussrc2 = src2[(y + iCenterShiftY12) * iWidth + (x + iCenterShiftX12)];
		dst[y * iWidth + x] = ussrc1 >= ussrc2 ? ussrc1 - ussrc2 : 0;
	}
	else
	{
		dst[y * iWidth + x] = 0;
	}
}

__kernel void kernel_SubFrame8(__global uchar* src1, __global uchar* src2, __global uchar* dst, int iWidth, int iHeight, int iCenterShiftX12, int iCenterShiftY12)
{
	int x = get_global_id(0);
	int y = get_global_id(1);

	if (x + iCenterShiftX12 >= 0 && x + iCenterShiftX12 < iWidth
		&& y + iCenterShiftY12 >= 0 && y + iCenterShiftY12 < iHeight)
	{
		uchar ucsrc1 = src1[y * iWidth + x];
		uchar ucsrc2 = src2[(y + iCenterShiftY12) * iWidth + (x + iCenterShiftX12)];
		dst[y * iWidth + x] = ucsrc1 >= ucsrc2 ? ucsrc1 - ucsrc2 : 0;
	}
	else
	{
		dst[y * iWidth + x] = 0;
	}
}

__kernel void kernel_Crop16(__global ushort* src, __global ushort* dst, int iWidthOri, int iHeightOri, int iWidthL, int iWidthR, int iHeightU, int iHeightD)
{
	int x = get_global_id(0);
	int y = get_global_id(1);

	if (x >= iWidthL && x <= iWidthR && y >= iHeightU && y <= iHeightD)
	{
		dst[(y - iHeightU) * (iWidthR - iWidthL + 1) + (x - iWidthL)] = src[y * iWidthOri + x];
	}
	else
	{
		return;
	}	
}

__kernel void kernel_Binary16(__global ushort* src, __global uchar* dst, ushort threshold)
{
	int idx = get_global_id(0);

	dst[idx] = (src[idx] <= threshold ? 0 : (uchar)255);
}

__kernel void kernel_GrayDilate16(__global ushort* pausDataIn, __global ushort* pausDataOut, int iWidth, int iHeight, int iHSize)
{
	int i, j, offset;
	int x, y;
	
	x = get_global_id(0);
	y = get_global_id(1);

	offset = iHSize / 2;

	if ((y - offset <= 0) || (y + offset >= iHeight) || (x - offset <= 0) || (x + offset >= iWidth))
	{
		pausDataOut[y * iWidth + x] = pausDataIn[y * iWidth + x];
	}
	else
	{
		ushort usMax = 0;
		for (j = -offset; j <= offset; j++)
		{
			for (i = -offset; i <= offset; i++)
			{
				if (pausDataIn[(y + j)*iWidth + x + i] >= usMax)
				{
					usMax = pausDataIn[(y + j)*iWidth + x + i];
				}
			}
		}
		pausDataOut[y * iWidth + x] = usMax;
	}
}

__kernel void kernel_GrayDilate8(__global uchar* paucDataIn, __global uchar* paucDataOut, int iWidth, int iHeight, int iHSize)
{
	int i, j, offset;
	int x, y;
	
	x = get_global_id(0);
	y = get_global_id(1);

	offset = iHSize / 2;

	if ((y - offset <= 0) || (y + offset >= iHeight) || (x - offset <= 0) || (x + offset >= iWidth))
	{
		paucDataOut[y * iWidth + x] = paucDataIn[y * iWidth + x];
	}
	else
	{
		uchar ucMax = 0;
		for (j = -offset; j <= offset; j++)
		{
			for (i = -offset; i <= offset; i++)
			{
				if (paucDataIn[(y + j)*iWidth + x + i] >= ucMax)
				{
					ucMax = paucDataIn[(y + j)*iWidth + x + i];
				}
			}
		}
		paucDataOut[y * iWidth + x] = ucMax;
	}
}


__kernel void kernel_GrayErode16(__global ushort* pausDataIn, __global ushort* pausDataOut, int iWidth, int iHeight, int iHSize)
{
	int i, j, offset;
	int x, y;

	x = get_global_id(0);
	y = get_global_id(1);

	offset = iHSize / 2;

	if ((y - offset <= 0) || (y + offset >= iHeight) || (x - offset <= 0) || (x + offset >= iWidth))
	{
		//pausDataOut[y * iWidth + x] = pausDataIn[y * iWidth + x];
		pausDataOut[y * iWidth + x] = 0;
	}
	else
	{
		ushort usMin = 65535;
		for (j = -offset; j <= offset; j++)
		{
			for (i = -offset; i <= offset; i++)
			{
				if (pausDataIn[(y + j)*iWidth + x + i] <= usMin)
				{
					usMin = pausDataIn[(y + j)*iWidth + x + i];
				}
			}
		}
		pausDataOut[y * iWidth + x] = usMin;
	}
}

__kernel void kernel_MedianFilter16(__global ushort* pausDataIn, __global ushort* pausDataOut,
	int iWidth, int iHeight, int iHSize)
{
	int i, j, offset;
	int x, y;

	x = get_global_id(0);
	y = get_global_id(1);

	offset = iHSize / 2;

	if ((y - offset <= 0) || (y + offset >= iHeight) || (x - offset <= 0) || (x + offset >= iWidth))
	{
		pausDataOut[y * iWidth + x] = pausDataIn[y * iWidth + x];
	}
	else
	{
		ushort pnData[25];	
		int iSeq = 0;
		for (j = -offset; j <= offset; j++)
		{
			for (i = -offset; i <= offset; i++)
			{
				pnData[iSeq++] = pausDataIn[(y + j)*iWidth + x + i];
			}
		}
		ushort tmp = pnData[0];
		for (int i = 0; i < iHSize*iHSize-1; i++)
		{
			for (int j = 1; j < iHSize*iHSize; j++)
			{
				if (pnData[i] > pnData[j])
				{
					tmp = pnData[i];
					pnData[i] = pnData[j];
					pnData[j] = tmp;
				}
			}
		}
		pausDataOut[y * iWidth + x] = pnData[iHSize*iHSize/2];
	}
}

__kernel void kernel_SaltFilter8(__global uchar* paucDataIn, __global uchar* paucDataOut,
	int iWidth, int iHeight)
{
	int i, j, offset;
	int x, y;

	x = get_global_id(0);
	y = get_global_id(1);

	int iHSize = 3;
	offset = iHSize / 2;

	if ((y - offset <= 0) || (y + offset >= iHeight) || (x - offset <= 0) || (x + offset >= iWidth))
	{
		paucDataOut[y * iWidth + x] = paucDataIn[y * iWidth + x];
	}
	else
	{
		uchar temp = 0;
		for (j = -offset; j <= offset; j++)
		{
			for (i = -offset; i <= offset; i++)
			{
				temp += paucDataIn[(y + j)*iWidth + x + i] / 255;
			}
		}
		paucDataOut[y * iWidth + x] = temp >= 3 ? 255 : 0;
	}
}

__kernel void kernel_Binning16(__global ushort* pausDataIn, __global ushort* pausDataOut, int iWidth, int iHeight, int iBinning)
{
	int x = get_global_id(0);
	int y = get_global_id(1);
	int iBinWidth = iWidth / iBinning;
	int iBinHeight = iHeight / iBinning;

	double temp = 0;
	for (int i = 0; i < iBinning; i++)
	{
		for (int j = 0; j < iBinning; j++)
		{
			int x_ori = (iBinning * x + j);
			int y_ori = (iBinning * y + i);
			temp += pausDataIn[y_ori * iWidth + x_ori];
		}
	}
	pausDataOut[y * iBinWidth + x] = (ushort)(temp / (iBinning * iBinning));
}

__kernel void kernel_Frame5Median16(global ushort* src1, global ushort* src2, global ushort* src3, global ushort* src4, global ushort* src5, global ushort* out)
{
	size_t gid = get_global_id(0);

	ushort pnData[5];
	pnData[0] = src1[gid];
	pnData[1] = src2[gid];
	pnData[2] = src3[gid];
	pnData[3] = src4[gid];
	pnData[4] = src5[gid];
	ushort tmp = pnData[0];
	for (int i = 0; i < 4; i++) 
	{
		for (int j = 1; j < 5; j++) 
		{
			if (pnData[i] > pnData[j]) 
			{
				tmp = pnData[i];
				pnData[i] = pnData[j];
				pnData[j] = tmp;
			}
		}
	}
	out[gid] = pnData[2];
}

__kernel void kernel_Rotate16(__global ushort* pausDataIn, __global ushort* pausDataOut, __global float* pafDataOut,
        int iWidth, int iHeight, int iRotate, double dAvg, double dStd, int iNoSatuZone)
{
        int x = get_global_id(0);
        int y = get_global_id(1);

	ushort usTemp = pausDataIn[y * iWidth + x] > 16383 ? 16383 : pausDataIn[y * iWidth + x];

	if (iRotate == 1)
	{
	    pausDataOut[x * iHeight + (iHeight - 1 - y)] = usTemp;
	    pafDataOut[x * iHeight + (iHeight - 1 - y)] = (float)usTemp;	// 研制、订购1～5翻转
//	    pausDataOut[(iWidth - 1 - x) * iHeight + y] = usTemp;
//	    pafDataOut[(iWidth - 1 - x) * iHeight + y] = (float)usTemp;	// 订购6翻转
//	    pausDataOut[x * iHeight + y] = usTemp;
//	    pafDataOut[x * iHeight + y] = (float)usTemp;	// 订购7翻转
	}
	else
	{
	    pausDataOut[y * iWidth + x] = usTemp;
	    pafDataOut[y * iWidth + x] = (float)usTemp;
	}    
}


__kernel void kernel_RemoveBadLine(__global ushort* pausDataIn, __global ushort* pausDataOut,
        int iWidth, int iHeight)
{
	int x = get_global_id(0);
        int y = get_global_id(1);

	if (iWidth == 6144)
	{
		pausDataOut[y * iWidth + x] = pausDataIn[y * iWidth + x];

		/// 研制
		if (x > 5032-150 && x < 5032+150 && y > 3239-150 && y < 3239+150)	// 研制去亮斑
			pausDataOut[y * iWidth + x] = pausDataIn[(y - 300) * iWidth + x];

		/// 订购1

		/// 订购2

		/// 订购3
		/*if (x > 195 && x < 430 && y > 3475 && y < 3550)	// 订购3去灰虫
			pausDataOut[y * iWidth + x] = pausDataIn[(y - 80) * iWidth + x];
		int iH = 3619;
		if (y == iH - 1 || y == iH || y == iH + 1)	// 订购3去坏行
			pausDataOut[y * iWidth + x] = (ushort)(((float)(pausDataIn[(y - 3) * iWidth + x]) + (float)(pausDataIn[(y + 3) * iWidth + x])) / 2.0);*/

		/// 订购4

		/// 订购5

		/// 订购6
		/*if (y > 1299-75 && y < 1299+75)	// 订购6去亮条1
			pausDataOut[y * iWidth + x] = pausDataIn[(y + 160) * iWidth + x];
		if (y > 1751-50 && y < 1751+50)	// 订购6去亮条2
			pausDataOut[y * iWidth + x] = pausDataIn[(y + 150) * iWidth + x];*/

		/// 订购7
		/*if (y > 783-10 && y < 783+10)	// 订购7去亮条1
			pausDataOut[y * iWidth + x] = pausDataIn[(y + 30) * iWidth + x];
		if (y > 4567-10 && y < 4567+10)	// 订购7去亮条2
			pausDataOut[y * iWidth + x] = pausDataIn[(y + 30) * iWidth + x];
		if (x > 1533-10 && x < 1533+10 && y > 4806-10 && y < 4806+10)	// 订购7去亮斑
			pausDataOut[y * iWidth + x] = pausDataIn[(y - 30) * iWidth + x];*/
		
	}
	else if (iWidth == 1024)
	{
		/// 除订购6
		pausDataOut[y * iWidth + x] = y > 1019 ? pausDataIn[(1019 - (1024 - y)) * iWidth + x] : pausDataIn[y * iWidth + x]; 
				
		/// 订购6
		/*pausDataOut[y * iWidth + x] = y < 5 ? pausDataIn[(y + 6) * iWidth + x] : pausDataIn[y * iWidth + x]; */

		/// 研制

		/// 订购1

		/// 订购2

		/// 订购3

		/// 订购4

		/// 订购5

		/// 订购6
		/*if (x > 217-10 && x < 217+10)	// 订购6去亮条1
			pausDataOut[y * iWidth + x] = pausDataIn[y * iWidth + (x-30)];
		if (y > 918-2 && y < 918+2)	// 订购6去亮条2
			pausDataOut[y * iWidth + x] = pausDataIn[(y-10) * iWidth + x];
		if (y > 926-2 && y < 926+2)	// 订购6去亮条3
			pausDataOut[y * iWidth + x] = pausDataIn[(y-10) * iWidth + x];*/

		/// 订购7
		/*if (x > 925-10 && x < 925+10)	// 订购7去亮条3
			pausDataOut[y * iWidth + x] = pausDataIn[y * iWidth + (x-30)];*/
	} 
}

__kernel void kernel_FlatBiasCorrect(__global ushort* pausDataIn, __global ushort* pausDataOut, __global ushort* pausFlat, __global ushort* pausBias,
	double dFlatAvr, int iWidth, int iHeight)
{
	int x = get_global_id(0);
        int y = get_global_id(1);
	
	ushort usTemp = pausDataIn[y * iWidth + x] - pausBias[y * iWidth + x] > 0 ? pausDataIn[y * iWidth + x] - pausBias[y * iWidth + x] : 0;
	double dCoef = (double)pausFlat[y * iWidth + x] / dFlatAvr > 0.1 ? (double)pausFlat[y * iWidth + x] / dFlatAvr : 0.1;
	pausDataOut[y * iWidth + x] = (double)usTemp / dCoef < 65535 ? (ushort)((double)usTemp / dCoef) : 65535;
}

__kernel void kernel_LCM(__global ushort* pausDataIn, __global ushort* pausDataOut,
	int iHSize, int iWidth, int iHeight)
{
	int i, j, offset;
	int x, y;

	x = get_global_id(0);
	y = get_global_id(1);

	offset = iHSize / 2;

	double dTemp,dL0,dM;
	double dAvr[8];

	if (x < iHSize+offset || x > iWidth-iHSize-offset-1 || y < iHSize+offset || y > iHeight-iHSize-offset-1)
	{
		pausDataOut[y * iWidth + x] = pausDataIn[y * iWidth + x];
	}
	else
	{
		/// L0
		dTemp = 0;
		dL0 = 0;
		for (int i = y-offset; i <y+offset; i++)
		{
			for (int j = x-offset; j < x+offset; j++)
			{
				dTemp = dTemp < pausDataIn[i * iWidth + j] ? (double)pausDataIn[i * iWidth + j] : dTemp;
			}
		}
		dL0 = dTemp;
		/// M1 左上
		dTemp = 0;
		dAvr[0] = 0;
		for (int i = y-offset-iHSize; i <y+offset-iHSize; i++)
		{
			for (int j = x-offset-iHSize; j < x+offset-iHSize; j++)
			{
				dTemp += (double)pausDataIn[i * iWidth + j];
			}
		}
		dAvr[0] = dTemp / (iHSize*iHSize);
		/// M2 中上
		dTemp = 0;
		dAvr[1] = 0;
		for (int i = y-offset-iHSize; i <y+offset-iHSize; i++)
		{
			for (int j = x-offset; j < x+offset; j++)
			{
				dTemp += (double)pausDataIn[i * iWidth + j];
			}
		}
		dAvr[1] = dTemp / (iHSize*iHSize);
		/// M3 右上
		dTemp = 0;
		dAvr[2] = 0;
		for (int i = y-offset-iHSize; i <y+offset-iHSize; i++)
		{
			for (int j = x-offset+iHSize; j < x+offset+iHSize; j++)
			{
				dTemp += (double)pausDataIn[i * iWidth + j];
			}
		}
		dAvr[2] = dTemp / (iHSize*iHSize);
		/// M4 左中
		dTemp = 0;
		dAvr[3] = 0;
		for (int i = y-offset; i <y+offset; i++)
		{
			for (int j = x-offset-iHSize; j < x+offset-iHSize; j++)
			{
				dTemp += (double)pausDataIn[i * iWidth + j];
			}
		}
		dAvr[3] = dTemp / (iHSize*iHSize);
		/// M5 右中
		dTemp = 0;
		dAvr[4] = 0;
		for (int i = y-offset; i <y+offset; i++)
		{
			for (int j = x-offset+iHSize; j < x+offset+iHSize; j++)
			{
				dTemp += (double)pausDataIn[i * iWidth + j];
			}
		}
		dAvr[4] = dTemp / (iHSize*iHSize);
		/// M6 左下
		dTemp = 0;
		dAvr[5] = 0;
		for (int i = y-offset+iHSize; i <y+offset+iHSize; i++)
		{
			for (int j = x-offset-iHSize; j < x+offset-iHSize; j++)
			{
				dTemp += (double)pausDataIn[i * iWidth + j];
			}
		}
		dAvr[5] = dTemp / (iHSize*iHSize);
		/// M7 中下
		dTemp = 0;
		dAvr[6] = 0;
		for (int i = y-offset+iHSize; i <y+offset+iHSize; i++)
		{
			for (int j = x-offset; j < x+offset; j++)
			{
				dTemp += (double)pausDataIn[i * iWidth + j];
			}
		}
		dAvr[6] = dTemp / (iHSize*iHSize);
		/// M8 右下
		dTemp = 0;
		dAvr[7] = 0;
		for (int i = y-offset+iHSize; i <y+offset+iHSize; i++)
		{
			for (int j = x-offset+iHSize; j < x+offset+iHSize; j++)
			{
				dTemp += (double)pausDataIn[i * iWidth + j];
			}
		}
		dAvr[7] = dTemp / (iHSize*iHSize);

		/// dM
		dM = 1000000.0;
		for (int i = 0; i < 8; i++)
			dM = dM > dAvr[i] ? dAvr[i] : dM;

				
		dTemp = dL0*dL0/dM;
		dTemp = dTemp > 65535 ? 65535 : dTemp;
		dTemp = dTemp < 0 ? 0 : dTemp;

		pausDataOut[y * iWidth + x] = (unsigned short)dTemp;
	}
}

__kernel void kernel_ReduceLargeDN16(__global ushort* src, __global ushort* dst, float fThresh)
{
	int gid = (int)get_global_id(0);
	dst[gid] = src[gid] > fThresh ? (ushort)fThresh : src[gid];
}

__kernel void kernel_ReduceSmallDN16(__global ushort* src, __global ushort* dst, float fAvr, float fStd)
{
	int gid = (int)get_global_id(0);
	dst[gid] = src[gid] < fAvr-7.0*fStd ? (ushort)fAvr : src[gid];
}


__kernel void kernel_Patch16(__global ushort* src, __global ushort* dst, int r, int c)
{
	int x = get_global_id(0);
	int y = get_global_id(1);

	dst[y*1024+x] = src[(r*1024+y)*6144+c*1024+x];
}

__kernel void kernel_DePatch8(__global uchar* src, __global uchar* dst, int r, int c)
{
	int x = get_global_id(0);
	int y = get_global_id(1);

	dst[(r*1024+y)*6144+c*1024+x] = src[y*1024+x];
}


__kernel void kernel_DePatch16(__global ushort* src, __global ushort* dst, int r, int c)
{
	int x = get_global_id(0);
	int y = get_global_id(1);

	dst[(r*1024+y)*6144+c*1024+x] = src[y*1024+x];
}

__kernel void kernel_BWErode8(__global uchar* paucDataIn, __global uchar* paucDataOut, int iWidth, int iHeight, int iHSize)
{
	int i, j, offset;
	int x, y;

	x = get_global_id(0);
	y = get_global_id(1);

	offset = iHSize / 2;

	if ((y - offset <= 0) || (y + offset >= iHeight) || (x - offset <= 0) || (x + offset >= iWidth))
	{
		paucDataOut[y * iWidth + x] = 0;
	}
	else
	{
		bool bZero = false;
		for (j = -offset; j <= offset; j++)
		{
			for (i = -offset; i <= offset; i++)
			{
				if (paucDataIn[(y + j)*iWidth + x + i] == 0)
				{
					bZero = true;
				}
			}
		}
		paucDataOut[y * iWidth + x] = bZero ? 0 : 255;
	}
}























