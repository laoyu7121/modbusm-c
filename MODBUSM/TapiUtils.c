/* ------------------------------------------------------------------
	TAPIUTILS.C
		Tapi additions to modbus.dll  9/97

--------------------------------------------------------------------*/

#define TAPI_CURRENT_VERSION 0x00010004

#include <windows.h>
#include <memory.h>
#include <string.h>
#include <tapi.h>
#include "winsock.h"

#include "modbusm.h"
#include "mbglobals.h"

typedef struct tagCommID {
        HANDLE hComm;
        char szDeviceName[1];
} CommID;

extern	struct GlobalData *pGlobal;
extern	DWORD		NumbrLineDevices;
extern	HLINEAPP	TapiUsageHandle;

HANDLE AllocateNetResources (WORD Protocol, int Timeout);
void ReleaseNetResources (LPCONNECTION	pNet);
LPCONNECTION FindConnect (HANDLE id);
BOOL AllocateControlThread (LPCONNECTION pNet, LPSERIALCONFIG	pCfg);


// All TAPI line functions return 0 for SUCCESS, so define it.
#define SUCCESS 0

// TAPI version that this sample is designed to use.
#define SAMPLE_TAPI_VERSION 0x00010004

// Early TAPI version
#define EARLY_TAPI_VERSION 0x00010003

// Possible return error for resynchronization functions.
#define WAITERR_WAITABORTED  1

DWORD	m_dwAPIVersion;   // the API version

BOOL HangupCall(LPCONNECTION	pNet);
BOOL ShutdownTAPI();

char		CallState[50];

HANDLE GetCommHandle (HCALL htCall)
{
	CommID FAR *cid;
	VARSTRING  *vs;
	LONG lrc;
	DWORD dwSize;
	HANDLE	hComDev;

    vs = (VARSTRING *) malloc (sizeof(VARSTRING));
    vs->dwTotalSize = sizeof(VARSTRING);
    vs->dwStringFormat = STRINGFORMAT_BINARY;

    do {
            /* get Win32 Comm handle associated with the call -  the call handle
            came from TAPI after making or answering a call */
            /* another Unimodem bug - should return structuretoosmall but instead returns
            success with needed > total */
			
			lrc = lineGetID(NULL, 0L, htCall, LINECALLSELECT_CALL, vs, "comm/datamodem");
            if (( lrc == LINEERR_STRUCTURETOOSMALL)  || (vs->dwTotalSize < vs->dwNeededSize)) 
			{
                    dwSize = vs->dwNeededSize;
                    free (vs);
                    vs = (VARSTRING *) malloc(dwSize);
                    vs->dwTotalSize = dwSize;
            } /* end if (need more space) */
            else if (lrc >= 0) 
                break; /* success  */
    } while (TRUE);

    cid = (CommID FAR *) ((LPSTR)vs + vs->dwStringOffset);

    hComDev = cid->hComm;
	free(vs);

	return (hComDev);
} /* end function (GetCommHandle) */

//
void HandleLineCallState(
    DWORD dwDevice, DWORD dwMessage, DWORD dwCallbackInstance,
    DWORD dwParam1, DWORD dwParam2, DWORD dwParam3)
{
	int	i;
	LPCONNECTION	pNet;
	HANDLE idConnect;

	for (i=0; i<MAX_CONNECTS; i++)
		{
		if (pGlobal->net[i].hCall == (HCALL)dwDevice)
			break;
		}

    // Error if this CALLSTATE doesn't apply to our call in progress.
    if (i >= MAX_CONNECTS)
        return;	
	
	idConnect = (HANDLE)(i + 1);
	pNet = &(pGlobal->net[i]);


    // dwParam1 is the specific CALLSTATE change that is occurring.
    switch (dwParam1)
    {
        case LINECALLSTATE_DIALTONE:
            strncpy (CallState, "Dial Tone", 50);
            break;

        case LINECALLSTATE_DIALING:
            strncpy (CallState, "Dialing", 50);
            break;

        case LINECALLSTATE_PROCEEDING:
            strncpy (CallState, "Proceeding", 50);
            break;

        case LINECALLSTATE_RINGBACK:
            strncpy (CallState, "RingBack", 50);
            break;

        case LINECALLSTATE_BUSY:
            strncpy (CallState, "Line busy, shutting down", 50);
            HangupCall(pNet);
            break;

        case LINECALLSTATE_IDLE:
            strncpy (CallState, "Line idle", 50);
            HangupCall(pNet);
            break;

        case LINECALLSTATE_SPECIALINFO:
            strncpy (CallState, "Special Info, probably couldn't dial number", 50);
            HangupCall(pNet);
            break;

        case LINECALLSTATE_DISCONNECTED:
        {


            switch (dwParam2)
            {
                case LINEDISCONNECTMODE_NORMAL:
                    strncpy (CallState, "Remote Party Disconnected", 50);
                    break;

                case LINEDISCONNECTMODE_UNKNOWN:
                    strncpy (CallState, "Disconnected: Unknown reason", 50);
                    break;

                case LINEDISCONNECTMODE_REJECT:
                    strncpy (CallState, "Remote Party rejected call", 50);
                    break;

                case LINEDISCONNECTMODE_PICKUP:
                    strncpy (CallState, "Disconnected: Local phone picked up", 50);
                    break;

                case LINEDISCONNECTMODE_FORWARDED:
                    strncpy (CallState, "Disconnected: Forwarded", 50);
                    break;

                case LINEDISCONNECTMODE_BUSY:
                    strncpy (CallState, "Disconnected: Busy", 50);
                    break;

                case LINEDISCONNECTMODE_NOANSWER:
                    strncpy (CallState, "Disconnected: No Answer", 50);
                    break;

                case LINEDISCONNECTMODE_BADADDRESS:
                    strncpy (CallState, "Disconnected: Bad Address", 50);
                    break;

                case LINEDISCONNECTMODE_UNREACHABLE:
                    strncpy (CallState, "Disconnected: Unreachable", 50);
                    break;

                case LINEDISCONNECTMODE_CONGESTION:
                    strncpy (CallState, "Disconnected: Congestion", 50);
                    break;

                case LINEDISCONNECTMODE_INCOMPATIBLE:
                    strncpy (CallState, "Disconnected: Incompatible", 50);
                    break;

                case LINEDISCONNECTMODE_UNAVAIL:
                    strncpy (CallState, "Disconnected: Unavail", 50);
                    break;

                case LINEDISCONNECTMODE_NODIALTONE:
                    strncpy (CallState, "Disconnected: No Dial Tone", 50);
                    break;

                default:
                    strncpy (CallState, "Disconnected: LINECALLSTATE; Bad Reason", 50);
                    break;

            }

            HangupCall(pNet);
            break;
        }

        case LINECALLSTATE_CONNECTED:  // CONNECTED!!!
            strncpy (CallState, "Connected!", 50);

			// Setup modbus connection
			pNet->idComDev = GetCommHandle (pNet->hCall);

			if (!AllocateControlThread (pNet, NULL))
				{
				PostMessage (pNet->hWnd, pNet->ConnectNotifyMsg, CALLDISCONNECT, 0);
				HangupCall(pNet);
				ReleaseNetResources(pNet);
				}
			else
				{
				// obtain Connection Handle and pass back to ModScan
				pNet->DirectConnection = FALSE;
				PostMessage (pNet->hWnd, pNet->ConnectNotifyMsg, CALLESTABLISHED, (DWORD)idConnect);
				}
			break;


        default:
            strncpy (CallState, "Unhandled LINECALLSTATE message", 50);
            break;
    }

}

void CALLBACK TapiCallbackFunction (DWORD dwDevice, DWORD dwMsg, DWORD dwCallbackInstance,
									DWORD dwParam1, DWORD dwParam2, DWORD dwParam3)
{
	int	i;

    switch(dwMsg)
    {
        case LINE_CALLSTATE:
			HandleLineCallState(dwDevice, dwMsg, dwCallbackInstance, dwParam1, dwParam2, dwParam3);
            break;

        case LINE_CLOSE:
            // Line has been shut down.
			for (i=0; i<MAX_CONNECTS; i++)
				{
				if (pGlobal->net[i].hLine == (HLINE)dwDevice)
					{
					pGlobal->net[i].hLine = NULL;
					pGlobal->net[i].hCall = NULL;
					HangupCall(&pGlobal->net[i]); // all handles invalidated by this time
					}
				}
            break;

        case LINE_CREATE:
            if (NumbrLineDevices <= dwParam1)
                NumbrLineDevices = dwParam1+1;
            break;

        case LINE_REPLY:
			for (i=0; i<MAX_CONNECTS; i++)
				{
				if (dwParam1 == (DWORD)pGlobal->net[i].idRequest)
					{
					pGlobal->net[i].AsyncReply = dwParam2;
					pGlobal->net[i].ReplyReceived = TRUE;
					}
				}
				break;

        default:
            break;
    }
    return;
}

//
//  FUNCTION: HandleLineErr(long)
//
//  PURPOSE: Handle several of the standard LINEERR errors
//
BOOL HandleLineErr(long lLineErr)
{
    BOOL bRet = FALSE;

    // lLineErr is really an async request ID, not an error.
    if (lLineErr > SUCCESS)
        return bRet;

    // All we do is dispatch the correct error handler.
    switch(lLineErr)
    {
        case SUCCESS:
            bRet = TRUE;
            break;
        
		case LINEERR_REINIT:
            ShutdownTAPI();
            break;

		case LINEERR_ALLOCATED:
                      strncpy (CallState, "LINEERR_ALLOCATED", 50);
					  break;
		case LINEERR_BADDEVICEID:
							  strncpy (CallState, "LINEERR_BADDEVICEID", 50);
							  break;
		case LINEERR_BEARERMODEUNAVAIL:
							  strncpy (CallState, "LINEERR_BEARERMODEUNAVAIL", 50);
							  break;
		case LINEERR_CALLUNAVAIL:
							  strncpy (CallState, "LINEERR_CALLUNAVAIL", 50);
							  break;
		case LINEERR_COMPLETIONOVERRUN:
							  strncpy (CallState, "LINEERR_COMPLETIONOVERRUN", 50);
							  break;
		case LINEERR_CONFERENCEFULL:
							  strncpy (CallState, "LINEERR_CONFERENCEFULL", 50);
							  break;
		case LINEERR_DIALBILLING:
							  strncpy (CallState, "LINEERR_DIALBILLING", 50);
							  break;
		case LINEERR_DIALDIALTONE:
							  strncpy (CallState, "LINEERR_DIALDIALTONE", 50);
							  break;
		case LINEERR_DIALPROMPT:
							  strncpy (CallState, "LINEERR_DIALPROMPT", 50);
							  break;
		case LINEERR_DIALQUIET:
							  strncpy (CallState, "LINEERR_DIALQUIET", 50);
							  break;
		case LINEERR_INCOMPATIBLEAPIVERSION:
							  strncpy (CallState, "LINEERR_INCOMPATIBLEAPIVERSION", 50);
							  break;
		case LINEERR_INCOMPATIBLEEXTVERSION:
							  strncpy (CallState, "LINEERR_INCOMPATIBLEEXTVERSION", 50);
							  break;
		case LINEERR_INIFILECORRUPT:
							  strncpy (CallState, "LINEERR_INIFILECORRUPT", 50);
							  break;
		case LINEERR_INUSE:
							  strncpy (CallState, "LINEERR_INUSE", 50);
							  break;
		case LINEERR_INVALADDRESS:
							  strncpy (CallState, "LINEERR_INVALADDRESS", 50);
							  break;
		case LINEERR_INVALADDRESSID:
							  strncpy (CallState, "LINEERR_INVALADDRESSID", 50);
							  break;
		case LINEERR_INVALADDRESSMODE:
							  strncpy (CallState, "LINEERR_INVALADDRESSMODE", 50);
							  break;
		case LINEERR_INVALADDRESSSTATE:
							  strncpy (CallState, "LINEERR_INVALADDRESSSTATE", 50);
							  break;
		case LINEERR_INVALAPPHANDLE:
							  strncpy (CallState, "LINEERR_INVALAPPHANDLE", 50);
							  break;
		case LINEERR_INVALAPPNAME:
							  strncpy (CallState, "LINEERR_INVALAPPNAME", 50);
							  break;
		case LINEERR_INVALBEARERMODE:
							  strncpy (CallState, "LINEERR_INVALBEARERMODE", 50);
							  break;
		case LINEERR_INVALCALLCOMPLMODE:
							  strncpy (CallState, "LINEERR_INVALCALLCOMPLMODE", 50);
							  break;
		case LINEERR_INVALCALLHANDLE:
							  strncpy (CallState, "LINEERR_INVALCALLHANDLE", 50);
							  break;
		case LINEERR_INVALCALLPARAMS:
							  strncpy (CallState, "LINEERR_INVALCALLPARAMS", 50);
							  break;
		case LINEERR_INVALCALLPRIVILEGE:
							  strncpy (CallState, "LINEERR_INVALCALLPRIVILEGE", 50);
							  break;
		case LINEERR_INVALCALLSELECT:
							  strncpy (CallState, "LINEERR_INVALCALLSELECT", 50);
							  break;
		case LINEERR_INVALCALLSTATE:
							  strncpy (CallState, "LINEERR_INVALCALLSTATE", 50);
							  break;
		case LINEERR_INVALCALLSTATELIST:
							  strncpy (CallState, "LINEERR_INVALCALLSTATELIST", 50);
							  break;
		case LINEERR_INVALCARD:
							  strncpy (CallState, "LINEERR_INVALCARD", 50);
							  break;
		case LINEERR_INVALCOMPLETIONID:
							  strncpy (CallState, "LINEERR_INVALCOMPLETIONID", 50);
							  break;
		case LINEERR_INVALCONFCALLHANDLE:
							  strncpy (CallState, "LINEERR_INVALCONFCALLHANDLE", 50);
							  break;
		case LINEERR_INVALCONSULTCALLHANDLE:
							  strncpy (CallState, "LINEERR_INVALCONSULTCALLHANDLE", 50);
							  break;
		case LINEERR_INVALCOUNTRYCODE:
							  strncpy (CallState, "LINEERR_INVALCOUNTRYCODE", 50);
							  break;
		case LINEERR_INVALDEVICECLASS:
							  strncpy (CallState, "LINEERR_INVALDEVICECLASS", 50);
							  break;
		case LINEERR_INVALDEVICEHANDLE:
							  strncpy (CallState, "LINEERR_INVALDEVICEHANDLE", 50);
							  break;
		case LINEERR_INVALDIALPARAMS:
							  strncpy (CallState, "LINEERR_INVALDIALPARAMS", 50);
							  break;
		case LINEERR_INVALDIGITLIST:
							  strncpy (CallState, "LINEERR_INVALDIGITLIST", 50);
							  break;
		case LINEERR_INVALDIGITMODE:
							  strncpy (CallState, "LINEERR_INVALDIGITMODE", 50);
							  break;
		case LINEERR_INVALDIGITS:
							  strncpy (CallState, "LINEERR_INVALDIGITS", 50);
							  break;
		case LINEERR_INVALEXTVERSION:
							  strncpy (CallState, "LINEERR_INVALEXTVERSION", 50);
							  break;
		case LINEERR_INVALGROUPID:
							  strncpy (CallState, "LINEERR_INVALGROUPID", 50);
							  break;
		case LINEERR_INVALLINEHANDLE:
							  strncpy (CallState, "LINEERR_INVALLINEHANDLE", 50);
							  break;
		case LINEERR_INVALLINESTATE:
							  strncpy (CallState, "LINEERR_INVALLINESTATE", 50);
							  break;
		case LINEERR_INVALLOCATION:
							  strncpy (CallState, "LINEERR_INVALLOCATION", 50);
							  break;
		case LINEERR_INVALMEDIALIST:
							  strncpy (CallState, "LINEERR_INVALMEDIALIST", 50);
							  break;
		case LINEERR_INVALMEDIAMODE:
							  strncpy (CallState, "LINEERR_INVALMEDIAMODE", 50);
							  break;
		case LINEERR_INVALMESSAGEID:
							  strncpy (CallState, "LINEERR_INVALMESSAGEID", 50);
							  break;
		case LINEERR_INVALPARAM:
							  strncpy (CallState, "LINEERR_INVALPARAM", 50);
							  break;
		case LINEERR_INVALPARKID:
							  strncpy (CallState, "LINEERR_INVALPARKID", 50);
							  break;
		case LINEERR_INVALPARKMODE:
							  strncpy (CallState, "LINEERR_INVALPARKMODE", 50);
							  break;
		case LINEERR_INVALPOINTER:
							  strncpy (CallState, "LINEERR_INVALPOINTER", 50);
							  break;
		case LINEERR_INVALPRIVSELECT:
							  strncpy (CallState, "LINEERR_INVALPRIVSELECT", 50);
							  break;
		case LINEERR_INVALRATE:
							  strncpy (CallState, "LINEERR_INVALRATE", 50);
							  break;
		case LINEERR_INVALREQUESTMODE:
							  strncpy (CallState, "LINEERR_INVALREQUESTMODE", 50);
							  break;
		case LINEERR_INVALTERMINALID:
							  strncpy (CallState, "LINEERR_INVALTERMINALID", 50);
							  break;
		case LINEERR_INVALTERMINALMODE:
							  strncpy (CallState, "LINEERR_INVALTERMINALMODE", 50);
							  break;
		case LINEERR_INVALTIMEOUT:
							  strncpy (CallState, "LINEERR_INVALTIMEOUT", 50);
							  break;
		case LINEERR_INVALTONE:
							  strncpy (CallState, "LINEERR_INVALTONE", 50);
							  break;
		case LINEERR_INVALTONELIST:
							  strncpy (CallState, "LINEERR_INVALTONELIST", 50);
							  break;
		case LINEERR_INVALTONEMODE:
							  strncpy (CallState, "LINEERR_INVALTONEMODE", 50);
							  break;
		case LINEERR_INVALTRANSFERMODE:
							  strncpy (CallState, "LINEERR_INVALTRANSFERMODE", 50);
							  break;
		case LINEERR_LINEMAPPERFAILED:
							  strncpy (CallState, "LINEERR_LINEMAPPERFAILED", 50);
							  break;
		case LINEERR_NOCONFERENCE:
							  strncpy (CallState, "LINEERR_NOCONFERENCE", 50);
							  break;
		case LINEERR_NODEVICE:
							  strncpy (CallState, "LINEERR_NODEVICE", 50);
							  break;
		case LINEERR_NODRIVER:
							  strncpy (CallState, "LINEERR_NODRIVER", 50);
							  break;
		case LINEERR_NOMEM:
							  strncpy (CallState, "LINEERR_NOMEM", 50);
							  break;
		case LINEERR_NOREQUEST:
							  strncpy (CallState, "LINEERR_NOREQUEST", 50);
							  break;
		case LINEERR_NOTOWNER:
							  strncpy (CallState, "LINEERR_NOTOWNER", 50);
							  break;
		case LINEERR_NOTREGISTERED:
							  strncpy (CallState, "LINEERR_NOTREGISTERED", 50);
							  break;
		case LINEERR_OPERATIONFAILED:
							  strncpy (CallState, "LINEERR_OPERATIONFAILED", 50);
							  break;
		case LINEERR_OPERATIONUNAVAIL:
							  strncpy (CallState, "LINEERR_OPERATIONUNAVAIL", 50);
							  break;
		case LINEERR_RATEUNAVAIL:
							  strncpy (CallState, "LINEERR_RATEUNAVAIL", 50);
							  break;
		case LINEERR_RESOURCEUNAVAIL:
							  strncpy (CallState, "LINEERR_RESOURCEUNAVAIL", 50);
							  break;
		case LINEERR_REQUESTOVERRUN:
							  strncpy (CallState, "LINEERR_REQUESTOVERRUN", 50);
							  break;
		case LINEERR_STRUCTURETOOSMALL:
							  strncpy (CallState, "LINEERR_STRUCTURETOOSMALL", 50);
							  break;
		case LINEERR_TARGETNOTFOUND:
							  strncpy (CallState, "LINEERR_TARGETNOTFOUND", 50);
							  break;
		case LINEERR_TARGETSELF:
							  strncpy (CallState, "LINEERR_TARGETSELF", 50);
							  break;
		case LINEERR_UNINITIALIZED:
							  strncpy (CallState, "LINEERR_UNINITIALIZED", 50);
							  break;
		case LINEERR_USERUSERINFOTOOBIG:
							  strncpy (CallState, "LINEERR_USERUSERINFOTOOBIG", 50);
							  break;
		case LINEERR_ADDRESSBLOCKED:
							  strncpy (CallState, "LINEERR_ADDRESSBLOCKED", 50);
							  break;
		case LINEERR_BILLINGREJECTED:
							  strncpy (CallState, "LINEERR_BILLINGREJECTED", 50);
							  break;
		case LINEERR_INVALFEATURE:
							  strncpy (CallState, "LINEERR_INVALFEATURE", 50);
							  break;
		case LINEERR_NOMULTIPLEINSTANCE:
							  strncpy (CallState, "LINEERR_NOMULTIPLEINSTANCE", 50);
							  break;
      // Unhandled errors fail.
        default:
            break;
    }
    return bRet;
}

//
//  FUNCTION: long WaitForReply(long)
//
//  PURPOSE: Resynchronize by waiting for a LINE_REPLY 
//
//  PARAMETERS:
//    lRequestID - The asynchronous request ID that we're
//                 on a LINE_REPLY for.
//
//  RETURN VALUE:
//    - 0 if LINE_REPLY responded with a success.
//    - LINEERR constant if LINE_REPLY responded with a LINEERR
//    - 1 if the line was shut down before LINE_REPLY is received.
//
//  COMMENTS:
//
//    This function allows us to resynchronize an asynchronous
//    TAPI line call by waiting for the LINE_REPLY message.  It
//    waits until a LINE_REPLY is received or the line is shut down.
//
//    Note that this could cause re-entrancy problems as
//    well as mess with any message preprocessing that might
//    occur on this thread (such as TranslateAccelerator).
//
//    This function should to be called from the thread that did
//    lineInitialize, or the PeekMessage is on the wrong thread
//    and the synchronization is not guaranteed to work.  Also note
//    that if another PeekMessage loop is entered while waiting,
//    this could also cause synchronization problems.
//
//    One more note.  This function can potentially be re-entered
//    if the call is dropped for any reason while waiting.  If this
//    happens, just drop out and assume the wait has been canceled.  
//    This is signaled by setting bReentered to FALSE when the function 
//    is entered and TRUE when it is left.  If bReentered is ever TRUE 
//    during the function, then the function was re-entered.
//

void WaitOnPreviousRequest (LPCONNECTION	pNet)
{

	MSG msg; 

	if (pNet->RequestWait)
		return;

	pNet->RequestWait = TRUE;
	while(pNet->idRequest != 0) 	
		{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
			{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			}
		}
	pNet->RequestWait = FALSE;
}

long WaitForReply (LPCONNECTION	pNet)
{
	if (pNet->ReplyWait)
		return (pNet->idRequest);

	pNet->ReplyWait = TRUE;
    if (pNet->idRequest > 0)
		{
        MSG msg; 

        pNet->ReplyReceived = FALSE;

        // TODO: shorten  timeout  via callparams struct.dwNoAnswerTimeout member
		while(!pNet->ReplyReceived) 	
			{
		    if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
				{
                TranslateMessage(&msg);
                DispatchMessage(&msg);
				}

			}
		pNet->idRequest = 0;
		return (pNet->AsyncReply);
		}

	pNet->ReplyWait = FALSE;
    return (pNet->idRequest);
}

//  FUNCTION: BOOL HangupCall()
//
//  PURPOSE: Hangup the call in progress if it exists.
BOOL HangupCall(LPCONNECTION	pNet)
{    
    LPLINECALLSTATUS pLineCallStatus = NULL;
    long lReturn;

    // If there is a call in progress, drop and deallocate it.
    if (pNet->hCall)
    {
		// Notify waiting application that call has been disconnected
		PostMessage (pNet->hWnd, pNet->ConnectNotifyMsg, CALLDISCONNECT, 0);

        pLineCallStatus = (LPLINECALLSTATUS)malloc(sizeof(LINECALLSTATUS));

        if (!pLineCallStatus)
        {
            ShutdownTAPI();
            return FALSE;
        }

        lReturn = lineGetCallStatus(pNet->hCall, pLineCallStatus);

        // Only drop the call when the line is not IDLE.
        if (!((pLineCallStatus -> dwCallState) & LINECALLSTATE_IDLE))
        {
			if (pNet->idRequest != 0)
				WaitOnPreviousRequest(pNet);
			pNet->ReplyReceived = FALSE;
			pNet->idRequest = lineDrop(pNet->hCall, NULL, 0);
            WaitForReply(pNet);
        }

        // The call is now idle.  Deallocate it!
        do
        {
            lReturn = lineDeallocateCall(pNet->hCall);
            if (HandleLineErr(lReturn))
                continue;
            else
                break;
        }
        while(lReturn != SUCCESS);

    }

    // if we have a line open, close it.
    if (pNet->hLine)
	{
       lReturn = lineClose(pNet->hLine);
       HandleLineErr(lReturn);
    }

    // Clean up.
    pNet->hCall = NULL;
    pNet->hLine = NULL;

    // Need to free buffer returned from lineGetCallStatus
    if (pLineCallStatus)
        free(pLineCallStatus);  
        
	return (TRUE);
}

//  FUNCTION: BOOL ShutdownTAPI()
//
//  PURPOSE: Shuts down all use of TAPI
BOOL ShutdownTAPI()
{
    long lReturn;
	int	i;

    // If we aren't initialized, then Shutdown is unnecessary.
    if (TapiUsageHandle == NULL)
        return TRUE;

  	for (i=0; i<MAX_CONNECTS; i++)
		HangupCall(&(pGlobal->net[i]));
  
    do
    {
        lReturn = lineShutdown(TapiUsageHandle);
        HandleLineErr(lReturn);
    }
    while(lReturn != SUCCESS);


	TapiUsageHandle = NULL;
    return TRUE;
}

// This function is utilized in conjunction with the TAPI, (Telephony API), interface to allow the application 
// to obtain the number of TAPI devices installed on the local machine.
_declspec(dllexport) DWORD PASCAL FAR NumberOfLineDevices()
{
	return (NumbrLineDevices);
}

//  FUNCTION: LPVOID CheckAndReAllocBuffer(LPVOID, size_t, LPCSTR)
//
//  PURPOSE: Checks and ReAllocates a buffer if necessary.
LPVOID CheckAndReAllocBuffer(LPVOID lpBuffer, size_t sizeBufferMinimum)
{
    size_t sizeBuffer;

    if (lpBuffer == NULL)  // Allocate the buffer if necessary. 
    {
        sizeBuffer = sizeBufferMinimum;
        lpBuffer = (LPVOID) LocalAlloc (LPTR, sizeBuffer);
            
        if (lpBuffer == NULL)
            return NULL;
    }
    else // If the structure already exists, make sure its good.
    {
        sizeBuffer = LocalSize((HLOCAL) lpBuffer);

        if (sizeBuffer == 0) // Bad pointer?
            return NULL;

        // Was the buffer big enough for the structure?
        if (sizeBuffer < sizeBufferMinimum)
        {
            LocalFree(lpBuffer);
            return CheckAndReAllocBuffer(NULL, sizeBufferMinimum);
        }
    }

    memset(lpBuffer, 0, sizeBuffer);       
    ((LPVARSTRING) lpBuffer ) -> dwTotalSize = (DWORD) sizeBuffer;
    return lpBuffer;
}

//  FUNCTION: MylineGetDevCaps(LPLINEDEVCAPS, DWORD , DWORD)
//
//  PURPOSE: Gets a LINEDEVCAPS structure for the specified line.
//
//  COMMENTS:
//
//    This function is a wrapper around lineGetDevCaps to make it easy
//    to handle the variable sized structure and any errors received.
//    
//    The returned structure has been allocated with LocalAlloc,
//    so LocalFree has to be called on it when you're finished with it,
//    or there will be a memory leak.
//
//    Similarly, if a lpLineDevCaps structure is passed in, it *must*
//    have been allocated with LocalAlloc and it could potentially be 
//    LocalFree()d.
//
//    If lpLineDevCaps == NULL, then a new structure is allocated.  It is
//    normal to pass in NULL for this parameter unless you want to use a 
//    lpLineDevCaps that has been returned by a previous I_lineGetDevCaps
//    call.
LPLINEDEVCAPS MylineGetDevCaps(LPLINEDEVCAPS lpLineDevCaps, DWORD dwDeviceID, DWORD dwAPIVersion)
{
    // Allocate enough space for the structure plus 1024.
    size_t sizeofLineDevCaps = sizeof(LINEDEVCAPS) + 1024;
    long lReturn;
    
    // Continue this loop until the structure is big enough.
    while(TRUE)
    {
        // Make sure the buffer exists, is valid and big enough.
        lpLineDevCaps = 
            (LPLINEDEVCAPS) CheckAndReAllocBuffer(
                (LPVOID) lpLineDevCaps, // Pointer to existing buffer, if any
                sizeofLineDevCaps);      // Minimum size the buffer should be

        if (lpLineDevCaps == NULL)
            return NULL;

        // Make the call to fill the structure.
        do
        {            
            lReturn = lineGetDevCaps(TapiUsageHandle, 
                    dwDeviceID, dwAPIVersion, 0, lpLineDevCaps);

            if (HandleLineErr(lReturn))
                continue;
            else
            {
                LocalFree(lpLineDevCaps);
                return NULL;
            }
        }
        while (lReturn != SUCCESS);

        // If the buffer was big enough, then succeed.
        if ((lpLineDevCaps -> dwNeededSize) <= (lpLineDevCaps -> dwTotalSize))
            return lpLineDevCaps;

        // Buffer wasn't big enough.  Make it bigger and try again.
        sizeofLineDevCaps = lpLineDevCaps -> dwNeededSize;
    }
}

// This function returns the name associated with a designated TAPI line device.  
// It may be used to fill a dialog box representing the possible selections for a modem connection.
_declspec(dllexport) BOOL PASCAL FAR GetLineDeviceName(DWORD dwDeviceID, LPBYTE buf, WORD BufLen)
{
    char szLineUnavail[] = "Line Unavailable";
    char szLineUnnamed[] = "Line Unnamed";
    char szLineNameEmpty[] = "Line Name is Empty";
    LPSTR lpszLineName;
    long lReturn;
    LINEEXTENSIONID lineExtID;
    BOOL bDone = FALSE;
    LPLINEDEVCAPS lpLineDevCaps = NULL;

	if (dwDeviceID >= NumbrLineDevices)
		return (FALSE);

    lReturn = lineNegotiateAPIVersion(TapiUsageHandle, dwDeviceID, 
            EARLY_TAPI_VERSION, SAMPLE_TAPI_VERSION,
            &m_dwAPIVersion, &lineExtID);

    lpLineDevCaps = MylineGetDevCaps(lpLineDevCaps, dwDeviceID, m_dwAPIVersion);

	if (lpLineDevCaps == NULL)
		return (FALSE);

	if ((lpLineDevCaps -> dwLineNameSize) &&
		(lpLineDevCaps -> dwLineNameOffset) &&
		(lpLineDevCaps -> dwStringFormat == STRINGFORMAT_ASCII))
			// This is the name of the device.
			lpszLineName = ((char *) lpLineDevCaps) + 
								lpLineDevCaps -> dwLineNameOffset;
	else  // DevCaps doesn't have a valid line name.  Unnamed.
			lpszLineName = szLineUnnamed;

	strncpy (buf, lpszLineName, BufLen);
	LocalFree(lpLineDevCaps);

	return (TRUE);
}

// Allows the application to obtain a text desciption of the state of a DialCall operation.  
// This data may be used to display a status box representing the call attempt, 
// (i.e. "Dial Tone", "Dialing", "Connect", etc.)
_declspec(dllexport) void PASCAL FAR GetCallState (char *buf, int buflen)
{
	strncpy (buf, CallState, buflen);
}

//
//  FUNCTION: MylineGetAddressCaps(LPLINEADDRESSCAPS, ..)
//
//  PURPOSE: Return a LINEADDRESSCAPS structure for the specified line.
//
LPLINEADDRESSCAPS MylineGetAddressCaps (
    LPLINEADDRESSCAPS lpLineAddressCaps,
    DWORD dwDeviceID, DWORD dwAddressID,
    DWORD dwAPIVersion, DWORD dwExtVersion)
{
    size_t sizeofLineAddressCaps = sizeof(LINEADDRESSCAPS) + 1024;
    long lReturn;
    
    // Continue this loop until the structure is big enough.
    while(TRUE)
    {
        // Make sure the buffer exists, is valid and big enough.
        lpLineAddressCaps = 
            (LPLINEADDRESSCAPS) CheckAndReAllocBuffer(
                (LPVOID) lpLineAddressCaps,
                sizeofLineAddressCaps);

        if (lpLineAddressCaps == NULL)
            return NULL;
            
        // Make the call to fill the structure.
        do
        {
            lReturn = 
                lineGetAddressCaps(TapiUsageHandle,
                    dwDeviceID, dwAddressID, dwAPIVersion, dwExtVersion,
                    lpLineAddressCaps);

            if (HandleLineErr(lReturn))
                continue;
            else
            {
                OutputDebugString("lineGetAddressCaps unhandled error\n");
                LocalFree(lpLineAddressCaps);
                return NULL;
            }
        }
        while (lReturn != SUCCESS);

        // If the buffer was big enough, then succeed.
        if ((lpLineAddressCaps -> dwNeededSize) <= 
            (lpLineAddressCaps -> dwTotalSize))
        {
            return lpLineAddressCaps;
        }

        // Buffer wasn't big enough.  Make it bigger and try again.
        sizeofLineAddressCaps = lpLineAddressCaps -> dwNeededSize;
    }
}

//
//  FUNCTION: CreateCallParams(LPLINECALLPARAMS, LPCSTR)
//
//  PURPOSE: Allocates and fills a LINECALLPARAMS structure
//
//
LPLINECALLPARAMS CreateCallParams (
    LPLINECALLPARAMS lpCallParams, LPCSTR lpszDisplayableAddress)
{
    size_t sizeDisplayableAddress;

    if (lpszDisplayableAddress == NULL)
        lpszDisplayableAddress = "";
        
    sizeDisplayableAddress = strlen(lpszDisplayableAddress) + 1;
                          
    lpCallParams = (LPLINECALLPARAMS) CheckAndReAllocBuffer(
        (LPVOID) lpCallParams, 
        sizeof(LINECALLPARAMS) + sizeDisplayableAddress);

    if (lpCallParams == NULL)
        return NULL;

    // This is where we configure the line.
    lpCallParams -> dwBearerMode = LINEBEARERMODE_VOICE;
	lpCallParams -> dwMinRate = 0;
	lpCallParams -> dwMaxRate = 0;
    lpCallParams -> dwMediaMode  = LINEMEDIAMODE_DATAMODEM;

    // This specifies that we want to use only IDLE calls and
    // don't want to cut into a call that might not be IDLE (ie, in use).
    lpCallParams -> dwCallParamFlags = LINECALLPARAMFLAGS_IDLE;
                                    
    // if there are multiple addresses on line, use first anyway.
    // It will take a more complex application than a simple tty app
    // to use multiple addresses on a line anyway.
    lpCallParams -> dwAddressMode = LINEADDRESSMODE_ADDRESSID;

    // Address we are dialing.
    lpCallParams -> dwDisplayableAddressOffset = sizeof(LINECALLPARAMS);
    lpCallParams -> dwDisplayableAddressSize = sizeDisplayableAddress;
    strcpy((LPSTR)lpCallParams + sizeof(LINECALLPARAMS),
           lpszDisplayableAddress);

    return lpCallParams;
}

// DialCall attempts to establish a connection by dialing the phone number specified in pDialParams.  
// If successful, the application will receive a CONNECT notification message containing the connection handle 
// which must then be passed to HookRspNotification to set up modbus message notification messages.  
// The HANDLE value returned from this function represents a handle to the call attempt, NOT to the actual 
// connection.  (Refer to code sample contained with the modbusm source code for clarification.)
_declspec(dllexport) HANDLE PASCAL FAR DialCall (DWORD dwDeviceID, LPDIALPARAMS pDialParams)
{
    long lReturn;
    LPLINEDEVCAPS lpLineDevCaps = NULL;
    LPLINECALLPARAMS  lpCallParams = NULL;
    LPLINEADDRESSCAPS lpAddressCaps = NULL;
	LPCONNECTION	pNet=NULL;
    BOOL bFirstDial = TRUE;
	BOOL GoodDial;
	HANDLE	idConnect;

	GoodDial = FALSE;

	if (dwDeviceID > NumbrLineDevices)
		{
        strncpy (CallState, "Invalid Line Device Id", 50);
        return (INVALID_HANDLE_VALUE);
		}

	idConnect = AllocateNetResources (pDialParams->Protocol, pDialParams->Timeout);

	pNet = FindConnect (idConnect);
	if (pNet == NULL)
		{
        strncpy (CallState, "No Connections available", 50);
        return (INVALID_HANDLE_VALUE);
		}

	pNet->hWnd = pDialParams->hWnd;
	pNet->ConnectNotifyMsg = pDialParams->ConnectMsg;

    // Get the line to use
    lpLineDevCaps = MylineGetDevCaps(lpLineDevCaps, dwDeviceID, m_dwAPIVersion);

	if (lpLineDevCaps == NULL)
		{
        strncpy (CallState, "TAPI Failure to obtain LineDeviceCaps.", 50);
        goto errExit;
		}

    // Does this line have the capability to make calls?
    if (!(lpLineDevCaps->dwLineFeatures & LINEFEATURE_MAKECALL))
		{
        strncpy (CallState, "The selected line doesn't support MAKECALL capabilities", 50);
             goto errExit;
		}

    // Open the Line for an outgoing call.
    do
    {
        lReturn = lineOpen(TapiUsageHandle, dwDeviceID, &(pNet->hLine),
            m_dwAPIVersion, 0, 0,
            LINECALLPRIVILEGE_NONE, 0, 0);

        if((lReturn == LINEERR_ALLOCATED)||(lReturn == LINEERR_RESOURCEUNAVAIL))
			{
            HangupCall(pNet);
            strncpy (CallState, "Line is already in use", 50);
             goto errExit;
			}

        if (HandleLineErr(lReturn))
            continue;
        else
			{
            strncpy (CallState, "Unable to use line", 50);
            goto errExit;
			}
    }
    while(lReturn != SUCCESS);

    // Start dialing the number
                               
    // Get the capabilities for the line device we're going to use.
    lpAddressCaps = MylineGetAddressCaps(lpAddressCaps,
        dwDeviceID, 0, m_dwAPIVersion, 0);
    if (lpAddressCaps == NULL)
		{
        strncpy (CallState, "TAPI Failure to obtain AddressCaps.", 50);
        goto errExit;
		}

    // Setup our CallParams.
    lpCallParams = CreateCallParams (lpCallParams, pDialParams->PhoneNumber);
    if (lpCallParams == NULL)
		{
        strncpy (CallState, "TAPI Failure to obtain CallParams.", 50);
        goto errExit;
		}

    do
    {                   
        if (bFirstDial)
            pNet->idRequest = lineMakeCall(pNet->hLine, &(pNet->hCall), pDialParams->PhoneNumber, 
                        0, lpCallParams);
        else
            pNet->idRequest = lineDial(pNet->hCall, pDialParams->PhoneNumber, 0);
		lReturn = WaitForReply (pNet);
        if (lReturn == WAITERR_WAITABORTED)
			{
            strncpy (CallState, "While Dialing, WaitForReply aborted.", 50);
             goto errExit;
			}
            
        if (HandleLineErr(lReturn))
            continue;
        else
            goto errExit;
    }
    while (lReturn != SUCCESS);
        
    bFirstDial = FALSE;
	
	GoodDial = TRUE;

  errExit:
	if (lpLineDevCaps)
		LocalFree(lpLineDevCaps);
    if (lpCallParams)
        LocalFree(lpCallParams);
    if (lpAddressCaps)
        LocalFree(lpAddressCaps);

	if (GoodDial)
		return (idConnect);
	else
		{
		ReleaseNetResources(pNet);
		return (INVALID_HANDLE_VALUE);
		}
}

// Provides the ability to cancel a requested DialCall operation.
_declspec(dllexport) void PASCAL FAR AbortTheCall (HANDLE idConnect)
{
	LPCONNECTION	pNet;

	pNet = FindConnect(idConnect);
	if (pNet == NULL)
		return;

	HangupCall (pNet);
	ReleaseNetResources (pNet);

}
