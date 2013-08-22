/** @file
  FAT recovery PEIM entry point

  Copyright 2013 Red Hat, Inc.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions
  of the BSD License which accompanies this distribution.  The
  full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _RECOVERY_THINFAT_H_
#define _RECOVERY_THINFAT_H_

#define PEI_THIN_FAT_PRIVATE_DATA_SIGNATURE SIGNATURE_32 ('d', 'i', 'r', 'e')

typedef struct {
  UINTN					Signature;
  EFI_PEI_DEVICE_RECOVERY_MODULE_PPI	DeviceRecoveryPpi;
  EFI_PEI_PPI_DESCRIPTOR		PpiDescriptor;
  EFI_PEI_NOTIFY_DESCRIPTOR		NotifyDescriptor;


} EFI_THIN_FAT_PRIVATE_DATA;

#define PEI_THIN_FAT_PRIVATE_DATA_FROM_THIS(a) \
  CR (a, PEI_THIN_FAT_PRIVATE_DATA, DeviceRecoveryPpi, PEI_FAT_PRIVATE_DATA_SIANGATURE)

#endif
