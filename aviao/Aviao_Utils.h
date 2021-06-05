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
#define SemaphoreReceive_PATTERN TEXT("AviaoRSemaphore-%lu")

#define STATE_AEROPORTO 0
#define STATE_VIAGEM 1

#define DEBUG

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
	HANDLE hMutexMemPar_CA; //shared w/Control
} Mutexes;

typedef struct {
	HANDLE hSemEscrita; // shared
	HANDLE hSemLeitura; // shared
	HANDLE hSemaphoreProduce; // not shared
	HANDLE hSemaphoreReceive; // not shared
} Semaphores;

typedef struct {
	HANDLE hEvent_CA; // not shared
} Events;

typedef struct {
	HANDLE hPThread;
	HANDLE hHBThread;
	HANDLE hVThread;
	HANDLE hRThread;
} Threads;

typedef struct {
	Mutexes mutexes;
	Semaphores semaphores;
	Events events;
	FileMaps filemaps;
	SharedMemory sharedmem;
	Threads threads;
	AviaoOriginator me;
	int terminar;
} Data;


typedef struct {
	BufferCircular* memPar;
	AviaoOriginator* me;
	HANDLE* hSemEscrita; // shared 
	HANDLE* hSemLeitura; // shared
	HANDLE* hMutexMempar; // shared
	HANDLE* hMutexMe; // not shared
	int* terminar;
} DadosHB;

typedef struct {
	BufferCircular* memPar;
	AviaoOriginator* me;

	int rType;
	TCHAR buffer[MAX_BUFFER];

	HANDLE* hSemaphoreProduce; // not shared
	HANDLE* hSemEscrita; // shared 
	HANDLE* hSemLeitura; // shared
	HANDLE* hMutexMempar; // shared
	HANDLE* hMutexMe; // not shared
	int* terminar;
} DadosP;

typedef struct {
	DWORD (*Map)[1000]; // testing if cant make it work just go with mempar
	AviaoOriginator* me;
	HANDLE* hMutexMapa; //shared
	HANDLE* hMutexMe; // not shared

	int * rType;
	HANDLE* hSemaphoreProduce;

	int* terminar;
} DadosV;

typedef struct {
	HANDLE* hEvent_CA; // not shared
	HANDLE* hSemaphoreReceive;
	Response* MemPar_CA;
	HANDLE* hMutexMemPar_CA;
	int* terminar;
} DadosR;


void error(const TCHAR* msg, int exit_code);
void init_dados(Data* dados, DadosHB* dadosHB, DadosP* dadosP, DadosV* dadosV, DadosR* dadosR);
void updatePos(DadosP* dados, int x, int y);
void updatePosV(DadosV* dados, int x, int y);
void updateDes(DadosP* dados, int x, int y);
void requestPos(DadosP* dados, HANDLE* hEvent_CA);
void init(TCHAR* buffer, AviaoOriginator* me);
