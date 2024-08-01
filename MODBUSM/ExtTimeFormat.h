// -------------------------------------------------------------
// (p) & (c) 2000: Roland J. Graf, AUSTRIA
//                 eSplines.com  
//                 roland.graf@aon.at
// -------------------------------------------------------------

// Extended time string formatting with millisecond

#ifndef __EXTTIMEFORMAT_H__
#define __EXTTIMEFORMAT_H__

#include <windows.h>

   const char* strfscode( const char* code );
   size_t strfstime( char *strDest, size_t maxsize, const char *format, const SYSTEMTIME *stimeptr );

   //const char* wcsfscode( const wchar_t* code );
   //size_t wcsfstime( wchar_t *strDest, size_t maxsize, const wchar_t *format, const SYSTEMTIME *stimeptr );


// -------------------------------------------------------------

#endif // __EXTTIMEFORMAT_H__

