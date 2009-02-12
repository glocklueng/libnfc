/*
 
Public platform independent Near Field Communication (NFC) library
Copyright (C) 2009, Roel Verdult
 
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libnfc.h"
#include "mifaretag.h"

static byte abtRecv[MAX_FRAME_LEN];
static ui32 uiRecvLen;
static dev_id di;
static MifareParam mp;
static MifareTag mtKeys;
static MifareTag mtDump;

bool is_first_block(ui32 uiBlock)
{
  // Test if we are in the small or big sectors
  if (uiBlock < 128) return ((uiBlock)%4 == 0); else return ((uiBlock)%16 == 0);
}

bool is_trailer_block(ui32 uiBlock)
{
  // Test if we are in the small or big sectors
  if (uiBlock < 128) return ((uiBlock+1)%4 == 0); else return ((uiBlock+1)%16 == 0);
}

ui32 get_trailer_block(ui32 uiFirstBlock)
{
  // Test if we are in the small or big sectors
  if (uiFirstBlock<128) return uiFirstBlock+3; else return uiFirstBlock+15;
}

int main(int argc, const char* argv[])
{			
  bool b4K;
  bool bKeyA;
  byte* pbtUID;
  bool bFailure;
  ui32 uiBlock;
  ui32 uiBlocks;
  ui32 uiTrailerBlock;
  FILE* pfKeys;
  FILE* pfDump;
  MifareCmd mc;
  
  if (argc < 4)
  {
    printf("mfwrite <a|b> <keys.mfd> <dump.mfd>\n");
    printf("\n");
    return 1;
  }

  bKeyA = (*(argv[1]) == 'a');

  pfKeys = fopen(argv[2],"rb");
  if (pfKeys == null)
  {
    printf("Could not open file: %s\n",argv[2]);
    return 1;
  }
  fread(&mtKeys,1,sizeof(mtKeys),pfKeys);
  fclose(pfKeys);

  pfDump = fopen(argv[3],"rb");
  if (pfKeys == null)
  {
    printf("Could not open file: %s\n",argv[3]);
    return 1;
  }
  fread(&mtDump,1,sizeof(mtDump),pfDump);
  fclose(pfDump);
  printf("Succesful opened MIFARE dump files\n");

  // Try to open the NFC reader
  di = acr122_connect(0);
  if (di == INVALID_DEVICE_ID)
  {
    printf("Error connecting NFC reader\n");
    return 1;
  }
  nfc_reader_init(di);
  // Let the reader only try once to find a tag
  nfc_configure_list_passive_infinite(di,false);

  // Drop the field so the tag will be reset
  nfc_configure_field(di,false);

  // Configure the communication channel
  nfc_configure_handle_crc(di,true);
  nfc_configure_handle_parity(di,true);
  printf("Connected to NFC reader\n");

  // MIFARE Classic tag info = ( tag_count[1], tag_nr[1], ATQA[2], SAK[1], uid_len[1], UID[uid_len] )
  uiRecvLen = MAX_FRAME_LEN;
  if (!nfc_reader_list_passive(di,MT_ISO14443A_106,null,null,abtRecv,&uiRecvLen))
  {
    printf("Error: no tag was found\n");
    return 1;
  }

  // Test if we are dealing with a MIFARE compatible tag
  if ((abtRecv[4] & 0x08) == 0)
  {
    printf("Error: tag is not a MIFARE Classic card\n");
    return 1;
  }

  // Get the info from the key dump
  b4K = (mtKeys.blContent[0].bm.abtATQA[0] == 0x02);
  pbtUID = mtKeys.blContent[0].bm.abtUID;
  
  // Compare if key dump UID is the same as the current tag UID
  if (memcmp(abtRecv+6,pbtUID,4) != 0)
  {
    printf("Expected MIFARE Classic %cK card with uid: %08x\n",b4K?'4':'1',swap_endian32(pbtUID));
  }

  // Get the info from the current tag
  pbtUID = abtRecv+6;
  b4K = (abtRecv[3] == 0x02);
  printf("Found MIFARE Classic %cK card with uid: %08x\n",b4K?'4':'1',swap_endian32(pbtUID));

  uiBlocks = (b4K)?0xff:0x3f;
  bFailure = false;
  printf("Writing %d blocks |",uiBlocks+1);

  // Write the card from begin to end;
  for (uiBlock=0; uiBlock<=uiBlocks; uiBlock++)
  {
    // Authenticate everytime we reach the first sector of a new block
    if (is_first_block(uiBlock))
    {
      // Show if the readout went well
      if (bFailure)
      {
        printf("x");
        // When a failure occured we need to redo the anti-collision
        if (!nfc_reader_list_passive(di,MT_ISO14443A_106,null,null,abtRecv,&uiRecvLen))
        {
          printf("!\nError: tag was removed\n");
          return 1;
        }
        bFailure = false;
      } else {
        // Skip this the first time, bFailure it means nothing (yet)
        if (uiBlock != 0)
        {
          printf(".");
        }
      }
      fflush(stdout);

      // Locate the trailer (with the keys) used for this sector
      uiTrailerBlock = get_trailer_block(uiBlock);

      // Set the authentication information (uid)
      memcpy(mp.mpa.abtUid,abtRecv+6,4);
      
      // Determin if we should use the a or the b key
      if (bKeyA)
      {
        mc = MC_AUTH_A;
        memcpy(mp.mpa.abtKey,mtKeys.blContent[uiTrailerBlock].bt.abtKeyA,6);
      } else {
        mc = MC_AUTH_B;
        memcpy(mp.mpa.abtKey,mtKeys.blContent[uiTrailerBlock].bt.abtKeyB,6);
      }

      // Try to authenticate for the current sector
      if (!nfc_reader_mifare_cmd(di,mc,uiBlock,&mp))
      { 
        printf("!\nError: authentication failed for block %02x\n",uiBlock);
        return 1;
      }
    }
    
    if (is_trailer_block(uiBlock))
    {
      // Copy the keys over from our key dump and store the retrieved access bits
      memcpy(mp.mpd.abtData,mtDump.blContent[uiBlock].bt.abtKeyA,6);
      memcpy(mp.mpd.abtData+6,mtDump.blContent[uiBlock].bt.abtAccessBits,4);
      memcpy(mp.mpd.abtData+10,mtDump.blContent[uiBlock].bt.abtKeyB,6);

      // Try to write the trailer
      nfc_reader_mifare_cmd(di,MC_WRITE,uiBlock,&mp);
    
    } else {

      // The first block 0x00 is read only, skip this
      if (uiBlock == 0) continue;

      // Make sure a earlier write did not fail
      if (!bFailure)
      {
        // Try to write the data block
        memcpy(mp.mpd.abtData,mtDump.blContent[uiBlock].bd.abtContent,16);
        if (!nfc_reader_mifare_cmd(di,MC_WRITE,uiBlock,&mp))
        {
          bFailure = true;
        }
      }
    }
  }
  printf("%c|\n",(bFailure)?'x':'.');
  fflush(stdout);
  
  printf("Done, all data is written!\n");

  return 0;
}
