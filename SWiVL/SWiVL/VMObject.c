
#include "driver.h"
#include "vmobject.tmh"

// TODO: figure out which functions can be paged (based on use by the queue)

static SWIVL_CPU_TASK SwivlVmoInitOnCpu;

#define VMO_ALLOC_INC 8

static ULONG vmoCount = 0;
static ULONG vmoAlloc = 0;
static WDFMEMORY vmoListMemory = NULL;
static PVMOBJECT *vmoList = NULL;

static NTSTATUS SwivlVmoFindIndex(_Out_ PULONG Index)
{
    NTSTATUS status;
    ULONG i;
    WDFMEMORY newMem;
    PVMOBJECT *newList;

    // search the current list for any free slots
    for (i = 0; i < vmoCount; i++)
    {
        if (!vmoList[i])
        {
            // slot is free
            *Index = i;
            return STATUS_SUCCESS;
        }
    }

    // need to add a new slot at the end of the list

    if (vmoAlloc == vmoCount)
    {
        // need to resize the VMO list (unfortunately, there's no "realloc" equivalent)

        // allocate new buffer
        status = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES, NonPagedPoolNx, 'LOMV', sizeof *vmoList * (vmoAlloc + VMO_ALLOC_INC), &newMem, (PVOID *)&newList);
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_VMOBJECT, "Unable to resize VMO list %!STATUS!", status);
            return status;
        }

        if (vmoAlloc)
        {
            // transfer list contents
            RtlCopyMemory(newList, vmoList, sizeof *vmoList * vmoAlloc);
            // free old buffer
            WdfObjectDelete(vmoListMemory);
        }

        // clear out new part of list
        RtlZeroMemory(&newList[vmoAlloc], sizeof *newList * VMO_ALLOC_INC);

        // update list info
        vmoAlloc += VMO_ALLOC_INC;
        vmoListMemory = newMem;
        vmoList = newList;
    }

    *Index = vmoCount++;
    return STATUS_SUCCESS;
}

NTSTATUS SwivlVmoCreate(_Out_ PVMOBJECT *Vmo)
{
    NTSTATUS status;
    ULONG index;
    WDFMEMORY vmoMem;
    PVMOBJECT vmoPtr;

    // find a free VMO index
    status = SwivlVmoFindIndex(&index);
    if (!NT_SUCCESS(status))
        return status;

    // allocate actual VMO
    status = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES, NonPagedPoolNx, 'BOMV', sizeof *vmoPtr, &vmoMem, (PVOID *)&vmoPtr);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_VMOBJECT, "Unable to create VMO %!STATUS!", status);
        return status;
    }
    vmoPtr->index = index;
    vmoPtr->mem = vmoMem;

    // initialize VMO

    // create VMCS region
    status = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES, NonPagedPool, 'SCMV', PAGE_SIZE, &vmoPtr->vmcsMem, &vmoPtr->vmcsRegion); // TODO: Nx?
    if (!NT_SUCCESS(status))
    {
        WdfObjectDelete(vmoMem);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_VMOBJECT, "Unable to allocate VMCS region %!STATUS!", status);
        return status;
    }

    // select target CPU
    vmoPtr->cpuInfo = SwivlCpuPickVmTarget();

    // continue init on the target CPU
    SwivlCpuDispatchTask(vmoPtr->cpuInfo, SwivlVmoInitOnCpu, vmoPtr);

    // insert VMO into list
    vmoList[index] = vmoPtr;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_VMOBJECT, "Successfully created VMO %p", vmoPtr);

    *Vmo = vmoPtr;

    return STATUS_SUCCESS;
}

VOID SwivlVmoInitOnCpu(_In_ PCPU_INFO CpuInfo, _In_opt_ PVOID Argument)
{
    PVMOBJECT vmo = (PVMOBJECT)Argument;

    // write VMCS identifier
    *(PUINT32)vmo->vmcsRegion = VmxGetVmcsIdentifier();
}

PVMOBJECT SwivlVmoFromIndex(_In_ ULONG Index)
{
    // NOTE: normally, there'd be an assertion here
    // however, an invalid index could be passed from userspace, which shouldn't trap in the kernel
    if (Index >= vmoCount)
        return NULL;

    return vmoList[Index];
}

VOID SwivlVmoDelete(_In_ ULONG Index)
{
    PVMOBJECT vmo;

    vmo = SwivlVmoFromIndex(Index);
    if (!vmo)
        return;

    WdfObjectDelete(vmo->vmcsMem);

    // self-delete last
    WdfObjectDelete(vmo->mem);

    // free list slot
    vmoList[Index] = NULL;
}
