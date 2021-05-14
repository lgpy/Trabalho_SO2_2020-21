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
#define ProduceEvent_PATTERN TEXT("AviaoPEvent-%d")

#define state_AEROPORTO 0
#define state_EMVIAGEM 1

typedef struct {
	BufferCircular* memPar;
	HANDLE hSemEscrita; // posições que estão vazias
	HANDLE hSemLeitura; // posições para ser lidas
	HANDLE hMutex;
	AviaoOriginator* me;
	int terminar;
} DadosHB;

typedef struct {
	BufferCircular* memPar;
	HANDLE hSemEscrita; // posições que estão vazias
	HANDLE hSemLeitura; // posições para ser lidas
	HANDLE hMutex;
	HANDLE hEvent;
	AviaoOriginator* me;
	CelulaBuffer cell;
	int terminar;
} DadosP;

typedef struct {
	Response* memPar;
	HANDLE hEvent;
} DadosR;

void error(const TCHAR* msg, int exit_code);
void init_dados(HANDLE* hFM_AC, HANDLE* hFM_CA, DadosP* dadosP, DadosHB* dadosHB, DadosR* dadosR);
void init(char* buffer, AviaoOriginator* me);