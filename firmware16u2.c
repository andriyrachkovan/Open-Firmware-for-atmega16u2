#include "firmware16u2.h"
////////////////////////////////////////////////////	
////////////////////////////////////////////////////	
////////////////////////////////////////////////////	
void SetupHardware_(void)
	{
	MCUSR &= ~(1 << WDRF);
	wdt_disable();
	cli();
	 CLKPR = (1 << CLKPCE);
     CLKPR = 0;
	sei();
	USB_Init_();
	TCCR0B = (1 << CS02);
	AVR_RESET_LINE_PORT_ |= AVR_RESET_LINE_MASK_;
	AVR_RESET_LINE_DDR_  |= AVR_RESET_LINE_MASK_;
	}
////////////////////////////////////////////////////	
////////////////////////////////////////////////////	
////////////////////////////////////////////////////	
int main(void)
	{
	SetupHardware_();
	RingBuffer_InitBuffer_(&USBtoUSART_Buffer_, USBtoUSART_Buffer_Data_, sizeof(USBtoUSART_Buffer_Data_));
	RingBuffer_InitBuffer_(&USARTtoUSB_Buffer_, USARTtoUSB_Buffer_Data_, sizeof(USARTtoUSB_Buffer_Data_));
	sei();//GlobalInterruptEnable_();
	for (;;)
		{
		if (!(RingBuffer_IsFull_(&USBtoUSART_Buffer_)))
			{
			int16_t ReceivedByte = CDC_Device_ReceiveByte_(&my_VirtualSerial_CDC_Interface);
			if (!(ReceivedByte < 0))
			  RingBuffer_Insert_(&USBtoUSART_Buffer_, ReceivedByte);
			}
		uint16_t BufferCount = RingBuffer_GetCount_(&USARTtoUSB_Buffer_);
		if (BufferCount)
			{
			Endpoint_SelectEndpoint_(my_VirtualSerial_CDC_Interface.My_Config.my_DataINEndpoint.my_Address);
			if (Endpoint_IsINReady_())
				{
				uint8_t BytesToSend = MIN_(BufferCount, (my_CDC_TXRX_EPSIZE - 1));
				while (BytesToSend--)
					{
					if (CDC_Device_SendByte_(&my_VirtualSerial_CDC_Interface,RingBuffer_Peek_(&USARTtoUSB_Buffer_)) != 0)break;//
					RingBuffer_Remove_(&USARTtoUSB_Buffer_);
					}
				}
			}
		if (Serial_IsSendReady_() && !(RingBuffer_IsEmpty_(&USBtoUSART_Buffer_)))Serial_SendByte_(RingBuffer_Remove_(&USBtoUSART_Buffer_));
		CDC_Device_USBTask_(&my_VirtualSerial_CDC_Interface);
		USB_DeviceTask_();
		}
	}
////////////////////////////////////////////////////	
////////////////////////////////////////////////////	
////////////////////////////////////////////////////	
ISR(USB_COM_vect, ISR_BLOCK)
	{
	uint8_t PrevSelectedEndpoint = Endpoint_GetCurrentEndpoint_();
	Endpoint_SelectEndpoint_(ENDPOINT_CONTROLEP_);
	USB_INT_Disable_(USB_INT_RXSTPI_);
	GlobalInterruptEnable_();
	USB_Device_ProcessControlRequest_();
	Endpoint_SelectEndpoint_(ENDPOINT_CONTROLEP_);
	USB_INT_Enable_(USB_INT_RXSTPI_);
	Endpoint_SelectEndpoint_(PrevSelectedEndpoint);
	}
ISR(USART1_RX_vect, ISR_BLOCK)
	{
	uint8_t ReceivedByte = UDR1;
	if ((USB_DeviceState_ == DEVICE_STATE_Configured_) && !(RingBuffer_IsFull_(&USARTtoUSB_Buffer_)))
	  RingBuffer_Insert_(&USARTtoUSB_Buffer_, ReceivedByte);	
	}
ISR(USB_GEN_vect, ISR_BLOCK)
	{
	if (USB_INT_HasOccurred_(USB_INT_SOFI_) && USB_INT_IsEnabled_(USB_INT_SOFI_))
		{
		USB_INT_Clear_(USB_INT_SOFI_);
		//EVENT_USB_Device_StartOfFrame();
		}
	if (USB_INT_HasOccurred_(USB_INT_SUSPI_) && USB_INT_IsEnabled_(USB_INT_SUSPI_))
		{
		USB_INT_Disable_(USB_INT_SUSPI_);
		USB_INT_Enable_(USB_INT_WAKEUPI_);
		USB_CLK_Freeze_();
		if (!(USB_Options_ & USB_OPT_MANUAL_PLL_))USB_PLL_Off_();
		USB_DeviceState_ = DEVICE_STATE_Unattached_;
		//EVENT_USB_Device_Disconnect();
		}
	if (USB_INT_HasOccurred_(USB_INT_WAKEUPI_) && USB_INT_IsEnabled_(USB_INT_WAKEUPI_))
		{
		if (!(USB_Options_ & USB_OPT_MANUAL_PLL_))
			{
			USB_PLL_On_();
			while (!(USB_PLL_IsReady_()));
			}
		USB_CLK_Unfreeze_();
		USB_INT_Clear_(USB_INT_WAKEUPI_);
		USB_INT_Disable_(USB_INT_WAKEUPI_);
		USB_INT_Enable_(USB_INT_SUSPI_);
		}
	if (USB_INT_HasOccurred_(USB_INT_EORSTI_) && USB_INT_IsEnabled_(USB_INT_EORSTI_))
		{
		USB_INT_Clear_(USB_INT_EORSTI_);
		USB_DeviceState_                = DEVICE_STATE_Default_;
		USB_INT_Clear_(USB_INT_SUSPI_);
		USB_INT_Disable_(USB_INT_SUSPI_);
		USB_INT_Enable_(USB_INT_WAKEUPI_);
		Endpoint_ConfigureEndpoint_(ENDPOINT_CONTROLEP_, EP_TYPE_CONTROL_,
		                           USB_Device_ControlEndpointSize_, 1);
		USB_INT_Enable_(USB_INT_RXSTPI_);
		}

	}
