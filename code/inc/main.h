#ifndef MAIN_H
#define MAIN_H

#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "n32l40x.h"

#define RT_EOK                          0               /**< There is no error */
#define RT_ERROR                        1               /**< A generic error happens */

#define RT_NULL                         (0)


/* n32l40x flash page 2KB */
#define N32L40X_PAGE_SIZE	(2048)
#define N32L40X_PAGE_INDEX_2_ADDR(n)		((uint32_t)(0x08000000 + N32L40X_PAGE_SIZE*n))
#define N32L40X_PAGE_TOTAL	(64)


#define LOG(...)			printf(__VA_ARGS__)


/**********************************
===================================
N32L406RB flash map (128KB)
page = 2KB
boot code = page0~page2 (6KB)
boot info = page3 (2KB)
app = page4~page63 (120KB)
===================================
0x8000000 -->  ----------
***************|page0***|
***************----------
***************|page1***|
***************----------
***************|page2***|
***************----------
***************|page3***|
***************----------
***************|page4***|
***************----------
***************|......**|
***************----------
***************|page34**|
***************----------
***************|......**|
***************----------
***************|page63**|
***************----------
**********************************/
#define	flash_page_boot_info			(3)
#define	flash_addr_boot_info			N32L40X_PAGE_INDEX_2_ADDR(flash_page_boot_info)

#define	flash_page_app					(4)
#define	flash_addr_app					N32L40X_PAGE_INDEX_2_ADDR(flash_page_app)


/* gpio define : uart tx */
#define UART_TX_GPIO_PORT					GPIOC
#define UART_TX_GPIO_PIN					GPIO_PIN_12
#define UART_TX_GPIO_CLK_ENABLE()			do{RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOC, ENABLE);}while(0)
#define UART_TX_GPIO_CLK_DISABLE()			do{RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOC, DISABLE);}while(0)
#define UART_TX_GPIO_AF						GPIO_AF6_UART5

/* gpio define : uart rx */
#define UART_RX_GPIO_PORT					GPIOD
#define UART_RX_GPIO_PIN					GPIO_PIN_2
#define UART_RX_GPIO_CLK_ENABLE()			do{RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOD, ENABLE);}while(0)
#define UART_RX_GPIO_CLK_DISABLE()			do{RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOD, DISABLE);}while(0)
#define UART_RX_GPIO_AF						GPIO_AF6_UART5

/* uart config define */
#define UARTn							UART5
#define UARTn_BAUDRATE					115200u
#define UARTn_CLK_ENABLE()				do{RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_UART5, ENABLE);}while(0)
#define UARTn_CLK_DISABLE()				do{RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_UART5, DISABLE);}while(0)
#define UARTn_IRQn						UART5_IRQn

#define uart_irq_callback				UART5_IRQHandler

typedef void (*p_function)(void);

void jump_to_app(uint32_t address);
int32_t n32l40x_flash_erase_one_page(uint8_t num);
int32_t n32l40x_flash_write_bytes(uint32_t start_addr, uint8_t *pdata, uint16_t length);
int32_t n32l40x_flash_read_bytes(uint32_t start_addr, uint8_t *pdata, uint16_t length);

uint8_t crc8(const uint8_t *data, uint8_t length);
int pack_check(const uint8_t *data);
void uart_send_bytes(uint8_t *data, uint8_t length);
void gen_respond_pack(uint8_t *tx_buffer, uint8_t *data, uint8_t length, uint8_t err);
void uart_init(void);

void mymemcopy(const uint8_t *copy, uint8_t *out, int len);

#endif
