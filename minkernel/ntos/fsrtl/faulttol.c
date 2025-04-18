/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    FaultTol.c

Abstract:

    The routines in this module help the file systems perform fault
    tolerance operation to the FT device drivers.

Author:

    David Goebel    [DavidGoe]  30-Mar-1993

Revision History:

--*/

#include "fsrtlp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FsRtlBalanceReads)
#pragma alloc_text(PAGE, FsRtlSyncVolumes)
#endif


NTSTATUS
FsRtlBalanceReads (
    IN PDEVICE_OBJECT TargetDevice
    )

/*++

Routine Description:

    This routine signals a device driver that it is now OK to start
    balancing reads from a mirrored drive.  This is typically called
    after the file system determines that a volume is clean.

Arguments:

    TargetDevice - Supplies the device to start balanced read from.

Return Value:

    NTSTATUS - The result of the operation.  This will be
        STATUS_INVALID_DEVICE_REQUEST if the volume is not a mirror.

--*/

{
    PIRP Irp;
    KEVENT Event;
    IO_STATUS_BLOCK Iosb;
    NTSTATUS Status;

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    Irp = IoBuildDeviceIoControlRequest( FT_BALANCED_READ_MODE,
                                         TargetDevice,
                                         NULL,
                                         0,
                                         NULL,
                                         0,
                                         FALSE,
                                         &Event,
                                         &Iosb );

    if ( Irp == NULL ) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = IoCallDriver( TargetDevice, Irp );


    if (Status == STATUS_PENDING) {
        Status = KeWaitForSingleObject( &Event,
                                        Executive,
                                        KernelMode,
                                        FALSE,
                                        NULL );

        ASSERT( Status == STATUS_SUCCESS );

        Status = Iosb.Status;
    }

    return Status;
}

NTSTATUS
FsRtlSyncVolumes (
    IN PDEVICE_OBJECT TargetDevice,
    IN PLARGE_INTEGER ByteOffset OPTIONAL,
    IN PLARGE_INTEGER ByteCount
    )

/*++

Routine Description:

    This routine signals a device driver that it must sync redundant
    members of a mirror from the primary member.  This is typically
    called after the file system determines that a volume is dirty.

Arguments:

    TargetDevice - Supplies the device to sync.

    ByteOffset - If specified, gives the location to start syncing

    ByteCount - Gives the byte count to sync.  Ignored if StartingOffset
        not specified.

Return Value:

    NTSTATUS - The result of the operation.  This will be
        STATUS_INVALID_DEVICE_REQUEST if the volume is not a mirror.

--*/

{
    UNREFERENCED_PARAMETER (TargetDevice);
    UNREFERENCED_PARAMETER (ByteOffset);
    UNREFERENCED_PARAMETER (ByteCount);

    return STATUS_SUCCESS;
}


