#include "DeviceManager.h"

DeviceManager::DeviceManager() 
{
    uiDevCount = 0;
    cpProgram = NULL;
    cxGPUContext = NULL;
    cqCommandQueue = NULL;

    cl_int err = GetPlatformID(&cpPlatform);

    //PrintPlatformInfo(cpPlatform, 1);

    // Get the number of GPU devices available to the platform
    err = clGetDeviceIDs(cpPlatform, CL_DEVICE_TYPE_GPU, 0, NULL, &uiDevCount);

    // Create the device list
    cdDevices = new cl_device_id [uiDevCount];
    clGetDeviceIDs(cpPlatform, CL_DEVICE_TYPE_GPU, uiDevCount, cdDevices, NULL);
    //for(int i = 0 ; i < uiDevCount ; i++) {
    //    PrintDeviceInfo(cdDevices[i], 0);
    //}

    // Allocations for perfs, loads and useful devices
    uiUsefulDevs = 0;
}

#include <QDebug>
DeviceManager::~DeviceManager() 
{
    if(cpProgram)clReleaseProgram(cpProgram);

    if(cxGPUContext)clReleaseContext(cxGPUContext);

    if(cqCommandQueue) clReleaseCommandQueue(cqCommandQueue);

    if (cdDevices)  delete [] cdDevices;

    qDebug() << "DeviceManager";
}

cl_int DeviceManager::GetPlatformID(cl_platform_id* clSelectedPlatformID) 
{
    char chBuffer[1024];
    cl_uint num_platforms = 0;
    cl_platform_id* clPlatformIDs;
    cl_int ciErrNum;
    *clSelectedPlatformID = NULL;

    // Get OpenCL platform count
    ciErrNum = clGetPlatformIDs (0, NULL, &num_platforms);
    if (ciErrNum != CL_SUCCESS) 
	{
        return -1000;
    } 
	else 
	{
        if(num_platforms == 0) 
		{
            return -2000;
        } 
		else 
		{
            // if there's a platform or more, make space for ID's
            if ((clPlatformIDs = (cl_platform_id*)malloc(num_platforms * sizeof(cl_platform_id))) == NULL) 
			{
                return -3000;
            }

            // get platform info for each platform and trap the NVIDIA platform if found
            ciErrNum = clGetPlatformIDs (num_platforms, clPlatformIDs, NULL);
            for(cl_uint i = 0; i < num_platforms; ++i) 
			{
                ciErrNum = clGetPlatformInfo (clPlatformIDs[i], CL_PLATFORM_NAME, 1024, &chBuffer, NULL);
                if(ciErrNum == CL_SUCCESS) 
				{
                    if(strstr(chBuffer, "NVIDIA") != NULL) 
					{
                        *clSelectedPlatformID = clPlatformIDs[i];
                        break;
                    }
                }
            }

            // default to zeroeth platform if NVIDIA not found
            if(*clSelectedPlatformID == NULL) 
			{
                *clSelectedPlatformID = clPlatformIDs[0];
            }

            free(clPlatformIDs);
        }
    }

    return CL_SUCCESS;
}

void DeviceManager::InitGPU(char* cPathAndName) 
{
    cl_int ciErrNum;			        // Error code var

    cxGPUContext = clCreateContext(0, uiDevCount, cdDevices, NULL, NULL, &ciErrNum);

	cqCommandQueue = clCreateCommandQueue(cxGPUContext, cdDevices[uiUsefulDevs], 0, &ciErrNum);
	//cqCommandQueue = clCreateCommandQueueWithProperties(cxGPUContext, cdDevices[uiUsefulDevs], 0, &ciErrNum);	// opencl 2.0取消了clCreateCommandQueue

    size_t szKernelLength = 0;
    char* cSourceCL = LoadProgSource(cPathAndName, "// My comment\n", &szKernelLength);

    cpProgram = clCreateProgramWithSource(cxGPUContext, 1, (const char **)&cSourceCL, &szKernelLength, &ciErrNum);

    const char *flags = "-cl-mad-enable -cl-fast-relaxed-math";

    ciErrNum = clBuildProgram(cpProgram, 0, NULL, flags, NULL, NULL);
    if(ciErrNum != CL_SUCCESS) 
	{
        size_t len;
        char   buffer[10000];

        clGetProgramBuildInfo(cpProgram, cdDevices[0], CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
		printf("%s\n", buffer);
    }

    delete cSourceCL;
};

//////////////////////////////////////////////////////////////////////////////
//! Loads a Program file and prepends the cPreamble to the code.
//!
//! @return the source string if succeeded, 0 otherwise
//! @param cFilename        program filename
//! @param cPreamble        code that is prepended to the loaded file, typically a set of #defines or a header
//! @param szFinalLength    returned length of the code string
//////////////////////////////////////////////////////////////////////////////
char* DeviceManager::LoadProgSource(const char* cFilename, const char* cPreamble, size_t* szFinalLength) 
{
    // locals
    FILE* pFileStream = NULL;
    size_t szSourceLength;

    // open the OpenCL source code file
#ifdef _WIN32   // Windows version
    if(fopen_s(&pFileStream, cFilename, "rb") != 0) 
	{
        return NULL;
    }
#else           // Linux version
    pFileStream = fopen(cFilename, "rb");
    if(pFileStream == 0) 
	{
        return NULL;
    }
#endif

    size_t szPreambleLength = strlen(cPreamble);

    // get the length of the source code
    fseek(pFileStream, 0, SEEK_END);
    szSourceLength = ftell(pFileStream);
    fseek(pFileStream, 0, SEEK_SET);

    // allocate a buffer for the source code string and read it in
    char* cSourceString = (char *)malloc(szSourceLength + szPreambleLength + 1);
    memcpy(cSourceString, cPreamble, szPreambleLength);
    if (fread((cSourceString) + szPreambleLength, szSourceLength, 1, pFileStream) != 1) 
	{
        fclose(pFileStream);
        free(cSourceString);
        return 0;
    }

    // close the file and return the total length of the combined (preamble + source) string
    fclose(pFileStream);
    if(szFinalLength != 0) 
	{
        *szFinalLength = szSourceLength + szPreambleLength;
    }
    cSourceString[szSourceLength + szPreambleLength] = '\0';

    return cSourceString;
}

void DeviceManager::PrintPlatformInfo(cl_platform_id id, unsigned verbose) 
{
    cl_uint err;
    char    str[256];

    err = clGetPlatformInfo(id, CL_PLATFORM_NAME, sizeof(str), str, NULL);

    err = clGetPlatformInfo(id, CL_PLATFORM_VERSION, sizeof(str), str, NULL);

    if (verbose) 
	{
        err = clGetPlatformInfo(id, CL_PLATFORM_EXTENSIONS, sizeof(str), str, NULL);
    }
};

void DeviceManager::PrintDeviceInfo(cl_device_id id, unsigned verbose) 
{
    cl_uint        err;
    cl_ulong       size = 0;
    cl_device_type type;
    char           str[256];

    err = clGetDeviceInfo(id, CL_DEVICE_NAME, sizeof(str), str, NULL);

    err = clGetDeviceInfo(id, CL_DEVICE_VERSION, sizeof(str), str, NULL);

    err = clGetDeviceInfo(id, CL_DEVICE_TYPE, sizeof(type), &type, NULL);

    err = clGetDeviceInfo(id, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(size), &size, NULL);

    if (verbose) 
	{
        err = clGetDeviceInfo(id, CL_DEVICE_EXTENSIONS, sizeof(str), str, NULL);
    }
}
