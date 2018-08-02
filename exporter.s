; ---------------------------------------------------------------------------
; exporter.s
; ---------------------------------------------------------------------------
;
; Exports cc65-internal routines (such as "pushax") as C-visible ones ("_pushax").
; See the companion header file exporter.h.

.import     pushax
.import     popax
.import     tosaddax
.export     _pushax := pushax
.export     _popax := popax
.export     _tosaddax := tosaddax

