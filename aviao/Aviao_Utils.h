#pragma once
#include <windows.h>
#include <tchar.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include "com_control_aviao.h"
#include "err_str.h"
#include "SO2_TP_DLL_2021.h"

#define dadosMutex_NAME TEXT("dadosMutex")

typedef struct {
	BufferCircular* memPar;
	HANDLE hSemEscrita; // posições que estão vazias
	HANDLE hSemLeitura; // posições para ser lidas
	HANDLE hMutex;
	int terminar;
	AviaoOriginator* me;
} DadosHeartBeatThread;

typedef struct {
	BufferCircular* memPar;
	HANDLE hSemEscrita; // posições que estão vazias
	HANDLE hSemLeitura; // posições para ser lidas
	HANDLE hMutex;
	int terminar;
	AviaoOriginator* me;
} DadosThread;