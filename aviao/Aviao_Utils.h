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
	HANDLE hSemaphore;
	AviaoOriginator* me;
	CelulaBuffer cell;
	int terminar;
} Dados;

typedef struct {
	Response* memPar;
	HANDLE hEvent;
} DadosR;

typedef struct {
	Dados * Dados;
	DadosR * dadosR;
	int terminar;
} DadosV;

void error(const TCHAR* msg, int exit_code);
void init_dados(HANDLE* hFM_AC, HANDLE* hFM_CA, Dados* dados, DadosR* dadosR);
void updatePos(Dados* dados, int x, int y);
void updateDes(Dados* dados, int x, int y);
void requestPos(Dados* dados, DadosR* dadosR);
void init(TCHAR* buffer, AviaoOriginator* me);
