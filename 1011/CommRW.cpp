/*
 * CommRW.cpp
 *
 *  Created on: Aug 31, 2016
 *      Author: hzh
 */
#include "CommRW.h"
#include "comm.h"
#include "platform.h"
#include "Utility.h"

//-----------------------------------------------------------------------------------------
#ifdef	COMM485
static UCHAR	read_buffer[5] = {0};
static UCHAR	response_read_buffer[7] = {0};
static UCHAR	write_buffer[7] = {0};
static UCHAR	response_write_buffer[5] = {0};

#else

static UCHAR	read_buffer[4] = {0};
static UCHAR	response_read_buffer[6] = {0};
static UCHAR	write_buffer[6] = {0};
static UCHAR	response_write_buffer[4] = {0};

#endif
//-----------------------------------------------------------------------------------------
RWOrderCtrlEnum rw_order_ctrl;
RWStatusCtrlEnum rw_status_ctrl;
RWEepromCtrlEnum rw_eeprom_ctrl;
RWSelectCtrlEnum rw_select_ctrl;

const CommInterface* comm_rw;

static int rw_axis_id = 0;//轴号
static int rw_addr = 0;//地址
static int rw_data = 0;//数据
static commx_cb rw_cb = NULL;

static int rw_addr_temp = 0;

//static bool rw_task_test = true;
//-----------------------------------------------------------------------------------------
static void rw_buffer_init (void)
{
	int i = 0;
	for (i = 0; i < sizeof (read_buffer); i++)
		read_buffer[i] = 0;
	for (i = 0; i < sizeof (response_read_buffer); i++)
		response_read_buffer[i] = 0;
	for (i = 0; i < sizeof (write_buffer); i++)
		write_buffer[i] = 0;
	for (i = 0; i < sizeof (response_write_buffer); i++)
		response_write_buffer[i] = 0;
}
//-----------------------------------------------------------------------------------------
static void rw_task_ok_init (void)
{
    rw_axis_id = 0;//轴号
    rw_addr = 0;//地址
    rw_data = 0;//数据
    rw_cb = NULL;

    rw_addr_temp = 0;

    rw_status_ctrl = RWIdle;
    rw_order_ctrl = RWNotStart;
    rw_eeprom_ctrl = IsNotStart;
    rw_select_ctrl = IsNotSelect;

    rw_buffer_init ();

//    rw_task_test = true;
}
//-----------------------------------------------------------------------------------------
static void rw_task_err_init (void)
{
//    rw_axis_id = 0;//轴号
//    rw_addr = 0;//地址
//    rw_data = 0;//数据
//    rw_cb = NULL;

//    rw_addr_temp = 0;

    rw_status_ctrl = RWIdle;
//    rw_order_ctrl = RWNotStart;
//    rw_eeprom_ctrl = IsNotStart;
//    rw_select_ctrl = IsNotSelect;

    rw_buffer_init ();

//    rw_task_test = true;
}
//-----------------------------------------------------------------------------------------
static void w_ctrl_info_ctrl (int data)
{
	switch (rw_eeprom_ctrl)
	{
	case IsREeprom:
		rw_order_ctrl = REepromBusy;//进入读eeprombusy
		break;
	case IsWEeprom:
		rw_order_ctrl = REepromBusy;//进入读eeprombusy
		break;
	default:
		rw_status_ctrl = RWErr;//报错
		break;
	}
}
//-----------------------------------------------------------------------------------------
static void r_eeprombusy_ctrl (int data)
{
	switch (rw_eeprom_ctrl)
	{
	case IsREeprom:
	{
		if (data & 0x08)//为真，表示忙，为假，表示空闲
			rw_order_ctrl = REepromBusy;//忙，说明在执行读进程，继续查询
		else
			rw_order_ctrl = RData;//空闲，说明读进程完成，可以进入读0x96
	}
		break;
	case IsWEeprom:
	{
		if (data & 0x08)
		{
			rw_order_ctrl = REepromBusy;//忙，说明在执行写进程，继续查询
		}
		else
		{
			rw_order_ctrl = RWEnd;//空闲，说明写进程完成，可以执行用户的回调函数了
			rw_cb (data);
			rw_task_ok_init ();
		}
	}
		break;
	default:
		rw_status_ctrl = RWErr;//其他状态，没商量，肯定是哪里错误了，直接置为错误
		break;
	}
}
//-----------------------------------------------------------------------------------------
static void r_data_ctrl (int data)
{
	switch (rw_eeprom_ctrl)
	{
	case IsREeprom:
		rw_order_ctrl = RWEnd;
		rw_cb (data);//执行用户的回调函数，操作完成
		rw_task_ok_init ();
		break;
	default:
		rw_status_ctrl = RWErr;//其他状态，没商量，肯定是哪里错误了，直接置为错误
		break;
	}
}
//-----------------------------------------------------------------------------------------
static void w_data_ctrl (int data)
{
	switch (rw_eeprom_ctrl)
	{
	case IsWEeprom:
		rw_order_ctrl = WCtrlInfo;//执行用户的回调函数
		break;
	default:
		rw_status_ctrl = RWErr;//其他状态，没商量，肯定是哪里错误了，直接置为错误
		break;
	}
}
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
static void r_data_read_cb (UINT16 length)
{
  	ResponseReadFrameStruct *response_read_frame = (ResponseReadFrameStruct *)response_read_buffer;
	UINT16 crc = Crc16 (response_read_buffer, sizeof (response_read_buffer) - 2);
	if (length != sizeof (ResponseReadFrameStruct))
	{
		rw_status_ctrl = RWErr;//收到的数据字节数错误
	}
	else if (response_read_frame->crc0 != (0xff & crc) || response_read_frame->crc1 != (0xff & (crc >> 8)))
	{
		rw_status_ctrl = RWErr;//crc错误
	}
	else
	{
		//得到读出的数据
		rw_data = (0xff & response_read_frame->data1);
		rw_data <<= 8;
		rw_data |= (0xff & response_read_frame->data0);
		switch (rw_order_ctrl)
		{
		case RArgument://是读参数，本次通信完成
			rw_order_ctrl = RWEnd;//置状态
			rw_cb (rw_data);//执行用户的回调函数
			rw_task_ok_init ();
			break;
		case WArgument://不会执行到这
			break;
		case WCtrlInfo://是写0x97的操作，进入写0x97控制
			w_ctrl_info_ctrl (rw_data);
			break;
		case REepromBusy://是读eeprombusy的操作，进入读eeprombusy控制
			r_eeprombusy_ctrl (rw_data);
			break;
		case RData://是读0x96的操作，进入读0x96控制
			r_data_ctrl (rw_data);
			break;
		case WData://不会执行到这
			break;
		default:
			break;
		}
		rw_status_ctrl = RWOK;//本次通信完成
	}
}
//-----------------------------------------------------------------------------------------
static void r_data_send_cb (UINT16 length)
{

}
//-----------------------------------------------------------------------------------------
static void  comm_read_data (void)
{
	//准备报文
	ReadFrameStruct *read_frame = (ReadFrameStruct *)read_buffer;
#ifdef	COMM485
	read_frame->nodeid = 0x00;
#endif
	read_frame->cmd = 0x84 + rw_axis_id;//命令码
	read_frame->addr = rw_addr;//地址码
	UINT16 crc = Crc16 (read_buffer, sizeof (read_buffer) - 2);
	read_frame->crc0 = crc & 0xff;//crc0
	read_frame->crc1 = (crc >> 8) & 0xff;//crc1
	//置状态
	rw_status_ctrl = RWWorking;
	//发送，结束报文
	comm_rw->Send (sizeof (read_buffer), read_buffer, r_data_send_cb);
	comm_rw->Read (sizeof (response_read_buffer), response_read_buffer, TIMEOUT, r_data_read_cb);
}
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
static void w_data_read_cb (UINT16 length)
{
	ResponseWriteFrameStruct *response_write_frame = (ResponseWriteFrameStruct *)response_write_buffer;
	UINT16 crc = Crc16 (response_write_buffer, sizeof (response_write_buffer) - 2);
	if (length != sizeof (ResponseWriteFrameStruct))
	{
		rw_status_ctrl = RWErr;//收到的报文字节数错误
	}
	else if (response_write_frame->crc0 != (0xff & crc) || response_write_frame->crc1 != (0xff & (crc >> 8)))
	{
		rw_status_ctrl = RWErr;//收到的报文crc错误
	}
	else
	{
		switch (rw_order_ctrl)
		{
		case RArgument://不会执行到这
			break;
		case WArgument://是写参数操作，执行用户的回调函数
			rw_order_ctrl = RWEnd;
			rw_cb (rw_data);
			rw_task_ok_init ();
			break;
		case WCtrlInfo://是写0x97操作，进入写0x97控制
			w_ctrl_info_ctrl (rw_data);
			break;
		case REepromBusy://不会执行到这
			break;
		case RData://不会执行到这
			break;
		case WData://是写0x96操作，进入写0x96控制
			w_data_ctrl (rw_data);
			break;
		default:
			break;
		}
		rw_status_ctrl = RWOK;//本次通信完成
	}
}
//-----------------------------------------------------------------------------------------
static void w_data_send_cb (UINT16 length)
{
	//啥也不干
}
//-----------------------------------------------------------------------------------------
static void comm_write_data (void)
{
	//准备报文
	WriteFrameStruct *write_frame = (WriteFrameStruct *)write_buffer;
#ifdef	COMM485
	write_frame->nodeid = 0x00;
#endif
	write_frame->cmd = 0x04 + rw_axis_id;
	write_frame->addr = rw_addr;
	write_frame->data0 = (0xff & rw_data);
	write_frame->data1 = 0xff & (rw_data >> 8);
	UINT16 crc = Crc16 (write_buffer, sizeof (write_buffer) - 2);
	write_frame->crc0 = crc & 0xff;
	write_frame->crc1 = (crc >> 8) & 0xff;
	//置状态
	rw_status_ctrl = RWWorking;
	//发送报文
	comm_rw->Send (sizeof (write_buffer), write_buffer, w_data_send_cb);
	comm_rw->Read (sizeof (response_write_buffer), response_write_buffer, TIMEOUT, w_data_read_cb);
}
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
static void comm_read_argument (void)
{
	rw_order_ctrl = RArgument;//要执行的操作
	comm_read_data ();//开始读参操作
}
//-----------------------------------------------------------------------------------------
static void comm_write_argument (void)
{
	rw_order_ctrl = WArgument;//要执行的操作
	comm_write_data ();//开始读参操作
}
//-----------------------------------------------------------------------------------------
static void comm_read_eeprom (void)
{
	switch (rw_order_ctrl)
	{
		case RWNotStart://是开始读，将操作控制置为 写0x97
		{
			rw_order_ctrl = WCtrlInfo;
			break;
		}
		case RWEnd://上一次操作完成，将操作控制置为 写0x97
		{
			rw_order_ctrl = WCtrlInfo;
			break;
		}
		case WCtrlInfo://写控制信息到0x97
		{
			rw_addr = 0x97;//地址
			rw_data = rw_axis_id | (0x2000 + (rw_addr_temp << 2));//控制信息，读位置1 + 10位地址

			comm_write_data ();//开始读eeprom操作
			break;
		}
		case REepromBusy://查询eeprombusy
		{
			rw_addr = 0x00;//读eeprombusy的地址

			comm_read_data ();//开始读eeprombusy
			break;
		}
		case RData://读0x96
		{
			rw_addr = 0x96;//data地址

			comm_read_data ();//开始读数据
			break;
		}
		default:
			break;
	}//switch
}
//-----------------------------------------------------------------------------------------
static void comm_write_eeprom (void)
{
	switch (rw_order_ctrl)
	{
		case RWNotStart://是第一次操作，将操作控制置为写0x96
		{
			rw_order_ctrl = WData;
			break;
		}
		case RWEnd://上一次通信完成，将操作控制置为写0x96
		{
			rw_order_ctrl = WData;
			break;
		}
		case WData://写0x96
		{
			rw_addr = 0x96;//data地址

			comm_write_argument ();//写开始写0x96
			break;
		}
		case WCtrlInfo://写控制信息到0x97
		{
			rw_addr = 0x97;//地址
			rw_data = rw_axis_id | (0x4000 + (rw_addr_temp << 2));//控制信息，读位置1 + 10位地址

			comm_write_argument ();//开始写0x97
			break;
		}
		case REepromBusy://查询eeprombusy
		{
			rw_addr = 0x00;//eeprombusy地址

			comm_read_data ();//开始读eeprombusy
			break;
		}
		default:
			break;
	}//switch
}
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
void read_argument (const int axis_id, const int addr, commx_cb cb)
{
	if (rw_status_ctrl == RWIdle)
	{
		if (axis_id < 0 || axis_id >= 4 || addr < 0 || addr > 0xb6 || NULL == cb)
		{
			return ;//参数无效
		}
		else
		{
			rw_axis_id = axis_id;//轴号
			rw_addr = addr;//地址
			rw_cb = cb;//回调

			rw_select_ctrl = IsReadArgument;//要执行的操作
		}
	}
	else
	{
		return ;//状态无效
	}
}
//-----------------------------------------------------------------------------------------
void write_argument (const int axis_id, const int addr, const int data, commx_cb cb)
{
	if (rw_status_ctrl == RWIdle)
	{
		if (axis_id < 0 || axis_id >= 4 || addr < 0 || addr > 0xb6 || NULL == cb)
		{
			return ;//参数无效
		}
		else
		{
			rw_axis_id = axis_id;//轴号
			rw_addr = addr;//地址
			rw_data = data;
			rw_cb = cb;//回调

			rw_select_ctrl = IsWriteArgument;//要执行的操作
		}
	}
	else
	{
		return ;//状态无效
	}
}
//-----------------------------------------------------------------------------------------
void read_eeprom (const int axis_id, const int addr, commx_cb cb)
{
	if (rw_status_ctrl == RWIdle)
	{
		if (axis_id < 0 || axis_id >= 4 || addr < 0 || addr > 0xb6 || NULL == cb)
		{
			return ;//参数无效
		}
		else
		{
			rw_axis_id = axis_id;//轴号
			rw_addr_temp = addr;//地址
			rw_cb = cb;//回调

			rw_select_ctrl = IsReadEeprom;//要执行的操作

			rw_eeprom_ctrl = IsREeprom;
		}
	}
	else
	{
		return ;//状态无效
	}
}
//-----------------------------------------------------------------------------------------
void write_eeprom (const int axis_id, const int addr, const int data, commx_cb cb)
{
	if (rw_status_ctrl == RWIdle)
	{
		if (axis_id < 0 || axis_id >= 4 || addr < 0 || addr > 0xb6 || NULL == cb)
		{
			return ;//参数无效
		}
		else
		{
			rw_axis_id = axis_id;//轴号
			rw_addr_temp = addr;//地址
			rw_data = data;
			rw_cb = cb;//回调

			rw_select_ctrl = IsWriteEeprom;

			rw_eeprom_ctrl = IsWEeprom;
		}
	}
	else
	{
		return ;//状态无效
	}
}
//-----------------------------------------------------------------------------------------
static void test_cb (int length)
{
}
//-----------------------------------------------------------------------------------------
void comm_rw_init (void)
{
    CommConfigStruct cfg;
    cfg.databits = kDatabits_8;
    cfg.loopback = kLoopbackOff;
    cfg.parity = kParityEven;
    cfg.stopbits = kStopbits_1;
    cfg.timeout = 255;
    comm_rw = CreatePlatformInterface()->Comm(COMM_PORT);
    comm_rw->Config(115200, cfg);

    rw_axis_id = 0;//轴号
    rw_addr = 0;//地址
    rw_data = 0;//数据
    rw_cb = NULL;

    rw_addr_temp = 0;

    rw_status_ctrl = RWIdle;
    rw_order_ctrl = RWNotStart;
    rw_eeprom_ctrl = IsNotStart;
    rw_select_ctrl = IsNotSelect;

    rw_buffer_init ();
}
//-----------------------------------------------------------------------------------------
static void task_status_ctrl (void)
{
	switch (rw_status_ctrl)
	{
		case RWErr:
//			rw_status_ctrl = RWIdle;
			rw_task_err_init ();
			break;
		default:
			break;
	}
}
//-----------------------------------------------------------------------------------------
static void task_handle (void)
{
	switch (IsReadArgument)
	{
		case IsNotSelect:
		{
			break;
		}
		case IsReadArgument:
		{
			rw_axis_id = 0;
			rw_addr = 0x24;
			rw_cb = test_cb;
			comm_read_argument ();
			break;
		}
		case IsWriteArgument:
		{
			comm_write_argument ();
			break;
		}
		case IsReadEeprom:
		{
			comm_read_eeprom ();
			break;
		}
		case IsWriteEeprom:
		{
			comm_write_eeprom ();
			break;
		}
		default:
		{
			break;
		}
	}
}
//-----------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------
void comm_rw_task_go (void)
{
//	if (rw_task_test)
//	{
//		rw_task_test = false;
//		read_argument (0, 0x24, test_cb);
//	}
//	read_argument (0, 0x24, test_cb);
	task_status_ctrl ();
	task_handle ();
}
//-----------------------------------------------------------------------------------------
const TaskStruct RWTask = {comm_rw_init, comm_rw_task_go};
//-----------------------------------------------------------------------------------------

