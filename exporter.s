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
.import     incsp2
.export     _incsp2 := incsp2
.import     tosaddax
.export     _tosaddax := tosaddax
.import     tossubax
.export     _tossubax := tossubax
.import     tosmulax
.export     _tosmulax := tosmulax
.import     tosdivax
.export     _tosdivax := tosdivax
.import     toseqax
.export     _toseqax := toseqax
.import     tosneax
.export     _tosneax := tosneax
.import     tosltax
.export     _tosltax := tosltax
.import     tosgtax
.export     _tosgtax := tosgtax
.import     tosleax
.export     _tosleax := tosleax
.import     tosgeax
.export     _tosgeax := tosgeax
.import     bnegax
.export     _bnegax := bnegax
.import     negax
.export     _negax := negax
.import     aslax1
.export     _aslax1 := aslax1
.import     ldaxi
.export     _ldaxi := ldaxi
.import     staxspidx
.export     _staxspidx := staxspidx

.importzp   sp
.exportzp   _sp = sp

.importzp   ptr1
.exportzp   _ptr1 = ptr1

.importzp   tmp1
.exportzp   _tmp1 = tmp1

