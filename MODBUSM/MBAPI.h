/*
#ifndef _MBAPIH_
#define _MBAPIH_

#pragma pack(1)

#define MAX_TCP_MSGSIZE		524

//
// The modbus server resides at TCP port 502
//
#define MBAP_PORT	502
#define MBAP_PROTOCOL    0 

//
// All messages over tpcpip are preceeded by a header
//
typedef struct _MBAP_HEADER
{
    unsigned short	TransactId;
    unsigned short	ProtocolType;
    unsigned short	CmdLength;		// Number of bytes in msg - 6
    unsigned char	DestId;
} MBAP_HEADER, *LPMBAP_HEADER;



//
// Poll Request & Response Messages
//
typedef struct _POLL_REQ
{
	MBAP_HEADER		header;
	unsigned char	cmd;
	unsigned char	start_addr_high;
	unsigned char	start_addr_low;
	unsigned char	num_points_high;
	unsigned char	num_points_low;
} POLL_REQ, *LPPOLL_REQ;

typedef struct _POLL_RESP
{
	MBAP_HEADER		header;
	unsigned char	cmd;
	unsigned char	length;	// number of bytes to follow (i.e. number of regs * 2)
	unsigned char	rd_data[1];
} POLL_RESP, *LPPOLL_RESP;


//
// Write single value Message & response
//
typedef struct _WRITE_REQ
{
	MBAP_HEADER		header;
	unsigned char	cmd;
	unsigned char	start_addr_hi;
	unsigned char	start_addr_lo;
	unsigned char	wr_data[1];
} WRITE_REQ, *LPWRITE_REQ;

typedef struct _WRITE_RESP
{
	MBAP_HEADER		header;
	unsigned char	cmd;
	unsigned char	start_addr_hi;
	unsigned char	start_addr_lo;
	unsigned char	wr_data[1];
} WRITE_RESP, *LPWRITE_RESP;

//
// Write Multiple Request & Response
//
typedef struct _WRITE_MULTI_REQ
{
	MBAP_HEADER		header;
	unsigned char	cmd;
	unsigned char	start_addr_hi;
	unsigned char	start_addr_lo;
	unsigned char	num_pts_hi;
	unsigned char	num_pts_lo;
	unsigned char	byte_count;
	unsigned char	wr_data[1];
} WRITE_MULTI_REQ, *LPWRITE_MULTI_REQ;

typedef struct _WRITE_MULTI_RESP
{
	MBAP_HEADER		header;
	unsigned char	cmd;
	unsigned char	start_addr_hi;
	unsigned char	start_addr_lo;
	unsigned char	num_pts_hi;
	unsigned char	num_pts_lo;
} WRITE_MULTI_RESP, *LPWRITE_MULTI_RESP;

//
// Execption response
//
typedef struct _EXCEPTION_RESP
{
	MBAP_HEADER		header;
	unsigned char	cmd;
	unsigned char	exception;
} EXCEPTION_RESP, *LPEXCEPTION_RESP;


#pragma pack()

#endif /* MBAPIH */
