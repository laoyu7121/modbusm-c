/* ------------------------------------------------------------------
	MBAPUTILS.C
		Tcp/ip additions to modbus.dll  11/97

--------------------------------------------------------------------*/
/*
#define TAPI_CURRENT_VERSION 0x00010004

#include <windows.h>
#include <memory.h>
#include <string.h>
#include <tapi.h>
#include "winsock.h"

#include "modbusm.h"
#include "mbglobals.h"
#include "mbapi.h"

extern	struct GlobalData *pGlobal;

HANDLE AllocateNetResources (WORD Protocol, int Timeout);
void ReleaseNetResources (LPCONNECTION	pNet);
LPCONNECTION FindConnect (HANDLE id);
void WINAPI BufferCharacters (LPCONNECTION pNet, char *pInBuf, int BytesIn, WORD SendorRcv);

_declspec(dllexport) LONG WINAPI MBAPWndProc(HWND, UINT, WPARAM, LPARAM);
DWORD	WINAPI	MBAP_Proc (LPVOID InPtr);	//Prototype for MBAP  Thread

 
_declspec(dllexport) HANDLE WINAPI ConnectTCP (struct in_addr SvrIPaddr, LPMBAPPARAMS pMBAPparams)
{
	LPCONNECTION	pNet=NULL;
	HANDLE	idConnect;
	struct sockaddr_in	local;
	HANDLE	hIOThread;
	DWORD	dwThreadID;

	idConnect = AllocateNetResources (pMBAPparams->Protocol, pMBAPparams->Timeout);

	pNet = FindConnect (idConnect);
	if (pNet == NULL)
        return (INVALID_HANDLE_VALUE);

	// get a socket
	pNet->SvrSock = socket (PF_INET, SOCK_STREAM, 0);
	if (pNet->SvrSock == INVALID_SOCKET)
		{
		ReleaseNetResources(pNet);
		return (INVALID_HANDLE_VALUE);
		}

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
	local.sin_port = htons(MODSVR_PORT);
	local.sin_addr.s_addr = SvrIPaddr.s_addr;
	if (connect (pNet->SvrSock, (struct sockaddr FAR *)&local, sizeof(local)) != 0)
		{
		ReleaseNetResources(pNet);
		return (INVALID_HANDLE_VALUE);
		}

	// connection successful
	// allocate transfer buffer
	pNet->hTcpBuf = GlobalAlloc (GHND, MAX_TCP_MSGSIZE);
	if (pNet->hTcpBuf == NULL)
		{
		ReleaseNetResources(pNet);
		return (INVALID_HANDLE_VALUE);
		}

	pNet->pTcpBuf = GlobalLock(pNet->hTcpBuf);
	pNet->TcpIndex = 0;

	// create the Thread & return the CONNECT handle
	// (MBAP_Proc operates as a hidden Window for 
	//  processing WinSock callback messages)
	hIOThread = CreateThread ((LPSECURITY_ATTRIBUTES)NULL,
								0,
								(LPTHREAD_START_ROUTINE)MBAP_Proc,
								(LPVOID)pNet,
								0,
								&dwThreadID);
	if (hIOThread = NULL)
		{
		ReleaseNetResources(pNet);
		return (INVALID_HANDLE_VALUE);
		}
	
	// everything looks good
	// save arguments passed from control app
	pNet->hWnd = pMBAPparams->hWnd;
	pNet->ConnectNotifyMsg = pMBAPparams->ConnectMsg;
	HookRspNotification (idConnect, pNet->hWnd, pMBAPparams->NotifyMsg, 0);
	// notify control app of successful connection
	PostMessage (pNet->hWnd, pNet->ConnectNotifyMsg, TCPCONNECT, 0);
	return (idConnect);
}
*/
/*
int CloseTCPConnection(LPCONNECTION	pNet)
{
	
	if (pNet->SvrSock != INVALID_SOCKET)
		{
		closesocket (pNet->SvrSock);
		PostMessage (pNet->hTCPMsgWin, WM_ENDTHREAD, 0, 0);
		pNet->SvrSock = INVALID_SOCKET;
		}

	ReleaseNetResources(pNet);
	return (MBUS_OK);
}

int PollRemoteSvr (LPCONNECTION	pNet, LPMODBUSMSG	pMsg)
{
	char		buf[MAX_TCP_MSGSIZE];
	LPPOLL_REQ	pRemMsg;
	int			BytesSent;
	WORD		Address;


	// Prepare the MBAP request message
	pRemMsg = (LPPOLL_REQ)&buf;

	pRemMsg->header.TransactId = ++pNet->TransactionId;
	pRemMsg->header.ProtocolType = MBAP_PROTOCOL;
	pRemMsg->header.CmdLength = htons(6);
	pRemMsg->header.DestId = pMsg->SlaveId;
	pRemMsg->cmd = pMsg->CmdId;
	Address = pMsg->Address-1;
	pRemMsg->start_addr_high = (BYTE)((Address >> 8) & 0xff);
	pRemMsg->start_addr_low = (BYTE)(Address & 0xff);
	pRemMsg->num_points_high = (BYTE)((pMsg->Length >> 8) & 0xff);
	pRemMsg->num_points_low = (BYTE)(pMsg->Length & 0xff);

	// send msg to server
	BytesSent = send (pNet->SvrSock, buf, 12, 0);

	// buffer sent characters to debug buffer
	pNet->TcpMsgStart = pNet->DebugIn;
	BufferCharacters (pNet, buf, BytesSent, DEBUG_XMIT);
	pNet->TcpMsgLen = BytesSent;

	if (BytesSent != 12)
		return (MBUS_WRITEFAIL);

	// start timeout timer
	pNet->TimerId = SetTimer (pNet->hTCPMsgWin, TCP_TIMEOUT_TICK, pNet->TimeOut+500, NULL);

	return (MBUS_OK);

}

int RemWriteSingle (LPCONNECTION	pNet, LPMODBUSMSG	pMsg)
{
	char		buf[MAX_TCP_MSGSIZE];
	LPWRITE_REQ pRemMsg;
	int			BytesSent;
	WORD		Address;

	// modbus write request
	pRemMsg = (LPWRITE_REQ)&buf;

	pRemMsg->header.TransactId = ++pNet->TransactionId;
	pRemMsg->header.ProtocolType = MBAP_PROTOCOL;
	pRemMsg->header.CmdLength = htons(6);
	pRemMsg->header.DestId = pMsg->SlaveId;
	pRemMsg->cmd = pMsg->CmdId;
	Address = pMsg->Address-1;
	pRemMsg->start_addr_hi = (BYTE)((Address >> 8) & 0xff);
	pRemMsg->start_addr_lo = (BYTE)(Address & 0xff);

	if (pMsg->CmdId == 5)
		{
		// Write single coil
		if (pNet->pBuf[0])
			pRemMsg->wr_data[0] = 0xff;
		else
			pRemMsg->wr_data[0] = 0;
		pRemMsg->wr_data[1] = 0;
		}
	else
		{
		// Write single holding register
		pRemMsg->wr_data[0] = (BYTE)((pNet->pBuf[0] >> 8) & 0xff);
		pRemMsg->wr_data[1] = (BYTE)(pNet->pBuf[0] & 0xff);
		}

	// send write request to server
	BytesSent = send (pNet->SvrSock, buf, 12, 0);

	// log debug data
	pNet->TcpMsgStart = pNet->DebugIn;
	BufferCharacters (pNet, buf, BytesSent, DEBUG_XMIT);
	pNet->TcpMsgLen = BytesSent;

	if (BytesSent != 12)
		return (MBUS_WRITEFAIL);

	// start timeout
	pNet->TimerId = SetTimer (pNet->hTCPMsgWin, TCP_TIMEOUT_TICK, pNet->TimeOut+500, NULL);
	return (MBUS_OK);
}
	
int RemWriteCoils (LPCONNECTION	pNet, LPMODBUSMSG	pMsg)
{
	char		buf[MAX_TCP_MSGSIZE];
	LPWRITE_MULTI_REQ	pRemMsg;
	int			BytesSent, len, cmdlen;
	int			i,j;
	BYTE		*pCoils, tbyte, mask;
	WORD		Address;

	// modbus write multiple coils command
	pRemMsg = (LPWRITE_MULTI_REQ)&buf;

	pRemMsg->header.TransactId = ++pNet->TransactionId;
	pRemMsg->header.ProtocolType = MBAP_PROTOCOL;
	len = pMsg->Length / 8;
	if ((len * 8) != pMsg->Length)
		++len;	// number of bytes required to contain Length coils
	cmdlen = len + sizeof(WRITE_MULTI_REQ) - 1;
	pRemMsg->header.CmdLength = htons((short)(cmdlen - 6));
	pRemMsg->header.DestId = pMsg->SlaveId;
	pRemMsg->cmd = pMsg->CmdId;
	Address = pMsg->Address-1;
	pRemMsg->start_addr_hi = (BYTE)((Address >> 8) & 0xff);
	pRemMsg->start_addr_lo = (BYTE)(Address & 0xff);
	pRemMsg->num_pts_hi = (BYTE)((pMsg->Length >> 8) & 0xff);
	pRemMsg->num_pts_lo = (BYTE)(pMsg->Length & 0xff);
	pRemMsg->byte_count = len;

 	j = 0; 
	pCoils = &(pRemMsg->wr_data[0]);
	for (i=0; i<pMsg->Length; i++)
		{
		if (j == 0)
			{
			mask = 0x01;
			tbyte = 0;
			}
		else 
			mask = mask << 1;

		if (pNet->pBuf[i] != 0)
			tbyte |= mask;
		
		++j;
		if ((j == 8) || (i == pMsg->Length-1))
			{
			*pCoils++ = tbyte;
			j = 0;
			}
		}

	BytesSent = send (pNet->SvrSock, buf, cmdlen, 0);

	pNet->TcpMsgStart = pNet->DebugIn;
	BufferCharacters (pNet, buf, BytesSent, DEBUG_XMIT);
	pNet->TcpMsgLen = BytesSent;

	if (BytesSent != cmdlen)
		return (MBUS_WRITEFAIL);

	pNet->TimerId = SetTimer (pNet->hTCPMsgWin, TCP_TIMEOUT_TICK, pNet->TimeOut+500, NULL);
	return (MBUS_OK);
}

int RemWriteRegs (LPCONNECTION	pNet, LPMODBUSMSG	pMsg)
{
	char		buf[MAX_TCP_MSGSIZE];
	LPWRITE_MULTI_REQ pRemMsg;
	int			i, j, BytesSent, cmdlen;
	WORD		Address;

	// modbus preset multiple registare command
	pRemMsg = (LPWRITE_MULTI_REQ)&buf;

	pRemMsg->header.TransactId = ++pNet->TransactionId;
	pRemMsg->header.ProtocolType = MBAP_PROTOCOL;
	cmdlen = (pMsg->Length * 2) + sizeof(WRITE_MULTI_REQ) - 1;
	pRemMsg->header.CmdLength = htons((short)(cmdlen-6));
	pRemMsg->header.DestId = pMsg->SlaveId;
	pRemMsg->cmd = pMsg->CmdId;
	Address = pMsg->Address-1;
	pRemMsg->start_addr_hi = (BYTE)((Address >> 8) & 0xff);
	pRemMsg->start_addr_lo = (BYTE)(Address & 0xff);
	pRemMsg->num_pts_hi = (BYTE)((pMsg->Length >> 8) & 0xff);
	pRemMsg->num_pts_lo = (BYTE)(pMsg->Length & 0xff);
	pRemMsg->byte_count = pMsg->Length * 2;

	j = 0;
   	for (i=0; i<pMsg->Length; i++)
		{
		pRemMsg->wr_data[j++] = (BYTE)(pNet->pBuf[i] >> 8) & 0xff;
		pRemMsg->wr_data[j++] = (BYTE)(pNet->pBuf[i] & 0xff);
		}

	BytesSent = send (pNet->SvrSock, buf, cmdlen, 0);

	pNet->TcpMsgStart = pNet->DebugIn;
	BufferCharacters (pNet, buf, BytesSent, DEBUG_XMIT);
	pNet->TcpMsgLen = BytesSent;

	if (BytesSent != cmdlen)
		return (MBUS_WRITEFAIL);

	pNet->TimerId = SetTimer (pNet->hTCPMsgWin, TCP_TIMEOUT_TICK, pNet->TimeOut+500, NULL);
	return (MBUS_OK);

}

// All modbus write requests go here
// sort out the request, format the msg, send to server 
//     & start the timeout timer
int WriteRemoteSvr (LPCONNECTION	pNet, LPMODBUSMSG	pMsg)
{
	switch (pMsg->CmdId)
		{
		case 5:
		case 6:
			return (RemWriteSingle (pNet, pMsg));
			break;
		case 15:
			return (RemWriteCoils (pNet, pMsg));
			break;
		case 16:
			return (RemWriteRegs (pNet, pMsg));
			break;
		default:
			break;
		}
	
	return (MBUS_NOTSUPPORTED);
}

int SendCustomMBAPMsg (LPCONNECTION	pNet)
{
	char		buf[MAX_TCP_MSGSIZE];
	LPPOLL_REQ	pRemMsg;
	int			BytesSent;
	WORD		OutCount;

	OutCount = pNet->TransparentOut - 2;	// remove CRC from User String

	if (OutCount+6 > MAX_TCP_MSGSIZE)
		return (MBUS_WRITEFAIL);	// too big!

	// Prepare the MBAP request message
	pRemMsg = (LPPOLL_REQ)&buf;

	pRemMsg->header.TransactId = ++pNet->TransactionId;
	pRemMsg->header.ProtocolType = MBAP_PROTOCOL;
	pRemMsg->header.CmdLength = htons(OutCount);
	memcpy ((BYTE *)&pRemMsg->header.DestId, pNet->TransparentBuf, OutCount);

	// send msg to server
	BytesSent = send (pNet->SvrSock, buf, OutCount+6, 0);

	// buffer sent characters to debug buffer
	pNet->TcpMsgStart = pNet->DebugIn;
	BufferCharacters (pNet, buf, BytesSent, DEBUG_XMIT);
	pNet->TcpMsgLen = BytesSent;

	if (BytesSent != OutCount+6)
		return (MBUS_WRITEFAIL);

	// start timeout timer
	pNet->TimerId = SetTimer (pNet->hTCPMsgWin, TCP_TIMEOUT_TICK, pNet->TimeOut+500, NULL);

	return (MBUS_OK);

}

//
// MBAP_Proc created to service WinSock callback
// and timer messages.
//
/*
DWORD	WINAPI	MBAP_Proc (LPVOID InPtr)
{
	LPCONNECTION	pNet;
	MSG msgMain;
	WNDCLASS wc;

	pNet = (LPCONNECTION)InPtr;

		wc.lpszMenuName     = NULL;
		wc.lpszClassName    = "MBAPMsgHandler";
		wc.hInstance        = NULL;
		wc.hIcon            = NULL;
		wc.hCursor          = NULL;
		wc.hbrBackground    = NULL;
		wc.style            = 0;
		wc.lpfnWndProc      = MBAPWndProc;
		wc.cbClsExtra       = 0;
		wc.cbWndExtra       = 0;

		RegisterClass(&wc);

	// Create the main window
	if ((pNet->hTCPMsgWin = CreateWindow("MBAPMsgHandler",
								 "",
								 WS_OVERLAPPEDWINDOW,
								 CW_USEDEFAULT, 0,
								 CW_USEDEFAULT, CW_USEDEFAULT,
								 NULL, NULL, NULL, NULL)) == NULL)
		{
		CloseTCPConnection(pNet);
		PostMessage (pNet->hWnd, pNet->ConnectNotifyMsg, TCPDISCONNECT, 0);
		return(0);
		}

	// let me know when new data comes in from the server
	// or if the connection fails
	WSAAsyncSelect (pNet->SvrSock, pNet->hTCPMsgWin, WM_TCPMSG, FD_READ | FD_CLOSE);

//	Do not Show the window but make sure it is updated.
//	ShowWindow(pNet->hTCPMsgWin, SW_SHOW);
	UpdateWindow(pNet->hTCPMsgWin);

	// Main message "pump"
	while (GetMessage((LPMSG) &msgMain, NULL, 0, 0))
	{
	   TranslateMessage((LPMSG) &msgMain);
	   DispatchMessage((LPMSG) &msgMain);
	}

	return(0);
}

//
// Completed response message has been received from the server
//
//
BOOL ProcessSvrResponse (LPCONNECTION pNet)
{
	LPPOLL_RESP		pRemMsg;
	LPEXCEPTION_RESP pExceptMsg;
	int		i,j;
	BYTE	*pCoils, mask;
	WORD	UserBytes;

	pRemMsg = (LPPOLL_RESP)pNet->pTcpBuf;
	
	// make sure the transaction id's match
	if (pRemMsg->header.TransactId != pNet->TransactionId)
		return (FALSE);

	if (pNet->DoTransparent)
		{
		UserBytes = ntohs(pRemMsg->header.CmdLength);
		if (UserBytes > MAX_USERMSG_SIZE)
			{
			pNet->TransparentIn = 0;
			pNet->Exception = MBUS_TIMEOUT;
			return (FALSE);
			}
		pNet->TransparentIn = UserBytes;
		memcpy (pNet->TransparentBuf, (BYTE *)&pRemMsg->header.DestId, UserBytes);
		return (TRUE);
		}

	// check for exception message
	if ((pRemMsg->cmd & 0x80) != 0)
		{
		pExceptMsg = (LPEXCEPTION_RESP)pRemMsg;
		pNet->Exception = pExceptMsg->exception;
		return (TRUE);
		}

	// correct response
	pNet->Exception = MBUS_OK;

	// Read Coils Response Message
	if (pRemMsg->cmd < 3)
		{
		pCoils = &(pRemMsg->rd_data[0]);

		j = 0;
		for (i=0; i<pNet->OutMsg.Length; i++)
			{
			if (j == 0)
				mask = 0x01;
			else 
				mask = mask << 1;
			if ((*pCoils & mask) == mask)
				pNet->pBuf[i] = 1;
			else
				pNet->pBuf[i] = 0;
			++j;
			if (j == 8)
				{
				++pCoils;
				j = 0;
				}
			}
		}
	else if (pRemMsg->cmd < 5)
		{
		// Read Register Response
		j = 0;
		for (i=0; i<pNet->OutMsg.Length; i++)
			{
			pNet->pBuf[i] = pRemMsg->rd_data[j++];
			pNet->pBuf[i] = (pNet->pBuf[i] << 8) + pRemMsg->rd_data[j++];
			}
		}

	return (TRUE);
}

/*
//
// Data has been received from the MBAP Server
//
LONG WINAPI ProcessTCPMsg (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	LPCONNECTION	pNet=NULL;
	int	i;
	int			BytesIn;
	LPMBAP_HEADER	pmbaphdr;
	int			IncomingSize;

	// find the connection associated with the hwnd
	for (i=0; i<MAX_CONNECTS; i++)
		{
		if ((pGlobal->net[i].InUse) && (pGlobal->net[i].hTCPMsgWin == hwnd))
			pNet = &(pGlobal->net[i]);
		}
	// abort if connection not found
	if (pNet == NULL)
		{
		DestroyWindow(hwnd);
		return (0);
		}

	if (lParam == FD_CLOSE)
		{
		//connection has been aborted from server side
		CloseTCPConnection(pNet);
		PostMessage (pNet->hWnd, pNet->ConnectNotifyMsg, TCPDISCONNECT, 0);
		return(0);
		}

	BytesIn = recv (pNet->SvrSock, &(pNet->pTcpBuf[pNet->TcpIndex]), MAX_TCP_MSGSIZE-pNet->TcpIndex, 0);

	BufferCharacters (pNet, &(pNet->pTcpBuf[pNet->TcpIndex]), BytesIn, DEBUG_RCV);
	pNet->TcpMsgLen += BytesIn;

	pNet->TcpIndex += BytesIn;

	if (pNet->TcpIndex < sizeof(MBAP_HEADER))
		return(0);	//keep waiting -- not received msg header yet!

	pmbaphdr = (LPMBAP_HEADER)pNet->pTcpBuf;
	IncomingSize = ntohs(pmbaphdr->CmdLength) + sizeof(MBAP_HEADER) - 1;
	if (pNet->TcpIndex < IncomingSize)
		return (0);	// keep waiting -- not received complete msg

	// reset receive buffer
	pNet->TcpIndex = 0;

	if (!ProcessSvrResponse (pNet))
		return (0);

	// got the correct response	
	KillTimer (hwnd, pNet->TimerId);

	// notify the main Application of completed transaction
	pNet->PollInProgress = FALSE;
	pNet->DoTransparent = FALSE;
	if (pNet->hWnd != NULL)
		PostMessage (pNet->hWnd, pNet->Notification, pNet->TcpMsgStart, pNet->TcpMsgLen);
	
	return (0);

}

LONG WINAPI ProcessTimerMsg (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	LPCONNECTION	pNet=NULL;
	int	i;
	// find the connection associated with the hwnd
	for (i=0; i<MAX_CONNECTS; i++)
		{
		if ((pGlobal->net[i].InUse) && (pGlobal->net[i].hTCPMsgWin == hwnd))
			pNet = &(pGlobal->net[i]);
		}
	// abort if connection not found
	if (pNet == NULL)
		{
		DestroyWindow(hwnd);
		return (0);
		}

	if (wParam == pNet->TimerId)
		{
		KillTimer (hwnd, pNet->TimerId);
		pNet->PollInProgress = FALSE;
		pNet->DoTransparent = FALSE;
		pNet->TransparentIn = 0;
		pNet->Exception = MBUS_TIMEOUT;
		PostMessage (pNet->hWnd, pNet->Notification, 0,0);
		}
	return (0);
}

//
// Windows Message processing for MBAP_Proc Thread
_declspec(dllexport) LONG WINAPI MBAPWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

	switch (msg)
		{
		case WM_ENDTHREAD: DestroyWindow(hwnd);
							return (0);
		case WM_TCPMSG:	return (ProcessTCPMsg (hwnd, msg, wParam, lParam));
		case WM_TIMER:	return (ProcessTimerMsg (hwnd, msg, wParam, lParam));
		default:
			break;
		}
	return(DefWindowProc(hwnd, msg, wParam, lParam));
}
*/