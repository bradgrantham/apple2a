; ---------------------------------------------------------------------------
; exporter.s
; ---------------------------------------------------------------------------
;
; Exports cc65-internal routines (such as "pushax") as C-visible ones ("_pushax").
; See the companion header file exporter.h.

.import     pushax
.export     _pushax := pushax
.import     popax
.export     _popax := popax
.import     tosaddax
.export     _tosaddax := tosaddax
.import     tossubax
.export     _tossubax := tossubax
.import     tosmulax
.export     _tosmulax := tosmulax
.import     tosdivax
.export     _tosdivax := tosdivax

.importzp   ptr1
.exportzp   _ptr1 = ptr1

.importzp   tmp1
.exportzp   _tmp1 = tmp1

