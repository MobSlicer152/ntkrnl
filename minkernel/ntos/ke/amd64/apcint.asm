        title  "Asynchronous Procedure Call Interrupt"
;++
;
; Copyright (c) 2000  Microsoft Corporation
;
; Module Name:
;
;   apcint.asm
;
; Abstract:
;
;   This module implements the code necessary to process the  Asynchronous
;   Procedure Call interrupt request.
;
; Author:
;
;   David N. Cutler (davec) 10-Nov-2000
;
; Environment:
;
;    Kernel mode only.
;
;--

        extern  KiDeliverApc:proc
        extern  __imp_HalEndSystemInterrupt:qword

include ksamd64.inc

        subttl  "Asynchronous Procedure Call Interrupt"
;++
;
; VOID
; KiApcInterrupt (
;     VOID
;     )
;
; Routine Description:
;
;   This routine is entered as the result of a software interrupt generated
;   at APC_LEVEL. Its function is to save the machine state and call the APC
;   delivery routine.
;
;   N.B. This is a directly connected interrupt that does not use an interrupt
;        object.
;
;   N.B. APC interrupts are never requested for user mode APCs.
;
; Arguments:
;
;   None.
;
; Return Value:
;
;   None.
;
;--

        NESTED_ENTRY KiApcInterrupt, _TEXT$00

        .pushframe                      ; mark machine frame

        push_reg rbp                    ; push dummy vector
        push_reg rbp                    ; save nonvolatile register

        GENERATE_INTERRUPT_FRAME        ; generate interrupt frame

        mov     ecx, APC_LEVEL          ; set new IRQL level

	ENTER_INTERRUPT			; raise IRQL, do EOI, enable interrupts

        mov     cl, KernelMode          ; set APC processor mode
        xor     edx, edx                ; set exception frame address
        lea     r8, (-128)[rbp]         ; set trap frame address
        call    KiDeliverApc            ; initiate APC execution

        EXIT_INTERRUPT <NoEOI>          ; lower IRQL and restore state

        NESTED_END KiApcInterrupt, _TEXT$00

        subttl  "Initiate User APC Execution"
;++
;
; Routine Description:
;
;   This routine generates an exception frame and attempts to deliver a user
;   APC.
;
; Arguments:
;
;   rbp - Supplies the address of the trap frame.
;
;   rsp - Supplies the address of the trap frame.
;
; Return value:
;
;   None.
;
;--

        NESTED_ENTRY KiInitiateUserApc, _TEXT$00

        GENERATE_EXCEPTION_FRAME        ; generate exception frame

        mov     rbx, gs:[PcCurrentThread] ; get current thread address
        cmp     byte ptr ThNpxState[rbx], LEGACY_STATE_SWITCH ; check if switched
        jne     short KiIU10            ; if ne, legacy state not switched

;
; N.B. The legacy floating point state must be saved and restored since saving
;      the state initializes some of the state.
;
; N.B. Interrupts must also be disabled during this sequence to ensure that a
;      get context APC interrupt does not occur.
;

        lea     rsi, (KTRAP_FRAME_LENGTH - 128)[rbp] ; get legacy save address
        cli                             ; disable interrupts
        fnsave  [esi]                   ; save legacy floating state
        mov     di, LfControlWord[rsi]  ; save current control word
        mov     word ptr LfControlWord[rsi], 03fh ; set to mask all exceptions
        frstor  [esi]                   ; restore legacy floating point state
        mov     LfControlWord[rsi], di  ; restore control word
        fldcw   word ptr LfControlWord[rsi] ; load legacy control word
        sti                             ; enable interrupts
KiIU10: mov     cl, UserMode            ; set APC processor mode
        mov     rdx, rsp                ; set exception frame address
        lea     r8, (-128)[rbp]         ; set trap frame address
        call    KiDeliverApc            ; deliver APC

        RESTORE_EXCEPTION_STATE         ; restore exception state/deallocate

        ret                             ; return

        NESTED_END KiInitiateUserApc, _TEXT$00

        end
