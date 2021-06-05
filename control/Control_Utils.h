#pragma once

#include <windows.h>
#include <tchar.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <time.h>
#include "com_control_aviao.h"
#include "com_control_passag.h"
#include "err_str.h"

#define DEFAULT_MAX_AVIOES 10
#define DEFAULT_MAX_AEROPORTOS 5
#define MAX_PASSAGEIROS 5

#define MutexAvioes_NAME TEXT("MutexAvioes")
#define MutexAeroportos_NAME TEXT("MutexAeroportos")
#define MutexPassageiros_NAME TEXT("MutexPassageiros")


#define REG_KEY_PATH TEXT("SOFTWARE\\TPSO2")
#define REG_MAX_AVIOES_KEY_NAME TEXT("MAX_AVIOES")
#define REG_MAX_AEROPORTOS_KEY_NAME TEXT("MAX_AEROPORTOS")
#define REG_MAX_AEROPORTOS_KEY_NAME TEXT("MAX_AEROPORTOS")

#define DEBUG

typedef struct {
	DWORD PId;

	int Seats;
	int Speed;

	Coords Coord;
	Coords Dest;

	time_t lastHB; //last Heartbeat in UNIX time

	HANDLE hFileMap;
	HANDLE hEvent;
	Response* memPar;
	HANDLE hMutexMemPar;
} Aviao;

typedef struct {
	TCHAR Name[30];
	Coords Coord;
} Aeroporto;

typedef struct {
	BufferCircular* memPar;
	HANDLE hSemEscrita; // posições que estão vazias
	HANDLE hSemLeitura; // posições para ser lidas

	int terminar;
	int aceitarAvioes;

	Passageiro Passageiros[MAX_PASSAGEIROS];
	HANDLE hMutexPassageiros;
	HANDLE hPassagEvents[MAX_PASSAGEIROS];
	HANDLE hThreadReqHandler;

	Aviao* Avioes;
	HANDLE hMutexAvioes;

	Aeroporto* Aeroportos;
	HANDLE hMutexAeroportos;

	int nAvioes, nAeroportos, nPassageiros;
	int MAX_AVIOES, MAX_AEROPORTOS;
} Dados;

typedef struct {
	Dados* dados;
	int index;
	int terminar;
} DadosPassag;

typedef struct {
	HANDLE hPipe;
	OVERLAPPED overlap;
} PIPEINST;

typedef struct {
	DWORD PId;
	TCHAR Name[MAX_Passag_NAME];
	DWORD AviaoPId;
	Coords Coord;
	Coords Dest;

	PIPEINST Pipe;
	DadosPassag dadosPassag;
	HANDLE hThread;
} Passageiro;


void error(const TCHAR* msg, int exit_code);
DWORD getRegVal(const TCHAR* ValueName, const int Default);
void init_dados(Dados* dados, HANDLE* hFileMap);
void PrintInfo(Dados* dados);
void PrintMenu(Dados* dados);
void Handler(Dados* dados, CelulaBuffer* cel);
int FindAviaobyPId(Dados* dados, DWORD PId);
int FindAeroportobyName(Dados* dados, TCHAR* name);
int AeroportoisIsolated(Dados* dados, Coords coords);
int AddAviao(Dados* dados, AviaoOriginator* newAviao);
int AddAeroporto(Dados* dados, Aeroporto* newAeroporto);
void RemoveAviao(Dados* dados, int index);
DWORD WINAPI ThreadConsumidor(LPVOID param);
DWORD WINAPI ThreadHBChecker(LPVOID param);
