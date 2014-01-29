/*++

Module Name:

    driver.c

Abstract:

    This file contains the driver entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "driver.tmh"

static KDEFERRED_ROUTINE SwivlDispatchFunc;
static SWIVL_CPU_TASK SwivlVmxInit;
static SWIVL_CPU_TASK SwivlVmxCleanup;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, SwivlEvtDeviceAdd)
#pragma alloc_text(PAGE, SwivlEvtDriverContextCleanup)
#endif

static WDFMEMORY cpuInfoMem = NULL;
static CPU_INFO *cpuInfo = NULL;
static ULONG procCount, activeProcCount;

/*++

Routine Description:
    DriverEntry initializes the driver and is the first routine called by the
    system after the driver is loaded. DriverEntry specifies the other entry
    points in the function driver, such as EvtDevice and DriverUnload.

Parameters Description:

    DriverObject - represents the instance of the function driver that is loaded
    into memory. DriverEntry must initialize members of DriverObject before it
    returns to the caller. DriverObject is allocated by the system before the
    driver is loaded, and it is released by the system after the system unloads
    the function driver from memory.

    RegistryPath - represents the driver specific path in the Registry.
    The function driver can use the path to store driver related data between
    reboots. The path does not store hardware instance specific data.

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise.

--*/
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
    ULONG procIndex;

    //
    // Initialize WPP Tracing
    //
    WPP_INIT_TRACING(DriverObject, RegistryPath);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    //
    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = SwivlEvtDriverContextCleanup;

    WDF_DRIVER_CONFIG_INIT(&config, SwivlEvtDeviceAdd);

    status = WdfDriverCreate(DriverObject, RegistryPath, &attributes, &config, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDriverCreate failed %!STATUS!", status);
        WPP_CLEANUP(DriverObject);
        return status;
    }

    // initialize VMX on each active CPU
    procCount = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "KeQueryActiveProcessorCountEx returned %lu", procCount);

    status = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES, NonPagedPoolNx, 'ICVH', procCount * sizeof *cpuInfo, &cpuInfoMem, (PVOID *)&cpuInfo);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfMemoryCreate failed %!STATUS!", status);
        WPP_CLEANUP(DriverObject);
        return status;
    }

    for (procIndex = 0; procIndex < procCount; procIndex++)
    {
        cpuInfo[procIndex].active = FALSE;

        InitializeListHead(&cpuInfo[procIndex].vmoList);
        cpuInfo[procIndex].vmoCount = 0;

        // grab processor number
        status = KeGetProcessorNumberFromIndex(procIndex, &cpuInfo[procIndex].procNum);
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "Unable to get processor number for index %lu %!STATUS!", procIndex, status);
            continue;
        }

        cpuInfo[procIndex].active = TRUE;

        // init DPC event
        KeInitializeEvent(&cpuInfo[procIndex].dpcEvent, NotificationEvent, FALSE);

        // initialize CPU DPC
        KeInitializeDpc(&cpuInfo[procIndex].dpc, SwivlDispatchFunc, &cpuInfo[procIndex]);
        // place it on the selected processor
        KeSetTargetProcessorDpcEx(&cpuInfo[procIndex].dpc, &cpuInfo[procIndex].procNum);
        // queue the init function
        SwivlCpuDispatchTask(&cpuInfo[procIndex], SwivlVmxInit, NULL);
    }

    // wait for initialization to finish
    activeProcCount = 0;
    for (procIndex = 0; procIndex < procCount; procIndex++)
    {
        status = SwivlCpuWaitForTask(&cpuInfo[procIndex]);
        if (!NT_SUCCESS(status))
        {
            cpuInfo[procIndex].active = FALSE;
            continue;
        }

        if (cpuInfo[procIndex].active)
            activeProcCount++;
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%lu/%lu CPUs initialized successfully", activeProcCount, procCount);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

/*++
Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. We create and initialize a device object to
    represent a new instance of the device.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
NTSTATUS SwivlEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    status = SwivlCreateDevice(DeviceInit);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

static VOID SwivlDispatchFunc(_In_ PKDPC Dpc, _In_opt_ PVOID DeferredContext, _In_opt_ PVOID SystemArgument1, _In_opt_ PVOID SystemArgument2)
{
    PCPU_INFO info;
    PFN_SWIVL_CPU_TASK task;

    UNREFERENCED_PARAMETER(Dpc);

    // grab CPU info/task
    info = (PCPU_INFO)DeferredContext;
    task = (PFN_SWIVL_CPU_TASK)(UINT_PTR)SystemArgument1;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "[%u, %u] %!FUNC! Entry", info->procNum.Group, info->procNum.Number);

    // invoke task
    task(info, SystemArgument2);

    // signal DPC event
    // TODO: how does 'Increment' work?
    KeSetEvent(&info->dpcEvent, 0, FALSE);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "[%u, %u] %!FUNC! Exit", info->procNum.Group, info->procNum.Number);
}

NTSTATUS SwivlCpuWaitForTask(_In_ PCPU_INFO CpuInfo)
{
    NTSTATUS status;

    // ignore inactive CPUs
    if (!CpuInfo->active)
        return STATUS_SUCCESS;

    status = KeWaitForSingleObject(&CpuInfo->dpcEvent, Executive, KernelMode, FALSE, NULL);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "[%u, %u] KeWaitForSingleObject failed %!STATUS!", CpuInfo->procNum.Group, CpuInfo->procNum.Number, status);
        return status;
    }

    KeClearEvent(&CpuInfo->dpcEvent);

    return STATUS_SUCCESS;
}

NTSTATUS SwivlCpuWaitForAllTasks(_In_ BOOLEAN ContinueOnFailure)
{
    NTSTATUS status;
    ULONG procIndex;

    for (procIndex = 0; procIndex < procCount; procIndex++)
    {
        status = SwivlCpuWaitForTask(&cpuInfo[procIndex]);
        if (!NT_SUCCESS(status))
        {
            if (ContinueOnFailure)
                continue;
            else
                return status;
        }
    }

    return STATUS_SUCCESS;
}

BOOLEAN SwivlCpuDispatchTask(_In_ PCPU_INFO CpuInfo, _In_ PFN_SWIVL_CPU_TASK Task, _In_opt_ PVOID Argument)
{
    // simply queue the DPC up
    return KeInsertQueueDpc(&CpuInfo->dpc, (PVOID)(UINT_PTR)Task, Argument);
}

PCPU_INFO SwivlCpuPickVmTarget(VOID)
{
    // for now, just pick the one with the smallest VMO list
    PCPU_INFO minInfo = NULL;
    ULONG minCount = MAXULONG;
    ULONG index;

    for (index = 0; index < procCount; index++)
    {
        if (!cpuInfo[index].active)
            continue;

        if (cpuInfo[index].vmoCount < minCount)
        {
            minInfo = &cpuInfo[index];
            minCount = cpuInfo[index].vmoCount;
        }
    }

    ASSERT(minInfo);
    return minInfo;
}

static VOID SwivlVmxInit(_In_ PCPU_INFO CpuInfo, _In_opt_ PVOID Argument)
{
    NTSTATUS status;
    WDFMEMORY vmxonMem = NULL;
    PVOID vmxonRegion = NULL;
    LONGLONG vmxonPhys;
    BOOLEAN vmxEnabled = FALSE;

    UNREFERENCED_PARAMETER(Argument);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "[%u, %u] %!FUNC! Entry", CpuInfo->procNum.Group, CpuInfo->procNum.Number);

    // allocate VMXON region
    status = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES, NonPagedPoolNx, 'RXMV', PAGE_SIZE, &vmxonMem, &vmxonRegion);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "[%u, %u] WdfMemoryCreate failed %!STATUS!", CpuInfo->procNum.Group, CpuInfo->procNum.Number, status);
        goto exit;
    }
    CpuInfo->vmxonMem = vmxonMem;
    CpuInfo->vmxonRegion = vmxonRegion;
    // grab physical address of region
    vmxonPhys = MmGetPhysicalAddress(vmxonRegion).QuadPart;
    // write VMCS identifier
    CpuInfo->vmcsId = VmxGetVmcsIdentifier();
    *(PUINT32)vmxonRegion = CpuInfo->vmcsId;

    // initialize VMX
    status = VmxEnable(&vmxonPhys);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "[%u, %u] VmxEnable failed %!STATUS!", CpuInfo->procNum.Group, CpuInfo->procNum.Number, status);
        goto exit;
    }
    vmxEnabled = TRUE;

    // TODO: perhaps signal success in the CPU_INFO struct? (aside from setting 'active' TRUE)
    status = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(status))
    {
        // cleanup
        if (vmxEnabled)
            VmxDisable();
        if (vmxonRegion)
            WdfObjectDelete(vmxonMem);

        CpuInfo->active = FALSE;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "[%u, %u] %!FUNC! Exit", CpuInfo->procNum.Group, CpuInfo->procNum.Number);
}

static VOID SwivlVmxCleanup(_In_ PCPU_INFO CpuInfo, _In_opt_ PVOID Argument)
{
    UNREFERENCED_PARAMETER(Argument);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "[%u, %u] %!FUNC! Entry", CpuInfo->procNum.Group, CpuInfo->procNum.Number);

    // disable VMX
    VmxDisable();

    // free VMXON region
    WdfObjectDelete(CpuInfo->vmxonMem);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "[%u, %u] %!FUNC! Exit", CpuInfo->procNum.Group, CpuInfo->procNum.Number);
}

/*++
Routine Description:

    Free all the resources allocated in DriverEntry.

Arguments:

    DriverObject - handle to a WDF Driver object.

Return Value:

    VOID.

--*/
VOID SwivlEvtDriverContextCleanup(_In_ WDFOBJECT DriverObject)
{
    ULONG procIndex;

    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    if (cpuInfo)
    {
        // cleanup VMX
        for (procIndex = 0; procIndex < procCount; procIndex++)
        {
            // skip inactive CPUs
            if (!cpuInfo[procIndex].active)
                continue;

            // issue cleanup task
            SwivlCpuDispatchTask(&cpuInfo[procIndex], SwivlVmxCleanup, NULL);
        }

        // wait for cleanup to finish
        SwivlCpuWaitForAllTasks(TRUE);

        // free CPU info list
        WdfObjectDelete(cpuInfoMem);
    }

    //
    // Stop WPP Tracing
    //
    WPP_CLEANUP(WdfDriverWdmGetDriverObject(DriverObject));
}
