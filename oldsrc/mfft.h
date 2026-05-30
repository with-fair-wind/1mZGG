#ifndef MFFT_H
#define MFFT_H

#include <iostream>
#include <memory.h>
#include <math.h>
#include <string>
#include <fstream>
#include "mpolyfit.h"

#define      MAX_MATRIX_SIZE                   4194304             // 2048 * 2048
#define      PI                     3.14159265358979323846264338327950288
#define      MAX_VECTOR_LENGTH              10000             //

/* Pi */
#define DPI (3.141592653589793238462643)

/* 2Pi */
#define D2PI (6.283185307179586476925287)

/* Radians to degrees */
#define DR2D (57.29577951308232087679815)

/* Degrees to radians */
#define DD2R (1.745329251994329576923691e-2)

/* Radians to arcseconds */
#define DR2AS (206264.8062470963551564734)

/* Arcseconds to radians */
#define DAS2R (4.848136811095359935899141e-6)

/* Seconds of time to radians */
#define DS2R (7.272205216643039903848712e-5)

/* Arcseconds in a full circle */
#define TURNAS (1296000.0)

/* Milliarcseconds to radians */
#define DMAS2R (DAS2R / 1e3)

/* Length of tropical year B1900 (days) */
#define DTY (365.242198781)

/* Seconds per day. */
#define DAYSEC (86400.0)

//简化指针删除命令
#ifndef DeletePtr
#define DeletePtr(a) {\
    if (a!=NULL)\
        delete a;\
        a = NULL;\
    }
#endif

#ifndef DeletePtrArray
#define DeletePtrArray(a) {\
    if (a!=NULL)\
        delete[] a;\
        a = NULL;\
    }
#endif

#define OutData2Txt(fpn,data,data_num){             \
    std::ofstream    outfile(fpn,std::ios::out);    \
    if(!outfile)                                    \
        {                                           \
            outfile.close();                        \
        }                                           \
    else {                                          \
            for(int i=0;i<data_num;i++)             \
            outfile << data[i] << "\n";             \
            outfile.close();                        \
        }                                           \
    }
typedef struct Complex
{
    double rl;
    double im;
}Complex;



class mFFT
{
public:
    mFFT(void);
    ~mFFT(void);

public:
    bool is_power_of_two(int num);
    bool is_power_of_two(int num, int &byte_num);
    //flag=0为FFT,flag=1为IFFT
    void FFT_process (float xreal[], float ximag[], const int num, float &fs0, int flag = 0);
    void FFT_process(float xreal[], const int num, float &fs0, int flag =0);
    void FFT_process (float xreal[], const int num, const int flag = 0);
    void Interp(float xdata[], float ydata[],const int num, float xdata_inter[], float ydata_out[], const int index);
    bool SampInterp(float* x,float* y,const long num_in,float* x_interp,float* y_interp,const long num_interp);
    long FindFirstElement(float* data,const long num,float threshold,int condition=1,long start_index=0,long endindex=-1);

    void getLarInterpOne(float xdata[], float ydata[],const int num, float x_inter, float &y_inter);
    float getLarInterpOne(float xdata[], float ydata[],const int num, float x_inter);
/////////////////////////////////////


    ////////////////////////////////////

    //基频，单位与数据采样时间单位的倒数,1/(（采样点数-1）*采样时间间隔)
    float   fs0 = 0;      //基频
    float   time0 = 0;    //采样时间间隔
    float*  m_data_x;       //时间序列
    float*  m_data_y;       //采集数值
    int     m_data_counter; //采集数据的个数
    float*  xd_inter = NULL;      //插值时间序列，根据请求的时间序列，获取对应的插值函数值
    float*  yd_inter = NULL;      //插值函数值
    int     xd_inter_size;//插值总点数
    float*  data_FFT_real = NULL;//傅里叶编号后的实部
    float*  data_FFT_img = NULL;//傅里叶变换后的虚部
    int     data_FFT_counter;//傅里叶变换后的数据个数

    float* data_ampfreq = NULL;//幅频特性的幅值
    float* data_phafreq = NULL;//相频特性的相位

    int    data_ampfreq_counter = 0;//频谱数据个数
    float  data_amp_A    = 1;    //最大幅值

private:
    bool fft(const Complex inVec[], const int len, Complex outVec[]);            // 基于蝶形算法的快速傅里叶变换
    bool ifft(const Complex inVec[], const int len, Complex outVec[]);
    int  get_computation_layers(int num);         // calculate the layers of computation needed for FFT

};



#endif // MFFT_H
