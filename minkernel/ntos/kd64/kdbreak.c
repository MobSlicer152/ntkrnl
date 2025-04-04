/*++

Copyright (c) 1990-2001  Microsoft Corporation

Module Name:

    kdbreak.c

Abstract:

    This module implements machine dependent functions to add and delete
    breakpoints from the kernel debugger breakpoint table.

Author:

    David N. Cutler 2-Aug-1990

Revision History:

--*/

#include "kdp.h"

//
// Define external references.
//

VOID
KdSetOwedBreakpoints(
    VOID
    );

BOOLEAN
KdpLowWriteContent(
    ULONG Index
    );

BOOLEAN
KdpLowRestoreBreakpoint(
    ULONG Index
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGEKD, KdpAddBreakpoint)
#pragma alloc_text(PAGEKD, KdpDeleteBreakpoint)
#pragma alloc_text(PAGEKD, KdpDeleteBreakpointRange)
#pragma alloc_text(PAGEKD, KdpSuspendBreakpoint)
#pragma alloc_text(PAGEKD, KdpSuspendAllBreakpoints)
#pragma alloc_text(PAGEKD, KdpRestoreAllBreakpoints)
#pragma alloc_text(PAGEKD, KdpLowWriteContent)
#pragma alloc_text(PAGEKD, KdpLowRestoreBreakpoint)
#if defined(_IA64_)
#pragma alloc_text(PAGEKD, KdpSuspendBreakpointRange)
#pragma alloc_text(PAGEKD, KdpRestoreBreakpointRange)
#endif
#endif

#if 0
#define BPVPRINT(Args) DPRINT(Args)
#else
#define BPVPRINT(Args)
#endif


ULONG
KdpAddBreakpoint (
    IN PVOID Address
    )

/*++

Routine Description:

    This routine adds an entry to the breakpoint table and returns a handle
    to the breakpoint table entry.

Arguments:

    Address - Supplies the address where to set the breakpoint.

Return Value:

    A value of zero is returned if the specified address is already in the
    breakpoint table, there are no free entries in the breakpoint table, the
    specified address is not correctly aligned, or the specified address is
    not valid. Otherwise, the index of the assigned breakpoint table entry
    plus one is returned as the function value.

--*/

{

    KDP_BREAKPOINT_TYPE Content;
    ULONG Index;
    LOGICAL Accessible;

    BPVPRINT(("KD: Setting breakpoint at 0x%p\n", Address));

    //
    // If the specified address is not properly aligned, then return zero.
    //

    if (((ULONG_PTR)Address & KDP_BREAKPOINT_ALIGN) != 0) {
        return 0;
    }


    //
    // Don't allow setting the same breakpoint twice.
    //

    for (Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index += 1) {
        if ((KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_IN_USE) != 0 &&
            KdpBreakpointTable[Index].Address == Address) {

            if ((KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_NEEDS_REPLACE) != 0) {

                //
                // Breakpoint was set, the page was written out and was not
                // accessible when the breakpoint was cleared.  Now the breakpoint
                // is being set again.  Just clear the defer flag:
                //
                KdpBreakpointTable[Index].Flags &= ~KD_BREAKPOINT_NEEDS_REPLACE;
                return Index + 1;

            } else {

                DPRINT(("KD: Attempt to set breakpoint %08x twice!\n", Address));
                return 0;

            }
        }
    }

    //
    // Search the breakpoint table for a free entry.
    //

    for (Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index += 1) {
        if (KdpBreakpointTable[Index].Flags == 0) {
            break;
        }
    }

    //
    // If a free entry was found, then write breakpoint and return the handle
    // value plus one. Otherwise, return zero.
    //

    if (Index == BREAKPOINT_TABLE_SIZE) {
        DPRINT(("KD: ran out of breakpoints!\n"));
        return 0;
    }


    BPVPRINT(("KD: using Index %d\n", Index));

    //
    // Get the instruction to be replaced. If the instruction cannot be read,
    // then mark breakpoint as not accessible.
    //

    Accessible = NT_SUCCESS(KdpCopyFromPtr(&Content,
                                           Address,
                                           sizeof(KDP_BREAKPOINT_TYPE),
                                           NULL));
    BPVPRINT(("KD: memory %saccessible\n", Accessible ? "" : "in"));

#if defined(_IA64_)
    if ( Accessible ) {
        KDP_BREAKPOINT_TYPE mBuf;
        PVOID BundleAddress;

        // change template to type 0 if current instruction is MLI

        // read in intruction template if current instruction is NOT slot 0.
        // check for two-slot MOVL instruction. Reject request if attempt to
        // set break in slot 2 of MLI template.

        if (((ULONG_PTR)Address & 0xf) != 0) {
            BundleAddress = (PVOID)((ULONG_PTR)Address & ~(0xf));
            if (!NT_SUCCESS(KdpCopyFromPtr(&mBuf,
                                           BundleAddress,
                                           sizeof(KDP_BREAKPOINT_TYPE),
                                           NULL))) {
                BPVPRINT(("KD: read 0x%p template failed\n", BundleAddress));
                return 0;
            } else {
                if (((mBuf & INST_TEMPL_MASK) >> 1) == 0x2) {
                    if (((ULONG_PTR)Address & 0xf) == 4) {
                        // if template= type 2 MLI, change to type 0
                        mBuf &= ~((INST_TEMPL_MASK >> 1) << 1);
                        KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_IA64_MOVL;
                        if (!NT_SUCCESS(KdpCopyToPtr(BundleAddress,
                                                     &mBuf,
                                                     sizeof(KDP_BREAKPOINT_TYPE),
                                                     NULL))) {
                            BPVPRINT(("KD: write to 0x%p template failed\n", BundleAddress));
                            return 0;
                         }
                         else {
                             BPVPRINT(("KD: change MLI template to type 0 at 0x%p set\n", Address));
                         }
                    } else {
                         // set breakpoint at slot 2 of MOVL is illegal
                         BPVPRINT(("KD: illegal to set BP at slot 2 of MOVL at 0x%p\n", BundleAddress));
                         return 0;
                    }
                }
            }
        }

        // insert break instruction

        KdpBreakpointTable[Index].Address = Address;
        KdpBreakpointTable[Index].Content = Content;
        KdpBreakpointTable[Index].Flags &= ~(KD_BREAKPOINT_STATE_MASK);
        KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_IN_USE;
        if (Address < (PVOID)GLOBAL_BREAKPOINT_LIMIT) {
            KdpBreakpointTable[Index].DirectoryTableBase =
                KeGetCurrentThread()->ApcState.Process->DirectoryTableBase[0];
            }
            switch ((ULONG_PTR)Address & 0xf) {
            case 0:
                Content = (Content & ~(INST_SLOT0_MASK)) | (KdpBreakpointInstruction << 5);
                break;

            case 4:
                Content = (Content & ~(INST_SLOT1_MASK)) | (KdpBreakpointInstruction << 14);
                break;

            case 8:
                Content = (Content & ~(INST_SLOT2_MASK)) | (KdpBreakpointInstruction << 23);
                break;

            default:
                BPVPRINT(("KD: KdpAddBreakpoint bad instruction slot#\n"));
                return 0;
            }
            if (!NT_SUCCESS(KdpCopyToPtr(Address,
                                         &Content,
                                         sizeof(KDP_BREAKPOINT_TYPE),
                                         NULL))) {

                BPVPRINT(("KD: Unable to write BP!\n"));
                return 0;
            }
            else {
                BPVPRINT(("KD: breakpoint at 0x%p set\n", Address));
            }

    } else {  // memory not accessible
        KdpBreakpointTable[Index].Address = Address;
        KdpBreakpointTable[Index].Flags &= ~(KD_BREAKPOINT_STATE_MASK);
        KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_IN_USE;
        KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_NEEDS_WRITE;
        KdpOweBreakpoint = TRUE;
        BPVPRINT(("KD: breakpoint write deferred\n"));
        if (Address < (PVOID)GLOBAL_BREAKPOINT_LIMIT) {
            KdpBreakpointTable[Index].DirectoryTableBase =
                KeGetCurrentThread()->ApcState.Process->DirectoryTableBase[0];
        }
    }
#else
    if ( Accessible ) {
        KdpBreakpointTable[Index].Address = Address;
        KdpBreakpointTable[Index].Content = Content;
        KdpBreakpointTable[Index].Flags = KD_BREAKPOINT_IN_USE;
        if (Address < (PVOID)GLOBAL_BREAKPOINT_LIMIT) {
            KdpBreakpointTable[Index].DirectoryTableBase =
                KeGetCurrentThread()->ApcState.Process->DirectoryTableBase[0];
        }
        if (!NT_SUCCESS(KdpCopyToPtr(Address,
                                     &KdpBreakpointInstruction,
                                     sizeof(KDP_BREAKPOINT_TYPE),
                                     NULL))) {

            DPRINT(("KD: Unable to write BP!\n"));
        }
    } else {
        KdpBreakpointTable[Index].Address = Address;
        KdpBreakpointTable[Index].Flags = KD_BREAKPOINT_IN_USE | KD_BREAKPOINT_NEEDS_WRITE;
        KdpOweBreakpoint = TRUE;
        BPVPRINT(("KD: breakpoint write deferred\n"));
        if (Address < (PVOID)GLOBAL_BREAKPOINT_LIMIT) {
            KdpBreakpointTable[Index].DirectoryTableBase =
                KeGetCurrentThread()->ApcState.Process->DirectoryTableBase[0];
        }
    }
#endif  // IA64

    return Index + 1;

}



VOID
KdSetOwedBreakpoints(
    VOID
    )

/*++

Routine Description:

    This function is called after returning from memory management calls
    that may cause an inpage.  Its purpose is to store pending
    breakpoints in pages just made valid.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KDP_BREAKPOINT_TYPE Content;
    BOOLEAN Enable;
    LONG Index;

    //
    // If we don't owe any breakpoints then return
    //

    if ( !KdpOweBreakpoint ) {
        return;
    }


    //
    // Freeze all other processors, disable interrupts, and save debug
    // port state.
    //

    Enable = KdEnterDebugger(NULL, NULL);
    KdpOweBreakpoint = FALSE;

    //
    // Search the breakpoint table for breakpoints that need to be
    // written or replaced.
    //

    for (Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index += 1) {
        if (KdpBreakpointTable[Index].Flags &
                (KD_BREAKPOINT_NEEDS_WRITE | KD_BREAKPOINT_NEEDS_REPLACE) ) {

            //
            // Breakpoint needs to be written
            //
            //BPVPRINT(("KD: Breakpoint %d at 0x%p: trying to %s after page in.\n",
            //          Index,
            //          KdpBreakpointTable[Index].Address,
            //          (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_NEEDS_WRITE) ?
            //          "set" : "clear"));

            if ((KdpBreakpointTable[Index].Address >= (PVOID)GLOBAL_BREAKPOINT_LIMIT) ||
                (KdpBreakpointTable[Index].DirectoryTableBase ==
                 KeGetCurrentThread()->ApcState.Process->DirectoryTableBase[0])) {

                //
                // Breakpoint is global, or its directory base matches
                //

                if (!NT_SUCCESS(KdpCopyFromPtr(&Content,
                                               KdpBreakpointTable[Index].Address,
                                               sizeof(KDP_BREAKPOINT_TYPE),
                                               NULL))) {

                    //BPVPRINT(("KD: read from 0x%p failed\n", KdpBreakpointTable[Index].Address));

                    KdpOweBreakpoint = TRUE;

                } else {
                    if (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_NEEDS_WRITE) {
                        KdpBreakpointTable[Index].Content = Content;
#if defined(_IA64_)
                        switch ((ULONG_PTR)KdpBreakpointTable[Index].Address & 0xf) {
                        case 0:
                            Content = (Content & ~(INST_SLOT0_MASK)) | (KdpBreakpointInstruction << 5);
                            break;

                        case 4:
                            Content = (Content & ~(INST_SLOT1_MASK)) | (KdpBreakpointInstruction << 14);
                            break;

                        case 8:
                            Content = (Content & ~(INST_SLOT2_MASK)) | (KdpBreakpointInstruction << 23);
                            break;

                        default:
                            BPVPRINT(("KD: illegal instruction address 0x%p\n", KdpBreakpointTable[Index].Address));
                            break;
                        }
                        if (!NT_SUCCESS(KdpCopyToPtr(KdpBreakpointTable[Index].Address,
                                                     &Content,
                                                     sizeof(KDP_BREAKPOINT_TYPE),
                                                     NULL))) {
                            KdpOweBreakpoint = TRUE;
                            BPVPRINT(("KD: write to 0x%p failed\n", KdpBreakpointTable[Index].Address));
                        }

                        // read in intruction template if current instruction is NOT slot 0.
                        // check for two-slot MOVL instruction. Reject request if attempt to
                        // set break in slot 2 of MLI template.
                        
                        else if (((ULONG_PTR)KdpBreakpointTable[Index].Address & 0xf) != 0) {
                            KDP_BREAKPOINT_TYPE mBuf;
                            PVOID BundleAddress;

                            BundleAddress = (PVOID)((ULONG_PTR)KdpBreakpointTable[Index].Address  & ~(0xf));
                            if (!NT_SUCCESS(KdpCopyFromPtr(&mBuf,
                                                           BundleAddress,
                                                           sizeof(KDP_BREAKPOINT_TYPE),
                                                           NULL))) {
                                KdpOweBreakpoint = TRUE;
                                BPVPRINT(("KD: read 0x%p template failed\n", KdpBreakpointTable[Index].Address));
                            } else {
                                if (((mBuf & INST_TEMPL_MASK) >> 1) == 0x2) {
                                    if (((ULONG_PTR)KdpBreakpointTable[Index].Address  & 0xf) == 4) {
                                        // if template= type 2 MLI, change to type 0
                                        mBuf &= ~((INST_TEMPL_MASK >> 1) << 1);
                                        KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_IA64_MOVL;
                                        if (!NT_SUCCESS(KdpCopyToPtr(BundleAddress,
                                                                     &mBuf,
                                                                     sizeof(KDP_BREAKPOINT_TYPE),
                                                                     NULL))) {
                                            KdpOweBreakpoint = TRUE;
                                            BPVPRINT(("KD: write to 0x%p template failed\n", KdpBreakpointTable[Index].Address));
                                        }
                                        else {
                                            KdpBreakpointTable[Index].Flags &= ~(KD_BREAKPOINT_STATE_MASK);
                                            KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_IN_USE;
                                            BPVPRINT(("KD: write to 0x%p ok\n", KdpBreakpointTable[Index].Address));
                                        }
                                    } else {
                                        // set breakpoint at slot 2 of MOVL is illegal
                                        KdpOweBreakpoint = TRUE;
                                        BPVPRINT(("KD: illegal attempt to set BP at slot 2 of 0x%p\n", KdpBreakpointTable[Index].Address));
                                    }
                                }
                                else {
                                    KdpBreakpointTable[Index].Flags &= ~(KD_BREAKPOINT_STATE_MASK);
                                    KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_IN_USE;
                                    BPVPRINT(("KD: write to 0x%p ok\n", KdpBreakpointTable[Index].Address));
                                }
                            }
                        } else {
                            KdpBreakpointTable[Index].Flags &= ~(KD_BREAKPOINT_STATE_MASK);
                            KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_IN_USE;
                            BPVPRINT(("KD: write to 0x%p ok\n", KdpBreakpointTable[Index].Address));
                        }
#else
                        if (!NT_SUCCESS(KdpCopyToPtr(KdpBreakpointTable[Index].Address,
                                                     &KdpBreakpointInstruction,
                                                     sizeof(KDP_BREAKPOINT_TYPE),
                                                     NULL))) {
                            KdpOweBreakpoint = TRUE;
                            DPRINT(("KD: write to 0x%p failed\n", KdpBreakpointTable[Index].Address));
                        } else {
                            KdpBreakpointTable[Index].Flags = KD_BREAKPOINT_IN_USE;
                            DPRINT(("KD: write to 0x%p ok\n", KdpBreakpointTable[Index].Address));
                        }
#endif
                    } else {
#if defined(_IA64_)

                        KDP_BREAKPOINT_TYPE mBuf;
                        PVOID BundleAddress;

                        // restore original instruction content

                        // Read in memory since adjancent instructions in the same bundle may have
                        // been modified after we save them.
                        if (!NT_SUCCESS(KdpCopyFromPtr(&mBuf,
                                                       KdpBreakpointTable[Index].Address,
                                                       sizeof(KDP_BREAKPOINT_TYPE),
                                                       NULL))) {
                            KdpOweBreakpoint = TRUE;
                            BPVPRINT(("KD: read 0x%p template failed\n", KdpBreakpointTable[Index].Address));
                        }
                        else {
                            switch ((ULONG_PTR)KdpBreakpointTable[Index].Address & 0xf) {
                            case 0:
                                mBuf = (mBuf & ~(INST_SLOT0_MASK))
                                             | (KdpBreakpointTable[Index].Content & INST_SLOT0_MASK);
                                break;

                            case 4:
                                mBuf = (mBuf & ~(INST_SLOT1_MASK))
                                             | (KdpBreakpointTable[Index].Content & INST_SLOT1_MASK);
                                break;

                            case 8:
                                mBuf = (mBuf & ~(INST_SLOT2_MASK))
                                             | (KdpBreakpointTable[Index].Content & INST_SLOT2_MASK);
                                break;

                            default:
                                KdpOweBreakpoint = TRUE;
                                BPVPRINT(("KD: illegal instruction address 0x%p\n", KdpBreakpointTable[Index].Address));
                            }

                            if (!NT_SUCCESS(KdpCopyToPtr(KdpBreakpointTable[Index].Address,
                                                         &mBuf,
                                                         sizeof(KDP_BREAKPOINT_TYPE),
                                                         NULL))) {
                                KdpOweBreakpoint = TRUE;
                                BPVPRINT(("KD: write to 0x%p failed\n", KdpBreakpointTable[Index].Address));
                            }
                            else {
                                 // restore template to MLI if displaced instruction was MOVL

                                if (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_IA64_MOVL) {
                                    BundleAddress = (PVOID)((ULONG_PTR)KdpBreakpointTable[Index].Address & ~(0xf));
                                    if (!NT_SUCCESS(KdpCopyFromPtr(&mBuf,
                                                                   BundleAddress,
                                                                   sizeof(KDP_BREAKPOINT_TYPE),
                                                                   NULL))) {
                                        KdpOweBreakpoint = TRUE;
                                        BPVPRINT(("KD: read template 0x%p failed\n", KdpBreakpointTable[Index].Address));
                                    }
                                    else {
                                        mBuf &= ~((INST_TEMPL_MASK >> 1) << 1); // set template to MLI
                                        mBuf |= 0x4;

                                        if (!NT_SUCCESS(KdpCopyToPtr(BundleAddress,
                                                                     &mBuf,
                                                                     sizeof(KDP_BREAKPOINT_TYPE),
                                                                     NULL))) {
                                            KdpOweBreakpoint = TRUE;
                                            BPVPRINT(("KD: write template to 0x%p failed\n", KdpBreakpointTable[Index].Address));
                                        } else {
                                            BPVPRINT(("KD: write to 0x%p ok\n", KdpBreakpointTable[Index].Address));
                                            if (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_SUSPENDED) {
                                                KdpBreakpointTable[Index].Flags |= (KD_BREAKPOINT_SUSPENDED | KD_BREAKPOINT_IN_USE);
                                            } else {
                                                KdpBreakpointTable[Index].Flags = 0;
                                            }
                                        }
                                    }
                                } else {
                                    BPVPRINT(("KD: write to 0x%p ok\n", KdpBreakpointTable[Index].Address));
                                    if (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_SUSPENDED) {
                                        KdpBreakpointTable[Index].Flags |= (KD_BREAKPOINT_SUSPENDED | KD_BREAKPOINT_IN_USE);
                                    } else {
                                        KdpBreakpointTable[Index].Flags = 0;
                                    }
                                }
                            }
                        }
#else
                        if (!NT_SUCCESS(KdpCopyToPtr(KdpBreakpointTable[Index].Address,
                                                     &KdpBreakpointTable[Index].Content,
                                                     sizeof(KDP_BREAKPOINT_TYPE),
                                                     NULL))) {
                            KdpOweBreakpoint = TRUE;
                            DPRINT(("KD: write to 0x%p failed\n", KdpBreakpointTable[Index].Address));
                        } else {
                            BPVPRINT(("KD: write to 0x%p ok\n", KdpBreakpointTable[Index].Address));
                            if (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_SUSPENDED) {
                                KdpBreakpointTable[Index].Flags = KD_BREAKPOINT_SUSPENDED | KD_BREAKPOINT_IN_USE;
                            } else {
                                KdpBreakpointTable[Index].Flags = 0;
                            }
                        }
#endif // _IA64_

                    }
                }

            } else {

                //
                // Breakpoint is local and its directory base does not match
                //

                KdpOweBreakpoint = TRUE;
            }
        }
    }

    KdExitDebugger(Enable);
    return;
}


BOOLEAN
KdpLowWriteContent (
    IN ULONG Index
    )

/*++

Routine Description:

    This routine attempts to replace the code that a breakpoint is
    written over.  This routine, KdpAddBreakpoint,
    KdpLowRestoreBreakpoint and KdSetOwedBreakpoints are responsible
    for getting data written as requested.  Callers should not
    examine or use KdpOweBreakpoints, and they should not set the
    NEEDS_WRITE or NEEDS_REPLACE flags.

    Callers must still look at the return value from this function,
    however: if it returns FALSE, the breakpoint record must not be
    reused until KdSetOwedBreakpoints has finished with it.

Arguments:

    Index - Supplies the index of the breakpoint table entry
        which is to be deleted.

Return Value:

    Returns TRUE if the breakpoint was removed, FALSE if it was deferred.

--*/

{
#if defined(_IA64_)
    KDP_BREAKPOINT_TYPE mBuf;
    PVOID BundleAddress;
#endif

    //
    // Do the contents need to be replaced at all?
    //

    if (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_NEEDS_WRITE) {

        //
        // The breakpoint was never written out.  Clear the flag
        // and we are done.
        //

        KdpBreakpointTable[Index].Flags &= ~KD_BREAKPOINT_NEEDS_WRITE;
        BPVPRINT(("KD: Breakpoint at 0x%p never written; flag cleared.\n",
                  KdpBreakpointTable[Index].Address));
        return TRUE;
    }

#if !defined(_IA64_)
    if (KdpBreakpointTable[Index].Content == KdpBreakpointInstruction) {

        //
        // The instruction is a breakpoint anyway.
        //

        BPVPRINT(("KD: Breakpoint at 0x%p; instr is really BP; flag cleared.\n",
                  KdpBreakpointTable[Index].Address));

        return TRUE;
    }
#endif


    //
    // Restore the instruction contents.
    //

#if defined(_IA64_)
    // Read in memory since adjancent instructions in the same bundle may have
    // been modified after we save them.
    if (!NT_SUCCESS(KdpCopyFromPtr(&mBuf,
                                   KdpBreakpointTable[Index].Address,
                                   sizeof(KDP_BREAKPOINT_TYPE),
                                   NULL))) {
        KdpOweBreakpoint = TRUE;
        BPVPRINT(("KD: read 0x%p failed\n", KdpBreakpointTable[Index].Address));
        KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_NEEDS_REPLACE;
        return FALSE;
    }
    else {

        switch ((ULONG_PTR)KdpBreakpointTable[Index].Address & 0xf) {
        case 0:
            mBuf = (mBuf & ~(INST_SLOT0_MASK))
                         | (KdpBreakpointTable[Index].Content & INST_SLOT0_MASK);
            break;

        case 4:
            mBuf = (mBuf & ~(INST_SLOT1_MASK))
                         | (KdpBreakpointTable[Index].Content & INST_SLOT1_MASK);
            break;

        case 8:
            mBuf = (mBuf & ~(INST_SLOT2_MASK))
                         | (KdpBreakpointTable[Index].Content & INST_SLOT2_MASK);
            break;

        default:
            KdpOweBreakpoint = TRUE;
            BPVPRINT(("KD: illegal instruction address 0x%p\n", KdpBreakpointTable[Index].Address));
            return FALSE;
        }

        if (!NT_SUCCESS(KdpCopyToPtr(KdpBreakpointTable[Index].Address,
                                     &mBuf,
                                     sizeof(KDP_BREAKPOINT_TYPE),
                                     NULL))) {
            KdpOweBreakpoint = TRUE;
            BPVPRINT(("KD: write to 0x%p failed\n", KdpBreakpointTable[Index].Address));
            KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_NEEDS_REPLACE;
            return FALSE;
        }
        else {

            if (NT_SUCCESS(KdpCopyFromPtr(&mBuf,
                                          KdpBreakpointTable[Index].Address,
                                          sizeof(KDP_BREAKPOINT_TYPE),
                                          NULL))) {
                BPVPRINT(("\tcontent after memory move = 0x%08x 0x%08x\n", (ULONG)(mBuf >> 32), (ULONG)mBuf));
            }

            // restore template to MLI if displaced instruction was MOVL

            if (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_IA64_MOVL) {
                BundleAddress = (PVOID)((ULONG_PTR)KdpBreakpointTable[Index].Address & ~(0xf));
                if (!NT_SUCCESS(KdpCopyFromPtr(&mBuf,
                                               BundleAddress,
                                               sizeof(KDP_BREAKPOINT_TYPE),
                                               NULL))) {
                    KdpOweBreakpoint = TRUE;
                    BPVPRINT(("KD: read template 0x%p failed\n", KdpBreakpointTable[Index].Address));
                    KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_NEEDS_REPLACE;
                    return FALSE;
                }
                else {
                    mBuf &= ~((INST_TEMPL_MASK >> 1) << 1); // set template to MLI
                    mBuf |= 0x4;

                    if (!NT_SUCCESS(KdpCopyToPtr(BundleAddress,
                                                 &mBuf,
                                                 sizeof(KDP_BREAKPOINT_TYPE),
                                                 NULL))) {
                        KdpOweBreakpoint = TRUE;
                        BPVPRINT(("KD: write template to 0x%p failed\n", KdpBreakpointTable[Index].Address));
                        KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_NEEDS_REPLACE;
                        return FALSE;
                    } else {
                        BPVPRINT(("KD: Breakpoint at 0x%p cleared.\n",
                                  KdpBreakpointTable[Index].Address));
                        return TRUE;
                    }
                }
            }
            else {   // not MOVL
                BPVPRINT(("KD: Breakpoint at 0x%p cleared.\n",
                          KdpBreakpointTable[Index].Address));
                return TRUE;
            }
        }
    }
#else
    if (!NT_SUCCESS(KdpCopyToPtr(KdpBreakpointTable[Index].Address,
                                 &KdpBreakpointTable[Index].Content,
                                 sizeof(KDP_BREAKPOINT_TYPE),
                                 NULL))) {

        KdpOweBreakpoint = TRUE;
        KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_NEEDS_REPLACE;
        BPVPRINT(("KD: Breakpoint at 0x%p; unable to clear, flag set.\n",
                  KdpBreakpointTable[Index].Address));
        return FALSE;
    } else {
        BPVPRINT(("KD: Breakpoint at 0x%p cleared.\n",
                  KdpBreakpointTable[Index].Address));
        return TRUE;
    }
#endif

}



BOOLEAN
KdpDeleteBreakpoint (
    IN ULONG Handle
    )

/*++

Routine Description:

    This routine deletes an entry from the breakpoint table.

Arguments:

    Handle - Supplies the index plus one of the breakpoint table entry
        which is to be deleted.

Return Value:

    A value of FALSE is returned if the specified handle is not a valid
    value or the breakpoint cannot be deleted because the old instruction
    cannot be replaced. Otherwise, a value of TRUE is returned.

--*/

{
    ULONG Index = Handle - 1;

    //
    // If the specified handle is not valid, then return FALSE.
    //

    if ((Handle == 0) || (Handle > BREAKPOINT_TABLE_SIZE)) {
        DPRINT(("KD: Breakpoint %d invalid.\n", Index));
        return FALSE;
    }

    //
    // If the specified breakpoint table entry is not valid, then return FALSE.
    //

    if (KdpBreakpointTable[Index].Flags == 0) {
        BPVPRINT(("KD: Breakpoint %d already clear.\n", Index));
        return FALSE;
    }

    //
    // If the breakpoint is already suspended, just delete it from the table.
    //

    if (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_SUSPENDED) {
        BPVPRINT(("KD: Deleting suspended breakpoint %d \n", Index));
        if ( !(KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_NEEDS_REPLACE) ) {
            BPVPRINT(("KD: already clear.\n"));
            KdpBreakpointTable[Index].Flags = 0;
            return TRUE;
        }
    }

    //
    // Replace the instruction contents.
    //

    if (KdpLowWriteContent(Index)) {

        //
        // Delete breakpoint table entry
        //

        BPVPRINT(("KD: Breakpoint %d deleted successfully.\n", Index));
        KdpBreakpointTable[Index].Flags = 0;
    }

    return TRUE;
}


BOOLEAN
KdpDeleteBreakpointRange (
    IN PVOID Lower,
    IN PVOID Upper
    )

/*++

Routine Description:

    This routine deletes all breakpoints falling in a given range
    from the breakpoint table.

Arguments:

    Lower - inclusive lower address of range from which to remove BPs.

    Upper - include upper address of range from which to remove BPs.

Return Value:

    TRUE if any breakpoints removed, FALSE otherwise.

--*/

{
    ULONG   Index;
    BOOLEAN ReturnStatus = FALSE;

    //
    // Examine each entry in the table in turn
    //

    for (Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index++)
    {
        if ( (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_IN_USE) &&
             ((KdpBreakpointTable[Index].Address >= Lower) &&
              (KdpBreakpointTable[Index].Address <= Upper)) )
        {

            //
            // Breakpoint is in use and falls in range, clear it.
            //

            if (KdpDeleteBreakpoint(Index+1))
            {
                ReturnStatus = TRUE;
            }
        }
    }

    return ReturnStatus;

}

VOID
KdpSuspendBreakpoint (
    ULONG Handle
    )
{
    ULONG Index = Handle - 1;

    if ( (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_IN_USE) &&
        !(KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_SUSPENDED) ) {

        KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_SUSPENDED;
        KdpLowWriteContent(Index);
    }

    return;

} // KdpSuspendBreakpoint

VOID
KdpSuspendAllBreakpoints (
    VOID
    )
{
    ULONG Handle;

    BreakpointsSuspended = TRUE;

    for ( Handle = 1; Handle <= BREAKPOINT_TABLE_SIZE; Handle++ ) {
        KdpSuspendBreakpoint(Handle);
    }

    return;

} // KdpSuspendAllBreakpoints

#if defined(_IA64_)


BOOLEAN
KdpSuspendBreakpointRange (
    IN PVOID Lower,
    IN PVOID Upper
    )

/*++

Routine Description:

    This routine suspend all breakpoints falling in a given range
    from the breakpoint table.

Arguments:

    Lower - inclusive lower address of range from which to suspend BPs.

    Upper - include upper address of range from which to suspend BPs.

Return Value:

    TRUE if any breakpoints suspended, FALSE otherwise.

Notes:
    The order of suspending breakpoints is opposite that of setting
    them in KdpAddBreakpoint() in case of duplicate addresses.

--*/

{
    ULONG   Index;
    BOOLEAN ReturnStatus = FALSE;

    BPVPRINT(("\nKD: entering KdpSuspendBreakpointRange() at 0x%p 0x%p\n", Lower, Upper));

    //
    // Examine each entry in the table in turn
    //

    for (Index = BREAKPOINT_TABLE_SIZE - 1; Index != -1; Index--) {

        if ( (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_IN_USE) &&
             ((KdpBreakpointTable[Index].Address >= Lower) &&
              (KdpBreakpointTable[Index].Address <= Upper))
           ) {

            //
            // Breakpoint is in use and falls in range, suspend it.
            //

            KdpSuspendBreakpoint(Index+1);
            ReturnStatus = TRUE;
        }
    }
    BPVPRINT(("KD: exiting KdpSuspendBreakpointRange() return 0x%d\n", ReturnStatus));

    return ReturnStatus;

} // KdpSuspendBreakpointRange



BOOLEAN
KdpRestoreBreakpointRange (
    IN PVOID Lower,
    IN PVOID Upper
    )

/*++

Routine Description:

    This routine writes back breakpoints falling in a given range
    from the breakpoint table.

Arguments:

    Lower - inclusive lower address of range from which to rewrite BPs.

    Upper - include upper address of range from which to rewrite BPs.

Return Value:

    TRUE if any breakpoints written, FALSE otherwise.

Notes:
    The order of writing breakpoints is opposite that of removing
    them in KdpSuspendBreakpointRange() in case of duplicate addresses.

--*/

{
    ULONG   Index;
    BOOLEAN ReturnStatus = FALSE;

    BPVPRINT(("\nKD: entering KdpRestoreBreakpointRange() at 0x%p 0x%p\n", Lower, Upper));

    //
    // Examine each entry in the table in turn
    //

    for (Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index++) {

        if ( (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_IN_USE) &&
             ((KdpBreakpointTable[Index].Address >= Lower) &&
              (KdpBreakpointTable[Index].Address <= Upper))
           ) {

            //
            // suspended breakpoint that falls in range, unsuspend it.
            //

            if (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_SUSPENDED) {

                KdpBreakpointTable[Index].Flags &= ~KD_BREAKPOINT_SUSPENDED;
                ReturnStatus = ReturnStatus || KdpLowRestoreBreakpoint(Index);
            }
        }
    }

    BPVPRINT(("KD: exiting KdpRestoreBreakpointRange() return 0x%d\n", ReturnStatus));

    return ReturnStatus;

} // KdpRestoreBreakpointRange

#endif // _IA64_


BOOLEAN
KdpLowRestoreBreakpoint (
    IN ULONG Index
    )

/*++

Routine Description:

    This routine attempts to write a breakpoint instruction.
    The old contents must have already been stored in the
    breakpoint record.

Arguments:

    Index - Supplies the index of the breakpoint table entry
        which is to be written.

Return Value:

    Returns TRUE if the breakpoint was written, FALSE if it was
    not and has been marked for writing later.

--*/

{
#if defined(_IA64_)
    KDP_BREAKPOINT_TYPE mBuf;
    PVOID BundleAddress;
#endif
    //
    // Does the breakpoint need to be written at all?
    //

    if (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_NEEDS_REPLACE) {

        //
        // The breakpoint was never removed.  Clear the flag
        // and we are done.
        //

        KdpBreakpointTable[Index].Flags &= ~KD_BREAKPOINT_NEEDS_REPLACE;
        return TRUE;
    }

    //
    // Replace the instruction contents.
    //

#if !defined(_IA64_)
    if (KdpBreakpointTable[Index].Content == KdpBreakpointInstruction) {

        //
        // The instruction is a breakpoint anyway.
        //

        return TRUE;
    }
#endif

    //
    // Replace the instruction contents.
    //

#if defined(_IA64_)

    // read in intruction in case the adjacent instruction has been modified.

    if (!NT_SUCCESS(KdpCopyFromPtr(&mBuf,
                                   KdpBreakpointTable[Index].Address,
                                   sizeof(KDP_BREAKPOINT_TYPE),
                                   NULL))) {
        BPVPRINT(("KD: read 0x%p template failed\n", KdpBreakpointTable[Index].Address));
        KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_NEEDS_WRITE;
        KdpOweBreakpoint = TRUE;
        return FALSE;
    }

    switch ((ULONG_PTR)KdpBreakpointTable[Index].Address & 0xf) {
        case 0:
            mBuf = (mBuf & ~(INST_SLOT0_MASK)) | (KdpBreakpointInstruction << 5);
            break;

        case 4:
            mBuf = (mBuf & ~(INST_SLOT1_MASK)) | (KdpBreakpointInstruction << 14);
            break;

        case 8:
            mBuf = (mBuf & ~(INST_SLOT2_MASK)) | (KdpBreakpointInstruction << 23);
            break;

        default:
            BPVPRINT(("KD: KdpAddBreakpoint bad instruction slot#\n"));
            return FALSE;
    }
    if (!NT_SUCCESS(KdpCopyToPtr(KdpBreakpointTable[Index].Address,
                                 &mBuf,
                                 sizeof(KDP_BREAKPOINT_TYPE),
                                 NULL))) {

        BPVPRINT(("KD: Unable to write BP!\n"));
        KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_NEEDS_WRITE;
        KdpOweBreakpoint = TRUE;
        return FALSE;
    }
    else {

        // check for two-slot MOVL instruction. Reject request if attempt to
        // set break in slot 2 of MLI template.
        // change template to type 0 if current instruction is MLI

        if (((ULONG_PTR)KdpBreakpointTable[Index].Address & 0xf) != 0) {
            BundleAddress = (PVOID)((ULONG_PTR)KdpBreakpointTable[Index].Address & ~(0xf));
            if (!NT_SUCCESS(KdpCopyFromPtr(&mBuf,
                                           BundleAddress,
                                           sizeof(KDP_BREAKPOINT_TYPE),
                                           NULL))) {
                BPVPRINT(("KD: read template failed at 0x%p\n", BundleAddress));
                KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_NEEDS_WRITE;
                KdpOweBreakpoint = TRUE;
                return FALSE;
            }
            else {
                if (((mBuf & INST_TEMPL_MASK) >> 1) == 0x2) {
                    if (((ULONG_PTR)KdpBreakpointTable[Index].Address & 0xf) == 4) {
                        // if template= type 2 MLI, change to type 0
                        mBuf &= ~((INST_TEMPL_MASK >> 1) << 1);
                        KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_IA64_MOVL;
                        if (!NT_SUCCESS(KdpCopyToPtr(BundleAddress,
                                                     &mBuf,
                                                     sizeof(KDP_BREAKPOINT_TYPE),
                                                     NULL))) {
                            BPVPRINT(("KD: write to 0x%p template failed\n", BundleAddress));
                            KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_NEEDS_WRITE;
                            KdpOweBreakpoint = TRUE;
                            return FALSE;
                        }
                        else {
                             BPVPRINT(("KD: change MLI template to type 0 at 0x%p set\n", BundleAddress));
                        }
                    } else {
                         // set breakpoint at slot 2 of MOVL is illegal
                         BPVPRINT(("KD: illegal to set BP at slot 2 of MOVL at 0x%p\n", BundleAddress));
                         KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_NEEDS_WRITE;
                         KdpOweBreakpoint = TRUE;
                         return FALSE;
                    }
                }
            }
        }
        BPVPRINT(("KD: breakpoint at 0x%p set\n", KdpBreakpointTable[Index].Address));
        return TRUE;
    }

#else
    if (!NT_SUCCESS(KdpCopyToPtr(KdpBreakpointTable[Index].Address,
                                 &KdpBreakpointInstruction,
                                 sizeof(KDP_BREAKPOINT_TYPE),
                                 NULL))) {

        KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_NEEDS_WRITE;
        KdpOweBreakpoint = TRUE;
        BPVPRINT(("KD: owe breakpoint at 0x%p\n", KdpBreakpointTable[Index].Address));
        return FALSE;

    } else {

        KdpBreakpointTable[Index].Flags &= ~KD_BREAKPOINT_NEEDS_WRITE;
        BPVPRINT(("KD: breakpoint at 0x%p set\n", KdpBreakpointTable[Index].Address));
        return TRUE;
    }
#endif

}


VOID
KdpRestoreAllBreakpoints (
    VOID
    )
{
    ULONG Index;

    BreakpointsSuspended = FALSE;

    for ( Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index++ ) {

        if ((KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_IN_USE) &&
            (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_SUSPENDED) ) {

            KdpBreakpointTable[Index].Flags &= ~KD_BREAKPOINT_SUSPENDED;
            KdpLowRestoreBreakpoint(Index);
        }
    }

    return;

} // KdpRestoreAllBreakpoints

VOID
KdDeleteAllBreakpoints(
    VOID
    )
{
    ULONG Handle;

    if (KdDebuggerEnabled == FALSE || KdPitchDebugger != FALSE) {
        return;
    }

    BreakpointsSuspended = FALSE;

    for ( Handle = 1; Handle <= BREAKPOINT_TABLE_SIZE; Handle++ ) {
        KdpDeleteBreakpoint(Handle);
    }

    return;
} // KdDeleteAllBreakpoints


VOID
KdCheckForDebugBreak(
    VOID
)
/*++

Routine Description:

    If necessary, poll for a request to break-in from the debugger.
    This function should be called by routines that run at an IRQL
    above clock level that want to be broken in by CTRL-C requests
    from the debugger. Crashdump and hiber, for example, run at
    HIGH_LEVEL and explicitly need to poll for breaking.

Arguments:

    None.

Return Value:

    None.

--*/
{
    if (KdDebuggerEnabled && KdPollBreakIn()) {
        DbgBreakPointWithStatus(DBG_STATUS_CONTROL_C);
    }
}
