#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <CL/opencl.h>

class DeviceManager 
{
	/// 函数
public:
    DeviceManager();
    ~DeviceManager(void);
    char* LoadProgSource(const char* cFilename, const char* cPreamble, size_t* szFinalLength);
    void InitGPU(char* cPathAndName);

private:
    cl_int GetPlatformID(cl_platform_id* clSelectedPlatformID);
    void PrintPlatformInfo(cl_platform_id id, unsigned verbose);
    void PrintDeviceInfo(cl_device_id id, unsigned verbose);

	/// 变量
public:
	cl_uint uiUsefulDevs;	// Indexed list of devices worth using
	cl_platform_id cpPlatform;	//OpenCL platform
	cl_device_id* cdDevices;	//OpenCL device list
	cl_context cxGPUContext;	//OpenCL context
	cl_program cpProgram;	// OpenCL program
	cl_command_queue cqCommandQueue;	// OpenCL command queue array

private:
	cl_uint uiDevCount;	// total # of devices available to the platform
};

#endif // DEVICEMANAGER_H
