/**
 * Copyright (C) 2009, Shao Miller <shao.miller@yrdsb.edu.on.ca>.
 * Copyright 2006-2008, V.
 * For WinAoE contact information, see http://winaoe.org/
 *
 * This file is part of WinVBlock, derived from WinAoE.
 *
 * WinVBlock is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * WinVBlock is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WinVBlock.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file
 *
 * Boot-time disk probing specifics
 *
 */

#include <ntddk.h>

#include "winvblock.h"
#include "portable.h"
#include "irp.h"
#include "driver.h"
#include "disk.h"
#include "mount.h"
#include "bus.h"
#include "debug.h"
#include "bus.h"
#include "aoe.h"
#include "ramdisk.h"
#include "memdisk.h"
#include "grub4dos.h"
#include "probe.h"

#ifdef _MSC_VER
#  pragma pack(1)
#endif
winvblock__def_struct ( abft )
{
  winvblock__uint32 Signature;	/* 0x54464261 (aBFT) */
  winvblock__uint32 Length;
  winvblock__uint8 Revision;
  winvblock__uint8 Checksum;
  winvblock__uint8 OEMID[6];
  winvblock__uint8 OEMTableID[8];
  winvblock__uint8 Reserved1[12];
  winvblock__uint16 Major;
  winvblock__uint8 Minor;
  winvblock__uint8 Reserved2;
  winvblock__uint8 ClientMac[6];
}

__attribute__ ( ( __packed__ ) );
#ifdef _MSC_VER
#  pragma pack()
#endif

void
find_aoe_disks (
  void
 )
{
  PHYSICAL_ADDRESS PhysicalAddress;
  winvblock__uint8_ptr PhysicalMemory;
  winvblock__uint32 Offset,
   Checksum,
   i;
  winvblock__bool FoundAbft = FALSE;
  abft AoEBootRecord;
  aoe__disk_type aoe_disk;
  bus__type_ptr bus_ptr;

  /*
   * Establish a pointer into the bus device's extension space
   */
  bus_ptr = get_bus_ptr ( ( driver__dev_ext_ptr ) bus__fdo->DeviceExtension );
  /*
   * Find aBFT
   */
  PhysicalAddress.QuadPart = 0LL;
  PhysicalMemory = MmMapIoSpace ( PhysicalAddress, 0xa0000, MmNonCached );
  if ( !PhysicalMemory )
    {
      DBG ( "Could not map low memory\n" );
    }
  else
    {
      for ( Offset = 0; Offset < 0xa0000; Offset += 0x10 )
	{
	  if ( ( ( abft_ptr ) & PhysicalMemory[Offset] )->Signature ==
	       0x54464261 )
	    {
	      Checksum = 0;
	      for ( i = 0;
		    i < ( ( abft_ptr ) & PhysicalMemory[Offset] )->Length;
		    i++ )
		Checksum += PhysicalMemory[Offset + i];
	      if ( Checksum & 0xff )
		continue;
	      if ( ( ( abft_ptr ) & PhysicalMemory[Offset] )->Revision != 1 )
		{
		  DBG ( "Found aBFT with mismatched revision v%d at "
			"segment 0x%4x. want v1.\n",
			( ( abft_ptr ) & PhysicalMemory[Offset] )->Revision,
			( Offset / 0x10 ) );
		  continue;
		}
	      DBG ( "Found aBFT at segment: 0x%04x\n", ( Offset / 0x10 ) );
	      RtlCopyMemory ( &AoEBootRecord, &PhysicalMemory[Offset],
			      sizeof ( abft ) );
	      FoundAbft = TRUE;
	      break;
	    }
	}
      MmUnmapIoSpace ( PhysicalMemory, 0xa0000 );
    }

#ifdef RIS
  FoundAbft = TRUE;
  RtlCopyMemory ( AoEBootRecord.ClientMac, "\x00\x0c\x29\x34\x69\x34", 6 );
  AoEBootRecord.Major = 0;
  AoEBootRecord.Minor = 10;
#endif

  if ( FoundAbft )
    {
      DBG ( "Attaching AoE disk from client NIC "
	    "%02x:%02x:%02x:%02x:%02x:%02x to major: %d minor: %d\n",
	    AoEBootRecord.ClientMac[0], AoEBootRecord.ClientMac[1],
	    AoEBootRecord.ClientMac[2], AoEBootRecord.ClientMac[3],
	    AoEBootRecord.ClientMac[4], AoEBootRecord.ClientMac[5],
	    AoEBootRecord.Major, AoEBootRecord.Minor );
      aoe_disk.disk.Initialize = AoE_SearchDrive;
      RtlCopyMemory ( aoe_disk.ClientMac, AoEBootRecord.ClientMac, 6 );
      RtlFillMemory ( aoe_disk.ServerMac, 6, 0xff );
      aoe_disk.Major = AoEBootRecord.Major;
      aoe_disk.Minor = AoEBootRecord.Minor;
      aoe_disk.MaxSectorsPerPacket = 1;
      aoe_disk.Timeout = 200000;	/* 20 ms. */
      aoe_disk.disk.io = aoe__disk_io;
      aoe_disk.disk.max_xfer_len = aoe__max_xfer_len;
      aoe_disk.disk.query_id = aoe__query_id;
      aoe_disk.disk.dev_ext.size = sizeof ( aoe__disk_type );

      if ( !Bus_AddChild ( bus__fdo, &aoe_disk.disk, TRUE ) )
	DBG ( "Bus_AddChild() failed for aBFT AoE disk\n" );
      else
	{
	  if ( bus_ptr->PhysicalDeviceObject != NULL )
	    {
	      IoInvalidateDeviceRelations ( bus_ptr->PhysicalDeviceObject,
					    BusRelations );
	    }
	}
    }
  else
    {
      DBG ( "No aBFT found\n" );
    }
}

safe_mbr_hook_ptr STDCALL
get_safe_hook (
  IN winvblock__uint8_ptr PhysicalMemory,
  IN int_vector_ptr InterruptVector
 )
{
  winvblock__uint32 Int13Hook;
  safe_mbr_hook_ptr SafeMbrHookPtr;
  winvblock__uint8 Signature[9] = { 0 };
  winvblock__uint8 VendorID[9] = { 0 };

  Int13Hook =
    ( ( ( winvblock__uint32 ) InterruptVector->Segment ) << 4 ) +
    ( ( winvblock__uint32 ) InterruptVector->Offset );
  SafeMbrHookPtr = ( safe_mbr_hook_ptr ) ( PhysicalMemory + Int13Hook );
  RtlCopyMemory ( Signature, SafeMbrHookPtr->Signature, 8 );
  RtlCopyMemory ( VendorID, SafeMbrHookPtr->VendorID, 8 );
  DBG ( "INT 0x13 Segment: 0x%04x\n", InterruptVector->Segment );
  DBG ( "INT 0x13 Offset: 0x%04x\n", InterruptVector->Offset );
  DBG ( "INT 0x13 Hook: 0x%08x\n", Int13Hook );
  DBG ( "INT 0x13 Safe Hook Signature: %s\n", Signature );
  if ( !( RtlCompareMemory ( Signature, "$INT13SF", 8 ) == 8 ) )
    {
      DBG ( "Invalid INT 0x13 Safe Hook Signature; End of chain\n" );
      return NULL;
    }
  return SafeMbrHookPtr;
}

extern void
probe__disks (
  void
 )
{
  find_aoe_disks (  );
  memdisk__find (  );
  grub4dos__find (  );
}
