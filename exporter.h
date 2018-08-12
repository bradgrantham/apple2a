#ifndef __EXPORTER_H__
#define __EXPORTER_H__

// Defines functions exported in exporter.s.

extern void pushax();
extern void popax();
extern void incsp2();
extern void tosaddax();
extern void tossubax();
extern void tosmulax();
extern void tosdivax();
extern void toseqax();
extern void tosneax();
extern void tosltax();
extern void tosgtax();
extern void tosleax();
extern void tosgeax();
extern void bnegax();
extern void negax();
extern void aslax1();
extern void ldaxi();
extern void staxspidx();

// Two bytes each.
extern unsigned int sp;
#pragma zpsym ("sp");
extern unsigned int ptr1;
#pragma zpsym ("ptr1");

// One byte each.
extern unsigned char tmp1;
#pragma zpsym ("tmp1");
extern unsigned char tmp2;
#pragma zpsym ("tmp2");

#endif // __EXPORTER_H__
