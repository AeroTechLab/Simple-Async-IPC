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


/// @file ipc_base_shm.h
/// @brief Platform and type abstractions for synchronous IP connections communication.
///
/// Multiplatform library for creation and handling of Internet Protocol (IP) 
/// sockets connections as server or client, using TCP or UDP protocols                           

#ifndef IPC_BASE_SHM_H
#define IPC_BASE_SHM_H


#include <stdint.h>
#include <stdbool.h>


enum MapType { SHM_CLIENT, SHM_SERVER };

/// @brief Creates a new IP connection structure (with defined properties) and add it to the asynchronous connections list                              
/// @param[in] mappingType flag defining connection as client or server, TCP or UDP (see ip_connection.h)                                   
/// @param[in] path IPv4 or IPv6 host string (NULL for server listening on any local address)                                         
/// @param[in] index IP port number (local for server, remote for client)       
/// @return unique generic identifier to newly created connection (NULL on error) 
void* SHM_OpenMapping( enum MapType mappingType, const char* dirPath, const char* filePath );

/// @brief Handle termination of given connection                                   
/// @param[in] mapping connection reference
void SHM_CloseMapping( void* mapping );
 
/// @brief Calls type specific client method for receiving network messages                      
/// @param[in] mapping client connection reference  
/// @param[in] message message string pointer
/// @return pointer to message string, overwritten on next call to ReceiveMessage() (NULL on error)  
bool SHM_ReadData( void* mapping, uint8_t* message );
                                                                             
/// @brief Calls type specific connection method for sending network messages                                                
/// @param[in] mapping connection reference   
/// @param[in] message message string pointer  
/// @return true on success, false on error  
bool SHM_WriteData( void* mapping, const uint8_t* message );


#endif // IPC_BASE_SHM_H
