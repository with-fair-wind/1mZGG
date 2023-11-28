#ifndef GETPERIOD_H
#define GETPERIOD_H
#include "mpolyfit.h"
#include <math.h>
#include <memory.h>
#include <iostream>
#include <string>
#include <sstream>
#include <sys/time.h>
#include <iomanip>
#include <algorithm>
#include <vector>
#include "mfft.h"
#include "photometry/myTypeDefine.h"


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


class GetPeriod
{
public:
    float* mfactors;            //拟合多项式的系数，从0阶开始，与数组下标一致
    int    mfactors_size;
    int    fitorders;           //多项式拟合的阶次
    float* myNoDC;              //去除直流分量后的y值
    int    myNoDC_size;

    float* myFit_x;             //拟合时使用的x轴数据
    float* myFit;               //拟合后的y值,这里为未去除直流分量的数值
    int    myFit_size;
    float* ampfreq;             //幅频信息
    float* phafreq;             //相频信息
    int    freq_size;           //频谱数据尺寸
    float  fs0;                 //基频,单位为x数据单位的倒数
    float  time0;               //时间采样间隔，单位与数据x单位一致
    float* mperiod;             //提取到的周期
    int    mperiod_size = 0;    //提出到的周期的个数
public:
    GetPeriod()
    {
        this->mfactors = NULL;
        this->myNoDC = NULL;
        this->myFit_x = NULL;
        this->myFit = NULL;
        this->ampfreq = NULL;
        this->phafreq = NULL;
        this->mperiod = NULL;

        this->fitorders = 6;
        this->fs0 = -1;
        this->time0 = -1;

    };

    virtual ~GetPeriod()
    {
        DeletePtrArray(mfactors);
        DeletePtrArray(myNoDC);
        DeletePtrArray(myFit_x);
        DeletePtrArray(myFit);
        DeletePtrArray(ampfreq);
        DeletePtrArray(phafreq);
        DeletePtrArray(mperiod);
    };
public:

    void remDCpart(float x[], float y[], const int num, float y_noDC[]);
    void remDCpart(float x[], float y[], const int num);
    //输入x和y数据序列及其数据尺寸，要求x和y尺寸一样
    //返回周期值period和基频信息，周期单位与x数据的单位一致
    //flag默认为0，表示先做拟合，去除直流或缓变信息，拟合阶次及其系数可以使用相应方法获取
    //flag=1表示使用原始数据
    void    getperiod(float* x, float* y, const int x_size, float* &period, int &period_size, float &f_base, int flag = 0);
    void    getperiod(double* x, double* y, const int x_size, float* &period, int &period_size, float &f_base, int flag = 0);
    bool    getMinSampPeriod(float* data_x,long data_num,float err_limit,float &minSampPeriod,float &maxErr);
    bool    SampPeriodErr(float* samplist, long samp_num,float sampPeriod,float &sampPeriodErr);
    void    out2file(std::string fpn);
    void    out2file();
    bool    LoadGDJFile(std::vector<double> &vec_mjd,std::vector<double> &vec_mag,std::string fpn);
    float*  getFactors(){return this->mfactors;};
    int     getFactorsSize(){return this->mfactors_size;};
    float*  getYNoDC(){return this->myNoDC;};
    int     getyNoDCSize(){return this->myNoDC_size;};
    float*  getyFit(){return this->myFit;};
    int     getyFitSize(){return this->myFit_size;};
    int     getFitOders(){return this->fitorders;};
    void    setFItOders(int oder){this->fitorders = oder;};


};

#endif // GETPERIOD_H
