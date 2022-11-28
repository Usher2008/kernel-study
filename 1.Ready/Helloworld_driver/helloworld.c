#include <ntddk.h>
#include <wdmsec.h>
#include <initguid.h>
#define CWK_CDO_SYB_NAME L"\\??\\usher_12345"
#define CWK_DVC_SEND_STR (ULONG)CTL_CODE(FILE_DEVICE_UNKNOWN, 0x911, METHOD_BUFFERED, FILE_WRITE_DATA)
#define CWK_DVC_RECV_STR (ULONG)CTL_CODE(FILE_DEVICE_UNKNOWN, 0x912, METHOD_BUFFERED, FILE_READ_DATA)
extern void breakpoint(void);

NTSTATUS cwkDispatch(
    IN PDEVICE_OBJECT dev,
    IN PIRP irp
);

PDEVICE_OBJECT g_cdo = NULL;

DEFINE_GUID(MYGUID, 0xFE30FA04, 0xC322, 0xBCDD, 0x20, 0xE8, 0xF8, 0xD2, 0x7C, 0x0C, 0xAE, 0xC7);

VOID DriverUnload(PDRIVER_OBJECT pDriver)
{
	UNREFERENCED_PARAMETER(pDriver);
	DbgPrint("Goodbye~\n");

    UNICODE_STRING cdo_syb = RTL_CONSTANT_STRING(CWK_CDO_SYB_NAME);
    ASSERT(g_cdo != NULL);
    IoDeleteSymbolicLink(&cdo_syb);
    IoDeleteDevice(g_cdo);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING pRegPath)
{
	DbgPrint("Hello Driver!\n");
	UNREFERENCED_PARAMETER(pRegPath);
	pDriver->DriverUnload = DriverUnload;

    UNICODE_STRING sddl = RTL_CONSTANT_STRING(L"D:P(A;;GA;;;WD)");
    UNICODE_STRING cdo_name = RTL_CONSTANT_STRING(L"\\Device\\usher_12345");
    UNICODE_STRING cdo_syb = RTL_CONSTANT_STRING(CWK_CDO_SYB_NAME);

    NTSTATUS status;
 
    status = IoCreateDeviceSecure(pDriver, 0, &cdo_name, FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN, FALSE, &sddl,
        (LPCGUID)&MYGUID, &g_cdo);
    if (!NT_SUCCESS(status))
        return status;

    status = IoCreateSymbolicLink(&cdo_syb, &cdo_name);
    if (!NT_SUCCESS(status))
    {
        //删除之前生成的设备对象
        IoDeleteDevice(g_cdo);
        return status;
    }

    ULONG i;
    for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; ++i)
    {
        pDriver->MajorFunction[i] = cwkDispatch;
    }

	return STATUS_SUCCESS;
}


NTSTATUS cwkDispatch(
    IN PDEVICE_OBJECT dev,
    IN PIRP irp
) 
{
    PIO_STACK_LOCATION irpsp = IoGetCurrentIrpStackLocation(irp);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG ret_len = 0;
    if (dev == g_cdo)
    {
        if (irpsp->MajorFunction == IRP_MJ_CREATE || irpsp->MajorFunction == IRP_MJ_CLOSE)
        {
            ;//生成和关闭请求
        }
        else if (irpsp->MajorFunction == IRP_MJ_DEVICE_CONTROL)
        {
            PVOID buffer = irp->AssociatedIrp.SystemBuffer;
            ULONG inlen = irpsp->Parameters.DeviceIoControl.InputBufferLength;
            ULONG outlen = irpsp->Parameters.DeviceIoControl.OutputBufferLength;
            switch (irpsp->Parameters.DeviceIoControl.IoControlCode)
            {
            case CWK_DVC_SEND_STR:
                ASSERT(buffer != NULL);
                ASSERT(inlen > 0);
                ASSERT(outlen == 0);
                DbgPrint((char*)buffer);
                break;
            case CWK_DVC_RECV_STR:
            default:
                status = STATUS_INVALID_PARAMETER;
                break;
            }
        }
    }
    irp->IoStatus.Information = ret_len;
    irp->IoStatus.Status = status;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}