#include <ntddk.h>

#define DRIVER_TAG 'myno'

UNICODE_STRING g_RegistryPath;



void SampleUnload(PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);
	ExFreePool(g_RegistryPath.Buffer);
	KdPrint(("Sample driver Unload called\n"));
}

NTSTATUS CallSomeKernelFunction()
{
	// Implementation of the kernel function
	// For example purposes, let's return STATUS_SUCCESS
	return STATUS_SUCCESS;
}

NTSTATUS DoWork()
{
	NTSTATUS status = CallSomeKernelFunction();
	NTSTATUS status_failed = STATUS_UNSUCCESSFUL;
	if (!NT_SUCCESS(status_failed)) {
		KdPrint(("Error occurred: 0x%08X\n", status));
		return status;
	}
	// continue with more operations
	return STATUS_SUCCESS;
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	DriverObject->DriverUnload = SampleUnload;
	KdPrint(("Sample driver Load called\n"));
	//NTSTATUS status = DoWork();
	//UNREFERENCED_PARAMETER(status);
	g_RegistryPath.Buffer = (PWCH)ExAllocatePool2(PagedPool, RegistryPath->Length, DRIVER_TAG);

	if (g_RegistryPath.Buffer == NULL)
	{
		KdPrint(("Failed to allocate memory\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	g_RegistryPath.MaximumLength = RegistryPath->Length;
	RtlCopyUnicodeString(&g_RegistryPath, (PUNICODE_STRING)RegistryPath);
	KdPrint(("Original registry path: %wZ\n", RegistryPath));
	KdPrint(("Copied registry path: %wZ\n", &g_RegistryPath));


	return STATUS_SUCCESS;
}