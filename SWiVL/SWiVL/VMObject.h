
typedef struct _VMOBJECT
{
    ULONG index;
    WDFMEMORY mem;

    PCPU_INFO cpuInfo;

    WDFMEMORY vmcsMem;
    PVOID vmcsRegion;

    LIST_ENTRY listEntry;
} VMOBJECT, *PVMOBJECT;

NTSTATUS SwivlVmoCreate(_Out_ PVMOBJECT *Vmo);
PVMOBJECT SwivlVmoFromIndex(_In_ ULONG Index);
VOID SwivlVmoDelete(_In_ ULONG Index);
