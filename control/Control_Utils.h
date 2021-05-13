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
	//Coords Coord;
	time_t lastHB;
} Aviao;

typedef struct {
	char Name[30];
	Coords Coord;
} Aeroporto;

typedef struct {
	BufferCircular* memPar;
	HANDLE hSemEscrita; // posições que estão vazias
	HANDLE hSemLeitura; // posições para ser lidas
	HANDLE hMutex;
	int terminar;

	Aviao* Avioes;
	Aeroporto* Aeroportos;
	int nAvioes, nAeroportos;
	int MAX_AVIOES, MAX_AEROPORTOS;
} DadosThread;

void error(const TCHAR* msg, int exit_code);
DWORD getRegVal(const char* ValueName, const int Default);
void init_dados(HANDLE* hFileMap, DadosThread* dados);
int FindAviaobyPId(Aviao* Avioes, int nAvioes, int PId);
int AddAviao(Aviao* Avioes, int nAvioes, Aviao* newAviao);