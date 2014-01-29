
#include "stdafx.h"

TCHAR ifacePath[MAX_PATH];

BOOL findIfacePath(void)
{
    SP_DEVICE_INTERFACE_DATA devIfaceData;
    DWORD devIfaceDetailSize;

    HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_SWIVL, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    devIfaceData.cbSize = sizeof devIfaceData;
    if (!SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &GUID_DEVINTERFACE_SWIVL, 0, &devIfaceData))
    {
        DebugBreak();
        return FALSE;
    }
    SetupDiGetDeviceInterfaceDetail(hDevInfo, &devIfaceData, NULL, 0, &devIfaceDetailSize, NULL);
    SP_DEVICE_INTERFACE_DETAIL_DATA *pDevIfaceDetailData = (SP_DEVICE_INTERFACE_DETAIL_DATA *)malloc(devIfaceDetailSize);
    if (!pDevIfaceDetailData)
    {
        DebugBreak();
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return FALSE;
    }
    pDevIfaceDetailData->cbSize = sizeof *pDevIfaceDetailData;
    if (!SetupDiGetDeviceInterfaceDetail(hDevInfo, &devIfaceData, pDevIfaceDetailData, devIfaceDetailSize, NULL, NULL))
    {
        DebugBreak();
        free(pDevIfaceDetailData);
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return FALSE;
    }
    _tcscpy(ifacePath, pDevIfaceDetailData->DevicePath);
    free(pDevIfaceDetailData);
    SetupDiDestroyDeviceInfoList(hDevInfo);
}

int innerMain(void)
{
    if (!findIfacePath())
    {
        _tperror(_T("Unable to find device interface path"));
        return 1;
    }
    HANDLE hIface = CreateFile(ifacePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    if (hIface == INVALID_HANDLE_VALUE)
    {
        _tperror(_T("Unable to open interface"));
        return 1;
    }

    /*
    if (!DeviceIoControl(hIface, IOCTL_SWIVL_TEST, NULL, 0, &test, sizeof(DWORD32), &nRet, NULL))
    {
        CloseHandle(hIface);
        _tperror(_T("Unable to complete IOCTL"));
        return 1;
    }
    _tprintf(_T("0x%08x\n"), test);
    */

    CloseHandle(hIface);
    return 0;
}

int _tmain(int argc, TCHAR **argv)
{
    int rc = innerMain();
    system("PAUSE");
    return rc;
}
