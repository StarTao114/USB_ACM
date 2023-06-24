#include "usbd_cdc_vcp.h" 
#include "string.h"	
#include "stdarg.h"		 
#include "stdio.h"	 
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32开发板
//usb vcp驱动代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2016/2/24
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 


//USB虚拟串口相关配置参数
LINE_CODING linecoding =
{
//	115200,		//波特率
	921600,
	0x00,   	//停止位,默认1位
	0x00,   	//校验位,默认无
	0x08    	//数据位,默认8位
}; 

u8  USART_PRINTF_Buffer[USB_USART_REC_LEN];	//usb_printf发送缓冲区

//用类似串口1接收数据的方法,来处理USB虚拟串口接收到的数据.
u8 USB_USART_RX_BUF[USB_USART_REC_LEN]; 	//接收缓冲,最大USART_REC_LEN个字节.
//接收状态
//bit15，	接收完成标志
//bit14，	接收到0x0d
//bit13~0，	接收到的有效字节数目
volatile u16 USB_USART_RX_STA=0;       				//接收状态标记	 

 
extern uint8_t  APP_Rx_Buffer [];			//虚拟串口发送缓冲区(发给电脑) 
extern uint32_t APP_Rx_ptr_in;   			//虚拟串口接收缓冲区(接收来自电脑的数据)

//虚拟串口配置函数(供USB内核调用)
CDC_IF_Prop_TypeDef VCP_fops = 
{
	VCP_Init,
	VCP_DeInit,
	VCP_Ctrl,
	VCP_DataTx,
	VCP_DataRx
}; 

//初始化VCP
//返回值:USBD_OK
uint16_t VCP_Init(void)
{ 
	return USBD_OK;
} 
//复位VCP
//返回值:USBD_OK
uint16_t VCP_DeInit(void)
{ 
	return USBD_OK;
} 
//控制VCP的设置
//buf:命令数据缓冲区/参数保存缓冲区
//len:数据长度
//返回值:USBD_OK
uint16_t VCP_Ctrl (uint32_t Cmd, uint8_t* Buf, uint32_t Len)
{ 
	switch (Cmd)
	{
		case SEND_ENCAPSULATED_COMMAND:break;   
		case GET_ENCAPSULATED_RESPONSE:break;  
		case SET_COMM_FEATURE:break;  
		case GET_COMM_FEATURE:break;  
 		case CLEAR_COMM_FEATURE:break;  
		case SET_LINE_CODING:
			linecoding.bitrate = (uint32_t)(Buf[0] | (Buf[1] << 8) | (Buf[2] << 16) | (Buf[3] << 24));
			linecoding.format = Buf[4];
			linecoding.paritytype = Buf[5];
			linecoding.datatype = Buf[6]; 
			//打印配置参数
			printf("linecoding.format:%d\r\n",linecoding.format);
			printf("linecoding.paritytype:%d\r\n",linecoding.paritytype);
			printf("linecoding.datatype:%d\r\n",linecoding.datatype);
			printf("linecoding.bitrate:%d\r\n",linecoding.bitrate);
			break; 
		case GET_LINE_CODING:
			Buf[0] = (uint8_t)(linecoding.bitrate);
			Buf[1] = (uint8_t)(linecoding.bitrate >> 8);
			Buf[2] = (uint8_t)(linecoding.bitrate >> 16);
			Buf[3] = (uint8_t)(linecoding.bitrate >> 24);
			Buf[4] = linecoding.format;
			Buf[5] = linecoding.paritytype;
			Buf[6] = linecoding.datatype; 
			break; 
		case SET_CONTROL_LINE_STATE:break;   
		case SEND_BREAK:break;   
		default:break;  
	} 
	return USBD_OK;
}
//发送一个字节给虚拟串口(发给电脑)
//data:要发送的数据
//返回值:USBD_OK
uint16_t VCP_DataTx (uint8_t data)
{  
	APP_Rx_Buffer[APP_Rx_ptr_in]=data;	//写入发送buf
	APP_Rx_ptr_in++;  					//写位置加1
	
	if(APP_Rx_ptr_in==APP_RX_DATA_SIZE)	//超过buf大小了,归零.
	{
		APP_Rx_ptr_in = 0;
	}   
	return USBD_OK;
} 
//处理从USB虚拟串口接收到的数据
//databuffer:数据缓存区
//Nb_bytes:接收到的字节数.
//返回值:USBD_OK
uint16_t VCP_DataRx (uint8_t* Buf, uint32_t Len)
{
	u8 i;
	u8 res;
	for(i=0;i<Len;i++)
	{  
		res=Buf[i]; 

		if(res == 0xa5 && (USB_USART_RX_STA&0x4000) == 0)			//接收刚开始
		{
			USB_USART_RX_STA = 0;
			USB_USART_RX_STA |= 0x4000;		//接收开始标识
		}
		if(USB_USART_RX_STA&0x4000)
		{
			if((USB_USART_RX_STA&0X3FFF) < MINIPC_FRAME_LENGTH)	//接收未结束	相等的时候已经多一个了
			{
				USB_USART_RX_BUF[USB_USART_RX_STA&0X3FFF]=res;
				USB_USART_RX_STA++;
			}
			else if(USB_USART_RX_BUF[ MINIPC_FRAME_LENGTH-1 ] != 0x5a)
			{
				USB_USART_RX_STA = 0;								//接收错误，重新开始
			}
			else
			{
				USB_USART_RX_STA &= 0xBFFF;	//清除接收开始标识
				USB_USART_RX_STA |= 0x8000;	//接收完成
			}
			if((USB_USART_RX_STA&0X3FFF) > 12u)
			{
				USB_USART_RX_STA = 0;								//接收错误，程序跑飞，重新开始
			}
		}
//		USB_USART_RX_STA = 0;
	}  
	return USBD_OK;
}
//usb虚拟串口,printf 函数
//确保一次发送数据不超USB_USART_REC_LEN字节
void usb_printf(char* fmt,...)  
{  
	u16 i,j;
	va_list ap;
	va_start(ap,fmt);
	vsprintf((char*)USART_PRINTF_Buffer,fmt,ap);
	va_end(ap);
	i=strlen((const char*)USART_PRINTF_Buffer);//此次发送数据的长度
	for(j=0;j<i;j++)//循环发送数据
	{
		VCP_DataTx(USART_PRINTF_Buffer[j]); 
	}
} 



























