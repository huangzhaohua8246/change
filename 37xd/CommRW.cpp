//@@ -0,0 +1,599 @@
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
#include "hccommparagenericdef.h"

RWCtrlEnum rw_step;
RWStatusEnum rw_status;
RWEepromEnum is_rw_eeprom;

const CommInterface* comm_rw;

QK4A_PARA rw_recv_data;
QK4A_PARA rw_send_data;

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

static int rw_axis_id = 0;//轴号
static int rw_addr = 0;//地址
static int rw_data = 0;//数据

static commx_cb rw_cb = NULL;
static recv_data_cb recv_cb = NULL;
//-----------------------------------------------------------------------------------------
static void rw_data_cb (int data)
{
	rw_send_data.r.read = 1;
	rw_send_data.r.id = rw_recv_data.s.id;
	rw_send_data.r.addr = rw_recv_data.s.addr;
	rw_send_data.r.data = data;
	recv_cb (rw_send_data.a);
	recv_cb = NULL;
	rw_send_data.a = rw_recv_data.a = 0;
}
//-----------------------------------------------------------------------------------------
//写0x97控制
static void w_ctrl_info_ctrl (int data)
{
	switch (is_rw_eeprom)
	{
	case IsReadEeprom:
		rw_step = REepromBusy;//进入读eeprombusy
		break;
	case IsWriteEeprom:
		rw_step = REepromBusy;//进入读eeprombusy
		break;
	default:
		rw_status = KReadWriteErr;//报错
		break;
	}
}
//-----------------------------------------------------------------------------------------
//读eeprombusy控制
static void r_eeprombusy_ctrl (int data)
{
	switch (is_rw_eeprom)
	{
	case IsReadEeprom:
	{
		if (data & 0x08)//为真，表示忙，为假，表示空闲
			rw_step = REepromBusy;//忙，说明在执行读进程，继续查询
		else
			rw_step = RData;//空闲，说明读进程完成，可以进入读0x96
	}
		break;
	case IsWriteEeprom:
	{
		if (data & 0x08)
		{
			rw_step = REepromBusy;//忙，说明在执行写进程，继续查询
		}
		else
		{
			rw_step = RWEnd;//空闲，说明写进程完成，可以执行用户的回调函数了
			rw_cb (data);
		}
	}
		break;
	default:
		rw_status = KReadWriteErr;//其他状态，没商量，肯定是哪里错误了，直接置为错误
		break;
	}
}
//-----------------------------------------------------------------------------------------
//读0x96控制
static void r_data_ctrl (int data)
{
	switch (is_rw_eeprom)
	{
	case IsReadEeprom:
		rw_step = RWEnd;
		rw_cb (data);//执行用户的回调函数，操作完成
		break;
	default:
		rw_status = KReadWriteErr;//其他状态，没商量，肯定是哪里错误了，直接置为错误
		break;
	}
}
//-----------------------------------------------------------------------------------------
//写0x96控制
static void w_data_ctrl (int data)
{
	switch (is_rw_eeprom)
	{
	case IsWriteEeprom:
		rw_step = WCtrlInfo;//执行用户的回调函数
		break;
	default:
		rw_status = KReadWriteErr;//其他状态，没商量，肯定是哪里错误了，直接置为错误
		break;
	}
}
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//读参数read回调
static void r_argu_read_cb (UINT16 length)
{
  	ResponseReadFrameStruct *response_read_frame = (ResponseReadFrameStruct *)response_read_buffer;
	UINT16 crc = Crc16 (response_read_buffer, sizeof (response_read_buffer) - 2);
	if (length != sizeof (ResponseReadFrameStruct))
	{
		rw_status = KReadWriteErr;//收到的数据字节数错误
	}
	else if (response_read_frame->crc0 != (0xff & crc) || response_read_frame->crc1 != (0xff & (crc >> 8)))
	{
		rw_status = KReadWriteErr;//crc错误
	}
	else
	{
		//得到读出的数据
		rw_data = (0xff & response_read_frame->data1);
		rw_data <<= 8;
		rw_data |= (0xff & response_read_frame->data0);
		switch (rw_step)
		{
		case RArgument://是读参数，本次通信完成
			rw_step = RWEnd;//置状态
			rw_cb (rw_data);//执行用户的回调函数
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
		rw_status = KReadWriteOK;//本次通信完成
	}
}

//-----------------------------------------------------------------------------------------
//读参数send回调
static void r_argu_send_cb (UINT16 length)
{
	//啥也不干
}
//-----------------------------------------------------------------------------------------
//读参数开始
static void  comm_read_argument (void)
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
	rw_status = KReadWriteWorking;
	//发送，结束报文
	comm_rw->Send (sizeof (read_buffer), read_buffer, r_argu_send_cb);
	comm_rw->Read (sizeof (response_read_buffer), response_read_buffer, TIMEOUT, r_argu_read_cb);
#ifdef	COMM_PORT_ADD
//	comm_rw_add->Send (sizeof (read_buffer), read_buffer, r_argu_add_send_cb);
  	comm_rw_add->Read (sizeof (response_read_buffer_add), response_read_buffer_add, TIMEOUT, r_argu_add_read_cb);
#endif
}
 //-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//写参数read回调
static void w_argu_read_cb (UINT16 length)
{
	ResponseWriteFrameStruct *response_write_frame = (ResponseWriteFrameStruct *)response_write_buffer;
	UINT16 crc = Crc16 (response_write_buffer, sizeof (response_write_buffer) - 2);
	if (length != sizeof (ResponseWriteFrameStruct))
	{
		rw_status = KReadWriteErr;//收到的报文字节数错误
	}
	else if (response_write_frame->crc0 != (0xff & crc) || response_write_frame->crc1 != (0xff & (crc >> 8)))
	{
		rw_status = KReadWriteErr;//收到的报文crc错误
	}
	else
	{
		switch (rw_step)
		{
		case RArgument://不会执行到这
			break;
		case WArgument://是写参数操作，执行用户的回调函数
			rw_step = RWEnd;
			rw_cb (rw_data);
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
		rw_status = KReadWriteOK;//本次通信完成
	}
}
//-----------------------------------------------------------------------------------------
//写参数send回调
static void w_argu_send_cb (UINT16 length)
{
	//啥也不干
}
//-----------------------------------------------------------------------------------------
//写参数开始
static void comm_write_argument (void)
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
	rw_status = KReadWriteWorking;
	//发送报文
	comm_rw->Send (sizeof (write_buffer), write_buffer, w_argu_send_cb);
	comm_rw->Read (sizeof (response_write_buffer), response_write_buffer, TIMEOUT, w_argu_read_cb);
}
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//读参数接口
static void read_argument (const int axis_id, const int addr, commx_cb cb)
{
	if (rw_status == KReadWriteIdle)
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

			rw_step = RArgument;//要执行的操作

			comm_read_argument ();//开始读参操作
		}
	}
	else
	{
		return ;//状态无效
	}
}
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//写参数接口
static void write_argument (const int axis_id, const int addr, const int data, commx_cb cb)
{
	if (rw_status == KReadWriteIdle)
	{
		if (axis_id < 0 || axis_id >= 4 || addr < 0 || addr > 0xb6 || NULL == cb)
		{
			return ;//参数无效
		}
		else
		{
			rw_axis_id = axis_id;//轴号
			rw_addr = addr;//地址
			rw_data = data;//数据
			rw_cb = cb;//写参回调

			rw_step = WArgument;//要执行的操作

			comm_write_argument ();//开始写参数操作
		}
	}
	else
	{
		return ;//状态无效
	}
}
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//读eeprom接口
static void read_eeprom (const int axis_id, const int addr, commx_cb cb)
{
	switch (rw_step)
	{
	case NotStart://是开始读，将操作控制置为 写0x97
		rw_step = WCtrlInfo;
		break;
	case RWEnd://上一次操作完成，将操作控制置为 写0x97
		rw_step = WCtrlInfo;
		break;
	case WCtrlInfo://写控制信息到0x97
	{
		if (rw_status == KReadWriteIdle)
		{
			if (axis_id < 0 || axis_id >= 4 || addr < 0 || addr > 0xb6 || NULL == cb)
			{
				return ;//参数无效
			}
			else
			{
				is_rw_eeprom = IsReadEeprom;//是读eeprom

				rw_axis_id = axis_id;//轴号
				rw_addr = 0x97;//地址
				rw_data = axis_id | (0x2000 + (addr << 2));//控制信息，读位置1 + 10位地址
				comm_write_argument ();//开始读eeprom操作
			}
		}
		else
		{
			return ;//状态无效
		}
	}
		break;
	case REepromBusy://查询eeprombusy
	{
		if (rw_status == KReadWriteIdle)
		{
			if (axis_id < 0 || axis_id >= 4 || addr < 0 || addr > 0xb6 || NULL == cb)
			{
				return ;//参数无效
			}
			else
			{
				is_rw_eeprom = IsReadEeprom;//是读eeprom

				rw_axis_id = axis_id;//轴号
				rw_addr = 0x00;//读eeprombusy的地址

				comm_read_argument ();//开始读eeprombusy
			}
		}
		else
		{
			return ;//状态无效
		}
	}
		break;
	case RData://读0x96
	{
		if (rw_status == KReadWriteIdle)
		{
			if (axis_id < 0 || axis_id >= 4 || addr < 0 || addr > 0xb6 || NULL == cb)
			{
				return ;//参数无效
			}
			else
			{
				is_rw_eeprom = IsReadEeprom;//是读eeprom

				rw_axis_id = axis_id;//轴号
				rw_addr = 0x96;//data地址
				rw_cb = cb;//读eeprom回调

				comm_read_argument ();//开始读数据
			}
		}
		else
		{
			return ;//状态无效
		}
	}
		break;
	default:
		break;
	}//switch
}
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//写eeprom接口
static void write_eeprom (const int axis_id, const int addr, const int data, commx_cb cb)
{
	switch (rw_step)
	{
	case NotStart://是第一次操作，将操作控制置为写0x96
		rw_step = WData;
		break;
	case RWEnd://上一次通信完成，将操作控制置为写0x96
		rw_step = WData;
		break;
	case WData://写0x96
	{
		if (rw_status == KReadWriteIdle)
		{
			if (axis_id < 0 || axis_id >= 4 || addr < 0 || addr > 0xb6 || NULL == cb)
			{
				return ;//参数无效
			}
			else
			{
				is_rw_eeprom = IsWriteEeprom;//是写eeprom

				rw_axis_id = axis_id;//轴号
				rw_addr = 0x96;//data地址
				rw_data = data;//要写入的参数值

				comm_write_argument ();//写开始写0x96
			}
		}
		else
		{
			return ;//状态无效
		}
	}
		break;
	case WCtrlInfo://写控制信息到0x97
	{
		if (rw_status == KReadWriteIdle)
		{
			if (axis_id < 0 || axis_id >= 4 || addr < 0 || addr > 0xb6 || NULL == cb)
			{
				return ;//参数无效
			}
			else
			{
				is_rw_eeprom = IsWriteEeprom;//是写0x97

				rw_axis_id = axis_id;//轴号
				rw_addr = 0x97;//地址
				rw_data = axis_id | (0x4000 + (addr << 2));//控制信息，读位置1 + 10位地址
				comm_write_argument ();//开始写0x97
			}
		}
		else
		{
			return ;//状态无效
		}
	}
		break;
	case REepromBusy://查询eeprombusy
	{
		if (rw_status == KReadWriteIdle)
		{
			if (axis_id < 0 || axis_id >= 4 || addr < 0 || addr > 0xb6 || NULL == cb)
			{
				return ;//参数无效
			}
			else
			{
				is_rw_eeprom = IsWriteEeprom;//是写eeprom

				rw_axis_id = axis_id;//轴号
				rw_addr = 0x00;//eeprombusy地址

				rw_cb = cb;//写eeprom回调

				comm_read_argument ();//开始读eeprombusy
			}
		}
		else
		{
			return ;//状态无效
		}
	}
		break;
	default:
		break;
	}//switch
}
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
static void array_init (void)
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
static void rw_init (void)
{
    rw_axis_id = 0;//轴号
    rw_addr = 0;//地址
    rw_data = 0;//数据

    rw_cb = NULL;
//    recv_cb = NULL;


    rw_status = KReadWriteIdle;//状态置为空闲
    is_rw_eeprom = IsNotStart;

    array_init ();
}
void rw_recv (UINT32 a, recv_data_cb cb)
{
//	if (recv_cb == NULL)
//		return ;
//	else
//	{
		rw_recv_data.a = a;
		recv_cb = cb;
//	}
}
//static void test_cb (int length)
//{}
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

//    recv_cb = NULL;

    rw_status = KReadWriteIdle;//状态置为空闲
    rw_step = NotStart;
    is_rw_eeprom = IsNotStart;

    array_init ();

//    rw_recv_data.s.cmd = 4;
//    rw_recv_data.s.id = 0;
//    rw_recv_data.s.addr = 0x00;
}
//-----------------------------------------------------------------------------------------
void comm_rw_task_go (void)
{
	switch (rw_status)
	{
	case KReadWriteWorking:
		break;
	case KReadWriteCtrlInfoOK:
		break;
	case KReadWriteDataOK:
		break;
	case KReadWriteDataErr:
		break;
	case KReadWriteEepromBusyIdle:
		break;
	case KReadWriteEepromBusyBusy:
		break;
	case KReadWriteEepromBusyErr:
		break;
	case KReadWriteOK:
		rw_init ();
		break;
	case KReadWriteErr:
		rw_status = KReadWriteIdle;
		comm_rw_init ();
		break;
	case KReadWriteIdle:
		break;
	default:
		break;
	}
	switch (rw_recv_data.s.cmd)
	{
	case 1://写参数
		write_argument (rw_recv_data.s.id, rw_recv_data.s.addr, rw_recv_data.s.data, rw_data_cb);
		break;
	case 2://读参数
		read_argument (rw_recv_data.s.id, rw_recv_data.s.addr, rw_data_cb);
		break;
	case 3://写eeprom
		write_eeprom (rw_recv_data.s.id, rw_recv_data.s.addr, rw_recv_data.s.data, rw_data_cb);
		break;
	case 4://读eeprom
		read_eeprom (rw_recv_data.s.id, rw_recv_data.s.addr, rw_data_cb);
		break;
	default:
		break;
	}
//	read_argument(0, 0x24, test_cb);
//	read_eeprom(0, 0x00, test_cb);

}
//-----------------------------------------------------------------------------------------
const TaskStruct RWTask = {comm_rw_init, comm_rw_task_go};