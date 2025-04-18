/*++

Copyright (c) 1990-2001  Microsoft Corporation

Module Name:

    kdapi.c

Abstract:

    Implementation of Kernel Debugger portable remote APIs.

Author:

    Mark Lucovsky (markl) 31-Aug-1990

Revision History:

    John Vert (jvert) 28-May-1991

        Added APIs for reading and writing physical memory
        (KdpReadPhysicalMemory and KdpWritePhysicalMemory)

    Wesley Witt (wesw) 18-Aug-1993

        Added KdpGetVersion, KdpWriteBreakPointEx, & KdpRestoreBreakPointEx


--*/

#include "kdp.h"

#if ACCASM && !defined(_MSC_VER)
long asm(const char *,...);
#pragma intrinsic(asm)
#endif

// XXX drewb - Shortcut to avoid cross-depot checkin
// build delay.  These constants are defined in ntdbg.h
// from the sdktools depot.  Once the internal sdktools
// ntdbg.h is updated from ntdbg.w this can be removed.
#ifndef DBGKD_CACHING_UNKNOWN
#define DBGKD_CACHING_UNKNOWN        0
#define DBGKD_CACHING_CACHED         1
#define DBGKD_CACHING_UNCACHED       2
#define DBGKD_CACHING_WRITE_COMBINED 3
#endif

BOOLEAN KdpContextSent;

LARGE_INTEGER KdpQueryPerformanceCounter (
    IN PKTRAP_FRAME TrapFrame
    );

extern LARGE_INTEGER Magic10000;
#define SHIFT10000   13
#define Convert100nsToMilliseconds(LARGE_INTEGER) (                         \
    RtlExtendedMagicDivide( (LARGE_INTEGER), Magic10000, SHIFT10000 )       \
    )

//
// Define forward referenced function prototypes.
//

VOID
KdpProcessInternalBreakpoint (
    ULONG BreakpointNumber
    );

VOID
KdpGetVersion(
    IN PDBGKD_MANIPULATE_STATE64 m
    );

NTSTATUS
KdpNotSupported(
    IN PDBGKD_MANIPULATE_STATE64 m
    );

VOID
KdpCauseBugCheck(
    IN PDBGKD_MANIPULATE_STATE64 m
    );

NTSTATUS
KdpWriteBreakPointEx(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    );

VOID
KdpRestoreBreakPointEx(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    );

VOID
KdpSearchMemory(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    );

ULONG
KdpSearchHammingDistance (
    ULONG_PTR Left,
    ULONG_PTR Right
    );

LOGICAL
KdpSearchPhysicalPage (
    IN PFN_NUMBER PageFrameIndex,
    ULONG_PTR RangeStart,
    ULONG_PTR RangeEnd,
    ULONG Flags,
    ULONG MmFlags
    );

LOGICAL
KdpSearchPhysicalMemoryRequested (
    VOID
    );

LOGICAL
KdpSearchPhysicalPageRange (
    ULONG MmFlags
    );

VOID
KdpFillMemory(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    );

VOID
KdpQueryMemory(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PCONTEXT Context
    );


#if i386
VOID
InternalBreakpointCheck (
    PKDPC Dpc,
    PVOID DeferredContext,
    PVOID SystemArgument1,
    PVOID SystemArgument2
    );

VOID
KdGetInternalBreakpoint(
    IN PDBGKD_MANIPULATE_STATE64 m
    );

long
SymNumFor(
    ULONG_PTR pc
    );

void PotentialNewSymbol (ULONG_PTR pc);

void DumpTraceData(PSTRING MessageData);

BOOLEAN
TraceDataRecordCallInfo(
    ULONG InstructionsTraced,
    LONG CallLevelChange,
    ULONG_PTR pc
    );

BOOLEAN
SkippingWhichBP (
    PVOID thread,
    PULONG BPNum
    );

ULONG_PTR
KdpGetReturnAddress(
    IN PCONTEXT ContextRecord
    );

ULONG_PTR
KdpGetCallNextOffset (
    ULONG_PTR Pc,
    IN PCONTEXT ContextRecord
    );

LONG
KdpLevelChange (
    ULONG_PTR Pc,
    PCONTEXT ContextRecord,
    IN OUT PBOOLEAN SpecialCall
    );

#endif // i386

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGEKD, KdEnterDebugger)
#pragma alloc_text(PAGEKD, KdExitDebugger)

#if !defined(_TRUSTED_WINDOWS_)
#pragma alloc_text(PAGEKD, KdpTimeSlipDpcRoutine)
#pragma alloc_text(PAGEKD, KdpTimeSlipWork)
#endif

#pragma alloc_text(PAGEKD, KdpSendWaitContinue)
#pragma alloc_text(PAGEKD, KdpReadVirtualMemory)
//#pragma alloc_text(PAGEKD, KdpReadVirtualMemory64)
#pragma alloc_text(PAGEKD, KdpWriteVirtualMemory)
//#pragma alloc_text(PAGEKD, KdpWriteVirtualMemory64)
#pragma alloc_text(PAGEKD, KdpGetContext)
#pragma alloc_text(PAGEKD, KdpSetContext)
#pragma alloc_text(PAGEKD, KdpWriteBreakpoint)
#pragma alloc_text(PAGEKD, KdpRestoreBreakpoint)
#pragma alloc_text(PAGEKD, KdpReportExceptionStateChange)
#pragma alloc_text(PAGEKD, KdpReportLoadSymbolsStateChange)
#pragma alloc_text(PAGEKD, KdpReportCommandStringStateChange)
#pragma alloc_text(PAGEKD, KdpReadPhysicalMemory)
#pragma alloc_text(PAGEKD, KdpWritePhysicalMemory)
#pragma alloc_text(PAGEKD, KdpReadControlSpace)
#pragma alloc_text(PAGEKD, KdpWriteControlSpace)
#pragma alloc_text(PAGEKD, KdpReadIoSpace)
#pragma alloc_text(PAGEKD, KdpWriteIoSpace)
#pragma alloc_text(PAGEKD, KdpReadIoSpaceExtended)
#pragma alloc_text(PAGEKD, KdpWriteIoSpaceExtended)
#pragma alloc_text(PAGEKD, KdpReadMachineSpecificRegister)
#pragma alloc_text(PAGEKD, KdpWriteMachineSpecificRegister)
#pragma alloc_text(PAGEKD, KdpGetBusData)
#pragma alloc_text(PAGEKD, KdpSetBusData)
#pragma alloc_text(PAGEKD, KdpGetVersion)
#pragma alloc_text(PAGEKD, KdpNotSupported)
#pragma alloc_text(PAGEKD, KdpCauseBugCheck)
#pragma alloc_text(PAGEKD, KdpWriteBreakPointEx)
#pragma alloc_text(PAGEKD, KdpRestoreBreakPointEx)
#pragma alloc_text(PAGEKD, KdpSearchMemory)
#pragma alloc_text(PAGEKD, KdpSearchHammingDistance)
#pragma alloc_text(PAGEKD, KdpSearchPhysicalPage)
#pragma alloc_text(PAGEKD, KdpSearchPhysicalMemoryRequested)
#pragma alloc_text(PAGEKD, KdpSearchPhysicalPageRange)
#pragma alloc_text(PAGEKD, KdpCheckLowMemory)
#pragma alloc_text(PAGEKD, KdpFillMemory)
#pragma alloc_text(PAGEKD, KdpQueryMemory)
#pragma alloc_text(PAGEKD, KdpSysGetVersion)
#pragma alloc_text(PAGEKD, KdpSysReadBusData)
#pragma alloc_text(PAGEKD, KdpSysWriteBusData)
#pragma alloc_text(PAGEKD, KdpSysCheckLowMemory)
#pragma alloc_text(PAGEKD, KdpSendTraceData)
#pragma alloc_text(PAGEKD, KdReportTraceData)
#if DBG
#pragma alloc_text(PAGEKD, KdpDprintf)
#endif
#if i386
#pragma alloc_text(PAGEKD, InternalBreakpointCheck)
#pragma alloc_text(PAGEKD, KdSetInternalBreakpoint)
#pragma alloc_text(PAGEKD, KdGetTraceInformation)
#pragma alloc_text(PAGEKD, KdGetInternalBreakpoint)
#pragma alloc_text(PAGEKD, SymNumFor)
#pragma alloc_text(PAGEKD, PotentialNewSymbol)
#pragma alloc_text(PAGEKD, DumpTraceData)
#pragma alloc_text(PAGEKD, TraceDataRecordCallInfo)
#pragma alloc_text(PAGEKD, SkippingWhichBP)
#pragma alloc_text(PAGEKD, KdQuerySpecialCalls)
#pragma alloc_text(PAGEKD, KdSetSpecialCall)
#pragma alloc_text(PAGEKD, KdClearSpecialCalls)
#pragma alloc_text(PAGEKD, KdpCheckTracePoint)
#pragma alloc_text(PAGEKD, KdpProcessInternalBreakpoint)
#endif // i386
#endif // ALLOC_PRAGMA


//
// This variable has a count for each time KdDisableDebugger has been called.
//
LONG KdDisableCount = 0 ;
BOOLEAN KdPreviouslyEnabled = FALSE ;


#if DBG
VOID
KdpDprintf(
    IN PCHAR f,
    ...
    )
/*++

Routine Description:

    Printf routine for the debugger that is safer than DbgPrint.  Calls
    the packet driver instead of reentering the debugger.

Arguments:

    f - Supplies printf format

Return Value:

    None

--*/
{
    char    buf[100];
    STRING  Output;
    va_list mark;

    va_start(mark, f);
    _vsnprintf(buf, 100, f, mark);
    va_end(mark);

    Output.Buffer = buf;
    Output.Length = (USHORT) strlen(Output.Buffer);
    KdpPrintString(&Output);
}
#endif // DBG


BOOLEAN
KdEnterDebugger(
    IN PKTRAP_FRAME TrapFrame,
    IN PKEXCEPTION_FRAME ExceptionFrame
    )

/*++

Routine Description:

    This function is used to enter the kernel debugger. Its purpose
    is to freeze all other processors and aqcuire the kernel debugger
    comm port.

Arguments:

    TrapFrame - Supplies a pointer to a trap frame that describes the
        trap.

    ExceptionFrame - Supplies a pointer to an exception frame that
        describes the trap.

Return Value:

    Returns the previous interrupt enable.

--*/

{

    BOOLEAN Enable;
#if DBG
    extern ULONG KiFreezeFlag;
#endif

    //
    // HACKHACK - do some crude timer support
    //            but not if called from KdSetOwedBreakpoints()
    //

    if (TrapFrame) {
        KdTimerStop = KdpQueryPerformanceCounter (TrapFrame);
        KdTimerDifference.QuadPart = KdTimerStop.QuadPart - KdTimerStart.QuadPart;
    } else {
        KdTimerStop.QuadPart = 0;
    }

    //
    // Save the current IRQL in the Prcb so the debugger can extract it
    // later on for debugging purposes.
    //

    KeGetCurrentPrcb()->DebuggerSavedIRQL = KeGetCurrentIrql();

    //
    // Freeze all other processors, raise IRQL to HIGH_LEVEL, and save debug
    // port state.  We lock the port so that KdPollBreakin and a debugger
    // operation don't interfere with each other.
    //

    Enable = KeFreezeExecution(TrapFrame, ExceptionFrame);
    KdpPortLocked = KeTryToAcquireSpinLockAtDpcLevel(&KdpDebuggerLock);
    KdSave(FALSE);
    KdEnteredDebugger = TRUE;

#if DBG

    if ((KiFreezeFlag & FREEZE_BACKUP) != 0) {
        DPRINT(("FreezeLock was jammed!  Backup SpinLock was used!\n"));
    }

    if ((KiFreezeFlag & FREEZE_SKIPPED_PROCESSOR) != 0) {
        DPRINT(("Some processors not frozen in debugger!\n"));
    }

    if (KdpPortLocked == FALSE) {
        DPRINT(("Port lock was not acquired!\n"));
    }

#endif

    return Enable;
}

VOID
KdExitDebugger(
    IN BOOLEAN Enable
    )

/*++

Routine Description:

    This function is used to exit the kernel debugger. It is the reverse
    of KdEnterDebugger.

Arguments:

    Enable - Supplies the previous interrupt enable which is to be restored.

Return Value:

    None.

--*/

{
#if !defined(_TRUSTED_WINDOWS_)
    ULONG Pending;
#endif

    //
    // restore stuff and exit
    //

    KdRestore(FALSE);
    if (KdpPortLocked) {
        KdpPortUnlock();
    }

    KeThawExecution(Enable);

    //
    // Do some crude timer support.  If KdEnterDebugger didn't
    // Query the performance counter, then don't do it here either.
    //

    if (KdTimerStop.QuadPart == 0) {
        KdTimerStart = KdTimerStop;
    } else {
        KdTimerStart = KeQueryPerformanceCounter(NULL);
    }

    //
    // Process a time slip
    //

#if !defined(_TRUSTED_WINDOWS_)
    if (!PoHiberInProgress) {

        Pending = InterlockedIncrement( (PLONG) &KdpTimeSlipPending);

        //
        // If there's wasn't a time slip pending, queue the DPC to handle it
        //

        if (Pending == 1) {
            InterlockedIncrement( (PLONG) &KdpTimeSlipPending);
            KeInsertQueueDpc(&KdpTimeSlipDpc, NULL, NULL);
        }
    }
#endif

    return;
}


#if !defined(_TRUSTED_WINDOWS_)

VOID
KdUpdateTimeSlipEvent(
    PVOID Event
    )

/*++

Routine Description:

    Update the reference to an event object which will be signalled when
    the debugger has caused the system clock to skew.

Arguments:

    Event - Supplies a pointer to an event object

Return Value:

    None

--*/

{
    KIRQL OldIrql;

    KeAcquireSpinLock(&KdpTimeSlipEventLock, &OldIrql);

    //
    // Dereference the old event and forget about it.
    // Remember the new event if there is one.
    //

    if (KdpTimeSlipEvent != NULL) {
        ObDereferenceObject(KdpTimeSlipEvent);
    }

    KdpTimeSlipEvent = Event;

    KeReleaseSpinLock(&KdpTimeSlipEventLock, OldIrql);
}

VOID
KdpTimeSlipDpcRoutine (
    PKDPC Dpc,
    PVOID DeferredContext,
    PVOID SystemArgument1,
    PVOID SystemArgument2
    )
{
    LONG OldCount, NewCount, j;

    UNREFERENCED_PARAMETER (Dpc);
    UNREFERENCED_PARAMETER (DeferredContext);
    UNREFERENCED_PARAMETER (SystemArgument1);
    UNREFERENCED_PARAMETER (SystemArgument2);

    //
    // Reset pending count.  If the current count is 1, then clear
    // the pending count.  if the current count is greater then 1,
    // then set to one and update the time now.
    //

    j = KdpTimeSlipPending;
    do {
        OldCount = j;
        NewCount = OldCount > 1 ? 1 : 0;

        j = InterlockedCompareExchange((PLONG)&KdpTimeSlipPending, NewCount, OldCount);

    } while (j != OldCount);

    //
    // If new count is non-zero, then process a time slip now
    //

    if (NewCount) {
        ExQueueWorkItem(&KdpTimeSlipWorkItem, DelayedWorkQueue);
    }
}

VOID
KdpTimeSlipWork (
    IN PVOID Context
    )
{
    KIRQL               OldIrql;
    LARGE_INTEGER       DueTime;

    UNREFERENCED_PARAMETER (Context);

    //
    // Update time from the real time clock.
    // If the lock is held by somebody else, don't bother as it's not worth
    // tying up a worker thread.
    //

    if (ExAcquireTimeRefreshLock(FALSE)) {
        ExUpdateSystemTimeFromCmos (FALSE, 0);
        ExReleaseTimeRefreshLock();

        //
        // If there's a time service installed, signal it's time slip event
        //

        KeAcquireSpinLock(&KdpTimeSlipEventLock, &OldIrql);
        if (KdpTimeSlipEvent) {
            KeSetEvent (KdpTimeSlipEvent, 0, FALSE);
        }
        KeReleaseSpinLock(&KdpTimeSlipEventLock, OldIrql);

        //
        // Insert a forced delay between time slip operations
        //

        DueTime.QuadPart = -1800000000;
        KeSetTimer (&KdpTimeSlipTimer, DueTime, &KdpTimeSlipDpc);
    }
}

#endif // !defined(_TRUSTED_WINDOWS_)


#if i386

#if 0
#define INTBP_PRINT(Args) DPRINT(Args)
#else
#define INTBP_PRINT(Args)
#endif

VOID
InternalBreakpointCheck (
    PKDPC Dpc,
    PVOID DeferredContext,
    PVOID SystemArgument1,
    PVOID SystemArgument2
    )
{
    LARGE_INTEGER dueTime;
    ULONG i;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(DeferredContext);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    dueTime.LowPart = (ULONG)(-1 * 10 * 1000 * 1000);
    dueTime.HighPart = -1;

    KeSetTimer(
        &InternalBreakpointTimer,
        dueTime,
        &InternalBreakpointCheckDpc
        );

    for ( i = 0 ; i < KdpNumInternalBreakpoints; i++ ) {
        if ( !(KdpInternalBPs[i].Flags & DBGKD_INTERNAL_BP_FLAG_INVALID) &&
             (KdpInternalBPs[i].Flags & DBGKD_INTERNAL_BP_FLAG_COUNTONLY) ) {

            PDBGKD_INTERNAL_BREAKPOINT b = KdpInternalBPs + i;
            ULONG callsThisPeriod;

            callsThisPeriod = b->Calls - b->CallsLastCheck;
            if ( callsThisPeriod > b->MaxCallsPerPeriod ) {
                b->MaxCallsPerPeriod = callsThisPeriod;
            }
            b->CallsLastCheck = b->Calls;
        }
    }

    return;

} // InternalBreakpointCheck


VOID
KdSetInternalBreakpoint (
    IN PDBGKD_MANIPULATE_STATE64 m
    )

/*++

Routine Description:

    This function sets an internal breakpoint.  "Internal breakpoint"
    means one in which control is not returned to the kernel debugger at
    all, but rather just update internal counting routines and resume.

Arguments:

    m - Supplies the state manipulation message.

Return Value:

    None.
--*/

{
    ULONG i;
    PDBGKD_INTERNAL_BREAKPOINT bp = NULL;
    ULONG savedFlags;

    for ( i = 0 ; i < KdpNumInternalBreakpoints; i++ ) {
        if ( KdpInternalBPs[i].Addr ==
                            m->u.SetInternalBreakpoint.BreakpointAddress ) {
            bp = &KdpInternalBPs[i];
            break;
        }
    }

    if ( !bp ) {
        for ( i = 0; i < KdpNumInternalBreakpoints; i++ ) {
            if ( KdpInternalBPs[i].Flags & DBGKD_INTERNAL_BP_FLAG_INVALID ) {
                bp = &KdpInternalBPs[i];
                break;
            }
        }
    }

    if ( !bp ) {
        if ( KdpNumInternalBreakpoints >= DBGKD_MAX_INTERNAL_BREAKPOINTS ) {
            return; // no space.  Probably should report error.
        }
        bp = &KdpInternalBPs[KdpNumInternalBreakpoints++];
        bp->Flags |= DBGKD_INTERNAL_BP_FLAG_INVALID; // force initialization
    }

    if ( bp->Flags & DBGKD_INTERNAL_BP_FLAG_INVALID ) {
        if ( m->u.SetInternalBreakpoint.Flags &
                                        DBGKD_INTERNAL_BP_FLAG_INVALID ) {
            return; // tried clearing a non-existant BP.  Ignore the request
        }
        bp->Calls = bp->MaxInstructions = bp->TotalInstructions = 0;
        bp->CallsLastCheck = bp->MaxCallsPerPeriod = 0;
        bp->MinInstructions = 0xffffffff;
        bp->Handle = 0;
        bp->Thread = 0;
    }

    savedFlags = bp->Flags;
    bp->Flags = m->u.SetInternalBreakpoint.Flags; // this could possibly invalidate the BP
    bp->Addr = m->u.SetInternalBreakpoint.BreakpointAddress;

    if ( bp->Flags & (DBGKD_INTERNAL_BP_FLAG_INVALID |
                      DBGKD_INTERNAL_BP_FLAG_SUSPENDED) ) {

        if ( (bp->Flags & DBGKD_INTERNAL_BP_FLAG_INVALID) &&
             (bp->Thread != 0) ) {
            // The breakpoint is active; defer its deletion
            bp->Flags &= ~DBGKD_INTERNAL_BP_FLAG_INVALID;
            bp->Flags |= DBGKD_INTERNAL_BP_FLAG_DYING;
        }

        // This is really a CLEAR bp request.

        if ( bp->Handle != 0 ) {
            KdpDeleteBreakpoint( bp->Handle );
        }
        bp->Handle = 0;

        return;
    }

    // now set the real breakpoint and remember its handle.

    if ( savedFlags & (DBGKD_INTERNAL_BP_FLAG_INVALID |
                       DBGKD_INTERNAL_BP_FLAG_SUSPENDED) ) {
        // breakpoint was invalid; activate it now
        bp->Handle = KdpAddBreakpoint( (PVOID)(ULONG_PTR)bp->Addr );

        INTBP_PRINT(("Added intbp %d of %d at %I64x, flags %x, handle %x\n",
                     (ULONG)(bp - KdpInternalBPs), KdpNumInternalBreakpoints,
                     bp->Addr, bp->Flags, bp->Handle));
    }

    if ( BreakpointsSuspended ) {
        KdpSuspendBreakpoint( bp->Handle );
    }

} // KdSetInternalBreakpoint

NTSTATUS
KdGetTraceInformation(
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
    )

/*++

Routine Description:

    This function gets data about an internal breakpoint and returns it
    in a buffer provided for it.  It is designed to be called from
    NTQuerySystemInformation.  It is morally equivalent to GetInternalBP
    except that it communicates locally, and returns all the breakpoints
    at once.

Arguments:

    SystemInforamtion - the buffer into which to write the result.
    SystemInformationLength - the maximum length to write
    RetrunLength - How much data was really written

Return Value:

    None.

--*/

{
    ULONG numEntries = 0;
    ULONG i = 0;
    PDBGKD_GET_INTERNAL_BREAKPOINT64 outPtr;

    for ( i = 0; i < KdpNumInternalBreakpoints; i++ ) {
        if ( !(KdpInternalBPs[i].Flags & DBGKD_INTERNAL_BP_FLAG_INVALID) ) {
            numEntries++;
        }
    }

    *ReturnLength = numEntries * sizeof(DBGKD_GET_INTERNAL_BREAKPOINT64);
    if ( *ReturnLength > SystemInformationLength ) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    //
    // We've got enough space.  Copy it in.
    //

    outPtr = (PDBGKD_GET_INTERNAL_BREAKPOINT64)SystemInformation;
    for ( i = 0; i < KdpNumInternalBreakpoints; i++ ) {
        if ( !(KdpInternalBPs[i].Flags & DBGKD_INTERNAL_BP_FLAG_INVALID) ) {
            outPtr->BreakpointAddress = KdpInternalBPs[i].Addr;
            outPtr->Flags = KdpInternalBPs[i].Flags;
            outPtr->Calls = KdpInternalBPs[i].Calls;
            outPtr->MaxCallsPerPeriod = KdpInternalBPs[i].MaxCallsPerPeriod;
            outPtr->MinInstructions = KdpInternalBPs[i].MinInstructions;
            outPtr->MaxInstructions = KdpInternalBPs[i].MaxInstructions;
            outPtr->TotalInstructions = KdpInternalBPs[i].TotalInstructions;
            outPtr++;
        }
    }

    return STATUS_SUCCESS;

} // KdGetTraceInformation

VOID
KdGetInternalBreakpoint(
    IN PDBGKD_MANIPULATE_STATE64 m
    )

/*++

Routine Description:

    This function gets data about an internal breakpoint and returns it
    to the calling debugger.

Arguments:

    m - Supplies the state manipulation message.

Return Value:

    None.

--*/

{
    ULONG i;
    PDBGKD_INTERNAL_BREAKPOINT bp = NULL;
    STRING messageHeader;

    messageHeader.Length = sizeof(*m);
    messageHeader.Buffer = (PCHAR)m;

    for ( i = 0; i < KdpNumInternalBreakpoints; i++ ) {
        if ( !(KdpInternalBPs[i].Flags & (DBGKD_INTERNAL_BP_FLAG_INVALID |
                                          DBGKD_INTERNAL_BP_FLAG_SUSPENDED)) &&
             (KdpInternalBPs[i].Addr ==
                        m->u.GetInternalBreakpoint.BreakpointAddress) ) {
            bp = &KdpInternalBPs[i];
            break;
        }
    }

    if ( !bp ) {
        m->u.GetInternalBreakpoint.Flags = DBGKD_INTERNAL_BP_FLAG_INVALID;
        m->u.GetInternalBreakpoint.Calls = 0;
        m->u.GetInternalBreakpoint.MaxCallsPerPeriod = 0;
        m->u.GetInternalBreakpoint.MinInstructions = 0;
        m->u.GetInternalBreakpoint.MaxInstructions = 0;
        m->u.GetInternalBreakpoint.TotalInstructions = 0;
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    } else {
        m->u.GetInternalBreakpoint.Flags = bp->Flags;
        m->u.GetInternalBreakpoint.Calls = bp->Calls;
        m->u.GetInternalBreakpoint.MaxCallsPerPeriod = bp->MaxCallsPerPeriod;
        m->u.GetInternalBreakpoint.MinInstructions = bp->MinInstructions;
        m->u.GetInternalBreakpoint.MaxInstructions = bp->MaxInstructions;
        m->u.GetInternalBreakpoint.TotalInstructions = bp->TotalInstructions;
        m->ReturnStatus = STATUS_SUCCESS;
    }

    m->ApiNumber = DbgKdGetInternalBreakPointApi;

    KdSendPacket(PACKET_TYPE_KD_STATE_MANIPULATE,
                 &messageHeader,
                 NULL,
                 &KdpContext
                 );

    return;

} // KdGetInternalBreakpoint
#endif // i386

KCONTINUE_STATUS
KdpSendWaitContinue (
    IN ULONG OutPacketType,
    IN PSTRING OutMessageHeader,
    IN PSTRING OutMessageData OPTIONAL,
    IN OUT PCONTEXT ContextRecord
    )

/*++

Routine Description:

    This function sends a packet, and then waits for a continue message.
    BreakIns received while waiting will always cause a resend of the
    packet originally sent out.  While waiting, manipulate messages
    will be serviced.

    A resend always resends the original event sent to the debugger,
    not the last response to some debugger command.

Arguments:

    OutPacketType - Supplies the type of packet to send.

    OutMessageHeader - Supplies a pointer to a string descriptor that describes
        the message information.

    OutMessageData - Supplies a pointer to a string descriptor that describes
        the optional message data.

    ContextRecord - Exception context

Return Value:

    A value of TRUE is returned if the continue message indicates
    success, Otherwise, a value of FALSE is returned.

--*/

{

    ULONG Length;
    STRING MessageData;
    STRING MessageHeader;
    DBGKD_MANIPULATE_STATE64 ManipulateState;
    ULONG ReturnCode;
    NTSTATUS Status;
    KCONTINUE_STATUS ContinueStatus;

    //
    // Loop servicing state manipulation message until a continue message
    // is received.
    //

    MessageHeader.MaximumLength = sizeof(DBGKD_MANIPULATE_STATE64);
    MessageHeader.Buffer = (PCHAR)&ManipulateState;
    MessageData.MaximumLength = KDP_MESSAGE_BUFFER_SIZE;
    MessageData.Buffer = (PCHAR)KdpMessageBuffer;
    KdpContextSent = FALSE;

ResendPacket:

    //
    // Send event notification packet to debugger on host.  Come back
    // here any time we see a breakin sequence.
    //

    KdSendPacket(
        OutPacketType,
        OutMessageHeader,
        OutMessageData,
        &KdpContext
        );

    //
    // After sending packet, if there is no response from debugger
    // AND the packet is for reporting symbol (un)load, the debugger
    // will be declared to be not present.  Note If the packet is for
    // reporting exception, the KdSendPacket will never stop.
    //

    if (KdDebuggerNotPresent) {
        return ContinueSuccess;
    }

    while (TRUE) {

        //
        // Wait for State Manipulate Packet without timeout.
        //

        do {

            ReturnCode = KdReceivePacket(
                            PACKET_TYPE_KD_STATE_MANIPULATE,
                            &MessageHeader,
                            &MessageData,
                            &Length,
                            &KdpContext
                            );
            if (ReturnCode == (USHORT)KDP_PACKET_RESEND) {
                goto ResendPacket;
            }
        } while (ReturnCode == KDP_PACKET_TIMEOUT);

        //
        // Switch on the return message API number.
        //

        switch (ManipulateState.ApiNumber) {

        case DbgKdReadVirtualMemoryApi:
            KdpReadVirtualMemory(&ManipulateState,&MessageData,ContextRecord);
            break;
#if 0
        case DbgKdReadVirtualMemory64Api:
            KdpReadVirtualMemory64(&ManipulateState,&MessageData,ContextRecord);
            break;
#endif
        case DbgKdWriteVirtualMemoryApi:
            KdpWriteVirtualMemory(&ManipulateState,&MessageData,ContextRecord);
            break;
#if 0
        case DbgKdWriteVirtualMemory64Api:
            KdpWriteVirtualMemory64(&ManipulateState,&MessageData,ContextRecord);
            break;
#endif

        case DbgKdCheckLowMemoryApi:
            KdpCheckLowMemory (&ManipulateState);
            break;

        case DbgKdReadPhysicalMemoryApi:
            KdpReadPhysicalMemory(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdWritePhysicalMemoryApi:
            KdpWritePhysicalMemory(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdGetContextApi:
            KdpGetContext(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdSetContextApi:
            KdpSetContext(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdWriteBreakPointApi:
            KdpWriteBreakpoint(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdRestoreBreakPointApi:
            KdpRestoreBreakpoint(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdReadControlSpaceApi:
            KdpReadControlSpace(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdWriteControlSpaceApi:
            KdpWriteControlSpace(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdReadIoSpaceApi:
            KdpReadIoSpace(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdWriteIoSpaceApi:
            KdpWriteIoSpace(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdReadIoSpaceExtendedApi:
            KdpReadIoSpaceExtended(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdWriteIoSpaceExtendedApi:
            KdpWriteIoSpaceExtended(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdReadMachineSpecificRegister:
            KdpReadMachineSpecificRegister(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdWriteMachineSpecificRegister:
            KdpWriteMachineSpecificRegister(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdGetBusDataApi:
            KdpGetBusData(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdSetBusDataApi:
            KdpSetBusData(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdContinueApi:
            if (NT_SUCCESS(ManipulateState.u.Continue.ContinueStatus) != FALSE) {
                return ContinueSuccess;
            } else {
                return ContinueError;
            }
            break;

        case DbgKdContinueApi2:
            if (NT_SUCCESS(ManipulateState.u.Continue2.ContinueStatus) != FALSE) {
                KdpGetStateChange(&ManipulateState,ContextRecord);
                return ContinueSuccess;
            } else {
                return ContinueError;
            }
            break;

        case DbgKdRebootApi:
            HalReturnToFirmware(HalRebootRoutine);
            break;

#if defined(i386)
        case DbgKdSetSpecialCallApi:
            KdSetSpecialCall(&ManipulateState,ContextRecord);
            break;

        case DbgKdClearSpecialCallsApi:
            KdClearSpecialCalls();
            break;

        case DbgKdSetInternalBreakPointApi:
            KdSetInternalBreakpoint(&ManipulateState);
            break;

        case DbgKdGetInternalBreakPointApi:
            KdGetInternalBreakpoint(&ManipulateState);
            break;

        case DbgKdClearAllInternalBreakpointsApi:
            KdpNumInternalBreakpoints = 0;
            break;

#endif // i386

        case DbgKdGetVersionApi:
            KdpGetVersion(&ManipulateState);
            break;

        case DbgKdCauseBugCheckApi:
            KdpCauseBugCheck(&ManipulateState);
            break;

        case DbgKdPageInApi:
            KdpNotSupported(&ManipulateState);
            break;

        case DbgKdWriteBreakPointExApi:
            Status = KdpWriteBreakPointEx(&ManipulateState,
                                          &MessageData,
                                          ContextRecord);
            if (Status) {
                ManipulateState.ApiNumber = DbgKdContinueApi;
                ManipulateState.u.Continue.ContinueStatus = Status;
                return ContinueError;
            }
            break;

        case DbgKdRestoreBreakPointExApi:
            KdpRestoreBreakPointEx(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdSwitchProcessor:
            KdRestore(FALSE);
            ContinueStatus = KeSwitchFrozenProcessor(ManipulateState.Processor);
            KdSave(FALSE);
            return ContinueStatus;

        case DbgKdSearchMemoryApi:
            KdpSearchMemory(&ManipulateState, &MessageData, ContextRecord);
            break;

        case DbgKdFillMemoryApi:
            KdpFillMemory(&ManipulateState, &MessageData, ContextRecord);
            break;
            
        case DbgKdQueryMemoryApi:
            KdpQueryMemory(&ManipulateState, ContextRecord);
            break;
            
            //
            // Invalid message.
            //

        default:
            MessageData.Length = 0;
            ManipulateState.ReturnStatus = STATUS_UNSUCCESSFUL;
            KdSendPacket(PACKET_TYPE_KD_STATE_MANIPULATE, &MessageHeader, &MessageData, &KdpContext);
            break;
        }
    }
}

VOID
KdpReadVirtualMemory(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response to a read virtual memory 32-bit
    state manipulation message. Its function is to read virtual memory
    and return.

Arguments:

    m - Supplies a pointer to the state manipulation message.

    AdditionalData - Supplies a pointer to a descriptor for the data to read.

    Context - Supplies a pointer to the current context.

Return Value:

    None.

--*/

{
    ULONG Length;
    STRING MessageHeader;

    UNREFERENCED_PARAMETER (Context);

    //
    // Trim the transfer count to fit in a single message.
    //

    Length = m->u.ReadMemory.TransferCount;
    if (Length > (PACKET_MAX_SIZE - sizeof(DBGKD_MANIPULATE_STATE64))) {
        Length = PACKET_MAX_SIZE - sizeof(DBGKD_MANIPULATE_STATE64);
    }

    //
    // Move the data to the destination buffer.
    //

    m->ReturnStatus =
        KdpCopyMemoryChunks(m->u.ReadMemory.TargetBaseAddress,
                            AdditionalData->Buffer,
                            Length,
                            0,
                            MMDBG_COPY_UNSAFE,
                            &Length);

    //
    // Set the actual number of bytes read, initialize the message header,
    // and send the reply packet to the host debugger.
    //

    AdditionalData->Length = (USHORT)Length;
    m->u.ReadMemory.ActualBytesRead = Length;

    MessageHeader.Length = sizeof(DBGKD_MANIPULATE_STATE64);
    MessageHeader.Buffer = (PCHAR)m;
    KdSendPacket(PACKET_TYPE_KD_STATE_MANIPULATE,
                 &MessageHeader,
                 AdditionalData,
                 &KdpContext);

    return;
}

VOID
KdpWriteVirtualMemory(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a write virtual memory 32-bit
    state manipulation message. Its function is to write virtual memory
    and return.

Arguments:

    m - Supplies a pointer to the state manipulation message.

    AdditionalData - Supplies a pointer to a descriptor for the data to write.

    Context - Supplies a pointer to the current context.

Return Value:

    None.

--*/

{

    STRING MessageHeader;

    UNREFERENCED_PARAMETER (Context);

    //
    // Move the data to the destination buffer.
    //

    m->ReturnStatus =
        KdpCopyMemoryChunks(m->u.WriteMemory.TargetBaseAddress,
                            AdditionalData->Buffer,
                            AdditionalData->Length,
                            0,
                            MMDBG_COPY_WRITE | MMDBG_COPY_UNSAFE,
                            &m->u.WriteMemory.ActualBytesWritten);

    //
    // Set the actual number of bytes written, initialize the message header,
    // and send the reply packet to the host debugger.
    //

    MessageHeader.Length = sizeof(DBGKD_MANIPULATE_STATE64);
    MessageHeader.Buffer = (PCHAR)m;
    KdSendPacket(PACKET_TYPE_KD_STATE_MANIPULATE,
                 &MessageHeader,
                 NULL,
                 &KdpContext);

    return;
}

VOID
KdpGetContext(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a get context state
    manipulation message.  Its function is to return the current
    context.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    STRING MessageHeader;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    if (m->Processor >= (USHORT)KeNumberProcessors) {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    } else {
        m->ReturnStatus = STATUS_SUCCESS;
        AdditionalData->Length = sizeof(CONTEXT);
        if (m->Processor == (USHORT)KeGetCurrentProcessorNumber()) {
            KdpQuickMoveMemory(AdditionalData->Buffer, (PCHAR)Context, sizeof(CONTEXT));
        } else {
            KdpQuickMoveMemory(AdditionalData->Buffer,
                          (PCHAR)&KiProcessorBlock[m->Processor]->ProcessorState.ContextFrame,
                          sizeof(CONTEXT)
                         );
        }
        KdpContextSent = TRUE;
    }

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        AdditionalData,
        &KdpContext
        );
}

VOID
KdpSetContext(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a set context state
    manipulation message.  Its function is set the current
    context.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    STRING MessageHeader;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == sizeof(CONTEXT));

    if ((m->Processor >= (USHORT)KeNumberProcessors) ||
        (KdpContextSent == FALSE)) {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    } else {
        m->ReturnStatus = STATUS_SUCCESS;
        if (m->Processor == (USHORT)KeGetCurrentProcessorNumber()) {
            KdpQuickMoveMemory((PCHAR)Context, AdditionalData->Buffer, sizeof(CONTEXT));
        } else {
            KdpQuickMoveMemory((PCHAR)&KiProcessorBlock[m->Processor]->ProcessorState.ContextFrame,
                          AdditionalData->Buffer,
                          sizeof(CONTEXT)
                         );
        }
    }

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL,
        &KdpContext
        );
}

VOID
KdpWriteBreakpoint(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a write breakpoint state
    manipulation message.  Its function is to write a breakpoint
    and return a handle to the breakpoint.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_WRITE_BREAKPOINT64 a = &m->u.WriteBreakPoint;
    STRING MessageHeader;

#if !DBG
    UNREFERENCED_PARAMETER (AdditionalData);
#endif

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    a->BreakPointHandle = KdpAddBreakpoint((PVOID)(ULONG_PTR)a->BreakPointAddress);
    if (a->BreakPointHandle != 0) {
        m->ReturnStatus = STATUS_SUCCESS;
    } else {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    }
    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL,
        &KdpContext
        );
    UNREFERENCED_PARAMETER(Context);
}

VOID
KdpRestoreBreakpoint(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a restore breakpoint state
    manipulation message.  Its function is to restore a breakpoint
    using the specified handle.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_RESTORE_BREAKPOINT a = &m->u.RestoreBreakPoint;
    STRING MessageHeader;

#if !DBG
    UNREFERENCED_PARAMETER(AdditionalData);
#endif

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);
    if (KdpDeleteBreakpoint(a->BreakPointHandle)) {
        m->ReturnStatus = STATUS_SUCCESS;
    } else {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    }
    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL,
        &KdpContext
        );
    UNREFERENCED_PARAMETER(Context);
}

#if defined(_X86_)

long
SymNumFor(
    ULONG pc
    )
{
    ULONG index;

    for (index = 0; index < NumTraceDataSyms; index++) {
        if ((TraceDataSyms[index].SymMin <= pc) &&
            (TraceDataSyms[index].SymMax > pc)) return(index);
    }
    return(-1);
}

#if 0
#define TRACE_PRINT(Args) DPRINT(Args)
#else
#define TRACE_PRINT(Args)
#endif

BOOLEAN TraceDataBufferFilled = FALSE;

void PotentialNewSymbol (ULONG pc)
{
    if (!TraceDataBufferFilled &&
        -1 != SymNumFor(pc)) {     // we've already seen this one
        TRACE_PRINT(("PNS %x repeat %d\n", pc, SymNumFor(pc)));
        return;
    }

    TraceDataBufferFilled = FALSE;

    // OK, we've got to start up a TraceDataRecord
    TraceDataBuffer[TraceDataBufferPosition].s.LevelChange = 0;

    if (-1 != SymNumFor(pc)) {
        int sym = SymNumFor(pc);
        TraceDataBuffer[TraceDataBufferPosition].s.SymbolNumber = (UCHAR) sym;
        KdpCurrentSymbolStart = TraceDataSyms[sym].SymMin;
        KdpCurrentSymbolEnd = TraceDataSyms[sym].SymMax;

        TRACE_PRINT(("PNS %x repeat %d at %d\n",
                     pc, sym, TraceDataBufferPosition));
        return;  // we've already seen this one
    }

    TraceDataSyms[NextTraceDataSym].SymMin = KdpCurrentSymbolStart;
    TraceDataSyms[NextTraceDataSym].SymMax = KdpCurrentSymbolEnd;

    TraceDataBuffer[TraceDataBufferPosition].s.SymbolNumber = NextTraceDataSym;

    // Bump the "next" pointer, wrapping if necessary.  Also bump the
    // "valid" pointer if we need to.
    NextTraceDataSym = (NextTraceDataSym + 1) % 256;
    if (NumTraceDataSyms < NextTraceDataSym) {
        NumTraceDataSyms = NextTraceDataSym;
    }

    TRACE_PRINT(("PNS %x in %x - %x, next %d, num %d\n", pc,
                 KdpCurrentSymbolStart, KdpCurrentSymbolEnd,
                 NextTraceDataSym, NumTraceDataSyms));
}

void DumpTraceData(PSTRING MessageData)
{
    TraceDataBuffer[0].LongNumber = TraceDataBufferPosition;
    MessageData->Length =
        (USHORT)(sizeof(TraceDataBuffer[0]) * TraceDataBufferPosition);
    MessageData->Buffer = (PVOID)TraceDataBuffer;
    TRACE_PRINT(("DumpTraceData returns %d records\n",
                 TraceDataBufferPosition));
    TraceDataBufferPosition = 1;
}

BOOLEAN
TraceDataRecordCallInfo(
    ULONG InstructionsTraced,
    LONG CallLevelChange,
    ULONG pc
    )
{
    // We've just exited a symbol scope.  The InstructionsTraced number goes
    // with the old scope, the CallLevelChange goes with the new, and the
    // pc fills in the symbol for the new TraceData record.

    long SymNum = SymNumFor(pc);

    if (KdpNextCallLevelChange != 0) {
        TraceDataBuffer[TraceDataBufferPosition].s.LevelChange =
                                                (char) KdpNextCallLevelChange;
        KdpNextCallLevelChange = 0;
    }


    if (InstructionsTraced >= TRACE_DATA_INSTRUCTIONS_BIG) {
       TraceDataBuffer[TraceDataBufferPosition].s.Instructions =
           TRACE_DATA_INSTRUCTIONS_BIG;
       TraceDataBuffer[TraceDataBufferPosition+1].LongNumber =
           InstructionsTraced;
       TraceDataBufferPosition += 2;
    } else {
       TraceDataBuffer[TraceDataBufferPosition].s.Instructions =
           (unsigned short)InstructionsTraced;
       TraceDataBufferPosition++;
    }

    if ((TraceDataBufferPosition + 2 >= TRACE_DATA_BUFFER_MAX_SIZE) ||
        (-1 == SymNum)) {
        if (TraceDataBufferPosition +2 >= TRACE_DATA_BUFFER_MAX_SIZE) {
            TraceDataBufferFilled = TRUE;
        }
       KdpNextCallLevelChange = CallLevelChange;
       TRACE_PRINT(("TDRCI nosym %x, lc %d, pos %d\n", pc, CallLevelChange,
                    TraceDataBufferPosition));
       return FALSE;
    }

    TraceDataBuffer[TraceDataBufferPosition].s.LevelChange =(char)CallLevelChange;
    TraceDataBuffer[TraceDataBufferPosition].s.SymbolNumber = (UCHAR) SymNum;
    KdpCurrentSymbolStart = TraceDataSyms[SymNum].SymMin;
    KdpCurrentSymbolEnd = TraceDataSyms[SymNum].SymMax;

    TRACE_PRINT(("TDRCI sym %d for %x, %x - %x, lc %d, pos %d\n", SymNum, pc,
                 KdpCurrentSymbolStart, KdpCurrentSymbolEnd, CallLevelChange,
                 TraceDataBufferPosition));
    return TRUE;
}

BOOLEAN
SkippingWhichBP (
    PVOID thread,
    PULONG BPNum
    )

/*
 * Return TRUE iff the pc corresponds to an internal breakpoint
 * that has just been replaced for execution.  If TRUE, then return
 * the breakpoint number in BPNum.
 */

{
    ULONG index;

    if (!IntBPsSkipping) return FALSE;

    for (index = 0; index < KdpNumInternalBreakpoints; index++) {
        if (!(KdpInternalBPs[index].Flags & DBGKD_INTERNAL_BP_FLAG_INVALID) &&
            (KdpInternalBPs[index].Thread == thread)) {
            *BPNum = index;
            return TRUE;
        }
    }
    return FALSE; // didn't match any
}


NTSTATUS
KdQuerySpecialCalls (
    IN PDBGKD_MANIPULATE_STATE64 m,
    ULONG Length,
    PULONG RequiredLength
    )
{
    *RequiredLength = sizeof(DBGKD_MANIPULATE_STATE64) +
                        (sizeof(ULONG) * KdNumberOfSpecialCalls);

    if ( Length < *RequiredLength ) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    m->u.QuerySpecialCalls.NumberOfSpecialCalls = KdNumberOfSpecialCalls;
    KdpQuickMoveMemory(
        (PCHAR)(m + 1),
        (PCHAR)KdSpecialCalls,
        sizeof(ULONG) * KdNumberOfSpecialCalls
        );

    return STATUS_SUCCESS;

} // KdQuerySpecialCalls


VOID
KdSetSpecialCall (
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PCONTEXT ContextRecord
    )

/*++

Routine Description:

    This function sets the addresses of the "special" call addresses
    that the watchtrace facility pushes back to the kernel debugger
    rather than stepping through.

Arguments:

    m - Supplies the state manipulation message.

Return Value:

    None.
--*/

{
    if ( KdNumberOfSpecialCalls >= DBGKD_MAX_SPECIAL_CALLS ) {
        return; // too bad
    }

    KdSpecialCalls[KdNumberOfSpecialCalls++] = (ULONG_PTR)m->u.SetSpecialCall.SpecialCall;

    NextTraceDataSym = 0;
    NumTraceDataSyms = 0;
    KdpNextCallLevelChange = 0;
    if (ContextRecord && !InstrCountInternal) {
        InitialSP = ContextRecord->Esp;
    }

} // KdSetSpecialCall


VOID
KdClearSpecialCalls (
    VOID
    )

/*++

Routine Description:

    This function clears the addresses of the "special" call addresses
    that the watchtrace facility pushes back to the kernel debugger
    rather than stepping through.

Arguments:

    None.

Return Value:

    None.

--*/

{
    KdNumberOfSpecialCalls = 0;
    return;

} // KdClearSpecialCalls


BOOLEAN
KdpCheckTracePoint(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN OUT PCONTEXT ContextRecord
    )
{
    ULONG pc = (ULONG)CONTEXT_TO_PROGRAM_COUNTER(ContextRecord);
    LONG BpNum;
    ULONG SkippedBPNum;
    BOOLEAN AfterSC = FALSE;

    if (ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP) {
        if (WatchStepOverSuspended) {
            //
            //  For background, see the comment below where WSOThread is
            //  wrong.  We've now stepped over the breakpoint in the non-traced
            //  thread, and need to replace it and restart the non-traced
            //  thread at full speed.
            //

            WatchStepOverHandle = KdpAddBreakpoint((PVOID)WatchStepOverBreakAddr);
            WatchStepOverSuspended = FALSE;
            ContextRecord->EFlags &= ~0x100L; /* clear trace flag */
            return TRUE; // resume non-traced thread at full speed
        }

        if ((!SymbolRecorded) && (KdpCurrentSymbolStart != 0) && (KdpCurrentSymbolEnd != 0)) {
            //
            //  We need to use oldpc here, because this may have been
            //  a 1 instruction call.  We've ALREADY executed the instruction
            //  that the new symbol is for, and if the pc has moved out of
            //  range, we might mess up.  Hence, use the pc from when
            //  SymbolRecorded was set.  Yuck.
            //

            PotentialNewSymbol(oldpc);
            SymbolRecorded = TRUE;
        }

        if (!InstrCountInternal &&
            SkippingWhichBP((PVOID)KeGetCurrentThread(),&SkippedBPNum)) {

            //
            //  We just single-stepped over a temporarily removed internal
            //  breakpoint.
            //  If it's a COUNTONLY breakpoint:
            //      Put the breakpoint instruction back and resume
            //      regular execution.
            //

            if (KdpInternalBPs[SkippedBPNum].Flags &
                DBGKD_INTERNAL_BP_FLAG_COUNTONLY) {

                IntBPsSkipping --;

                KdpRestoreAllBreakpoints();

                ContextRecord->EFlags &= ~0x100L;  // Clear trace flag
                KdpInternalBPs[SkippedBPNum].Thread = 0;

                if (KdpInternalBPs[SkippedBPNum].Flags &
                        DBGKD_INTERNAL_BP_FLAG_DYING) {
                    KdpDeleteBreakpoint(KdpInternalBPs[SkippedBPNum].Handle);
                    KdpInternalBPs[SkippedBPNum].Flags |=
                            DBGKD_INTERNAL_BP_FLAG_INVALID; // bye, bye
                }

                return TRUE;
            }

            //
            //  If it's not:
            //      set up like it's a ww, by setting Begin and KdpCurrentSymbolEnd
            //      and bop off into single step land.  We probably ought to
            //      disable all breakpoints here, too, so that we don't do
            //      anything foul like trying two non-COUNTONLY's at the
            //      same time or something...
            //

            KdpCurrentSymbolEnd = 0;
            KdpCurrentSymbolStart = (ULONG_PTR) KdpInternalBPs[SkippedBPNum].ReturnAddress;

            ContextRecord->EFlags |= 0x100L; /* Trace on. */
            InitialSP = ContextRecord->Esp;

            InstructionsTraced = 1;  /* Count the initial call instruction. */
            InstrCountInternal = TRUE;
        }

    } /* if single step */
    else if (ExceptionRecord->ExceptionCode == STATUS_BREAKPOINT) {
        if (WatchStepOver && pc == WatchStepOverBreakAddr) {
            //
            //  This is a breakpoint after completion of a "special call"
            //

            if ((WSOThread != (PVOID)KeGetCurrentThread()) ||
                (WSOEsp + 0x20 < ContextRecord->Esp) ||
                (ContextRecord->Esp + 0x20 < WSOEsp)) {
                //
                //  Here's the story up to this point: the traced thread
                //  cruised along until it it a special call.  The tracer
                //  placed a breakpoint on the instruction immediately after
                //  the special call returns and restarted the traced thread
                //  at full speed.  Then, some *other* thread hit the
                //  breakpoint.  So, to correct for this, we're going to
                //  remove the breakpoint, single step the non-traced
                //  thread one instruction, replace the breakpoint,
                //  restart the non-traced thread at full speed, and wait
                //  for the traced thread to get to this breakpoint, just
                //  like we were when this happened.  The assumption
                //  here is that the traced thread won't hit the breakpoint
                //  while it's removed, which I believe to be true, because
                //  I don't think a context switch can occur during a single
                //  step operation.
                //
                //  For extra added fun, it's possible to execute interrupt
                //  routines IN THE SAME THREAD!!!  That's why we need to keep
                //  the stack pointer as well as the thread address: the APC
                //  code can result in pushing on the stack and doing a call
                //  that's really part on an interrupt service routine in the
                //  context of the current thread.  Lovely, isn't it?
                //

                WatchStepOverSuspended = TRUE;
                KdpDeleteBreakpoint(WatchStepOverHandle);
                ContextRecord->EFlags |= 0x100L; // Set trace flag
                return TRUE; // single step "non-traced" thread
            }

            //
            //  we're in the thread we started in; resume in single-step mode
            //  to continue the trace.
            //

            WatchStepOver = FALSE;
            KdpDeleteBreakpoint(WatchStepOverHandle);
            ContextRecord->EFlags |= 0x100L; // back to single step mode
            AfterSC = TRUE; // put us into the regular watchStep code

        } else {

            for ( BpNum = 0; BpNum < (LONG) KdpNumInternalBreakpoints; BpNum++ ) {
                if ( !(KdpInternalBPs[BpNum].Flags &
                       (DBGKD_INTERNAL_BP_FLAG_INVALID |
                        DBGKD_INTERNAL_BP_FLAG_SUSPENDED) ) &&
                     ((ULONG_PTR)KdpInternalBPs[BpNum].Addr == pc) ) {
                    break;
                }
            }

            if ( BpNum < (LONG) KdpNumInternalBreakpoints ) {

                //
                //  This is an internal monitoring breakpoint.
                //  Restore the instruction and start in single-step
                //  mode so that we can retore the breakpoint once the
                //  instruction executes, or continue stepping if this isn't
                //  a COUNTONLY breakpoint.
                //

                KdpProcessInternalBreakpoint( BpNum );
                KdpInternalBPs[BpNum].Thread = (PVOID)KeGetCurrentThread();
                IntBPsSkipping ++;

                KdpSuspendAllBreakpoints();

                ContextRecord->EFlags |= 0x100L;  // Set trace flag
                if (!(KdpInternalBPs[BpNum].Flags &
                        DBGKD_INTERNAL_BP_FLAG_COUNTONLY)) {
                    KdpInternalBPs[BpNum].ReturnAddress =
                                    KdpGetReturnAddress( ContextRecord );
                }
                return TRUE;
            }
        }
    } /* if breakpoint */

//  if (AfterSC) {
//      DPRINT(( "1: KdpCurrentSymbolStart %x  KdpCurrentSymbolEnd %x\n", KdpCurrentSymbolStart, KdpCurrentSymbolEnd ));
//  }

    if ((AfterSC || ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP) &&
        KdpCurrentSymbolStart != 0 &&
        ((KdpCurrentSymbolEnd == 0 && ContextRecord->Esp <= InitialSP) ||
         (KdpCurrentSymbolStart <= pc && pc < KdpCurrentSymbolEnd))) {
        ULONG lc;
        BOOLEAN IsSpecialCall;

        //
        //  We've taken a step trace, but are still executing in the current
        //  function.  Remember that we executed an instruction and see if the
        //  instruction changes the call level.
        //

        lc = KdpLevelChange( pc, ContextRecord, &IsSpecialCall );
        InstructionsTraced++;
        CallLevelChange += lc;

        //
        //  See if instruction is a transfer to a special routine, one that we
        //  cannot trace through since it may swap contexts
        //

        if (IsSpecialCall) {

//  DPRINT( ("2: pc=%x, level change %d\n", pc, lc) );

            //
            //  We are about to transfer to a special call routine.  Since we
            //  cannot trace through this routine, we execute it atomically by
            //  setting a breakpoint at the next logical offset.
            //
            //  Note in the case of an indirect jump to a special call routine, the
            //  level change will be -1 and the next offset will be the ULONG that's
            //  on the top of the stack.
            //
            //  However, we've already adjusted the level based on this
            //  instruction.  We need to undo this except for the magic -1 call.
            //

            if (lc != -1) {
                CallLevelChange -= lc;
            }

            //
            //  Set up for stepping over a procedure
            //

            WatchStepOver = TRUE;
            WatchStepOverBreakAddr = KdpGetCallNextOffset( pc, ContextRecord );
            WSOThread = (PVOID)KeGetCurrentThread( );
            WSOEsp = ContextRecord->Esp;

            //
            //  Establish the breakpoint
            //

            WatchStepOverHandle = KdpAddBreakpoint( (PVOID)WatchStepOverBreakAddr );


            //
            //  Note that we are continuing rather than tracing and rely on hitting
            //  the breakpoint in the current thread context to resume the watch
            //  action.
            //

            ContextRecord->EFlags &= ~0x100L;
            return TRUE;
        }

        //
        //  Resume execution with the trace flag set.  Avoid going over the wire to
        //  the remote debugger.
        //

        ContextRecord->EFlags |= 0x100L;  // Set trace flag

        return TRUE;
    }

    if ((AfterSC || (ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP)) &&
        (KdpCurrentSymbolStart != 0)) {
        //
        // We're WatchTracing, but have just changed symbol range.
        // Fill in the call record and return to the debugger if
        // either we're full or the pc is outside of the known
        // symbol scopes.  Otherwise, resume stepping.
        //
        int lc;
        BOOLEAN IsSpecialCall;

        InstructionsTraced++; // don't forget to count the call/ret instruction.

//  if (AfterSC) {
//      DPRINT(( "3: InstrCountInternal: %x\n", InstrCountInternal ));
//  }

        if (InstrCountInternal) {

            // We've just finished processing a non-COUNTONLY breakpoint.
            // Record the appropriate data and resume full speed execution.

            if (SkippingWhichBP((PVOID)KeGetCurrentThread(),&SkippedBPNum)) {

                KdpInternalBPs[SkippedBPNum].Calls++;


                if (KdpInternalBPs[SkippedBPNum].MinInstructions > InstructionsTraced) {
                    KdpInternalBPs[SkippedBPNum].MinInstructions = InstructionsTraced;
                }
                if (KdpInternalBPs[SkippedBPNum].MaxInstructions < InstructionsTraced) {
                    KdpInternalBPs[SkippedBPNum].MaxInstructions = InstructionsTraced;
                }
                KdpInternalBPs[SkippedBPNum].TotalInstructions += InstructionsTraced;

                KdpInternalBPs[SkippedBPNum].Thread = 0;

                IntBPsSkipping--;
                KdpRestoreAllBreakpoints();

                if (KdpInternalBPs[SkippedBPNum].Flags &
                    DBGKD_INTERNAL_BP_FLAG_DYING) {
                    KdpDeleteBreakpoint(KdpInternalBPs[SkippedBPNum].Handle);
                    KdpInternalBPs[SkippedBPNum].Flags |=
                        DBGKD_INTERNAL_BP_FLAG_INVALID; // bye, bye
                }
            }

            KdpCurrentSymbolStart = 0;
            InstrCountInternal = FALSE;
            ContextRecord->EFlags &= ~0x100L; // clear trace flag
            return TRUE; // Back to normal execution.
        }

        if (TraceDataRecordCallInfo( InstructionsTraced, CallLevelChange, pc)) {

            //
            //  Everything was cool internally.  We can keep executing without
            //  going back to the remote debugger.
            //
            //  We have to compute lc after calling
            //  TraceDataRecordCallInfo, because LevelChange relies on
            //  KdpCurrentSymbolStart and KdpCurrentSymbolEnd corresponding to
            //  the pc.
            //

            lc = KdpLevelChange( pc, ContextRecord, &IsSpecialCall );
            InstructionsTraced = 0;
            CallLevelChange = lc;

            //
            //  See if instruction is a transfer to a special routine, one that we
            //  cannot trace through since it may swap contexts
            //

            if (IsSpecialCall) {

//  DPRINT(( "4: pc=%x, level change %d\n", pc, lc));

                //
                //  We are about to transfer to a special call routine.  Since we
                //  cannot trace through this routine, we execute it atomically by
                //  setting a breakpoint at the next logical offset.
                //
                //  Note in the case of an indirect jump to a special call routine, the
                //  level change will be -1 and the next offset will be the ULONG that's
                //  on the top of the stack.
                //
                //  However, we've already adjusted the level based on this
                //  instruction.  We need to undo this except for the magic -1 call.
                //

                if (lc != -1) {
                    CallLevelChange -= lc;
                }

                //
                //  Set up for stepping over a procedure
                //

                WatchStepOver = TRUE;
                WSOThread = (PVOID)KeGetCurrentThread();

                //
                //  Establish the breakpoint
                //

                WatchStepOverHandle =
                    KdpAddBreakpoint( (PVOID)KdpGetCallNextOffset( pc, ContextRecord ));

                //
                //  Resume execution with the trace flag set.  Avoid going over the wire to
                //  the remote debugger.
                //

                ContextRecord->EFlags &= ~0x100L;
                return TRUE;
            }

            ContextRecord->EFlags |= 0x100L; // Set trace flag
            return TRUE; // Off we go
        }

        lc = KdpLevelChange( pc, ContextRecord, &IsSpecialCall );
        InstructionsTraced = 0;
        CallLevelChange = lc;

        // We need to go back to the remote debugger.  Just fall through.

        if ((lc != 0) && IsSpecialCall) {
            // We're hosed
            DPRINT(( "Special call on first entry to symbol scope @ %x\n", pc ));
        }
    }

    SymbolRecorded = FALSE;
    oldpc = pc;

    return FALSE;
}

#endif // defined(_X86_)

VOID
KdpSetCommonState(
    IN ULONG NewState,
    IN PCONTEXT ContextRecord,
    OUT PDBGKD_ANY_WAIT_STATE_CHANGE WaitStateChange
    )
{
    PCHAR PcMemory;
    ULONG InstrCount;
    PUCHAR InstrStream;
    
    WaitStateChange->NewState = NewState;
    WaitStateChange->ProcessorLevel = KeProcessorLevel;
    WaitStateChange->Processor = (USHORT)KeGetCurrentProcessorNumber();
    WaitStateChange->NumberProcessors = (ULONG)KeNumberProcessors;
    WaitStateChange->Thread = (ULONG64)(LONG64)(LONG_PTR)KeGetCurrentThread();
    PcMemory = (PCHAR)CONTEXT_TO_PROGRAM_COUNTER(ContextRecord);
    WaitStateChange->ProgramCounter = (ULONG64)(LONG64)(LONG_PTR)PcMemory;

    RtlZeroMemory(&WaitStateChange->AnyControlReport,
                  sizeof(WaitStateChange->AnyControlReport));
    
    //
    // Copy instruction stream immediately following location of event.
    //

    InstrStream = WaitStateChange->ControlReport.InstructionStream;
    KdpCopyFromPtr(InstrStream, PcMemory, DBGKD_MAXSTREAM, &InstrCount);
    WaitStateChange->ControlReport.InstructionCount = (USHORT)InstrCount;

    //
    // Clear breakpoints in copied area.
    // If there were any breakpoints cleared, recopy the instruction area
    // without them.
    //

    if (KdpDeleteBreakpointRange(PcMemory, PcMemory + InstrCount - 1)) {
        KdpCopyFromPtr(InstrStream, PcMemory, InstrCount, &InstrCount);
    }
}

BOOLEAN
KdpSwitchProcessor (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN OUT PCONTEXT ContextRecord,
    IN BOOLEAN SecondChance
    )
{
    BOOLEAN Status;

    //
    // Save port state
    //

    KdSave(FALSE);

    //
    // Process state change for this processor
    //

    Status = KdpReportExceptionStateChange (
                ExceptionRecord,
                ContextRecord,
                SecondChance
                );

    //
    // Restore port state and return status
    //

    KdRestore(FALSE);
    return Status;
}

BOOLEAN
KdpReportExceptionStateChange (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN OUT PCONTEXT ContextRecord,
    IN BOOLEAN SecondChance
    )

/*++

Routine Description:

    This routine sends an exception state change packet to the kernel
    debugger and waits for a manipulate state message.

Arguments:

    ExceptionRecord - Supplies a pointer to an exception record.

    ContextRecord - Supplies a pointer to a context record.

    SecondChance - Supplies a boolean value that determines whether this is
        the first or second chance for the exception.

Return Value:

    A value of TRUE is returned if the exception is handled. Otherwise, a
    value of FALSE is returned.

--*/

{
    STRING MessageData;
    STRING MessageHeader;
    DBGKD_ANY_WAIT_STATE_CHANGE WaitStateChange;
    KCONTINUE_STATUS Status;

#if i386
    if (KdpCheckTracePoint(ExceptionRecord,ContextRecord)) return TRUE;
#endif

    do {

        //
        // Construct the wait state change message and message descriptor.
        //

        KdpSetCommonState(DbgKdExceptionStateChange, ContextRecord,
                          &WaitStateChange);
        
        if (sizeof(EXCEPTION_RECORD) ==
            sizeof(WaitStateChange.u.Exception.ExceptionRecord)) {
            KdpQuickMoveMemory((PCHAR)&WaitStateChange.u.Exception.ExceptionRecord,
                               (PCHAR)ExceptionRecord,
                               sizeof(EXCEPTION_RECORD));
        } else {
            ExceptionRecord32To64((PEXCEPTION_RECORD32)ExceptionRecord,
                                  &WaitStateChange.u.Exception.ExceptionRecord);
        }

        WaitStateChange.u.Exception.FirstChance = !SecondChance;

        KdpSetStateChange(&WaitStateChange,
                          ExceptionRecord,
                          ContextRecord,
                          SecondChance
                          );

        MessageHeader.Length = sizeof(WaitStateChange);
        MessageHeader.Buffer = (PCHAR)&WaitStateChange;

#if i386
        //
        // Construct the wait state change data and data descriptor.
        //

        DumpTraceData(&MessageData);
#else
        MessageData.Length = 0;
#endif

        //
        // Send packet to the kernel debugger on the host machine,
        // wait for answer.
        //

        Status = KdpSendWaitContinue(
                    PACKET_TYPE_KD_STATE_CHANGE64,
                    &MessageHeader,
                    &MessageData,
                    ContextRecord
                    );

    } while (Status == ContinueProcessorReselected) ;

    return (BOOLEAN) Status;
}


BOOLEAN
KdpReportLoadSymbolsStateChange (
    IN PSTRING PathName,
    IN PKD_SYMBOLS_INFO SymbolInfo,
    IN BOOLEAN UnloadSymbols,
    IN OUT PCONTEXT ContextRecord
    )

/*++

Routine Description:

    This routine sends a load symbols state change packet to the kernel
    debugger and waits for a manipulate state message.

Arguments:

    PathName - Supplies a pointer to the pathname of the image whose
        symbols are to be loaded.

    BaseOfDll - Supplies the base address where the image was loaded.

    ProcessId - Unique 32-bit identifier for process that is using
        the symbols.  -1 for system process.

    CheckSum - Unique 32-bit identifier from image header.

    UnloadSymbol - TRUE if the symbols that were previously loaded for
        the named image are to be unloaded from the debugger.

Return Value:

    A value of TRUE is returned if the exception is handled. Otherwise, a
    value of FALSE is returned.

--*/

{

    PSTRING AdditionalData;
    STRING MessageData;
    STRING MessageHeader;
    DBGKD_ANY_WAIT_STATE_CHANGE WaitStateChange;
    KCONTINUE_STATUS Status;

    do {

        //
        // Construct the wait state change message and message descriptor.
        //

        KdpSetCommonState(DbgKdLoadSymbolsStateChange, ContextRecord,
                          &WaitStateChange);
        KdpSetContextState(&WaitStateChange, ContextRecord);
        WaitStateChange.u.LoadSymbols.UnloadSymbols = UnloadSymbols;
        WaitStateChange.u.LoadSymbols.BaseOfDll = (ULONG64)SymbolInfo->BaseOfDll;
        WaitStateChange.u.LoadSymbols.ProcessId = (ULONG) SymbolInfo->ProcessId;
        WaitStateChange.u.LoadSymbols.CheckSum = SymbolInfo->CheckSum;
        WaitStateChange.u.LoadSymbols.SizeOfImage = SymbolInfo->SizeOfImage;
        if (ARGUMENT_PRESENT( PathName )) {
            KdpCopyFromPtr(KdpPathBuffer,
                           PathName->Buffer,
                           PathName->Length,
                           &WaitStateChange.u.LoadSymbols.PathNameLength);
            WaitStateChange.u.LoadSymbols.PathNameLength++;

            MessageData.Buffer = (PCHAR) KdpPathBuffer;
            MessageData.Length = (USHORT)WaitStateChange.u.LoadSymbols.PathNameLength;
            MessageData.Buffer[MessageData.Length-1] = '\0';
            AdditionalData = &MessageData;
        } else {
            WaitStateChange.u.LoadSymbols.PathNameLength = 0;
            AdditionalData = NULL;
        }

        MessageHeader.Length = sizeof(WaitStateChange);
        MessageHeader.Buffer = (PCHAR)&WaitStateChange;

        //
        // Send packet to the kernel debugger on the host machine, wait
        // for the reply.
        //

        Status = KdpSendWaitContinue(
                    PACKET_TYPE_KD_STATE_CHANGE64,
                    &MessageHeader,
                    AdditionalData,
                    ContextRecord
                    );

    } while (Status == ContinueProcessorReselected);

    return (BOOLEAN) Status;
}


VOID
KdpReportCommandStringStateChange (
    IN PSTRING Name,
    IN PSTRING Command,
    IN OUT PCONTEXT ContextRecord
    )

/*++

Routine Description:

    This routine sends a command string packet to the kernel
    debugger and waits for a manipulate state message.

Arguments:

    Name - Identifies the originator of the command.

    Command - Command string.

    ContextRecord - Context information.

Return Value:

    None.

--*/

{

    STRING MessageData;
    STRING MessageHeader;
    DBGKD_ANY_WAIT_STATE_CHANGE WaitStateChange;
    KCONTINUE_STATUS Status;
    ULONG Length, Copied;

    do {

        //
        // Construct the wait state change message and message descriptor.
        //

        KdpSetCommonState(DbgKdCommandStringStateChange, ContextRecord,
                          &WaitStateChange);
        KdpSetContextState(&WaitStateChange, ContextRecord);
        RtlZeroMemory(&WaitStateChange.u.CommandString,
                      sizeof(WaitStateChange.u.CommandString));

        //
        // Transfer the string data into the message buffer.
        // The name is just a simple identifier so limit
        // it to a relatively short length.
        //

        MessageData.Buffer = (PCHAR) KdpMessageBuffer;

        if (Name->Length > 127) {
            Length = 127;
        } else {
            Length = Name->Length;
        }
        
        KdpCopyFromPtr(MessageData.Buffer, Name->Buffer, Length, &Copied);
        MessageData.Length = (USHORT)Copied + 1;
        MessageData.Buffer[MessageData.Length - 1] = '\0';

        Length = PACKET_MAX_SIZE - sizeof(WaitStateChange) -
            MessageData.Length;
        if (Command->Length < Length) {
            Length = Command->Length;
        }
        KdpCopyFromPtr(MessageData.Buffer + MessageData.Length,
                       Command->Buffer, Length, &Copied);
        Length = Copied + 1;

        MessageData.Length = (USHORT) (MessageData.Length + Length);

        MessageData.Buffer[MessageData.Length - 1] = '\0';
        
        MessageHeader.Length = sizeof(WaitStateChange);
        MessageHeader.Buffer = (PCHAR)&WaitStateChange;

        //
        // Send packet to the kernel debugger on the host machine, wait
        // for the reply.
        //

        Status = KdpSendWaitContinue(
                    PACKET_TYPE_KD_STATE_CHANGE64,
                    &MessageHeader,
                    &MessageData,
                    ContextRecord
                    );

    } while (Status == ContinueProcessorReselected);
}


VOID
KdpReadPhysicalMemory(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response to a read physical memory
    state manipulation message. Its function is to read physical memory
    and return.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_READ_MEMORY64 a = &m->u.ReadMemory;
    ULONG Length;
    STRING MessageHeader;
    ULONG MmFlags;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    //
    // make sure that nothing but a read memory message was transmitted
    //

    ASSERT(AdditionalData->Length == 0);

    //
    // Trim transfer count to fit in a single message
    //

    if (a->TransferCount > (PACKET_MAX_SIZE - sizeof(DBGKD_MANIPULATE_STATE64))) {
        Length = PACKET_MAX_SIZE - sizeof(DBGKD_MANIPULATE_STATE64);
    } else {
        Length = a->TransferCount;
    }

    //
    // Initially there was no way to control the caching
    // flags for physical memory access.  Such control
    // is necessary for robust physical access, though,
    // as the proper kind of access must be made to avoid
    // breaking the processor TBs.  Rather than create a
    // new protocol request, the ActualBytes field
    // has been overridden to pass flags on input.  Prior
    // versions of the debugger set this to zero so this
    // is a compatible change.
    //

    MmFlags = MMDBG_COPY_PHYSICAL | MMDBG_COPY_UNSAFE;
    switch(a->ActualBytesRead)
    {
    case DBGKD_CACHING_CACHED:
        MmFlags |= MMDBG_COPY_CACHED;
        break;
    case DBGKD_CACHING_UNCACHED:
        MmFlags |= MMDBG_COPY_UNCACHED;
        break;
    case DBGKD_CACHING_WRITE_COMBINED:
        MmFlags |= MMDBG_COPY_WRITE_COMBINED;
        break;
    }
    
    m->ReturnStatus =
        KdpCopyMemoryChunks(a->TargetBaseAddress,
                            AdditionalData->Buffer,
                            Length,
                            0,
                            MmFlags,
                            &Length);

    AdditionalData->Length = (USHORT)Length;
    a->ActualBytesRead = Length;

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        AdditionalData,
        &KdpContext
        );
    UNREFERENCED_PARAMETER(Context);
}



VOID
KdpWritePhysicalMemory(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response to a write physical memory
    state manipulation message. Its function is to write physical memory
    and return.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_WRITE_MEMORY64 a = &m->u.WriteMemory;
    STRING MessageHeader;
    ULONG MmFlags;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    // See ReadPhysical for an explanation of the ActualBytes usage.
    MmFlags = MMDBG_COPY_PHYSICAL | MMDBG_COPY_WRITE | MMDBG_COPY_UNSAFE;
    switch(a->ActualBytesWritten)
    {
    case DBGKD_CACHING_CACHED:
        MmFlags |= MMDBG_COPY_CACHED;
        break;
    case DBGKD_CACHING_UNCACHED:
        MmFlags |= MMDBG_COPY_UNCACHED;
        break;
    case DBGKD_CACHING_WRITE_COMBINED:
        MmFlags |= MMDBG_COPY_WRITE_COMBINED;
        break;
    }

    m->ReturnStatus =
        KdpCopyMemoryChunks(a->TargetBaseAddress,
                            AdditionalData->Buffer,
                            a->TransferCount,
                            0,
                            MmFlags,
                            &a->ActualBytesWritten);

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL,
        &KdpContext
        );
    UNREFERENCED_PARAMETER(Context);
}

VOID
KdpReadControlSpace(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a read control space state
    manipulation message.  Its function is to read implementation
    specific system data.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_READ_MEMORY64 a = &m->u.ReadMemory;
    STRING MessageHeader;
    ULONG Length;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    if (a->TransferCount > (PACKET_MAX_SIZE - sizeof(DBGKD_MANIPULATE_STATE64))) {
        Length = PACKET_MAX_SIZE - sizeof(DBGKD_MANIPULATE_STATE64);
    } else {
        Length = a->TransferCount;
    }

    m->ReturnStatus = KdpSysReadControlSpace(m->Processor,
                                             a->TargetBaseAddress,
                                             AdditionalData->Buffer,
                                             Length, &Length);

    AdditionalData->Length = (USHORT)Length;
    a->ActualBytesRead = Length;

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        AdditionalData,
        &KdpContext
        );
    UNREFERENCED_PARAMETER(Context);
}

VOID
KdpWriteControlSpace(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a write control space state
    manipulation message.  Its function is to write implementation
    specific system data.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_WRITE_MEMORY64 a = &m->u.WriteMemory;
    ULONG Length;
    STRING MessageHeader;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    m->ReturnStatus = KdpSysWriteControlSpace(m->Processor,
                                              a->TargetBaseAddress,
                                              AdditionalData->Buffer,
                                              AdditionalData->Length,
                                              &Length);

    a->ActualBytesWritten = Length;

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        AdditionalData,
        &KdpContext
        );
    UNREFERENCED_PARAMETER(Context);
}

VOID
KdpReadIoSpace(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a read io space state
    manipulation message.  Its function is to read system io
    locations.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_READ_WRITE_IO64 a = &m->u.ReadWriteIo;
    STRING MessageHeader;
    ULONG Length;

#if !DBG
    UNREFERENCED_PARAMETER(AdditionalData);
#endif

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    // Zero-fill the entire value so that shorter reads
    // do not leave unset bytes.
    a->DataValue = 0;

    m->ReturnStatus = KdpSysReadIoSpace(Isa, 0, 1, a->IoAddress,
                                        &a->DataValue, a->DataSize, &Length);

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL,
        &KdpContext
        );
    UNREFERENCED_PARAMETER(Context);
}

VOID
KdpWriteIoSpace(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a write io space state
    manipulation message.  Its function is to write to system io
    locations.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_READ_WRITE_IO64 a = &m->u.ReadWriteIo;
    STRING MessageHeader;
    ULONG Length;

#if !DBG
    UNREFERENCED_PARAMETER(AdditionalData);
#endif

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    m->ReturnStatus = KdpSysWriteIoSpace(Isa, 0, 1, a->IoAddress,
                                         &a->DataValue, a->DataSize, &Length);

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL,
        &KdpContext
        );
    UNREFERENCED_PARAMETER(Context);
}

VOID
KdpReadIoSpaceExtended(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a read io space extended state
    manipulation message.  Its function is to read system io
    locations.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_READ_WRITE_IO_EXTENDED64 a = &m->u.ReadWriteIoExtended;
    STRING MessageHeader;
    ULONG Length;

#if !DBG
    UNREFERENCED_PARAMETER(AdditionalData);
#endif

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    // Zero-fill the entire value so that shorter reads
    // do not leave unset bytes.
    a->DataValue = 0;

    m->ReturnStatus = KdpSysReadIoSpace(a->InterfaceType, a->BusNumber,
                                        a->AddressSpace, a->IoAddress,
                                        &a->DataValue, a->DataSize, &Length);

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL,
        &KdpContext
        );
    UNREFERENCED_PARAMETER(Context);
}

VOID
KdpWriteIoSpaceExtended(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a write io space extended state
    manipulation message.  Its function is to write to system io
    locations.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_READ_WRITE_IO_EXTENDED64 a = &m->u.ReadWriteIoExtended;
    STRING MessageHeader;
    ULONG Length;

#if !DBG
    UNREFERENCED_PARAMETER(AdditionalData);
#endif

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    m->ReturnStatus = KdpSysWriteIoSpace(a->InterfaceType, a->BusNumber,
                                         a->AddressSpace, a->IoAddress,
                                         &a->DataValue, a->DataSize, &Length);

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL,
        &KdpContext
        );
    UNREFERENCED_PARAMETER(Context);
}

VOID
KdpReadMachineSpecificRegister(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a read MSR
    manipulation message.  Its function is to read the MSR.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_READ_WRITE_MSR a = &m->u.ReadWriteMsr;
    STRING MessageHeader;
    ULARGE_INTEGER l;

    UNREFERENCED_PARAMETER(Context);
#if !DBG
    UNREFERENCED_PARAMETER(AdditionalData);
#endif

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    m->ReturnStatus = KdpSysReadMsr(a->Msr, &l.QuadPart);

    a->DataValueLow  = l.LowPart;
    a->DataValueHigh = l.HighPart;

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL,
        &KdpContext
        );
}

VOID
KdpWriteMachineSpecificRegister(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a write of a MSR
    manipulation message.  Its function is to write to the MSR

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_READ_WRITE_MSR a = &m->u.ReadWriteMsr;
    STRING MessageHeader;
    ULARGE_INTEGER l;

    UNREFERENCED_PARAMETER(Context);
#if !DBG
    UNREFERENCED_PARAMETER(AdditionalData);
#endif

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    l.HighPart = a->DataValueHigh;
    l.LowPart = a->DataValueLow;

    m->ReturnStatus = KdpSysWriteMsr(a->Msr, &l.QuadPart);

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL,
        &KdpContext
        );
}

VOID
KdpGetBusData (
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response to a get bus data state
    manipulation message.  Its function is to read I/O configuration
    space.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_GET_SET_BUS_DATA a = &m->u.GetSetBusData;
    ULONG Length;
    STRING MessageHeader;

    UNREFERENCED_PARAMETER (Context);

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    //
    // Trim length to fit in a single message
    //

    if (a->Length > (PACKET_MAX_SIZE - sizeof(DBGKD_MANIPULATE_STATE64))) {
        Length = PACKET_MAX_SIZE - sizeof(DBGKD_MANIPULATE_STATE64);
    } else {
        Length = a->Length;
    }

    m->ReturnStatus = KdpSysReadBusData(a->BusDataType, a->BusNumber,
                                        a->SlotNumber, a->Offset,
                                        AdditionalData->Buffer,
                                        Length, &Length);

    a->Length = Length;
    AdditionalData->Length = (USHORT)Length;

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        AdditionalData,
        &KdpContext
        );
}

VOID
KdpSetBusData (
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response to a set bus data state
    manipulation message.  Its function is to write I/O configuration
    space.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_GET_SET_BUS_DATA a = &m->u.GetSetBusData;
    ULONG Length;
    STRING MessageHeader;

    UNREFERENCED_PARAMETER (Context);

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    m->ReturnStatus = KdpSysWriteBusData(a->BusDataType, a->BusNumber,
                                         a->SlotNumber, a->Offset,
                                         AdditionalData->Buffer,
                                         a->Length, &Length);

    a->Length = Length;

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL,
        &KdpContext
        );
}



#if i386
VOID
KdpProcessInternalBreakpoint (
    ULONG BreakpointNumber
    )
{
    if ( !(KdpInternalBPs[BreakpointNumber].Flags &
           DBGKD_INTERNAL_BP_FLAG_COUNTONLY) ) {
        return;     // We only deal with COUNTONLY breakpoints
    }

    //
    // We've hit a real internal breakpoint; make sure the timeout is
    // kicked off.
    //

    if ( !BreakPointTimerStarted ) { // ok, maybe there's a prettier way to do this.
        KeInitializeDpc(
            &InternalBreakpointCheckDpc,
            &InternalBreakpointCheck,
            NULL
            );
        KeInitializeTimer( &InternalBreakpointTimer );
        // KeSetTimer can only be called at <= DISPATCH_LEVEL
        // so just queue the timer DPC routine directly for
        // the initial check.
        KeInsertQueueDpc(&InternalBreakpointCheckDpc, NULL, NULL);
        BreakPointTimerStarted = TRUE;
    }

    KdpInternalBPs[BreakpointNumber].Calls++;

} // KdpProcessInternalBreakpoint
#endif


VOID
KdpGetVersion(
    IN PDBGKD_MANIPULATE_STATE64 m
    )

/*++

Routine Description:

    This function returns to the caller a general information packet
    that contains useful information to a debugger.  This packet is also
    used for a debugger to determine if the writebreakpointex and
    readbreakpointex apis are available.

Arguments:

    m - Supplies the state manipulation message.

Return Value:

    None.

--*/

{
    STRING messageHeader;


    messageHeader.Length = sizeof(*m);
    messageHeader.Buffer = (PCHAR)m;

    KdpSysGetVersion(&m->u.GetVersion64);

    //
    // the usual stuff
    //
    m->ReturnStatus = STATUS_SUCCESS;
    m->ApiNumber = DbgKdGetVersionApi;

    KdSendPacket(PACKET_TYPE_KD_STATE_MANIPULATE,
                 &messageHeader,
                 NULL,
                 &KdpContext
                 );

    return;
} // KdGetVersion


NTSTATUS
KdpNotSupported(
    IN PDBGKD_MANIPULATE_STATE64 m
    )

/*++

Routine Description:

    This routine returns STATUS_UNSUCCESSFUL to the debugger

Arguments:

    m - Supplies a DBGKD_MANIPULATE_STATE64 struct to answer with

Return Value:

    0, to indicate that the system should not continue

--*/

{
    STRING          MessageHeader;

    //
    // setup packet
    //
    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;
    m->ReturnStatus = STATUS_UNSUCCESSFUL;

    //
    // send back our response
    //
    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL,
        &KdpContext
        );

    //
    // return the caller's continue status value.  if this is a non-zero
    // value the system is continued using this value as the continuestatus.
    //
    return 0;
} // KdpNotSupported


VOID
KdpCauseBugCheck(
    IN PDBGKD_MANIPULATE_STATE64 m
    )

/*++

Routine Description:

    This routine causes a bugcheck.  It is used for testing the debugger.

Arguments:

    m - Supplies the state manipulation message.

Return Value:

    None.

--*/

{
    UNREFERENCED_PARAMETER (m);

    KeBugCheckEx( MANUALLY_INITIATED_CRASH, 0, 0, 0, 0 );

} // KdCauseBugCheck


NTSTATUS
KdpWriteBreakPointEx(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a write breakpoint state 'ex'
    manipulation message.  Its function is to clear breakpoints, write
    new breakpoints, and continue the target system.  The clearing of
    breakpoints is conditional based on the presence of breakpoint handles.
    The setting of breakpoints is conditional based on the presence of
    valid, non-zero, addresses.  The continueing of the target system
    is conditional based on a non-zero continuestatus.

    This api allows a debugger to clear breakpoints, add new breakpoint,
    and continue the target system all in one api packet.  This reduces the
    amount of traffic across the wire and greatly improves source stepping.


Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_BREAKPOINTEX       a = &m->u.BreakPointEx;
    PDBGKD_WRITE_BREAKPOINT64 b;
    STRING                    MessageHeader;
    ULONG                     i;
    ULONG                     Size;
    DBGKD_WRITE_BREAKPOINT64  BpBuf[BREAKPOINT_TABLE_SIZE];

    UNREFERENCED_PARAMETER (Context);

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    //
    // verify that the packet size is correct
    //
    if (AdditionalData->Length !=
        a->BreakPointCount * sizeof(DBGKD_WRITE_BREAKPOINT64))
    {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
        KdSendPacket(
            PACKET_TYPE_KD_STATE_MANIPULATE,
            &MessageHeader,
            AdditionalData,
            &KdpContext
            );
        return m->ReturnStatus;
    }

    KdpCopyFromPtr(BpBuf,
                   AdditionalData->Buffer,
                   a->BreakPointCount * sizeof(DBGKD_WRITE_BREAKPOINT64),
                   &Size);

    if (Size == a->BreakPointCount * sizeof(DBGKD_WRITE_BREAKPOINT64))
    {
        m->ReturnStatus = STATUS_SUCCESS;
    }
    else
    {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
        KdSendPacket(
            PACKET_TYPE_KD_STATE_MANIPULATE,
            &MessageHeader,
            AdditionalData,
            &KdpContext
            );
        return m->ReturnStatus;
    }

    //
    // loop thru the breakpoint handles passed in from the debugger and
    // clear any breakpoint that has a non-zero handle
    //
    b = BpBuf;
    for (i=0; i<a->BreakPointCount; i++,b++) {
        if (b->BreakPointHandle) {
            if (!KdpDeleteBreakpoint(b->BreakPointHandle)) {
                m->ReturnStatus = STATUS_UNSUCCESSFUL;
            }
            b->BreakPointHandle = 0;
        }
    }

    //
    // loop thru the breakpoint addesses passed in from the debugger and
    // add any new breakpoints that have a non-zero address
    //
    b = BpBuf;
    for (i=0; i<a->BreakPointCount; i++,b++) {
        if (b->BreakPointAddress) {
            b->BreakPointHandle = KdpAddBreakpoint( (PVOID)(ULONG_PTR)b->BreakPointAddress );
            if (!b->BreakPointHandle) {
                m->ReturnStatus = STATUS_UNSUCCESSFUL;
            }
        }
    }

    //
    // send back our response
    //

    KdpCopyToPtr(AdditionalData->Buffer,
                 BpBuf,
                 a->BreakPointCount * sizeof(DBGKD_WRITE_BREAKPOINT64),
                 &Size);

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        AdditionalData,
        &KdpContext
        );

    //
    // return the caller's continue status value.  if this is a non-zero
    // value the system is continued using this value as the continuestatus.
    //
    return a->ContinueStatus;
}


VOID
KdpRestoreBreakPointEx(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a restore breakpoint state 'ex'
    manipulation message.  Its function is to clear a list of breakpoints.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_BREAKPOINTEX         a = &m->u.BreakPointEx;
    PDBGKD_RESTORE_BREAKPOINT   b;
    STRING                      MessageHeader;
    ULONG                       i;
    ULONG                       Size;
    DBGKD_RESTORE_BREAKPOINT    BpBuf[BREAKPOINT_TABLE_SIZE];

    UNREFERENCED_PARAMETER (Context);

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    //
    // verify that the packet size is correct
    //
    if (AdditionalData->Length !=
                       a->BreakPointCount*sizeof(DBGKD_RESTORE_BREAKPOINT))
    {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    }
    else
    {
        KdpCopyFromPtr(BpBuf,
                       AdditionalData->Buffer,
                       a->BreakPointCount * sizeof(DBGKD_RESTORE_BREAKPOINT),
                       &Size);

        if (Size == a->BreakPointCount*sizeof(DBGKD_RESTORE_BREAKPOINT))
        {
            m->ReturnStatus = STATUS_SUCCESS;

            //
            // loop thru the breakpoint handles passed in from the debugger and
            // clear any breakpoint that has a non-zero handle
            //
            b = BpBuf;
            for (i=0; i<a->BreakPointCount; i++,b++) {
                if (!KdpDeleteBreakpoint(b->BreakPointHandle)) {
                    m->ReturnStatus = STATUS_UNSUCCESSFUL;
                }
            }
        }
        else
        {
            m->ReturnStatus = STATUS_UNSUCCESSFUL;
        }
    }

    //
    // send back our response
    //
    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        AdditionalData,
        &KdpContext
        );
}

NTSTATUS
KdDisableDebugger(
    VOID
    )
/*++

Routine Description:

    This function is called to disable the debugger.

Arguments:

    None.

Return Value:

    NTSTATUS.

--*/

{
    KIRQL oldIrql;
    NTSTATUS Status;

    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    KdpPortLock();

    if (!KdDisableCount) {

        KdPreviouslyEnabled = KdDebuggerEnabled && (!KdPitchDebugger);

        if (KdPreviouslyEnabled &&
            !NT_SUCCESS(Status = KdpAllowDisable())) {
            KdpPortUnlock();
            KeLowerIrql(oldIrql);
            return Status;
        }
        
        if (KdDebuggerEnabled) {

            KdpSuspendAllBreakpoints();
            KiDebugRoutine = KdpStub;
            KdDebuggerEnabled = FALSE ;
            SharedUserData->KdDebuggerEnabled = 0;
        }
    }
    
    KdDisableCount++;
    
    KdpPortUnlock();
    KeLowerIrql(oldIrql);
    return STATUS_SUCCESS;
}

NTSTATUS
KdEnableDebugger(
   VOID
   )
/*++

Routine Description:

    This function is called to reenable the debugger after a call to
    KdDisableDebugger.

Arguments:

    None.

Return Value:

    NTSTATUS.

--*/
{
    KIRQL oldIrql ;

    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql) ;
    KdpPortLock();

    if (KdDisableCount == 0) {
        KdpPortUnlock();
        KeLowerIrql(oldIrql);
        return STATUS_INVALID_PARAMETER;
    }
        
    KdDisableCount-- ;

    if (!KdDisableCount) {
        if (KdPreviouslyEnabled) {

            //
            // Ugly HACKHACK - Make sure the timers aren't reset.
            //
            PoHiberInProgress = TRUE ;
            KdInitSystem(0, NULL);
            KdpRestoreAllBreakpoints();
            PoHiberInProgress = FALSE ;
        }
    }
    
    KdpPortUnlock();
    KeLowerIrql(oldIrql);
    return STATUS_SUCCESS;
}


VOID
KdpSearchMemory(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function implements a memory pattern searcher.  This will
    find an instance of a pattern that begins in the range
    SearchAddress..SearchAddress+SearchLength.  The pattern may
    end outside of the range.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies the pattern to search for

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PUCHAR Pattern = (PUCHAR) AdditionalData->Buffer;
    ULONG_PTR StartAddress = (ULONG_PTR)m->u.SearchMemory.SearchAddress;
    ULONG_PTR EndAddress = (ULONG_PTR)(StartAddress + m->u.SearchMemory.SearchLength);
    ULONG PatternLength = m->u.SearchMemory.PatternLength;

    STRING MessageHeader;
    ULONG MaskIndex;
    PUCHAR PatternTail;
    ULONG_PTR DataTail;
    ULONG TailLength;
    ULONG Data;
    ULONG FirstWordPattern[4];
    ULONG FirstWordMask[4];
    UCHAR DataTailVal;

    UNREFERENCED_PARAMETER (Context);

    //
    // On failure, return STATUS_NO_MORE_ENTRIES.  DON'T RETURN
    // STATUS_UNSUCCESSFUL!  That return status indicates that the
    // operation is not supported, and the debugger will fall back
    // to a debugger-side search.
    //

    m->ReturnStatus = STATUS_NO_MORE_ENTRIES;

    //
    // Do a fast search for the beginning of the pattern
    //

    if (PatternLength > 3) {
        FirstWordMask[0] = 0xffffffff;
    } else {
        FirstWordMask[0] = 0xffffffff >> (8*(4-PatternLength));
    }

    FirstWordMask[1] = FirstWordMask[0] << 8;
    FirstWordMask[2] = FirstWordMask[1] << 8;
    FirstWordMask[3] = FirstWordMask[2] << 8;

    FirstWordPattern[0] = 0;
    KdpQuickMoveMemory((PCHAR)FirstWordPattern,
                       (PCHAR)Pattern,
                       PatternLength < 5 ? PatternLength : 4);

    FirstWordPattern[1] = FirstWordPattern[0] << 8;
    FirstWordPattern[2] = FirstWordPattern[1] << 8;
    FirstWordPattern[3] = FirstWordPattern[2] << 8;


/*
{
    int i;
    for (i = 0; i < (int)PatternLength; i++) {
        KdpDprintf("%08x: %02x\n", &Pattern[i], Pattern[i]);
    }
    for (i = 0; i < 4; i++) {
        KdpDprintf("%d: %08x %08x\n", i, FirstWordPattern[i], FirstWordMask[i]);
    }
}
*/



    //
    // Get starting mask
    //

    MaskIndex = (ULONG) (StartAddress & 3);
    StartAddress = StartAddress & ~3;

    while (StartAddress < EndAddress) {

        // Get the current data DWORD.  StartAddress is
        // properly aligned and we only need the one DWORD
        // so we can directly call MmDbgCopyMemory.
        if (!NT_SUCCESS(MmDbgCopyMemory(StartAddress, &Data, 4,
                                        MMDBG_COPY_UNSAFE))) {
//KdpDprintf("\n%08x: Inaccessible\n", StartAddress);
            StartAddress += 4;
            MaskIndex = 0;
            continue;
        }
        
        //
        // search for a match in each of the 4 starting positions
        //

//KdpDprintf("\n%08x: %08x ", StartAddress, Data);

        for ( ; MaskIndex < 4; MaskIndex++) {
//KdpDprintf(" %d", MaskIndex);

            if ( (Data & FirstWordMask[MaskIndex]) == FirstWordPattern[MaskIndex]) {

                //
                // first word matched
                //

                if ( (4-MaskIndex) >= PatternLength ) {

                    //
                    // string is all in this word; good match
                    //
//KdpDprintf(" %d hit, complete\n", MaskIndex);

                    m->u.SearchMemory.FoundAddress = StartAddress + MaskIndex;
                    m->ReturnStatus = STATUS_SUCCESS;
                    goto done;

                } else {

                    //
                    // string is longer; see if tail matches
                    //
//KdpDprintf(" %d hit, check tail\n", MaskIndex);

                    PatternTail = Pattern + 4 - MaskIndex;
                    DataTail = StartAddress + 4;
                    TailLength = PatternLength - 4 + MaskIndex;

//KdpDprintf("Pattern == %08x\n", Pattern);
//KdpDprintf("PatternTail == %08x\n", PatternTail);
//KdpDprintf("DataTail == %08x\n", DataTail);

                    while (TailLength) {
                        if (!NT_SUCCESS(MmDbgCopyMemory(DataTail,
                                                        &DataTailVal,
                                                        1,
                                                        MMDBG_COPY_UNSAFE))) {
//KdpDprintf("Tail %08x: Inaccessible\n", DataTail);
                            break;
                        }

//KdpDprintf("D: %02x  P: %02x\n", DataTailVal, *PatternTail);

                        if (DataTailVal != *PatternTail) {
//KdpDprintf("Tail failed at %08x\n", DataTail);
                            break;
                        } else {
                            DataTail++;
                            PatternTail++;
                            TailLength--;
                        }
                    }

                    if (TailLength == 0) {

                        //
                        // A winner
                        //

                        m->u.SearchMemory.FoundAddress = StartAddress + MaskIndex;
                        m->ReturnStatus = STATUS_SUCCESS;
                        goto done;

                    }
                }
            }
        }

        StartAddress += 4;
        MaskIndex = 0;
    }

done:
//KdpDprintf("\n");
    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL,
        &KdpContext
        );

}


VOID
KdpCheckLowMemory(
    IN PDBGKD_MANIPULATE_STATE64 Message
    )

/*++

Routine Description:


Arguments:

    Message - Supplies the state manipulation message.

Return Value:

    None.

Description:

    This function gets called when the !chklowmem
    debugger extension is used.

--*/

{
    STRING MessageHeader;

    MessageHeader.Length = sizeof(*Message);
    MessageHeader.Buffer = (PCHAR)Message;

    Message->ReturnStatus = KdpSysCheckLowMemory(MMDBG_COPY_UNSAFE);

    //
    // Acknowledge the packet received.
    //

    KdSendPacket (
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL,
        &KdpContext
        );
}



//
// !search support routines
//



ULONG
KdpSearchHammingDistance (
    ULONG_PTR Left,
    ULONG_PTR Right
    )
/*++

Routine Description:

    This routine computes the Hamming distance (# of positions where the
    values are different).

    If this function becomes a bottleneck we should switch to a function
    table version.

Arguments:

    Left, Right operand.

Return Value:

    Hamming distance.

Environment:

    Any.

--*/

{
    ULONG_PTR Value;
    ULONG Index;
    ULONG Distance;

    Value = Left ^ Right;
    Distance = 0;

    for (Index = 0; Index < 8 * sizeof(ULONG_PTR); Index++) {

        if ((Value & (ULONG_PTR)0x01)) {

            Distance += 1;
        }

        Value >>= 1;
    }

    return Distance;
}



LOGICAL
KdpSearchPhysicalPage (
    IN PFN_NUMBER PageFrameIndex,
    ULONG_PTR RangeStart,
    ULONG_PTR RangeEnd,
    ULONG Flags,
    ULONG MmFlags
    )
/*++

Routine Description:

    This routine searches the physical page corresponding to a
    certain PFN index for any ULONG_PTR values in range [Start..End].

Arguments:

    PageFrameIndex - PFN index

    RangeStart - lowest possible value searched for

    RangeEnd - highest possible value searched for

    Flags - flags to control the search

    MmFlags - flags to control the MmDbg routines for memory access

Return Value:

    TRUE if a hit has been found, FALSE otherwise.
    The function stops after the first hit in the page is
    encountered and the information related to the hit (PFN index,
    offset, corresponding VA) is registered in the hit database.

Environment:

    Call triggered only from Kd extension.

--*/

{
    LOGICAL Status;
    NTSTATUS CopyStatus;
    ULONG Index;
    PHYSICAL_ADDRESS Pa;

    Pa.QuadPart = ((ULONGLONG)PageFrameIndex) << PAGE_SHIFT;

    Status = FALSE;

    if (KdpSearchPfnValue) {

        HARDWARE_PTE PteValue;

        //
        // We need to search for a PFN
        //

        for (Index = 0; Index < PAGE_SIZE; Index += sizeof(HARDWARE_PTE)) {

            CopyStatus = MmDbgCopyMemory ((ULONG64)(Pa.QuadPart) + Index,
                                          &PteValue,
                                          sizeof PteValue,
                                          MMDBG_COPY_PHYSICAL | MmFlags);

            if (NT_SUCCESS(CopyStatus)) {

                if (PteValue.PageFrameNumber == RangeStart) {

                    if (KdpSearchPageHitIndex < SEARCH_PAGE_HIT_DATABASE_SIZE) {

                        KdpSearchPageHits[KdpSearchPageHitIndex] = PageFrameIndex;
                        KdpSearchPageHitOffsets[KdpSearchPageHitIndex] = Index;
                        KdpSearchPageHitIndex += 1;
                    }

                    if ((Flags & KDP_SEARCH_ALL_OFFSETS_IN_PAGE) == 0) {
                        Status = TRUE;
                        break;
                    }
                }
            }
        }
    }
    else {

        ULONG_PTR Value;

        //
        // We need to search for an address pattern
        //
        
        for (Index = 0; Index < PAGE_SIZE; Index += sizeof(ULONG_PTR)) {

            CopyStatus = MmDbgCopyMemory ((ULONG64)(Pa.QuadPart) + Index,
                                          &Value,
                                          sizeof Value,
                                          MMDBG_COPY_PHYSICAL | MmFlags);

            if (NT_SUCCESS(CopyStatus)) {

                if ((Value >= RangeStart && Value <= RangeEnd) ||
                    (KdpSearchHammingDistance(Value, RangeStart) == 1)) {

                    if (KdpSearchPageHitIndex < SEARCH_PAGE_HIT_DATABASE_SIZE) {
                        KdpSearchPageHits[KdpSearchPageHitIndex] = PageFrameIndex;
                        KdpSearchPageHitOffsets[KdpSearchPageHitIndex] = Index;

                        KdpSearchPageHitIndex += 1;
                    }

                    if ((Flags & KDP_SEARCH_ALL_OFFSETS_IN_PAGE) == 0) {
                        Status = TRUE;
                        break;
                    }
                }
            }
        }
    }

    return Status;
}



LOGICAL
KdpSearchPhysicalMemoryRequested (
    VOID
    )
/*++

Routine Description:

    This routine determines if a physical range search has been
    requested. This is controlled by a global variable set in
    the `!search' debug extension.

Arguments:

    None

Return Value:

    TRUE if physical range search was requested.


Environment:

    Call triggered only from Kd extension.

--*/
{
    if (KdpSearchInProgress) {

        return TRUE;
    }
    else {

        return FALSE;
    }

}



LOGICAL
KdpSearchPhysicalPageRange (
    ULONG MmFlags
    )
/*++

Routine Description:

    This routine will start a search in a range of physical pages in case
    `KdpSearchInProgress' is true. the parameters for the search are picked up
    from global vairiables that are set inside a kernel debugger extension.

Arguments:

    MmFlags - flags to control the MmDbg routines for memory access

Return Value:

    TRUE if the function executed a search and FALSE otherwise.
    The results of the search are specified in the KdpSearchPageHits
    and related variables. this global variables offers the mechanism
    for the debugger extension to pickup the results of the search.


Environment:

    Call triggered only from Kd extension.

    Note. The !search extension make sure that the range requested
    is part of the system memory therefore we do not have to
    worry about sparse PFN databases here.

--*/

{
    PFN_NUMBER CurrentFrame;
    ULONG Flags;

    //
    // The debugger extension is supposed to set KdpSearchInProgress
    // to TRUE if a search is requested.
    //

    if (!KdpSearchInProgress) {

        return FALSE;
    }


    Flags = 0;

    //
    // If the search range is only one page we will give all
    // hits inside a page. By default we get only the first hit inside
    // a page.
    //

    if (KdpSearchEndPageFrame == KdpSearchStartPageFrame) {

        KdpSearchEndPageFrame += 1;

        Flags |= KDP_SEARCH_ALL_OFFSETS_IN_PAGE;
    }

    for (CurrentFrame = KdpSearchStartPageFrame;
         CurrentFrame < KdpSearchEndPageFrame;
         CurrentFrame += 1) {

        KdpSearchPhysicalPage (CurrentFrame,
                               KdpSearchAddressRangeStart,
                               KdpSearchAddressRangeEnd,
                               Flags,
                               MmFlags);

    }

    return TRUE;
}

VOID
KdpFillMemory(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    Fill a section of memory with a given pattern.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies the pattern to search for.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    STRING MessageHeader;
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG Length = m->u.FillMemory.Length;
    PUCHAR Pattern = (PUCHAR) AdditionalData->Buffer;
    PUCHAR Pat = Pattern;
    PUCHAR PatEnd = Pat + m->u.FillMemory.PatternLength;
    ULONG Filled = 0;
    ULONG ChunkFlags = MMDBG_COPY_WRITE | MMDBG_COPY_UNSAFE;

    UNREFERENCED_PARAMETER (Context);

    if (m->u.FillMemory.Flags & DBGKD_FILL_MEMORY_PHYSICAL) {
        ChunkFlags |= MMDBG_COPY_PHYSICAL;
    } else if (!(m->u.FillMemory.Flags & DBGKD_FILL_MEMORY_VIRTUAL)) {
        Status = STATUS_INVALID_PARAMETER;
    }

    if (NT_SUCCESS(Status)) {
        
        ULONG64 Address = m->u.FillMemory.Address;
        
        while (Length-- > 0) {
            ULONG Done;

            if (!NT_SUCCESS(Status =
                            KdpCopyMemoryChunks(Address, Pat, 1, 0,
                                                ChunkFlags, &Done))) {
                break;
            }

            Address++;
            if (++Pat == PatEnd) {
                Pat = Pattern;
            }
            Filled++;
        }

        // If nothing was filled return an error, otherwise
        // consider it a success.
        Status = Filled > 0 ? STATUS_SUCCESS : Status;
        
    }
        
    m->ReturnStatus = Status;
    m->u.FillMemory.Length = Filled;
    
    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL,
        &KdpContext
        );
}

VOID
KdpQueryMemory(
    IN PDBGKD_MANIPULATE_STATE64 m,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    Query what kind of memory a particular address refers to.

Arguments:

    m - Supplies the state manipulation message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    STRING MessageHeader;
    NTSTATUS Status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER (Context);

    if (m->u.QueryMemory.AddressSpace == DBGKD_QUERY_MEMORY_VIRTUAL) {

        PVOID Addr = (PVOID)(ULONG_PTR)m->u.QueryMemory.Address;

        //
        // Right now all we check for is user/session/kernel.
        //
        
        if (Addr < MM_HIGHEST_USER_ADDRESS) {
            m->u.QueryMemory.AddressSpace = DBGKD_QUERY_MEMORY_PROCESS;
        } else if (MmIsSessionAddress(Addr)) {
            m->u.QueryMemory.AddressSpace = DBGKD_QUERY_MEMORY_SESSION;
        } else {
            m->u.QueryMemory.AddressSpace = DBGKD_QUERY_MEMORY_KERNEL;
        }

        // Always return the most permissive flags.
        m->u.QueryMemory.Flags =
            DBGKD_QUERY_MEMORY_READ |
            DBGKD_QUERY_MEMORY_WRITE |
            DBGKD_QUERY_MEMORY_EXECUTE;
    
    } else {
        Status = STATUS_INVALID_PARAMETER;
    }
    
    m->ReturnStatus = Status;
    m->u.QueryMemory.Reserved = 0;
    
    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    KdSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL,
        &KdpContext
        );
}

VOID
KdpSysGetVersion(
    PDBGKD_GET_VERSION64 Version
    )

/*++

Routine Description:

    This function returns to the caller a general information packet
    that contains useful information to a debugger.  This packet is also
    used for a debugger to determine if the writebreakpointex and
    readbreakpointex apis are available.

Arguments:

    Version - Supplies the structure to fill in

Return Value:

    None.

--*/

{
    *Version = KdVersionBlock;
}

NTSTATUS
KdpSysReadBusData(
    BUS_DATA_TYPE BusDataType,
    ULONG BusNumber,
    ULONG SlotNumber,
    ULONG Address,
    PVOID Buffer,
    ULONG Request,
    PULONG Actual
    )

/*++

Routine Description:

    Reads I/O configuration space.

Arguments:

    BusDataType - Bus data type.

    BusNumber - Bus number.

    SlotNumber - Slot number.

    Address - Configuration space address.

    Buffer - Data buffer.

    Request - Amount of data to move.

    Actual - Amount of data actually moved.

Return Value:

    NTSTATUS.

--*/

{
    *Actual = HalGetBusDataByOffset(BusDataType, BusNumber, SlotNumber,
                                    Buffer, Address, Request);
    return *Actual == Request ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

NTSTATUS
KdpSysWriteBusData(
    BUS_DATA_TYPE BusDataType,
    ULONG BusNumber,
    ULONG SlotNumber,
    ULONG Address,
    PVOID Buffer,
    ULONG Request,
    PULONG Actual
    )

/*++

Routine Description:

    Writes I/O configuration space.

Arguments:

    BusDataType - Bus data type.

    BusNumber - Bus number.

    SlotNumber - Slot number.

    Address - Configuration space address.

    Buffer - Data buffer.

    Request - Amount of data to move.

    Actual - Amount of data actually moved.

Return Value:

    NTSTATUS.

--*/

{
    *Actual = HalSetBusDataByOffset(BusDataType, BusNumber, SlotNumber,
                                    Buffer, Address, Request);
    return *Actual == Request ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}









NTSTATUS
KdpSysCheckLowMemory(
    ULONG MmFlags
    )

/*++

Routine Description:


Arguments:

    MmFlags - 0 or MMDBG_COPY_UNSAFE to indicate whether the routine
              is being used from local kd or from remote kd.

Return Value:

    NTSTATUS.

Description:

    This function gets called when the !chklowmem
    debugger extension is used.

--*/

{
    PFN_NUMBER Page;
    PFN_NUMBER NextPage;
    ULONG CorruptionOffset;
    NTSTATUS Status;

    Status = STATUS_SUCCESS;

    if (KdpSearchPhysicalMemoryRequested()) {

        //
        // This is a !search kd extension call.
        //

        KdpSearchPhysicalPageRange(MmFlags);
    }
    else {

        // MmDbgIsLowMemOk is only usable from real kd, not
        // local kd, so don't allow local kd access.
        if ((MmFlags & MMDBG_COPY_UNSAFE) == 0) {
            return STATUS_NOT_IMPLEMENTED;
        }
        
        //
        // Check low physical memory on machines with more than 4GB.
        //

        Page = 0;

        do {

            if (! MmDbgIsLowMemOk (Page, &NextPage, &CorruptionOffset)) {
                Status = (NTSTATUS) Page;
                break;
            }

            Page = NextPage;

        } while (Page != 0);
    }

    return Status;
}

BOOLEAN
KdRefreshDebuggerNotPresent(
    VOID
    )

/*++

Routine Description:


Arguments:

    None.

Return Value:

    BOOLEAN.

Description:

    KdRefreshDebuggerPresent attempts to communicate with
    the debugger host machine to refresh the state of
    KdDebuggerNotPresent.  It returns the state of
    KdDebuggerNotPresent while the kd locks are held.
    KdDebuggerNotPresent may immediately change state
    after the kd locks are released so it may not
    match the return value.

--*/

{
    STRING Output;
    BOOLEAN Enable;
    BOOLEAN NotPresent;

    if (KdPitchDebugger) {
        // Machine was booted non-debug, so the debugger
        // can't be active.
        return TRUE;
    }
        
    //
    // In order to be compatible with all debuggers this
    // routine doesn't use a new KD API.  Instead it
    // just sends an output string without checking
    // for the current state of KdDebuggerNotPresent.
    // The transport code will automatically update
    // KdDebuggerNotPresent during communication.
    //
    
    Output.Buffer = "KDTARGET: Refreshing KD connection\n";
    Output.Length = (USHORT)strlen(Output.Buffer);

    Enable = KdEnterDebugger(NULL, NULL);
    
    KdpPrintString(&Output);
    NotPresent = KdDebuggerNotPresent;

    KdExitDebugger(Enable);

    return NotPresent;
}

//----------------------------------------------------------------------------
//
// Tracing data support.
//
//----------------------------------------------------------------------------

VOID
KdpSendTraceData(
    PSTRING Data
    )
{
    ULONG Length;
    STRING MessageData;
    STRING MessageHeader;
    DBGKD_TRACE_IO TraceIo;
    
    //
    // Move the output string to the message buffer.
    //

    KdpCopyFromPtr(KdpMessageBuffer,
                   Data->Buffer,
                   Data->Length,
                   &Length);

    //
    // If the total message length is greater than the maximum packet size,
    // then truncate the output string.
    //

    if ((sizeof(TraceIo) + Length) > PACKET_MAX_SIZE) {
        Length = PACKET_MAX_SIZE - sizeof(TraceIo);
    }

    //
    // Construct the print string message and message descriptor.
    //

    TraceIo.ApiNumber = DbgKdPrintTraceApi;
    TraceIo.ProcessorLevel = KeProcessorLevel;
    TraceIo.Processor = (USHORT)KeGetCurrentProcessorNumber();
    TraceIo.u.PrintTrace.LengthOfData = Length;
    MessageHeader.Length = sizeof(TraceIo);
    MessageHeader.Buffer = (PCHAR)&TraceIo;

    //
    // Construct the print string data and data descriptor.
    //

    MessageData.Length = (USHORT)Length;
    MessageData.Buffer = (PCHAR) KdpMessageBuffer;

    //
    // Send packet to the kernel debugger on the host machine.
    //

    KdSendPacket(
        PACKET_TYPE_KD_TRACE_IO,
        &MessageHeader,
        &MessageData,
        &KdpContext
        );
}

VOID
KdReportTraceData(
    IN PVOID VoidBuffer,
    IN PVOID Context
    )
{
    PWMI_BUFFER_HEADER Buffer;
    BOOLEAN Enable;
    STRING Data;

    UNREFERENCED_PARAMETER (Context);

    Buffer = VoidBuffer;
    Data.Buffer = (PCHAR)Buffer;
    if (Buffer->Wnode.BufferSize > 0xffff) {
        Data.Length = 0xffff;
    } else {
        Data.Length = (USHORT)Buffer->Wnode.BufferSize;
    }
    
    if (KdDebuggerNotPresent == FALSE) {
        Enable = KdEnterDebugger(NULL, NULL);

        KdpSendTraceData(&Data);

        KdExitDebugger(Enable);
    }
}

//----------------------------------------------------------------------------
//
// Debugger hibernate/suspend support.
//
//----------------------------------------------------------------------------

NTSTATUS
KdPowerTransition(
    DEVICE_POWER_STATE newDeviceState
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    switch(newDeviceState) {
    
    case PowerDeviceD0:
        KdD0Transition();
        break;
        
    case PowerDeviceD3:
        KdD3Transition();
        break;

    default:
        status = STATUS_INVALID_PARAMETER_1;
    }

    return status;        
}    
