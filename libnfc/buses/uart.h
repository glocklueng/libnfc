/*-
 * Public platform independent Near Field Communication (NFC) library
 * 
 * Copyright (C) 2009, 2010, Roel Verdult, Romuald Conty
 * 
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 */

/**
 * @file uart.h
 * @brief UART driver header
 */

#ifndef __NFC_BUS_UART_H__
#define __NFC_BUS_UART_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include <nfc/nfc-types.h>

// Handle platform specific includes
#ifndef _WIN32
  #include <termios.h>
  #include <sys/ioctl.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <limits.h>
  #include <sys/time.h>

  // unistd.h is needed for usleep() fct.
  #include <unistd.h>
  #define delay_ms( X ) usleep( X * 1000 )
#else
  #include <windows.h>

  #define snprintf _snprintf
  #define strdup _strdup
  #define delay_ms( X ) Sleep( X )
#endif

// Path to the serial port is OS-dependant.
// Try to guess what we should use.
//
// XXX: Some review from users cross-compiling is welcome!
#if defined(_WIN32)
  #define SERIAL_STRING "COM"
//#elif defined(__APPLE__)
// TODO: find UART connection string for PN53X device on Mac OS X
//  #define SERIAL_STRING ""
#elif defined (__FreeBSD__) || defined (__OpenBSD__)
  // XXX: Not tested
  #define SERIAL_STRING "/dev/cuau"
#elif defined (__linux__)
  #define SERIAL_STRING "/dev/ttyUSB"
#else
  #error "Can't determine serial string for your system"
#endif

// Define shortcut to types to make code more readable
typedef void* serial_port;
#define INVALID_SERIAL_PORT (void*)(~1)
#define CLAIMED_SERIAL_PORT (void*)(~2)

serial_port uart_open(const char* pcPortName);
void uart_close(const serial_port sp);

void uart_set_speed(serial_port sp, const uint32_t uiPortSpeed);
uint32_t uart_get_speed(const serial_port sp);

bool uart_receive(const serial_port sp, byte_t* pbtRx, size_t* pszRxLen);
bool uart_send(const serial_port sp, const byte_t* pbtTx, const size_t szTxLen);

#endif // __NFC_BUS_UART_H__

