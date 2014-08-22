/*
    pkTriggerCord
    Copyright (C) 2011-2014 Andras Salamon <andras.salamon@melda.info>
    Remote control of Pentax DSLR cameras.

    based on:

    PK-Remote
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2008 Pontus Lidman <pontus@lysator.liu.se>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by 
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU General Public License
    and GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>

#include "pslr_scsi.h"

#define IOCTL_SCSI_PASS_THROUGH_DIRECT  0x4D014
#define SCSI_IOCTL_DATA_IN              1
#define SCSI_IOCTL_DATA_OUT             0

/*
#define IOCTL_STORAGE_QUERY_PROPERTY    0x2d1400


typedef enum _STORAGE_PROPERTY_ID 
{
    StorageDeviceProperty = 0,
    StorageAdapterProperty,
    StorageDeviceIdProperty,
    StorageDeviceUniqueIdProperty,              // See storduid.h for details
    StorageDeviceWriteCacheProperty,
    StorageMiniportProperty,
    StorageAccessAlignmentProperty,
    StorageDeviceSeekPenaltyProperty,
    StorageDeviceTrimProperty,
    StorageDeviceWriteAggregationProperty
} STORAGE_PROPERTY_ID;


typedef enum _STORAGE_QUERY_TYPE
{
    PropertyStandardQuery = 0,          // Retrieves the descriptor
    PropertyExistsQuery,                // Used to test whether the descriptor is supported
    PropertyMaskQuery,                  // Used to retrieve a mask of writeable fields in the descriptor
    PropertyQueryMaxDefined     // use to validate the value
} STORAGE_QUERY_TYPE;

typedef struct _STORAGE_PROPERTY_QUERY
{
    STORAGE_PROPERTY_ID PropertyId;
    STORAGE_QUERY_TYPE QueryType;
    BYTE  AdditionalParameters[1];
} STORAGE_PROPERTY_QUERY;

typedef enum _STORAGE_BUS_TYPE {
    BusTypeUnknown = 0x00,
    BusTypeScsi,
    BusTypeAtapi,
    BusTypeAta,
    BusType1394,
    BusTypeSsa,
    BusTypeFibre,
    BusTypeUsb,
    BusTypeRAID,
    BusTypeiScsi,
    BusTypeSas,
    BusTypeSata,
    BusTypeSd,
    BusTypeMmc,
    BusTypeVirtual,
    BusTypeFileBackedVirtual,
    BusTypeMax,
    BusTypeMaxReserved = 0x7F
} STORAGE_BUS_TYPE;

typedef struct _STORAGE_DEVICE_DESCRIPTOR 
{
    DWORD Version;
    DWORD Size;
    BYTE  DeviceType;
    BYTE  DeviceTypeModifier;
    BOOLEAN RemovableMedia;
    BOOLEAN CommandQueueing;
    DWORD VendorIdOffset;
    DWORD ProductIdOffset;
    DWORD ProductRevisionOffset;
    DWORD SerialNumberOffset;
    STORAGE_BUS_TYPE BusType;
    DWORD RawPropertiesLength;
    BYTE  RawDeviceProperties[1];
} STORAGE_DEVICE_DESCRIPTOR;
*/

typedef struct _SCSI_PASS_THROUGH_DIRECT
{
  USHORT Length;
  UCHAR  ScsiStatus;
  UCHAR  PathId;
  UCHAR  TargetId;
  UCHAR  Lun;
  UCHAR  CdbLength;
  UCHAR  SenseInfoLength;
  UCHAR  DataIn;
  ULONG  DataTransferLength;
  ULONG  TimeOutValue;
  PVOID  DataBuffer;
  ULONG  SenseInfoOffset;
  UCHAR  Cdb[16];
} SCSI_PASS_THROUGH_DIRECT;


typedef struct _SCSI_PASS_THROUGH_WITH_BUFFER
{
    SCSI_PASS_THROUGH_DIRECT sptd;
    ULONG             Filler;      // realign buffers to double word boundary
    UCHAR             ucSenseBuf[32];
} SCSI_PASS_THROUGH_WITH_BUFFER;

char **get_drives(int *driveNum) {
    char **ret;
    ret = malloc( ('Z'-'C'+1) * sizeof(char *));
    int driveLetter;
    int j=0;
    for( driveLetter = 'C'; driveLetter<='Z'; ++driveLetter ) {
	ret[j] = malloc( 2 * sizeof (char) );
	snprintf(ret[j], 2, "%c", driveLetter);
	++j;
    }
    *driveNum = j;
    return ret;
}

pslr_result get_drive_info(char* driveName, int* hDevice, 
                            char* vendorId, int vendorIdSizeMax,
                            char* productId, int productIdSizeMax
                           )
{
    bool Status;
    STORAGE_PROPERTY_QUERY query;
    STORAGE_DEVICE_DESCRIPTOR* pdescriptor;
    byte descriptorBuf[256];
    DWORD bytesRead;
    pslr_result drive_status = PSLR_DEVICE_ERROR;
    HANDLE hDrive;
    char fullDriveName[7];

    vendorId[0] = '\0';
    productId[0] = '\0';    
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    
    snprintf( fullDriveName, 7, "\\\\.\\%s:", driveName);

    hDrive = CreateFile(fullDriveName,
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        0,
                        NULL);

    if (hDrive != INVALID_HANDLE_VALUE)
    {
       Status = DeviceIoControl(hDrive,
                                IOCTL_STORAGE_QUERY_PROPERTY,
                                &query,
                                sizeof(query),
                                &descriptorBuf,
                                sizeof(descriptorBuf),
                                &bytesRead,
                                (LPOVERLAPPED)0);
       if (Status==FALSE)
       {
          int LastError = GetLastError(); //lastError alwasy return 1450
          if(LastError != 0)
          {
             CancelIo(hDrive);
          }
       }
       else
       {
	   *hDevice = (int)hDrive;
	   drive_status = PSLR_OK;          
         
	   pdescriptor = (STORAGE_DEVICE_DESCRIPTOR *)descriptorBuf;
       
	   if(pdescriptor->VendorIdOffset != 0) {
             int i = 0;
             while((descriptorBuf[pdescriptor->VendorIdOffset + i] != 0)
                   &&(i < vendorIdSizeMax)
                  ) {
                vendorId[i] = descriptorBuf[pdescriptor->VendorIdOffset + i];
                i++;
             }
	     vendorId[i]='\0';
          }
          if(pdescriptor->ProductIdOffset != 0) {
             int i = 0;
             while((descriptorBuf[pdescriptor->ProductIdOffset + i] != 0)
                   &&(i < productIdSizeMax)
                  ) {
                productId[i] = descriptorBuf[pdescriptor->ProductIdOffset + i];
                i++;
             }
	     productId[i]='\0';
          }
       }
    }	
    return drive_status;
}

void close_drive(int *hDevice)
{
  CloseHandle((HANDLE)*hDevice);
}

int scsi_read(int sg_fd, uint8_t *cmd, uint32_t cmdLen,
              uint8_t *buf, uint32_t bufLen)
{
   SCSI_PASS_THROUGH_WITH_BUFFER sptdwb;
   DWORD outByte=0;
   int Status;
   int LastError=0;
   uint8_t dataIn[64*1024];
   
   sptdwb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
   sptdwb.sptd.ScsiStatus = 0;
   sptdwb.sptd.PathId = 0;
   sptdwb.sptd.TargetId = 0;
   sptdwb.sptd.Lun = 0;
   sptdwb.sptd.CdbLength = cmdLen;
   sptdwb.sptd.SenseInfoLength = sizeof(sptdwb.ucSenseBuf);
   sptdwb.sptd.DataIn = SCSI_IOCTL_DATA_IN;
   sptdwb.sptd.DataTransferLength = bufLen;
   sptdwb.sptd.TimeOutValue = 10;
   sptdwb.sptd.DataBuffer = dataIn;
   sptdwb.sptd.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFER,ucSenseBuf);
   
   memset(sptdwb.sptd.Cdb, 0, sizeof(sptdwb.sptd.Cdb));
   memcpy(sptdwb.sptd.Cdb, cmd, cmdLen);

   Status=DeviceIoControl((HANDLE)sg_fd,
                          IOCTL_SCSI_PASS_THROUGH_DIRECT,
                          &sptdwb,
                          sizeof(sptdwb),
                          &sptdwb,
                          sizeof(sptdwb),
                          &outByte ,
                          NULL);
   if (Status==0)
   {
      LastError = GetLastError();
      if(LastError != 0)
      {
         CancelIo((HANDLE)sg_fd);
      }
   }
   
   memcpy(buf,sptdwb.sptd.DataBuffer,bufLen);
   
   if(LastError != 0)
   {
      return -PSLR_SCSI_ERROR;
   }
   else
   {
       if (sptdwb.sptd.DataTransferLength == bufLen)
       {
            return bufLen;
       }
       else
       {
            return bufLen - sptdwb.sptd.DataTransferLength;
       }
   }
}

int scsi_write(int sg_fd, uint8_t *cmd, uint32_t cmdLen,
               uint8_t *buf, uint32_t bufLen)
{
   SCSI_PASS_THROUGH_WITH_BUFFER sptdwb;
   DWORD outByte=0;
   int Status;
   int LastError=0;
   
   sptdwb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
   sptdwb.sptd.ScsiStatus = 0;
   sptdwb.sptd.PathId = 0;
   sptdwb.sptd.TargetId = 0;
   sptdwb.sptd.Lun = 0;
   sptdwb.sptd.CdbLength = cmdLen;
   sptdwb.sptd.SenseInfoLength = sizeof(sptdwb.ucSenseBuf);
   sptdwb.sptd.DataIn = SCSI_IOCTL_DATA_OUT;
   sptdwb.sptd.DataTransferLength = bufLen;
   sptdwb.sptd.TimeOutValue = 10;
   sptdwb.sptd.DataBuffer = buf;
   sptdwb.sptd.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFER,ucSenseBuf);
   
   memset(sptdwb.sptd.Cdb, 0, sizeof(sptdwb.sptd.Cdb));
   memcpy(sptdwb.sptd.Cdb, cmd, cmdLen);

   Status=DeviceIoControl((HANDLE)sg_fd,
                          IOCTL_SCSI_PASS_THROUGH_DIRECT,
                          &sptdwb,
                          sizeof(sptdwb),
                          &sptdwb,
                          sizeof(sptdwb),
                          &outByte ,
                          NULL);
   if (Status==0)
   {
      LastError = GetLastError();
      if(LastError != 0)
      {
         CancelIo((HANDLE)sg_fd);
      }
   }
   if(LastError != 0)
   {
      return PSLR_SCSI_ERROR;
   }
   else
   {
      return PSLR_OK;
   }
}
