#pragma once

#include <windows.h>
#include <tchar.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <time.h>
#include "com_control_passag.h"
#include "err_str.h"


#define STATE_WAITING 0
#define STATE_BOARDED 1

#define _SECOND 10000000

typedef struct {
	HANDLE hRHThread;
	HANDLE hWThread;
	HANDLE hEvent;
	HANDLE hPipe;
	PassagOriginator me;
	BOOL terminar;
	int State;

	int sec;
	HANDLE hWTimer;
	LARGE_INTEGER liDueTime;
} Data;

DWORD WINAPI ThreadResponseHandler(LPVOID param);
void error(const TCHAR* msg, int exit_code);
void init(Data* dados);
void PrintInfo(Data* dados);