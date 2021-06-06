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




typedef struct {
	HANDLE hRHThread;
	HANDLE hEvent;
	HANDLE hPipe;
	PassagOriginator me;
	BOOL terminar;
} Data;

void error(const TCHAR* msg, int exit_code);
void init(Data* dados);
void PrintInfo(Data* dados);