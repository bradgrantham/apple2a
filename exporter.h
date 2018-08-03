#ifndef __EXPORTER_H__
#define __EXPORTER_H__

// Defines functions exported in exporter.s.

extern void pushax();
extern void popax();
extern void tosaddax();
extern void tossubax();
extern void tosmulax();
extern void tosdivax();

// Two bytes each.
extern unsigned int ptr1;
#pragma zpsym ("ptr1");

// One byte each.
extern unsigned char tmp1;
#pragma zpsym ("tmp1");

#endif // __EXPORTER_H__
