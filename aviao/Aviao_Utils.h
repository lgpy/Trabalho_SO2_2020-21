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
#define ProduceSemaphore_PATTERN TEXT("AviaoPSemaphore-%lu")

#define STATE_AEROPORTO 0
#define STATE_VIAGEM 1

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
	HANDLE hSemaphore;
	AviaoOriginator* me;
	CelulaBuffer cell;
	int terminar;
} DadosP;

typedef struct {
	Response* memPar;
	HANDLE hEvent;
} DadosR;

typedef struct {
	DadosP * dadosP;
	DadosR * dadosR;
	int terminar;
} DadosV;

void error(const TCHAR* msg, int exit_code);
void init_dados(HANDLE* hFM_AC, HANDLE* hFM_CA, DadosP* dadosP, DadosHB* dadosHB, DadosR* dadosR);
void updatePos(DadosP* dadosP, DadosR* dadosR, AviaoOriginator* me, int x, int y);
void requestPos(DadosP* dadosP, DadosR* dadosR);
void init(TCHAR* buffer, AviaoOriginator* me);