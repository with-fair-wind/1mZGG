#ifndef LABEL_H
#define LABEL_H

#include <map>
#include <omp.h>
#include <math.h>
#include <string.h>

#ifndef __linux__
#include "windows.h"
#else
#include "unistd.h"
#include "sys/sysinfo.h"
#endif

typedef struct {
    unsigned int minx;
    unsigned int miny;
    unsigned int maxx;
    unsigned int maxy;

    unsigned int area;
    unsigned int m10;
    unsigned int m01;

    double centroid_x;
    double centroid_y;

    unsigned int label;

    double bgavg;
    double bgrms;
    double snr;
    double cx;
    double cy;
    unsigned int area0;
    double avgval;
    double rms;

    unsigned short imax;
} Blob;

typedef std::map<unsigned int, Blob*> Blobs;
typedef std::pair<unsigned int,Blob *> LabelBlob;

class Labeler {
public:
    Labeler();
    virtual ~Labeler();

    unsigned int Label(unsigned char* img, int width, int height, Blobs &blobs, int roix, int roiy, int roiwidth, int roiheight, bool roi=false);
    unsigned int LabelEff(unsigned char* img, int width, int height, Blobs &blobs, int roix, int roiy, int roiwidth, int roiheight, bool roi=false);
    unsigned int LabelWithMask(unsigned char* img, unsigned char* mask, int width, int height, Blobs &blobs, int roix, int roiy, int roiwidth, int roiheight, bool roi = false);
    unsigned int LabelWithMaskEff(unsigned char* img, unsigned char* mask, int width, int height, Blobs &blobs, int roix, int roiy, int roiwidth, int roiheight, bool roi = false);
    void Centroids(unsigned short* data, int width, int height, Blobs &blobs, int ext, double thresh1, double thresh2);
    void Centroid(unsigned short* data, int width, int height, Blob* blob, int ext, double thresh1, double thresh2);

    void FilterByArea(Blobs &blobs, unsigned int minArea, unsigned int maxArea);
    void FilterByMaxIntensity(Blobs &blobs, unsigned short* udata, int width, int height);

    void ReleaseBlobs(Blobs &bs);
protected:

private:
    size_t m_width;
    size_t m_height;
    unsigned char* m_mask;

    size_t m_stack_size;
    size_t m_stack_pointer;
    size_t* m_stack;

	int m_iNumCPUCores;

    void Push(size_t val);
    size_t Pop();
    size_t Top();
};

#endif // LABEL_H
