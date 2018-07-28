; ---------------------------------------------------------------------------
; interrupt.s
; ---------------------------------------------------------------------------
;
; Interrupt handler.
;
; Checks for a BRK instruction and returns from all valid interrupts.

.import   _stop
.export   _irq_int, _nmi_int

.segment  "CODE"

.PC02                             ; Force 65C02 assembly mode

; ---------------------------------------------------------------------------
; Non-maskable interrupt (NMI) service routine

_nmi_int:  RTI                    ; Return from all NMI interrupts

; ---------------------------------------------------------------------------
; Maskable interrupt (IRQ) service routine

_irq_int:  PHX                    ; Save X register contents to stack
           TSX                    ; Transfer stack pointer to X
           PHA                    ; Save accumulator contents to stack
           INX                    ; Increment X so it points to the status
           INX                    ;   register value saved on the stack
           LDA $100,X             ; Load status register contents
           AND #$10               ; Isolate B status bit
           BNE break              ; If B = 1, BRK detected

; ---------------------------------------------------------------------------
; IRQ detected, return

irq:       PLA                    ; Restore accumulator contents
           PLX                    ; Restore X register contents
           RTI                    ; Return from all IRQ interrupts

; ---------------------------------------------------------------------------
; BRK detected, stop

break:     JMP break              ; If BRK is detected, something very bad
                                  ;   has happened, so stop running

