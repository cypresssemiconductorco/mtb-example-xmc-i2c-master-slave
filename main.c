/*******************************************************************************
* File Name:   main.c
*
* Description: This code example demonstrates the implementation of an I2C 
*              master and an I2C slave using the Universal Serial Interface
*              Channel (USIC) module available in XMC MCUs. It configures one
*              USIC module as I2C master and another one as I2C slave on the
*              same XMC MCU using the device configurator. I2C master module
*              sends commands to the I2C slave module to toggle the LEDs present
*              on XMC development kit. User has to make the connection between
*              I2C master and I2C slave externally.
*
* Related Document: See README.md
*
********************************************************************************
*
* Copyright (c) 2015-2021, Infineon Technologies AG
* All rights reserved.                        
*                                             
* Boost Software License - Version 1.0 - August 17th, 2003
* 
* Permission is hereby granted, free of charge, to any person or organization
* obtaining a copy of the software and accompanying documentation covered by
* this license (the "Software") to use, reproduce, display, distribute,
* execute, and transmit the Software, and to prepare derivative works of the
* Software, and to permit third-parties to whom the Software is furnished to
* do so, all subject to the following:
* 
* The copyright notices in the Software and this entire statement, including
* the above license grant, this restriction and the following disclaimer,
* must be included in all copies of the Software, in whole or in part, and
* all derivative works of the Software, unless such copies or derivative
* works are solely in the form of machine-executable object code generated by
* a source language processor.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
* SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
* FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*                                                                              
*****************************************************************************/

#include "cybsp.h"
#include "cy_utils.h"

/*******************************************************************************
* Defines
*******************************************************************************/
/* SysTick timer frequency in Hz  */
#define TICKS_PER_SECOND           1000

/* I2C Master sends every 500ms a command to toggle LED on slave */
#define I2C_MASTER_SEND_TASK_MS    500

/* 8-bit command patterns to set LED port on slave to high/low */
#define CMD_LED_HIGH               0xaa
#define CMD_LED_LOW                0x55

/* I2C receive event interrupt priority */
#define I2C_RECEIVE_EVENT_PRIORITY 63

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* Variable for keeping track of time */
static volatile uint32_t ticks = 0;

/*******************************************************************************
* Function Name: SysTick_Handler
********************************************************************************
* Summary:
* This is the interrupt handler function for the SysTick timer interrupt.
* It counts the time elapsed in milliseconds since the timer started.
* Every I2C_MASTER_SEND_TASK_MS milliseconds, an I2C-command is sent from
* I2C-master to I2C-slave to toggle a LED.
*
* Parameters:
*  none
*
* Return:
*  none
*
*******************************************************************************/
void SysTick_Handler(void)
{
    static uint8_t tx_master_send = CMD_LED_LOW;
    static uint32_t ticks = 0;

    ticks++;
    if (ticks == I2C_MASTER_SEND_TASK_MS)
    {
        /* Prepare command to toggle LED on I2C slave */
        switch (tx_master_send)
        {
            case CMD_LED_LOW:
            {
                tx_master_send = CMD_LED_HIGH;
            }
            break;
            case CMD_LED_HIGH:
            {
                tx_master_send = CMD_LED_LOW;
            }
            break;
        }

        /* Send START conditon */
        XMC_I2C_CH_MasterStart(I2C_MASTER_HW, I2C_SLAVE_SLAVE_ADDRESS, XMC_I2C_CH_CMD_WRITE);

        /* Wait for acknowledge and reset status */
        while((XMC_I2C_CH_GetStatusFlag(I2C_MASTER_HW) & XMC_I2C_CH_STATUS_FLAG_ACK_RECEIVED) == 0U)
        {
            /* wait for ACK from slave */
        }

        XMC_I2C_CH_ClearStatusFlag(I2C_MASTER_HW,(uint32_t)XMC_I2C_CH_STATUS_FLAG_ACK_RECEIVED);

        /* Transmit next command from I2C master to I2C slave */
        XMC_I2C_CH_MasterTransmit(I2C_MASTER_HW, tx_master_send);

        /* Wait for acknowledge and reset status */
        while((XMC_I2C_CH_GetStatusFlag(I2C_MASTER_HW) & XMC_I2C_CH_STATUS_FLAG_ACK_RECEIVED) == 0U)
        {
            /* wait for ACK from slave */
        }

        XMC_I2C_CH_ClearStatusFlag(I2C_MASTER_HW,(uint32_t)XMC_I2C_CH_STATUS_FLAG_ACK_RECEIVED);

        /* Wait until TX FIFO is empty */
        while (!XMC_USIC_CH_TXFIFO_IsEmpty(I2C_MASTER_HW))
        {
            /* wait until all data is sent by HW */
        }

        /* Send STOP conditon */
        XMC_I2C_CH_MasterStop(I2C_MASTER_HW);

        /* Reset ticks counter */
        ticks = 0;
    }
}

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function.
* The I2C master and slave interface are initialized using personalities.
* Interrupt priority is set for the receive event of the I2C slave.
* Systick timer is initialized to call SysTick_Handler every 1 ms.
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Set NVIC priority */
    NVIC_SetPriority(I2C_SLAVE_RECEIVE_EVENT_IRQN, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), I2C_RECEIVE_EVENT_PRIORITY, 0));

    /* Enable IRQ */
    NVIC_EnableIRQ(I2C_SLAVE_RECEIVE_EVENT_IRQN);

    /* System timer configuration */
    SysTick_Config(SystemCoreClock / TICKS_PER_SECOND);
    while(1);
}

/*******************************************************************************
* Function Name: I2C_SLAVE_RECEIVE_EVENT_HANDLER
********************************************************************************
* Summary:
* This is the interrupt handler function for the I2C slave receive handler.
* It is called every time a message is received by the I2C slave peripheral.
* Based on the received command, the LED is turned on/off.
*
* Parameters:
*  none
*
* Return:
*  none
*
*******************************************************************************/
void I2C_SLAVE_RECEIVE_EVENT_HANDLER()
{
    /* Read received data from I2C module */
    uint8_t rx_slave_receive = XMC_I2C_CH_GetReceivedData(I2C_SLAVE_HW);

    /* Interpret command and set LED state accordingly */
    switch(rx_slave_receive)
    {
        case CMD_LED_HIGH:
        {
            XMC_GPIO_SetOutputHigh(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
        }
        break;
        case CMD_LED_LOW:
        {
            XMC_GPIO_SetOutputLow(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
        }
        break;
    }
}

/* [] END OF FILE */