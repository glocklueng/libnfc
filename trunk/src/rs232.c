/**
 * Public platform independent Near Field Communication (NFC) library
 * 
 * Copyright (C) 2009, Roel Verdult
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
 * 
 * @file rs232.c
 * @brief
 */

/*
Based on rs232-code written by Teunis van Beelen available:
http://www.teuniz.net/RS-232/index.html
*/


#include "rs232.h"

#include "messages.h"

// Test if we are dealing with unix operating systems
#ifndef _WIN32

#include <termios.h>
typedef struct termios term_info;
typedef struct {
  int fd;           // Serial port file descriptor
  term_info tiOld;  // Terminal info before using the port
  term_info tiNew;  // Terminal info during the transaction
} serial_port_unix;

// Set time-out on 30 miliseconds
struct timeval tv = { 
  .tv_sec = 0,      // 0 second
  .tv_usec = 30000 // 30,000 micro seconds
};

// Work-around to claim rs232 interface using the c_iflag (software input processing) from the termios struct
#define CCLAIMED 0x80000000

serial_port rs232_open(const char* pcPortName)
{
  serial_port_unix* sp = malloc(sizeof(serial_port_unix));

  if (sp == 0) return INVALID_SERIAL_PORT;

  sp->fd = open(pcPortName, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
  if(sp->fd == -1)
  {
    rs232_close(sp);
    return INVALID_SERIAL_PORT;
  }

  if(tcgetattr(sp->fd,&sp->tiOld) == -1)
  {
    rs232_close(sp);
    return INVALID_SERIAL_PORT;
  }

  // Make sure the port is not claimed already
  if (sp->tiOld.c_iflag & CCLAIMED)
  {
    rs232_close(sp);
    return CLAIMED_SERIAL_PORT;
  }

  // Copy the old terminal info struct
  sp->tiNew = sp->tiOld;

  sp->tiNew.c_cflag = CS8 | CLOCAL | CREAD;
  sp->tiNew.c_iflag = CCLAIMED | IGNPAR;
  sp->tiNew.c_oflag = 0;
  sp->tiNew.c_lflag = 0;

  sp->tiNew.c_cc[VMIN] = 0;      // block until n bytes are received
  sp->tiNew.c_cc[VTIME] = 0;     // block until a timer expires (n * 100 mSec.)

  if(tcsetattr(sp->fd,TCSANOW,&sp->tiNew) == -1)
  {
    rs232_close(sp);
    return INVALID_SERIAL_PORT;
  }

  tcflush(sp->fd, TCIFLUSH);
  return sp;
}

void rs232_set_speed(serial_port sp, const uint32_t uiPortSpeed)
{
  DBG("Serial port speed requested to be set to %d bauds.", uiPortSpeed);
  // Set port speed (Input and Output)
  speed_t stPortSpeed = B9600;
  switch(uiPortSpeed) {
    case 9600: stPortSpeed = B9600;
    break;
    case 19200: stPortSpeed = B19200;
    break;
    case 38400: stPortSpeed = B38400;
    break;
    case 57600: stPortSpeed = B57600;
    break;
    case 115200: stPortSpeed = B115200;
    break;
    case 230400: stPortSpeed = B230400;
    break;
#ifdef B460800
    case 460800: stPortSpeed = B460800;
    break;
#endif
    default:
#ifdef B460800
      ERR("Unable to set serial port speed to %d bauds. Speed value must be one of these constants: 9600 (default), 19200, 38400, 57600, 115200, 230400 or 460800.", uiPortSpeed);
#else
      ERR("Unable to set serial port speed to %d bauds. Speed value must be one of these constants: 9600 (default), 19200, 38400, 57600, 115200 or 230400.", uiPortSpeed);
#endif
  };
  const serial_port_unix* spu = (serial_port_unix*)sp;
  cfsetispeed((struct termios*)&spu->tiNew, stPortSpeed);
  cfsetospeed((struct termios*)&spu->tiNew, stPortSpeed);
  if( tcsetattr(spu->fd, TCSADRAIN, &spu->tiNew)  == -1)
  {
    ERR("Unable to apply new speed settings.");
  }
}

uint32_t rs232_get_speed(const serial_port sp)
{
  uint32_t uiPortSpeed = 0;
  const serial_port_unix* spu = (serial_port_unix*)sp;
  switch (cfgetispeed(&spu->tiNew))
  {
    case B9600: uiPortSpeed = 9600;
    break;
    case B19200: uiPortSpeed = 19200;
    break;
    case B38400: uiPortSpeed = 38400;
    break;
    case B57600: uiPortSpeed = 57600;
    break;
    case B115200: uiPortSpeed = 115200;
    break;
    case B230400: uiPortSpeed = 230400;
    break;
#ifdef B460800
    case B460800: uiPortSpeed = 460800;
    break;
#endif
  }

  return uiPortSpeed;
}

void rs232_close(const serial_port sp)
{
  tcsetattr(((serial_port_unix*)sp)->fd,TCSANOW,&((serial_port_unix*)sp)->tiOld);
  close(((serial_port_unix*)sp)->fd);
  free(sp);
}

bool rs232_cts(const serial_port sp)
{
  char status;
  if (ioctl(((serial_port_unix*)sp)->fd,TIOCMGET,&status) < 0) return false;
  return (status & TIOCM_CTS);
}

bool rs232_receive(const serial_port sp, byte_t* pbtRx, size_t* pszRxLen)
{
  int iResult;
  int byteCount;
  fd_set rfds;

  // Reset file descriptor
  FD_ZERO(&rfds);
  FD_SET(((serial_port_unix*)sp)->fd,&rfds);
  iResult = select(((serial_port_unix*)sp)->fd+1, &rfds, NULL, NULL, &tv);

  // Read error
  if (iResult < 0) {
    DBG("RX error.");
    *pszRxLen = 0;
    return false;
  }
  // Read time-out
  if (iResult == 0) {
    DBG("RX time-out.");
    *pszRxLen = 0;
    return false;
  }

  // Number of bytes in the input buffer
  ioctl(((serial_port_unix*)sp)->fd, FIONREAD, &byteCount);

  // Empty buffer
  if (byteCount == 0) {
    DBG("RX empty buffer.");
    *pszRxLen = 0;
    return false;
  }

  // There is something available, read the data
  *pszRxLen = read(((serial_port_unix*)sp)->fd,pbtRx,byteCount);

  return (*pszRxLen > 0);
}

bool rs232_send(const serial_port sp, const byte_t* pbtTx, const size_t szTxLen)
{
  int iResult;
  iResult = write(((serial_port_unix*)sp)->fd,pbtTx,szTxLen);
  return (iResult >= 0);
}

#else
// The windows serial port implementation

typedef struct { 
  HANDLE hPort;     // Serial port handle
  DCB dcb;          // Device control settings
  COMMTIMEOUTS ct;  // Serial port time-out configuration
} serial_port_windows;

serial_port rs232_open(const char* pcPortName)
{
  char acPortName[255];
  serial_port_windows* sp = malloc(sizeof(serial_port_windows));

  // Copy the input "com?" to "\\.\COM?" format
  sprintf(acPortName,"\\\\.\\%s",pcPortName);
  _strupr(acPortName);

  // Try to open the serial port
  sp->hPort = CreateFileA(acPortName,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
  if (sp->hPort == INVALID_HANDLE_VALUE)
  {
    rs232_close(sp);
    return INVALID_SERIAL_PORT;
  }

  // Prepare the device control
  memset(&sp->dcb, 0, sizeof(DCB));
  sp->dcb.DCBlength = sizeof(DCB);
  if(!BuildCommDCBA("baud=9600 data=8 parity=N stop=1",&sp->dcb))
  {
    rs232_close(sp);
    return INVALID_SERIAL_PORT;
  }

  // Update the active serial port
  if(!SetCommState(sp->hPort,&sp->dcb))
  {
    rs232_close(sp);
    return INVALID_SERIAL_PORT;
  }

  sp->ct.ReadIntervalTimeout         = 0;
  sp->ct.ReadTotalTimeoutMultiplier  = 0;
  sp->ct.ReadTotalTimeoutConstant    = 30;
  sp->ct.WriteTotalTimeoutMultiplier = 0;
  sp->ct.WriteTotalTimeoutConstant   = 30;

  if(!SetCommTimeouts(sp->hPort,&sp->ct))
  {
    rs232_close(sp);
    return INVALID_SERIAL_PORT;
  }

  PurgeComm(sp->hPort, PURGE_RXABORT | PURGE_RXCLEAR);

  return sp;
}

void rs232_close(const serial_port sp)
{
  CloseHandle(((serial_port_windows*)sp)->hPort);
  free(sp);
}

void rs232_set_speed(serial_port sp, const uint32_t uiPortSpeed)
{
  serial_port_windows* spw;

  DBG("Serial port speed requested to be set to %d bauds.", uiPortSpeed);
  // Set port speed (Input and Output)
  switch(uiPortSpeed) {
    case 9600:
    case 19200:
    case 38400:
    case 57600:
    case 115200:
    case 230400:
    case 460800:
    break;
    default:
      ERR("Unable to set serial port speed to %d bauds. Speed value must be one of these constants: 9600 (default), 19200, 38400, 57600, 115200, 230400 or 460800.", uiPortSpeed);
  };

  spw = (serial_port_windows*)sp;
  spw->dcb.BaudRate = uiPortSpeed;
  if (!SetCommState(spw->hPort, &spw->dcb))
  {
    ERR("Unable to apply new speed settings.");
  }
}

uint32_t rs232_get_speed(const serial_port sp)
{
  const serial_port_windows* spw = (serial_port_windows*)sp;
  if (!GetCommState(spw->hPort, (serial_port)&spw->dcb))
    return spw->dcb.BaudRate;
  
  return 0;
}

bool rs232_cts(const serial_port sp)
{
  DWORD dwStatus;
  if (!GetCommModemStatus(((serial_port_windows*)sp)->hPort,&dwStatus)) return false;
  return (dwStatus & MS_CTS_ON);
}

bool rs232_receive(const serial_port sp, byte_t* pbtRx, size_t* pszRxLen)
{
  ReadFile(((serial_port_windows*)sp)->hPort,pbtRx,*pszRxLen,(LPDWORD)pszRxLen,NULL);
  return (*pszRxLen != 0);
}

bool rs232_send(const serial_port sp, const byte_t* pbtTx, const size_t szTxLen)
{
  DWORD dwTxLen = 0;
  return WriteFile(((serial_port_windows*)sp)->hPort,pbtTx,szTxLen,&dwTxLen,NULL);
  return (dwTxLen != 0);
}

#endif
