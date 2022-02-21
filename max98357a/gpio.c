#include "driver.h"
#include "gpiowrapper.h"
#include <gpio.h>
#include <reshub.h>

static ULONG MaxmDebugLevel = 100;
static ULONG MaxmDebugCatagories = DBG_INIT || DBG_PNP || DBG_IOCTL;

NTSTATUS
GpioWriteDataSynchronously(
	_In_ GPIO_CONTEXT *GpioContext,
	_In_ PVOID Data
)
/*++

Routine Description:

This helper routine abstracts creating and sending an I/O
request (I2C Read) to the Spb I/O target.

Arguments:

SpbContext - Pointer to the current device context
Data       - A buffer to receive the data at at the above address
Length     - The amount of data to be read from the above address

Return Value:

NTSTATUS Status indicating success or failure

--*/
{
	WDF_MEMORY_DESCRIPTOR inputDescriptor, outputDescriptor;
	NTSTATUS status;

	WdfWaitLockAcquire(GpioContext->GpioLock, NULL);

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputDescriptor, Data, 1);
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor, Data, 1);

	status = WdfIoTargetSendIoctlSynchronously(GpioContext->GpioIoTarget, NULL, IOCTL_GPIO_WRITE_PINS, &inputDescriptor, &outputDescriptor, NULL, NULL);

	WdfWaitLockRelease(GpioContext->GpioLock);

	return status;
}

VOID
GpioTargetDeinitialize(
	IN WDFDEVICE FxDevice,
	IN GPIO_CONTEXT *GpioContext
)
/*++

Routine Description:

This helper routine is used to free any members added to the SPB_CONTEXT,
note the SPB I/O target is parented to the device and will be
closed and free'd when the device is removed.

Arguments:

FxDevice   - Handle to the framework device object
SpbContext - Pointer to the current device context

Return Value:

NTSTATUS Status indicating success or failure

--*/
{
	UNREFERENCED_PARAMETER(FxDevice);
	UNREFERENCED_PARAMETER(GpioContext);

	//
	// Free any SPB_CONTEXT allocations here
	//
	if (GpioContext->GpioLock != NULL)
	{
		WdfObjectDelete(GpioContext->GpioLock);
	}
}

NTSTATUS
GpioTargetInitialize(
	IN WDFDEVICE FxDevice,
	IN GPIO_CONTEXT *GpioContext
)
/*++

Routine Description:

This helper routine opens the Spb I/O target and
initializes a request object used for the lifetime
of communication between this driver and Spb.

Arguments:

FxDevice   - Handle to the framework device object
SpbContext - Pointer to the current device context

Return Value:

NTSTATUS Status indicating success or failure

--*/
{
	WDF_OBJECT_ATTRIBUTES objectAttributes;
	WDF_IO_TARGET_OPEN_PARAMS openParams;
	UNICODE_STRING gpioDeviceName;
	WCHAR gpioDeviceNameBuffer[RESOURCE_HUB_PATH_SIZE];
	NTSTATUS status;

	WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
	objectAttributes.ParentObject = FxDevice;

	status = WdfIoTargetCreate(
		FxDevice,
		&objectAttributes,
		&GpioContext->GpioIoTarget);

	if (!NT_SUCCESS(status))
	{
		MaxmPrint(
			DEBUG_LEVEL_ERROR,
			DBG_IOCTL,
			"Error creating IoTarget object - %!STATUS!",
			status);

		WdfObjectDelete(GpioContext->GpioIoTarget);
		goto exit;
	}

	RtlInitEmptyUnicodeString(
		&gpioDeviceName,
		gpioDeviceNameBuffer,
		sizeof(gpioDeviceNameBuffer));

	status = RESOURCE_HUB_CREATE_PATH_FROM_ID(
		&gpioDeviceName,
		GpioContext->GpioResHubId.LowPart,
		GpioContext->GpioResHubId.HighPart);

	if (!NT_SUCCESS(status))
	{
		MaxmPrint(
			DEBUG_LEVEL_ERROR,
			DBG_IOCTL,
			"Error creating GPIO resource hub path string - %!STATUS!",
			status);
		goto exit;
	}

	WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(
		&openParams,
		&gpioDeviceName,
		(FILE_GENERIC_WRITE));

	status = WdfIoTargetOpen(GpioContext->GpioIoTarget, &openParams);

	if (!NT_SUCCESS(status))
	{
		MaxmPrint(
			DEBUG_LEVEL_ERROR,
			DBG_IOCTL,
			"Error opening GPIO target for communication - %!STATUS!",
			status);
		goto exit;
	}

	//
	// Allocate a waitlock to guard access to the default buffers
	//
	status = WdfWaitLockCreate(
		WDF_NO_OBJECT_ATTRIBUTES,
		&GpioContext->GpioLock);

	if (!NT_SUCCESS(status))
	{
		MaxmPrint(
			DEBUG_LEVEL_ERROR,
			DBG_IOCTL,
			"Error creating Spb Waitlock - %!STATUS!",
			status);
		goto exit;
	}

exit:

	if (!NT_SUCCESS(status))
	{
		GpioTargetDeinitialize(FxDevice, GpioContext);
	}

	return status;
}