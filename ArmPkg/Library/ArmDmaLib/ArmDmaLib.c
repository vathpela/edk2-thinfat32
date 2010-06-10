/** @file
  Generic ARM implementation of DmaLib.h

  Copyright (c) 2008 - 2010, Apple Inc. All rights reserved.<BR>
  
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Base.h>
#include <Library/DebugLib.h>
#include <Library/DmaLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UncachedMemoryAllocationLib.h>
#include <Library/IoLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/ArmLib.h>

#include <Protocol/Cpu.h>

typedef struct {
  EFI_PHYSICAL_ADDRESS      HostAddress;
  EFI_PHYSICAL_ADDRESS      DeviceAddress;
  UINTN                     NumberOfBytes;
  DMA_MAP_OPERATION         Operation;
  BOOLEAN                   DoubleBuffer;
} MAP_INFO_INSTANCE;



EFI_CPU_ARCH_PROTOCOL      *gCpu;
UINTN                      gCacheAlignment = 0;




/**                                                                 
  Provides the DMA controller-specific addresses needed to access system memory.
  
  Operation is relative to the DMA bus master.
            
  @param  Operation             Indicates if the bus master is going to read or write to system memory.
  @param  HostAddress           The system memory address to map to the DMA controller.
  @param  NumberOfBytes         On input the number of bytes to map. On output the number of bytes
                                that were mapped.                                                 
  @param  DeviceAddress         The resulting map address for the bus master controller to use to
                                access the hosts HostAddress.                                        
  @param  Mapping               A resulting value to pass to Unmap().
                                  
  @retval EFI_SUCCESS           The range was mapped for the returned NumberOfBytes.
  @retval EFI_UNSUPPORTED       The HostAddress cannot be mapped as a common buffer.                                
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.
  @retval EFI_DEVICE_ERROR      The system hardware could not map the requested address.
                                   
**/
EFI_STATUS
EFIAPI
DmaMap (
  IN     DMA_MAP_OPERATION              Operation,
  IN     VOID                           *HostAddress,
  IN OUT UINTN                          *NumberOfBytes,
  OUT    PHYSICAL_ADDRESS               *DeviceAddress,
  OUT    VOID                           **Mapping
  )
{
  EFI_STATUS            Status;
  MAP_INFO_INSTANCE     *Map;
  VOID                  *Buffer;

  if ( HostAddress == NULL || NumberOfBytes == NULL || 
       DeviceAddress == NULL || Mapping == NULL ) {
    return EFI_INVALID_PARAMETER;
  }
  

  if (Operation >= MapOperationMaximum) {
    return EFI_INVALID_PARAMETER;
  }

  *DeviceAddress = ConvertToPhysicalAddress (HostAddress);

  // Remember range so we can flush on the other side
  Map = AllocatePool (sizeof (MAP_INFO_INSTANCE));
  if (Map == NULL) {
    return  EFI_OUT_OF_RESOURCES;
  }
  
  *Mapping = Map;

  if (((UINTN)HostAddress & (gCacheAlignment - 1)) != 0) {
    Map->DoubleBuffer  = TRUE;
    Status = DmaAllocateBuffer (EfiBootServicesData, EFI_SIZE_TO_PAGES (*NumberOfBytes), &Buffer);
    if (EFI_ERROR (Status)) {
      return Status;
    }
    
    *DeviceAddress = (PHYSICAL_ADDRESS)(UINTN)Buffer;
    
  } else {
    Map->DoubleBuffer  = FALSE;
  }

  *NumberOfBytes &= *NumberOfBytes & ~(gCacheAlignment - 1); // Only do it on full cache lines
  
  Map->HostAddress   = (UINTN)HostAddress;
  Map->DeviceAddress = *DeviceAddress;
  Map->NumberOfBytes = *NumberOfBytes;
  Map->Operation     = Operation;

  if (Map->DoubleBuffer) {
    if (Map->Operation == MapOperationBusMasterWrite) {
      CopyMem ((VOID *)(UINTN)Map->DeviceAddress, (VOID *)(UINTN)Map->HostAddress, Map->NumberOfBytes);
    }
  } else {
    // EfiCpuFlushTypeWriteBack, EfiCpuFlushTypeInvalidate
    if (Map->Operation == MapOperationBusMasterWrite || Map->Operation == MapOperationBusMasterRead) {
      gCpu->FlushDataCache (gCpu, (EFI_PHYSICAL_ADDRESS)(UINTN)HostAddress,  Map->NumberOfBytes, EfiCpuFlushTypeWriteBackInvalidate);
    }
  }
  
  return EFI_SUCCESS;
}


/**                                                                 
  Completes the DmaMapBusMasterRead(), DmaMapBusMasterWrite(), or DmaMapBusMasterCommonBuffer()
  operation and releases any corresponding resources.
            
  @param  Mapping               The mapping value returned from DmaMap*().
                                  
  @retval EFI_SUCCESS           The range was unmapped.
  @retval EFI_DEVICE_ERROR      The data was not committed to the target system memory.
                                   
**/
EFI_STATUS
EFIAPI
DmaUnmap (
  IN  VOID                         *Mapping
  )
{
  MAP_INFO_INSTANCE *Map;
  
  if (Mapping == NULL) {
    ASSERT (FALSE);
    return EFI_INVALID_PARAMETER;
  }
  
  Map = (MAP_INFO_INSTANCE *)Mapping;
  
  if (Map->DoubleBuffer) {
    if (Map->Operation == MapOperationBusMasterRead) {
      CopyMem ((VOID *)(UINTN)Map->HostAddress, (VOID *)(UINTN)Map->DeviceAddress, Map->NumberOfBytes);
    }
    
    DmaFreeBuffer (EFI_SIZE_TO_PAGES (Map->NumberOfBytes), (VOID *)(UINTN)Map->DeviceAddress);
  
  } else {
    if (Map->Operation == MapOperationBusMasterWrite) {
      //
      // Make sure we read buffer from uncached memory and not the cache
      //
      gCpu->FlushDataCache (gCpu, Map->HostAddress, Map->NumberOfBytes, EfiCpuFlushTypeInvalidate);
    }
  }
  
  FreePool (Map);

  return EFI_SUCCESS;
}

/**                                                                 
  Allocates pages that are suitable for an DmaMap() of type MapOperationBusMasterCommonBuffer.
  mapping.                                                                       
            
  @param  MemoryType            The type of memory to allocate, EfiBootServicesData or
                                EfiRuntimeServicesData.                               
  @param  Pages                 The number of pages to allocate.                                
  @param  HostAddress           A pointer to store the base system memory address of the
                                allocated range.                                        

                                @retval EFI_SUCCESS           The requested memory pages were allocated.
  @retval EFI_UNSUPPORTED       Attributes is unsupported. The only legal attribute bits are
                                MEMORY_WRITE_COMBINE and MEMORY_CACHED.                     
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The memory pages could not be allocated.  
                                   
**/
EFI_STATUS
EFIAPI
DmaAllocateBuffer (
  IN  EFI_MEMORY_TYPE              MemoryType,
  IN  UINTN                        Pages,
  OUT VOID                         **HostAddress
  )
{
  if (HostAddress == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // The only valid memory types are EfiBootServicesData and EfiRuntimeServicesData
  //
  // We used uncached memory to keep coherency
  //
  if (MemoryType == EfiBootServicesData) {
    *HostAddress = UncachedAllocatePages (Pages);
  } else if (MemoryType != EfiRuntimeServicesData) {
    *HostAddress = UncachedAllocateRuntimePages (Pages);
  } else {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}


/**                                                                 
  Frees memory that was allocated with DmaAllocateBuffer().
            
  @param  Pages                 The number of pages to free.                                
  @param  HostAddress           The base system memory address of the allocated range.                                    
                                  
  @retval EFI_SUCCESS           The requested memory pages were freed.
  @retval EFI_INVALID_PARAMETER The memory range specified by HostAddress and Pages
                                was not allocated with DmaAllocateBuffer().
                                     
**/
EFI_STATUS
EFIAPI
DmaFreeBuffer (
  IN  UINTN                        Pages,
  IN  VOID                         *HostAddress
  )
{
  if (HostAddress == NULL) {
     return EFI_INVALID_PARAMETER;
  } 
  
  UncachedFreePages (HostAddress, Pages);
  return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
ArmDmaLibConstructor (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS              Status;

  // Get the Cpu protocol for later use
  Status = gBS->LocateProtocol (&gEfiCpuArchProtocolGuid, NULL, (VOID **)&gCpu);
  ASSERT_EFI_ERROR(Status);

  gCacheAlignment = ArmDataCacheLineLength ();
  
  return Status;
}
