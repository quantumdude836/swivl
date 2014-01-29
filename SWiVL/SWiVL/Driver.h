/*++

Module Name:

    driver.h

Abstract:

    This file contains the driver definitions.

Environment:

    Kernel-mode Driver Framework

--*/

#define INITGUID

#include <ntddk.h>
#include <ntstrsafe.h>
#include <wdf.h>

typedef struct _CPU_INFO
{
    BOOLEAN active;
    PROCESSOR_NUMBER procNum;

    KDPC dpc;
    KEVENT dpcEvent;

    UINT32 vmcsId;
    WDFMEMORY vmxonMem;
    PVOID vmxonRegion;

    LIST_ENTRY vmoList;
    ULONG vmoCount;
} CPU_INFO, *PCPU_INFO;

#include "device.h"
#include "queue.h"
#include "trace.h"
#include "vmobject.h"
#include "vmx.h"

typedef VOID SWIVL_CPU_TASK(_In_ PCPU_INFO CpuInfo, _In_opt_ PVOID Argument);
typedef SWIVL_CPU_TASK *PFN_SWIVL_CPU_TASK;

BOOLEAN SwivlCpuDispatchTask(_In_ PCPU_INFO CpuInfo, _In_ PFN_SWIVL_CPU_TASK Task, _In_opt_ PVOID Argument);
NTSTATUS SwivlCpuWaitForTask(_In_ PCPU_INFO CpuInfo);
NTSTATUS SwivlCpuWaitForAllTasks(_In_ BOOLEAN ContinueOnFailure);
PCPU_INFO SwivlCpuPickVmTarget(VOID);

//
// WDFDRIVER Events
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD SwivlEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP SwivlEvtDriverContextCleanup;
