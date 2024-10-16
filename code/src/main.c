#include "main.h"

p_function jump_to_application;
uint32_t jump_address;

uint32_t bootloader_exit_flag = 0xFFFFFFFF;
uint32_t bootloader_run_flag = 	((uint32_t)(0xAAAAAAAA));


#define command_respond			0x00
#define	command_readflash		0x01
#define command_writeflash		0x02
#define command_iap				0x03
#define command_boot			0x04

volatile struct {
	enum {
		rx_idle,
		rx_ing,
		rx_done
	} rx_sta;
	uint16_t rx_index;
	uint8_t rx_buffer[5+255];
	uint8_t tx_buffer[5+255];
	uint8_t length;
} pack_status = {
rx_idle
};
uint8_t data_buffer[5+255];


int main(void)
{
	uint8_t command;
	uint32_t address;
	uint8_t length;
	
	if (( *((uint32_t *)flash_addr_boot_info) == bootloader_run_flag) || (!(((*(__IO uint32_t*)flash_addr_app) & 0x2FFE0000) == 0x20000000)))
	{
		uart_init();
		
		LOG("------------------> boot loader <------------------\r\n");

		while (1)
		{
			if (pack_status.rx_sta == rx_done)
			{
				__disable_irq();
				if (pack_check((const uint8_t *)pack_status.rx_buffer) >= 0)
				{
					command = pack_status.rx_buffer[2];
					switch (command)
					{
						case command_readflash:
						{
							if (pack_status.rx_buffer[1] == 5)
							{
								address = pack_status.rx_buffer[3] + (pack_status.rx_buffer[4]*256) + (pack_status.rx_buffer[5]*256*256) + (pack_status.rx_buffer[6]*256*256*256);
								length = pack_status.rx_buffer[7];
								
								if (n32l40x_flash_read_bytes(address, data_buffer, length) >= 0){
									gen_respond_pack((uint8_t *)pack_status.tx_buffer, data_buffer, length, 0x00);
								}
								else{
									gen_respond_pack((uint8_t *)pack_status.tx_buffer, NULL, 0, 0x01);
								}
							}
							else{
								gen_respond_pack((uint8_t *)pack_status.tx_buffer, NULL, 0, 0x01);
							}
							uart_send_bytes((uint8_t *)pack_status.tx_buffer, pack_status.tx_buffer[1] + 5);
							break;
						}
						case command_writeflash:
						{
							if (pack_status.rx_buffer[1] >= 5)
							{
								address = pack_status.rx_buffer[3] + (pack_status.rx_buffer[4]*256) + (pack_status.rx_buffer[5]*256*256) + (pack_status.rx_buffer[6]*256*256*256);
								length = pack_status.rx_buffer[1] - 4;
								
								mymemcopy((const uint8_t *)(pack_status.rx_buffer + 7), data_buffer, length);
								if (n32l40x_flash_write_bytes(address, data_buffer, length) == RT_EOK){
									gen_respond_pack((uint8_t *)pack_status.tx_buffer, NULL, 0, 0x00);
								}
								else{
									gen_respond_pack((uint8_t *)pack_status.tx_buffer, NULL, 0, 0x01);
								}
							}
							else{
								gen_respond_pack((uint8_t *)pack_status.tx_buffer, NULL, 0, 0x01);
							}
							uart_send_bytes((uint8_t *)pack_status.tx_buffer, pack_status.tx_buffer[1] + 5);
							break;
						}
						case command_boot:
						{
							gen_respond_pack((uint8_t *)pack_status.tx_buffer, NULL, 0, 0x00);
							
							if (pack_status.rx_buffer[3] == 0x00)	// set bootloader flag
							{
								/* set boot info */
								if (n32l40x_flash_write_bytes(flash_addr_boot_info, (uint8_t *)&bootloader_run_flag, sizeof(bootloader_run_flag)) != RT_EOK){
									gen_respond_pack((uint8_t *)pack_status.tx_buffer, NULL, 0, 0x01);
								}
							}
							else if (pack_status.rx_buffer[3] == 0x01)	// exit bootloader
							{
								/* set boot info */
								if (n32l40x_flash_write_bytes(flash_addr_boot_info, (uint8_t *)&bootloader_exit_flag, sizeof(bootloader_exit_flag)) != RT_EOK){
									gen_respond_pack((uint8_t *)pack_status.tx_buffer, NULL, 0, 0x01);
								}
								else{
									uart_send_bytes((uint8_t *)pack_status.tx_buffer, pack_status.tx_buffer[1] + 5);
									__NVIC_SystemReset();
								}
							}
							else{
								gen_respond_pack((uint8_t *)pack_status.tx_buffer, NULL, 0, 0x01);
							}
							uart_send_bytes((uint8_t *)pack_status.tx_buffer, pack_status.tx_buffer[1] + 5);
						}
						default:
						{
							gen_respond_pack((uint8_t *)pack_status.tx_buffer, NULL, 0, 0x01);
							uart_send_bytes((uint8_t *)pack_status.tx_buffer, pack_status.tx_buffer[1] + 5);
							break;
						}
					}
				}

				pack_status.rx_sta = rx_idle;
				
				__enable_irq();
			}
		}
	}
	
	jump_to_app(flash_addr_app);
	
	return 0;
}


/**
jump to application
*/
void jump_to_app(uint32_t address)
{
    /* Judge whether the top of stack address is legal or not */
    if (((*(__IO uint32_t*)address) & 0x2FFE0000) == 0x20000000)
    {
        /* Jump to user application */
        jump_address = *(__IO uint32_t*) (address + 4);
        jump_to_application = (p_function) jump_address;
			
        /* Initialize user application's Stack Pointer */
        __set_MSP(*(__IO uint32_t*) address);
        jump_to_application();
    }
}


/*
 * erase one page
 **/
int32_t n32l40x_flash_erase_one_page(uint8_t num)
{
	int32_t err = RT_EOK;
	
	FLASH_Unlock();

	if (FLASH_COMPL != FLASH_EraseOnePage(N32L40X_PAGE_INDEX_2_ADDR(num)))
	{
		err = -RT_ERROR;
	}

	FLASH_Lock();
	
	return err;
}

/*
 * read flash
 **/
int32_t n32l40x_flash_read_bytes(uint32_t start_addr, uint8_t *pdata, uint16_t length)
{
	uint8_t *p_addr = (uint8_t *)start_addr;

	if ((start_addr < N32L40X_PAGE_INDEX_2_ADDR(0)) ||
		((start_addr + length - 1) > (N32L40X_PAGE_INDEX_2_ADDR(N32L40X_PAGE_TOTAL)-1)))
	{
		LOG("err! read flash addr is overflow! addr:%x leng:%x\r\n", start_addr, length);
		return -RT_ERROR;
	}

	while (length > 0)
	{
		*pdata = *(__IO uint8_t*)(p_addr);
		pdata++;
		p_addr++;
		length--;
	}

	return RT_EOK;
}


/*
 * write flash
 **/
static uint8_t flash_buffer[N32L40X_PAGE_SIZE] = {0};
int32_t n32l40x_flash_write_bytes(uint32_t start_addr, uint8_t *pdata, uint16_t length)
{
	uint8_t *p_buffer = RT_NULL;
	uint32_t page_addr_start, counter;
	uint32_t offset;
	uint8_t *p_temp;
	
	/* step1 judge address overflow? */
	if ((start_addr < N32L40X_PAGE_INDEX_2_ADDR(0)) ||
		((start_addr + length - 1) > (N32L40X_PAGE_INDEX_2_ADDR(N32L40X_PAGE_TOTAL)-1)))
	{
		LOG("err! write flash addr is overflow! addr:%x leng:%x\r\n", start_addr, length);
		return -RT_ERROR;
	}


	/* step2 page buffer 2KB */
	p_buffer = flash_buffer;

	/* step3 write */
	while (length)
	{
		memset((void *)p_buffer, 0, N32L40X_PAGE_SIZE);
		
		/* step3.1 get start addr */
		page_addr_start = N32L40X_PAGE_INDEX_2_ADDR(0) + ((start_addr - N32L40X_PAGE_INDEX_2_ADDR(0))/N32L40X_PAGE_SIZE)*N32L40X_PAGE_SIZE;

		/* step3.2 read page data that will be erase */
		if (n32l40x_flash_read_bytes(page_addr_start, p_buffer, N32L40X_PAGE_SIZE) != RT_EOK)
		{
			LOG("err! write flash failed: cannot read!\r\n");
			goto flag_free_err;
		}

		/* step3.3 buffer */
		offset = start_addr - page_addr_start;
		while ((offset < N32L40X_PAGE_SIZE) && (length > 0))
		{
			p_buffer[offset++] = *pdata++;
			length--;
		}

		/* step3.4 init flash clk */
		if(FLASH_HSICLOCK_DISABLE == FLASH_ClockInit())
		{
			LOG("err! write flash failed HSI not yet ready\r\n");
			goto flag_free_err;
		}

		/* step3.5 Unlocks the FLASH Program Erase Controller */
		FLASH_Unlock();

		/* step3.6 Erase */
		if (FLASH_COMPL != FLASH_EraseOnePage(page_addr_start))
		{
			LOG("Flash EraseOnePage Error. Please Deal With This Error Promptly\r\n");
			goto flag_lock_err;
		}

		/* step3.7 Program */
		p_temp = p_buffer;
		for (counter = 0; counter < N32L40X_PAGE_SIZE; counter += 4)
		{
			if (FLASH_COMPL != FLASH_ProgramWord(page_addr_start + counter, *((uint32_t *)p_temp)))
			{		
				LOG("Flash ProgramWord Error.\r\n");
				goto flag_lock_err;
			}
			p_temp += 4;
		}

		/* step3.8 Locks the FLASH Program Erase Controller */
		FLASH_Lock();

		/* step3.9 Check */
		p_temp = p_buffer;
		for (counter = 0; counter < N32L40X_PAGE_SIZE; counter += 4)
		{
			if (*((uint32_t *)p_temp) != (*(__IO uint32_t*)(page_addr_start + counter)))
			{
				LOG("Flash Program Test Failed\r\n");
				goto flag_free_err;
			}
			p_temp += 4;
		}

		/* step3.10 next round */		
		/* offset address */
		start_addr = page_addr_start + N32L40X_PAGE_SIZE;
	}
	
	return RT_EOK;

flag_lock_err:
	/* flash lock */
	FLASH_Lock();
flag_free_err:
	/* free */
	return -RT_ERROR;
}


int fputc(int ch, FILE* f)
{
    USART_SendData(UARTn, (uint8_t)ch);
    while (USART_GetFlagStatus(UARTn, USART_FLAG_TXDE) == RESET);

    return (ch);
}


/*
	uart init
*/
void uart_init(void)
{
	GPIO_InitType gpio_init;
	USART_InitType usart_init;
	NVIC_InitType NVIC_InitStructure;

	/* step 1 enable gpio clk */
	UART_TX_GPIO_CLK_ENABLE();
	UART_RX_GPIO_CLK_ENABLE();

	/* step 2 enable uart clk */
	UARTn_CLK_ENABLE();

	/* step 3 gpio config */
	/* Initialize gpio_init */
	GPIO_InitStruct(&gpio_init);
	/* Configure USARTy Tx as alternate function push-pull */
	gpio_init.Pin            = UART_TX_GPIO_PIN;    
	gpio_init.GPIO_Mode      = GPIO_Mode_AF_PP;
	gpio_init.GPIO_Alternate = UART_TX_GPIO_AF;
	GPIO_InitPeripheral(UART_TX_GPIO_PORT, &gpio_init);
	
	/* Initialize gpio_init */
	GPIO_InitStruct(&gpio_init);
	/* Configure USARTy Rx as alternate function push-pull */
	gpio_init.Pin            = UART_RX_GPIO_PIN;    
	gpio_init.GPIO_Mode      = GPIO_Mode_AF_PP;
	gpio_init.GPIO_Pull      = GPIO_Pull_Up;
	gpio_init.GPIO_Alternate = UART_RX_GPIO_AF;
	GPIO_InitPeripheral(UART_RX_GPIO_PORT, &gpio_init);

	/* step 4 uart config */
	USART_StructInit(&usart_init);
	usart_init.BaudRate            = UARTn_BAUDRATE;
	usart_init.WordLength          = USART_WL_8B;
	usart_init.StopBits            = USART_STPB_1;
	usart_init.Parity              = USART_PE_NO;
	usart_init.HardwareFlowControl = USART_HFCTRL_NONE;
	usart_init.Mode                = USART_MODE_RX | USART_MODE_TX;
	USART_Init(UARTn, &usart_init);

    /* Enable Receive and Transmit interrupts */
    USART_ConfigInt(UARTn, USART_INT_RXDNE, ENABLE);

	/* step 4.1 uart enable */
	USART_Enable(UARTn, ENABLE);


    /* Configure the NVIC Preemption Priority Bits */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);

    /* Enable the USARTy Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel            = UARTn_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd         = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}


void uart_irq_callback(void)
{
	if (USART_GetIntStatus(UARTn, USART_INT_RXDNE) != RESET)
	{
		void byte_receive_callback(uint8_t byte);
		byte_receive_callback(USART_ReceiveData(UARTn));
	}
}


void byte_receive_callback(uint8_t byte)
{
	if ((pack_status.rx_sta == rx_idle) && (byte == 0xAA))
	{
		pack_status.rx_sta = rx_ing;
		pack_status.rx_buffer[0] = 0xAA;
		pack_status.rx_index = 1;
	}
	else if(pack_status.rx_sta == rx_ing)
	{
		pack_status.rx_buffer[pack_status.rx_index++] = byte;
		if (pack_status.rx_index == 2)
		{
			pack_status.length = byte;
		}		
		if (pack_status.rx_index == (pack_status.length + 5))
		{
			if (byte == 0xBB)
			{
				pack_status.rx_sta = rx_done;
			}
			else
			{
				pack_status.rx_sta = rx_idle;
			}
		}
	}
}


int pack_check(const uint8_t *data)
{
	uint8_t length;
	uint8_t crc8_pack;
	
	length = data[1] + 2;
	crc8_pack = data[length+1];
	
	if (crc8(data, length) == crc8_pack)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

// 定义CRC8的多项式
#define CRC8_POLYNOMIAL 0x07

// 计算CRC8校验值
uint8_t crc8(const uint8_t *data, uint8_t length) 
{
	uint8_t crc = 0x00;  // 初始值
	for (uint8_t i = 0; i < length; ++i) {
		uint8_t byte = data[i];
		for (uint8_t j = 0; j < 8; ++j) {
			// 检查最高位是否为1
			if ((crc & 0x80) ^ (byte & 0x80)) {
				crc = (crc << 1) ^ CRC8_POLYNOMIAL;
			} else {
				crc <<= 1;
			}
			// 移动下一个位
			byte <<= 1;
		}
	}
    return crc;
}


void uart_send_bytes(uint8_t *data, uint8_t length)
{
	while (length--)
	{
		USART_SendData(UARTn, (uint8_t)*data++);
		while (USART_GetFlagStatus(UARTn, USART_FLAG_TXDE) == RESET);
	}
}


void gen_respond_pack(uint8_t *tx_buffer, uint8_t *data, uint8_t length, uint8_t err)
{
	uint8_t *temp = tx_buffer;
	uint8_t len = length + 1;
	
	*temp++ = 0xAA;
	*temp++ = len;
	*temp++ = command_respond;
	*temp++ = err;
	
	while(length--)
	{
		*temp++ = *data++;
	}
	
	*temp++ = crc8((const uint8_t *)(tx_buffer + 1), len+2);
	*temp = 0xBB;
}


void mymemcopy(const uint8_t *copy, uint8_t *out, int len)
{
	while (len--)
	{
		*out++ = *copy++;
	}
}
