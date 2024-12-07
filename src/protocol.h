
// protocol.h
#ifdef MULTITHREAD_ENABLED
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#endif

#ifndef PROTOCOL_H
#define PROTOCOL_H

// includes

#include "util.h"

#ifdef MULTITHREAD_ENABLED
// variables
#ifdef _WIN32
extern CRITICAL_SECTION CriticalSection; 
#else
extern  pthread_mutex_t CriticalSection;
#endif
#endif

// functions

extern void loop  ();
extern void event ();

#ifdef USE_OPENING_BOOK
extern void book_parameter();
#endif

extern void get   (char string[], int size);
extern void send  (const char format[], ...);

#endif // !defined PROTOCOL_H

// end of protocol.h

