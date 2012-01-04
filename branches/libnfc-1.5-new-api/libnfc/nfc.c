/*-
 * Public platform independent Near Field Communication (NFC) library
 * 
 * Copyright (C) 2009, Roel Verdult, Romuald Conty
 * Copyright (C) 2010, Roel Verdult, Romuald Conty, Romain Tartière
 * Copyright (C) 2011, Romuald Conty, Romain Tartière
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
 * @file nfc.c
 * @brief NFC library implementation
 */

/* vim:set ts=2 sw=2 et: */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif // HAVE_CONFIG_H

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <nfc/nfc.h>

#include "nfc-internal.h"
#include "drivers.h"

#include <sys/param.h>

#define LOG_CATEGORY "libnfc.general"

const struct nfc_driver_t *nfc_drivers[] = {
#  if defined (DRIVER_PN53X_USB_ENABLED)
  &pn53x_usb_driver,
#  endif /* DRIVER_PN53X_USB_ENABLED */
#  if defined (DRIVER_ACR122_ENABLED)
  &acr122_driver,
#  endif /* DRIVER_ACR122_ENABLED */
#  if defined (DRIVER_PN532_UART_ENABLED)
  &pn532_uart_driver,
#  endif /* DRIVER_PN532_UART_ENABLED */
#  if defined (DRIVER_ARYGON_ENABLED)
  &arygon_driver,
#  endif /* DRIVER_ARYGON_ENABLED */
  NULL
};

/**
 * @brief Get the defaut NFC device
 * @param connstring \a nfc_connstring pointer where the default connection string will be stored 
 * @return \e true on success
 *
 * This function fill \e connstring with the LIBNFC_DEFAULT_DEVICE environment variable content
 * if is set otherwise it will search for the first available device, and fill
 * \e connstring with the corresponding \a nfc_connstring value.
 *
 * This function returns true when LIBNFC_DEFAULT_DEVICE is set or an available device is found.
 * 
 * @note The \e connstring content can be invalid if LIBNFC_DEFAULT_DEVICE is
 * set with incorrect value.
 */
bool 
nfc_get_default_device (nfc_connstring *connstring)
{
  char *env_default_connstring = getenv ("LIBNFC_DEFAULT_DEVICE");
  if (NULL == env_default_connstring) {
    // LIBNFC_DEFAULT_DEVICE is not set, we fallback on probing for the first available device
    size_t szDeviceFound;
    nfc_connstring listed_cs[1];
    nfc_list_devices (listed_cs, 1, &szDeviceFound);
    if (szDeviceFound) {
      strncpy (*connstring, listed_cs[0], sizeof(nfc_connstring));
    } else {
      return false;
    }
  } else {
    strncpy (*connstring, env_default_connstring, sizeof(nfc_connstring));
  }
  return true;
}

/**
 * @brief Connect to a NFC device
 * @param connstring The device connection string if specific device is wanted, \c NULL otherwise
 * @return Returns pointer to a \a nfc_device struct if successfull; otherwise returns \c NULL value.
 *
 * If \e connstring is \c NULL, the \a nfc_get_default_device() function is used.
 * 
 * If \e connstring is set, this function will try to claim the right device using information provided by \e connstring.
 *
 * When it has successfully claimed a NFC device, memory is allocated to save the device information.
 * It will return a pointer to a \a nfc_device struct.
 * This pointer should be supplied by every next functions of libnfc that should perform an action with this device.
 *
 * @note Depending on the desired operation mode, the device needs to be configured by using nfc_initiator_init() or nfc_target_init(), 
 * optionally followed by manual tuning of the parameters if the default parameters are not suiting your goals.
 */
nfc_device *
nfc_connect (const nfc_connstring connstring)
{
  log_init ();
  nfc_device *pnd = NULL;

  nfc_connstring ncs;
  if (connstring == NULL) {
    if (!nfc_get_default_device (&ncs)) {
      log_fini ();
      return NULL;
    }
  } else {
    strncpy (ncs, connstring, sizeof (nfc_connstring));
  }
  
  // Search through the device list for an available device
  const struct nfc_driver_t *ndr;
  const struct nfc_driver_t **pndr = nfc_drivers;
  while ((ndr = *pndr)) {
    // Specific device is requested: using device description 
    if (0 != strncmp (ndr->name, ncs, strlen(ndr->name))) {
      pndr++;
      continue;
    }

    pnd = ndr->connect (ncs);
    // Test if the connection was successful
    if (pnd == NULL) {
      log_put (LOG_CATEGORY, NFC_PRIORITY_TRACE, "Unable to connect to \"%s\".", ncs);
      log_fini ();
      return pnd;
    }
    
    log_put (LOG_CATEGORY, NFC_PRIORITY_TRACE, "[%s] has been claimed.", pnd->acName);
    log_fini ();
    return pnd;
  }

  // Too bad, no driver can decode connstring
  log_put (LOG_CATEGORY, NFC_PRIORITY_TRACE, "No driver available to handle \"%s\".", ncs);
  log_fini ();
  return NULL;
}

/**
 * @brief Disconnect from a NFC device
 * @param pnd \a nfc_device struct pointer that represent currently used device
 *
 * Initiator's selected tag is disconnected and the device, including allocated \a nfc_device struct, is released.
 */
void
nfc_disconnect (nfc_device *pnd)
{
  if (pnd) {
    // Go in idle mode
    nfc_idle (pnd);
    // Disconnect, clean up and release the device 
    pnd->driver->disconnect (pnd);
    
    log_fini ();
  }
}

/**
 * @brief Probe for discoverable supported devices (ie. only available for some drivers)
 * @param[out] pnddDevices array of \a nfc_device_desc_t previously allocated by the caller.
 * @param szDevices size of the \a pnddDevices array.
 * @param[out] pszDeviceFound number of devices found.
 */
void
nfc_list_devices (nfc_connstring connstrings[] , size_t szDevices, size_t *pszDeviceFound)
{
  size_t  szN;
  *pszDeviceFound = 0;
  const struct nfc_driver_t *ndr;
  const struct nfc_driver_t **pndr = nfc_drivers;

  log_init ();
  while ((ndr = *pndr)) {
    szN = 0;
    if (ndr->probe (connstrings + (*pszDeviceFound), szDevices - (*pszDeviceFound), &szN)) {
      *pszDeviceFound += szN;
      log_put (LOG_CATEGORY, NFC_PRIORITY_TRACE, "%ld device(s) found using %s driver", (unsigned long) szN, ndr->name);
      if (*pszDeviceFound == szDevices)
	  break;
    }
    pndr++;
  }
  log_fini ();
}

/**
 * @brief Set a device's integer-property value
 * @return Returns 0 on success, otherwise returns libnfc's error code (negative value)
 * @param pnd \a nfc_device struct pointer that represent currently used device
 * @param property \a nfc_property which will be set
 * @param value integer value
 *
 * Sets integer property.
 *
 * @see nfc_property enum values
 */
int 
nfc_device_set_property_int (nfc_device *pnd, const nfc_property property, const int value)
{
  HAL (device_set_property_int, pnd, property, value);
}


/**
 * @brief Set a device's boolean-property value
 * @return Returns 0 on success, otherwise returns libnfc's error code (negative value)
 * @param pnd \a nfc_device struct pointer that represent currently used device
 * @param property \a nfc_property which will be set
 * @param bEnable boolean to activate/disactivate the property
 *
 * Configures parameters and registers that control for example timing,
 * modulation, frame and error handling.  There are different categories for
 * configuring the \e PN53X chip features (handle, activate, infinite and
 * accept).
 */
int
nfc_device_set_property_bool (nfc_device *pnd, const nfc_property property, const bool bEnable)
{
  HAL (device_set_property_bool, pnd, property, bEnable);
}

/**
 * @brief Initialize NFC device as initiator (reader)
 * @return Returns 0 on success, otherwise returns libnfc's error code (negative value)
 * @param pnd \a nfc_device struct pointer that represent currently used device
 *
 * The NFC device is configured to function as RFID reader.
 * After initialization it can be used to communicate to passive RFID tags and active NFC devices.
 * The reader will act as initiator to communicate peer 2 peer (NFCIP) to other active NFC devices.
 * - Crc is handled by the device (NP_HANDLE_CRC = true)
 * - Parity is handled the device (NP_HANDLE_PARITY = true)
 * - Cryto1 cipher is disabled (NP_ACTIVATE_CRYPTO1 = false)
 * - Easy framing is enabled (NP_EASY_FRAMING = true)
 * - Auto-switching in ISO14443-4 mode is enabled (NP_AUTO_ISO14443_4 = true)
 * - Invalid frames are not accepted (NP_ACCEPT_INVALID_FRAMES = false)
 * - Multiple frames are not accepted (NP_ACCEPT_MULTIPLE_FRAMES = false)
 * - 14443-A mode is activated (NP_FORCE_ISO14443_A = true)
 * - speed is set to 106 kbps (NP_FORCE_SPEED_106 = true)
 * - Let the device try forever to find a target (NP_INFINITE_SELECT = true)
 * - RF field is shortly dropped (if it was enabled) then activated again
 */
int
nfc_initiator_init (nfc_device *pnd)
{
  int res = 0;
  // Drop the field for a while
  if ((res = nfc_device_set_property_bool (pnd, NP_ACTIVATE_FIELD, false)) < 0)
    return res;
  // Enable field so more power consuming cards can power themselves up
  if ((res = nfc_device_set_property_bool (pnd, NP_ACTIVATE_FIELD, true)) < 0)
    return res;
  // Let the device try forever to find a target/tag
  if ((res = nfc_device_set_property_bool (pnd, NP_INFINITE_SELECT, true)) < 0)
    return res;
  // Activate auto ISO14443-4 switching by default
  if ((res = nfc_device_set_property_bool (pnd, NP_AUTO_ISO14443_4, true)) < 0)
    return res;
  // Force 14443-A mode
  if ((res = nfc_device_set_property_bool (pnd, NP_FORCE_ISO14443_A, true)) < 0)
    return res;
  // Force speed at 106kbps
  if ((res = nfc_device_set_property_bool (pnd, NP_FORCE_SPEED_106, true)) < 0)
    return res;
  // Disallow invalid frame
  if ((res = nfc_device_set_property_bool (pnd, NP_ACCEPT_INVALID_FRAMES, false)) < 0)
    return res;
  // Disallow multiple frames
  if ((res = nfc_device_set_property_bool (pnd, NP_ACCEPT_MULTIPLE_FRAMES, false)) < 0)
    return res;
  // Make sure we reset the CRC and parity to chip handling.
  if ((res = nfc_device_set_property_bool (pnd, NP_HANDLE_CRC, true)) < 0)
    return res;
  if ((res = nfc_device_set_property_bool (pnd, NP_HANDLE_PARITY, true)) < 0)
    return res;
  // Activate "easy framing" feature by default
  if ((res = nfc_device_set_property_bool (pnd, NP_EASY_FRAMING, true)) < 0)
    return res;
  // Deactivate the CRYPTO1 cipher, it may could cause problems when still active
  if ((res = nfc_device_set_property_bool (pnd, NP_ACTIVATE_CRYPTO1, false)) < 0)
    return res;

  HAL (initiator_init, pnd);
}

/**
 * @brief Select a passive or emulated tag
 * @return Returns selected passive target count on success, otherwise returns libnfc's error code (negative value)
 *
 * @param pnd \a nfc_device struct pointer that represent currently used device
 * @param nm desired modulation
 * @param pbtInitData optional initiator data used for Felica, ISO14443B, Topaz polling or to select a specific UID in ISO14443A.
 * @param szInitData length of initiator data \a pbtInitData.
 * @note pbtInitData is used with different kind of data depending on modulation type:
 * - for an ISO/IEC 14443 type A modulation, pbbInitData contains the UID you want to select;
 * - for an ISO/IEC 14443 type B modulation, pbbInitData contains Application Family Identifier (AFI) (see ISO/IEC 14443-3);
 * - for a FeliCa modulation, pbbInitData contains polling payload (see ISO/IEC 18092 11.2.2.5).
 *
 * @param[out] pnt \a nfc_target struct pointer which will filled if available
 *
 * The NFC device will try to find one available passive tag or emulated tag. 
 *
 * The chip needs to know with what kind of tag it is dealing with, therefore
 * the initial modulation and speed (106, 212 or 424 kbps) should be supplied.
 */
int
nfc_initiator_select_passive_target (nfc_device *pnd,
                                     const nfc_modulation nm,
                                     const uint8_t *pbtInitData, const size_t szInitData,
                                     nfc_target *pnt)
{
  uint8_t  abtInit[MAX(12, szInitData)];
  size_t  szInit;

  switch (nm.nmt) {
  case NMT_ISO14443A:
    iso14443_cascade_uid (pbtInitData, szInitData, abtInit, &szInit);
    break;

  default:
    memcpy (abtInit, pbtInitData, szInitData);
    szInit = szInitData;
    break;
  }

  HAL (initiator_select_passive_target, pnd, nm, abtInit, szInit, pnt);
}

/**
 * @brief List passive or emulated tags
 * @return Returns the number of targets found on success, otherwise returns libnfc's error code (negative value)
 *
 * @param pnd \a nfc_device struct pointer that represent currently used device
 * @param nm desired modulation
 * @param[out] ant array of \a nfc_target that will be filled with targets info
 * @param szTargets size of \a ant (will be the max targets listed)
 *
 * The NFC device will try to find the available passive tags. Some NFC devices
 * are capable to emulate passive tags. The standards (ISO18092 and ECMA-340)
 * describe the modulation that can be used for reader to passive
 * communications. The chip needs to know with what kind of tag it is dealing
 * with, therefore the initial modulation and speed (106, 212 or 424 kbps)
 * should be supplied.
 */
int
nfc_initiator_list_passive_targets (nfc_device *pnd,
                                    const nfc_modulation nm,
                                    nfc_target ant[], const size_t szTargets)
{
  nfc_target nt;
  size_t  szTargetFound = 0;
  uint8_t *pbtInitData = NULL;
  size_t  szInitDataLen = 0;
  int res = 0;

  pnd->last_error = 0;

  // Let the reader only try once to find a tag
  if ((res = nfc_device_set_property_bool (pnd, NP_INFINITE_SELECT, false)) < 0) {
    return res;
  }

  prepare_initiator_data (nm, &pbtInitData, &szInitDataLen);

  while (nfc_initiator_select_passive_target (pnd, nm, pbtInitData, szInitDataLen, &nt) > 0) {
    nfc_initiator_deselect_target (pnd);
    if (szTargets == szTargetFound) {
      break;
    }
    size_t i;
    bool seen = false;
    // Check if we've already seen this tag
    for (i = 0; i < szTargetFound; i++) {
      if (memcmp(&(ant[i]), &nt, sizeof (nfc_target)) == 0) {
        seen = true;
      }
    }
    if (seen) {
      break;
    }
    memcpy (&(ant[szTargetFound]), &nt, sizeof (nfc_target));
    szTargetFound++;
    // deselect has no effect on FeliCa and Jewel cards so we'll stop after one...
    // ISO/IEC 14443 B' cards are polled at 100% probability so it's not possible to detect correctly two cards at the same time
    if ((nm.nmt == NMT_FELICA) || (nm.nmt == NMT_JEWEL) || (nm.nmt == NMT_ISO14443BI) || (nm.nmt == NMT_ISO14443B2SR) || (nm.nmt == NMT_ISO14443B2CT)) {
      break;
    }
  }
  return szTargetFound;
}

/**
 * @brief Polling for NFC targets
 * @return Returns polled targets count, otherwise returns libnfc's error code (negative value).
 *
 * @param pnd \a nfc_device struct pointer that represent currently used device
 * @param ppttTargetTypes array of desired target types
 * @param szTargetTypes \e ppttTargetTypes count
 * @param uiPollNr specifies the number of polling (0x01 – 0xFE: 1 up to 254 polling, 0xFF: Endless polling)
 * @note one polling is a polling for each desired target type
 * @param uiPeriod indicates the polling period in units of 150 ms (0x01 – 0x0F: 150ms – 2.25s)
 * @note e.g. if uiPeriod=10, it will poll each desired target type during 1.5s
 * @param[out] pnt pointer on \a nfc_target (over)writable struct
 */
int
nfc_initiator_poll_target (nfc_device *pnd,
                           const nfc_modulation *pnmModulations, const size_t szModulations,
                           const uint8_t uiPollNr, const uint8_t uiPeriod,
                           nfc_target *pnt)
{
  HAL (initiator_poll_target, pnd, pnmModulations, szModulations, uiPollNr, uiPeriod, pnt);
}


/**
 * @brief Select a target and request active or passive mode for D.E.P. (Data Exchange Protocol)
 * @return Returns selected D.E.P tagets count on success, otherwise returns libnfc's error code (negative value).
 *
 * @param pnd \a nfc_device struct pointer that represent currently used device
 * @param ndm desired D.E.P. mode (\a NDM_ACTIVE or \a NDM_PASSIVE for active, respectively passive mode)
 * @param ndiInitiator pointer \a nfc_dep_info struct that contains \e NFCID3 and \e General \e Bytes to set to the initiator device (optionnal, can be \e NULL)
 * @param[out] pnt is a \a nfc_target struct pointer where target information will be put.
 *
 * The NFC device will try to find an available D.E.P. target. The standards
 * (ISO18092 and ECMA-340) describe the modulation that can be used for reader
 * to passive communications.
 * 
 * @note \a nfc_dep_info will be returned when the target was acquired successfully.
 */
int
nfc_initiator_select_dep_target (nfc_device *pnd, 
                                 const nfc_dep_mode ndm, const nfc_baud_rate nbr,
                                 const nfc_dep_info *pndiInitiator, nfc_target *pnt, const int timeout)
{
  HAL (initiator_select_dep_target, pnd, ndm, nbr, pndiInitiator, pnt, timeout);
}

/**
 * @brief Deselect a selected passive or emulated tag
 * @return Returns 0 on success, otherwise returns libnfc's error code (negative value).
 * @param pnd \a nfc_device struct pointer that represents currently used device
 *
 * After selecting and communicating with a passive tag, this function could be
 * used to deactivate and release the tag. This is very useful when there are
 * multiple tags available in the field. It is possible to use the \fn
 * nfc_initiator_select_passive_target() function to select the first available
 * tag, test it for the available features and support, deselect it and skip to
 * the next tag until the correct tag is found.
 */
int
nfc_initiator_deselect_target (nfc_device *pnd)
{
  HAL (initiator_deselect_target, pnd);
}

/**
 * @brief Send data to target then retrieve data from target
 * @return Returns received bytes count on success, otherwise returns libnfc's error code
 *
 * @param pbtTx contains a byte array of the frame that needs to be transmitted.
 * @param szTx contains the length in bytes.
 * @param timeout in milliseconds
 * 
 * The NFC device (configured as initiator) will transmit the supplied bytes (\a pbtTx) to the target.
 * It waits for the response and stores the received bytes in the \a pbtRx byte array.
 *
 * If timeout is not a null pointer, it specifies the maximum interval to wait for the function to be executed.
 * If timeout is a null pointer, the function blocks indefinitely (until an error is raised or function is completed).
 *
 * If \a NP_EASY_FRAMING option is disabled the frames will sent and received in raw mode: \e PN53x will not handle input neither output data.
 *
 * The parity bits are handled by the \e PN53x chip. The CRC can be generated automatically or handled manually.
 * Using this function, frames can be communicated very fast via the NFC initiator to the tag.
 *
 * Tests show that on average this way of communicating is much faster than using the regular driver/middle-ware (often supplied by manufacturers).
 *
 * @warning The configuration option \a NP_HANDLE_PARITY must be set to \c true (the default value).
 */
int
nfc_initiator_transceive_bytes (nfc_device *pnd, const uint8_t *pbtTx, const size_t szTx, uint8_t *pbtRx,
                                size_t *pszRx, int timeout)
{
  HAL (initiator_transceive_bytes, pnd, pbtTx, szTx, pbtRx, pszRx, timeout)
}

/**
 * @brief Transceive raw bit-frames to a target
 * @return Returns received bits count on success, otherwise returns libnfc's error code
 *
 * @param pbtTx contains a byte array of the frame that needs to be transmitted.
 * @param szTxBits contains the length in bits.
 *
 * @note For example the REQA (0x26) command (first anti-collision command of
 * ISO14443-A) must be precise 7 bits long. This is not possible by using
 * nfc_initiator_transceive_bytes(). With that function you can only
 * communicate frames that consist of full bytes. When you send a full byte (8
 * bits + 1 parity) with the value of REQA (0x26), a tag will simply not
 * respond. More information about this can be found in the anti-collision
 * example (\e nfc-anticol).
 *
 * @param pbtTxPar parameter contains a byte array of the corresponding parity bits needed to send per byte.
 *
 * @note For example if you send the SELECT_ALL (0x93, 0x20) = [ 10010011,
 * 00100000 ] command, you have to supply the following parity bytes (0x01,
 * 0x00) to define the correct odd parity bits. This is only an example to
 * explain how it works, if you just are sending two bytes with ISO14443-A
 * compliant parity bits you better can use the
 * nfc_initiator_transceive_bytes() function.
 * 
 * @param[out] pbtRx response from the tag
 * @param[out] pbtRxPar parameter contains a byte array of the corresponding parity bits
 *
 * The NFC device (configured as \e initiator) will transmit low-level messages
 * where only the modulation is handled by the \e PN53x chip. Construction of
 * the frame (data, CRC and parity) is completely done by libnfc.  This can be
 * very useful for testing purposes. Some protocols (e.g. MIFARE Classic)
 * require to violate the ISO14443-A standard by sending incorrect parity and
 * CRC bytes. Using this feature you are able to simulate these frames.
 */
int
nfc_initiator_transceive_bits (nfc_device *pnd, const uint8_t *pbtTx, const size_t szTxBits, const uint8_t *pbtTxPar,
                               uint8_t *pbtRx, uint8_t *pbtRxPar)
{
  HAL (initiator_transceive_bits, pnd, pbtTx, szTxBits, pbtTxPar, pbtRx, pbtRxPar);
}

/**
 * @brief Send data to target then retrieve data from target
 * @return Returns received bytes count on success, otherwise returns libnfc's error code.
 *
 * This function is similar to nfc_initiator_transceive_bytes() with the following differences:
 * - A precise cycles counter will indicate the number of cycles between emission & reception of frames.
 * - It only supports mode with \a NP_EASY_FRAMING option disabled.
 * - Overall communication with the host is heavier and slower.
 *
 * Timer control:
 * By default timer configuration tries to maximize the precision, which also limits the maximum
 * cycles count before saturation/timeout.
 * E.g. with PN53x it can count up to 65535 cycles, so about 4.8ms, with a precision of about 73ns.
 * - If you're ok with the defaults, set *cycles = 0 before calling this function.
 * - If you need to count more cycles, set *cycles to the maximum you expect but don't forget
 *   you'll loose in precision and it'll take more time before timeout, so don't abuse!
 *
 * @warning The configuration option \a NP_EASY_FRAMING must be set to \c false.
 * @warning The configuration option \a NP_HANDLE_PARITY must be set to \c true (the default value).
 */
int
nfc_initiator_transceive_bytes_timed (nfc_device *pnd, const uint8_t *pbtTx, const size_t szTx, uint8_t *pbtRx,
                                size_t *pszRx, uint32_t *cycles)
{
  HAL (initiator_transceive_bytes_timed, pnd, pbtTx, szTx, pbtRx, pszRx, cycles)
}

/**
 * @brief Transceive raw bit-frames to a target
 * @return Returns received bits count on success, otherwise returns libnfc's error code
 *
 * This function is similar to nfc_initiator_transceive_bits() with the following differences:
 * - A precise cycles counter will indicate the number of cycles between emission & reception of frames.
 * - It only supports mode with \a NP_EASY_FRAMING option disabled and CRC must be handled manually.
 * - Overall communication with the host is heavier and slower.
 *
 * Timer control:
 * By default timer configuration tries to maximize the precision, which also limits the maximum
 * cycles count before saturation/timeout.
 * E.g. with PN53x it can count up to 65535 cycles, so about 4.8ms, with a precision of about 73ns.
 * - If you're ok with the defaults, set *cycles = 0 before calling this function.
 * - If you need to count more cycles, set *cycles to the maximum you expect but don't forget
 *   you'll loose in precision and it'll take more time before timeout, so don't abuse!
 *
 * @warning The configuration option \a NP_EASY_FRAMING must be set to \c false.
 * @warning The configuration option \a NP_HANDLE_CRC must be set to \c false.
 * @warning The configuration option \a NP_HANDLE_PARITY must be set to \c true (the default value).
 */
int
nfc_initiator_transceive_bits_timed (nfc_device *pnd, const uint8_t *pbtTx, const size_t szTxBits, const uint8_t *pbtTxPar,
                               uint8_t *pbtRx, size_t *pszRxBits, uint8_t *pbtRxPar, uint32_t *cycles)
{
  HAL (initiator_transceive_bits_timed, pnd, pbtTx, szTxBits, pbtTxPar, pbtRx, pszRxBits, pbtRxPar, cycles);
}

/**
 * @brief Initialize NFC device as an emulated tag
 * @return Returns 0 on success, otherwise returns libnfc's error code
 *
 * @param pnd \a nfc_device struct pointer that represent currently used device
 * @param ntm target mode restriction that you want to emulate (eg. NTM_PASSIVE_ONLY)
 * @param pnt pointer to \a nfc_target struct that represents the wanted emulated target
 *
 * @note \a pnt can be updated by this function: if you set NBR_UNDEFINED
 * and/or NDM_UNDEFINED (ie. for DEP mode), these fields will be updated.
 *
 * @param[out] pbtRx Rx buffer pointer
 * @param[out] pszRx received bytes count
 *
 * This function initializes NFC device in \e target mode in order to emulate a
 * tag using the specified \a nfc_target_mode_t.
 * - Crc is handled by the device (NP_HANDLE_CRC = true)
 * - Parity is handled the device (NP_HANDLE_PARITY = true)
 * - Cryto1 cipher is disabled (NP_ACTIVATE_CRYPTO1 = false)
 * - Auto-switching in ISO14443-4 mode is enabled (NP_AUTO_ISO14443_4 = true)
 * - Easy framing is disabled (NP_EASY_FRAMING = false)
 * - Invalid frames are not accepted (NP_ACCEPT_INVALID_FRAMES = false)
 * - Multiple frames are not accepted (NP_ACCEPT_MULTIPLE_FRAMES = false)
 * - RF field is dropped
 *
 * @warning Be aware that this function will wait (hang) until a command is
 * received that is not part of the anti-collision. The RATS command for
 * example would wake up the emulator. After this is received, the send and
 * receive functions can be used.
 */
int
nfc_target_init (nfc_device *pnd, nfc_target *pnt, uint8_t *pbtRx, size_t * pszRx)
{
  int res = 0;
  // Disallow invalid frame
  if ((res = nfc_device_set_property_bool (pnd, NP_ACCEPT_INVALID_FRAMES, false)) < 0)
    return false;
  // Disallow multiple frames
  if ((res = nfc_device_set_property_bool (pnd, NP_ACCEPT_MULTIPLE_FRAMES, false)) < 0)
    return false;
  // Make sure we reset the CRC and parity to chip handling.
  if ((res = nfc_device_set_property_bool (pnd, NP_HANDLE_CRC, true)) < 0)
    return false;
  if ((res = nfc_device_set_property_bool (pnd, NP_HANDLE_PARITY, true)) < 0)
    return false;
  // Activate auto ISO14443-4 switching by default
  if ((res = nfc_device_set_property_bool (pnd, NP_AUTO_ISO14443_4, true)) < 0)
    return false;
  // Activate "easy framing" feature by default
  if ((res = nfc_device_set_property_bool (pnd, NP_EASY_FRAMING, true)) < 0)
    return false;
  // Deactivate the CRYPTO1 cipher, it may could cause problems when still active
  if ((res = nfc_device_set_property_bool (pnd, NP_ACTIVATE_CRYPTO1, false)) < 0)
    return false;
  // Drop explicitely the field
  if ((res = nfc_device_set_property_bool (pnd, NP_ACTIVATE_FIELD, false)) < 0)
    return false;

  HAL (target_init, pnd, pnt, pbtRx, pszRx);
}

/**
 * @brief Turn NFC device in idle mode
 * @return Returns \c true if action was successfully performed; otherwise returns \c false.
 *
 * @param pnd \a nfc_device struct pointer that represent currently used device
 *
 * This function switch the device in idle mode.
 * In initiator mode, the RF field is turned off and the device is set to low power mode (if avaible);
 * In target mode, the emulation is stoped (no target available from external initiator) and the device is set to low power mode (if avaible).
 */
bool
nfc_idle (nfc_device *pnd)
{
  HAL (idle, pnd);
}

/**
 * @brief Abort current running command
 * @return Returns \c true if action was successfully performed; otherwise returns \c false.
 *
 * @param pnd \a nfc_device struct pointer that represent currently used device
 *
 * Some commands (ie. nfc_target_init()) are blocking functions and will return only in particular conditions (ie. external initiator request).
 * This function attempt to abort the current running command.
 *
 * @note The blocking function (ie. nfc_target_init()) will failed with DEABORT error.
 */
bool
nfc_abort_command (nfc_device *pnd)
{
  HAL (abort_command, pnd);
}

/**
 * @brief Send bytes and APDU frames
 * @return Returns sent bytes count on success, otherwise returns libnfc's error code
 *
 * @param pnd \a nfc_device struct pointer that represent currently used device
 * @param pbtTx pointer to Tx buffer
 * @param szTx size of Tx buffer
 * @param timeout in milliseconds
 *
 * This function make the NFC device (configured as \e target) send byte frames
 * (e.g. APDU responses) to the \e initiator.
 *
 * If timeout is not a null pointer, it specifies the maximum interval to wait for the function to be executed.
 * If timeout is a null pointer, the function blocks indefinitely (until an error is raised or function is completed).
 */
int
nfc_target_send_bytes (nfc_device *pnd, const uint8_t *pbtTx, const size_t szTx, int timeout)
{
  HAL (target_send_bytes, pnd, pbtTx, szTx, timeout);
}

/**
 * @brief Receive bytes and APDU frames
 * @return Returns received bytes count on success, otherwise returns libnfc's error code
 * 
 * @param pnd \a nfc_device struct pointer that represent currently used device
 * @param[out] pbtRx pointer to Rx buffer
 * @param[out] pszRx received byte count
 * @param timeout in milliseconds
 *
 * This function retrieves bytes frames (e.g. ADPU) sent by the \e initiator to the NFC device (configured as \e target).
 *
 * If timeout is not a null pointer, it specifies the maximum interval to wait for the function to be executed.
 * If timeout is a null pointer, the function blocks indefinitely (until an error is raised or function is completed).
 */
int
nfc_target_receive_bytes (nfc_device *pnd, uint8_t *pbtRx, size_t *pszRx, int timeout)
{
  HAL (target_receive_bytes, pnd, pbtRx, pszRx, timeout);
}

/**
 * @brief Send raw bit-frames
 * @return Returns sent bits count on success, otherwise returns libnfc's error code.
 *
 * This function can be used to transmit (raw) bit-frames to the \e initiator
 * using the specified NFC device (configured as \e target).
 */
int
nfc_target_send_bits (nfc_device *pnd, const uint8_t *pbtTx, const size_t szTxBits, const uint8_t *pbtTxPar)
{
  HAL (target_send_bits, pnd, pbtTx, szTxBits, pbtTxPar);
}

/**
 * @brief Receive bit-frames
 * @return Returns received bits count on success, otherwise returns libnfc's error code
 *
 * This function makes it possible to receive (raw) bit-frames.  It returns all
 * the messages that are stored in the FIFO buffer of the \e PN53x chip.  It
 * does not require to send any frame and thereby could be used to snoop frames
 * that are transmitted by a nearby \e initiator.  @note Check out the
 * NP_ACCEPT_MULTIPLE_FRAMES configuration option to avoid losing transmitted
 * frames.
 */
int
nfc_target_receive_bits (nfc_device *pnd, uint8_t *pbtRx, size_t *pszRxBits, uint8_t *pbtRxPar)
{
  HAL (target_receive_bits, pnd, pbtRx, pszRxBits, pbtRxPar);
}

/**
 * @brief Return the PCD error string
 * @return Returns a string
 */
const char *
nfc_strerror (const nfc_device *pnd)
{
  return pnd->driver->strerror (pnd);
}

/**
 * @brief Renders the PCD error in pcStrErrBuf for a maximum size of szBufLen chars
 * @return Returns 0 upon success
 */
int
nfc_strerror_r (const nfc_device *pnd, char *pcStrErrBuf, size_t szBufLen)
{
  return (snprintf (pcStrErrBuf, szBufLen, "%s", nfc_strerror (pnd)) < 0) ? -1 : 0;
}

/**
 * @brief Display the PCD error a-la perror
 */
void
nfc_perror (const nfc_device *pnd, const char *pcString)
{
  fprintf (stderr, "%s: %s\n", pcString, nfc_strerror (pnd));
}

/* Special data accessors */

/**
 * @brief Returns the device name
 * @return Returns a string with the device name
 */
const char *
nfc_device_get_name (nfc_device *pnd)
{
  return pnd->acName;
}

/* Misc. functions */

/**
 * @brief Returns the library version
 * @return Returns a string with the library version
 */
const char *
nfc_version (void)
{
#ifdef SVN_REVISION
  return PACKAGE_VERSION " (r" SVN_REVISION ")";
#else
  return PACKAGE_VERSION;
#endif // SVN_REVISION
}
