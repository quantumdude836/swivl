/*++

Module Name:

    device.h

Abstract:

    This file contains the device definitions.

Environment:

    Kernel-mode Driver Framework

--*/

#include "swivl.h"

//
// Function to initialize the device and its callbacks
//
NTSTATUS SwivlCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit);
