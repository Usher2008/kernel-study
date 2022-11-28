#include <windows.h>
#include <iostream>

#define CWK_DEV_SYM L"\\\\.\\usher_12345"
#define CWK_DVC_SEND_STR (ULONG)CTL_CODE(FILE_DEVICE_UNKNOWN, 0x911, METHOD_BUFFERED, FILE_WRITE_DATA)
#define CWK_DVC_RECV_STR (ULONG)CTL_CODE(FILE_DEVICE_UNKNOWN, 0x912, METHOD_BUFFERED, FILE_READ_DATA)

int main()
{
    HANDLE device = NULL;

    device = CreateFile(CWK_DEV_SYM, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, 0);
    if (device == INVALID_HANDLE_VALUE)
    {
        printf("Open device failed.\r\n");
        return -1;
    }
    else
        printf("Open device successfully.\r\n");

    char msg[] = {"Hello driver, this is a message from app.\r\n"};
    DWORD ret_len = 0;
    if (!DeviceIoControl(device, CWK_DVC_SEND_STR, msg, strlen(msg) + 1, NULL, 0, &ret_len, 0))
    {
        printf("Send message failed.\r\n");
        return -2;
    }
    else
        printf("Send message successfully.\r\n");

    CloseHandle(device);
    printf("Close device Handle successfully.\r\n");
}