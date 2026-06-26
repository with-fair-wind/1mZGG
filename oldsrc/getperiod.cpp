#include "getperiod.h"
#include <iostream>
#include <algorithm>
#include <iterator>
#include <functional>
//GetPeriod::GetPeriod()
//{


//}

void GetPeriod::remDCpart(float x[], float y[], const int num, float y_noDC[])
{
    //int fitoders = 6;//设置拟合的阶次
    //复制输入参数x和y
    float* x_temp = new float[num*sizeof(float)];
    float* y_temp = new float[num*sizeof(float)];
    memcpy(x_temp,x,num*sizeof(float));
    memcpy(y_temp,y,num*sizeof(float));

    size_t m_size = num;
    datafit::mpolyfit dtfit;
//    std::vector<double> m_x,m_y;
//    m_x.resize(m_size);
//    m_y.resize(m_size);
//    for(size_t i =0;i<m_size;++i)
//    {
//        m_x[i] = x[i];
//        m_y[i] = y[i];
//    }
    //dtfit.polyfit(m_x,m_y,6,true);
    std::vector<double> x_temp_v;
    std::vector<double> y_temp_v;
    x_temp_v.resize(m_size);
    y_temp_v.resize(m_size);
    size_t i = 0;
    for(i=0; i<m_size;i++)
    {
        x_temp_v[i] = x_temp[i];
        y_temp_v[i] = y_temp[i];
    }
    dtfit.polyfit(x_temp_v,y_temp_v,this->fitorders,true);


    //dtfit.polyfit(x_temp,y_temp,m_size,this->fitorders,true);
    size_t fcsize = dtfit.getFactorSize();
    this->mfactors_size = fcsize;
    //factors_size = fcsize;
    DeletePtrArray(this->mfactors);
    this->mfactors = new float[fcsize*sizeof (float)];
    for(int i =0 ;i<(int)fcsize;i++)
    {
        this->mfactors[i] = dtfit.getFactor(i);
    }

    //memcpy(this->mfactors, factors,fcsize*sizeof (float));
    std::vector<double> ypoly;
    dtfit.getFitedYs(ypoly);
    DeletePtrArray(myFit);
    this->myFit = new float[num*sizeof(float)];
    DeletePtrArray(myNoDC);
    this->myNoDC = new float[num*sizeof(float)];

    for(int i = 0; i<num;i++)
    {
        this->myFit[i] = ypoly[i];
        this->myNoDC[i] = y_temp[i]-ypoly[i];
        y_noDC[i] = y_temp[i]-ypoly[i];
    }
    this->myFit_size = num;
    this->myNoDC_size = num;
    DeletePtrArray(x_temp);
    DeletePtrArray(y_temp);
}

void GetPeriod::remDCpart(float x[], float y[], const int num)
{
    float* y_NoDC_temp = new float[num*sizeof (float)];
    this->remDCpart(x, y, num, y_NoDC_temp);
    DeletePtrArray(y_NoDC_temp);
}
bool GetPeriod::SampPeriodErr(float* samplist, long samp_num,float sampPeriod,float &sampPeriodErr)
{
    if(samplist==NULL||samp_num==0)return false;
    std::vector<float> samperr_vec;
    samperr_vec.resize(samp_num);
    double var_samperr=0;
    float* sampPeriodErr_ptr;
    float sampPeriodErr_limit = fabs(sampPeriod/2.0);
    for(int i=0;i<samp_num;++i)
    {
        samperr_vec[i] = fmod(fabs(samplist[i]),sampPeriod);
        if(samperr_vec[i]>sampPeriodErr_limit)
            samperr_vec[i] -= sampPeriodErr_limit;
        else if(samperr_vec[i]<(-sampPeriodErr_limit))
            samperr_vec[i] += sampPeriodErr_limit;
        var_samperr += samperr_vec[i];
    }
    var_samperr /= samp_num;
    sampPeriodErr_ptr = std::max_element(&samperr_vec[0],&samperr_vec[0]+samp_num);
    if(samp_num>2)
    {
        if(*sampPeriodErr_ptr>3*var_samperr)
        {
            for(int j=0;j<samp_num;++j)
            {
                if(samperr_vec[j]>sampPeriodErr_limit) samperr_vec[j]=0;
            }
        }
        sampPeriodErr = *std::max_element(&samperr_vec[0],&samperr_vec[0]+samp_num);
    }
    else
    {
        sampPeriodErr = *sampPeriodErr_ptr;
    }
    return true;
}

bool GetPeriod::getMinSampPeriod(float* data_x,long data_num,float err_limit_rate,float &minSampPeriod,float &maxErr)
{
    if(data_x==NULL||data_num==0)return false;
    std::vector<float> delta_vec;
    if(data_num<2)
    {
        delta_vec.resize(1);
        delta_vec[0] = 0;
    }
    else
    {
        delta_vec.resize(data_num-1);
        for(int i=0;i<data_num-1;++i)
        {
            delta_vec[i] = data_x[i+1]-data_x[i];
        }
    }
    float delta_min;
    delta_min = *std::min_element(&delta_vec[0],&delta_vec[0]+delta_vec.size());
    if(delta_min <= 0)return false;
    float sampPeriod = delta_min;
    int samPeriod_search_Coef = 1;
    float m_sampPeriod_Err;
    bool err_flag = false;
    for(samPeriod_search_Coef=0;samPeriod_search_Coef<5;++samPeriod_search_Coef)
    {
        sampPeriod = delta_min*pow(2,-samPeriod_search_Coef);
        err_flag = this->SampPeriodErr(data_x,data_num,sampPeriod,m_sampPeriod_Err);
        if(err_flag==false)return false;
        if(m_sampPeriod_Err<err_limit_rate*delta_min)break;
    }
    if(samPeriod_search_Coef==5)return false;
    minSampPeriod = sampPeriod;
    maxErr = m_sampPeriod_Err;
    return true;
}
void GetPeriod::getperiod(double* x, double* y, const int x_size, float* &period, int &period_size, float &f_base, int flag)

{
    if(x==NULL||y==NULL||x_size<2)
    {
        period=NULL;
        period_size = 0;
        f_base = -1;
        return;
    }
    std::vector<float> m_x,m_y;
    m_x.resize(x_size);
    m_y.resize(x_size);
    for(long i=0;i<x_size;++i)
    {
        m_x[i] = x[i]-x[0];
        m_y[i] = y[i];
    }
    getperiod(&m_x[0],&m_y[0],x_size,period,period_size,f_base,flag);
}


void GetPeriod::getperiod(float* x, float* y, const int x_size, float* &period, int &period_size, float &f_base, int flag)
{
    bool err_flag = false;
    //复制输入参数x和y
    float* x_temp = new float[x_size*sizeof(float)];
    float* y_temp = new float[x_size*sizeof(float)];
    memcpy(x_temp,x,x_size*sizeof(float));
    memcpy(y_temp,y,x_size*sizeof(float));


///////////
    //按时间数据最大最小的范围作为基本范围，按2倍原数据量进行插值
    int   sample_rate =1;
    float time_range = x_temp[x_size-1]-x[0];
    float sampe_err_limit_rate = 0.2;
    float delta_time = 1;
    float samp_maxerr=0;
    err_flag = this->getMinSampPeriod(x,x_size,sampe_err_limit_rate,delta_time,samp_maxerr);
    if(err_flag==false)return;
    //设置插值后的数据时间
    int x_inter_size = time_range/delta_time+1;
    float* x_inter = new float[x_inter_size*sizeof(float)];
    float* y_inter = new float[x_inter_size*sizeof(float)];

    memset(x_inter,0,x_inter_size*sizeof(float));
    memset(y_inter,0,x_inter_size*sizeof(float));


    for (int i=0;i<x_inter_size;i++)
    {
        x_inter[i] = x_temp[0]+delta_time*i;
    }

    ///输出监测x_inter
    //OutData2Txt("./x_inter.txt",x_inter,x_inter_size);
    ///
    ///
    DeletePtrArray(myFit_x);
    this->myFit_x = new float[x_inter_size*sizeof(float)];
    memcpy(this->myFit_x,x_inter,x_inter_size*sizeof(float));
    mFFT* fftProcess = new mFFT;

    //根据标识量选择是否去除直流分量
    //不做插值
    if (flag == 0)
    {
        float* y_noDC_temp = new float[x_size*sizeof(float)];
        memset(y_noDC_temp,0,x_size*sizeof(float));
        this->remDCpart(x_temp,y_temp,x_size,y_noDC_temp);
        memcpy(y_temp,y_noDC_temp,x_size*sizeof(float));
        DeletePtrArray(y_noDC_temp);
    }
    //求插值后的y值
    fftProcess->SampInterp(x_temp,y_temp,x_size,x_inter,y_inter,x_inter_size);
    //fftProcess->Interp(x_temp,y_temp,x_size,x_inter,y_inter,x_inter_size);

    //输出y_inter，监测
    //OutData2Txt("./y_inter.txt",y_inter,x_inter_size);

    //FFT处理，处理后的结果可以用mFFT类的对应变量取出来
    //fftProcess->FFT_process(y_temp,x_inter_size,0);
    fftProcess->FFT_process(y_inter,x_inter_size,0);
    //执行过插值函数后才能获得正确的采样时间间隔
    this->time0 = fftProcess->time0;
    this->fs0 = fftProcess->fs0;
    //this->fs0 = fftProcess->fs0*2;//FFT处理时做了截断，这里进行调整，后续统一FFT和这里的逻辑
    //只取一半频谱信息，因为是对称重复的，只要前半段即可
    //int freq_size_temp = fftProcess->data_ampfreq_counter/2;
    int freq_size_temp = fftProcess->data_ampfreq_counter;
    //int freq_size_temp = fftProcess->data_ampfreq_counter;//FFT程序已经自动取了一半
    this->freq_size = freq_size_temp;
    DeletePtrArray(this->ampfreq);
    this->ampfreq = new float[freq_size_temp*sizeof (float)];
    memset(this->ampfreq,0,freq_size_temp*sizeof (float));
    memcpy(this->ampfreq,fftProcess->data_ampfreq,freq_size_temp*sizeof (float));

    //this->phafreq = NULL;
    DeletePtrArray(this->phafreq);
    this->phafreq = new float[freq_size_temp*sizeof (float)];
    memcpy(this->phafreq,fftProcess->data_phafreq,freq_size_temp*sizeof (float));
    //拟合频谱参数

    //搜索频谱最大值,后续增加依据标准差判断等
    float amp_max_temp = 0;
    int   index_amp_max_temp = 0;
    int   count_period = 0;         //后续增加多周期计算
    float amp_sum = 0;
    float var_amp = 0;
    float std_amp = 0;
    for(int i=0;i<freq_size_temp;i++)
    {
        if(this->ampfreq[i] > amp_max_temp)
        {
            amp_max_temp = this->ampfreq[i];
            index_amp_max_temp = i;
        }
        amp_sum += this->ampfreq[i];
    }
    var_amp = amp_sum/freq_size_temp;
    for(int i = 0; i<freq_size_temp;i++)
    {
        std_amp += pow(this->ampfreq[i]-var_amp,2);
    }
    std_amp = sqrt(std_amp/(freq_size_temp-1));

    //计算周期
    //count_period = 1;
    DeletePtrArray(this->mperiod);
    if(amp_max_temp < 1.5*std_amp)
    {
        count_period = 1;//无明显周期
        this->mperiod = new float[1*sizeof (float)];
        this->mperiod[0] = index_amp_max_temp;
        this->mperiod_size = 1;
    }
    else
    {
        count_period = 1;
        this->mperiod = new float[count_period*sizeof (float)];
        this->mperiod[0] = index_amp_max_temp;
        this->mperiod_size = 1;
    }
    period = this->mperiod;
    period_size = this->mperiod_size;
    f_base = this->fs0;




    ////////////////////////
    //3倍标准差为判据


    //找到

    DeletePtrArray(x_temp);
    DeletePtrArray(y_temp);
    DeletePtrArray(x_inter);
    DeletePtrArray(y_inter);
    DeletePtr(fftProcess);
}
bool GetPeriod::LoadGDJFile(std::vector<double> &vec_mjd,std::vector<double> &vec_mag,std::string fpn)
{
    bool err_flag=false;
    //获得打开数据源文件
    char str_line[256];
    std::string str="";
    std::vector<std::string> vec_str;
    DateTime mdtime;
    DateTime dtime_temp;
    FILE *file=NULL;
    file = fopen(fpn.c_str(),"r");
    if (file == NULL) return false;
    rewind(file);//文件指针回到文件开始位置
    for(int i=0;i<11;++i)
    {
        if(fgets(str_line, 256, file) == NULL)
        {
            err_flag=true;
            break;
        }
    }
    if(err_flag == true)
    {
        if(file!=NULL)fclose(file),file=NULL;
        return false;
    }
    char split_char;
    split_char = ' ';
    long long_dtime;
    double m_mag;
    double m_mjd;
    vec_mjd.resize(0);
    vec_mag.resize(0);
    while (fgets(str_line, 256, file) != NULL)
    {
        str = str_line;
        str.erase(0,str.find_first_not_of(" "));
        str.erase(str.find_last_not_of(" ")+1);
        vec_str = dtime_temp.stringSplit(str,split_char);
        if(vec_str.size()<17)
        {
            err_flag=true;
            break;
        }
        long_dtime = stol(vec_str[6]);
        mdtime.year = long_dtime/10000;
        mdtime.month = (long_dtime-mdtime.year*10000)/100;
        mdtime.day = long_dtime%100;
        long_dtime = stol(vec_str[7]);
        mdtime.hour = long_dtime/1e10;
        mdtime.minute = (long_dtime-mdtime.hour*1e10)/1e8;
        mdtime.second = (long_dtime-mdtime.hour*1e10-mdtime.minute*1e8)/1e6;
        mdtime.millisecond = (long_dtime%1000000)/1000;
        m_mjd = mdtime.getMJD();
        vec_mjd.push_back(m_mjd);
        m_mag = (double)stoi(vec_str[10])/100.0;
        vec_mag.push_back(m_mag);
    }
    if(err_flag == true)
    {
        if(file!=NULL)fclose(file),file=NULL;
        return false;
    }
    if(file!=NULL)fclose(file),file=NULL;
    return true;
}
