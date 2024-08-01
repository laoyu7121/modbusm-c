	
#define MODBUS_ASCII	0
#define MODBUS_RTU		1

#define	MODBUS_DLL_REV	0x0566

/*
560 22.04.2012	open connection async and not sync - set the timeout by setOpenConnectionTimeout
							short timeout for the first 3 chars we get from controller (different from the timeout for all the data we get from controller) -  - set the timeout by setShortTimeout
							instead of deleting the buffer in case of error 274 (inverse code) - we save the events that are in the buffer to files.
							option to read the buffer (and save to files) before we send command to controller - by setCleanBuffBeforeWriting
							change in mReadFile - ((dwCurrent - dwStart) >= Timeout)  and not ((dwCurrent - dwStart) > Timeout)  because if > is used we wait twice the timeout
557 02.03.2010	We added the SetDebugLevel for uc.net
556				we add debug info on ">>> Clean Buffer ..." message at HSBC (Jan 2010)
555 07.12.2009  Reput the waiting after broadcast, becasue it caused problem in installation in China
554 02.11.2009  Recompile the DLL as Guy did not take latest files when he compile it. 
553 16.08.2009	MAX_CONNECTS	255 instead 81 
552 12.01.2009  took out the waiting after brodcast 
551 07.10.2008  Add CheckCRCforBus2 
550 16.06.2008  Mega Support CmdFE = 72 01 04 0F 03 to read how many employee in memory 
549 10.06.2008  Mega Support for two new Msgs Cmd 26 and Cmd 17  
548             by Guy
547	27.11.2007	Added debugprint command in order to support debuggin
546 24.09.2007  bugzID:1577:   MotorComnet=On .  Alarm Priority bus is not working
545				fix bug introduced into v543 

544				by Guy - suppot for new Dot net Com motor
  
543 19.12.2006  Catch error 274 in DLL. Clean the buffer in case of error 274, 273 or 264  

542  9.11.2006  CmdFF = 72 00 0E 02 to read how many employee in memory  

541 31.10.2006  Cmd72 to read controller memory 

540 23.04.2006  Alarm on second bus compatible with new eprom 30.7.

539 14.03.2006  Ping. Some corrections, and changes
  
538 08.02.2006  Cmd 43 for definitions of input group was not send correctly when sending many inputs in a group. 
				
537 18.12.2005  Send 81 01 / C1 01 in case we see 2nd byte of CRC = Protocol (because Ctr Adr 04). 
				See more comment in FormatDDS function

536	21.07.2005	Fixed problem with Window 2000 support for ping and add StopThread, StopPinging
535	23.05.2005	Support Ping through threads (Added MODBUSTCPAlive and StopPinging)
534 18.07.2004  To support new Eprom 01/06/2004 (Holidays Cmd 15 - Send Time Date 31 01) 

533 04.05.2004	Release the thread handle when closing the connection. 

532 10.03.2004	Encryption: Modifications in Calc_CRC_DDS, DDSAnswer, FormatDDS, InterpretDDS
				New function: setPassword
				New structure: _PASSWORDPARAMS
				New parameter in ConnectDDS

531 29.02.2004	We have moved the "SetBaudRate (pNet, pNet->SaveBaudeRate);" command after the broacast sleep to wait 
				until all the message is send in the serial port.

530 26.02.2004	New parameter in MODBUSMSG: BaudRate. Mofify the baude rate in the dll beofre sending a broacast message
				and restore the baudrate after send
				The old baud rate is stored in a pNet variable 'SaveBaudRate'
				New function 'SetBaudRate' to change the baud rate within the dll
				In TCP we are not changing the baud rate in the DLL.

529 11.02.2004	In OpenConnection When com port number > 9 we have to modify the name of the port for CreateFile

528 10.02.2004	MAX_CONNECTS 81	instead of 9
	
527 06.02.2004  In OpenConnection: CreateFile function: replace flag FILE_ATTRIBUTE_NORMAL by FILE_FLAG_OVERLAPPED
				In Control_Proc: 
					Add ResetEvent (OverlappedEventW/S);
					Modify the 3rd param of from NUll to &ovW in WaitCommEvent (pNet->idComDev, &Cevent, &ovW);

526 02.02.2004  MAX_CONNECTS	9 au lieu de 81 

525 15.01.2004	Control_Proc: Increase the broadcast sleep time from 50 to 200
				In Debug mode, we are creating log for each IDconnect: "Modbus_Log_%id.log"

524 29.12.2003	DDSAnswer: Removing the length message check in event mode only check 60 or 70

523 29.12.2003	DDSAnswer: correction of the event mode message length

522 24.12.2003	Removing all the Modbus functions to reduce time
				Removing none used parameter in the _CONNECTION structure (Transparent...)
				When receiving comand 56 use only DDSAnswer and not InterpretDDS

521 22.12.2003	Print 'IdConnect' in debug mode
				In MODBUSResponse we have moved the line "*MaxArraySize = pNet->OutMsg.Length;" before the buf copy
				to avoid copying only one char

520 18.12.2003	We have removed the TCP/IP module MBAPutils.c and the Time exetension ExtTimeFormat.cpp from
				the project. (Modification of the mLog function print
				Event mode: When sending message 57 or 41 we are not expected to receive response from controller
				(in FormatDDS we have returning expected = 0)
				For the command 41 we are removing the data bytes (00 00) in formatDDS
				Reduce the broadcast sleep time from 500 to 100
				Reduce the sleep in Event mode from 200 to 10
				Modification of the mLog function to reduce the print time (using fprintf)
				
519 16.12.2003	Event mode: instead of using the PollBus function using a StartEvent
				New parameter: pNet->iConnectionMode = 1 for event mode and 0 for polling
				New function: StartEventMode
				New function: StopEventMode
				Modification of the InterpretDDS function to introduce pNet->iConnectionMode parameter
				add in WriteMODBUS and PollMODBUS pNet->iConnectionMode = 0
				The MODBUSResponse function parameters has been modified:
					LPWORD MaxArraySize instead of WORD MaxArraySize
					We give back the length of the buf to Amadeus5 through MaxArraySize
					In the ModuleModbus file in Amadeus5, we have also replace from byVal to ByRef.
				DDSAnswer: 
					correction of the length of the response message pNet->OutMsg.Length used to send 
					back to Amadeus
					Add 2 bytes at the start of the response message: the byte '71' and the ctr addr 
					in the 2 event mode messages
				In "InterpretDDS" acceptance of the command 56 from the controller that has first byte '40' 
				instead of '41' .
				When sending the command 57 as an answer to 56 we don't want to wait for a response Expected = 0

518 04.11.2003	Modem is not working at all
				FormatDDS and SetTimeout: in case pNet->Call not null then 
					ReadTotalTimeoutMultiplier = 0;
					WriteTotalTimeoutMultiplier = 0; 	

517 10.10.2003	FormatDDS Com 76 does not took zero out

516 02.10.2003	FormatDDS New command 76 
				Control_Proc: correction of the SendBuf

515 23.09.2003	InterpreteDDS: 
					check the controller address if FF or ctrADD (error 273)
					check if answer command correspond to question command (error 274)
				FormatDDS not cut the timeout by 5 at all
				setTimeout updated

514 14.09.2003	FormatDDS if TCP do not cut the timeout by 5

513 03.08.2003	FormatDDS 0B 01 always 
  
512 15.07.2003	Lost of transaction if exactly same message. 
				Now send 60 / 70 in answer 
				The buffer is clean. (DDSAnswer)

511 25.06.2003	Correction of Crash for TCP due to divison by zero 
				pNet->dwBaudRate was null. Now Update in OpenConnection instead of SetupConnection
				First version in Source safe
  
510 16.06.2003	FormatDDS Timeout changed dynamically, Expected for 0B=3, for 4B=3
				Interpret DDS 
					- Correction of E1 (we were testing on pBuf[4] instead of pBuf[3]	 
					- Add also possibility of having answer to Write commands so Querys are not useful only for Modbus. 
				Calling Convention far Pascal => Winapi 											 

509 13.06.2003	FormatDDS msg 4B put 00 in the byte, and expected 16.

508 10.06.2003	FormatDDS Msg 0B was sent with lot of bytes. Idem for all querys!

507 29.05.2003	Add msg 54 in FormatDDS

506 12.11.2002	Version avec multiprotocolage

505 14.10.2002	Version avec TCP, SetTimeout, SetBaudRate 							

---------------------------------------------------------------------------------

Rev 2.01 Debug mode support
			support for channels 5-9
Rev 3.01 10/10/96 OCX support
Additions for custom command 65
Rev 3.02 11/12/96
Corrected expected byte count for cmd 65 response
Rev 4.00 2/13/97
Added support for sending/recving modem strings
Rev 5.00  12/97
added TAPI & MBAP network support
Rev 5.01 Added extra delays surrounding RTS operation
Rev 5.02 Corrected CRC overwrite error
(Checksum comparison was overwriting checksum in receive string)
Rev 5.03 Changed the end-of-frame determination for
receiving response to transparent messages
Rev 5.04 Added logic to check for correct slave ID
in response message 11/99

*/

#define MAX_CONNECTS	700		// Lantronix is supporting up to 81 com ports

#define MODSVR_PORT 502

#define WM_ENDTHREAD	WM_USER+1
#define WM_TCPMSG		WM_USER+2

#define TCP_TIMEOUT_TICK	1


typedef struct _CONNECTION   
{
	BOOL		DirectConnection;
	BOOL		InUse;
	HANDLE		idComDev;			// Comm Device Handle
	HANDLE		ThreadEnable;		// Event Obj to Activate Send/Rcv Thread
	BOOL		ThreadActive;		// Used to block until Send/Rcv Thread Terminates
	HWND		hWnd;				// AM5 Window to recv notification msgs
	UINT		Notification;		// Notification Msg Definition
	LPARAM		lEvent;				// (Not Used)
	WORD		Protocol;			// Either RTU or ASCII
	int			TimeOut;			// Expected Slave Response Time
	int			TempTimeOut;		// DDS Use only
	int			ShortTimeOut;		// Time out for 3 first bytes when reading from device
	int			OpenConnectionTimeout; //in miliseconds - we double it by 1000 to get microseconds.
	BOOL	CleanBuffBeforeWriting;		// shouldwe clean the tcp line before we write msg to controllers
    DWORD		dwBaudRate;         // DDS Use Only 
	BOOL		PollInProgress;		// Msg Synchronization Flag
	MODBUSMSG	OutMsg;				// Current Msg Out
	int			Exception;			// Error Flag
	HANDLE		hMem;				// Memory allocated on OpenConnection
	LPWORD		pBuf;				// Deallocated on CloseConnection
	HANDLE		hDebugMem;			// Memory allocated on OpenConnection
	LPWORD		pDebugBuf;			// Deallocated on CloseConnection
	int			DebugIn;			// input index into Debug Buffer	
	BYTE		RTS_Delay[2];		// Delays surrounding hardware RTS signal
	BOOL		IsClosing;			// Flag used to shutdown Comm Thread
	//   TAPI Additions  9/97
	HLINE		hLine;				// Handle to a line
	HCALL		hCall;				// Handle to the call
	UINT		ConnectNotifyMsg;	// Used to tell app when connection has been established
	long		idRequest;			// Used for async notifications from TAPI
	long		AsyncReply;
	BOOL		ReplyReceived;
	BOOL		ReplyWait;
	BOOL		RequestWait;
	// TCP/IP Additions 11/97
	SOCKET		SvrSock;			// Socket used for MBAP comm
	HWND		hTCPMsgWin;			// Hidden Window for processing winsock msgs
	WORD		TransactionId;		// MBAP msg identifier
	UINT		TimerId;			// Timeout counter for Server Response
	HANDLE		hTcpBuf;			// Memory allocated on OpenTCPConnection
	LPBYTE		pTcpBuf;			// Deallocated on CloseConnection
	int			TcpIndex;			// Tcp Receive index
	int			TcpMsgStart;		// indexes into debug buffer for Tcp msgs
	int			TcpMsgLen;
	// TCP/IP transparent connection Additions 09/2002
	BOOL		IsSocket;
	int			PortNo;
	// Event mode parameter 08/12/2003
	int			iConnectionMode;	// Used for Event mode: 0 - Pulling 1-Event mode
	// Debug mode parameter 21/12/2003
	int			iConnectionID;		// Used for debug mode dll to keep the idConnect
	PASSWORDPARAMS	Password;		// Password structure 4 bytes + int
	DWORD		SaveBaudeRate;		// Save the baude rate to restore it after broadcast
	HANDLE		TheadID;
	void			 (*ptr) (void);
} CONNECTION, *LPCONNECTION;

struct GlobalData
{
	int			InstanceCtr;
	CONNECTION	net[MAX_CONNECTS];

} DefaultGlobals;



struct  sTCPInfo {
	char	address[100];
//	unsigned long dwAddress;
	//	int		lpfn;
	HWND	hWnd;
	long	msg;
	long	addressID;
	long	PingTimeout;
	long	WithDebug;
	HANDLE	hMutex;
} ;
