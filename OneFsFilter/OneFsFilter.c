/*++

Module Name:

    OneFsFilter.c

Abstract:

    This is the main module of the OneFsFilter miniFilter driver.

Environment:

    Kernel mode

--*/
#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

#pragma warning(error:4100)     //  Enable-Unreferenced formal parameter
#pragma warning(error:4101)     //  Enable-Unreferenced local variable
#pragma warning(error:4061)     //  Enable-missing enumeration in switch statement
#pragma warning(error:4505)     //  Enable-identify dead functions


#include <fltKernel.h>
#include <dontuse.h>



#define UNICODE_PATH_POOL_TAG "unip"

#define PATH_COMP_SEPARATOR ((WCHAR)L'\\')
#define PATH_ENTER_DIRECTORY (L"\\*")

typedef struct _GLOBAL_DATA {
    PFLT_FILTER Filter;
    UNICODE_STRING SourceDirectory;
    UNICODE_STRING DestinationDirectory;

} global_t;


ULONG_PTR OperationStatusCtx = 1;

#define DBG_ERR            0x00000001
#define DBG_WRN            0x00000002
#define DBG_MSG            0x00000004

#define DBG_ALL            0xFFFFFFFF

ULONG gTraceFlags = DBG_ALL;

#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(DBG_ALL,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))

#define PrintError(_string) \
    PT_DBG_PRINT(DBG_ERR, _string)

global_t Globals;


/*************************************************************************
    Prototypes
*************************************************************************/

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    );

NTSTATUS
OneFsFilterInstanceSetup (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
    );

VOID
OneFsFilterInstanceTeardownStart (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

VOID
OneFsFilterInstanceTeardownComplete (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

NTSTATUS
OneFsFilterUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    );

NTSTATUS
OneFsFilterInstanceQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
OneFsFilterPreOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

VOID
OneFsFilterOperationStatusCallback (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
    _In_ NTSTATUS OperationStatus,
    _In_ PVOID RequesterContext
    );

FLT_POSTOP_CALLBACK_STATUS
OneFsFilterPostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
OneFsFilterPreOperationNoPostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

BOOLEAN
OneFsFilterDoRequestOperationStatus(
    _In_ PFLT_CALLBACK_DATA Data
    );

EXTERN_C_END

_When_(return == 0, _Post_satisfies_(String->Buffer != NULL))
NTSTATUS
AllocateUnicodeString(
    _Inout_ PUNICODE_STRING String
);

VOID
FreeUnicodeString(
    _Inout_ PUNICODE_STRING String
);

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, OneFsFilterUnload)
#pragma alloc_text(PAGE, OneFsFilterInstanceQueryTeardown)
#pragma alloc_text(PAGE, OneFsFilterInstanceSetup)
#pragma alloc_text(PAGE, OneFsFilterInstanceTeardownStart)
#pragma alloc_text(PAGE, OneFsFilterInstanceTeardownComplete)
#pragma alloc_text(PAGE, AllocateUnicodeString)
#pragma alloc_text(PAGE, FreeUnicodeString)
#endif

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {


    { IRP_MJ_CREATE,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },
/*
    { IRP_MJ_CREATE_NAMED_PIPE,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_CLOSE,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_READ,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_WRITE,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },
*/
    { IRP_MJ_QUERY_INFORMATION,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_SET_INFORMATION,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },
/*
    { IRP_MJ_QUERY_EA,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_SET_EA,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_FLUSH_BUFFERS,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },
*/
    { IRP_MJ_QUERY_VOLUME_INFORMATION,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },
/*
    { IRP_MJ_SET_VOLUME_INFORMATION,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },
*/
    { IRP_MJ_DIRECTORY_CONTROL,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },
/*
    { IRP_MJ_FILE_SYSTEM_CONTROL,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_DEVICE_CONTROL,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_INTERNAL_DEVICE_CONTROL,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_SHUTDOWN,
      0,
      OneFsFilterPreOperationNoPostOperation,
      NULL },                               //post operations not supported

    { IRP_MJ_LOCK_CONTROL,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_CLEANUP,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_CREATE_MAILSLOT,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_QUERY_SECURITY,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_SET_SECURITY,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_QUERY_QUOTA,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_SET_QUOTA,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_PNP,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_RELEASE_FOR_SECTION_SYNCHRONIZATION,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_ACQUIRE_FOR_MOD_WRITE,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_RELEASE_FOR_MOD_WRITE,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_ACQUIRE_FOR_CC_FLUSH,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_RELEASE_FOR_CC_FLUSH,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_FAST_IO_CHECK_IF_POSSIBLE,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_NETWORK_QUERY_OPEN,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_MDL_READ,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_MDL_READ_COMPLETE,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_PREPARE_MDL_WRITE,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_MDL_WRITE_COMPLETE,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_VOLUME_MOUNT,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },

    { IRP_MJ_VOLUME_DISMOUNT,
      0,
      OneFsFilterPreOperation,
      OneFsFilterPostOperation },
*/


    { IRP_MJ_OPERATION_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof( FLT_REGISTRATION ),         //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    NULL,                               //  Context
    Callbacks,                          //  Operation callbacks

    OneFsFilterUnload,                           //  MiniFilterUnload

    OneFsFilterInstanceSetup,                    //  InstanceSetup
    OneFsFilterInstanceQueryTeardown,            //  InstanceQueryTeardown
    OneFsFilterInstanceTeardownStart,            //  InstanceTeardownStart
    OneFsFilterInstanceTeardownComplete,         //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};



NTSTATUS
OneFsFilterInstanceSetup (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
    )
/*++

Routine Description:

    This routine is called whenever a new instance is created on a volume. This
    gives us a chance to decide if we need to attach to this volume or not.

    If this routine is not defined in the registration structure, automatic
    instances are always created.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Flags describing the reason for this attach request.

Return Value:

    STATUS_SUCCESS - attach
    STATUS_FLT_DO_NOT_ATTACH - do not attach

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );
    UNREFERENCED_PARAMETER( VolumeDeviceType );
    UNREFERENCED_PARAMETER( VolumeFilesystemType );

    PAGED_CODE();

    PT_DBG_PRINT( DBG_MSG,
                  ("OneFsFilter!OneFsFilterInstanceSetup: Entered\n") );

    return STATUS_SUCCESS;
}


NTSTATUS
OneFsFilterInstanceQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This is called when an instance is being manually deleted by a
    call to FltDetachVolume or FilterDetach thereby giving us a
    chance to fail that detach request.

    If this routine is not defined in the registration structure, explicit
    detach requests via FltDetachVolume or FilterDetach will always be
    failed.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Indicating where this detach request came from.

Return Value:

    Returns the status of this operation.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( DBG_MSG,
                  ("OneFsFilter!OneFsFilterInstanceQueryTeardown: Entered\n") );

    return STATUS_SUCCESS;
}


VOID
OneFsFilterInstanceTeardownStart (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This routine is called at the start of instance teardown.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Reason why this instance is being deleted.

Return Value:

    None.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( DBG_MSG,
                  ("OneFsFilter!OneFsFilterInstanceTeardownStart: Entered\n") );
}


VOID
OneFsFilterInstanceTeardownComplete (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This routine is called at the end of instance teardown.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Reason why this instance is being deleted.

Return Value:

    None.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( DBG_MSG,
                  ("OneFsFilter!OneFsFilterInstanceTeardownComplete: Entered\n") );
}


/*************************************************************************
    MiniFilter initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This is the initialization routine for this miniFilter driver.  This
    registers with FltMgr and initializes all global data structures.

Arguments:

    DriverObject - Pointer to driver object created by the system to
        represent this driver.

    RegistryPath - Unicode string identifying where the parameters for this
        driver are located in the registry.

Return Value:

    Routine can return non success error codes.

--*/
{
    NTSTATUS status;

    HANDLE hRegKey = NULL;
    OBJECT_ATTRIBUTES attributes;
    UNICODE_STRING regKeyName;
    ULONG regValLength;

    /* For scalar reg value only
    UCHAR regValStackBuffer[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONG)];
    PKEY_VALUE_PARTIAL_INFORMATION regValStack = (PKEY_VALUE_PARTIAL_INFORMATION)regValStackBuffer;
    ULONG regValStackLength = sizeof(regValStackBuffer);  
    */

    PKEY_VALUE_PARTIAL_INFORMATION regValPool = NULL;
    ULONG regValPoolLength = 0;

    WCHAR sourceDirectoryTail;
    WCHAR destinationDirectoryTail;

    UNREFERENCED_PARAMETER( RegistryPath );

    PT_DBG_PRINT( DBG_MSG,
                  ("OneFsFilter!DriverEntry: Entered\n") );

    // Get scalar values from registry
    InitializeObjectAttributes(&attributes,
        RegistryPath,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL);
    status = ZwOpenKey(&hRegKey, KEY_READ, &attributes);
    if (!NT_SUCCESS(status))
    {
        PrintError(("[%s] Registry open failed: %08x", __func__, status));
        goto DriverEntryCleanup;
    }

    // Get src dir from registry
    RtlInitUnicodeString(&regKeyName, L"SourceDirectory");
    status = ZwQueryValueKey(
        hRegKey,
        &regKeyName,
        KeyValuePartialInformation,
        NULL,
        0,
        &regValLength);
    if (status != STATUS_BUFFER_TOO_SMALL && status != STATUS_BUFFER_OVERFLOW)
    {
        status = STATUS_INVALID_PARAMETER;
        goto DriverEntryCleanup;
    }
    regValPool = ExAllocatePoolWithTag(PagedPool, regValLength, UNICODE_PATH_POOL_TAG);
    if (regValPool == NULL)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto DriverEntryCleanup;
    }
    regValPoolLength = regValLength;

    status = ZwQueryValueKey(
        hRegKey,
        &regKeyName,
        KeyValuePartialInformation,
        regValPool,
        regValPoolLength,
        &regValLength
    );
    if (!NT_SUCCESS(status))
    {
        goto DriverEntryCleanup;
    }
    if (regValPool->Type != REG_SZ) // Unicode nul terminated string
    {
        goto DriverEntryCleanup;
    }
    Globals.SourceDirectory.MaximumLength = (USHORT)regValPool->DataLength;
    status = AllocateUnicodeString(&(Globals.SourceDirectory));
    if (!NT_SUCCESS(status))
    {
        goto DriverEntryCleanup;
    }
    Globals.SourceDirectory.Length = (USHORT)regValPool->DataLength - sizeof(UNICODE_NULL);
    RtlCopyMemory(Globals.SourceDirectory.Buffer, regValPool->Data, Globals.SourceDirectory.Length);
    
    // Get dst dir from registry
    RtlInitUnicodeString(&regKeyName, L"DestinationDirectory");
    status = ZwQueryValueKey(
        hRegKey,
        &regKeyName,
        KeyValuePartialInformation,
        regValPool,
        regValPoolLength,
        &regValLength
    );
    if (!NT_SUCCESS(status))
    {
        if (status != STATUS_BUFFER_TOO_SMALL && status != STATUS_BUFFER_OVERFLOW)
        {
            // Free pool by the cleanup routine
            goto DriverEntryCleanup;
        }
        // Else we know it's the pool's problem
        ExFreePoolWithTag(regValPool, UNICODE_PATH_POOL_TAG);
        regValPool = ExAllocatePoolWithTag(PagedPool, regValLength, UNICODE_PATH_POOL_TAG);
        if (regValPool == NULL)
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto DriverEntryCleanup;
        }
        regValPoolLength = regValLength;
    }
    status = ZwQueryValueKey(
        hRegKey,
        &regKeyName,
        KeyValuePartialInformation,
        regValPool,
        regValPoolLength,
        &regValLength
    );
    if (!NT_SUCCESS(status))
    {
        goto DriverEntryCleanup;
    }
    if (regValPool->Type != REG_SZ) // Unicode nul terminated string
    {
        goto DriverEntryCleanup;
    }
    Globals.DestinationDirectory.MaximumLength = (USHORT)regValPool->DataLength;
    status = AllocateUnicodeString(&(Globals.DestinationDirectory));
    if (!NT_SUCCESS(status))
    {
        goto DriverEntryCleanup;
    }
    Globals.DestinationDirectory.Length = (USHORT)regValPool->DataLength - sizeof(UNICODE_NULL);
    RtlCopyMemory(Globals.DestinationDirectory.Buffer, regValPool->Data, Globals.DestinationDirectory.Length);
    
    // Registry value validations
    sourceDirectoryTail = (WCHAR)Globals.SourceDirectory.Buffer[Globals.SourceDirectory.Length / sizeof(WCHAR) - 1];
    destinationDirectoryTail = (WCHAR)Globals.DestinationDirectory.Buffer[Globals.DestinationDirectory.Length / sizeof(WCHAR) - 1];
    if (sourceDirectoryTail == PATH_COMP_SEPARATOR
        || destinationDirectoryTail == PATH_COMP_SEPARATOR)
    {
        status = STATUS_INVALID_PARAMETER;
        goto DriverEntryCleanup;
    }

    //
    //  Register with FltMgr to tell it our callback routines
    //

    status = FltRegisterFilter( DriverObject,
                                &FilterRegistration,
                                &Globals.Filter );

    FLT_ASSERT( NT_SUCCESS( status ) );

    if (NT_SUCCESS( status )) {

        //
        //  Start filtering i/o
        //

        status = FltStartFiltering( Globals.Filter );

        if (!NT_SUCCESS( status )) {

            FltUnregisterFilter( Globals.Filter );
        }
    }

DriverEntryCleanup:
    if (regValPool != NULL)
    {
        ExFreePoolWithTag(regValPool, UNICODE_PATH_POOL_TAG);
        regValPool = NULL;
    }
    if (hRegKey != NULL)
    {
        ZwClose(hRegKey);
    }
    if (!NT_SUCCESS(status))
    {
        FreeUnicodeString(&Globals.SourceDirectory);
        FreeUnicodeString(&Globals.DestinationDirectory);
    }
    return status;
}

NTSTATUS
OneFsFilterUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    )
/*++

Routine Description:

    This is the unload routine for this miniFilter driver. This is called
    when the minifilter is about to be unloaded. We can fail this unload
    request if this is not a mandatory unload indicated by the Flags
    parameter.

Arguments:

    Flags - Indicating if this is a mandatory unload.

Return Value:

    Returns STATUS_SUCCESS.

--*/
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( DBG_MSG,
                  ("OneFsFilter!OneFsFilterUnload: Entered\n") );

    FltUnregisterFilter( Globals.Filter );

    return STATUS_SUCCESS;
}


/*************************************************************************
    MiniFilter callback routines.
*************************************************************************/
FLT_PREOP_CALLBACK_STATUS
OneFsFilterPreOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine is a pre-operation dispatch routine for this miniFilter.

    This is non-pageable because it could be called on the paging path

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The context for the completion routine for this
        operation.

Return Value:

    The return value is the status of the operation.

--*/
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );

    PT_DBG_PRINT( DBG_MSG,
                  ("OneFsFilter!OneFsFilterPreOperation: Entered\n") );

    //
    //  See if this is an operation we would like the operation status
    //  for.  If so request it.
    //
    //  NOTE: most filters do NOT need to do this.  You only need to make
    //        this call if, for example, you need to know if the oplock was
    //        actually granted.
    //

    if (OneFsFilterDoRequestOperationStatus( Data )) {

        status = FltRequestOperationStatusCallback( Data,
                                                    OneFsFilterOperationStatusCallback,
                                                    (PVOID)(++OperationStatusCtx) );
        if (!NT_SUCCESS(status)) {

            PT_DBG_PRINT(DBG_MSG,
                          ("OneFsFilter!OneFsFilterPreOperation: FltRequestOperationStatusCallback Failed, status=%08x\n",
                           status) );
        }
    }

    // This template code does not do anything with the callbackData, but
    // rather returns FLT_PREOP_SUCCESS_WITH_CALLBACK.
    // This passes the request down to the next miniFilter in the chain.

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}



VOID
OneFsFilterOperationStatusCallback (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
    _In_ NTSTATUS OperationStatus,
    _In_ PVOID RequesterContext
    )
/*++

Routine Description:

    This routine is called when the given operation returns from the call
    to IoCallDriver.  This is useful for operations where STATUS_PENDING
    means the operation was successfully queued.  This is useful for OpLocks
    and directory change notification operations.

    This callback is called in the context of the originating thread and will
    never be called at DPC level.  The file object has been correctly
    referenced so that you can access it.  It will be automatically
    dereferenced upon return.

    This is non-pageable because it could be called on the paging path

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    RequesterContext - The context for the completion routine for this
        operation.

    OperationStatus -

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );

    
    PT_DBG_PRINT( DBG_MSG,
                  ("OneFsFilter!OneFsFilterOperationStatusCallback: Entered\n") );

    PT_DBG_PRINT(DBG_MSG,
                  ("OneFsFilter!OneFsFilterOperationStatusCallback: Status=%08x ctx=%p IrpMj=%02x.%02x \"%s\"\n",
                   OperationStatus,
                   RequesterContext,
                   ParameterSnapshot->MajorFunction,
                   ParameterSnapshot->MinorFunction,
                   FltGetIrpName(ParameterSnapshot->MajorFunction)) );
}


FLT_POSTOP_CALLBACK_STATUS
OneFsFilterPostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    )
/*++

Routine Description:

    This routine is the post-operation completion routine for this
    miniFilter.

    This is non-pageable because it may be called at DPC level.

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The completion context set in the pre-operation routine.

    Flags - Denotes whether the completion is successful or is being drained.

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );
    UNREFERENCED_PARAMETER( Flags );

    PT_DBG_PRINT( DBG_MSG,
                  ("OneFsFilter!OneFsFilterPostOperation: Entered\n") );

    return FLT_POSTOP_FINISHED_PROCESSING;
}


FLT_PREOP_CALLBACK_STATUS
OneFsFilterPreOperationNoPostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine is a pre-operation dispatch routine for this miniFilter.

    This is non-pageable because it could be called on the paging path

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The context for the completion routine for this
        operation.

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );

    PT_DBG_PRINT( DBG_MSG,
                  ("OneFsFilter!OneFsFilterPreOperationNoPostOperation: Entered\n") );

    // This template code does not do anything with the callbackData, but
    // rather returns FLT_PREOP_SUCCESS_NO_CALLBACK.
    // This passes the request down to the next miniFilter in the chain.

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}


BOOLEAN
OneFsFilterDoRequestOperationStatus(
    _In_ PFLT_CALLBACK_DATA Data
    )
/*++

Routine Description:

    This identifies those operations we want the operation status for.  These
    are typically operations that return STATUS_PENDING as a normal completion
    status.

Arguments:

Return Value:

    TRUE - If we want the operation status
    FALSE - If we don't

--*/
{
    PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;

    //
    //  return boolean state based on which operations we are interested in
    //

    return (BOOLEAN)

            //
            //  Check for oplock operations
            //

             (((iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) &&
               ((iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_FILTER_OPLOCK)  ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_BATCH_OPLOCK)   ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_1) ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_2)))

              ||

              //
              //    Check for directy change notification
              //

              ((iopb->MajorFunction == IRP_MJ_DIRECTORY_CONTROL) &&
               (iopb->MinorFunction == IRP_MN_NOTIFY_CHANGE_DIRECTORY))
             );
}


_When_(return == 0, _Post_satisfies_(String->Buffer != NULL))
NTSTATUS
AllocateUnicodeString(
    _Inout_ PUNICODE_STRING String
)
{
    PAGED_CODE();
    String->Buffer = ExAllocatePoolWithTag(
        NonPagedPool,
        String->MaximumLength,
        UNICODE_PATH_POOL_TAG
    );
    if (String->Buffer == NULL)
    {
        PrintError(("[%s] Allocation failed for size %d", __func__, String->MaximumLength));
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    String->Length = 0;
    return STATUS_SUCCESS;
}

VOID
FreeUnicodeString(
    _Inout_ PUNICODE_STRING String
)
{
    PAGED_CODE();
    if (String->Buffer)
    {
        ExFreePoolWithTag(String->Buffer, UNICODE_PATH_POOL_TAG);
    }
    String->Length = String->MaximumLength = 0;
    String->Buffer = NULL;
}