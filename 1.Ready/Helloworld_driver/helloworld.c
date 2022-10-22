#include <ntddk.h>
extern void breakpoint(void);
VOID DriverUnload(PDRIVER_OBJECT pDriver)
{
	UNREFERENCED_PARAMETER(pDriver);
	DbgPrint("Goodbye~\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING pRegPath)
{
	breakpoint();
	DbgPrint("Hello Driver!\n");
	UNREFERENCED_PARAMETER(pRegPath);
	pDriver->DriverUnload = DriverUnload;
	return STATUS_SUCCESS;
}