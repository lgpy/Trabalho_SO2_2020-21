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

#define MutexMempar_NAME TEXT("MutexMempar")
#define MutexMapa_NAME TEXT("MutexMapa")
#define MutexMe_PATTERN TEXT("MutexMe-%lu")

#define SemaphoreProduce_PATTERN TEXT("AviaoPSemaphore-%lu")

#define STATE_AEROPORTO 0
#define STATE_VIAGEM 1


typedef struct {
	BufferCircular* MemPar_AC; // shared
	Response* MemPar_CA; // not shared
} SharedMemory;

typedef struct {
	HANDLE hFM_AC;
	HANDLE hFM_CA;
} FileMaps;

typedef struct {
	HANDLE hMutexMempar; // shared
	HANDLE hMutexMe; // not shared
	HANDLE hMutexMapa; //shared
} Mutexes;

typedef struct {
	HANDLE hSemEscrita; // shared
	HANDLE hSemLeitura; // shared
	HANDLE hSemaphoreProduce; // not shared
} Semaphores;

typedef struct {
	HANDLE hEvent_CA; // not shared
} Events;

typedef struct {
	HANDLE hPThread;
	HANDLE hHBThread;
	HANDLE hVThread;
} Threads;

typedef struct {
	Mutexes mutexes;
	Semaphores semaphores;
	Events events;
	FileMaps filemaps;
	SharedMemory sharedmem;
	Threads threads;
	AviaoOriginator me;
} Data;

/*
typedef struct {
	Response* memPar;
	HANDLE hEvent;
} DadosR;

typedef struct {
	Dados* dados;
	DadosR* dadosR;
	HANDLE hMutexMapa;
	int terminar;
};*/

typedef struct {
	BufferCircular* memPar;
	AviaoOriginator* me;
	HANDLE* hSemEscrita; // shared 
	HANDLE* hSemLeitura; // shared
	HANDLE* hMutexMempar; // shared
	HANDLE* hMutexMe; // not shared
	int terminar;
} DadosHB;

typedef struct {
	BufferCircular* memPar;
	AviaoOriginator* me;

	int rType;
	TCHAR buffer[TAM_BUFFER];

	HANDLE* hSemaphoreProduce; // not shared
	HANDLE* hSemEscrita; // shared 
	HANDLE* hSemLeitura; // shared
	HANDLE* hMutexMempar; // shared
	HANDLE* hMutexMe; // not shared
	int terminar;
} DadosP;

typedef struct {
	DWORD (*Map)[1000]; // testing if cant make it work just go with mempar
	AviaoOriginator* me;
	HANDLE* hMutexMapa; //shared
	HANDLE* hMutexMe; // not shared

	int * rType;
	HANDLE* hSemaphoreProduce;
} DadosV;


void error(const TCHAR* msg, int exit_code);
void init_dados(Data* dados, DadosHB* dadosHB, DadosP* dadosP, DadosV* dadosV); 
void updatePos(DadosP* dados, int x, int y);
void updatePosV(DadosV* dados, int x, int y);
void updateDes(DadosP* dados, int x, int y);
void requestPos(DadosP* dados, HANDLE* hEvent_CA);
void init(TCHAR* buffer, AviaoOriginator* me);
