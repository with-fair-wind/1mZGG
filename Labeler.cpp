#include "Labeler.h"

Labeler::Labeler() {
    m_stack = NULL;
    m_mask  = NULL;

    m_width  = 0;
    m_height = 0;
    m_stack_size    = 0;
    m_stack_pointer = 0;

#ifndef __linux__
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	m_iNumCPUCores = sysInfo.dwNumberOfProcessors;
#else
    m_iNumCPUCores = 16;
#endif	
}

Labeler::~Labeler() {
    if(m_stack) {
        delete[] m_stack;
    }
    m_stack = NULL;

    if(m_mask) {
        delete[] m_mask;
    }
    m_mask = NULL;

    m_width  = 0;
    m_height = 0;
    m_stack_size    = 0;
    m_stack_pointer = 0;


}

unsigned int Labeler::Label(unsigned char* img, int width, int height, Blobs &blobs, int roix, int roiy, int roiwidth, int roiheight, bool roi) {
    if(roi) {
        if(roix < 0) {
            roix = 0;
        }

        if(roiy < 0) {
            roiy = 0;
        }

        if(roix + roiwidth > width) {
            roiwidth = width - roix;
        }

        if(roiy + roiheight > height) {
            roiheight = height - roiy;
        }
    } else {
        roix      = 0;
        roiy      = 0;
        roiwidth  = width;
        roiheight = height;
    }

    if((m_width != width) || (m_height != height)) {
        m_width = width;
        m_height = height;

        if(m_stack) {
            delete[] m_stack;
        }
        if(m_mask) {
            delete[] m_mask;
        }
        m_mask = NULL;

        m_stack_size    = width * height;
        m_stack         = new size_t[m_stack_size];
        m_mask          = new unsigned char[m_stack_size];
        m_stack_pointer = 0;
    }

    ReleaseBlobs(blobs);

    if(m_mask) {
        memset(m_mask, 0x00, m_stack_size);
    } else {
        return 0;
    }

    unsigned char* data = img;

    m_stack_pointer = 0;
    size_t label = 0;

//    #pragma omp parallel for num_threads(m_iNumCPUCores)
    for(int row = roiy ; row < roiy + roiheight ; row++) {
        for(int col = roix ; col < roix +roiwidth ; col++) {
            size_t idx = row * width + col;

            if(!m_mask[idx]) {
                m_mask[idx] = 1;

                if(data[idx]) {
                    Blob* blob = new Blob;

                    blob->label = label;
                    blob->minx = col;
                    blob->maxx = col;
                    blob->miny = row;
                    blob->maxy = row;
                    blob->area = 1;
                    blob->m10  = col;
                    blob->m01  = row;

                    Push(idx);

                    while(m_stack_pointer > 0) {
                        size_t idx0 = Pop();

                        unsigned int row0 = idx0 / width;
                        unsigned int col0 = idx0 % width;

                        if(col0 < blob->minx) {
                            blob->minx = col0;
                        }
                        if(col0 > blob->maxx) {
                            blob->maxx = col0;
                        }
                        if(row0 < blob->miny) {
                            blob->miny = row0;
                        }
                        if(row0 > blob->maxy) {
                            blob->maxy = row0;
                        }

                        ///8 neighbor
                        int srow = row0 - 1;
                        int arow = row0 + 1;
                        int scol = col0 - 1;
                        int acol = col0 + 1;

                        ///row - 1
                        if((srow >= 0)) {
                            if(scol >= 0) {
                                size_t idx1 = srow * width + scol;
                                if(!m_mask[idx1]) {
                                    m_mask[idx1] = 1;

                                    if(data[idx1]) {
                                        Push(idx1);

                                        blob->area++;
                                        blob->m10 += scol;
                                        blob->m01 += srow;
                                    }
                                }
                            }

                            size_t idx2 = srow * width + col0;
                            if(!m_mask[idx2]) {
                                m_mask[idx2] = 1;

                                if(data[idx2]) {
                                    Push(idx2);

                                    blob->area++;
                                    blob->m10 += col0;
                                    blob->m01 += srow;
                                }
                            }

                            if(acol < width) {
                                size_t idx1 = srow * width + acol;
                                if(!m_mask[idx1]) {
                                    m_mask[idx1] = 1;

                                    if(data[idx1]) {
                                        Push(idx1);

                                        blob->area++;
                                        blob->m10 += acol;
                                        blob->m01 += srow;
                                    }
                                }
                            }
                        }

                        ///row
                        if(scol >= 0) {
                            size_t idx1 = row0 * width + scol;
                            if(!m_mask[idx1]) {
                                m_mask[idx1] = 1;

                                if(data[idx1]) {
                                    Push(idx1);

                                    blob->area++;
                                    blob->m10 += scol;
                                    blob->m01 += row0;
                                }
                            }
                        }

                        if(acol < width) {
                            size_t idx1 = row0 * width + acol;
                            if(!m_mask[idx1]) {
                                m_mask[idx1] = 1;

                                if(data[idx1]) {
                                    Push(idx1);

                                    blob->area++;
                                    blob->m10 += acol;
                                    blob->m01 += row0;
                                }
                            }
                        }

                        ///row + 1
                        if((arow < height)) {
                            if(scol >= 0) {
                                size_t idx1 = arow * width + scol;
                                if(!m_mask[idx1]) {
                                    m_mask[idx1] = 1;

                                    if(data[idx1]) {
                                        Push(idx1);

                                        blob->area++;
                                        blob->m10 += scol;
                                        blob->m01 += arow;
                                    }
                                }
                            }

                            size_t idx2 = arow * width + col0;
                            if(!m_mask[idx2]) {
                                m_mask[idx2] = 1;

                                if(data[idx2]) {
                                    Push(idx2);

                                    blob->area++;
                                    blob->m10 += col0;
                                    blob->m01 += arow;
                                }
                            }

                            if(acol < width) {
                                size_t idx1 = arow * width + acol;
                                if(!m_mask[idx1]) {
                                    m_mask[idx1] = 1;

                                    if(data[idx1]) {
                                        Push(idx1);

                                        blob->area++;
                                        blob->m10 += acol;
                                        blob->m01 += arow;
                                    }
                                }
                            }
                        }
                    }

                    blob->centroid_x = blob->m10 / (double)blob->area;
                    blob->centroid_y = blob->m01 / (double)blob->area;
                    blobs.insert(LabelBlob(label,blob));
                    label++;
                }
            }
        }
    }

    return label;
}

/**
 * Blob计算的高效算法，比Label速度快一倍左右. (2048*2048) Label:5.97ms, LabelEff:2.95ms
 * 但是此算法破坏了图像，最后参数img中所有的数据都为0，在后续不使用二值化图像的情况下，此算法适用
 */
unsigned int Labeler::LabelEff(unsigned char* img, int width, int height, Blobs &blobs, int roix, int roiy, int roiwidth, int roiheight, bool roi) {
    if(roi) {
        if(roix < 0) {
            roix = 0;
        }

        if(roiy < 0) {
            roiy = 0;
        }

        if(roix + roiwidth > width) {
            roiwidth = width - roix;
        }

        if(roiy + roiheight > height) {
            roiheight = height - roiy;
        }
    } else {
        roix      = 0;
        roiy      = 0;
        roiwidth  = width;
        roiheight = height;
    }

    if((m_width != width) || (m_height != height)) {
        m_width = width;
        m_height = height;

        if(m_stack) {
            delete[] m_stack;
        }
        if(m_mask) {
            delete[] m_mask;
        }

        m_stack_size    = width * height;
        m_stack         = new size_t[m_stack_size];
        m_stack_pointer = 0;
        //m_mask          = new unsigned char[m_stack_size];
    }

    ReleaseBlobs(blobs);

    //memcpy(m_mask, img, m_stack_size);
    unsigned char* data = img;

    m_stack_pointer = 0;
    size_t label = 0;

//#pragma omp parallel for num_threads(m_iNumCPUCores)
    for(int row = roiy ; row < roiy + roiheight ; row++) 
	{
        for(int col = roix ; col < roix +roiwidth ; col++) 
		{
            size_t idx = row * width + col;

            if(data[idx]) 
			{
                data[idx] = 0;

                Blob* blob = new Blob;

                blob->label = label;
                blob->minx = col;
                blob->maxx = col;
                blob->miny = row;
                blob->maxy = row;
                blob->area = 1;
                blob->m10  = col;
                blob->m01  = row;

                Push(idx);

                while(m_stack_pointer > 0) 
				{
                    size_t idx0 = Pop();

                    unsigned int row0 = idx0 / width;
                    unsigned int col0 = idx0 % width;

                    if(col0 < blob->minx) {
                        blob->minx = col0;
                    }
                    if(col0 > blob->maxx) {
                        blob->maxx = col0;
                    }
                    if(row0 < blob->miny) {
                        blob->miny = row0;
                    }
                    if(row0 > blob->maxy) {
                        blob->maxy = row0;
                    }

                    ///8 neighbor
                    int srow = row0 - 1;
                    int arow = row0 + 1;
                    int scol = col0 - 1;
                    int acol = col0 + 1;

                    ///row - 1
                    if((srow >= 0)) {
                        if(scol >= 0) {
                            size_t idx1 = srow * width + scol;
                            if(data[idx1]) {
                                data[idx1] = 0;

                                Push(idx1);

                                blob->area++;
                                blob->m10 += scol;
                                blob->m01 += srow;
                            }
                        }

                        size_t idx2 = srow * width + col0;
                        if(data[idx2]) {
                            data[idx2] = 0;

                            Push(idx2);

                            blob->area++;
                            blob->m10 += col0;
                            blob->m01 += srow;
                        }

                        if(acol < width) {
                            size_t idx1 = srow * width + acol;
                            if(data[idx1]) {
                                data[idx1] = 0;

                                Push(idx1);

                                blob->area++;
                                blob->m10 += acol;
                                blob->m01 += srow;
                            }
                        }
                    }

                    ///row
                    if(scol >= 0) {
                        size_t idx1 = row0 * width + scol;
                        if(data[idx1]) {
                            data[idx1] = 0;

                            Push(idx1);

                            blob->area++;
                            blob->m10 += scol;
                            blob->m01 += row0;
                        }
                    }

                    if(acol < width) {
                        size_t idx1 = row0 * width + acol;
                        if(data[idx1]) {
                            data[idx1] = 0;

                            Push(idx1);

                            blob->area++;
                            blob->m10 += acol;
                            blob->m01 += row0;
                        }
                    }

                    ///row + 1
                    if((arow < height)) {
                        if(scol >= 0) {
                            size_t idx1 = arow * width + scol;
                            if(data[idx1]) {
                                data[idx1] = 0;

                                Push(idx1);

                                blob->area++;
                                blob->m10 += scol;
                                blob->m01 += arow;
                            }
                        }

                        size_t idx2 = arow * width + col0;
                        if(data[idx2]) {
                            data[idx2] = 0;

                            Push(idx2);

                            blob->area++;
                            blob->m10 += col0;
                            blob->m01 += arow;
                        }

                        if(acol < width) {
                            size_t idx1 = arow * width + acol;
                            if(data[idx1]) {
                                data[idx1] = 0;

                                Push(idx1);

                                blob->area++;
                                blob->m10 += acol;
                                blob->m01 += arow;
                            }
                        }
                    }
                }

                blob->centroid_x = blob->m10 / (double)blob->area;
                blob->centroid_y = blob->m01 / (double)blob->area;
                blobs.insert(LabelBlob(label,blob));
                label++;
            }
        }
    }

    return label;
}

/**
 * 当前二值图像和掩模二值图像计算Blob，若掩模二值图像中的点和当前二值图像中的点有交集，则不计算当前二值图像的Blob。
 * 最后得到的Blob是存在于当前二值图像中，且和掩模二值图像无交集的点
 */
unsigned int Labeler::LabelWithMask(unsigned char* img, unsigned char* mask, int width, int height, Blobs &blobs, int roix, int roiy, int roiwidth, int roiheight, bool roi) {
    if(roi) {
        if(roix < 0) {
            roix = 0;
        }

        if(roiy < 0) {
            roiy = 0;
        }

        if(roix + roiwidth > width) {
            roiwidth = width - roix;
        }

        if(roiy + roiheight > height) {
            roiheight = height - roiy;
        }
    } else {
        roix      = 0;
        roiy      = 0;
        roiwidth  = width;
        roiheight = height;
    }

    if((m_width != width) || (m_height != height)) {
        m_width = width;
        m_height = height;

        if(m_stack) {
            delete[] m_stack;
        }
        if(m_mask) {
            delete[] m_mask;
        }

        m_stack_size    = width * height;
        m_stack         = new size_t[m_stack_size];
        m_mask          = new unsigned char[m_stack_size];
        m_stack_pointer = 0;
    }

    ReleaseBlobs(blobs);

    if(m_mask) {
        memset(m_mask, 0x00, m_stack_size);
    } else {
        return 0;
    }

    unsigned char* data = img;

    m_stack_pointer = 0;
    size_t label = 0;

    for(int row = roiy ; row < roiy + roiheight ; row++) {
        for(int col = roix ; col < roix +roiwidth ; col++) {
            size_t idx = row * width + col;

            if(!m_mask[idx]) {
                m_mask[idx] = 1;

                bool mask_flag = false;
                if(data[idx]) {
                    Blob* blob = new Blob;

                    blob->label = label;
                    blob->minx = col;
                    blob->maxx = col;
                    blob->miny = row;
                    blob->maxy = row;
                    blob->area = 1;
                    blob->m10  = col;
                    blob->m01  = row;

                    Push(idx);

                    if(mask[idx]) {
                        mask_flag = true;
                    }

                    while(m_stack_pointer > 0) {
                        size_t idx0 = Pop();

                        unsigned int row0 = idx0 / width;
                        unsigned int col0 = idx0 % width;

                        if(col0 < blob->minx) {
                            blob->minx = col0;
                        }
                        if(col0 > blob->maxx) {
                            blob->maxx = col0;
                        }
                        if(row0 < blob->miny) {
                            blob->miny = row0;
                        }
                        if(row0 > blob->maxy) {
                            blob->maxy = row0;
                        }

                        ///8 neighbor
                        int srow = row0 - 1;
                        int arow = row0 + 1;
                        int scol = col0 - 1;
                        int acol = col0 + 1;

                        ///row - 1
                        if((srow >= 0)) {
                            if(scol >= 0) {
                                size_t idx1 = srow * width + scol;
                                if(!m_mask[idx1]) {
                                    m_mask[idx1] = 1;

                                    if(data[idx1]) {
                                        Push(idx1);

                                        if(mask[idx1]) {
                                            mask_flag = true;
                                        }

                                        blob->area++;
                                        blob->m10 += scol;
                                        blob->m01 += srow;
                                    }
                                }
                            }

                            size_t idx2 = srow * width + col0;
                            if(!m_mask[idx2]) {
                                m_mask[idx2] = 1;

                                if(data[idx2]) {
                                    Push(idx2);

                                    if(mask[idx2]) {
                                        mask_flag = true;
                                    }

                                    blob->area++;
                                    blob->m10 += col0;
                                    blob->m01 += srow;
                                }
                            }

                            if(acol < width) {
                                size_t idx1 = srow * width + acol;
                                if(!m_mask[idx1]) {
                                    m_mask[idx1] = 1;

                                    if(data[idx1]) {
                                        Push(idx1);

                                        if(mask[idx1]) {
                                            mask_flag = true;
                                        }

                                        blob->area++;
                                        blob->m10 += acol;
                                        blob->m01 += srow;
                                    }
                                }
                            }
                        }

                        ///row
                        if(scol >= 0) {
                            size_t idx1 = row0 * width + scol;
                            if(!m_mask[idx1]) {
                                m_mask[idx1] = 1;

                                if(data[idx1]) {
                                    Push(idx1);

                                    if(mask[idx1]) {
                                        mask_flag = true;
                                    }

                                    blob->area++;
                                    blob->m10 += scol;
                                    blob->m01 += row0;
                                }
                            }
                        }

                        if(acol < width) {
                            size_t idx1 = row0 * width + acol;
                            if(!m_mask[idx1]) {
                                m_mask[idx1] = 1;

                                if(data[idx1]) {
                                    Push(idx1);

                                    if(mask[idx1]) {
                                        mask_flag = true;
                                    }

                                    blob->area++;
                                    blob->m10 += acol;
                                    blob->m01 += row0;
                                }
                            }
                        }

                        ///row + 1
                        if((arow < height)) {
                            if(scol >= 0) {
                                size_t idx1 = arow * width + scol;
                                if(!m_mask[idx1]) {
                                    m_mask[idx1] = 1;

                                    if(data[idx1]) {
                                        Push(idx1);

                                        if(mask[idx1]) {
                                            mask_flag = true;
                                        }

                                        blob->area++;
                                        blob->m10 += scol;
                                        blob->m01 += arow;
                                    }
                                }
                            }

                            size_t idx2 = arow * width + col0;
                            if(!m_mask[idx2]) {
                                m_mask[idx2] = 1;

                                if(data[idx2]) {
                                    Push(idx2);

                                    if(mask[idx2]) {
                                        mask_flag = true;
                                    }

                                    blob->area++;
                                    blob->m10 += col0;
                                    blob->m01 += arow;
                                }
                            }

                            if(acol < width) {
                                size_t idx1 = arow * width + acol;
                                if(!m_mask[idx1]) {
                                    m_mask[idx1] = 1;

                                    if(data[idx1]) {
                                        Push(idx1);

                                        if(mask[idx1]) {
                                            mask_flag = true;
                                        }

                                        blob->area++;
                                        blob->m10 += acol;
                                        blob->m01 += arow;
                                    }
                                }
                            }
                        }
                    }

                    if(mask_flag) {
                        delete blob;
                    } else {
                        blob->centroid_x = blob->m10 / (double)blob->area;
                        blob->centroid_y = blob->m01 / (double)blob->area;

                        blobs.insert(LabelBlob(label,blob));
                        label++;
                    }
                }
            }
        }
    }

    return label;
}

/**
 * Blob计算的高效算法，比Label速度快一倍左右. (2048*2048) Label:5.97ms, LabelEff:2.95ms
 * 但是此算法破坏了图像，最后参数img中所有的数据都为0，在后续不使用二值化图像的情况下，此算法适用
 */
unsigned int Labeler::LabelWithMaskEff(unsigned char* img, unsigned char* mask, int width, int height, Blobs &blobs, int roix, int roiy, int roiwidth, int roiheight, bool roi) {
    if(roi) {
        if(roix < 0) {
            roix = 0;
        }

        if(roiy < 0) {
            roiy = 0;
        }

        if(roix + roiwidth > width) {
            roiwidth = width - roix;
        }

        if(roiy + roiheight > height) {
            roiheight = height - roiy;
        }
    } else {
        roix      = 0;
        roiy      = 0;
        roiwidth  = width;
        roiheight = height;
    }

    if((m_width != width) || (m_height != height)) {
        m_width = width;
        m_height = height;

        if(m_stack) {
            delete[] m_stack;
        }

        m_stack_size    = width * height;
        m_stack         = new size_t[m_stack_size];
        m_stack_pointer = 0;
    }

    ReleaseBlobs(blobs);

    unsigned char* data = img;

    m_stack_pointer = 0;
    size_t label = 0;

    for(int row = roiy ; row < roiy + roiheight ; row++) {
        for(int col = roix ; col < roix +roiwidth ; col++) {
            size_t idx = row * width + col;

            bool mask_flag = false;

            if(data[idx]) {
                data[idx] = 0;

                Blob* blob = new Blob;

                blob->label = label;
                blob->minx = col;
                blob->maxx = col;
                blob->miny = row;
                blob->maxy = row;
                blob->area = 1;
                blob->m10  = col;
                blob->m01  = row;

                Push(idx);

                if(mask[idx]) {
                    mask_flag = true;
                }

                while(m_stack_pointer > 0) {
                    size_t idx0 = Pop();

                    unsigned int row0 = idx0 / width;
                    unsigned int col0 = idx0 % width;

                    if(col0 < blob->minx) {
                        blob->minx = col0;
                    }
                    if(col0 > blob->maxx) {
                        blob->maxx = col0;
                    }
                    if(row0 < blob->miny) {
                        blob->miny = row0;
                    }
                    if(row0 > blob->maxy) {
                        blob->maxy = row0;
                    }

                    ///8 neighbor
                    int srow = row0 - 1;
                    int arow = row0 + 1;
                    int scol = col0 - 1;
                    int acol = col0 + 1;

                    ///row - 1
                    if((srow >= 0)) {
                        if(scol >= 0) {
                            size_t idx1 = srow * width + scol;

                            if(data[idx1]) {
                                data[idx1] = 0;

                                Push(idx1);

                                if(mask[idx1]) {
                                    mask_flag = true;
                                }

                                blob->area++;
                                blob->m10 += scol;
                                blob->m01 += srow;
                            }
                        }

                        size_t idx2 = srow * width + col0;
                        if(data[idx2]) {
                            data[idx2] = 0;
                            Push(idx2);

                            if(mask[idx2]) {
                                mask_flag = true;
                            }

                            blob->area++;
                            blob->m10 += col0;
                            blob->m01 += srow;
                        }

                        if(acol < width) {
                            size_t idx1 = srow * width + acol;
                            if(data[idx1]) {
                                data[idx1] = 0;

                                Push(idx1);

                                if(mask[idx1]) {
                                    mask_flag = true;
                                }

                                blob->area++;
                                blob->m10 += acol;
                                blob->m01 += srow;
                            }
                        }
                    }

                    ///row
                    if(scol >= 0) {
                        size_t idx1 = row0 * width + scol;
                        if(data[idx1]) {
                            data[idx1] = 0;

                            Push(idx1);

                            if(mask[idx1]) {
                                mask_flag = true;
                            }

                            blob->area++;
                            blob->m10 += scol;
                            blob->m01 += row0;
                        }
                    }

                    if(acol < width) {
                        size_t idx1 = row0 * width + acol;
                        if(data[idx1]) {
                            data[idx1] = 0;

                            Push(idx1);

                            if(mask[idx1]) {
                                mask_flag = true;
                            }

                            blob->area++;
                            blob->m10 += acol;
                            blob->m01 += row0;
                        }
                    }

                    ///row + 1
                    if((arow < height)) {
                        if(scol >= 0) {
                            size_t idx1 = arow * width + scol;
                            if(data[idx1]) {
                                data[idx1] = 0;

                                Push(idx1);

                                if(mask[idx1]) {
                                    mask_flag = true;
                                }

                                blob->area++;
                                blob->m10 += scol;
                                blob->m01 += arow;
                            }
                        }

                        size_t idx2 = arow * width + col0;
                        if(data[idx2]) {
                            data[idx2] = 0;

                            Push(idx2);

                            if(mask[idx2]) {
                                mask_flag = true;
                            }

                            blob->area++;
                            blob->m10 += col0;
                            blob->m01 += arow;
                        }

                        if(acol < width) {
                            size_t idx1 = arow * width + acol;
                            if(data[idx1]) {
                                data[idx1] = 0;

                                Push(idx1);

                                if(mask[idx1]) {
                                    mask_flag = true;
                                }

                                blob->area++;
                                blob->m10 += acol;
                                blob->m01 += arow;
                            }
                        }
                    }
                }

                if(mask_flag) {
                    delete blob;
                } else {
                    blob->centroid_x = blob->m10 / (double)blob->area;
                    blob->centroid_y = blob->m01 / (double)blob->area;

                    blobs.insert(LabelBlob(label,blob));
                    label++;
                }
            }
        }
    }

    return label;
}

void Labeler::ReleaseBlobs(Blobs &blobs) {
    for (Blobs::iterator it=blobs.begin(); it!=blobs.end(); ++it) {
		if ((*it).second) {
			delete ((*it).second);
		}        
    }
    blobs.clear();
}

void Labeler::Push(size_t val) {
    if(m_stack) {
        if(m_stack_pointer < m_stack_size - 1) {
            m_stack[m_stack_pointer] = val;
            m_stack_pointer++;
        }
    }
}

size_t Labeler::Pop() {
    if(m_stack) {
        if(m_stack_pointer > 0) {
            m_stack_pointer--;
            return m_stack[m_stack_pointer];
        }
    }

    return 0;
}

size_t Labeler::Top() {
    if(m_stack) {
        if(m_stack_pointer > 0) {
            return m_stack[m_stack_pointer - 1];
        }
    }

    return 0;
}

void Labeler::FilterByArea(Blobs &blobs, unsigned int minArea, unsigned int maxArea) {
    if(blobs.size() > 0) {
        Blobs::iterator it=blobs.begin();
        while(it!=blobs.end()) {
            Blob *blob=(*it).second;
            if ((blob->area < minArea) || (blob->area > maxArea)) {
                delete blob;

                Blobs::iterator tmp=it;
                ++it;
                blobs.erase(tmp);
            } else {
                ++it;
            }
        }
    }
}

void Labeler::FilterByMaxIntensity(Blobs &blobs, unsigned short* udata, int width, int height) {
    unsigned short imax = 0;

    if(blobs.size() > 0) {
        Blobs::iterator it=blobs.begin();
        while(it!=blobs.end()) {
            Blob *blob=(*it).second;

            unsigned short imax0 = 0;
            for(int i = blob->miny ; i <= blob->maxy ; i++) {
                for(int j = blob->minx ; j <= blob->maxx ; j++) {
                    unsigned short val = udata[i * width + j];

                    if(imax0 < val) {
                        imax0 = val;
                    }
                }
            }

            blob->imax = imax0;

            if(imax0 > imax) {
                imax = imax0;
            }

            ++it;
        }

        it=blobs.begin();
        while(it!=blobs.end()) {
            Blob *blob=(*it).second;

            if ((blob->imax < imax)) {
                delete blob;

                Blobs::iterator tmp=it;
                ++it;
                blobs.erase(tmp);
            } else {
                ++it;
            }
        }
    }
}

void Labeler::Centroid(unsigned short* data, int width, int height, Blob* blob, int ext, double thresh1, double thresh2) {
	if (blob->area == 1)
	{
		blob->cx = (blob->minx + blob->maxx) / 2.0;
		blob->cy = (blob->miny + blob->maxy) / 2.0;
	}
	else
	{
		bool bValid = true;

		int telx = blob->minx - ext;
		int tely = blob->miny - ext;
		int terx = blob->maxx + ext;
		int tery = blob->maxy + ext;

		if (telx < 0) {
			telx = 0;
		}
		if (tely < 0) {
			tely = 0;
		}
		if (terx >= width) {
			terx = width - 1;
		}
		if (tery >= height) {
			tery = height - 1;
		}

		int w = terx - telx + 1;
		int h = tery - tely + 1;

		double avg = 0;
		double rms = 0;

		for (int y = tely; y <= tery; y++) {
			for (int x = telx; x <= terx; x++) {
				unsigned short val = data[y * width + x];

				avg += val;
				rms += val * val;
			}
		}

		avg /= (w * h);
		rms = rms / (w * h) - avg * avg;

		if (rms < 1) {   //no data
			//     return;
			bValid = false;
		}

		double threshval = avg + thresh1 * sqrt(rms);

		avg = 0;
		rms = 0;
		int bgcount = 0;

		for (int y = tely; y <= tery; y++) {
			for (int x = telx; x <= terx; x++) {
				unsigned short val = data[y * width + x];

				if (val < threshval) {
					avg += val;
					rms += val * val;

					bgcount++;
				}
			}
		}

		avg /= (bgcount);
		rms = rms / bgcount - avg * avg;
		if (rms < 0) {
			//    return;
			bValid = false;
		}

		double bgavg = avg;
		double bgrms = rms;

		blob->bgavg = bgavg;
		blob->bgrms = bgrms;

		double bgthreshval = bgavg + thresh2 * sqrt(bgrms);

		avg = 0;
		rms = 0;
		int starcount = 0;

		double xt = 0;
		double yt = 0;
		double grey = 0;

		for (int y = tely; y <= tery; y++) {
			for (int x = telx; x <= terx; x++) {
				unsigned short val = data[y * width + x];

				if (val >= bgthreshval) {
					avg += val - bgavg;
					rms += (val - bgavg) * (val - bgavg);
					grey += val;

					xt += (val)* x;
					yt += (val)* y;

					starcount++;
				}
			}
		}

		avg /= starcount;

		blob->snr = (avg) / sqrt(bgrms);
		blob->cx = xt / grey;
		blob->cy = yt / grey;
		blob->area0 = starcount;
		blob->avgval = avg;
		blob->rms = rms / starcount - avg * avg;


		if (bValid)
		{
            if (isnan(blob->cx) || isnan(blob->cy)) {
				blob->cx = (blob->minx + blob->maxx) / 2.0;
				blob->cy = (blob->miny + blob->maxy) / 2.0;
			}

			if (blob->cx < 1) {
				blob->cx = (blob->minx + blob->maxx) / 2.0;
			}

			if (blob->cy < 1) {
				blob->cy = (blob->miny + blob->maxy) / 2.0;
			}
		}
		else
		{
			blob->cx = (blob->minx + blob->maxx) / 2.0;
			blob->cy = (blob->miny + blob->maxy) / 2.0;
		}
	}	 
}

void Labeler::Centroids(unsigned short* data, int width, int height, Blobs &blobs, int ext, double thresh1, double thresh2) {
    for (Blobs::iterator it = blobs.begin(); it != blobs.end(); ++it) {
        Centroid(data, width, height, (*it).second, ext, thresh1, thresh2);
    }
}
