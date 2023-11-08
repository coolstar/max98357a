#if !defined(_MAXM_H_)
#define _MAXM_H_

#pragma warning(disable:4200)  // suppress nameless struct/union warning
#pragma warning(disable:4201)  // suppress nameless struct/union warning
#pragma warning(disable:4214)  // suppress bit field types other than int warning
#include <initguid.h>
#include <wdm.h>

#pragma warning(default:4200)
#pragma warning(default:4201)
#pragma warning(default:4214)
#include <wdf.h>

#include <portcls.h>

#include <stdint.h>
#include "gpiowrapper.h"

//
// String definitions
//

#define DRIVERNAME                 "max98357a.sys: "

#define MAXM_POOL_TAG            (ULONG) 'mxaM'

#define NTDEVICE_NAME_STRING       L"\\Device\\MAX98357A"
#define SYMBOLIC_NAME_STRING       L"\\DosDevices\\MAX98357A"

#define true 1
#define false 0

typedef enum {
	CSAudioEndpointTypeDSP,
	CSAudioEndpointTypeSpeaker,
	CSAudioEndpointTypeHeadphone,
	CSAudioEndpointTypeMicArray,
	CSAudioEndpointTypeMicJack
} CSAudioEndpointType;

typedef enum {
	CSAudioEndpointRegister,
	CSAudioEndpointStart,
	CSAudioEndpointStop,
	CSAudioEndpointOverrideFormat
} CSAudioEndpointRequest;

typedef struct CSAUDIOFORMATOVERRIDE {
	UINT16 channels;
	UINT16 frequency;
	UINT16 bitsPerSample;
	UINT16 validBitsPerSample;
	BOOL force32BitOutputContainer;
} CsAudioFormatOverride;

typedef struct CSAUDIOARG {
	UINT32 argSz;
	CSAudioEndpointType endpointType;
	CSAudioEndpointRequest endpointRequest;
	union {
		CsAudioFormatOverride formatOverride;
	};
} CsAudioArg, * PCsAudioArg;

typedef struct _MAXM_CONTEXT
{

	//
	// Handle back to the WDFDEVICE
	//

	WDFDEVICE FxDevice;

	WDFQUEUE ReportQueue;

	GPIO_CONTEXT SdmodeGpioContext;

	BOOLEAN DevicePoweredOn;

	WDFWORKITEM CSAudioWorkItem;
	PCALLBACK_OBJECT CSAudioAPICallback;
	PVOID CSAudioAPICallbackObj;

	BOOLEAN CSAudioRequestsOn;

} MAXM_CONTEXT, *PMAXM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(MAXM_CONTEXT, GetDeviceContext)

//
// Function definitions
//

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_UNLOAD MaxmDriverUnload;

EVT_WDF_DRIVER_DEVICE_ADD MaxmEvtDeviceAdd;

EVT_WDFDEVICE_WDM_IRP_PREPROCESS MaxmEvtWdmPreprocessMnQueryId;

EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL MaxmEvtInternalDeviceControl;

//
// Helper macros
//

#define DEBUG_LEVEL_ERROR   1
#define DEBUG_LEVEL_INFO    2
#define DEBUG_LEVEL_VERBOSE 3

#define DBG_INIT  1
#define DBG_PNP   2
#define DBG_IOCTL 4

#if 0
#define MaxmPrint(dbglevel, dbgcatagory, fmt, ...) {          \
    if (MaxmDebugLevel >= dbglevel &&                         \
        (MaxmDebugCatagories && dbgcatagory))                 \
		    {                                                           \
        DbgPrint(DRIVERNAME);                                   \
        DbgPrint(fmt, __VA_ARGS__);                             \
		    }                                                           \
}
#else
#define MaxmPrint(dbglevel, fmt, ...) {                       \
}
#endif
#endif