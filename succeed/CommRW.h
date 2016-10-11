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

//DEBUG
#define	COMM485//不注释用于485通信，注释用于232通信

#define 	COMM_PORT			COM1//用于默认的COM口
//#define		COMM_PORT_ADD		COM1//用于增加的COM口

//DEBUG

#define TIMEOUT 	20

//-----------------------------------------------------------------------------------------
//读写顺序控制枚举
typedef enum
{
	NotStart = 100,
	RArgument,
	WArgument,
	WCtrlInfo,
	REepromBusy,
	RData,
	WData,
	RWEnd,
} RWCtrlEnum;
//读写eeprom控制枚举
typedef enum
{
	IsReadEeprom,
	IsWriteEeprom,
	IsNotStart,
}RWEepromEnum;
//读写状态控制枚举
typedef enum
{
	KReadWriteWorking,
	KReadWriteCtrlInfoOK,
	KReadWriteDataOK,
	KReadWriteDataErr,
	KReadWriteEepromBusyIdle,
	KReadWriteEepromBusyBusy,
	KReadWriteEepromBusyErr,

	KReadWriteOK,
	KReadWriteErr,
	KReadWriteIdle,
} RWStatusEnum;
//-----------------------------------------------------------------------------------------
//定义帧格式
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
//typedef void(*commx_cb)(int);
typedef void(*comm_rw_finish_cb)(UINT32);
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
/*	函数名：comm_rw_start
	参数：
		第一个参数：从上位机发来的，包含读写操作所需要的参数配置（配置必须正确）
		第二个参数：回调函数，当读写操作完成后，会执行用户的回调，并且会通过传参的方式将数据传过去（回调函数不能为空）
	返回值：无

	功能：通过调用这个接口可以实现“读参数，写参数，读eeprom，写eeprom”四个功能
	注意：第一个参数必须配置正确，回调函数不能为空，且回调中尽量少干事，事越少越好
 *
 */
//-----------------------------------------------------------------------------------------
void comm_rw_start (UINT32, comm_rw_finish_cb);
//-----------------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif//ACTION_COMMRW_H_
