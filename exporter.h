#ifndef __EXPORTER_H__
#define __EXPORTER_H__

// Defines functions exported in exporter.s.

extern void pushax();
extern void popax();
extern void tosaddax();

extern char ptr1;
#pragma zpsym ("ptr1");

#endif // __EXPORTER_H__
