/* ------------------------------------------------------------------
	MODBUS.C
		Win32 COMM routines to accept standard MODBUS message
		requests and return response data to calling application.
--------------------------------------------------------------------*/

#define TAPI_CURRENT_VERSION 0x00010004

#include <winsock2.h>
#include <windows.h>

#include <memory.h>

#include <tapi.h>
#include <time.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <string.h>
//The following .h requires the installation of Microsoft Plateform SDK 
//#include <IPHlpApi.h>
//#include <icmpapi.h>
//#include <process.h>

#include "modbusm.h"
#include "mbglobals.h"
#include "ExtTimeFormat.h"
#include "list.h" 


DWORD	WINAPI	Control_Proc (LPVOID InPtr);	//Prototype for Send/Rcv Thread


extern void CALLBACK TapiCallbackFunction (DWORD dwDevice, DWORD dwMsg, DWORD dwCallbackInstance,
											DWORD dwParam1, DWORD dwParam2, DWORD dwParam3);

void ReleaseNetResources (LPCONNECTION	pNet);
BOOL AllocateControlThread (LPCONNECTION pNet, LPSERIALCONFIG	pCfg);
BOOL HangupCall(LPCONNECTION	pNet);

int CloseTCPConnection(LPCONNECTION	pNet);
void DebugPrint(char *msg,LPCONNECTION pNet,int debugLevel);
void PingAddr( void * pParam );
void SaveBufferData(DWORD cleanRecvBufCount, char *cleanRecvBuf,LPCONNECTION pNet, char *sBufferPath, int codePart, int trnCount);

HANDLE 		ProcessHandle;
HLINEAPP	TapiUsageHandle;
DWORD		NumbrLineDevices;
HANDLE		hMapFile;
LPVOID		lpMapAddress;

struct Node	*ThreadList;
struct GlobalData *pGlobal;
int m_DebugLevel;

int			m_ShortTimeOut;		// Time out for 3 first bytes when reading from device
int			m_OpenConnectionTimeout; //in miliseconds - we double it by 1000 to get microseconds.
BOOL	m_CleanBuffBeforeWriting;		// shouldwe clean the tcp line before we write msg to controllers
BOOL	m_DoSaveTrnInverseCode;		// should we save as file the trn when we get error as inverse code (c1 instead of 81 or 81 instead of c1)

#define DEBUGLEVEL_DETAILED		3	
#define DEBUGLEVEL_FUNCTIONAL	1
#define DEBUGLEVEL_NONE			0

/*
typedef DWORD (WINAPI *TIcmpSendEcho2)(
  HANDLE IcmpHandle, 
  IPAddr DestinationAddress,
  LPVOID RequestData,
  WORD RequestSize,
  PIP_OPTION_INFORMATION RequestOptions,
  LPVOID ReplyBuffer,
  DWORD ReplySize,
  DWORD Timeout
);

typedef HANDLE (WINAPI *TIcmpCreateFile)(void);
typedef BOOL (WINAPI *TIcmpCloseHandle)( HANDLE IcmpHandle);


TIcmpSendEcho2 m_IcmpSendEcho2;
TIcmpCreateFile m_IcmpCreateFile;
TIcmpCloseHandle m_IcmpCloseHandle;

TIcmpSendEcho2 m_IcmpSendEcho2;
TIcmpCreateFile m_IcmpCreateFile;
TIcmpCloseHandle m_IcmpCloseHandle;

*/

/////////////////////////////////////////////////////////////////////////
//
//	void mLog(char *buf, DWORD length, DWORD expected, int error, int retries, int IdConnect, char *comment) 
//
//	function debug: Foe each network we are writing in a different file the information.
//	The idConnect is receive from each thread.
//
/////////////////////////////////////////////////////////////////////////

void mLog(char *buf, DWORD length, DWORD expected, int error, int retries, int IdConnect, char *comment) 
{
    FILE *file;
	SYSTEMTIME SystemTime;
	char strDest[32];
	char strMsg[2*MAX_BUF_SIZE];
	DWORD i = 0;
	unsigned char c;
	char sFileName[50];
	
	GetLocalTime (&SystemTime);

	sprintf(sFileName,"Modbus_Log_connect%u_%2.2d%2.2d%2.2d%2.2d.log",IdConnect,SystemTime.wYear,SystemTime.wMonth,SystemTime.wDay,SystemTime.wHour);
	
	file = fopen(sFileName,"a");
	
	strcpy(strMsg, "Msg: ");

	while (i < length) 
	{
		c = buf[i++];
		sprintf(strDest,"%2.2X ",c);
		strcat(strMsg,strDest);
	}
	// Create system time structure
	//GetSystemTime(&SystemTime);
	//GetLocalTime (&SystemTime);
	sprintf(strDest,"%dh%d %ds %dmilli",SystemTime.wHour,SystemTime.wMinute, SystemTime.wSecond, SystemTime.wMilliseconds);
	if (length > 0 )
		fprintf(file, "%s : %s length=%u, expected=%u, error=%u, retries=%u, idConnect=%u  %s\n", strDest, comment, length, expected, error, retries, IdConnect, strMsg);
	else
		fprintf(file, "%s : %s length=%u, expected=%u, error=%u, retries=%u, idConnect=%u\n", strDest, comment, length, expected, error, retries, IdConnect);

	fclose(file);
}


/////////////////////////////////////////////////////////////////////////
//
//	void writeTrnToFile(char *buf, DWORD length, DWORD expected, int error, int retries, int IdConnect, char *comment) 
//
//	
//	writes trns to file
//
/////////////////////////////////////////////////////////////////////////

void writeTrnToFile(char *buf, DWORD length, int trnCount, int IdConnect,char *bufPath, int codePart) 
{
    FILE *file;
	SYSTEMTIME SystemTime;
	char strDestOrg[32];
	char strDest[32];
	char strMsg[2*MAX_BUF_SIZE];
	DWORD i = 0;
	unsigned char c;
	char sFileName[MAX_PATH];
	char sFileNewname[MAX_PATH];
	char sFileNewPath[MAX_PATH];
	int result;

	LPSECURITY_ATTRIBUTES attr;
	attr = NULL;

	//put it in different dir

	memset(strMsg,0x00,2*MAX_BUF_SIZE);
	memset(strDestOrg,0x00,32);
	memset(strDest,0x00,32);
	memset(sFileName,0x00,MAX_PATH);
	memset(sFileNewname,0x00,MAX_PATH);
	memset(sFileNewPath,0x00,MAX_PATH);

	GetLocalTime (&SystemTime);
	sprintf(strDestOrg,"%d%2.2d%2.2d%2.2d%2.2d%2.2d",SystemTime.wYear,SystemTime.wMonth,SystemTime.wDay,SystemTime.wHour,SystemTime.wMinute, SystemTime.wSecond);
	
	sprintf(sFileName,"%s\\ModbusTrns_%s_%u_%u.tmp",bufPath,strDestOrg, trnCount, IdConnect);


	file = fopen(sFileName,"a");
	
	while (i < length) 
	{
		c = buf[i++];
		sprintf(strDest,"%2.2X ",c);
		strcat(strMsg,strDest);
	}
	
	if (length > 0 ){
		fprintf(file, "%s\n",strMsg);
	}

	fclose(file);

	sprintf(sFileNewPath,"%s\\polEvtModbus",bufPath);
	CreateDirectory(sFileNewPath, attr);
	
	sprintf(sFileNewname,"%s\\ModbusTrns_%s_%u_%u_p%u.toDo",sFileNewPath,strDestOrg, trnCount, IdConnect, codePart);
	
	result= rename( sFileName , sFileNewname );
	if ( result != 0 )
		OutputDebugString("DLL Modbus: Error in renaming file from cleaning buffer!");

}


void  myGetCurrentPath(char *buf) 
{     
	char tmpBuffer[MAX_PATH];     
	char tmpChar[1];     
	DWORD tmpCharCount;
	DWORD tmpPos;
	DWORD tmpN;

	memset(tmpBuffer,0x00,MAX_PATH);
	tmpCharCount = GetModuleFileName( NULL, tmpBuffer, MAX_PATH );     
	tmpChar[0]='\\' ;
	tmpPos=0;

	for (tmpN = 0; tmpN < tmpCharCount; tmpN++){
		buf[tmpN]=tmpBuffer[tmpN];
		if (tmpBuffer[tmpN] == tmpChar[0]){
			tmpPos=tmpN;
		}
	}
	
	if(tmpPos > 0) 
	{
		for (tmpN = tmpPos; tmpN < tmpCharCount; tmpN++){
			buf[tmpN]='\0';
		}
	}

} 

void WINAPI MSGCPY(char *str1,char* str2,int length)
{
	int i;
	for(i=0;i<length;i++)
		str1[i]=str2[i];

}


int WINAPI MSGCMPR(char *str1,char* str2,int length)
{
	int i,flag=1;
	for(i=0;i<length;i++)
	{   
		if(str1[i]!=str2[i])
		{
			flag=0;
			break;
		}	  
	}
	if (flag==0)
		return (0);
	else
		return (1);
}

void WINAPI Correct(BYTE *pOut,int length, int bufindex, int jump)
{			
	BYTE  TempOut[MAX_BUF_SIZE];
	int i,j ;
	for(i = 0; i < 4;i++)
		TempOut[i] = pOut[i];

	// Tous les Jump enlever les 0z
	for (i = 0; i <= ( (length*2) / jump ); i++)
	{
		for (j = 0; j < jump;j++)
		{
			TempOut[4+i*(jump-1)+j] = pOut[4+i*jump+j];
		}					
	}
	for(i=0;i<(bufindex - (length*2/jump));i++)
		pOut[i]=TempOut[i];
}

/////////////////////////////////////////////////////////////////////////
//
//	void	Initialize (LPCONNECTION	pNet)
//
//	Called nine times whenever dll first loaded to
//	clear CONNECTION structures
//
/////////////////////////////////////////////////////////////////////////

void	Initialize (LPCONNECTION	pNet)
{
	pNet->DirectConnection = FALSE;
	pNet->InUse = FALSE;
	pNet->idComDev = INVALID_HANDLE_VALUE;
	pNet->ThreadEnable = NULL;
	pNet->ThreadActive = FALSE;
	pNet->hWnd = NULL;
	pNet->Protocol = MODBUS_RTU;
	pNet->TimeOut = 0;
	pNet->TempTimeOut = 0;
	pNet->ShortTimeOut = 0;
	pNet->OpenConnectionTimeout = 0;
	pNet->CleanBuffBeforeWriting = FALSE;
	pNet->dwBaudRate = 0;
	pNet->hMem = NULL;
	pNet->hDebugMem = NULL;
	pNet->DebugIn = 0;
	pNet->hLine = (HLINE)NULL;
	pNet->hCall = (HCALL)NULL;
	pNet->idRequest = 0;
	pNet->RTS_Delay[0] = 0;
	pNet->RTS_Delay[1] = 0;
	pNet->IsClosing = FALSE;
	pNet->ReplyReceived = FALSE;
	pNet->ReplyWait = FALSE;
	pNet->RequestWait = FALSE;
	pNet->SvrSock = INVALID_SOCKET;
	pNet->hTcpBuf = NULL;
	pNet->IsSocket = FALSE;
	pNet->PortNo = 0;
	pNet->iConnectionMode = 0;
	pNet->iConnectionID = 0;
	pNet->SaveBaudeRate = 0;
	pNet->Password.Activated = 0;
	pNet->Password.Byte1 = 0;
	pNet->Password.Byte2 = 0;
	pNet->Password.Byte3 = 0;
	pNet->Password.Byte4 = 0;
}

/////////////////////////////////////////////////////////////////////////
//
//	BOOL WINAPI DllMain (HANDLE hModule, DWORD fdwReason, LPVOID lpReserved)//
//
//	Main entry for DLL
//	Called for each Application Instance, (and each Thread within an App).
//	
//	Allow each Application to control up to four CONNECTS
//
/////////////////////////////////////////////////////////////////////////

BOOL WINAPI DllMain (HANDLE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	int	i;
	BOOL	First;
	long	lReturn;
	//HMODULE hLibrary;

	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
			ProcessHandle = hModule;
			First = FALSE;
			CreateList(&ThreadList);

			TapiUsageHandle = INVALID_HANDLE_VALUE;
			lReturn = lineInitialize (&TapiUsageHandle,
									hModule,
									TapiCallbackFunction,
									"Modbus.dll",
									&NumbrLineDevices);

			/*hMapFile = OpenFileMapping (FILE_MAP_ALL_ACCESS,
								FALSE, "ModbusMapObj");
			
			if (hMapFile == NULL)
			{
				First = TRUE;
				hMapFile = CreateFileMapping ((HANDLE)0xFFFFFFFF,
								NULL, PAGE_READWRITE, 0, MAX_CONNECTS * sizeof(CONNECTION),
								"ModbusMapObj");
			}
			lpMapAddress = MapViewOfFile (hMapFile,
								FILE_MAP_ALL_ACCESS,
								0,
								0,
								0);
			*/
			First = TRUE;

			if (lpMapAddress != NULL)
				pGlobal = lpMapAddress;
			else
			{
				pGlobal = &DefaultGlobals;
				//return (FALSE);
			}
			
			if (First)
			{
				pGlobal->InstanceCtr = 1;
				for (i=0; i<MAX_CONNECTS; i++)
					Initialize(&(pGlobal->net[i]));
			}
			else
				++pGlobal->InstanceCtr;

			wVersionRequested = MAKEWORD( 2, 2 );
 
			err = WSAStartup( wVersionRequested, &wsaData );
			
			if ( err != 0 ) 
			{
				return (FALSE);
			}
 
			/* Confirm that the WinSock DLL supports 2.2.*/
			/* Note that if the DLL supports versions greater    */
			/* than 2.2 in addition to 2.2, it will still return */
			/* 2.2 in wVersion since that is the version we      */
			/* requested.                                        */
 
			if ( LOBYTE( wsaData.wVersion ) != 2 ||
				HIBYTE( wsaData.wVersion ) != 2 ) 
			{
				/* Tell the user that we could not find a usable */
				/* WinSock DLL.                                  */
				WSACleanup( );
			}
			
			OutputDebugString("Loading library:");
			/*
			hLibrary=LoadLibrary("iphlpapi.dll");
			m_IcmpSendEcho2=(TIcmpSendEcho2)GetProcAddress(hLibrary,"IcmpSendEcho");
			if (m_IcmpSendEcho2==NULL)
			{
				hLibrary=LoadLibrary("icmp.dll");
				m_IcmpSendEcho2=(TIcmpSendEcho2)GetProcAddress(hLibrary,"IcmpSendEcho");
				m_IcmpCreateFile=(TIcmpCreateFile)GetProcAddress(hLibrary,"IcmpCreateFile");
				m_IcmpCloseHandle=(TIcmpCloseHandle)GetProcAddress(hLibrary,"IcmpCloseHandle");
			
				OutputDebugString("icmp\n");
			}
			else
			{
				m_IcmpCreateFile=(TIcmpCreateFile)GetProcAddress(hLibrary,"IcmpCreateFile");
				m_IcmpCloseHandle=(TIcmpCloseHandle)GetProcAddress(hLibrary,"IcmpCloseHandle");
				OutputDebugString("iphlpapi\n");
			}
			*/
	
			m_DebugLevel = DEBUGLEVEL_NONE;
			break;
		case DLL_THREAD_ATTACH:
									break;
		case DLL_THREAD_DETACH:
									break;
		case DLL_PROCESS_DETACH:
			if (TapiUsageHandle != INVALID_HANDLE_VALUE)
				lineShutdown(TapiUsageHandle);
			--pGlobal->InstanceCtr;
			if (pGlobal->InstanceCtr <= 0)
			{
				for (i=0; i<MAX_CONNECTS; i++)
				{
					if (pGlobal->net[i].idComDev != INVALID_HANDLE_VALUE)
						CloseConnection((HANDLE)(i+1));
					if (pGlobal->net[i].ThreadEnable != NULL)
						CloseHandle(pGlobal->net[i].ThreadEnable);
				}
				if (lpMapAddress != NULL)
					UnmapViewOfFile (lpMapAddress);
				

			}
			if (hMapFile != NULL)
				CloseHandle(hMapFile);
			WSACleanup( );
			if (ThreadList)
				free(ThreadList);

			break;
	}
	return (TRUE);
}

/////////////////////////////////////////////////////////////////////////
//
//	LPCONNECTION FindConnect (HANDLE id)
//
//	Correlates connection handle (1 - MAX_CONNECTS) to appropriate CONNECT Struct
//
/////////////////////////////////////////////////////////////////////////

LPCONNECTION FindConnect (HANDLE id)
{
	int	i;
	LPCONNECTION	pNet;

	if (((int)id < 1) || ((int)id > MAX_CONNECTS))
		return (NULL);

	i = (int)id - 1;
	pNet = &(pGlobal->net[i]);
	if (!pNet->InUse)
		return (NULL);

	return (pNet);
}

/////////////////////////////////////////////////////////////////////////
//
//	HANDLE AllocateNetResources (WORD Protocol, int TimeOut)
//
//	Finds an empty slot in the CONNECT Structure Array
//	and allocates memory for data buffers 
//
//	If successful, returns HANDLE to the connection
//
/////////////////////////////////////////////////////////////////////////

HANDLE AllocateNetResources (WORD Protocol, int TimeOut)
{
	int	idx;
	LPCONNECTION	pNet;

	// Loop through connection, finding which was is free
	for (idx=0; idx < MAX_CONNECTS; idx++)
	{	
		if (!pGlobal->net[idx].InUse)
			break;
	}

	if (idx >= MAX_CONNECTS)
		return (INVALID_HANDLE_VALUE);		// No connections available

	pNet = &(pGlobal->net[idx]);
	pNet->InUse = TRUE;
	pNet->IsClosing = FALSE;
	
	pNet->hMem = NULL;
	pNet->hDebugMem = NULL;
	pNet->ThreadEnable = NULL;
	pNet->idComDev = INVALID_HANDLE_VALUE;
	pNet->SvrSock = INVALID_SOCKET;
	pNet->hTcpBuf = NULL;
	pNet->TheadID = NULL;

	// allocate & lock the mem
	// (used to contain data points to & from the MODBUS)
	pNet->hMem = GlobalAlloc (GHND, MAX_POINTS*2);
	if (pNet->hMem == NULL)
	{
		ReleaseNetResources(pNet);
		return (INVALID_HANDLE_VALUE);
	}
		
	pNet->pBuf = GlobalLock(pNet->hMem);

	// allocate & lock the mem
	// (used to contain debug strings)
	pNet->hDebugMem = GlobalAlloc (GHND, DEBUG_BUFSIZE*2);
	if (pNet->hDebugMem == NULL)
	{
		ReleaseNetResources(pNet);
		return (INVALID_HANDLE_VALUE);
	}

	pNet->pDebugBuf = GlobalLock(pNet->hDebugMem);

	// Protocol assignment goes with the CONNECT
	pNet->Protocol = Protocol;
	pNet->TimeOut = TimeOut;
	pNet->TempTimeOut = TimeOut;
	pNet->ShortTimeOut = TimeOut / 5;
	pNet->OpenConnectionTimeout = TimeOut*5;
	pNet->CleanBuffBeforeWriting = FALSE;
	pNet->PollInProgress = FALSE;

	pNet->ReplyReceived = FALSE;
	pNet->ReplyWait = FALSE;
	pNet->RequestWait = FALSE;

	pNet->iConnectionMode = 0;
	// password cleaning
	pNet->Password.Activated = 0;
	pNet->Password.Byte1 = 0;
	pNet->Password.Byte2 = 0;
	pNet->Password.Byte3 = 0;
	pNet->Password.Byte4 = 0;

	return ((HANDLE)(idx+1));
}

/////////////////////////////////////////////////////////////////////////
//
//	void ReleaseNetResources (LPCONNECTION pNet)
//
//	Returns allocated memory for the connection,
//	Closes all open handles
//	and marks the Connect struct as available 
//
/////////////////////////////////////////////////////////////////////////

void ReleaseNetResources (LPCONNECTION	pNet)
{
	char strDebug[255];
	if (pNet->hMem != NULL)
		{
		GlobalUnlock(pNet->hMem);
		GlobalFree(pNet->hMem);
		pNet->hMem = NULL;
		}
	if (pNet->hDebugMem != NULL)
		{
		GlobalUnlock(pNet->hDebugMem);
		GlobalFree(pNet->hDebugMem);
		pNet->hDebugMem = NULL;
		}

	
	// Release the event handle called in order to start/stop listetning
	if (pNet->ThreadEnable != NULL)
		{
		CloseHandle (pNet->ThreadEnable);
		pNet->ThreadEnable = NULL;
		}
	if (pNet->idComDev != INVALID_HANDLE_VALUE)
		{
		CloseHandle (pNet->idComDev);
		pNet->idComDev = INVALID_HANDLE_VALUE;
		}
	if (pNet->SvrSock != INVALID_SOCKET)
		{
		closesocket (pNet->SvrSock);
			sprintf(strDebug,"released socket: %d\n",  pNet->SvrSock);

		OutputDebugString(strDebug);
	//		PostMessage (pNet->hTCPMsgWin, WM_ENDTHREAD, 0, 0);
		pNet->SvrSock = INVALID_SOCKET;
		}
	if (pNet->hTcpBuf != NULL)
	{
		GlobalUnlock(pNet->hTcpBuf);
		GlobalFree(pNet->hTcpBuf);
		pNet->hTcpBuf = NULL;
	}

	if (pNet->TheadID != NULL)
	{
		CloseHandle (pNet->TheadID );
	}
	pNet->InUse = FALSE;

}

/////////////////////////////////////////////////////////////////////////
//
//	int	SetBaudRate (LPCONNECTION pNet, DWORD dwBaudRate)
//
//	Sets up the baud rate when the port is already openned. 
//  Remark: On AMD board (not intel) it is possible that we must close the serial port before 
//	modifying the baud rate (problem on the Daniel computer
//	Called by Control_Proc() 
//
/////////////////////////////////////////////////////////////////////////

int	SetBaudRate (LPCONNECTION pNet, DWORD dwBaudRate)
{
	DCB	dcb;
	int iRep;
	
	FillMemory(&dcb, sizeof(dcb), 0);

	iRep = GetCommState (pNet->idComDev, &dcb); // get current DCB

	dcb.BaudRate = dwBaudRate;

	iRep = SetCommState (pNet->idComDev, &dcb);

	#ifdef M_DEBUG
		mLog(NULL, 0, iRep, dwBaudRate, 0, pNet->iConnectionID, "SetBaudRate: Baud rate");
	#endif

	return (iRep);
}

/////////////////////////////////////////////////////////////////////////
//
//	int	SetupConnection (int idx, LPPORTCONFIG pCfg)
//
//	Sets up the DCB & configures the serial port.
//	Called by OpenConnection() 
//
/////////////////////////////////////////////////////////////////////////

int	SetupConnection (LPCONNECTION pNet, LPSERIALCONFIG pCfg)
{
	DCB	dcb;
	int iRep;

	// Get current configuration
	GetCommState (pNet->idComDev, &dcb);

	// Setup baudrate, parity, etc.
	// Protocol dictates number of data bits
	dcb.BaudRate = pCfg->dwBaudRate;
	dcb.ByteSize = pCfg->bDataBits;
	dcb.Parity = pCfg->bParity;
	dcb.StopBits = pCfg->bStopBits;

	// Setup Flow Control
	dcb.fOutxDsrFlow = pCfg->handshake_DTR;
	dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;	// Hold DTR high whenever port is open

	dcb.fOutxCtsFlow = pCfg->handshake_RTS;
	dcb.fRtsControl = RTS_CONTROL_DISABLE;		// Toggle RTS during poll using EscapeCommFunction

	// XON/XOFF Not Used
	dcb.fInX = FALSE;
	dcb.fOutX = FALSE;

	dcb.fBinary = TRUE;
	dcb.fParity = TRUE;
	dcb.fNull = FALSE;
	
	//return TRUE if everything looks cool
	iRep = SetCommState (pNet->idComDev, &dcb);
	
	#ifdef M_DEBUG
		mLog(NULL, 0, iRep, (int)pCfg->dwBaudRate, 0, pNet->iConnectionID, "SetupConnection: Baud rate");
	#endif

	return iRep;
}

/////////////////////////////////////////////////////////////////////////
//	HANDLE OpenConnection(WORD Protocol, struct in_addr SvrIPaddr, int PortNo, int timeout, LPPORTCONFIG	pCfg)
//
//	Called by exported routines ConnectRTU() & ConnectASCII() to
//	open & setup the COMM port. 
//	
//	Allocates memory for MODBUS data points, opens the I/O, and
//	creates the Send/Rcv Thread. 
//  Uses TCP or serial connection
//
//  Returns:
//  INVALID_HANDLE_VALUE (-1)	- If failed to connect to address
//  otherwise returns the Handle of the connection 
/////////////////////////////////////////////////////////////////////////

HANDLE OpenConnection(	WORD Protocol,
						struct in_addr SvrIPaddr, 
						int PortNo, int timeout, 
						LPSERIALCONFIG	pCfg, 
						LPPASSWORDPARAMS pPassword)
{
	char szport[20];
	LPCONNECTION	pNet;
	HANDLE	idConnect;
	struct sockaddr_in	local;
	int one = 1;
	int zero = 0;
	#ifdef M_DEBUG
		LPVOID lpMsgBuf;
	#endif
	int flag = 1;
	ULONG NonBlock;
	char errBuf[255];
	fd_set  fdread;
	fd_set  fdwrite;
	int ret = 0;
	struct timeval selectTimeOut; 
	int res=0;

	if (pCfg != NULL) 
	{
		// check valid entries for port & timout params
		//portNo > 9 transforme en portNo > 81
		if ((PortNo < 1) || (PortNo > MAX_CONNECTS) || (timeout < 50))  
			return (INVALID_HANDLE_VALUE);
	}

	idConnect = AllocateNetResources (Protocol, timeout);
	
	pNet = FindConnect(idConnect);
	DebugPrint("OpenConnection",pNet,DEBUGLEVEL_DETAILED);
	if (pNet == NULL)
		return (INVALID_HANDLE_VALUE);

	pNet->iConnectionID = (int)idConnect;
	// if there no configuration paramteres of the serial connection, we are using sockets
	pNet->ShortTimeOut = m_ShortTimeOut;
	if (pCfg == NULL) 
	{
		pNet->OpenConnectionTimeout = m_OpenConnectionTimeout;
		pNet->CleanBuffBeforeWriting = m_CleanBuffBeforeWriting;

		pNet->IsSocket = TRUE;
		pNet->PortNo = PortNo;
		pNet->dwBaudRate = 4800; // TODO check? ce n'estpas utilise en TCP

		// get a socket
		//PF_INET6 for ipv6
		pNet->SvrSock = socket (PF_INET, SOCK_STREAM, 0);

		//set the non block mode
		NonBlock = 1;
	    if (ioctlsocket(pNet->SvrSock, FIONBIO, &NonBlock) == SOCKET_ERROR)
	    {
		  res=WSAGetLastError();
		  sprintf(errBuf,"ioctlsocket() failed (set to non block mode) with error %d\n",res);
		  DebugPrint(errBuf,pNet,DEBUGLEVEL_DETAILED);
		  ReleaseNetResources(pNet);
		  return (INVALID_HANDLE_VALUE);
	    }



	//	sprintf(strDebug,"Opened socket: %d\n",  pNet->SvrSock);

	//	OutputDebugString(strDebug);

	//	closesocket (pNet->SvrSock);
		//pNet->SvrSock = INVALID_SOCKET;
		if (pNet->SvrSock == INVALID_SOCKET) 
		{
			ReleaseNetResources(pNet);
			return (INVALID_HANDLE_VALUE);
		}

		setsockopt(pNet->SvrSock, SOL_SOCKET, SO_KEEPALIVE, (char *) &one, sizeof(one));
		setsockopt(pNet->SvrSock, SOL_SOCKET, SO_RCVBUF, (char *) &zero, sizeof(zero));
		setsockopt(pNet->SvrSock, SOL_SOCKET, SO_SNDBUF, (char *) &zero, sizeof(zero));
		//disable the Nagle algorithm
		setsockopt(pNet->SvrSock, IPPROTO_TCP, TCP_NODELAY, (char *) &flag,sizeof(int));
		
		local.sin_family = AF_INET;
		local.sin_port = 0;
		local.sin_addr.s_net = 0;
		local.sin_addr.s_host = 0;
		local.sin_addr.s_lh = 0;
		local.sin_addr.s_impno = 0;
	
		// bind socket to port	
		if (bind (pNet->SvrSock, (struct sockaddr FAR *)&local, sizeof(local)) != 0) 
		{
			ReleaseNetResources(pNet);
			return (INVALID_HANDLE_VALUE);
		}

		// try to connect to server
		local.sin_port = htons((SHORT)pNet->PortNo);
		local.sin_addr.s_addr = SvrIPaddr.s_addr;
		if (connect (pNet->SvrSock, (struct sockaddr FAR *)&local, sizeof(local)) != 0) 
		{
			//zzzz WSAGetLastError returns 0 always ???
			res=WSAGetLastError();
			//if (res != WSAEWOULDBLOCK){ //non blocking operation in progress
			//	ReleaseNetResources(pNet);
			//	return (INVALID_HANDLE_VALUE);
			//}
		}
		

		// Always clear the read set before calling select()
		FD_ZERO(&fdread);
		FD_ZERO(&fdwrite);
		// Add socket s to the read set
		FD_SET(pNet->SvrSock, &fdread);
		FD_SET(pNet->SvrSock, &fdwrite);
		
		if (pNet->OpenConnectionTimeout < 25)
		{
			pNet->OpenConnectionTimeout=25;
		}

		selectTimeOut.tv_sec = 0;      // wait seconds for data
		selectTimeOut.tv_usec = pNet->OpenConnectionTimeout*1000;    //  and microseconds. (1000 micro is 1 mili)

		if ((ret = select(0, &fdread, &fdwrite, NULL, &selectTimeOut)) == SOCKET_ERROR) 
		{	
			res = WSAGetLastError();
			sprintf(errBuf,"Error in select() functionwith error %d\n",res);
			DebugPrint(errBuf,pNet,DEBUGLEVEL_DETAILED);
			
			ReleaseNetResources(pNet);
			return (INVALID_HANDLE_VALUE);
		}
		if (ret > 0) //ret = number of ready sockets
		{
			if (FD_ISSET(pNet->SvrSock, &fdwrite)) //check to see whether the socket is part of a set.
			{
				// write can be DONE on socket s
			}else{
				ReleaseNetResources(pNet);
				return (INVALID_HANDLE_VALUE);
			}
		}else{
			ReleaseNetResources(pNet);
			return (INVALID_HANDLE_VALUE);
		}

		setsockopt(pNet->SvrSock, SOL_SOCKET, SO_KEEPALIVE, (char *) &one, sizeof(one));
		pNet->idComDev = (HANDLE) pNet->SvrSock;
		pNet->ptr = 0;
	} 
	else // Otherwise we are using a serial connection
	{
		pNet->IsSocket = FALSE;
		wsprintf (szport, "\\\\.\\COM%d", PortNo);

		#ifdef M_DEBUG
			mLog(NULL, 0, 0, 0, PortNo, (int)idConnect, szport);
		#endif
		pNet->dwBaudRate  = pCfg->dwBaudRate;

		// Open the device
		pNet->idComDev = CreateFile (szport, GENERIC_READ | GENERIC_WRITE,
									0,		//exclusive access
									NULL,	// no security
									OPEN_EXISTING,
									//0, //FILE_ATTRIBUTE_NORMAL,
									FILE_FLAG_OVERLAPPED, 
									NULL);

		if (pNet->idComDev == INVALID_HANDLE_VALUE)
		{

			#ifdef M_DEBUG
				FormatMessage( 
					FORMAT_MESSAGE_ALLOCATE_BUFFER | 
					FORMAT_MESSAGE_FROM_SYSTEM | 
					FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL,
					GetLastError(),
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
					(LPTSTR) &lpMsgBuf,
					0,
					NULL 
				);
				mLog(NULL, 0, 0, 0, 0, (int)idConnect, (char *)lpMsgBuf);
				// Free the buffer.
				LocalFree( lpMsgBuf );
			#endif

			ReleaseNetResources(pNet);
			return (INVALID_HANDLE_VALUE);
		}
	}

	memcpy (&(pNet->Password), pPassword, sizeof(PASSWORDPARAMS));

	if (!AllocateControlThread(pNet, pCfg)) 
	{
		return (INVALID_HANDLE_VALUE);
	} 
	else 
	{
		return (idConnect);
	}
}

/////////////////////////////////////////////////////////////////////////
//	BOOL CommInits (LPCONNECTION pNet, LPSERIALCONFIG	pCfg) 
//
//	Called by AllocateControlThread for serial connections
//	
//	Init the com buffer and timers
//
/////////////////////////////////////////////////////////////////////////

BOOL CommInits (LPCONNECTION pNet, LPSERIALCONFIG	pCfg) 
{
	COMMTIMEOUTS	CommTimeOuts;

	// Cleanup any residual characters which may be hanging around
	SetupComm (pNet->idComDev, 4096, 4096);
	PurgeComm (pNet->idComDev, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);

	// MODBUS is always MASTER/SLAVE
	// Send/Rcv Thread simply blocks until it receives an answer
	// Configure the driver to block for up to time specified by timeout
	CommTimeOuts.ReadIntervalTimeout = 0;
	CommTimeOuts.ReadTotalTimeoutMultiplier = 0;
	CommTimeOuts.ReadTotalTimeoutConstant = pNet->TimeOut;
	CommTimeOuts.WriteTotalTimeoutMultiplier = 0;
	CommTimeOuts.WriteTotalTimeoutConstant = pNet->TimeOut;
	SetCommTimeouts (pNet->idComDev, &CommTimeOuts);

	// Direct Connection
	// Configure serial settings
	if (SetupConnection (pNet, pCfg) == 0) 
	{
		#ifdef M_DEBUG
			mLog(NULL, 0, 0, 0, (int)GetLastError(), pNet->iConnectionID, "SetupConnection error");
		#endif
		return (FALSE);
	}

	return (TRUE);
}

/////////////////////////////////////////////////////////////////////////
//	BOOL AllocateControlThread (LPCONNECTION pNet, LPSERIALCONFIG	pCfg)
//
//	Called by AllocateControlThread for serial connections
//	
//	Init the com buffer and timers
//
/////////////////////////////////////////////////////////////////////////

BOOL AllocateControlThread (LPCONNECTION pNet, LPSERIALCONFIG	pCfg)
{
	HANDLE	hIOThread;
	DWORD	dwThreadID;

	if (pCfg != NULL) 
	{
		if (!CommInits (pNet, pCfg)) 
		{
			ReleaseNetResources(pNet);
			return (FALSE);
		}
	}
	// if everything looks good to here
	// create the Thread & return the CONNECT handle
	hIOThread = CreateThread ((LPSECURITY_ATTRIBUTES)NULL,
								0,
								(LPTHREAD_START_ROUTINE)Control_Proc,
								(LPVOID)pNet,
								0,
								&dwThreadID);
	if (hIOThread == NULL)
	{
		ReleaseNetResources(pNet);
		return (FALSE);
	}

	pNet->ThreadEnable = CreateEvent (NULL,	FALSE,	FALSE,	NULL);
	pNet->TheadID = hIOThread;
	pNet->ThreadActive = TRUE;
	pNet->PollInProgress = FALSE;

	if (pCfg != NULL) 
	{
		pNet->DirectConnection = TRUE;
		pNet->RTS_Delay[0] = pCfg->RTS_Delay[0];
		pNet->RTS_Delay[1] = pCfg->RTS_Delay[1];
	} 
	else 
	{
		pNet->DirectConnection = FALSE;
	}
	
	return (TRUE);
}

/////////////////////////////////////////////////////////////////////////
//
//	_declspec(dllexport) int WINAPI setTimeout(HANDLE idConnect, int timeout) 
//
//	Called by Amadeus5
//	Allow to change the timeout of an existing connection.
//
/////////////////////////////////////////////////////////////////////////

_declspec(dllexport) int WINAPI setTimeout(HANDLE idConnect, int timeout) 
{
	LPCONNECTION	pNet;
	COMMTIMEOUTS	CommTimeOuts;

	pNet = FindConnect (idConnect);
	if (pNet == NULL)
		return (MBUS_INVALIDH);

	pNet->TimeOut = timeout;
	pNet->TempTimeOut = timeout;

	if ((pNet->SvrSock == INVALID_SOCKET) && (!(pNet->IsSocket))) 
	{
		GetCommTimeouts (pNet->idComDev, &CommTimeOuts);
		CommTimeOuts.ReadIntervalTimeout = 0;
		if (pNet->hCall == (HCALL)NULL) 
			CommTimeOuts.ReadTotalTimeoutMultiplier = (11000/pNet->dwBaudRate) ;
		else
			CommTimeOuts.ReadTotalTimeoutMultiplier = 0 ;
		CommTimeOuts.ReadTotalTimeoutConstant = pNet->TimeOut;
		if (pNet->hCall == (HCALL)NULL) 
			CommTimeOuts.WriteTotalTimeoutMultiplier = (11000/pNet->dwBaudRate) ;
		else
			CommTimeOuts.WriteTotalTimeoutMultiplier = 0 ;
		CommTimeOuts.WriteTotalTimeoutConstant = pNet->TimeOut;
		SetCommTimeouts (pNet->idComDev, &CommTimeOuts);
	}

	return (MBUS_OK);
}

/////////////////////////////////////////////////////////////////////////
//
//	_declspec(dllexport) int WINAPI setShortTimeout(HANDLE idConnect, int timeout) 
//
//	Called by Amadeus5
//	ShortTimeout is used for the 3 first bytes in readFile.
//
/////////////////////////////////////////////////////////////////////////

_declspec(dllexport) int WINAPI setShortTimeout(int timeout) 
{
	m_ShortTimeOut = timeout;

	return (MBUS_OK);
}
 
/////////////////////////////////////////////////////////////////////////
//
//	_declspec(dllexport) int WINAPI setOpenConnectionTimeout(HANDLE idConnect, int timeout) 
//
//	Called by Amadeus5
//	OpenConnectionTimeout is used when a connection to socket is opened. (in miliseconds)
//
/////////////////////////////////////////////////////////////////////////

_declspec(dllexport) int WINAPI setOpenConnectionTimeout(int timeout) 
{
	m_OpenConnectionTimeout = timeout;

	return (MBUS_OK);
}

/////////////////////////////////////////////////////////////////////////
//
//	_declspec(dllexport) int WINAPI setDoSaveTrnInverseCode(int mode) 
//
//	Called by Amadeus5
//	setDoSaveTrnInverseCode is used to set if we want to save as file the data from controller that has inverse code, c1 instead of 81 or 81 instead of c1. (default is FALSE)
//
/////////////////////////////////////////////////////////////////////////

_declspec(dllexport) int WINAPI setDoSaveTrnInverseCode(int mode) 
{												 
	
	if (mode == 1)
		m_DoSaveTrnInverseCode = TRUE;
	else
		m_DoSaveTrnInverseCode = FALSE;

	return (MBUS_OK);
}

/////////////////////////////////////////////////////////////////////////
//
//	_declspec(dllexport) int WINAPI setCleanBuffBeforeWriting(int mode) 
//
//	Called by Amadeus5
//	setCleanBuffBeforeWriting is used to clean the tcp line before we write command to the controller. (default is FALSE)
//
/////////////////////////////////////////////////////////////////////////

_declspec(dllexport) int WINAPI setCleanBuffBeforeWriting(int mode) 
{
	
	if (mode == 1)
		m_CleanBuffBeforeWriting = TRUE;
	else
		m_CleanBuffBeforeWriting = FALSE;

	return (MBUS_OK);
}

/////////////////////////////////////////////////////////////////////////
//
//	_declspec(dllexport) int WINAPI setSerialCfg(HANDLE idConnect, LPSERIALCONFIG	pCfg)  
//
//	Called by Amadeus5
//	Allow to change the parameters of an existing serial connection.
//
/////////////////////////////////////////////////////////////////////////

_declspec(dllexport) int WINAPI setSerialCfg(HANDLE idConnect, LPSERIALCONFIG	pCfg) 
{
	LPCONNECTION	pNet;

	pNet = FindConnect (idConnect);
	if (pNet == NULL)
		return (MBUS_INVALIDH);


	if (SetupConnection (pNet, pCfg) == 0) 
	{
		return (MBUS_WRITEFAIL);
	}

	return (MBUS_OK);
}




/////////////////////////////////////////////////////////////////////////
//
//	_declspec(dllexport) WORD WINAPI Get_Modbus_DLL_Revision()  
//
//	Called by Amadeus5
//	This function simply returns a version indication of the dll.
//
/////////////////////////////////////////////////////////////////////////

_declspec(dllexport) WORD WINAPI Get_Modbus_DLL_Revision()
{
	char strVersion[255];
	sprintf(strVersion,"ModbusDLL:Get_Modbus_DLL_Revision:: DLL version %x\n",  MODBUS_DLL_REV);

	OutputDebugString(strVersion);
	return (MODBUS_DLL_REV);
}

/////////////////////////////////////////////////////////////////////////
//
//	_declspec(dllexport) int WINAPI setPassword (HANDLE	idConnect, LPPASSWORDPARAMS pPassword)  
//
//	Called by Amadeus5
//	Password can be changed for the following cases:
//	1. Controller forgot password
//	2. Application wants to switch to encrypted mode
//	3. password refresh
//
/////////////////////////////////////////////////////////////////////////

_declspec(dllexport) int WINAPI setPassword (HANDLE	idConnect, LPPASSWORDPARAMS pPassword)
{
	LPCONNECTION	pNet;
	
	#ifdef M_DEBUG
		char TempBuf[4];
	#endif

	pNet = FindConnect (idConnect);
	if (pNet == NULL)
		return (MBUS_INVALIDH);	// Invalid CONNECT handle

	memcpy (&(pNet->Password), pPassword, sizeof(PASSWORDPARAMS));

	#ifdef M_DEBUG
		memset(TempBuf,0,4);
		TempBuf[0] = (BYTE)pNet->Password.Byte1;
		TempBuf[1] = (BYTE)pNet->Password.Byte2;
		TempBuf[2] = (BYTE)pNet->Password.Byte3;
		TempBuf[3] = (BYTE)pNet->Password.Byte4;
		mLog(TempBuf, 4, 0, 0, pNet->Password.Activated, (int)idConnect, "setPassword");
	#endif

	return (MBUS_OK);
}

/////////////////////////////////////////////////////////////////////////
//
//	_declspec(dllexport) HANDLE WINAPI ConnectDDS(struct in_addr SvrIPaddr, int PortNo,	int	timeout, LPSERIALCONFIG	pCfg, WORD DDSProtocol, LPPASSWORDPARAMS pPassword)  
//
//	Called by Amadeus5
//	function to open the COM or the TCP connection
//
//	Returns:
//	INVALID_HANDLE_VALUE (-1)	- if connection fails
//	Connection handle			- if connection succeds
/////////////////////////////////////////////////////////////////////////

_declspec(dllexport) HANDLE WINAPI ConnectDDS(struct in_addr SvrIPaddr,
											  int PortNo,
											  int	timeout,
											  LPSERIALCONFIG	pCfg,
											  WORD DDSProtocol, 
											  LPPASSWORDPARAMS pPassword)
{
	//char strVersion[255];
	//sprintf(strVersion,"ModbusDLL: ConnectDDS::DLL version %x\n",  MODBUS_DLL_REV);

	//OutputDebugString(strVersion);
	return (OpenConnection (DDSProtocol, SvrIPaddr, PortNo, timeout, pCfg, pPassword));
}

/////////////////////////////////////////////////////////////////////////
//
//	_declspec(dllexport) int WINAPI HookRspNotification(HANDLE		idConnect,
//														HWND		hWnd,
//														UINT		NotifyMsg,
//														long		lEvent) 
//
//	Called by Amadeus5
//	This function provides a mechanism for the dll to notify the controlling application whenever the results 
//	of a poll request are available to be read.  idConnect identifies the connection.  
//	hWnd is the handle of the Window which will receive the notification message.  
//	The notification message will be posted by the dll based on either data coming back from a slave device 
//	or a timeout condtion.
//
/////////////////////////////////////////////////////////////////////////

_declspec(dllexport) int WINAPI HookRspNotification(HANDLE		idConnect,
													HWND		hWnd,
													UINT		NotifyMsg,
													long		lEvent)
{
	LPCONNECTION	pNet;

	//OutputDebugString("HookRspNotification \n");

	pNet = FindConnect (idConnect);
	if (pNet == NULL)
		return (MBUS_INVALIDH);

	pNet->hWnd = hWnd;
		// this message gets sent to the hWnd whenever a MODBUS
		// message transaction completes
	pNet->Notification = NotifyMsg;
	pNet->lEvent = lEvent;	//lEvent currently not used

	return (MBUS_OK);
}


_declspec(dllexport) void WINAPI SetCallback(HANDLE		idConnect, 
											 //BYTE(*ptr)(BYTE))
											 void(*ptr)(void))
{
	LPCONNECTION	pNet;
	//OutputDebugString("SetCallback::Entered \n");
	pNet = FindConnect (idConnect);
	if (pNet == NULL)
		return;
	//OutputDebugString("SetCallback::Callback function configured successfully\n");
	pNet->ptr = ptr;
}

/////////////////////////////////////////////////////////////////////////
//
//	_declspec(dllexport) int WINAPI CloseConnection (HANDLE idConnect)
//
//	Called by Amadeus5
//	This function closes an open connection ond releases the resources associated with it.
//
/////////////////////////////////////////////////////////////////////////

_declspec(dllexport) int WINAPI CloseConnection (HANDLE idConnect)
{
	LPCONNECTION	pNet;

	pNet = FindConnect (idConnect);
	
	if (pNet == NULL)
		return (MBUS_INVALIDH);	// device NOT open
	
	DebugPrint("CloseConnection",pNet,DEBUGLEVEL_DETAILED);

	// for serial communication
	if ((pNet->hCall == (HCALL)NULL) && (!pNet->IsSocket))
	{
		// purge out the device & close it down
		SetCommMask (pNet->idComDev, 0);
		PurgeComm (pNet->idComDev, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
	}
	// for modem communication
	if (pNet->hCall != (HCALL)NULL)
	{
		HangupCall(pNet);
	}
	
	pNet->IsClosing = TRUE;
	// enable the Send/Rcv Thread & wait on it to terminate
	while (pNet->ThreadActive)
		SetEvent(pNet->ThreadEnable);
	
	if (pNet->IsSocket == FALSE)	
		CloseHandle (pNet->idComDev);

	// if both handles are equal then this is a TCP connection
	// we should remove the idComDev so we don't release the same handle twice
	//if (pNet->idComDev== pNet->	SvrSock)
		//pNet->idComDev = INVALID_SOCKET;
	

		

	pNet->idComDev = INVALID_HANDLE_VALUE;
	
	
	ReleaseNetResources(pNet);	

	return (MBUS_OK);
}

/////////////////////////////////////////////////////////////////////////
//
//	_declspec(dllexport) int WINAPI CloseConnection (HANDLE idConnect)
//
//	Called by Amadeus5
//	This is an entry point function to tell the DLL to start listening on the second bus.
//	pNet->iConnectionMode is set to 1
//
/////////////////////////////////////////////////////////////////////////

_declspec(dllexport) int WINAPI StartEventMode (HANDLE	idConnect)
{
	LPCONNECTION	pNet;

	pNet = FindConnect (idConnect);
	if (pNet == NULL)
		return (MBUS_INVALIDH);	// device NOT open

	if  (pNet->PollInProgress)
		return (MBUS_INPROGRESS);	// Msg overrun

	setTimeout(idConnect, pNet->TimeOut);
	
	pNet->Exception = MBUS_OK;
	// Event Mode enabled
	pNet->iConnectionMode = 1;

	SetEvent(pNet->ThreadEnable);

	return (MBUS_OK);
}

/////////////////////////////////////////////////////////////////////////
//
//	_declspec(dllexport) int WINAPI StopEventMode (HANDLE idConnect)
//
//	Called by Amadeus5
//	This is an entry point function to tell the DLL to stop listening on the second bus.
//	pNet->iConnectionMode is set to 0
//
/////////////////////////////////////////////////////////////////////////

_declspec(dllexport) int WINAPI StopEventMode (HANDLE	idConnect)
{
	LPCONNECTION	pNet;

	pNet = FindConnect (idConnect);
	if (pNet == NULL)
		return (MBUS_INVALIDH);	// device NOT open

	if  (pNet->PollInProgress)
		return (MBUS_INPROGRESS);	// Msg overrun

	// Event Mode disabled
	pNet->iConnectionMode = 0;

	return (MBUS_OK);
}

/////////////////////////////////////////////////////////////////////////
//
//	_declspec(dllexport) int WINAPI PollMODBUS (HANDLE	idConnect, LPMODBUSMSG	pMsg)
//
//	Called by Amadeus5
//	The controlling application may initiate a request for data from a slave device using this function.  
//	Parameters contained within pMsg define the slave node, data type, and addresses of the data to be read.  
//	The dll will pass the request on to the designated slave and return immediately.  
//	Notification of completion will be posted via the Windows Message configured via the HookRspNotification function. 
//	The dll does not do any buffering of message going out the connection.  
//	The controlling application must wait for the completion of a PollMODBUS or WriteMODBUS initiated 
//	transaction before requesting another, otherwise the requested function will return an OVERRUN error.
//
/////////////////////////////////////////////////////////////////////////

_declspec(dllexport) int WINAPI PollMODBUS (HANDLE	idConnect, LPMODBUSMSG	pMsg)
{
	LPCONNECTION	pNet;
	long	Temp;
	// Get the network info
	pNet = FindConnect (idConnect);
	if (pNet == NULL)
		return (MBUS_INVALIDH);	// device NOT open

	if  (pNet->PollInProgress)
		return (MBUS_INPROGRESS);	// Msg overrun

	// check for obvious parameter errors
	if (pMsg->SlaveId == 0)
		return (MBUS_INVALIDID);

	if (pMsg->Address == 0)
		return (MBUS_INVALIDADDR);

	Temp = pMsg->Address - 1;
	if ((pMsg->Length == 0) || ((Temp + pMsg->Length) > 0xffff))
		return (MBUS_INVALIDLEN);

	// currently only provides poll support for Msg Types 01-04
	// (maximum of 960 COILS or 128 REGISTERS per scan)
	// Added command 65 10/31/96
	switch (pMsg->CmdId)
{
		case 1:
		case 2:	if (pMsg->Length > MAX_COILXFER)
					return (MBUS_INVALIDLEN);
				break;
		case 3:
		case 4:	if (pMsg->Length > MAX_REGXFER)
					return (MBUS_INVALIDLEN);
				break;
		case 65: if (pMsg->Length > 64)
					 return (MBUS_INVALIDLEN);
				break;
		default:
				return (MBUS_INVALIDCMD);
				break;
		}

	// parameters look O.K.
	// copy data to CONNECT Output Msg Struct and tell Send/Rcv Thread to send it.
	memcpy (&(pNet->OutMsg), pMsg, sizeof(MODBUSMSG));

	pNet->PollInProgress = TRUE;
	pNet->Exception = MBUS_OK;
	// Event Mode disabled
	pNet->iConnectionMode = 0;

	SetEvent(pNet->ThreadEnable);

	// Msg queued up for transmission
	return (MBUS_OK);
}

/////////////////////////////////////////////////////////////////////////
//
//	_declspec(dllexport) int WINAPI WriteMODBUS (HANDLE			idConnect,
//											  LPMODBUSMSG	pMsg,
//											  LPWORD		pDataArray)
//
//	Called by Amadeus5
// This function is called by the VB application
// The controlling application may write data to a slave device using this function.  
// Parameters contained within pMsg define the slave node, data type, and addresses of the data to be written.  
// The dll will pass the request on to the designated slave and return immediately.  
// Notification of completion will be posted via the Windows Message configured via the HookRspNotification function. 
//
/////////////////////////////////////////////////////////////////////////

_declspec(dllexport) int WINAPI WriteMODBUS (HANDLE			idConnect,
											  LPMODBUSMSG	pMsg,
											  LPWORD		pDataArray)
{
	LPCONNECTION	pNet;
	long	Temp;

	// Get the related CONNECT struct given the idConnect
	pNet = FindConnect (idConnect);
	if (pNet == NULL || pMsg==0)
		return (MBUS_INVALIDH);	// Invalid CONNECT handle

	if  (pNet->PollInProgress)
		return (MBUS_INPROGRESS);	// Msg Output Overrun

	// check for illogical request params
	if (pMsg->Address == 0)
		return (MBUS_INVALIDADDR);

	Temp = pMsg->Address - 1;
	if ((pMsg->Length == 0) || ((Temp + pMsg->Length) > 0xffff))
		return (MBUS_INVALIDLEN);

	switch (pMsg->CmdId)
	{
		case 5:
		case 6:	if (pMsg->Length > 1)
					return (MBUS_INVALIDLEN);
				break;
		case 15: if (pMsg->Length > MAX_COILXFER)
					return (MBUS_INVALIDLEN);
				break;
		case 16: if (pMsg->Length > MAX_REGXFER)
					return (MBUS_INVALIDLEN);
				break;
		case 22:
				if (pMsg->Length != 2)
					return (MBUS_INVALIDLEN);
				break;
		default:
				return (MBUS_INVALIDCMD);
				break;
	}

	// request params look OK
	// copy data to CONNECT Output Msg Struct
	memcpy (&(pNet->OutMsg), pMsg, sizeof(MODBUSMSG));

	// copy data points to transfer into CONNECT allocated mem buf
	if (pNet->hMem == NULL)
		return (MBUS_OUTOFMEM);

	if (pDataArray != NULL)
	{
		Temp = pMsg->Length * 2;
		memcpy (pNet->pBuf, pDataArray, Temp);
	}

	// kick start the Send/Rcv Thread
	pNet->PollInProgress = TRUE;
	pNet->Exception = MBUS_OK;
	// Event Mode disabled, because we are sending info to the controllers
	pNet->iConnectionMode = 0;

	// The thread is waiting to be run, release it.
	SetEvent(pNet->ThreadEnable);

	return (MBUS_OK);
}

/////////////////////////////////////////////////////////////////////////
//
//	_declspec(dllexport) int WINAPI MODBUSResponse (HANDLE		idConnect,
//												LPMODBUSMSG	pMsg,
//												LPWORD		pDataArray,
//												LPWORD		MaxArraySize)
//
//	Called by Amadeus5
//	The results of the last PollMODBUS or WriteMODBUS command may be obtained from this function.  
//	The original message parameters are contained in pMsg.  
//	If any errors were encounter with the communications with the slave, 
//	this function will return an error status.  
//	If the request was a READ, data from the slave will be moved to pDataArray.  
//	MaxArraySize indicates the size of the user supplied buffer.
//
/////////////////////////////////////////////////////////////////////////

_declspec(dllexport) int WINAPI MODBUSResponse (HANDLE		idConnect,
												LPMODBUSMSG	pMsg,
												LPWORD		pDataArray,
												LPWORD		MaxArraySize)
{
	LPCONNECTION	pNet;
	int	temp;

//	#ifdef M_DEBUG
//		char TempBuf[220];
//	#endif	

	// obtain the results of the last transaction
	pNet = FindConnect (idConnect);

	if (pNet == NULL)
		return (MBUS_INVALIDH);		// ERROR--Invalid handle

	if (pNet->PollInProgress)
		return (MBUS_INPROGRESS);	// Msg not yet completed Application is out of sync
	
	// if polling
	if (pNet->iConnectionMode == 0)
		// copy original message to response buffer
		memcpy (pMsg, &(pNet->OutMsg), sizeof(MODBUSMSG));

	// copy MODBUS data points from CONNECT Struct
	// to response buffer (watch for NULL pointers & too small buffers)
	// 
	// (not all commands will utilize the data point buffer, but its
	//	easier to do a simple copy than try to figure out which cmd 
	//	we're dealing with.)
	//
	if (pNet->hMem == NULL)
		return (MBUS_OUTOFMEM);

	*MaxArraySize = pNet->OutMsg.Length;		
	if (pDataArray != NULL)
	{
		if (*MaxArraySize <= MAX_POINTS)
			temp = *MaxArraySize;
		else
			temp = MAX_POINTS;
		temp *= 2;
		memcpy (pDataArray, pNet->pBuf, temp);
	}
	
	// any problems with the message transaction will show up in the Exception indication.
	return (pNet->Exception);
}
  
/////////////////////////////////////////////////////////////////////////
//
//	void	WINAPI	Calc_CRC_DDS (LPCONNECTION pNet, BYTE FAR *pData, int count)
//
//	Called by FormatDDS and InterpretDDS
//
// This is used for both sending and recieving messages to thec controller
//  pNet - the network information
//	pData - the data
//	count	- Date Length
//
// The functions calculates the CRC and then appenend the CRC to the end of the data
// pointed by pData and terminate with 0x04
//
/////////////////////////////////////////////////////////////////////////

void	WINAPI	Calc_CRC_DDS (LPCONNECTION pNet, BYTE FAR *pData, int count)
{
    WORD temp, RA, RB, RC, RD;
    int	i, TempCount;
	BYTE pDataTemp[MAX_BUF_SIZE];
    
	RC = 0x0000;
	// Copy the information to pDataTemp
	for ( i = 0; i < count;i++)
	{
		pDataTemp[i] = pData[i];
	}

	// If Password Activated
	// Add the password bytes to the end of the buffer
	if (pNet->Password.Activated == 1 )
	{
		TempCount = count + 4;
		pDataTemp[count] = (BYTE)pNet->Password.Byte1;
		pDataTemp[count + 1] = (BYTE)pNet->Password.Byte2;
		pDataTemp[count + 2] = (BYTE)pNet->Password.Byte3;
		pDataTemp[count + 3] = (BYTE)pNet->Password.Byte4;
	}
	else
	{
		TempCount = count;
	}

//	#ifdef M_DEBUG//		mLog((char*)pDataTemp, TempCount, 0, 0, 0, pNet->iConnectionID, "Calc_CRC_DDS");
//	#endif
		
	for (i = 0; i < TempCount; i++)
	{ 
		RA = (WORD)pDataTemp[i];
		if (i < count)
			*pData++ ;
		RD = RC;
		RC = RC >> 8;
		RC = RC ^ RA;		
		RB = RC;
		RC = RC >> 4;
		RC = RC ^ RB;		
		RB = RC;
		RC = RC << 5;
		RC = RC ^ RB;		
		RD = RD << 8;
		RC = RC ^ RD;		
		RB = RC;
		RC = RC << 12;
		RC = RC ^ RB;				
	} 
	temp = RC >> 8;
	
	*pData++ = (BYTE)temp & 0x00ff;	
	*pData++ = (BYTE)RC & 0x00ff ;
	// Terminate the message
	*pData = 0x04;
}

/////////////////////////////////////////////////////////////////////////
//
//	DWORD	WINAPI	FormatDDS (LPCONNECTION pNet, BYTE *pOut, LPDWORD pCount)
//
//	Called by Control_Proc
//	Format a message receive from the Amadeus5 application and prepare it.
//	Calculation of the CRC, start and end flags, encryption...
//
/////////////////////////////////////////////////////////////////////////

DWORD	WINAPI	FormatDDS (LPCONNECTION pNet, BYTE *pOut, LPDWORD pCount)
{
	COMMTIMEOUTS	CommTimeOuts;
	WORD	temp1, temp2, *pValue;
	int	bufindex, i ;
 	int	taddr;
	int dMsgLen;	// Message Length
	DWORD Expected;
	//char buf[100];

	temp2 = pNet->Protocol;
	temp1 = temp2 & 0xFF;

	// First byte contains the protocol
	pOut[0] = (BYTE)temp1;  


	// Second byte contains the SlaveId-1
	if (pNet->OutMsg.SlaveId == 0)
		pOut[1] = 0x5F;
	else
		pOut[1] = pNet->OutMsg.SlaveId - 1; // Protocol 4 on enleve le -1

	temp2 = pNet->OutMsg.Length * 2 + 1; // Longueur + 1 pour la commande 
	temp1 = temp2 & 0xFF;

	// Third byte cotnains the message length
	pOut[2] = (BYTE)temp1;  // Longueur


	taddr = pNet->OutMsg.Address-1; 
	temp1 = (taddr >> 8) & 0xFF;
	temp1 = taddr & 0xFF;
	pOut[3] = (BYTE)temp1;  // Cmd 
	
	pValue = pNet->pBuf;
	bufindex = 4; 
	for (i=0; i<pNet->OutMsg.Length; i++)
	{
		temp2 = *pValue;
		temp1 = (temp2 >> 8) & 0xFF;
		pOut[bufindex++] = (BYTE)temp1;
		temp1 = temp2 & 0xFF;
		pOut[bufindex++] = (BYTE)temp1;
		++pValue;
	}

	//sprintf (buf, "DLL Modbus: FormatDDS pOut[3] %c \n", pOut[3]);
	//OutputDebugString(buf);
	switch (pOut[3])
	{
		//test if we add a bit because of Modbus
		case 0x02:
			Correct(pOut,pNet->OutMsg.Length, bufindex, 24);
			bufindex = bufindex - ((pNet->OutMsg.Length *2)/24);
			pOut[2] = pOut[2] - ((pNet->OutMsg.Length *2)/24);
			break;
		case 0x46:
			Correct(pOut,pNet->OutMsg.Length, bufindex, 18);
			bufindex = bufindex - ((pNet->OutMsg.Length *2)/18);
			pOut[2] = pOut[2] - ((pNet->OutMsg.Length *2)/18);
			break;
		case 0x06:
		//case 0x07:
		//case 0x12:
			Correct(pOut,pNet->OutMsg.Length, bufindex, 10);
			bufindex = bufindex - ((pNet->OutMsg.Length *2)/10);
			pOut[2] = pOut[2] - ((pNet->OutMsg.Length *2)/10);
			break;
		
		//For new Eprom 1.6.2004 WP have 10 DP
		case 0x07:
		case 0x12:
			// Byte number 2 in Command 0x07 and 0x12 defines the length of the message
			// If the MSB is set then the message is 12 bytes length otherwise its 10 bytes length
			if (pOut[5] & 0x80) 
				dMsgLen = 12;
			else
				dMsgLen = 10;

			Correct(pOut,pNet->OutMsg.Length, bufindex, dMsgLen);
			bufindex = bufindex - ((pNet->OutMsg.Length *2)/dMsgLen);
			pOut[2] = pOut[2] - ((pNet->OutMsg.Length *2)/dMsgLen);
			break;

		//Mega 
		case 0x17:
			dMsgLen = 12;
			Correct(pOut,pNet->OutMsg.Length, bufindex, dMsgLen);
			bufindex = bufindex - ((pNet->OutMsg.Length *2)/dMsgLen);
			pOut[2] = pOut[2] - ((pNet->OutMsg.Length *2)/dMsgLen);
			break;

		case 0x11:
		case 0x54:
			Correct(pOut,pNet->OutMsg.Length, bufindex, 14);
			bufindex = bufindex - ((pNet->OutMsg.Length *2)/14);
			pOut[2] = pOut[2] - ((pNet->OutMsg.Length *2)/14);
			break;
		case 0x14:
			Correct(pOut,pNet->OutMsg.Length, bufindex, 8);
			bufindex = bufindex - ((pNet->OutMsg.Length *2)/8);
			pOut[2] = pOut[2] - ((pNet->OutMsg.Length *2)/8);
			break;
		case 0x43:
			Correct(pOut,pNet->OutMsg.Length, bufindex, 4);
			bufindex = bufindex - ((pNet->OutMsg.Length *2)/4);
			pOut[2] = pOut[2] - ((pNet->OutMsg.Length *2)/4);
			break;

		//For Mega: to support more than FFFF employees		
		// cardholder number is on 3 bytes 
		case 0x26:
			// Same message is used for download cardholder, delete cardholder , update cardholder level 
			// Byte number 1 contain the message length 
			dMsgLen = (pOut[4] & 0x1F) ;
			
			if (dMsgLen %2==1)	 //Odd  
				{
				dMsgLen ++; //the length in message does not count the cmd itself  
				Correct(pOut,pNet->OutMsg.Length, bufindex, dMsgLen);
				bufindex = bufindex - ((pNet->OutMsg.Length *2)/dMsgLen);
				pOut[2] = pOut[2] - ((pNet->OutMsg.Length *2)/dMsgLen);
				}
			break;
		case 0x0d:
		case 0x0e:
		case 0x22:
		case 0x24:
		case 0x36:
		case 0x37:
		case 0x79:
		case 0x7f:
		//case 0x76:
		case 0x15:
			bufindex --;
			pOut[2] --;
			break;
		case 0x41:
		case 0x55:
		case 0x81:
		case 0xc1:
		case 0x01:
			bufindex --;
			bufindex --;
			pOut[2] --;
			pOut[2] --;
			break;
		case 0x08:
		case 0x71:
		case 0x73:
		case 0x78:
			bufindex = 4;
			pOut[2] = 1;
			break;
		case 0x72:
			bufindex = 8;
			pOut[2] = 5;
			break;
		case 0xfd: //TPLE read memory CmdFD = 72 00 00 0E 03
			bufindex = 8;
			pOut[2] = 5;
			pOut[3] = 0x72;
			pOut[4] = 0;
			pOut[5] = 0;
			pOut[6] = 0x0e;
			pOut[7] = 03;
			break;		
		case 0xfe: //Mega read memory CmdFE = 72 01 04 0F 03
			bufindex = 8;
			pOut[2] = 5;
			pOut[3] = 0x72;
			pOut[4] = 01;
			pOut[5] = 04;
			pOut[6] = 0x0f;
			pOut[7] = 03;
			break;		
		case 0xff: //TPL read memory CmdFF = 72 00 00 0E 02 
			bufindex = 8;
			pOut[2] = 5;
			pOut[3] = 0x72;
			pOut[4] = 0;
			pOut[5] = 0;
			pOut[6] = 0x0e;
			pOut[7] = 02;
			break;		
		// Commands 0b and 31 are always sent with 01 after 
		case 0x31: 
		case 0x0b:
			bufindex = 5;
			pOut[2] = 2;
			pOut[4] = 1;
			break;
		case 0x4b:
		case 0x4c:
			bufindex = 5;
			pOut[2] = 2;
			break;
		default:
			break;
	}

	if (pNet->OutMsg.SlaveId == 0)
		Expected = 0;		//No response to broadcast
	else
	{
		switch (pOut[3])
		{
			case 0x81:
			case 0xc1:
			case 0x01:
				Expected = 3;
				break;
			case 0x08:
				Expected = 7;
				break;
			case 0x31:
				Expected = 3;
			//	Expected = 11;
				break;
			case 0x0b:
				Expected = 3;
				break;
			case 0x41:
				Expected = 0;
				break;
			case 0x4b:
				Expected = 3;
				break;
			case 0x4c:
				Expected = 23;
				break;
			case 0x57:
				Expected = 0;
				break;
			case 0x71:
				Expected = 16;
				break;
			case 0x72:
				Expected = 4 + pOut[7] + 3 ;
				break;
			case 0x73:
				Expected = 13;
				break;
			case 0x74:
				Expected = 18;
				break;
			default:
				// Most configuration commands, we expect an answer of 7 bytes length, 
				Expected = 7;	// Nombre de bits attendu pour la reponse
				break;
		}
	}
	
	// if there is an encryption, we have to execute a Xor of the first byte 
	// of the password with all the data bytes of the messages:
	// ex: Message: 40 00 01 31 7B DF 04 and password 01 01 01 01
	// 31 xor 01; no xor with the 3 first and last bytes 

	//#ifdef M_DEBUG
		// mLog((char*)pOut, bufindex, 0, 0, 0, pNet->iConnectionID, "pOut before encrypt in FormatDDS");
	//#endif				
	
	if (pNet->Password.Activated == 1)
	{
		for (i = 3; i < bufindex;i++)
		{
			pOut[i] = pOut[i] ^ pNet->Password.Byte1;
		}
	}
	
	Calc_CRC_DDS (pNet, pOut, bufindex);    
	*pCount = bufindex + 3;

	// For Message 81/c1 we check that the last byte of the CRC is not = to Protocol Byte (first byte)
	// if it is the case it cause problem to Controller address 4, as he understand it as a start of message for him
	// To solve the problem, we send cmd 81 01 or c1 01  instaed of 81 or C1
	// and we recalculate the CRC 
	// The problem appears with Ardan Protocol (AD) sending C1 to controller 14
	// also with Keico Protocol A9, sending 81 to controller 01 was producing A9 as CRC second byte 
	// See case 
	if ((pOut[3]==0x81) || (pOut[3]==0xc1) )
	{
		if (pOut[5]==pOut[0])
		{
			bufindex ++;
			pOut[2] ++;
			pOut[4]=1;
			Calc_CRC_DDS (pNet, pOut, bufindex);    
			*pCount = bufindex + 3;			
		}
	}

	//Timeout Special pour la commande 09 et 73
	if ((pOut[3]==0x09) || (pOut[3]==0x73) )
		pNet->TimeOut = 10 * pNet->TempTimeOut;
	else
	{
		pNet->TimeOut =  pNet->TempTimeOut;
	}

	CommTimeOuts.ReadIntervalTimeout = 0;
	if (pNet->hCall == (HCALL)NULL) 
		CommTimeOuts.ReadTotalTimeoutMultiplier = (11000/pNet->dwBaudRate) ;
	else
		CommTimeOuts.ReadTotalTimeoutMultiplier = 0 ;
	CommTimeOuts.ReadTotalTimeoutConstant = pNet->TimeOut;
	if (pNet->hCall == (HCALL)NULL) 
		CommTimeOuts.WriteTotalTimeoutMultiplier = (11000/pNet->dwBaudRate) ;
	else
		CommTimeOuts.WriteTotalTimeoutMultiplier = 0 ;
	CommTimeOuts.WriteTotalTimeoutConstant = pNet->TimeOut;
	SetCommTimeouts (pNet->idComDev, &CommTimeOuts);
	//Fin du timeout special

	return(Expected);	
}

/////////////////////////////////////////////////////////////////////////
//
//	int	WINAPI	DDSAnswer (LPCONNECTION pNet, BYTE *pInBuf, DWORD BytesIn)
//
//	Called by InterpretDDS
//	Prepare the buffer to be sent to Amadeus5 
//
/////////////////////////////////////////////////////////////////////////

int	WINAPI	DDSAnswer (LPCONNECTION pNet, BYTE *pInBuf, DWORD BytesIn)
{
	int		i,iTemp;
	WORD	temp;
	BYTE 	*pData;
	WORD	*pValue;

	pValue = pNet->pBuf;
	for (i=0; i<100; i++)
		memset (pValue++,0,sizeof(pValue));

	pValue = pNet->pBuf;

	if (BytesIn == 7)
	{	
		pNet->OutMsg.Length = 1;
		pData = &pInBuf[3];	
		temp = *pData++;
		*pValue++ = temp;
	}
	else
	{
		iTemp = 0;
		if (pNet->iConnectionMode == 1)
		{
			if ( (pInBuf[7] == 0x60 || pInBuf[7] == 0x70) )
			{
				*pValue++ = 0x71;
				iTemp++;
				*pValue++ = pInBuf[1];
				iTemp++;
			}
			if ( pInBuf[2] == 0x05 && pInBuf[3] == 0x71 )
			{
				*pValue++ = 0x71;
				iTemp++;
				*pValue++ = pInBuf[1];
				iTemp++;
			}
		}
		pNet->OutMsg.Length = (int) pInBuf[2]; 
		pData = &pInBuf[3];	

		for (i=0; i<pNet->OutMsg.Length ; i++)
		{
			temp = *pData++;
			*pValue++ = temp;
		}
		pNet->OutMsg.Length += iTemp; // add 2 bytes for the 'byte 71 and the controller address
	}
	
	return (MBUS_OK);
}

int	WINAPI	CheckCRCforBus2 (LPCONNECTION pNet, BYTE *pInBuf, DWORD BytesIn)
{
	BYTE	cks1,cks2,cks3;
	DWORD	dwTemp;

	dwTemp = BytesIn - 3;	
	cks1 = pInBuf[dwTemp];
	cks2 = pInBuf[dwTemp+1];
	cks3 = pInBuf[dwTemp+2];

	Calc_CRC_DDS (pNet, pInBuf, dwTemp);

	if ((pInBuf[dwTemp] != cks1) || (pInBuf[dwTemp+1] != cks2))
	{
   		pInBuf[dwTemp] = cks1;	// restore original ckecksum bytes
		pInBuf[dwTemp+1] = cks2;
		pInBuf[dwTemp+2] = cks3;
 		return (MBUS_CHECKSUM);
	}
	return (MBUS_OK);
}

/////////////////////////////////////////////////////////////////////////
//
//	int	WINAPI	InterpretDDS (LPCONNECTION pNet, BYTE *pInBuf, DWORD BytesIn, DWORD Expected, BYTE *pSendBuf)
//
//	Called by Control_Proc
//	Analyse of the receive packet.
//	Verification of the flags, CRC, encryption, message validity, controller address...
//
//	pInBuf-  The answer
//  BytesIn - Answer Length
//	Expected - how many bytes we expected to receive
//	pSendBuf - The data sent to the controller
/////////////////////////////////////////////////////////////////////////

int	WINAPI	InterpretDDS (LPCONNECTION pNet, BYTE *pInBuf, DWORD BytesIn, DWORD Expected, BYTE *pSendBuf)
{
	BYTE	cks1,cks2,cks3;
	DWORD	j, dwTemp;
	int	retval;
//	char buf[100];

	if (BytesIn != Expected)
		return (MBUS_TIMEOUT);

	// Answer must start with 0x41 and end with 0x04
	if ( (pInBuf[0] != 0x41) || (pInBuf[BytesIn-1] != 0x04)  )
		return (MBUS_INVALIDPROTO);
    
	// if the answer is from a different controller which we sent the command
	if ((pInBuf[1] != 0xFF)  && (pInBuf[1] != pSendBuf[1]) )
		return (MBUS_INVCTRADDR);

	// Get the last 2 bytes
	dwTemp = BytesIn - 3;
	// Which is the CRC
	cks1 = pInBuf[dwTemp];
	cks2 = pInBuf[dwTemp+1];
	cks3 = pInBuf[dwTemp+2];

	Calc_CRC_DDS (pNet, pInBuf, dwTemp);

//	#ifdef M_DEBUG
//		mLog(pInBuf, BytesIn, Expected, 0, 0, pNet->iConnectionID, "pInBuf in InterpretDDS after CRC");
//	#endif

	if ((pInBuf[dwTemp] != cks1) || (pInBuf[dwTemp+1] != cks2))
	{
   		pInBuf[dwTemp] = cks1;	// restore original ckecksum bytes
		pInBuf[dwTemp+1] = cks2;
		pInBuf[dwTemp+2] = cks3;
 		return (MBUS_CHECKSUM);
	}

	// if there is an encryption, we have to execute a Xor of the first byte 
	// of the password with all the data bytes of the messages:

	if (pNet->Password.Activated == 1)
	{
		for (j = 3; j < (BytesIn - 3);j++)
		{
			pInBuf[j] = pInBuf[j] ^ pNet->Password.Byte1;
			pSendBuf[j] = pSendBuf[j] ^ pNet->Password.Byte1;
		}
	}

	//if (pNet->iConnectionMode == 0)
	//{
		
//		sprintf (buf, "DLL Modbus: InterpretDDS - pNet->OutMsg.CmdId %d \n", pNet->OutMsg.CmdId);
//		OutputDebugString(buf);
		switch (pNet->OutMsg.CmdId)
		{
			case 4: // for read commands (queries)
//				sprintf (buf, "DLL Modbus: InterpretDDS - pSendBuf[3] %d \n", pSendBuf[3]);
//				OutputDebugString(buf);
				switch (pSendBuf[3])
				{
					case 0x81: if (pInBuf[3] != 0x60) retval=MBUS_INVRESPCODE; break;
					case 0xC1: if (pInBuf[3] != 0x70) retval=MBUS_INVRESPCODE; break;
					case 0x0B: if (pInBuf[3] != 0x62 && (pInBuf[3] != 0x66)) retval=MBUS_INVRESPCODE; break;
					case 0x4B: if (pInBuf[3] != 0x63) retval=MBUS_INVRESPCODE; break;
					case 0x72: if (pInBuf[3] != 0x64) retval=MBUS_INVRESPCODE; break;
					case 0x4C: if (pInBuf[3] != 0x65) retval=MBUS_INVRESPCODE; break;
				}

				if (retval == MBUS_INVRESPCODE) 
				{	
					OutputDebugString("DLL Modbus: Error 274 \n");
					break;
				}
						retval=DDSAnswer(pNet, pInBuf, BytesIn);
				break;
			case 16: // For configuration commands
//				sprintf (buf, "DLL Modbus: InterpretDDS - pSendBuf[3] %d \n", pSendBuf[3]);
//				OutputDebugString(buf);
				// verification if the answer is expected
				// for each message sent , we expect a certain answer
				// So we check the answer according to the question code
				switch (pSendBuf[3])
				{
					// Polling
					// For 0x81 we expect answer 0x60
					case 0x81: if (pInBuf[3] != 0x60) retval=MBUS_INVRESPCODE; break;
					// For 0xC1 we expect answer 0x70
					case 0xC1: if (pInBuf[3] != 0x70) retval=MBUS_INVRESPCODE; break;
					// Get Time and Date
					// look at Query message section in the protocol reference to understand the expected bytes
					case 0x0B: if (pInBuf[3] != 0x62 && (pInBuf[3] != 0x66)) retval=MBUS_INVRESPCODE; break;
					case 0x4B: if (pInBuf[3] != 0x63) retval=MBUS_INVRESPCODE; break;
					case 0x72: if (pInBuf[3] != 0x64) retval=MBUS_INVRESPCODE; break;
					case 0x4C: if (pInBuf[3] != 0x65) retval=MBUS_INVRESPCODE; break;
					case 0x08: 
					case 0x02: 
					case 0x03: 
					case 0x05:
					case 0x06: 
					case 0x46:
					case 0x07:
					case 0x09: 
					case 0x0C: 
					case 0x0D: 
					case 0x0E: 
					case 0x10:
					case 0x11:
					case 0x12:
					case 0x13:
					case 0x14:
					case 0x16:
					case 0x20:
					case 0x21:
					case 0x22:
					case 0x23:
					case 0x24: 
					case 0x25: 
					case 0x32:
					case 0x33:
					case 0x34:
					case 0x35:
					case 0x37: 
					case 0x40: 
					case 0x42: 
					case 0x51:
					case 0x52:
					case 0x54:
					case 0x55:
					case 0x76: 
					case 0x77: 
					case 0x79: 
					case 0x7F: 
						if (pInBuf[3] != 0x61 && (pInBuf[3] != 0xE1) && (pInBuf[3] != 0xA1) && (pInBuf[3] != 0xB1)) retval=MBUS_INVRESPCODE; break;
				}

				if (retval == MBUS_INVRESPCODE) 
				{	
					OutputDebugString("DLL Modbus: Error 274 \n");
					break;
				}
				switch (pInBuf[3])
				{
					case 0x60: //Polling
					case 0x70: //Polling
					case 0x62: //Answer 0B
					case 0x63: //Answer 4B
					case 0x64: //Answer 72
					case 0x65: //Answer 4C
					case 0x66: //Answer 0B01
					case 0x67:
					case 0x68:
					case 0x69:
						// Currently returns always MBUS_OK
						retval=DDSAnswer(pNet, pInBuf, BytesIn);
						break;				
					case 0x61: 
						retval=MBUS_OK;
						break;				
					case 0xe1:
						retval=MBUS_INVALRSP;
						break;
					case 0xA1: //card number exceeds controller max capacity
						retval=MBUS_EXCEEDS_MEMORY;
						break;
					case 0xB1://card number exceeds controller max capacity - with extended cards memory (lift extended program)
						retval=MBUS_EXCEEDS_EXTENDED_MEMORY;
						break;				
					default:
						retval=MBUS_INVALRSP;
						break;
				}
				break;				
			case 1:
			case 2:
			case 3:
			case 5:
			case 6:
			case 15:
			case 22:
			case 65:
			default:
				retval=MBUS_INVALIDCMD;
				break;
		}
	//}
	//else // if (pNet->iConnectionMode == 1)
	//{
	//	retval = DDSAnswer(pNet, pInBuf, BytesIn);
	//}
	return (retval);
}

int	 WINAPI	InterpretDDSshort (LPCONNECTION pNet, BYTE *pInBuf, DWORD BytesIn)
{
	BYTE	cks1,cks2,cks3;
	DWORD	j, dwTemp;
	int 	retval;

	retval = MBUS_OK;
	
	// Answer must start with 0x41 and end with 0x04
	if ( (pInBuf[0] != 0x41) || (pInBuf[BytesIn-1] != 0x04)  )
		return (MBUS_INVALIDPROTO);
	
	// check if we want this trn to save from the buffer that we cleaned
	switch (pInBuf[3])
	{
		case 0x60: //Polling
		case 0x70: //Polling
		case 0x62: //Answer 0B
		case 0x63: //Answer 4B
		case 0x64: //Answer 72
		case 0x65: //Answer 4C
		case 0x66: //Answer 0B01
		case 0x67: //send alarm zone OR partition status
		case 0x68:
		case 0x69:
			break;				
		default:
			return (MBUS_INVRESPCODE);
	}
	
	// Get the last 2 bytes
	dwTemp = BytesIn - 3;
	// Which is the CRC
	cks1 = pInBuf[dwTemp];
	cks2 = pInBuf[dwTemp+1];
	cks3 = pInBuf[dwTemp+2];

	Calc_CRC_DDS (pNet, pInBuf, dwTemp);

//	#ifdef M_DEBUG
//		mLog(pInBuf, BytesIn, Expected, 0, 0, pNet->iConnectionID, "pInBuf in InterpretDDS after CRC");
//	#endif

	if ((pInBuf[dwTemp] != cks1) || (pInBuf[dwTemp+1] != cks2))
	{
   		pInBuf[dwTemp] = cks1;	// restore original ckecksum bytes
		pInBuf[dwTemp+1] = cks2;
		pInBuf[dwTemp+2] = cks3;
 		return (MBUS_CHECKSUM);
	}

	// if there is an encryption, we have to execute a Xor of the first byte 
	// of the password with all the data bytes of the messages:

	if (pNet->Password.Activated == 1)
	{
		for (j = 3; j < (BytesIn - 3);j++)
		{
			pInBuf[j] = pInBuf[j] ^ pNet->Password.Byte1;
		}
	}
	
	pNet->OutMsg.Length = (int) pInBuf[2];  //the message length - not really needed (because we write all the message to file and donot send back to A5 by function), but we do it like DDSAnswer does.

	//retval=DDSAnswer(pNet, pInBuf, BytesIn);

	return (retval);
}

void WINAPI BufferCharacters (LPCONNECTION pNet, char *pInBuf, int BytesIn, WORD SendorRcv)
{
	WORD	temp;
	int	i;

	if (pNet->hDebugMem == NULL)
		return;

	for (i=0; i<BytesIn; i++)
		{
		temp = (unsigned char)pInBuf[i];
		temp += SendorRcv;
		pNet->pDebugBuf[pNet->DebugIn] = temp;
		pNet->DebugIn++;
		if (pNet->DebugIn == DEBUG_BUFSIZE)
			pNet->DebugIn = 0;
		}
}

/////////////////////////////////////////////////////////////////////////
//
//	int WINAPI mReadFile(
//						HANDLE hFile,                // handle to file
//						LPVOID lpBuffer,             // data buffer
//						DWORD nNumberOfBytesToRead,  // number of bytes to read
//						LPDWORD lpNumberOfBytesRead, // number of bytes read
//						LPOVERLAPPED lpOverlapped,   // overlapped buffer
//						HANDLE OverlappedEvent,
//						DWORD Timeout
//					 ) 
//
//	Called by Control_Proc
//	Read information from the device (serial, TCP or modem)
//
/////////////////////////////////////////////////////////////////////////

int WINAPI mReadFile(
						HANDLE hFile,                // handle to file
						LPVOID lpBuffer,             // data buffer
						DWORD nNumberOfBytesToRead,  // number of bytes to read
						LPDWORD lpNumberOfBytesRead, // number of bytes read
						LPOVERLAPPED lpOverlapped,   // overlapped buffer
						HANDLE OverlappedEvent,
						DWORD Timeout
					 ) 
{
	DWORD nOneStepNumberOfBytesRead;
	DWORD Error;
	int r;
	BOOL retry;
	DWORD dwStart, dwCurrent;

	#ifdef M_DEBUG
		int nRetries = 0;
	#endif
	
	dwStart = GetTickCount();

	*lpNumberOfBytesRead = 0;

	do 
	{
	 	r = ReadFile(	hFile,
						(LPBYTE)lpBuffer + *lpNumberOfBytesRead,
						nNumberOfBytesToRead - *lpNumberOfBytesRead,
						&nOneStepNumberOfBytesRead,
						lpOverlapped
					);
		if (r == 0)  //no success, but it can be pending
		{
			Error = GetLastError();

			if (Error == ERROR_IO_PENDING)  //pending completion asynchronously
			{
				do 
				{
					WaitForSingleObject (OverlappedEvent, Timeout);
					if (HasOverlappedIoCompleted(lpOverlapped)) 
					{
						r = GetOverlappedResult(hFile, lpOverlapped, &nOneStepNumberOfBytesRead, FALSE);
						if (r != 0) 
						{
							break;
						} 
						else 
						{
							Error = GetLastError();
							if (Error != ERROR_IO_INCOMPLETE) 
							{
								CancelIo(hFile);
								break;
							}
						}
					}
					//DebugPrint("read **Time outA**",NULL,DEBUGLEVEL_DETAILED);
					dwCurrent = GetTickCount();
					if ((dwCurrent - dwStart) >= Timeout) 
					{
						//DebugPrint("read **Time outB**",NULL,DEBUGLEVEL_DETAILED);
						CancelIo(hFile);
						break;
					}
				} while(TRUE);
			} 
			else 
			{
				break;
			}
		}

		*lpNumberOfBytesRead += nOneStepNumberOfBytesRead;
		nOneStepNumberOfBytesRead = 0;

		dwCurrent = GetTickCount();

		retry = (	(*lpNumberOfBytesRead < nNumberOfBytesToRead) && ((dwCurrent - dwStart) < Timeout) 	);
		//if (retry) DebugPrint("read retry",NULL,DEBUGLEVEL_DETAILED);

	} while(retry);

	return r;
}

/////////////////////////////////////////////////////////////////////////
//
//	DWORD	WINAPI	Control_Proc (LPVOID InPtr)
//
//	Thread started in AllocateControlThread
//	Main task of the DLL: read and write to the device.
//	2 modes: 
//	pNet->iConnectionMode == 0 polling mode: We are sending a command and waiting for an answer or timeout
//	pNet->iConnectionMode == 1 Event mode: We are waiting for messages until stop conditions
//
/////////////////////////////////////////////////////////////////////////

DWORD	WINAPI	Control_Proc (LPVOID InPtr)
{
	char buffer[255];
	int nbcar;
	LPCONNECTION	pNet;
	DWORD	OutCount, BytesOut, Expected, BytesIn, i, cleanRecvBufCount;
	char	LocalBuf[MAX_BUF_SIZE];
	char	TempLocalBuf[MAX_BUF_SIZE];
	char	SendBuf[MAX_BUF_SIZE];
	int		bResp;
	DWORD	Cevent;
	int		DebugIndx, BytesBuffered;
	DWORD	Error;
	OVERLAPPED	ovR, ovW;
	int counter;
	HANDLE	OverlappedEventW, OverlappedEventR;
	DWORD	currentThreadID;
	char cleanRecvBuf[MAX_BUF_SIZEX4];
	BOOL doReadCleanBuf;
	
	char sBufferPath[MAX_PATH] ;
	WORD	*pValue;
	int			tmpTimeOut;
	int trnCount;

	pNet = (LPCONNECTION)InPtr;

	counter=0;
	OverlappedEventW = CreateEvent (NULL, FALSE, FALSE, NULL);
	OverlappedEventR = CreateEvent (NULL, FALSE, FALSE, NULL);
	doReadCleanBuf=FALSE;
	cleanRecvBufCount=0;
	myGetCurrentPath(sBufferPath); //get the current path of the dll

	while (pNet->idComDev != INVALID_HANDLE_VALUE)
	{
		counter++;
		// wait indefinitely for somthing to happen
		DebugPrint("Control_Proc:Waiting for signaling of object",pNet,DEBUGLEVEL_DETAILED);
		WaitForSingleObject(pNet->ThreadEnable, INFINITE);
		currentThreadID = GetCurrentThreadId();
		
		if (pNet->IsClosing)
			break;
		
		if (pNet->TimeOut < 50)
		{
			pNet->TimeOut=50;

		}
		if (pNet->ShortTimeOut < 50)
		{
			pNet->ShortTimeOut=50;
		}
		
		//pNet->ShortTimeOut = 50; // for tests
		
		// If polling
		if( pNet->iConnectionMode == 0 ) //polling 
		{
			doReadCleanBuf = FALSE;
			cleanRecvBufCount = 0;
			
			if (!(pNet->IsSocket)) 
			{
			// purge the input buffer
				PurgeComm (pNet->idComDev, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
			}else{
					if (pNet->CleanBuffBeforeWriting){
						//cleaning the input buffer.
						ovR.hEvent = OverlappedEventR;
						ovR.Offset = 0;
						ovR.OffsetHigh = 0;
						
						memset(cleanRecvBuf,0x00, MAX_BUF_SIZEX4);
						bResp = mReadFile(pNet->idComDev, cleanRecvBuf, MAX_BUF_SIZEX4, &BytesIn, &ovR, OverlappedEventR, pNet->ShortTimeOut); 
						if (BytesIn > 0){
							doReadCleanBuf = TRUE;
							cleanRecvBufCount = BytesIn;
							
							#ifdef M_DEBUG
								mLog(cleanRecvBuf, BytesIn, Expected, 0, 0, pNet->iConnectionID, "ModbusDLL >>> Before sending The Buffer was cleaned of ");
							#endif	
							sprintf(buffer,"ModbusDLL >>> Before sending The Buffer was cleaned of %u chars \n",BytesIn);
							DebugPrint(buffer,pNet,DEBUGLEVEL_DETAILED);
						}
					}
			}
			if (doReadCleanBuf){
				SaveBufferData(cleanRecvBufCount,cleanRecvBuf,pNet,sBufferPath,1, &trnCount);
			}
			
			memset(TempLocalBuf,0x00,MAX_BUF_SIZE);
			memset(LocalBuf,0x00,MAX_BUF_SIZE);
			Expected = FormatDDS (pNet, LocalBuf, &OutCount); //format the command to controller before sending
			
			// we add it to write commands (especially 09 -clear memory), because we change the timeout in  FormatDDS function 
			switch ((BYTE)LocalBuf[3])
			{
				case 0x81:
				case 0xc1:
				case 0x01:
					tmpTimeOut = pNet->ShortTimeOut;
					break;
				default:
					tmpTimeOut = pNet->TimeOut;
					break;
			}

			if (!pNet->IsSocket && (pNet->OutMsg.BaudRate > 0))
			{
				pNet->SaveBaudeRate = pNet->dwBaudRate;
				SetBaudRate (pNet, pNet->OutMsg.BaudRate);
			}

			if (pNet->DirectConnection)
			{
				// Added manual control of RTS line 4/16/98 
				EscapeCommFunction (pNet->idComDev, SETRTS);
				if (pNet->RTS_Delay[0] != 0)
					Sleep(pNet->RTS_Delay[0]);
				SetCommMask (pNet->idComDev, EV_TXEMPTY);
			}

			// Transmit the msg
			
			//To test if there is ECHO 
			MSGCPY(TempLocalBuf,LocalBuf,(int)Expected);
			memset(SendBuf,0x00,MAX_BUF_SIZE);
			MSGCPY(SendBuf,LocalBuf,MAX_BUF_SIZE);

			// 1/3/00 added support for overlapped operation necessitated by MS TAPI changes
			ovW.hEvent = OverlappedEventW;
			ovW.Offset = 0;
			ovW.OffsetHigh = 0;

			#ifdef M_DEBUG
				mLog(LocalBuf, OutCount, Expected, 0, 0, pNet->iConnectionID, "Send LocalBuf after FormatDDS");
			#endif				
			DebugPrint("Control_Proc:Sending Message to controller",pNet,DEBUGLEVEL_DETAILED);
			if (!WriteFile (pNet->idComDev, LocalBuf, OutCount, &BytesOut, &ovW)) 
			{
				Error = GetLastError();
				if (Error == ERROR_IO_PENDING) 
				{
					WaitForSingleObject (OverlappedEventW, 1000);
					GetOverlappedResult (pNet->idComDev, &ovW, &BytesOut, FALSE);
					ResetEvent (OverlappedEventW);
				}
			}

			if (pNet->DirectConnection)
			{
				// Wait for all the characters to go out
				Cevent = EV_TXEMPTY;
				//WaitCommEvent (pNet->idComDev, &Cevent, NULL);
				WaitCommEvent (pNet->idComDev, &Cevent, &ovW);
				// Before releasing RTS
				if (pNet->RTS_Delay[1] != 0)
					Sleep(pNet->RTS_Delay[1]);
				EscapeCommFunction (pNet->idComDev, CLRRTS);
				// Added July 30, 2000
				// RS-485 modems were echoing transmitted chars
				// and they were being picked up in the receive buffer
				// purge the input buffer
				// PurgeComm (pNet->idComDev, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
			}
			//		Buffer transmit characters
			DebugIndx = pNet->DebugIn;
			BufferCharacters (pNet, LocalBuf, BytesOut, DEBUG_XMIT);
			BytesBuffered = BytesOut;

			if (Expected > 0)
			{
				// if not a broadcast msg
				// wait for the reply
				memset(LocalBuf,0x00,MAX_BUF_SIZE);

				// 1/3/00 added support for overlapped operation
				ovR.hEvent = OverlappedEventR;
				ovR.Offset = 0;
				ovR.OffsetHigh = 0;
				DebugPrint("Reading information from controller",pNet,DEBUGLEVEL_DETAILED);
				bResp = mReadFile(pNet->idComDev, LocalBuf, Expected, &BytesIn, &ovR, OverlappedEventR, tmpTimeOut); //we put here shortTimeOut, if long message from polling we continue to read in POLLING DDS PROTOCOLE 4 CORRECTION

				//Si pas de timeout 
				
				if(Expected == BytesIn) 
				{ 
					//******************************************************************
					//********************* ECHO CORRECTION ****************************
					//******************************************************************
 					if (OutCount >= Expected)   
					{	
						if(MSGCMPR(TempLocalBuf, LocalBuf, Expected))
						{
							DebugPrint("Reading information from controller ECHO Correction All",pNet,DEBUGLEVEL_DETAILED);

							if (OutCount > Expected)   
							{
								//Read the rest of the echo - Expected
								bResp = mReadFile(pNet->idComDev, LocalBuf, OutCount - Expected, &BytesIn, &ovR, OverlappedEventR, pNet->TimeOut);

							}
							//Read the response
							bResp = mReadFile(pNet->idComDev, LocalBuf, Expected, &BytesIn, &ovR, OverlappedEventR, pNet->TimeOut);
						}
					}//end if (OutCount > Expected)   
					else 
					{
						if(MSGCMPR(TempLocalBuf, LocalBuf, OutCount))
						{
							DebugPrint("Reading information from controller ECHO Correction Part",pNet,DEBUGLEVEL_DETAILED);

							//In this case, we have already a part of the response
							//we will save the part of the response in TempLocalBuf
							for (i=0; i < MAX_BUF_SIZE - OutCount ; i++)
							{
								TempLocalBuf[i] = LocalBuf[OutCount + i];
							}
							
							//Read the end of the response in LocalBuf
							bResp = mReadFile(pNet->idComDev, LocalBuf, OutCount, &BytesIn, &ovR, OverlappedEventR, pNet->TimeOut);

							//We connect the pieces of the response in TempLocalBuf
							for (i=0; i < OutCount ;i++)
							{
								TempLocalBuf[Expected - OutCount + i] = LocalBuf[i];
							}
							//We put all the response in LocalBuf
							MSGCPY(LocalBuf, TempLocalBuf,	Expected );
							if (BytesIn == OutCount )
								BytesIn = Expected;
						}
					}

					//end of echo correction

					//******************************************************************
					//************ POLLING DDS PROTOCOLE 4 CORRECTION ******************
					//******************************************************************
					
					memset(TempLocalBuf,0x00,MAX_BUF_SIZE); //
					
					if (Expected==3) //for sure this is a Protocol 4 response
					{
						//if we see that the polling message we get from controller is long, we read here the rest of the message.

						// 0x41 is the start of an answer from the controller
						if  ((BytesIn == Expected) && (((BYTE) LocalBuf[0]) == 0x41))  //no timeout
						{
							Expected=(DWORD) ((BYTE) LocalBuf[2]); //message length
							Expected = Expected + 3; //+3 for CRC and end message 04
							MSGCPY(TempLocalBuf,LocalBuf,3);

							memset(LocalBuf,0x00,MAX_BUF_SIZE);
							DebugPrint("Reading more according to Protocol4",pNet,DEBUGLEVEL_DETAILED);
							bResp = mReadFile(pNet->idComDev, LocalBuf, Expected, &BytesIn, &ovR, OverlappedEventR, pNet->TimeOut);

 							if  (BytesIn == Expected)  //no timeout, we got the all message
							{
								//we connect the pieces of the message in TempLocalBuf
								for (i=0; i < Expected ;i++)
									TempLocalBuf[3 + i] = LocalBuf[i];
								
								Expected = Expected + 3 ;  // +3 pour Entete
								BytesIn = Expected;
								MSGCPY(LocalBuf, TempLocalBuf,	Expected );                   
							}//else if (BytesIn>0) {
								//lets wait for the rest of the message again
							//}

						}
					} //End Proto 4 correction

				} // End of the Echo & Protocole 4 Corrections
				sprintf(buffer, "Finished reading %u  " ,BytesIn);
				DebugPrint(buffer,pNet,DEBUGLEVEL_DETAILED);
				ResetEvent (OverlappedEventR);

				#ifdef M_DEBUG
					mLog(LocalBuf, BytesIn, Expected, 0, 0, pNet->iConnectionID, "Before InterpretDDS");
				#endif	
				
				pNet->Exception = InterpretDDS (pNet, LocalBuf, BytesIn, Expected, SendBuf);
				
				//==============
				//if we have an error 274, 273, or 264 
				// we wait the timeout and clean the buffer 
				// if there were inteferences in the line, there is no point sending again
				// we read upto 50 bytes from line
				if ((pNet->Exception == MBUS_INVRESPCODE) |   (pNet->Exception == MBUS_INVCTRADDR) |   (pNet->Exception == MBUS_INVALIDPROTO) )
				{
					//At HSBC on jan 2010 we increase it to 350 and we put debug info, but did not found how to put the IP address
					if (pNet->IsSocket == FALSE)
					{
						sprintf(buffer,">>> Clean Buffer in DLL (max 350) Error %d  \n",pNet->Exception);
						OutputDebugString(buffer);
					}
					else
					{	
						sprintf(buffer,">>> Clean Buffer in DLL (max 350) Error %d  TCP %d Cmd %d  Received %d %d %d %d\n",pNet->Exception ,pNet->SvrSock, SendBuf[3], LocalBuf[0], LocalBuf[1], LocalBuf[2], LocalBuf[3]  );
						OutputDebugString(buffer);
					}
					//Sleep (pNet->TimeOut);  

					if ((pNet->Exception == MBUS_INVRESPCODE) && (m_DoSaveTrnInverseCode))
					{
						//save the buffer with inversed code, maybe we have there an event
						SaveBufferData(BytesIn,LocalBuf,pNet,sBufferPath,2, &trnCount);
					}

					memset(cleanRecvBuf,0x00, MAX_BUF_SIZEX4);
					nbcar=0; 
					do 
					{
						memset(TempLocalBuf,0x00,MAX_BUF_SIZE); 
						bResp = mReadFile(pNet->idComDev, TempLocalBuf, 1, &BytesIn, &ovR, OverlappedEventR, pNet->ShortTimeOut);
						
						if (BytesIn ==1) {
							cleanRecvBuf[nbcar] = TempLocalBuf[0];
							nbcar++;  //we count the number of chars to be saved late in SaveBufferData function
						}
					}
					while ((BytesIn ==1) && (nbcar < 350));
					// at least one time we will wait the timeout !!
					
					if ((pNet->Exception == MBUS_INVRESPCODE) && (m_DoSaveTrnInverseCode))
					{
						//save the characters read from the buffer with inversed code, maybe we have there an event
						SaveBufferData(nbcar,cleanRecvBuf,pNet,sBufferPath,3, &trnCount);
					}
				}
				//==============


				#ifdef M_DEBUG
					mLog(LocalBuf, BytesIn, Expected, 0, 0, pNet->iConnectionID, "After InterpretDDS");
				#endif	

				BufferCharacters (pNet, LocalBuf, BytesIn, DEBUG_RCV);
				BytesBuffered += BytesIn;
			}
			else
			{
				if( (BYTE) SendBuf[1] == 0x5F )
					Sleep (200); // Delay after broadcast to keep from overrunning next poll request
				if (!pNet->IsSocket && (pNet->OutMsg.BaudRate > 0))
				{
					SetBaudRate (pNet, pNet->SaveBaudeRate);
					pNet->SaveBaudeRate = 0;
				}
			}
			//DebugPrint("Posting back message",pNet);
			// notify the main Application of completed transaction
			pNet->PollInProgress = FALSE;
			if (pNet->hWnd != NULL && pNet->ptr==0) {
				DebugPrint("Posting using Postmessage",pNet, DEBUGLEVEL_DETAILED);
				PostMessage (pNet->hWnd, pNet->Notification, (WPARAM)DebugIndx, (LPARAM)BytesBuffered);

			}
			else
			if (pNet->ptr) {
					DebugPrint("Posting using callback function",pNet,DEBUGLEVEL_DETAILED);
					(pNet->ptr)();
			}
			else
				DebugPrint("Message not posted",pNet, DEBUGLEVEL_DETAILED);
			// go back to sleep 
		} // if( pNet->iConnectionMode == 0 )

		///////////////////////////
		// Event Mode
		///////////////////////
		else		// Else handling event mode
		{
			// pNet->iConnectionMode = 1
			#ifdef M_DEBUG
				mLog(NULL, 0, 0, 0, 0, pNet->iConnectionID, "Start Listen");
			#endif	
			DebugPrint("Control_Proc:Listening for Event",pNet,DEBUGLEVEL_DETAILED);
			ovR.hEvent = OverlappedEventR;
			ovR.Offset = 0;
			ovR.OffsetHigh = 0;
			
			do
			{
				Sleep (5); //av 200
				memset(TempLocalBuf,0x00,MAX_BUF_SIZE);
				memset(LocalBuf,0x00,MAX_BUF_SIZE);
				//for(i = 0;i < MAX_BUF_SIZE;i++)
				//{
				//  LocalBuf[i]='\0';
				//  TempLocalBuf[i]='\0';
				//}					
				Expected = 3;
				bResp = mReadFile(pNet->idComDev, LocalBuf, Expected, &BytesIn, &ovR, OverlappedEventR, pNet->TimeOut);
				
			} while (!pNet->IsClosing && (BytesIn < 3) && (pNet->iConnectionMode == 1) );
				// Listen until network is closed or there is a message pending or the application want to transmit
			// The application may want to change the mode from listing to polling 

//				#ifdef M_DEBUG
//					mLog(LocalBuf, (int)BytesIn, Expected, 0, 0, pNet->iConnectionMode, "Stop Listen");
//				#endif	
			
			if (pNet->IsClosing )
			{
				DebugPrint("IsClosing==TRUE",pNet,DEBUGLEVEL_DETAILED);
				break;
			}
			
			// we need to re-check here because we don't want that in case the mode has passed to polling 
			// to enter into the following code
			if (pNet->iConnectionMode == 1) 
			{
				//if ( (BytesIn == Expected) && ( ((BYTE)LocalBuf[0] == 0x41) || ((BYTE)LocalBuf[0] == 0x40) ) )  //no timeout
				if ( (BytesIn == Expected) )  //no timeout
				{
					Expected = (DWORD) ((BYTE) LocalBuf[2]); //message length
					Expected = Expected + 3; //+3 for CRC and end message 04
					MSGCPY(TempLocalBuf,LocalBuf,3);

					bResp = mReadFile(pNet->idComDev, LocalBuf, Expected, &BytesIn, &ovR, OverlappedEventR, pNet->TimeOut);

 					if  (BytesIn == Expected)  //no timeout
					{
						//we connect the pieces of the message in TempLocalBuf
						for (i=0; i < Expected ;i++)
							TempLocalBuf[3 + i] = LocalBuf[i];
						
						Expected = Expected + 3 ;  // +3 pour Entete
						BytesIn = Expected;
						MSGCPY(LocalBuf, TempLocalBuf,	Expected );
					}
				}

				#ifdef M_DEBUG
					mLog(LocalBuf, BytesIn, Expected, 0, 0, pNet->iConnectionID, "BUS 2");
				#endif	
				
				// if we are receiving the message 56 we prefere answer immediatly OK to the controller
				// before parsing and giving the message to Amadeus5 to reduce response time	
				// We took out verification that first byte = 0x40 because NewEprom from Yehuda send AA for Systal protocol, and old eprom send 40. So we cannot check anything
				if( ((BYTE)LocalBuf[1] == 0xFF) && ((BYTE)LocalBuf[2] == 0x0A) && ((BYTE)LocalBuf[3] == 0x56) && ((BYTE)LocalBuf[BytesIn-1] == 0x04) )
				{
					#ifdef M_DEBUG
						mLog(LocalBuf, BytesIn, Expected, 0, 0, pNet->iConnectionID, "AlarmOnBus2 - AA");
					#endif	
					
					pNet->Exception = DDSAnswer(pNet, LocalBuf, BytesIn);
					if (CheckCRCforBus2(pNet, LocalBuf, BytesIn)== MBUS_CHECKSUM)
						pNet->Exception = MBUS_CHECKSUM;

				}
					else
				{
					for(i = 0;i < MAX_BUF_SIZE;i++)						
					  SendBuf[i]='\0';
					MSGCPY(SendBuf,LocalBuf,MAX_BUF_SIZE);

					pNet->Exception = InterpretDDS (pNet, LocalBuf, BytesIn, Expected, SendBuf);
				}				
				//		Buffer receive characters'
				//		Notify Host App
				BufferCharacters (pNet, LocalBuf, BytesIn, DEBUG_RCV);
				BytesBuffered += BytesIn;
				pNet->PollInProgress = FALSE;
				DebugPrint("Posting back ",pNet,DEBUGLEVEL_DETAILED);
				if (pNet->hWnd != NULL && pNet->ptr==0) {
				PostMessage (pNet->hWnd, pNet->Notification, (WPARAM)DebugIndx, (LPARAM)BytesBuffered);

				}
				if (pNet->ptr) {
					DebugPrint("Calling callback function",pNet,DEBUGLEVEL_DETAILED);
					(pNet->ptr)();
				}
			}
		}
	}

	// signal done
	pNet->ThreadActive = FALSE;
	CloseHandle (OverlappedEventR);
	CloseHandle (OverlappedEventW);
	return (TRUE);
}

void SaveBufferData(DWORD cleanRecvBufCount, char *cleanRecvBuf,LPCONNECTION pNet, char *sBufferPath, int codePart, int *oTrnCount)
{
	char	LocalBuf[MAX_BUF_SIZE];
	int nextMesLength;
	int nextMesPos;
	int i;
	int j;
	int res;

	//--Start - save the data from buffer that was cleaned.
	
	//find trn in cleanRecvBuf
	//trnCount=0;
	nextMesLength=0;
	nextMesPos=0;
	i=0;
	while (i<cleanRecvBufCount)
	{
		//check that it is trn from polling
		if ((cleanRecvBuf[i] == 0x41) && ((i+cleanRecvBuf[i+2]+5) < MAX_BUF_SIZEX4) && (cleanRecvBuf[i + cleanRecvBuf[i+2] + 5] == 0x04))
		{	//found answer.
			nextMesLength=cleanRecvBuf[i+2]; 
			nextMesPos = i+nextMesLength+6;
			if (nextMesLength > 9)
			{
				// it is trn - save in file
				//fill LocalBuf  as if it was receiving buffer
				memset(LocalBuf,0x00,MAX_BUF_SIZE);

				for(j=0;j<nextMesLength+6; j++){
					LocalBuf[j] = cleanRecvBuf[i+j];
				}

				res = InterpretDDSshort (pNet, LocalBuf, nextMesLength+6); //prepare the command read from the buffer, to be sent to A5
				
				if (res == 0) {			//OK
					if ((*oTrnCount) == 1000) {(*oTrnCount) = 0;}
					writeTrnToFile(LocalBuf, nextMesLength+6, (*oTrnCount)++, pNet->iConnectionID,sBufferPath, codePart); 
				}
			}

			nextMesLength=0;
			i=nextMesPos;
			
		}else{ //keep looking for answer.
			i++;
		}
	}
	//--End - save the data from buffer that was cleaned.
}

void DebugPrint(char *msg,LPCONNECTION pNet, int debugLevel)
{
	char buffer[255];
	char time[50];
	int connection;
	SYSTEMTIME SystemTime;
	DWORD i = 0;
	
	

	if (debugLevel <= m_DebugLevel) {
		//GetSystemTime(&SystemTime);
		GetLocalTime (&SystemTime);
    	sprintf(time,"%dh%d %ds %dmilli",SystemTime.wHour,SystemTime.wMinute, SystemTime.wSecond, SystemTime.wMilliseconds);

		/*sprintf(time,"%4d-%02d-%02d %02d-%02d-%02d:  ModbusDLL    ",	SystemTime.wYear,
														SystemTime.wMonth,
														SystemTime.wDay,
														SystemTime.wHour,
														SystemTime.wMinute,
														SystemTime.wSecond,
														SystemTime.wSecond); */

        
		if (pNet!=NULL	){
			
			connection = pNet->iConnectionID;
			sprintf(buffer,"%s ModbusDLL >> ConnectionID%2d  TCP%d  port%d :  %s \n" ,time,connection, pNet->SvrSock, pNet->PortNo ,msg);
			mLog(NULL, 0, 0, 0, 0, connection, buffer);
		}
		
		OutputDebugString(buffer);
	}
}

_declspec(dllexport) void WINAPI setDebugLevel(long DebugLevel)
{
char msg[50];

	
	m_DebugLevel = DebugLevel;

	sprintf(msg,"setDebugLevel:: debugLevel=%d", DebugLevel);
	DebugPrint(msg,0,DEBUGLEVEL_FUNCTIONAL);
	
}

/* NOT IN USE ANY MORE 

 //===========================================================
//=============== PING DEV ==================================
//===========================================================

_declspec(dllexport) void WINAPI MODBUSTCPAlive (HANDLE		hWnd,
												long		msg,
												char *		strAddress,
												long		addressID,
												long		PingTimeout,
												long		WithDebug)
{

	
	struct sTCPInfo *myInfo;
	unsigned long dwAddress;
	HANDLE hMutex;
	char strTemp[255];

	if (WithDebug)
	{
		sprintf(strTemp,"Entered MODBUSTCPAlive %s, %d, %d \n", strAddress, addressID, PingTimeout);
		OutputDebugString(strTemp);
	}

	dwAddress = inet_addr( strAddress);
	myInfo = malloc(sizeof(struct sTCPInfo));
	strcpy(myInfo->address ,strAddress);
	myInfo->hWnd = hWnd;
	myInfo->msg = msg;
	myInfo->addressID = addressID;
	myInfo->PingTimeout = PingTimeout;
	myInfo->WithDebug = WithDebug;
	
	hMutex=CreateEvent(NULL,FALSE,FALSE,NULL);	
	myInfo->hMutex = hMutex;
	AddList(ThreadList,dwAddress, hMutex);	

//	_beginthread( PingAddr, 0, (void *) myInfo);

}


_declspec(dllexport) void WINAPI StopThread(char *		strAddress)
{
	HANDLE threadID;
	BOOL result;
	unsigned long dwAddress;
	char strTemp[255];
	
	dwAddress = inet_addr( strAddress);
	SetContinuePinging(ThreadList,dwAddress, FALSE);
	
	threadID=GetThreadID(ThreadList,dwAddress);
	
	result=SetEvent( threadID);
//	sprintf(strTemp,"Releasing Mutex %x with result %d\n", threadID , result);
//	OutputDebugString(strTemp);

	if (result==FALSE) {
		sprintf(strTemp,"Error %x \n", GetLastError());
		OutputDebugString(strTemp);
	}


}

_declspec(dllexport) void WINAPI setDebugLevel(long DebugLevel)
{
char msg[50];

	
	m_DebugLevel = DebugLevel;

	sprintf(msg,"setDebugLevel:: debugLevel=%d", DebugLevel);
	DebugPrint(msg,0,DEBUGLEVEL_FUNCTIONAL);
	
}

void PingAddr( void * pParam ) {
	int retries;
	DWORD dwRetVal;
	char strTemp[255];
	unsigned long dwAddress;	int OptionPingTimeout;
	char *sDataToSend = "Data Buffer";
	HANDLE hIcmpFile;
	struct sTCPInfo *myInfo;
	LPVOID ReplyBuffer;
	BYTE notifiedTimeout;
	HANDLE hMutex;
	char * myAddress = "";
	DWORD answer;
	
	notifiedTimeout=FALSE;
	myInfo= (struct sTCPInfo *) pParam;

	if ((hIcmpFile = m_IcmpCreateFile()) == INVALID_HANDLE_VALUE)
		if (myInfo->WithDebug) OutputDebugString("\tUnable to open file.\n");
	else
		if (myInfo->WithDebug) OutputDebugString("\tFile created.\n");


	myAddress = myInfo->address;
	dwAddress = inet_addr( myAddress);
	OptionPingTimeout = myInfo->PingTimeout;
	hMutex = myInfo->hMutex;

	if (myInfo->WithDebug)
	{
		sprintf(strTemp,"hMutex is %x for %s \n", hMutex, myAddress);
		OutputDebugString(strTemp);
	}

	ReplyBuffer = (VOID*) malloc(sizeof(ICMP_ECHO_REPLY) + sizeof(sDataToSend)*8 );
	
    if (hIcmpFile) {
		dwRetVal=0;
		retries=1;
		while ( (dwRetVal==0 ) && (GetContinuePinging(ThreadList,dwAddress)) )//&& WaitForSingleObject (hMutex,75)==WAIT_TIMEOUT) 	
		{	
			if (dwRetVal =m_IcmpSendEcho2(	hIcmpFile,
//											NULL,
//											NULL,
//											NULL,
											dwAddress,  
											sDataToSend, 
											sizeof(sDataToSend),
											NULL,
											ReplyBuffer,
											8*sizeof(sDataToSend) + sizeof(ICMP_ECHO_REPLY), 
											OptionPingTimeout))	// in milliseconds
			{
				if (myInfo->WithDebug) sprintf(strTemp,"Ping %s succeeded.\n",myAddress);
				if (myInfo->WithDebug) OutputDebugString(strTemp);
			}
			else {
				if (myInfo->WithDebug) sprintf(strTemp,"Ping %s failed (%d) \n ", myAddress , retries);
				if (myInfo->WithDebug) OutputDebugString(strTemp);
			}
			
			retries++;
			//as retries is an int, we limit the max value to be 30000
			if (retries==30000) 
				retries=1;
			
			answer= WaitForSingleObject (hMutex,500);
			//sprintf(strTemp,"WaitForSingleObject %s return %d \n ", myInfo->address , answer);
			//OutputDebugString(strTemp);
			
		}
	}

	// Post a message back, doesn't  matter if succeed or not
    if (myInfo->hWnd!= NULL && GetContinuePinging(ThreadList,dwAddress)) {
		if (myInfo->WithDebug) sprintf(strTemp,"Posting message back to AM5 with result %d\n",dwRetVal);
		if (myInfo->WithDebug) OutputDebugString(strTemp);
		PostMessage (myInfo->hWnd, myInfo->msg, (WPARAM)myInfo->addressID, (LPARAM)dwRetVal);
	}

	if (GetContinuePinging(ThreadList,dwAddress)== FALSE) 
	{
		if (myInfo->WithDebug) sprintf(strTemp,"Ping %s Stopped.\n",myAddress);
		if (myInfo->WithDebug) OutputDebugString(strTemp);
		PostMessage (myInfo->hWnd, MODBUSDLL_PINGING_STOPED	, (WPARAM)myInfo->addressID, (LPARAM)dwRetVal);
	}

    m_IcmpCloseHandle(hIcmpFile);
	free( myInfo);
	CloseHandle(hMutex);
	RemoveList(ThreadList,dwAddress);
	if (myInfo->WithDebug) sprintf(strTemp,"Thread Closed for %s \n",myAddress);
	if (myInfo->WithDebug) OutputDebugString(strTemp);
}

*/