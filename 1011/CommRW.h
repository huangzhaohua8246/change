/*
 * CommRW.h
 *
 *  Created on: Aug 31, 2016
 *      Author: hzh
 */
#ifndef COMMRW_H_
#define COMMRW_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "Task.h"
#include "datatype.h"
extern const TaskStruct RWTask;

#define		COMM485//不注释用于485通信，注释用于232通信
#define 	COMM_PORT			COM1//用于默认的COM口

#define TIMEOUT 	20
//-----------------------------------------------------------------------------------------
//读写顺序控制枚举
typedef enum
{
	RWNotStart = 100,
	RArgument,
	WArgument,
	WCtrlInfo,
	REepromBusy,
	RData,
	WData,
	RWEnd,
} RWOrderCtrlEnum;
//读写eeprom控制枚举
typedef enum
{
	IsNotStart,
	IsREeprom,
	IsWEeprom,
}RWEepromCtrlEnum;
//读写状态控制枚举
typedef enum
{
	RWWorking,
	RWCtrlInfoOK,
	RWDataOK,
	RWDataErr,
	RWEepromBusyIdle,
	RWEepromBusyBusy,
	RWEepromBusyErr,

	RWOK,
	RWErr,
	RWIdle,
} RWStatusCtrlEnum;
typedef enum
{
	IsNotSelect,
	IsReadArgument,
	IsWriteArgument,
	IsReadEeprom,
	IsWriteEeprom,
} RWSelectCtrlEnum;

typedef struct
{

#ifdef	COMM485
	UCHAR nodeid;
#endif

	UCHAR cmd;
	UCHAR addr;
	UCHAR crc0;
	UCHAR crc1;
} ReadFrameStruct;//读报文
typedef struct
{

#ifdef	COMM485
	UCHAR nodeid;
#endif

	UCHAR cmd;
	UCHAR addr;
	UCHAR data0;
	UCHAR data1;
	UCHAR crc0;
	UCHAR crc1;
} ResponseReadFrameStruct;//应答读报文
typedef struct
{

#ifdef	COMM485
	UCHAR nodeid;
#endif

	UCHAR cmd;
	UCHAR addr;
	UCHAR data0;
	UCHAR data1;
	UCHAR crc0;
	UCHAR crc1;
} WriteFrameStruct;//写报文
typedef struct
{

#ifdef	COMM485
	UCHAR nodeid;
#endif

	UCHAR cmd;
	UCHAR addr;
	UCHAR crc0;
	UCHAR crc1;
} ResponseWriteFrameStruct;//应答写报文
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
typedef void(*commx_cb)(int);
//-----------------------------------------------------------------------------------------
/* 接口说明：
 * 以下接口，如果参数不对，程序会直接结束
 * 回调函数不能为空，只要回调函数被执行，说明操作成功，没有被执行，说明操作失败
 * 对于读参数和读eeprom，读出的数据为用户回调函数的参数
 * ！！！！！！！！！注意：回调函数尽量少干事！！！！！！！！！！！！！！！
 * */
//-----------------------------------------------------------------------------------------
void read_argument (int/*轴号*/, int/*地址*/, commx_cb/*用户的回调*/);
//-----------------------------------------------------------------------------------------
void write_argument (int/*轴号*/, int/*地址*/, int/*要写入的数据*/, commx_cb/*用户的回调*/);
//-----------------------------------------------------------------------------------------
void read_eeprom (int/*轴号*/, int/*地址*/, commx_cb/*用户的回调*/);
//-----------------------------------------------------------------------------------------
void write_eeprom (int/*轴号*/, int/*地址*/, int/*要写入的数据*/, commx_cb/*用户的回调*/);
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif

#endif//ACTION_COMMRW_H_
