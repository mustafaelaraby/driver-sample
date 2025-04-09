#include <fltKernel.h>
#include <ntddk.h>
#include <dontuse.h>
#include <suppress.h>

// Driver-specific constants
#define DRIVER_NAME             L"ToufyFilter"
#define DRIVER_TAG              'myno'
#define TARGET_FOLDER_PATH      L"\\Device\\HarddiskVolume2\\toufy" // Adjust for your D: drive

// Global data
PFLT_FILTER g_FilterHandle = nullptr;
UNICODE_STRING g_RegistryPath = { 0 };

// Function prototypes
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);
FLT_PREOP_CALLBACK_STATUS PreOperationCallback(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);
NTSTATUS FilterUnloadCallback(FLT_FILTER_UNLOAD_FLAGS Flags);

// Filter registration structure
const FLT_OPERATION_REGISTRATION Callbacks[] = {
	{ IRP_MJ_READ, 0, PreOperationCallback, nullptr },
	{ IRP_MJ_WRITE, 0, PreOperationCallback, nullptr },
	{ IRP_MJ_SET_INFORMATION, 0, PreOperationCallback, nullptr }, // Blocks copy/rename
	{ IRP_MJ_CREATE, 0, PreOperationCallback, nullptr },         // Blocks opening
	{ IRP_MJ_OPERATION_END }
};

const FLT_REGISTRATION FilterRegistration = {
	sizeof(FLT_REGISTRATION),           // Size
	FLT_REGISTRATION_VERSION,           // Version
	0,                                  // Flags
	nullptr,                            // Context
	Callbacks,                          // Operation callbacks
	FilterUnloadCallback,               // Filter unload callback
	nullptr,                            // Instance setup callback
	nullptr,                            // Instance query teardown callback
	nullptr,                            // Instance teardown start callback
	nullptr,                            // Instance teardown complete callback
	nullptr,                            // Generate file name callback
	nullptr,                            // Normalize name component callback
	nullptr                             // Transaction notification callback
};

// Driver entry point
extern "C" NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
)
{
	NTSTATUS status;

	KdPrint(("ToufyFilter: DriverEntry called\n"));

	// Copy RegistryPath
	g_RegistryPath.Buffer = (PWCH)ExAllocatePool2(PagedPool, RegistryPath->Length, DRIVER_TAG);
	if (g_RegistryPath.Buffer == nullptr) {
		KdPrint(("ToufyFilter: Failed to allocate memory for RegistryPath\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	g_RegistryPath.MaximumLength = RegistryPath->Length;
	RtlCopyUnicodeString(&g_RegistryPath, RegistryPath);
	KdPrint(("ToufyFilter: Registry path copied: %wZ\n", &g_RegistryPath));

	// Register the filter
	status = FltRegisterFilter(DriverObject, &FilterRegistration, &g_FilterHandle);
	if (!NT_SUCCESS(status)) {
		KdPrint(("ToufyFilter: FltRegisterFilter failed with status 0x%08X\n", status));
		ExFreePool(g_RegistryPath.Buffer);
		return status;
	}

	// Start filtering
	status = FltStartFiltering(g_FilterHandle);
	if (!NT_SUCCESS(status)) {
		KdPrint(("ToufyFilter: FltStartFiltering failed with status 0x%08X\n", status));
		FltUnregisterFilter(g_FilterHandle);
		ExFreePool(g_RegistryPath.Buffer);
		return status;
	}

	KdPrint(("ToufyFilter: Driver successfully loaded and filtering started\n"));

	// Set unload routine
	DriverObject->DriverUnload = DriverUnload;

	return STATUS_SUCCESS;
}

// Driver unload routine
VOID DriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);
	if (g_FilterHandle) {
		FltUnregisterFilter(g_FilterHandle);
		g_FilterHandle = nullptr;
	}
	if (g_RegistryPath.Buffer) {
		ExFreePool(g_RegistryPath.Buffer);
		g_RegistryPath.Buffer = nullptr;
	}
	KdPrint(("ToufyFilter: Driver unloaded\n"));
}

// Pre-operation callback to block file operations
FLT_PREOP_CALLBACK_STATUS PreOperationCallback(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	PFLT_FILE_NAME_INFORMATION nameInfo = nullptr;
	NTSTATUS status = FltGetFileNameInformation(
		Data,
		FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
		&nameInfo
	);

	if (!NT_SUCCESS(status)) {
		KdPrint(("ToufyFilter: FltGetFileNameInformation failed with status 0x%08X\n", status));
		return FLT_PREOP_SUCCESS_WITH_CALLBACK;
	}

	UNICODE_STRING targetFolder = RTL_CONSTANT_STRING(TARGET_FOLDER_PATH);

	KdPrint(("ToufyFilter: Checking file: %wZ\n", &nameInfo->Name));

	// Check if the file is in the target folder
	if (RtlPrefixUnicodeString(&targetFolder, &nameInfo->Name, TRUE)) {
		switch (Data->Iopb->MajorFunction) {
		case IRP_MJ_READ:
			KdPrint(("ToufyFilter: Blocking READ on %wZ\n", &nameInfo->Name));
			Data->IoStatus.Status = STATUS_ACCESS_DENIED;
			Data->IoStatus.Information = 0;
			FltReleaseFileNameInformation(nameInfo);
			return FLT_PREOP_COMPLETE;

		case IRP_MJ_WRITE:
			KdPrint(("ToufyFilter: Blocking WRITE on %wZ\n", &nameInfo->Name));
			Data->IoStatus.Status = STATUS_ACCESS_DENIED;
			Data->IoStatus.Information = 0;
			FltReleaseFileNameInformation(nameInfo);
			return FLT_PREOP_COMPLETE;

		case IRP_MJ_SET_INFORMATION:
			KdPrint(("ToufyFilter: Blocking SET_INFORMATION (copy/rename) on %wZ\n", &nameInfo->Name));
			Data->IoStatus.Status = STATUS_ACCESS_DENIED;
			Data->IoStatus.Information = 0;
			FltReleaseFileNameInformation(nameInfo);
			return FLT_PREOP_COMPLETE;

		case IRP_MJ_CREATE:
			KdPrint(("ToufyFilter: Blocking CREATE (open) on %wZ\n", &nameInfo->Name));
			Data->IoStatus.Status = STATUS_ACCESS_DENIED;
			Data->IoStatus.Information = 0;
			FltReleaseFileNameInformation(nameInfo);
			return FLT_PREOP_COMPLETE;
		}
	}

	FltReleaseFileNameInformation(nameInfo);
	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

// Filter unload callback
NTSTATUS FilterUnloadCallback(_In_ FLT_FILTER_UNLOAD_FLAGS Flags)
{
	UNREFERENCED_PARAMETER(Flags);
	if (g_FilterHandle) {
		FltUnregisterFilter(g_FilterHandle);
		g_FilterHandle = nullptr;
	}
	KdPrint(("ToufyFilter: FilterUnloadCallback called\n"));
	return STATUS_SUCCESS;
}
