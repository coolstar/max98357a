#define DESCRIPTOR_DEF
#include "driver.h"
#include "stdint.h"

#define bool int
#define MHz 1000000

static ULONG MaxmDebugLevel = 100;
static ULONG MaxmDebugCatagories = DBG_INIT || DBG_PNP || DBG_IOCTL;

NTSTATUS
DriverEntry(
__in PDRIVER_OBJECT  DriverObject,
__in PUNICODE_STRING RegistryPath
)
{
	NTSTATUS               status = STATUS_SUCCESS;
	WDF_DRIVER_CONFIG      config;
	WDF_OBJECT_ATTRIBUTES  attributes;

	MaxmPrint(DEBUG_LEVEL_INFO, DBG_INIT,
		"Driver Entry\n");

	WDF_DRIVER_CONFIG_INIT(&config, MaxmEvtDeviceAdd);

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

	//
	// Create a framework driver object to represent our driver.
	//

	status = WdfDriverCreate(DriverObject,
		RegistryPath,
		&attributes,
		&config,
		WDF_NO_HANDLE
		);

	if (!NT_SUCCESS(status))
	{
		MaxmPrint(DEBUG_LEVEL_ERROR, DBG_INIT,
			"WdfDriverCreate failed with status 0x%x\n", status);
	}

	return status;
}

int IntCSSTArg2 = 1;

VOID
UpdateIntcSSTStatus(
	IN PMAXM_CONTEXT pDevice,
	int sstStatus
) {
	IntcSSTArg *SSTArg = &pDevice->sstArgTemp;
	RtlZeroMemory(SSTArg, sizeof(IntcSSTArg));

	if (pDevice->IntcSSTHwMultiCodecCallback) {
		if (sstStatus != 1 || pDevice->IntcSSTStatus) {
			SSTArg->chipModel = 98357;
			SSTArg->caller = 0xc0000165; //gmaxcodec
			if (sstStatus) {
				if (sstStatus == 1) {
					if (!pDevice->IntcSSTStatus) {
						return;
					}
					SSTArg->sstQuery = 12;
					SSTArg->dword11 = 2;
					SSTArg->querySize = 21;
				}
				else {
					SSTArg->sstQuery = 11;
					SSTArg->querySize = 20;
				}

				SSTArg->deviceInD0 = (pDevice->DevicePoweredOn != 0);
			}
			else {
				SSTArg->sstQuery = 10;
				SSTArg->querySize = 18;
				SSTArg->deviceInD0 = 1;
			}
			ExNotifyCallback(pDevice->IntcSSTHwMultiCodecCallback, SSTArg, &IntCSSTArg2);
		}
	}
}

VOID
IntcSSTWorkItemFunc(
	IN WDFWORKITEM  WorkItem
)
{
	WDFDEVICE Device = (WDFDEVICE)WdfWorkItemGetParentObject(WorkItem);
	PMAXM_CONTEXT pDevice = GetDeviceContext(Device);

	UpdateIntcSSTStatus(pDevice, 0);
}

DEFINE_GUID(GUID_SST_RTK_1,
	0xDFF21CE2, 0xF70F, 0x11D0, 0xB9, 0x17, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96); //Headphones
DEFINE_GUID(GUID_SST_RTK_2,
	0xDFF21CE1, 0xF70F, 0x11D0, 0xB9, 0x17, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96); //InsideMobileLid
DEFINE_GUID(GUID_SST_RTK_3,
	0xDFF21BE1, 0xF70F, 0x11D0, 0xB9, 0x17, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96); //Also InsideMobileLid?
DEFINE_GUID(GUID_SST_RTK_4,
	0xDFF21FE3, 0xF70F, 0x11D0, 0xB9, 0x17, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96); //Line out

VOID
IntcSSTCallbackFunction(
	IN WDFWORKITEM  WorkItem,
	IntcSSTArg *SSTArgs,
	PVOID Argument2
) {
	if (!WorkItem) {
		return;
	}
	WDFDEVICE Device = (WDFDEVICE)WdfWorkItemGetParentObject(WorkItem);
	PMAXM_CONTEXT pDevice = GetDeviceContext(Device);

	if (Argument2 == &IntCSSTArg2) {
		return;
	}

	//gmaxCodec checks that querySize is greater than 0x10 first thing
	if (SSTArgs->querySize <= 0x10) {
		return;
	}

	//Intel Caller: 0xc00000a3 (STATUS_DEVICE_NOT_READY)
	//GMax Caller: 0xc0000165

	if (SSTArgs->chipModel == 98357) {
		/*

		Gmax (no SST driver):
			init:	sstQuery = 10
					dwordc = 18
					deviceInD0 = 1

			stop:	sstQuery = 11
					dwordc = 20
					deviceInD0 = 0
		*/

		/*
			
		Gmax (SST driver)
			post-init:	sstQuery = 12
						dwordc = 21
						dword11 = 2

		*/
		if (Argument2 != &IntCSSTArg2) { //Intel SST is calling us
			bool checkCaller = (SSTArgs->caller != 0);

			if (SSTArgs->sstQuery == 11) {
				if (SSTArgs->querySize >= 0x15) {
					if (SSTArgs->deviceInD0 == 0) {
						pDevice->IntcSSTStatus = 0; //SST is inactive
						SSTArgs->caller = STATUS_SUCCESS;
						//mark device as inactive?
					}
					else {
						SSTArgs->caller = STATUS_INVALID_PARAMETER_5;
					}
				}
				else {
					SSTArgs->caller = STATUS_BUFFER_TOO_SMALL;
				}
			}

			//SST Query 1:
			//	sstQuery: 10, querySize: 0x9e, dword11: 0x0
			//	deviceInD0: 0x1, byte25: 0

			if (SSTArgs->sstQuery == 10) { //gmax responds no matter what
				if (SSTArgs->querySize >= 0x15) {
					if (SSTArgs->deviceInD0 == 1) {
						pDevice->IntcSSTStatus = 1;
						SSTArgs->caller = STATUS_SUCCESS;
						//mark device as active??
					}
					else {
						SSTArgs->caller = STATUS_INVALID_PARAMETER_5;
					}
				}
				else {
					SSTArgs->caller = STATUS_BUFFER_TOO_SMALL;
				}
			}

			//SST Query 2:
			//	sstQuery: 2048, querySize: 0x9e, dword11: 0x00
			//	deviceInD0: 0, byte25: 0

			if (SSTArgs->sstQuery == 2048) {
				if (SSTArgs->querySize >= 0x11) {
					SSTArgs->deviceInD0 = 1;
					SSTArgs->caller = STATUS_SUCCESS;
				}
				else {
					SSTArgs->caller = STATUS_BUFFER_TOO_SMALL;
				}
			}

			//SST Query 3:
			//	sstQuery: 2051, querySize: 0x9e, dword11: 0x00
			//	deviceInD0: 0, byte25: 0

			if (SSTArgs->sstQuery == 2051) {
				if (SSTArgs->querySize >= 0x9E) {
					if (SSTArgs->deviceInD0) {
						SSTArgs->caller = STATUS_INVALID_PARAMETER;
					} else {
						
						SSTArgs->deviceInD0 = 0;
						SSTArgs->dword11 = (1 << 24) | 0;

						SSTArgs->guid = GUID_SST_RTK_2;

						SSTArgs->byte25 = 1;
						SSTArgs->dword26 = KSAUDIO_SPEAKER_STEREO; //Channel Mapping
						SSTArgs->dword2A = JACKDESC_RGB(255, 174, 201); //Color (gmax sets to 0)
						SSTArgs->dword2E = eConnTypeOtherAnalog; //EPcxConnectionType
						SSTArgs->dword32 = eGeoLocInsideMobileLid; //EPcxGeoLocation
						SSTArgs->dword36 = eGenLocInternal; //genLocation?
						SSTArgs->dword3A = ePortConnIntegratedDevice; //portConnection?
						SSTArgs->dword3E = 1; //isConnected?
						SSTArgs->byte42 = 0;
						SSTArgs->byte43 = 0;
						SSTArgs->caller = STATUS_SUCCESS;
					}
				}
				else {
					SSTArgs->caller = STATUS_BUFFER_TOO_SMALL;
				}
			}

			//This is the minimum for SST to initialize. Everything after is extra
			//SST Query 4:
			//	sstQuery: 2054, querySize: 0x9e, dword11: 0x00
			//	deviceInD0: 0, byte25: 0
			if (SSTArgs->sstQuery == 2054) {
				if (SSTArgs->querySize >= 0x9E) {
					if (SSTArgs->deviceInD0) {
						SSTArgs->caller = STATUS_INVALID_PARAMETER;
					}
					else {
						SSTArgs->dword11 = 2;
						SSTArgs->caller = STATUS_SUCCESS;
					}
				}
				else {
					SSTArgs->caller = STATUS_BUFFER_TOO_SMALL;
				}
			}

			//SST Query 5:
			//	sstQuery: 2055, querySize: 0x9e, dword11: 0x00
			//	deviceInD0: 0, byte25: 0

			if (SSTArgs->sstQuery == 2055) {
				if (SSTArgs->querySize < 0x22) {
					SSTArgs->caller = STATUS_BUFFER_TOO_SMALL;
				}
				else {
					SSTArgs->caller = STATUS_NOT_SUPPORTED;
				}
			}

			//SST Query 6:
			//	sstQuery: 13, querySize: 0x9e, dword11: 0x00
			//	deviceInD0: 1, byte25: 0
			if (SSTArgs->sstQuery == 13) {
				if (SSTArgs->querySize >= 0x14) {
					if (SSTArgs->deviceInD0) {
						pDevice->IntcSSTStatus = 1;
						SSTArgs->caller = STATUS_SUCCESS;

						//UpdateIntcSSTStatus(pDevice, 1);
					}
					else {
						SSTArgs->caller = STATUS_INVALID_PARAMETER;
					}
				}
				else {
					SSTArgs->caller = STATUS_BUFFER_TOO_SMALL;
				}
			}

			//SST Query 7:
			//	sstQuery: 2064, querySize: 0x9e, dword11: 0x00
			//	deviceInD0: 0, byte25: 0
			if (SSTArgs->sstQuery == 2064) {
				if (SSTArgs->querySize >= 0x19) {
					if (!SSTArgs->deviceInD0) {
						unsigned int data1 = SSTArgs->guid.Data1;
						//DbgPrint("data1: %d\n", data1);
						if (data1 != -1 && data1 < 1) {
							SSTArgs->dword11 = 0; //no feedback on max98357a
							SSTArgs->caller = STATUS_SUCCESS;
						}
						else {
							SSTArgs->caller = STATUS_INVALID_PARAMETER;
						}
					}
					else {
						SSTArgs->caller = STATUS_INVALID_PARAMETER;
					}
				}
				else {
					SSTArgs->caller = STATUS_BUFFER_TOO_SMALL;
				}
			}

			if (checkCaller) {
				if (SSTArgs->caller != STATUS_SUCCESS) {
					//DbgPrint("Warning: Returned error 0x%x; query: %d\n", SSTArgs->caller, SSTArgs->sstQuery);
				}
			}
		}
	}
	else {
		//On SST Init: chipModel = 0, caller = 0xc00000a3, sstQuery = 10, dwordc: 0x9e

		if (SSTArgs->sstQuery == 10 && pDevice->IntcSSTWorkItem) {
			WdfWorkItemEnqueue(pDevice->IntcSSTWorkItem); //SST driver was installed after us...
		}
	}
}

NTSTATUS
OnPrepareHardware(
_In_  WDFDEVICE     FxDevice,
_In_  WDFCMRESLIST  FxResourcesRaw,
_In_  WDFCMRESLIST  FxResourcesTranslated
)
/*++

Routine Description:

This routine caches the SPB resource connection ID.

Arguments:

FxDevice - a handle to the framework device object
FxResourcesRaw - list of translated hardware resources that
the PnP manager has assigned to the device
FxResourcesTranslated - list of raw hardware resources that
the PnP manager has assigned to the device

Return Value:

Status

--*/
{
	PMAXM_CONTEXT pDevice = GetDeviceContext(FxDevice);
	BOOLEAN fSdmodeGpioResourceFound = FALSE;
	NTSTATUS status = STATUS_INSUFFICIENT_RESOURCES;

	UNREFERENCED_PARAMETER(FxResourcesRaw);

	//
	// Parse the peripheral's resources.
	//

	ULONG resourceCount = WdfCmResourceListGetCount(FxResourcesTranslated);

	for (ULONG i = 0; i < resourceCount; i++)
	{
		PCM_PARTIAL_RESOURCE_DESCRIPTOR pDescriptor;
		UCHAR Class;
		UCHAR Type;

		pDescriptor = WdfCmResourceListGetDescriptor(
			FxResourcesTranslated, i);

		switch (pDescriptor->Type)
		{
		case CmResourceTypeConnection:
			//
			// Look for I2C or SPI resource and save connection ID.
			//
			Class = pDescriptor->u.Connection.Class;
			Type = pDescriptor->u.Connection.Type;
			if (Class == CM_RESOURCE_CONNECTION_CLASS_GPIO && Type == CM_RESOURCE_CONNECTION_TYPE_GPIO_IO) {
				if (fSdmodeGpioResourceFound == FALSE) {
					pDevice->SdmodeGpioContext.GpioResHubId.LowPart = pDescriptor->u.Connection.IdLowPart;
					pDevice->SdmodeGpioContext.GpioResHubId.HighPart = pDescriptor->u.Connection.IdHighPart;
					fSdmodeGpioResourceFound = TRUE;
				}
			}
			break;
		default:
			//
			// Ignoring all other resource types.
			//
			break;
		}
	}

	//
	// An SPB resource is required.
	//

	if (fSdmodeGpioResourceFound == FALSE)
	{
		status = STATUS_NOT_FOUND;
		return status;
	}

	status = GpioTargetInitialize(FxDevice, &pDevice->SdmodeGpioContext);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	WDF_OBJECT_ATTRIBUTES attributes;
	WDF_WORKITEM_CONFIG workitemConfig;

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&attributes, MAXM_CONTEXT);
	attributes.ParentObject = FxDevice;
	WDF_WORKITEM_CONFIG_INIT(&workitemConfig, IntcSSTWorkItemFunc);
	status = WdfWorkItemCreate(&workitemConfig,
		&attributes,
		&pDevice->IntcSSTWorkItem);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	return status;
}

NTSTATUS
OnReleaseHardware(
_In_  WDFDEVICE     FxDevice,
_In_  WDFCMRESLIST  FxResourcesTranslated
)
/*++

Routine Description:

Arguments:

FxDevice - a handle to the framework device object
FxResourcesTranslated - list of raw hardware resources that
the PnP manager has assigned to the device

Return Value:

Status

--*/
{
	PMAXM_CONTEXT pDevice = GetDeviceContext(FxDevice);
	NTSTATUS status = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(FxResourcesTranslated);

	UpdateIntcSSTStatus(pDevice, 2);

	GpioTargetDeinitialize(FxDevice, &pDevice->SdmodeGpioContext);

	if (pDevice->IntcSSTCallbackObj) {
		ExUnregisterCallback(pDevice->IntcSSTCallbackObj);
		pDevice->IntcSSTCallbackObj = NULL;
	}

	if (pDevice->IntcSSTWorkItem) {
		WdfWorkItemFlush(pDevice->IntcSSTWorkItem);
		WdfObjectDelete(pDevice->IntcSSTWorkItem);
		pDevice->IntcSSTWorkItem = NULL;
	}

	if (pDevice->IntcSSTHwMultiCodecCallback) {
		ObfDereferenceObject(pDevice->IntcSSTHwMultiCodecCallback);
		pDevice->IntcSSTHwMultiCodecCallback = NULL;
	}

	return status;
}

NTSTATUS
OnSelfManagedIoInit(
	_In_
	WDFDEVICE FxDevice
) {
	PMAXM_CONTEXT pDevice = GetDeviceContext(FxDevice);
	NTSTATUS status = STATUS_SUCCESS;

	UNICODE_STRING IntcAudioSSTMultiHwCodecAPI;
	RtlInitUnicodeString(&IntcAudioSSTMultiHwCodecAPI, L"\\CallBack\\IntcAudioSSTMultiHwCodecAPI");


	OBJECT_ATTRIBUTES attributes;
	InitializeObjectAttributes(&attributes,
		&IntcAudioSSTMultiHwCodecAPI,
		OBJ_KERNEL_HANDLE | OBJ_OPENIF | OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
		NULL,
		NULL
	);
	status = ExCreateCallback(&pDevice->IntcSSTHwMultiCodecCallback, &attributes, TRUE, TRUE);
	if (!NT_SUCCESS(status)) {

		return status;
	}

	pDevice->IntcSSTCallbackObj = ExRegisterCallback(pDevice->IntcSSTHwMultiCodecCallback,
		IntcSSTCallbackFunction,
		pDevice->IntcSSTWorkItem
	);
	if (!pDevice->IntcSSTCallbackObj) {

		return STATUS_NO_CALLBACK_ACTIVE;
	}

	UpdateIntcSSTStatus(pDevice, 0);

	return status;
}

NTSTATUS
OnD0Entry(
_In_  WDFDEVICE               FxDevice,
_In_  WDF_POWER_DEVICE_STATE  FxPreviousState
)
/*++

Routine Description:

This routine allocates objects needed by the driver.

Arguments:

FxDevice - a handle to the framework device object
FxPreviousState - previous power state

Return Value:

Status

--*/
{
	UNREFERENCED_PARAMETER(FxPreviousState);

	PMAXM_CONTEXT pDevice = GetDeviceContext(FxDevice);
	NTSTATUS status = STATUS_SUCCESS;

	unsigned char gpio_data;
	gpio_data = 1;
	status =  GpioWriteDataSynchronously(&pDevice->SdmodeGpioContext, &gpio_data);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	pDevice->DevicePoweredOn = TRUE;

	return status;
}

NTSTATUS
OnD0Exit(
_In_  WDFDEVICE               FxDevice,
_In_  WDF_POWER_DEVICE_STATE  FxPreviousState
)
/*++

Routine Description:

This routine destroys objects needed by the driver.

Arguments:

FxDevice - a handle to the framework device object
FxPreviousState - previous power state

Return Value:

Status

--*/
{
	UNREFERENCED_PARAMETER(FxPreviousState);

	PMAXM_CONTEXT pDevice = GetDeviceContext(FxDevice);
	NTSTATUS status = STATUS_SUCCESS;

	unsigned char gpio_data;
	gpio_data = 0;
	status = GpioWriteDataSynchronously(&pDevice->SdmodeGpioContext, &gpio_data);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	pDevice->DevicePoweredOn = FALSE;

	return STATUS_SUCCESS;
}

NTSTATUS
MaxmEvtDeviceAdd(
IN WDFDRIVER       Driver,
IN PWDFDEVICE_INIT DeviceInit
)
{
	NTSTATUS                      status = STATUS_SUCCESS;
	WDF_IO_QUEUE_CONFIG           queueConfig;
	WDF_OBJECT_ATTRIBUTES         attributes;
	WDFDEVICE                     device;
	WDF_INTERRUPT_CONFIG interruptConfig;
	WDFQUEUE                      queue;
	UCHAR                         minorFunction;
	PMAXM_CONTEXT               devContext;

	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	MaxmPrint(DEBUG_LEVEL_INFO, DBG_PNP,
		"MaxmEvtDeviceAdd called\n");

	{
		WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
		WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);

		pnpCallbacks.EvtDevicePrepareHardware = OnPrepareHardware;
		pnpCallbacks.EvtDeviceReleaseHardware = OnReleaseHardware;
		pnpCallbacks.EvtDeviceSelfManagedIoInit = OnSelfManagedIoInit;
		pnpCallbacks.EvtDeviceD0Entry = OnD0Entry;
		pnpCallbacks.EvtDeviceD0Exit = OnD0Exit;

		WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);
	}

	//
	// Setup the device context
	//

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, MAXM_CONTEXT);

	//
	// Create a framework device object.This call will in turn create
	// a WDM device object, attach to the lower stack, and set the
	// appropriate flags and attributes.
	//

	status = WdfDeviceCreate(&DeviceInit, &attributes, &device);

	if (!NT_SUCCESS(status))
	{
		MaxmPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfDeviceCreate failed with status code 0x%x\n", status);

		return status;
	}

	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);

	queueConfig.EvtIoInternalDeviceControl = MaxmEvtInternalDeviceControl;

	status = WdfIoQueueCreate(device,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&queue
		);

	if (!NT_SUCCESS(status))
	{
		MaxmPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfIoQueueCreate failed 0x%x\n", status);

		return status;
	}

	//
	// Create manual I/O queue to take care of hid report read requests
	//

	devContext = GetDeviceContext(device);

	WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);

	queueConfig.PowerManaged = WdfFalse;

	status = WdfIoQueueCreate(device,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&devContext->ReportQueue
		);

	if (!NT_SUCCESS(status))
	{
		MaxmPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfIoQueueCreate failed 0x%x\n", status);

		return status;
	}

	devContext->FxDevice = device;

	return status;
}

VOID
MaxmEvtInternalDeviceControl(
IN WDFQUEUE     Queue,
IN WDFREQUEST   Request,
IN size_t       OutputBufferLength,
IN size_t       InputBufferLength,
IN ULONG        IoControlCode
)
{
	NTSTATUS            status = STATUS_SUCCESS;
	WDFDEVICE           device;
	PMAXM_CONTEXT     devContext;

	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(InputBufferLength);

	device = WdfIoQueueGetDevice(Queue);
	devContext = GetDeviceContext(device);

	switch (IoControlCode)
	{
	default:
		status = STATUS_NOT_SUPPORTED;
		break;
	}

	WdfRequestComplete(Request, status);

	return;
}
