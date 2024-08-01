// -------------------------------------------------------------
// (p) & (c) 2000: Roland J. Graf, AUSTRIA
//                 eSplines.com  
//                 roland.graf@aon.at
// -------------------------------------------------------------


#ifndef __EXTTIMEFORMAT_H__
#define __EXTTIMEFORMAT_H__

#include <windows.h>

extern "C" {

   const char* strfscode( const char* code );
   size_t strfstime( char *strDest, size_t maxsize, const char *format, const SYSTEMTIME *stimeptr );
};
   //const char* wcsfscode( const wchar_t* code );
   //size_t wcsfstime( wchar_t *strDest, size_t maxsize, const wchar_t *format, const SYSTEMTIME *stimeptr );


// -------------------------------------------------------------

#endif

#include <tchar.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>

#define ASSERT assert


static char szFCode[3] = _T("%s");

// strfscode: Sets the millisecond  char formatting code 
const char* strfscode( const char* code )
{
   if( code ){
      strncpy( szFCode, code, sizeof(szFCode)-1);
      szFCode[2] = char(0);
   }
   return szFCode;
}


// strfstime: Extended time string formatting with millisecond.
size_t strfstime( char *strDest, size_t maxsize, const char *format, const SYSTEMTIME *stimeptr )
{
   struct tm tmTime;
   
   memset(&tmTime, 0, sizeof(tmTime));
   
   tmTime.tm_sec  = stimeptr->wSecond;    /* seconds after the minute - [0,59] */
   tmTime.tm_min  = stimeptr->wMinute;    /* minutes after the hour - [0,59] */
   tmTime.tm_hour = stimeptr->wHour;      /* hours since midnight - [0,23] */
	ASSERT(stimeptr->wDay >= 1 && stimeptr->wDay <= 31);
   tmTime.tm_mday = stimeptr->wDay;       /* day of the month - [1,31] */
	ASSERT(stimeptr->wMonth >= 1 && stimeptr->wMonth <= 12);
   tmTime.tm_mon  = stimeptr->wMonth-1;   /* months since January - [0,11] */
	ASSERT(stimeptr->wYear >= 1900);
   tmTime.tm_year = stimeptr->wYear-1900; /* years since 1900 */
   tmTime.tm_wday = stimeptr->wDayOfWeek; /* days since Sunday - [0,6] */
   // tmTime.tm_yday  = NOT USED HERE;    /* days since January 1 - [0,365] */
   // tmTime.tm_isdst = NOT USED HERE;    /* daylight savings time flag */

   const char* pStrFound = strstr(format, szFCode );
   
   if( pStrFound ){
      
      if( maxsize < (strlen(format)+1+3) )
         return 0;
      
      char* pNewFormat = new char[maxsize+1];
      
      long lOffset = pStrFound-format;
      strncpy(pNewFormat, format, lOffset);
      pNewFormat[lOffset] = char(0);
      
      char szMilliSec[4];
      itoa(stimeptr->wMilliseconds, szMilliSec, 10);
      for(int i=strlen(szMilliSec); i<3; i++ )
         strcat(pNewFormat+lOffset, "0"); 
      
      strcat(pNewFormat+lOffset, szMilliSec);
      strcat(pNewFormat+lOffset, format+lOffset+strlen(szFCode));
      
      size_t size = strftime( strDest, maxsize, pNewFormat, &tmTime );
      
      delete pNewFormat;
      
      return size;
   }
   return strftime( strDest, maxsize, format, &tmTime );
}

// -------------------------------------------------------------

static wchar_t wszFCode_t[3] = L"%s";


// wcsfscode: Sets the millisecond  wchar_t formatting code 
const char* wcsfscode( const wchar_t* code )
{
   if( code ){
      wcsncpy( wszFCode_t, code, sizeof(szFCode)-1);
      wszFCode_t[2] = wchar_t(0);
   }
   return szFCode;
}


size_t wcsfstime( wchar_t *strDest, size_t maxsize, const wchar_t *format, const SYSTEMTIME *stimeptr )
{
   struct tm tmTime;
   
   memset(&tmTime, 0, sizeof(tmTime));
   
   tmTime.tm_sec  = stimeptr->wSecond;    /* seconds after the minute - [0,59] */
   tmTime.tm_min  = stimeptr->wMinute;    /* minutes after the hour - [0,59] */
   tmTime.tm_hour = stimeptr->wHour;      /* hours since midnight - [0,23] */
	ASSERT(stimeptr->wDay >= 1 && stimeptr->wDay <= 31);
   tmTime.tm_mday = stimeptr->wDay;       /* day of the month - [1,31] */
	ASSERT(stimeptr->wMonth >= 1 && stimeptr->wMonth <= 12);
   tmTime.tm_mon  = stimeptr->wMonth-1;   /* months since January - [0,11] */
	ASSERT(stimeptr->wYear >= 1900);
   tmTime.tm_year = stimeptr->wYear-1900; /* years since 1900 */
   tmTime.tm_wday = stimeptr->wDayOfWeek; /* days since Sunday - [0,6] */
   // tmTime.tm_yday  = NOT USED HERE;    /* days since January 1 - [0,365] */
   // tmTime.tm_isdst = NOT USED HERE;    /* daylight savings time flag */

   const wchar_t* pStrFound = wcsstr(format, wszFCode_t );
   
   if( pStrFound ){
   
      if( maxsize < (wcslen(format)+1+3) )
         return 0;
      
      wchar_t* pNewFormat = new wchar_t[maxsize+1];
      
      long lOffset = pStrFound-format;
      wcsncpy(pNewFormat, format, lOffset);
      pNewFormat[lOffset] = wchar_t(0);
      
      wchar_t wszMilliSec[4];
      _itow(stimeptr->wMilliseconds, wszMilliSec, 10);
      for(int i=wcslen(wszMilliSec); i<3; i++ )
         wcscat(pNewFormat+lOffset, L"0"); 
      
      wcscat(pNewFormat+lOffset, wszMilliSec);
      wcscat(pNewFormat+lOffset, format+lOffset+wcslen(wszFCode_t));
      
      size_t size = wcsftime( strDest, maxsize, pNewFormat, &tmTime );
      
      delete pNewFormat;
      
      return size;
   }
   return wcsftime( strDest, maxsize, format, &tmTime );
}

// -------------------------------------------------------------

