/*-
 * Public platform independent Near Field Communication (NFC) library
 * 
 * Copyright (C) 2009, Roel Verdult
 * Copyright (C) 2010, Romain Tartière, Romuald Conty
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
 */

/**
 * @file pn53x_usb.c
 * @brief Driver common routines for PN53x chips using USB
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif // HAVE_CONFIG_H

/*
Thanks to d18c7db and Okko for example code
*/

#include <stdio.h>
#include <stdlib.h>
#include <usb.h>
#include <string.h>

#include "../drivers.h"
#include "../chips/pn53x.h"

#include <nfc/nfc.h>
#include <nfc/nfc-messages.h>

#define BUFFER_LENGTH 256
#define USB_TIMEOUT   30000

// Find transfer endpoints for bulk transfers
void get_end_points(struct usb_device *dev, usb_spec_t* pus)
{
  uint32_t uiIndex;
  uint32_t uiEndPoint;
  struct usb_interface_descriptor* puid = dev->config->interface->altsetting;

  // 3 Endpoints maximum: Interrupt In, Bulk In, Bulk Out
  for(uiIndex = 0; uiIndex < puid->bNumEndpoints; uiIndex++)
  {
    // Only accept bulk transfer endpoints (ignore interrupt endpoints)
    if(puid->endpoint[uiIndex].bmAttributes != USB_ENDPOINT_TYPE_BULK) continue;

    // Copy the endpoint to a local var, makes it more readable code
    uiEndPoint = puid->endpoint[uiIndex].bEndpointAddress;

    // Test if we dealing with a bulk IN endpoint
    if((uiEndPoint & USB_ENDPOINT_DIR_MASK) == USB_ENDPOINT_IN)
    {
      pus->uiEndPointIn = uiEndPoint;
    }

    // Test if we dealing with a bulk OUT endpoint
    if((uiEndPoint & USB_ENDPOINT_DIR_MASK) == USB_ENDPOINT_OUT)
    {
      pus->uiEndPointOut = uiEndPoint;
    }
  }
}

bool pn53x_usb_list_devices(nfc_device_desc_t pnddDevices[], size_t szDevices, size_t *pszDeviceFound,usb_candidate_t candidates[], int num_candidates, char * target_name)
{
  int ret, i;
  
  struct usb_bus *bus;
  struct usb_device *dev;
  usb_dev_handle *udev;
  uint32_t uiBusIndex = 0;
  char string[256];

  string[0]= '\0';
  usb_init();

  // usb_find_busses will find all of the busses on the system. Returns the number of changes since previous call to this function (total of new busses and busses removed).
  if ((ret= usb_find_busses() < 0)) return false;
  // usb_find_devices will find all of the devices on each bus. This should be called after usb_find_busses. Returns the number of changes since the previous call to this function (total of new device and devices removed).
  if ((ret= usb_find_devices() < 0)) return false;

  *pszDeviceFound = 0;

  for (bus = usb_get_busses(); bus; bus = bus->next)
  {
    for (dev = bus->devices; dev; dev = dev->next, uiBusIndex++)
    {
      for(i = 0; i < num_candidates; ++i)
      {
        // DBG("Checking device %04x:%04x (%04x:%04x)",dev->descriptor.idVendor,dev->descriptor.idProduct,candidates[i].idVendor,candidates[i].idProduct);
        if (candidates[i].idVendor==dev->descriptor.idVendor && candidates[i].idProduct==dev->descriptor.idProduct)
        {
          // Make sure there are 2 endpoints available
          // with libusb-win32 we got some null pointers so be robust before looking at endpoints:
          if (dev->config == NULL || dev->config->interface == NULL || dev->config->interface->altsetting == NULL)
          {
            // Nope, we maybe want the next one, let's try to find another
            continue;
          }
          if (dev->config->interface->altsetting->bNumEndpoints < 2)
          {
            // Nope, we maybe want the next one, let's try to find another
            continue;
          }
          if (dev->descriptor.iManufacturer || dev->descriptor.iProduct)
          {
            udev = usb_open(dev);
            if(udev)
            {
              usb_get_string_simple(udev, dev->descriptor.iManufacturer, string, sizeof(string));
              if(strlen(string) > 0)
                strcpy(string + strlen(string)," / ");
              usb_get_string_simple(udev, dev->descriptor.iProduct, string + strlen(string), sizeof(string) - strlen(string));
            }
            usb_close(udev);
          }
          if(strlen(string) == 0)
            strcpy(pnddDevices[*pszDeviceFound].acDevice, target_name);
          else
            strcpy(pnddDevices[*pszDeviceFound].acDevice, string);
          pnddDevices[*pszDeviceFound].pcDriver = target_name;
          pnddDevices[*pszDeviceFound].uiBusIndex = uiBusIndex;
          (*pszDeviceFound)++;
          // Test if we reach the maximum "wanted" devices
          if((*pszDeviceFound) == szDevices) 
          {
            return true;
          }
        }
      }
    }
  }
  if(*pszDeviceFound)
    return true;
  return false;
}

nfc_device_t* pn53x_usb_connect(const nfc_device_desc_t* pndd,const char * target_name, int target_chip)
{
  nfc_device_t* pnd = NULL;
  usb_spec_t* pus;
  usb_spec_t us;
  struct usb_bus *bus;
  struct usb_device *dev;
  uint32_t uiBusIndex;

  us.uiEndPointIn = 0;
  us.uiEndPointOut = 0;
  us.pudh = NULL;

  DBG("Attempt to connect to %s device", target_name);
  usb_init();

  uiBusIndex= pndd->uiBusIndex;

  for (bus = usb_get_busses(); bus; bus = bus->next)
  {
    for (dev = bus->devices; dev; dev = dev->next, uiBusIndex--)
    {
      // DBG("Checking device %04x:%04x",dev->descriptor.idVendor,dev->descriptor.idProduct);
      if(uiBusIndex == 0)
      {
        // Open the USB device
        us.pudh = usb_open(dev);

        get_end_points(dev,&us);
        if(usb_set_configuration(us.pudh,1) < 0)
        {
          DBG("%s", "Setting config failed");
          usb_close(us.pudh);
          // we failed to use the specified device
          return NULL;
        }

        if(usb_claim_interface(us.pudh,0) < 0)
        {
          DBG("%s", "Can't claim interface");
          usb_close(us.pudh);
          // we failed to use the specified device
          return NULL;
        }
        // Allocate memory for the device info and specification, fill it and return the info
        pus = malloc(sizeof(usb_spec_t));
        *pus = us;
        pnd = malloc(sizeof(nfc_device_t));
        strcpy(pnd->acName,target_name);
        pnd->nc = target_chip;
        pnd->nds = (nfc_device_spec_t)pus;
        pnd->bActive = true;
        pnd->bCrc = true;
        pnd->bPar = true;
        pnd->ui8TxBits = 0;
        return pnd;
      }
    }
  }
  // We ran out of devices before the index required
  DBG("%s","Device index not found!");
  return NULL;
}

void pn53x_usb_disconnect(nfc_device_t* pnd)
{
  usb_spec_t* pus = (usb_spec_t*)pnd->nds;
  int ret;

  if((ret = usb_release_interface(pus->pudh,0)) < 0) {
    ERR("usb_release_interface failed (%i)",ret);
  }

  if((ret = usb_close(pus->pudh)) < 0) {
    ERR("usb_close failed (%i)",ret);
  }
/*  
  if((ret = usb_reset(pus->pudh)) < 0) {
    ERR("usb_reset failed (%i, if errno: %s)",ret, strerror(-ret));
  }
*/
  free(pnd->nds);
  free(pnd);
}

bool pn53x_usb_transceive(nfc_device_t* pnd, const byte_t* pbtTx, const size_t szTxLen, byte_t* pbtRx, size_t* pszRxLen)
{
  size_t uiPos = 0;
  int ret = 0;
  byte_t abtTx[BUFFER_LENGTH] = { 0x00, 0x00, 0xff }; // Every packet must start with "00 00 ff"
  byte_t abtRx[BUFFER_LENGTH];
  usb_spec_t* pus = (usb_spec_t*)pnd->nds;
  // TODO: Move this one level up for libnfc-1.6
  uint8_t ack_frame[] = { 0x00, 0x00, 0xff, 0x00, 0xff, 0x00 };

  // Packet length = data length (len) + checksum (1) + end of stream marker (1)
  abtTx[3] = szTxLen;
  // Packet length checksum 
  abtTx[4] = 0x0100 - abtTx[3];
  // Copy the PN53X command into the packet abtTx
  memmove(abtTx+5,pbtTx,szTxLen);

  // Calculate data payload checksum
  abtTx[szTxLen+5] = 0;
  for(uiPos=0; uiPos < szTxLen; uiPos++) 
  {
    abtTx[szTxLen+5] -= abtTx[uiPos+5];
  }

  // End of stream marker
  abtTx[szTxLen+6] = 0;

#ifdef DEBUG
  PRINT_HEX("TX", abtTx,szTxLen+7);
#endif

  ret = usb_bulk_write(pus->pudh, pus->uiEndPointOut, (char*)abtTx, szTxLen+7, USB_TIMEOUT);
  if( ret < 0 )
  {
    DBG("usb_bulk_write failed with error %d", ret);
    pnd->iLastError = DEIO;
    return false;
  }

  ret = usb_bulk_read(pus->pudh, pus->uiEndPointIn, (char*)abtRx, BUFFER_LENGTH, USB_TIMEOUT);
  if( ret < 0 )
  {
    DBG( "usb_bulk_read failed with error %d", ret);
    pnd->iLastError = DEIO;
    return false;
  }

#ifdef DEBUG
  PRINT_HEX("RX", abtRx,ret);
#endif

  if (!pn53x_transceive_callback (pnd, abtRx, ret))
    return false;

    ret = usb_bulk_read(pus->pudh, pus->uiEndPointIn, (char*)abtRx, BUFFER_LENGTH, USB_TIMEOUT);
    if( ret < 0 )
    {
      DBG("usb_bulk_read failed with error %d", ret);
    pnd->iLastError = DEIO;
      return false;
    }

#ifdef DEBUG
    PRINT_HEX("RX", abtRx,ret);
#endif

#ifdef DEBUG
  PRINT_HEX("TX", ack_frame,6);
#endif
  usb_bulk_write(pus->pudh, pus->uiEndPointOut, (char *)ack_frame, 6, USB_TIMEOUT);

  // When the answer should be ignored, just return a succesful result
  if(pbtRx == NULL || pszRxLen == NULL) return true;

  // Only succeed when the result is at least 00 00 FF xx Fx Dx xx .. .. .. xx 00 (x = variable)
  if(ret < 9) 
  {
    DBG("%s","No data");
    pnd->iLastError = DEINVAL;
    return false;
  }

  // Remove the preceding and appending bytes 00 00 FF xx Fx .. .. .. xx 00 (x = variable)
  *pszRxLen = ret - 7 - 2;

  // Get register: nuke extra byte (awful hack)
  if ((abtRx[5]==0xd5) && (abtRx[6]==0x07) && (*pszRxLen==2)) {
      // DBG("awful hack: abtRx[7]=%02x, abtRx[8]=%02x, we only keep abtRx[8]=%02x", abtRx[7], abtRx[8], abtRx[8]);
      *pszRxLen = (*pszRxLen) - 1;
      memcpy( pbtRx, abtRx + 8, *pszRxLen);
      return true;
  }

  memcpy( pbtRx, abtRx + 7, *pszRxLen);

  if (abtRx[5] != pbtTx[0] + 1) {
    pnd->iLastError = DEISERRFRAME;
  }
  return true;
}