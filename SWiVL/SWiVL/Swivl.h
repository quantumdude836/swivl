/*++

Module Name:

    swivl.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

//
// Define an Interface Guid so that app can find the device and talk to it.
//

// {ec1e7b44-1dd6-4f8c-a840-02486bd952ed}
DEFINE_GUID(GUID_DEVINTERFACE_SWIVL, 0xec1e7b44, 0x1dd6, 0x4f8c, 0xa8, 0x40, 0x02 ,0x48, 0x6b, 0xd9, 0x52, 0xed);

#define FILE_DEVICE_HYPERVISOR (0x8000 | 'VH')

#define SWIVL_SUBFUNC_ROOT 0x0000
#define SWIVL_SUBFUNC_VM   0x0400
#define SWIVL_SUBFUNC_VCPU 0x0800

#define IOCTL_SWIVL_CREATE_VM CTL_CODE(FILE_DEVICE_HYPERVISOR, SWIVL_SUBFUNC_ROOT | 0x00, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SWIVL_VM_TEST   CTL_CODE(FILE_DEVICE_HYPERVISOR, SWIVL_SUBFUNC_VM   | 0x00, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SWIVL_DELETE_VM CTL_CODE(FILE_DEVICE_HYPERVISOR, SWIVL_SUBFUNC_VM   | 0xff, METHOD_BUFFERED, FILE_ANY_ACCESS)
