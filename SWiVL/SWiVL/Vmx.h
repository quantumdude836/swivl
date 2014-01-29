
// low-level VMX instruction interface

UINT32 VmxGetVmcsIdentifier(VOID);
NTSTATUS VmxEnable(PLONGLONG VmxonPhys);
VOID VmxDisable(VOID);
