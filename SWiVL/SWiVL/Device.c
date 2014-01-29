/*++

Module Name:

    device.c - Device handling events for example driver.

Abstract:

   This file contains the device entry points and callbacks.
    
Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "device.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SwivlCreateDevice)
#endif

/*++

Routine Description:

    Worker routine called to create a device and its software resources.

Arguments:

    DeviceInit - Pointer to an opaque init structure. Memory for this
                    structure will be freed by the framework when the WdfDeviceCreate
                    succeeds. So don't access the structure after that point.

Return Value:

    NTSTATUS

--*/
NTSTATUS SwivlCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit)
{
    WDFDEVICE device;
    NTSTATUS status;

    PAGED_CODE();

    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_HYPERVISOR);

    status = WdfDeviceCreate(&DeviceInit, WDF_NO_OBJECT_ATTRIBUTES, &device);

    if (NT_SUCCESS(status))
    {
        //
        // Create a device interface so that applications can find and talk
        // to us.
        //
        status = WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_SWIVL, NULL);

        if (NT_SUCCESS(status))
        {
            //
            // Initialize the I/O Package and any Queues
            //
            status = SwivlQueueInitialize(device);
        }
    }

    return status;
}
