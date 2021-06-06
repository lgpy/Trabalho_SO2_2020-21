#pragma once

#define ERR_CONTROL_ALREADY_RUNNING TEXT("Another control is already in execution.")
#define ERR_CONTROL_NOT_RUNNING TEXT("Control is not in execution.")
#define ERR_CONTROL_FULL TEXT("Control is full.")

#define ERR_INSUFFICIENT_MEMORY TEXT("Insufficient memory available.")

#define ERR_OPEN_FILE_MAPPING TEXT("Error while opening file mapping.")
#define ERR_CREATE_FILE_MAPPING TEXT("Error while creating file mapping.")
#define ERR_MAP_VIEW_OF_FILE TEXT("Error while mapping view of file.")

#define ERR_REG_CREATE_KEY TEXT("Error while creating/opening registry key.")
#define ERR_REG_KEY_TYPE TEXT("Registry key type is not DWORD.")
#define ERR_REG_SET_KEY TEXT("Error while setting registry key.")

#define ERR_CREATE_SEMAPHORE TEXT("Error while creating a semaphore.")
#define ERR_CREATE_MUTEX TEXT("Error while creating a mutex.")
#define ERR_CREATE_EVENT TEXT("Error while creating a event.")
#define ERR_CREATE_THREAD TEXT("Error while creating a thread.")
#define ERR_OPEN_EVENT TEXT("Error while oppening a event.")

#define ERR_WAITING_PIPE TEXT("Error while waiting for a named pipe.")
#define ERR_CONNECT_PIPE TEXT("Error while connecting to a named pipe.")
#define ERR_WRITE_PIPE TEXT("Error while writing to a named pipe.")
#define ERR_READ_PIPE TEXT("Error while reading a named pipe.")
#define ERR_CREATE_PIPE TEXT("Error while creating a named pipe.")


#define ERR_CREATE_WAITABLETIMER TEXT("Error while creating waitable timer.")
#define ERR_SET_WAITABLETIMER TEXT("Error while setting waitable timer.")
