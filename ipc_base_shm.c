//////////////////////////////////////////////////////////////////////////////////////
//                                                                                  //
//  Copyright (c) 2016-2017 Leonardo Consoni <consoni_2519@hotmail.com>             //
//                                                                                  //
//  This file is part of Simple Async IPC.                                          //
//                                                                                  //
//  Simple Async IPC is free software: you can redistribute it and/or modify        //
//  it under the terms of the GNU Lesser General Public License as published        //
//  by the Free Software Foundation, either version 3 of the License, or            //
//  (at your option) any later version.                                             //
//                                                                                  //
//  Simple Async IPC is distributed in the hope that it will be useful,             //
//  but WITHOUT ANY WARRANTY; without even the implied warranty of                  //
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                    //
//  GNU Lesser General Public License for more details.                             //
//                                                                                  //
//  You should have received a copy of the GNU Lesser General Public License        //
//  along with Simple Async IPC. If not, see <http://www.gnu.org/licenses/>.        //
//                                                                                  //
//////////////////////////////////////////////////////////////////////////////////////


#include "ipc_base_shm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define SHM_READ 0xF0   ///< Flag to allow data to be read from the created shared object
#define SHM_WRITE 0x0F  ///< Flag to allow data to be written to the created shared object

#define SHARED_OBJECT_PATH_MAX_LENGTH 256   ///< Maximum length of the shared object path (file mapping path, network variable)
  
typedef struct _SHMMappingData SHMMappingData;
typedef SHMMappingData* SHMMapping;  
  
#ifdef WIN32

#include <windows.h>

typedef struct _SHMMappingData
{
  void* dataIn;
  void* dataOut;
  HANDLE readHandle, writeHandle;
  Byte readCount, writeCount;
};


IPCBaseConnection SHM_OpenConnection( Byte mappingType, const char* mappingName, uint16_t channel )
{
  int accessFlag = FILE_MAP_ALL_ACCESS;
  if( flags == SHM_READ ) accessFlag = FILE_MAP_READ;
  if( flags == SHM_WRITE ) accessFlag = FILE_MAP_WRITE;
  HANDLE mappedFile = OpenFileMapping( accessFlag, FALSE, mappingName );
  if( mappedFile == NULL )
  {
    mappedFile = CreateFileMapping( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, objectSize, mappingName );
    if( mappedFile == NULL )
    {
      return (void*) -1;
    }
  }
  
  if( sharedObjectsList == NULL ) sharedObjectsList = kh_init( SO );
  
  int insertionStatus;
  khint_t newSharedMemoryID = kh_put( SO, sharedObjectsList, mappingName, &insertionStatus );
  
  kh_value( sharedObjectsList, newSharedMemoryID ).handle = mappedFile;
  kh_value( sharedObjectsList, newSharedMemoryID ).data = MapViewOfFile( mappedFile, flags, 0, 0, 0 );
  
  return kh_value( sharedObjectsList, newSharedMemoryID ).data;
  
  char mappingFilePath[ SHARED_OBJECT_PATH_MAX_LENGTH ];
  
  SHMMapping newMapping = (SHMMapping) malloc( sizeof(SHMMappingData) );
  
  sprintf( mappingFilePath, "/dev/shm/%s_server_client_%u", mappingName, channel );
  
  if( mappingType & IPC_CLIENT )
    newMapping->dataIn = OpenFileMapping( mappingFilePath, S_IRUSR );
  else // if( mappingType & IPC_SERVER )
    newMapping->dataOut = OpenFileMapping( mappingFilePath, S_IWUSR );
  
  sprintf( mappingFilePath, "/dev/shm/%s_client_server_%u", mappingName, channel );
  
  if( mappingType & IPC_CLIENT )
    newMapping->dataOut = OpenFileMapping( mappingFilePath, S_IWUSR );
  else // if( mappingType & IPC_SERVER )
    newMapping->dataIn = OpenFileMapping( mappingFilePath, S_IRUSR );
  
  if( newMapping->dataIn == NULL || newMapping->dataOut == NULL )
  {
    SHM_CloseConnection( newMapping );
    return NULL;
  }
  
  newMapping->readCount = newMapping->writeCount = 0;
  
  return newMapping;
}

bool SHM_ReceiveMessage( IPCBaseConnection ref_mapping, Byte* message )
{  
  if( ref_mapping == NULL ) return false;
  SHMMapping mapping = (SHMMapping) ref_mapping;
    
  if( ((Byte*) mapping->dataIn)[ IPC_MAX_MESSAGE_LENGTH ] == mapping->readCount ) return false;
  
  memcpy( message, mapping->dataIn, IPC_MAX_MESSAGE_LENGTH );
  
  mapping->readCount = ((Byte*) mapping->dataIn)[ IPC_MAX_MESSAGE_LENGTH ];
  
  return true;
}

bool SHM_SendMessage( IPCBaseConnection ref_mapping, const Byte* message )
{  
  if( ref_mapping == NULL ) return false;
  SHMMapping mapping = (SHMMapping) ref_mapping;
  
  memcpy( mapping->dataOut, message, IPC_MAX_MESSAGE_LENGTH );
  
  ((Byte*) mapping->dataIn)[ IPC_MAX_MESSAGE_LENGTH ] = ++mapping->writeCount;
  
  return true;
}

void SHM_CloseConnection( IPCBaseConnection ref_mapping )
{
  if( ref_mapping == NULL ) return;
  SHMMapping mapping = (SHMMapping) ref_mapping;
  
  if( mapping->dataIn != NULL ) shmdt( mapping->dataIn );
  if( mapping->dataOut != NULL ) shmdt( mapping->dataOut );
  
  free( mapping );
}

void SharedObjects_DestroyObject( void* sharedObjectData )
{
  for( khint_t sharedObjectID = 0; sharedObjectID != kh_end( sharedObjectsList ); sharedObjectID++ )
  {
    if( !kh_exist( sharedObjectsList, sharedObjectID ) ) continue;
    
    if( kh_value( sharedObjectsList, sharedObjectID ).data == sharedObjectData )
    {
      UnmapViewOfFile( sharedObjectData );
      CloseHandle( kh_value( sharedObjectsList, sharedObjectID ).handle );
      kh_del( SO, sharedObjectsList, sharedObjectID );
      
      if( kh_size( sharedObjectsList ) == 0 )
      {
        kh_destroy( SO, sharedObjectsList );
        sharedObjectsList = NULL;
      }
      
      break;
    }
  }
}

#else

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

struct _SHMMappingData
{
  void* dataIn;
  void* dataOut;
  Byte readCount, writeCount;
};

void* OpenFileMapping( const char* mappingFilePath, int accessOption )
{
  // Shared memory is mapped to a file. So we create a new file.
  FILE* mappedFile = fopen( mappingFilePath, "r+" );
  if( mappedFile == NULL )
  {
    if( (mappedFile = fopen( mappingFilePath, "w+" )) == NULL )
    {
      perror( "Failed to open memory mapped file" );
      return NULL;
    }
  }
  fclose( mappedFile );
  
  // Generates exclusive key from the file name (same name generate same key)
  key_t sharedKey = ftok( mappingFilePath, 1 );
  if( sharedKey == -1 )
  {
    perror( "Failed to aquire shared memory key" );
    return NULL;
  }
  
  // Reserves shared memory area and returns a file descriptor to it
  int sharedMemoryID = shmget( sharedKey, IPC_MAX_MESSAGE_LENGTH + 1, IPC_CREAT | accessOption );
  if( sharedMemoryID == -1 )
  {
    perror( "Failed to create shared memory segment" );
    return NULL;
  }
  
  //DEBUG_PRINT( "Got shared memory area ID %d", sharedMemoryID );
  
  // Maps created shared memory area to program address (pointer)
  void* newSharedObject = shmat( sharedMemoryID, NULL, 0 );
  if( newSharedObject == (void*) -1 ) 
  {
    perror( "Failed to bind object" );
    return NULL;
  }
  
  return newSharedObject;
}

IPCBaseConnection SHM_OpenMapping( Byte mappingType, const char* mappingName, uint16_t index )
{
  char mappingFilePath[ SHARED_OBJECT_PATH_MAX_LENGTH ];
  
  SHMMapping newMapping = (SHMMapping) malloc( sizeof(SHMMappingData) );
  
  sprintf( mappingFilePath, "/dev/shm/%s_server_client_%u", mappingName, index );
  
  if( mappingType & IPC_CLIENT )
    newMapping->dataIn = OpenFileMapping( mappingFilePath, S_IRUSR );
  else // if( mappingType & IPC_SERVER )
    newMapping->dataOut = OpenFileMapping( mappingFilePath, S_IWUSR );
  
  sprintf( mappingFilePath, "/dev/shm/%s_client_server_%u", mappingName, index );
  
  if( mappingType & IPC_CLIENT )
    newMapping->dataOut = OpenFileMapping( mappingFilePath, S_IWUSR );
  else // if( mappingType & IPC_SERVER )
    newMapping->dataIn = OpenFileMapping( mappingFilePath, S_IRUSR );
  
  if( newMapping->dataIn == NULL || newMapping->dataOut == NULL )
  {
    SHM_CloseMapping( newMapping );
    return NULL;
  }
  
  newMapping->readCount = newMapping->writeCount = 0;
  
  return newMapping;
}

bool SHM_ReadData( IPCBaseConnection ref_mapping, Byte* message )
{  
  if( ref_mapping == NULL ) return false;
  SHMMapping mapping = (SHMMapping) ref_mapping;
    
  if( ((Byte*) mapping->dataIn)[ IPC_MAX_MESSAGE_LENGTH ] == mapping->readCount ) return false;
  
  memcpy( message, mapping->dataIn, IPC_MAX_MESSAGE_LENGTH );
  
  mapping->readCount = ((Byte*) mapping->dataIn)[ IPC_MAX_MESSAGE_LENGTH ];
  
  return true;
}

bool SHM_WriteData( IPCBaseConnection ref_mapping, const Byte* message )
{  
  if( ref_mapping == NULL ) return false;
  SHMMapping mapping = (SHMMapping) ref_mapping;
  
  memcpy( mapping->dataOut, message, IPC_MAX_MESSAGE_LENGTH );
  
  ((Byte*) mapping->dataIn)[ IPC_MAX_MESSAGE_LENGTH ] = ++mapping->writeCount;
  
  return true;
}

void SHM_CloseMapping( IPCBaseConnection ref_mapping )
{
  if( ref_mapping == NULL ) return;
  SHMMapping mapping = (SHMMapping) ref_mapping;
  
  if( mapping->dataIn != NULL ) shmdt( mapping->dataIn );
  if( mapping->dataOut != NULL ) shmdt( mapping->dataOut );
  
  free( mapping );
}

#endif
