#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif // HAVE_CONFIG_H

#include <stdlib.h>
#include <nfc/nfc.h>

#include "utils/nfc-utils.h"
#include "libnfc/chips/pn53x.h"

int
main (int argc, const char *argv[])
{
  nfc_device *pnd;
  nfc_target nt;

  // Display libnfc version
  const char *acLibnfcVersion = nfc_version ();
  printf ("%s uses libnfc %s\n", argv[0], acLibnfcVersion);

  // Connect using the first available NFC device
  pnd = nfc_connect (NULL);

  if (pnd == NULL) {
    ERR ("%s", "Unable to connect to NFC device.");
    return EXIT_FAILURE;
  }
  // Set connected NFC device to initiator mode
  if (nfc_initiator_init (pnd) < 0) {
    nfc_perror (pnd, "nfc_initiator_init");
    exit (EXIT_FAILURE);    
  }

  printf ("Connected to NFC reader: %s\n", nfc_device_get_name (pnd));

  // Poll for a ISO14443A (MIFARE) tag
  const nfc_modulation nmMifare = {
    .nmt = NMT_ISO14443A,
    .nbr = NBR_106,
  };
  if (nfc_initiator_select_passive_target (pnd, nmMifare, NULL, 0, &nt) > 0) {
    printf ("The following (NFC) ISO14443A tag was found:\n");
    printf ("    ATQA (SENS_RES): ");
    print_hex (nt.nti.nai.abtAtqa, 2);
    printf ("       UID (NFCID%c): ", (nt.nti.nai.abtUid[0] == 0x08 ? '3' : '1'));
    print_hex (nt.nti.nai.abtUid, nt.nti.nai.szUidLen);
    printf ("      SAK (SEL_RES): ");
    print_hex (&nt.nti.nai.btSak, 1);
    if (nt.nti.nai.szAtsLen) {
      printf ("          ATS (ATR): ");
      print_hex (nt.nti.nai.abtAts, nt.nti.nai.szAtsLen);
    }
  }
  // Disconnect from NFC device
  nfc_close (pnd);
  return EXIT_SUCCESS;
}
