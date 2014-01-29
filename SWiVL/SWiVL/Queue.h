/*++

Module Name:

    queue.h

Abstract:

    This file contains the queue definitions.

Environment:

    Kernel-mode Driver Framework

--*/

NTSTATUS SwivlQueueInitialize(_In_ WDFDEVICE Device);

//
// Events from the IoQueue object
//
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL SwivlEvtIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_STOP SwivlEvtIoStop;
