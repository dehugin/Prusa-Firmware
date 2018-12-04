//uart2.cpp
#include "uart2.h"

volatile unsigned char readRxBuffer, rxData1 = 0, rxData2 = 0, rxData3 = 0, rxCSUM = 0;
volatile bool startRxFlag = false, confirmedPayload = false, txNAKNext = false,
  txACKNext = false, txRESEND = false, pendingACK = false;
volatile uint8_t rxCount;

byte lastTxPayload[3] = {0, 0, 0};

void uart2_init(void)
{
    //DDRH &=	~0x01;
    //PORTH |= 0x01;
    //rbuf_ini(uart2_ibuf, sizeof(uart2_ibuf) - 4);
    cli();
    UCSR2A = (0 << U2X2); // baudrate multiplier
    UCSR2B = (1 << RXEN2) | (1 << TXEN2) | (0 << UCSZ22); // enable receiver and transmitter
    UCSR2C = (0 << UMSEL21) | (0 << UMSEL20) | (0 << UPM21) |
    (0 << UPM20) | (1 << USBS2) |(1 << UCSZ21) | (1 << UCSZ20); // Use 8-bit character sizes

    UBRR2H = (BAUD_PRESCALE >> 8); // Load upper 8-bits of the baud rate value into the high byte of the UBRR register
    UBRR2L = BAUD_PRESCALE; // Load lower 8-bits of the baud rate value into the low byte of the UBRR register
    
    UCSR2B |= (1 << RXCIE2); // enable rx interrupt
    sei();
}

ISR(USART2_RX_vect)
{
  cli();
  readRxBuffer = UDR2;
  printf_P(PSTR("\nUART2 RX 0x%2X\n"), readRxBuffer);
  if ((readRxBuffer == 0x7F) && (!startRxFlag)) {// check for start of framing bytes
    startRxFlag = true;
    rxCount = 0;
  } else if (startRxFlag == true) {
    if (rxCount > 0) {
      if (rxCount > 1) {
        if (rxCount > 2) {
          if (rxCount > 3) {
            if (readRxBuffer == 0x7F) {
              confirmedPayload = true; // set confirm payload bit true for processing my main loop
            } else {
              txNAKNext = true; // **send universal nack here **
            }
          } else {
            rxCSUM = readRxBuffer;
            ++rxCount;
          }
        } else {
          rxData3 = readRxBuffer;
          ++rxCount;
        }
      } else {
        rxData2 = readRxBuffer;
        ++rxCount;
      }
    } else {
      rxData1 = readRxBuffer;
      ++rxCount;
    }
  } else if (readRxBuffer == 0x06) pendingACK = false;  // ACK Received Clear pending flag
  else   if (readRxBuffer == 0x15) txRESEND = true;     // Resend last message
  sei();
}

void uart2_txPayload(unsigned char payload[3])
{
  for (int i = 0; i < 3; i++) lastTxPayload[i] = payload[i];  // Backup incase resend on NACK
  unsigned char csum = 0;
  loop_until_bit_is_set(UCSR2A, UDRE2);   // Do nothing until UDR is ready for more data to be written to it
  UDR2 = 0x7F;                            // Start byte 0x7F
  delay(10);
  for (int i = 0; i < 3; i++) {           // Send data
    loop_until_bit_is_set(UCSR2A, UDRE2); // Do nothing until UDR is ready for more data to be written to it
    UDR2 = (0xFF & payload[i]);
    delay(10);
    csum += (0xFF & payload[i]);
  }
  csum = 0x2D; //(csum/3);
  loop_until_bit_is_set(UCSR2A, UDRE2);   // Do nothing until UDR is ready for more data to be written to it
  UDR2 = (0xFF & csum);
  delay(10);// Send Checksum
  loop_until_bit_is_set(UCSR2A, UDRE2);   // Do nothing until UDR is ready for more data to be written to it
  UDR2 = 0x7F;
  delay(10);// End byte
  pendingACK = true;                      // Set flag to wait for ACK
}

void uart2_txACK(bool ACK)
{
  if (ACK) {
    loop_until_bit_is_set(UCSR2A, UDRE2); // Do nothing until UDR is ready for more data to be written to it
    UDR2 = 0x06; // ACK HEX
    txACKNext = false;
  } else {
    loop_until_bit_is_set(UCSR2A, UDRE2); // Do nothing until UDR is ready for more data to be written to it
    UDR2 = 0x15; // NACK HEX
    txNAKNext = false;
  }
}