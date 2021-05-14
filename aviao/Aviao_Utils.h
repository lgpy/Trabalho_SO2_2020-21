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

#define dadosMutex_NAME TEXT("AviaoDadosMutex")
#define dadosEvent_PATTERN TEXT("AviaoDadosEvent-%d")

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
	HANDLE hEvent;
	CelulaBuffer cell;
} DadosThread;

void error(const TCHAR* msg, int exit_code);
void init_dados(HANDLE* hFileMap, DadosThread* dados, DadosHeartBeatThread* dadosHB);