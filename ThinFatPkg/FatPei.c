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

#include "FatPei.h"

EFI_THIN_FAT_PRIVATE_DATA *mPrivateData = NULL;

/**
  @param  FileHandle		Handle of the file being operated on.
  @param  PeiServices		Describes the list of PEI services.

  @retval EFI_SUCCESS		The entry point was executed successfully
  @retval EFI_OUT_OF_RESOURCES	There is not enough memory to complete
				the operations.

**/
EFI_STATUS
EFIAPI
FatPeimEntry (
  IN EFI_PEI_FILE_HANDLE	FileHandle,
  IN CONST EFI_PEI_SERVICES	**PeiServices
  )
{
  EFI_STATUS                Status;
  EFI_PHYSICAL_ADDRESS      Address;
  EFI_THIN_FAT_PRIVATE_DATA *PrivateData;

  Status = PeiServicesRegisterForShadow (FileHandle);
  if (!EFI_ERROR (Status)) {
    return Status;
  }

  //
  // initialize private data
  //
  Status = PeiServicesAlocatePages (
            EfiBootServicesCode,
	    (sizeof (EFI_THIN_FAT_PRIVATE_DATA) - 1) / THINFAT_PAGE_SIZE + 1,
	    &Address
	    );
  if (EFI_ERROR(Status)) {
    return EFI_OUT_OF_RESOURCES;
  }

  PrivateData = (EFI_THIN_FAT_PRIVATE_DATA *) (UINTN) Address;

  //
  // Install Ppi
  //
  PrivateData->DeviceRecoveryPpi.GetNumberRecoveryCapsules = GetNumberRecoveryCapsules;
  PrivateData->DeviceRecoveryPpi.GetRecoveryCapsuleInfo    = GetRecoveryCapsuleInfo;
  PrivateData->DeviceRecoveryPpi.LoadRecoveryCapsule       = LoadRecoveryCapsule;
  PrivateData->PpiDescriptor.Flags                         = EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST;
  PrivateData->PpiDescriptor.Guid = &gEfiPeiDeviceRecoveryModulePpiGuid;
  PrivateData->PpiDescriptor.Ppi = &PrivateData->DeviceRecoveryPpi;

  Status = PeiServicesInstallPpi (&PrivateData->PpiDescriptor);
  if (EFI_ERROR (Status)) {
    return EFI_OUT_OF_RESOURCES;
  }

  mPrivateData = PrivateData;

  PrivateData->NotifyDescriptor.Flags = 
    (
      EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK |
      EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST
    );
  PrivateData->NotifyDescriptor.Guid    = &gEfiPeiVirtualBlockIoPpiGuid;
  PrivateData->NotifyDescriptor.Notify  = BlockIoNotifyEntry;
  return PeiServicesNotifyPpi (&PrivateData->NotifyDescriptor);
}
