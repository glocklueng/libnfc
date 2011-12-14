/*-
 * Public platform independent Near Field Communication (NFC) library
 * 
 * Copyright (C) 2009, Roel Verdult
 * Copyright (C) 2010, Romuald Conty
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
 * @file nfc.h
 * @brief libnfc interface
 *
 * Provide all usefull functions (API) to handle NFC devices.
 */

#ifndef _LIBNFC_H_
#  define _LIBNFC_H_

#  include <sys/time.h>

#  include <stdint.h>
#  include <stdbool.h>

#  ifdef _WIN32
  /* Windows platform */
#    ifndef _WINDLL
    /* CMake compilation */
#      ifdef nfc_EXPORTS
#        define  NFC_EXPORT __declspec(dllexport)
#      else
      /* nfc_EXPORTS */
#        define  NFC_EXPORT __declspec(dllimport)
#      endif
       /* nfc_EXPORTS */
#    else
      /* _WINDLL */
    /* Manual makefile */
#      define NFC_EXPORT
#    endif
       /* _WINDLL */
#  else
      /* _WIN32 */
#    define NFC_EXPORT
#  endif
       /* _WIN32 */

#  include <nfc/nfc-types.h>

#  ifdef __cplusplus
extern  "C" {
#  endif                        // __cplusplus

/* NFC Device/Hardware manipulation */
  NFC_EXPORT bool nfc_get_default_device (nfc_connstring *connstring);
  NFC_EXPORT nfc_device *nfc_connect (const nfc_connstring connstring);
  NFC_EXPORT void nfc_disconnect (nfc_device *pnd);
  NFC_EXPORT bool nfc_abort_command (nfc_device *pnd);
  NFC_EXPORT void nfc_list_devices (nfc_connstring connstrings[], size_t connstrings_len, size_t *pszDeviceFound);
  NFC_EXPORT bool nfc_idle (nfc_device *pnd);

/* NFC initiator: act as "reader" */
  NFC_EXPORT bool nfc_initiator_init (nfc_device *pnd);
  NFC_EXPORT bool nfc_initiator_select_passive_target (nfc_device *pnd, const nfc_modulation nm, const uint8_t *pbtInitData, const size_t szInitData, nfc_target *pnt);
  NFC_EXPORT bool nfc_initiator_list_passive_targets (nfc_device *pnd, const nfc_modulation nm, nfc_target ant[], const size_t szTargets, size_t *pszTargetFound);
  NFC_EXPORT bool nfc_initiator_poll_target (nfc_device *pnd, const nfc_modulation *pnmTargetTypes, const size_t szTargetTypes, const uint8_t uiPollNr, const uint8_t uiPeriod, nfc_target *pnt);
  NFC_EXPORT bool nfc_initiator_select_dep_target (nfc_device *pnd, const nfc_dep_mode ndm, const nfc_baud_rate nbr, const nfc_dep_info *pndiInitiator, nfc_target *pnt, const int timeout);
  NFC_EXPORT bool nfc_initiator_deselect_target (nfc_device *pnd);
  NFC_EXPORT bool nfc_initiator_transceive_bytes (nfc_device *pnd, const uint8_t *pbtTx, const size_t szTx, uint8_t *pbtRx, size_t *pszRx, int timeout);
  NFC_EXPORT bool nfc_initiator_transceive_bits (nfc_device *pnd, const uint8_t *pbtTx, const size_t szTxBits, const uint8_t *pbtTxPar, uint8_t *pbtRx, size_t *pszRxBits, uint8_t *pbtRxPar);
  NFC_EXPORT bool nfc_initiator_transceive_bytes_timed (nfc_device *pnd, const uint8_t *pbtTx, const size_t szTx, uint8_t *pbtRx, size_t *pszRx, uint32_t *cycles);
  NFC_EXPORT bool nfc_initiator_transceive_bits_timed (nfc_device *pnd, const uint8_t *pbtTx, const size_t szTxBits, const uint8_t *pbtTxPar, uint8_t *pbtRx, size_t *pszRxBits, uint8_t *pbtRxPar, uint32_t *cycles);

/* NFC target: act as tag (i.e. MIFARE Classic) or NFC target device. */
  NFC_EXPORT bool nfc_target_init (nfc_device *pnd, nfc_target *pnt, uint8_t *pbtRx, size_t *pszRx);
  NFC_EXPORT bool nfc_target_send_bytes (nfc_device *pnd, const uint8_t *pbtTx, const size_t szTx, int timeout);
  NFC_EXPORT bool nfc_target_receive_bytes (nfc_device *pnd, uint8_t *pbtRx, size_t *pszRx, int timeout);
  NFC_EXPORT bool nfc_target_send_bits (nfc_device *pnd, const uint8_t *pbtTx, const size_t szTxBits, const uint8_t *pbtTxPar);
  NFC_EXPORT bool nfc_target_receive_bits (nfc_device *pnd, uint8_t *pbtRx, size_t *pszRxBits, uint8_t *pbtRxPar);

/* Error reporting */
  NFC_EXPORT const char *nfc_strerror (const nfc_device *pnd);
  NFC_EXPORT int nfc_strerror_r (const nfc_device *pnd, char *pcStrErrBuf, size_t szBufLen);
  NFC_EXPORT void nfc_perror (const nfc_device *pnd, const char *pcString);

/* Special data accessors */
  NFC_EXPORT const char *nfc_device_name (nfc_device *pnd);

/* Properties accessors */
  NFC_EXPORT int nfc_device_set_property_int (nfc_device *pnd, const nfc_property property, const int value);
  NFC_EXPORT int nfc_device_set_property_bool (nfc_device *pnd, const nfc_property property, const bool bEnable);

/* Misc. functions */
  NFC_EXPORT void iso14443a_crc (uint8_t *pbtData, size_t szLen, uint8_t *pbtCrc);
  NFC_EXPORT void iso14443a_crc_append (uint8_t *pbtData, size_t szLen);
  NFC_EXPORT uint8_t *iso14443a_locate_historical_bytes (uint8_t *pbtAts, size_t szAts, size_t *pszTk);
  NFC_EXPORT const char *nfc_version (void);

/* Error codes */
#define NFC_SUCCESS		 0	// No error
#define NFC_EIO			-1	// Input / output error, device will not be usable anymore
#define NFC_ENOTSUP		-2	// Operation not supported
#define NFC_EINVARG		-3	// Invalid argument(s)
#define NFC_DEVICE_ERROR  -4  //Device error

/* PN53x specific errors */
// TODO: Be not PN53x-specific here
#define ETIMEOUT	0x01
#define ECRC		0x02
#define EPARITY		0x03
#define EBITCOUNT	0x04
#define EFRAMING	0x05
#define EBITCOLL	0x06
#define ESMALLBUF	0x07
#define EBUFOVF		0x09
#define ERFTIMEOUT	0x0a
#define ERFPROTO	0x0b
#define EOVHEAT		0x0d
#define EINBUFOVF	0x0e
#define EINVPARAM	0x10
#define EDEPUNKCMD	0x12
#define EINVRXFRAM	0x13
#define EMFAUTH		0x14
#define ENSECNOTSUPP	0x18	// PN533 only
#define EBCC		0x23
#define EDEPINVSTATE	0x25
#define EOPNOTALL	0x26
#define ECMD		0x27
#define ETGREL		0x29
#define ECID		0x2a
#define ECDISCARDED	0x2b
#define ENFCID3		0x2c
#define EOVCURRENT	0x2d
#define ENAD		0x2e

/* PN53x framing-level errors */
#define EFRAACKMISMATCH   0x0100  /* Unexpected data */
#define EFRAISERRFRAME    0x0101  /* Error frame */

/* Communication-level errors */
#define ECOMIO            0x1000  /* Input/output error */
#define ECOMTIMEOUT       0x1001  /* Operation timeout */

/* Software level errors */
#define ETGUIDNOTSUP      0xFF00  /* Target UID not supported */
#define EOPABORT          0xFF01  /* Operation aborted */
#define EINVALARG         0xFF02  /* Invalid argument */
#define EDEVNOTSUP        0xFF03  /* Not supported by device */
#define ENOTIMPL          0xFF04  /* Not (yet) implemented in libnfc */

#  ifdef __cplusplus
}
#  endif                        // __cplusplus
#endif                          // _LIBNFC_H_
