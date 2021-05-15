#pragma once
#include <windows.h>
#include <tchar.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <time.h>
#include "com_control_aviao.h"
#include "err_str.h"

#define DEFAULT_MAX_AVIOES 10
#define DEFAULT_MAX_AEROPORTOS 5

#define ControlMutex_NAME TEXT("ControlMutex")

#define REG_KEY_PATH TEXT("SOFTWARE\\TPSO2")
#define REG_MAX_AVIOES_KEY_NAME TEXT("MAX_AVIOES")
#define REG_MAX_AEROPORTOS_KEY_NAME TEXT("MAX_AEROPORTOS")
#define REG_MAX_AEROPORTOS_KEY_NAME TEXT("MAX_AEROPORTOS")

typedef struct {
	DWORD PId;
	int Seats;
	int Speed;
	Coords Coord;
	time_t lastHB; //last Heartbeat in UNIX time

	HANDLE hFileMap;
	HANDLE hEvent;
	Response* memPar;
} Aviao;

typedef struct {
	TCHAR Name[30];
	Coords Coord;
} Aeroporto;

typedef struct {
	BufferCircular* memPar;
	HANDLE hSemEscrita; // posi��es que est�o vazias
	HANDLE hSemLeitura; // posi��es para ser lidas
	HANDLE hMutex;
	int terminar;

	Aviao* Avioes;
	Aeroporto* Aeroportos;
	int nAvioes, nAeroportos;
	int MAX_AVIOES, MAX_AEROPORTOS;
} DadosThread;

void error(const TCHAR* msg, int exit_code);
DWORD getRegVal(const TCHAR* ValueName, const int Default);
void init_dados(HANDLE* hFileMap, DadosThread* dados);
void PrintMenu(Aviao* Avioes, const int nAvioes, Aeroporto* Aeroportos, const int nAeroportos);
void Handler(DadosThread* dados, CelulaBuffer* cel);
int FindAviaobyPId(Aviao* Avioes, const int nAvioes, int PId);
int FindAeroportobyName(Aeroporto* Aeroportos, const int nAeroportos, TCHAR* name);
int AddAviao(Aviao* Avioes, int* nAvioes, const int Max_Avioes, AviaoOriginator* newAviao);
int AddAeroporto(Aeroporto* Aeroportos, int* nAeroportos, const int Max_Aeroportos, TCHAR* name, Coords coords);