#pragma once

#include <wdm.h>
#include <wdf.h>

#define DEFAULT_SPB_BUFFER_SIZE 64
#define RESHUB_USE_HELPER_ROUTINES

//
// SPB (I2C) context
//

typedef struct _GPIO_CONTEXT
{
	WDFIOTARGET GpioIoTarget;
	LARGE_INTEGER GpioResHubId;
	WDFWAITLOCK GpioLock;
} GPIO_CONTEXT;

VOID
GpioTargetDeinitialize(
	IN WDFDEVICE FxDevice,
	IN GPIO_CONTEXT *GpioContext
);

NTSTATUS
GpioTargetInitialize(
	IN WDFDEVICE FxDevice,
	IN GPIO_CONTEXT *GpioContext
);

NTSTATUS
GpioWriteDataSynchronously(
	_In_ GPIO_CONTEXT* GpioContext,
	_In_ PVOID Data
);