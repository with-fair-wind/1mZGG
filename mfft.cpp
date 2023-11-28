#include "mfft.h"





mFFT::mFFT()
{

    fs0             =       0;      //基频
    time0           =       0;    //采样时间间隔
    m_data_x        =       NULL;       //时间序列
    m_data_y        =       NULL;       //采集数值
    m_data_counter  =       0; //采集数据的个数
    xd_inter        =       NULL;      //插值时间序列，根据请求的时间序列，获取对应的插值函数值
    yd_inter        =       NULL;      //插值函数值
    xd_inter_size   =       0;//插值总点数
    data_FFT_real   =       NULL;//傅里叶编号后的实部
    data_FFT_img    =       NULL;//傅里叶变换后的虚部
    data_FFT_counter=       0;//傅里叶变换后的数据个数

    data_ampfreq    =       NULL;//幅频特性的幅值
    data_phafreq    =       NULL;//相频特性的相位

    data_ampfreq_counter =  0;//频谱数据个数
    data_amp_A      =       1;    //最大幅值
}


mFFT::~mFFT()
{
    DeletePtrArray(m_data_x);
    DeletePtrArray(m_data_y);
    DeletePtrArray(xd_inter);
    DeletePtrArray(yd_inter);
    DeletePtrArray(data_FFT_real);
    DeletePtrArray(data_FFT_img);
    DeletePtrArray(data_ampfreq);
    DeletePtrArray(data_phafreq);

}


bool mFFT::is_power_of_two(int num)
{
    int byte_num;
    return this->is_power_of_two(num,byte_num);
}

bool mFFT::is_power_of_two(int num, int &byte_num)
{
    if (num<1) {
        byte_num = 0;
        return false;}
    if (num == 1)
    {
        byte_num = 1;
        return false;
    }
    bool outflag = true;
    int temp = num;
    int mod = 1;
    byte_num = 1;
    while (temp > 0)
    {
        if(temp<2)break;
        mod = temp%2;
        if (mod) outflag = false;
        temp = floor(temp/2);
        if(temp)byte_num++;
    }

    return outflag;
}

int mFFT::get_computation_layers(int num)
{
    int nLayers = 0;
    int len = num;
    if (len == 2)
        return 1;
    while (true) {
        len = len / 2;
        nLayers++;
        if (len == 2)
            return nLayers + 1;
        if (len < 1)
            return -1;
    }
}


// 1纬向量傅里叶变换
// 要进行傅里叶变换的输入向量
// 输入向量的尺寸，即数据长度
// 傅里叶变换结果，即输出向量
bool mFFT::fft(const Complex inVec[], const int vecLen, Complex outVec[])
{
    if ((vecLen <= 0) || (NULL == inVec) || (NULL == outVec))
        return false;
    if (!is_power_of_two(vecLen))
        return false;

    // 权重数组
    Complex         *pVec = new Complex[vecLen];
    Complex         *Weights = new Complex[vecLen];
    Complex         *X = new Complex[vecLen];
    int             *pnInvBits = new int[vecLen];

    memcpy(pVec, inVec, vecLen*sizeof(Complex));

    // 计算权重序列
    double fixed_factor = (-2 * PI) / vecLen;
    for (int i = 0; i < vecLen / 2; i++) {
        double angle = i * fixed_factor;
        Weights[i].rl = cos(angle);
        Weights[i].im = sin(angle);
    }
    for (int i = vecLen / 2; i < vecLen; i++) {
        Weights[i].rl = -(Weights[i - vecLen / 2].rl);
        Weights[i].im = -(Weights[i - vecLen / 2].im);
    }

    int r = get_computation_layers(vecLen);

    // 计算倒序位码
    int index = 0;
    for (int i = 0; i < vecLen; i++) {
        index = 0;
        for (int m = r - 1; m >= 0; m--) {
            index += (1 && (i & (1 << m))) << (r - m - 1);
        }
        pnInvBits[i] = index;
        X[i].rl = pVec[pnInvBits[i]].rl;
        X[i].im = pVec[pnInvBits[i]].im;
    }

    // 计算快速傅里叶变换
    for (int L = 1; L <= r; L++) {
        int distance = 1 << (L - 1);
        int W = 1 << (r - L);

        int B = vecLen >> L;
        int N = vecLen / B;

        for (int b = 0; b < B; b++) {
            int mid = b*N;
            for (int n = 0; n < N / 2; n++) {
                int index = n + mid;
                int dist = index + distance;
                pVec[index].rl = X[index].rl + (Weights[n*W].rl * X[dist].rl - Weights[n*W].im * X[dist].im);                      // Fe + W*Fo
                pVec[index].im = X[index].im + Weights[n*W].im * X[dist].rl + Weights[n*W].rl * X[dist].im;
            }
            for (int n = N / 2; n < N; n++) {
                int index = n + mid;
                pVec[index].rl = X[index - distance].rl + Weights[n*W].rl * X[index].rl - Weights[n*W].im * X[index].im;        // Fe - W*Fo
                pVec[index].im = X[index - distance].im + Weights[n*W].im * X[index].rl + Weights[n*W].rl * X[index].im;
            }
        }

        memcpy(X, pVec, vecLen*sizeof(Complex));
    }

    memcpy(outVec, pVec, vecLen*sizeof(Complex));
    if (Weights)      delete[] Weights;
    if (X)                 delete[] X;
    if (pnInvBits)    delete[] pnInvBits;
    if (pVec)           delete[] pVec;
    return true;
}

//傅里叶逆变换
bool mFFT::ifft(const Complex inVec[], const int len, Complex outVec[])
{
    if ((len <= 0) || (NULL == inVec) || (NULL == outVec))
        return false;
    if (false == is_power_of_two(len)) {
        return false;
    }

    double         *W_rl = new double[len];
    double         *W_im = new double[len];
    double         *X_rl = new double[len];
    double         *X_im = new double[len];
    double         *X2_rl = new double[len];
    double         *X2_im = new double[len];

    double fixed_factor = (-2 * PI) / len;
    for (int i = 0; i < len / 2; i++) {
        double angle = i * fixed_factor;
        W_rl[i] = cos(angle);
        W_im[i] = sin(angle);
    }
    for (int i = len / 2; i < len; i++) {
        W_rl[i] = -(W_rl[i - len / 2]);
        W_im[i] = -(W_im[i - len / 2]);
    }

    // 时域数据写入X1
    for (int i = 0; i < len; i++) {
        X_rl[i] = inVec[i].rl;
        X_im[i] = inVec[i].im;
    }
    memset(X2_rl, 0, sizeof(double)*len);
    memset(X2_im, 0, sizeof(double)*len);

    int r = get_computation_layers(len);
    if (-1 == r)
        return false;
    for (int L = r; L >= 1; L--) {
        int distance = 1 << (L - 1);
        int W = 1 << (r - L);

        int B = len >> L;
        int N = len / B;


        for (int b = 0; b < B; b++) {
            for (int n = 0; n < N / 2; n++) {
                int index = n + b*N;
                X2_rl[index] = (X_rl[index] + X_rl[index + distance]) / 2;
                X2_im[index] = (X_im[index] + X_im[index + distance]) / 2;
            }
            for (int n = N / 2; n < N; n++) {
                int index = n + b*N;
                X2_rl[index] = (X_rl[index] - X_rl[index - distance]) / 2;           // 需要再除以W_rl[n*W]
                X2_im[index] = (X_im[index] - X_im[index - distance]) / 2;
                double square = W_rl[n*W] * W_rl[n*W] + W_im[n*W] * W_im[n*W];          // c^2+d^2
                double part1 = X2_rl[index] * W_rl[n*W] + X2_im[index] * W_im[n*W];         // a*c+b*d
                double part2 = X2_im[index] * W_rl[n*W] - X2_rl[index] * W_im[n*W];          // b*c-a*d
                if (square > 0) {
                    X2_rl[index] = part1 / square;
                    X2_im[index] = part2 / square;
                }
            }
        }
        memcpy(X_rl, X2_rl, sizeof(double)*len);
        memcpy(X_im, X2_im, sizeof(double)*len);
    }

    // 位码倒序
    int index = 0;
    for (int i = 0; i < len; i++) {
        index = 0;
        for (int m = r - 1; m >= 0; m--) {
            index += (1 && (i & (1 << m))) << (r - m - 1);
        }
        outVec[i].rl = X_rl[index];
        outVec[i].im = X_im[index];
    }

    if (W_rl)      delete[] W_rl;
    if (W_im)    delete[] W_im;
    if (X_rl)      delete[] X_rl;
    if (X_im)     delete[] X_im;
    if (X2_rl)     delete[] X2_rl;
    if (X2_im)    delete[] X2_im;

    return true;
}

void mFFT::FFT_process(float xreal[], float ximag[], const int num, float &fs0, int flag)
{
    DeletePtr(this->data_FFT_img);
    DeletePtr(this->data_FFT_real);
    this->data_FFT_real = new float[num*sizeof(float)];
    this->data_FFT_img = new float[num*sizeof(float)];
    memset(this->data_FFT_img,0,num*sizeof (float));
    memset(this->data_FFT_real,0,num*sizeof (float));
    memcpy(this->data_FFT_real,xreal,num*sizeof (float));
    memcpy(this->data_FFT_img,ximag,num*sizeof (float));
    //基频，单位与数据采样时间单位的倒数,1/(（采样点数-1）*采样时间间隔)
    fs0 = -1;
    int byteNum = 0;
    int N_FFT = 0;
    int index = num;
    float *FFT_real = NULL;
    float *FFT_img = NULL;
    if(!this->is_power_of_two(num,byteNum))
    {
        if(byteNum > 16)
        {
            N_FFT = 16;
            index = pow(2,N_FFT);
            for(int i=index;i<num;i++)
            {
                this->data_FFT_real[i] = 0;
                this->data_FFT_img[i] = 0;
            }
            memcpy(FFT_real,xreal,index*sizeof (float));//只处理前2^16个数据,有隐患，后续改进
            memcpy(FFT_img,ximag,index*sizeof (float));
        }
        else if(byteNum < 0)
        {
            memset(this->data_FFT_real,-1,num*sizeof (float));
            memset(this->data_FFT_img,-1,num*sizeof (float));
            return;
        }
        else
        {
            N_FFT = byteNum;
            index = pow(2,N_FFT);
            FFT_real = new float[index*sizeof (float)];
            FFT_img = new float[index*sizeof (float)];
//            memset(FFT_real,0,index*sizeof (float));
//            memset(FFT_img,0,index*sizeof (float));
            memcpy(FFT_real,this->data_FFT_real,num*sizeof (float));
            memcpy(FFT_img,this->data_FFT_img,num*sizeof (float));
        }
    }



    Complex* data_in = new Complex[index*sizeof (Complex)];
    Complex* data_out = new Complex[index*sizeof (Complex)];
    memset(data_in, 0, index*sizeof(Complex));//输入数据置零，只有实数输入时，只再次赋值实部即可
    memset(data_out, 0, index*sizeof(Complex));
      {
        for (int i = 0; i< index; i++)
         {
             data_in[i].rl = FFT_real[i];
             data_in[i].im = FFT_img[i];
          }
      }


    if(flag == 0)
    {
        this->fft(data_in, index, data_out);
    }
    else
    {
        this->ifft(data_in,index,data_out);
    }
    //初始化幅频相频数组
    //int ampfreq_size = index/2;
    int ampfreq_size = index;
    this->data_ampfreq_counter = ampfreq_size;
    DeletePtr(this->data_ampfreq);
    this->data_ampfreq = new float[ampfreq_size*sizeof (float)];
    memset(this->data_ampfreq,0,ampfreq_size*sizeof(float));
    DeletePtr(this->data_phafreq);
    this->data_phafreq = new float[ampfreq_size*sizeof (float)];
    memset(this->data_phafreq,0,ampfreq_size*sizeof(float));
    //后续修改为独立控制变量，以便有时需要单独提供完整FFT数据，但是只取一半幅频数据
    this->data_FFT_counter = ampfreq_size;


    float FFT_amp_max = 0;
      for (int i = 0; i< ampfreq_size; i++)
       {
           this->data_FFT_real[i] = data_out[i].rl;

           this->data_FFT_img[i] = data_out[i].im;
           this->data_ampfreq[i] = sqrt(this->data_FFT_real[i]*this->data_FFT_real[i]+
                                        this->data_FFT_img[i]*this->data_FFT_img[i]);
          if(i == 0) this->data_ampfreq[0] /= index;
          else this->data_ampfreq[i] /= (index/2);
          if(fabs(this->data_ampfreq[i])>FFT_amp_max) FFT_amp_max = this->data_ampfreq[i];
          this->data_phafreq[i] = atan2(this->data_FFT_img[i],this->data_FFT_real[i])*DR2D;
        }
      this->data_amp_A = FFT_amp_max;

      ///输出数据，测试
      //OutData2Txt("./amp.txt",this->data_ampfreq,ampfreq_size);//输出的是全频率


      //幅频特性归一化
//      if(FFT_amp_max != 0)
//      {
//          for(int i = 0; i<num;i++)
//          {
//              this->data_ampfreq[i] /= FFT_amp_max;
//          }
//      }


    fs0 = 1/((index-1)*this->time0);
    this->fs0 = fs0;

    DeletePtrArray(data_in);
    DeletePtrArray(data_out);
    DeletePtrArray(FFT_real);
    DeletePtrArray(FFT_img);

}


void mFFT::FFT_process(float xreal[], const int num, float &fs0, int flag)
{

    float* ximg;
    ximg = new float[num*sizeof(float)];
    memset(ximg,0,num*sizeof (float));

    this->FFT_process(xreal,ximg,num,fs0,flag);

}

void mFFT::FFT_process (float xreal[], const int num, const int flag)
{

     this->FFT_process(xreal,num,this->fs0,flag);
}
bool mFFT::SampInterp(float* x,float* y,const long num_ini,float* x_interp,float* y_interp,const long num_interp)
{
    if(x==NULL||y==NULL||num_ini<1||x_interp==NULL||y_interp==NULL||num_interp<1)return false;
    if(num_ini==1)
    {
        for(int i=0;i<num_interp;++i)
            y_interp[i]=y[0];
        return true;
    }
    double delta_x;
    long element_index=0;
    long start_index = 0;
    for(int i=0;i<num_interp;++i)
    {
        if(i+1<num_interp)
        {
            delta_x =x_interp[i+1]-x_interp[i];
        }
        else if(num_interp==1)
        {
            delta_x = 0;
        }
        else
        {
            delta_x = x_interp[num_interp-1]-x_interp[num_interp-2];
        }
        delta_x = fabs(delta_x);
        element_index = this->FindFirstElement(x,num_ini,x_interp[i],1,start_index,-1);
        if(element_index<=-1)
        {
            y_interp[i]=y[num_ini-1];
        }
        else if(element_index==0)
        {
            y_interp[i]=y[0];
        }
        else
        {
            if(fabs(x[element_index]-x_interp[i])<delta_x*0.5)y_interp[i] = y[element_index];
            else if(fabs(x[element_index-1]-x_interp[i])<delta_x*0.5) y_interp[i] = y[element_index-1];
            else y_interp[i] = 0;
        }
        start_index = element_index;
        if(start_index<0)start_index = 0;
    }
    this->time0 = delta_x;
    DeletePtrArray(this->xd_inter);
    this->xd_inter = new float[num_interp*sizeof(float)];
    memcpy(this->xd_inter,x_interp,num_interp*sizeof(float));
    DeletePtrArray(this->yd_inter);
    this->yd_inter = new float[num_interp*sizeof(float)];
    memcpy(this->yd_inter,y_interp,num_interp*sizeof(float));
    this->xd_inter_size = num_interp;
    return true;
}
//condition==0表示找到等于阈值的参数的第一个元素的索引
//condition==1表示找到大于阈值的参数的第一个元素的索引,2表示大于等于阈值的第一个元素的索引
//condition==-1表示找到小于阈值的参数的第一个元素的索引，-2表示小于等于阈值的第一个元素的索引
//endindex=-1表示直接到结尾
long mFFT::FindFirstElement(float* data,const long num,float threshold,int condition,long start_index,long end_index)
{
    if(data==NULL||num<1)return -1;
    if(start_index>num-1)start_index=num-1;
    if(end_index>num||end_index<0||end_index==-1)end_index=num;
    long search_result_index=-1;
    for(int i=start_index;i<end_index;++i)
    {
        if(condition==0)
        {
            if(data[i]==threshold)
            {
                search_result_index=i;
                break;
            }
        }
        else if (condition==1)
        {
            if(data[i]>threshold)
            {
                search_result_index=i;
                break;
            }
        }
        else if (condition==2)
        {
            if(data[i]>=threshold)
            {
                search_result_index=i;
                break;
            }
        }
        else if (condition==-1)
        {
            if(data[i]<threshold)
            {
                search_result_index=i;
                break;
            }
        }
        else if (condition==-2)
        {
            if(data[i]<=threshold)
            {
                search_result_index=i;
                break;
            }
        }
    }
    return search_result_index;

}
void mFFT::Interp(float xdata[], float ydata[],const int num, float xdata_inter[], float ydata_out[], const int index)
{
    long double time0_temp = 0;
    if (index <= 0) return;
    if (num == 1)
    {
        time0_temp = 0;
        for(int i=1;i<index;i++)
        {
            ydata_out[i] = ydata[0];
            time0_temp += xdata_inter[i];
        }
        this->time0 = time0_temp/index;
        DeletePtrArray(this->xd_inter);
        this->xd_inter = new float[index*sizeof(float)];
        memcpy(this->xd_inter,xdata_inter,index*sizeof(float));
        DeletePtrArray(this->yd_inter);
        this->yd_inter = new float[index*sizeof(float)];
        memcpy(this->yd_inter,ydata_out,index*sizeof(float));
        this->xd_inter_size = index;
        return;
    }
    if (index == 1)
    {
        this->time0 = -1;
        this->fs0 = 0;
        ydata_out[0] = getLarInterpOne(xdata, ydata, num, xdata_inter[0]);
        DeletePtrArray(this->xd_inter);
        this->xd_inter = new float[index*sizeof(float)];
        memcpy(this->xd_inter,xdata_inter,index*sizeof(float));
        DeletePtrArray(this->yd_inter);
        this->yd_inter = new float[index*sizeof(float)];
        memcpy(this->yd_inter,ydata_out,index*sizeof(float));
        this->xd_inter_size = index;
        return;
    }
    time0_temp = 0;
    int var_time_count = 0;
    if(index > 1000) var_time_count = 1000;
    else var_time_count = index-1;
    time0_temp = xdata_inter[var_time_count]-xdata_inter[0];
    this->time0 = time0_temp/var_time_count;
    float xdata_temp[2] = {0};
    float ydata_temp[2] = {0};
    int k =0;
    for(int i = 0;i<index;i++)
    {
        if((k >= num-1) & (k >1)) k = num-1-1;
        else
        {
            for(int j = num-1-1;j>=0;j--)
            {
    //            if(xdata_inter[i] >= xdata[j] && xdata_inter[i]<=xdata[j+1])
    //            {
    //                k = j;
    //                break;
    //            }
    //            else if(j > num-1-1) k = num-1-1;
                if(xdata[j] > xdata_inter[i]) continue;
                else
                {
                    k = j;
                    break;
                }
            }
        }
        xdata_temp[0] = xdata[k];
        xdata_temp[1] = xdata[k+1];
        ydata_temp[0] = ydata[k];
        ydata_temp[1] = ydata[k+1];
        ydata_out[i] = getLarInterpOne(xdata_temp,ydata_temp,2,xdata_inter[i]);
    }

    DeletePtrArray(this->xd_inter);
    this->xd_inter = new float[index*sizeof(float)];
    memcpy(this->xd_inter,xdata_inter,index*sizeof(float));
    DeletePtrArray(this->yd_inter);
    this->yd_inter = new float[index*sizeof(float)];
    memcpy(this->yd_inter,ydata_out,index*sizeof(float));
    this->xd_inter_size = index;
}

void mFFT::getLarInterpOne(float xdata[], float ydata[],const int num, float x_inter, float &y_inter)
{

    y_inter = getLarInterpOne(xdata, ydata, num, x_inter);

}

float mFFT::getLarInterpOne(float xdata[], float ydata[],const int num, float x_inter)
{
    //int k = 0;
    float y_inter=0;
    if (num == 1)
    {
        y_inter = ydata[0];
        return y_inter;
    }
//    for(int i=0;i<num;i++)
//    {
//        int k = i;
//        if(x_inter < xdata[i])
//        {
//            if (i<num-1) continue;
//        }
//        if((i == num-1) & (i >1))
//        {
//            k = num-1-1;

//        }
//        if (xdata[k+1] == xdata[k])
//        {
//            return ydata[k];
//            break;
//        }
//        //y_inter = ydata[k]*(x_inter-xdata[k+1])/(xdata[k]-xdata[k+1])+
//        //          ydata[k+1]*(x_inter-xdata[k])/(xdata[k+1]-xdata[k]);
//        y_inter = (x_inter-xdata[k])*(ydata[k+1]-ydata[k])/(xdata[k+1]-xdata[k]);
//        break;
//    }
    if (xdata[1] == xdata[0])
    {
        return ydata[0];
    }
    else
        y_inter = (x_inter-xdata[0])*(ydata[1]-ydata[0])/(xdata[1]-xdata[0])+ydata[0];

    return y_inter;
}


///////////////////////////////
//void mFFT::bitrp (float xreal [], float ximag [], int n)
//{
//    // 位反转置换 Bit-reversal Permutation
//    int i, j, a, b, p;

//    for (i = 1, p = 0; i < n; i *= 2)
//        {
//        p ++;
//        }
//    for (i = 0; i < n; i ++)
//        {
//        a = i;
//        b = 0;
//        for (j = 0; j < p; j ++)
//            {
//            b = (b << 1) + (a & 1);    // b = b * 2 + a % 2;
//            a >>= 1;        // a = a / 2;
//            }
//        if ( b > i)
//            {
//            swap (xreal [i], xreal [b]);
//            swap (ximag [i], ximag [b]);
//            }
//        }
//}

//void mFFT::FFT(float xreal [], float ximag [], const int num)
//{
//    unsigned long int n = num;
//    if(num%2)
//    {
//        n =num-1;
//    }
//    else
//    {
//        n = num;
//    }
//    // 快速傅立叶变换，将复数 x 变换后仍保存在 x 中，xreal, ximag 分别是 x 的实部和虚部
//    float wreal [N / 2], wimag [N / 2], treal, timag, ureal, uimag, arg;
//    int m, k, j, t, index1, index2;

//    bitrp (xreal, ximag, n);

//    // 计算 1 的前 n / 2 个 n 次方根的共轭复数 W'j = wreal [j] + i * wimag [j] , j = 0, 1, ... , n / 2 - 1
//    arg = - 2 * PI / n;
//    treal = cos (arg);
//    timag = sin (arg);
//    wreal [0] = 1.0;
//    wimag [0] = 0.0;
//    for (j = 1; j < n / 2; j ++)
//        {
//        wreal [j] = wreal [j - 1] * treal - wimag [j - 1] * timag;
//        wimag [j] = wreal [j - 1] * timag + wimag [j - 1] * treal;
//        }

//    for (m = 2; m <= n; m *= 2)
//        {
//        for (k = 0; k < n; k += m)
//            {
//            for (j = 0; j < m / 2; j ++)
//                {
//                index1 = k + j;
//                index2 = index1 + m / 2;
//                t = n * j / m;    // 旋转因子 w 的实部在 wreal [] 中的下标为 t
//                treal = wreal [t] * xreal [index2] - wimag [t] * ximag [index2];
//                timag = wreal [t] * ximag [index2] + wimag [t] * xreal [index2];
//                ureal = xreal [index1];
//                uimag = ximag [index1];
//                xreal [index1] = ureal + treal;
//                ximag [index1] = uimag + timag;
//                xreal [index2] = ureal - treal;
//                ximag [index2] = uimag - timag;
//                }
//            }
//        }

//    if(num%2)
//    {
//        xreal[num-1] = 0;
//        ximag[num-1] = 0;
//    }
//    return;
//}

//void  mFFT::IFFT(float xreal [], float ximag [], const int num)
//{
//    unsigned long int n = num;
//    if(num%2)
//    {
//        n =num-1;
//    }
//    else
//    {
//        n = num;
//    }
//    // 快速傅立叶逆变换
//    float wreal [N / 2], wimag [N / 2], treal, timag, ureal, uimag, arg;
//    int m, k, j, t, index1, index2;

//    bitrp (xreal, ximag, n);

//    // 计算 1 的前 n / 2 个 n 次方根 Wj = wreal [j] + i * wimag [j] , j = 0, 1, ... , n / 2 - 1
//    arg = 2 * PI / n;
//    treal = cos (arg);
//    timag = sin (arg);
//    wreal [0] = 1.0;
//    wimag [0] = 0.0;
//    for (j = 1; j < n / 2; j ++)
//        {
//        wreal [j] = wreal [j - 1] * treal - wimag [j - 1] * timag;
//        wimag [j] = wreal [j - 1] * timag + wimag [j - 1] * treal;
//        }

//    for (m = 2; m <= n; m *= 2)
//        {
//        for (k = 0; k < n; k += m)
//            {
//            for (j = 0; j < m / 2; j ++)
//                {
//                index1 = k + j;
//                index2 = index1 + m / 2;
//                t = n * j / m;    // 旋转因子 w 的实部在 wreal [] 中的下标为 t
//                treal = wreal [t] * xreal [index2] - wimag [t] * ximag [index2];
//                timag = wreal [t] * ximag [index2] + wimag [t] * xreal [index2];
//                ureal = xreal [index1];
//                uimag = ximag [index1];
//                xreal [index1] = ureal + treal;
//                ximag [index1] = uimag + timag;
//                xreal [index2] = ureal - treal;
//                ximag [index2] = uimag - timag;
//                }
//            }
//        }

//    for (j=0; j < n; j ++)
//        {
//        xreal [j] /= n;
//        ximag [j] /= n;
//        }

//    if(num%2)
//    {
//        xreal[num-1] = 0;
//        ximag[num-1] = 0;
//    }

//    return;
//}

//void mFFT::FFT_process(float xreal[], float ximag[], const int num,int flag)
//{
//    unsigned long int n = num;
//    if(num%2)
//    {
//        n =num-1;
//        xreal[num-1] = 0;
//        ximag[num-1] = 0;
//    }
//    else
//    {
//        n = num;
//    }
//    if(flag == 0)
//    {
//        FFT(xreal, ximag, n);
//    }
//    else
//    {
//        IFFT(xreal,ximag,n);
//    }
//}

////void myFFT::FFT_process (float xreal[], int flag)
////{
////    float y
////}

//void mFFT::FFT_test ()
//{
//    char inputfile [] = "input.txt";    // 从文件 input.txt 中读入原始数据
//    char outputfile [] = "output.txt";    // 将结果输出到文件 output.txt 中
//    float xreal [N] = {}, ximag [N] = {};
//    int n, i;
//    FILE *input, *output;

//    if (!(input = fopen (inputfile, "r")))
//        {
//        printf ("Cannot open file. ");
//        exit (1);
//        }
//    if (!(output = fopen (outputfile, "w")))
//        {
//        printf ("Cannot open file. ");
//        exit (1);
//        }

//    i = 0;
//    while ((fscanf (input, "%f%f", xreal + i, ximag + i)) != EOF)
//        {
//        i ++;
//        }
//    n = i;    // 要求 n 为 2 的整数幂
//    while (i > 1)
//        {
//        if (i % 2)
//            {
//            fprintf (output, "%d is not a power of 2! ", n);
//            exit (1);
//            }
//        i /= 2;
//        }

//    FFT (xreal, ximag, n);
//    fprintf (output, "FFT:    i	    real	imag ");
//    for (i = 0; i < n; i ++)
//        {
//        fprintf (output, "%4d    %8.4f    %8.4f ", i, xreal [i], ximag [i]);
//        }
//    fprintf (output, "================================= ");

//    IFFT (xreal, ximag, n);
//    fprintf (output, "IFFT:    i	    real	imag ");
//    for (i = 0; i < n; i ++)
//        {
//        fprintf (output, "%4d    %8.4f    %8.4f ", i, xreal [i], ximag [i]);
//        }

//    if ( fclose (input))
//        {
//        printf ("File close error. ");
//        exit (1);
//        }
//    if ( fclose (output))
//        {
//        printf ("File close error. ");
//        exit (1);
//        }
//}

