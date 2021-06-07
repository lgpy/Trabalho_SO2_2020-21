#pragma once

#include <windows.h>
#include <windowsx.h>
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

#define MutexAvioes_NAME TEXT("MutexAvioes")
#define MutexAeroportos_NAME TEXT("MutexAeroportos")
#define MutexPassageiros_NAME TEXT("MutexPassageiros")


#define REG_KEY_PATH TEXT("SOFTWARE\\TPSO2")
#define REG_MAX_AVIOES_KEY_NAME TEXT("MAX_AVIOES")
#define REG_MAX_AEROPORTOS_KEY_NAME TEXT("MAX_AEROPORTOS")
#define REG_MAX_AEROPORTOS_KEY_NAME TEXT("MAX_AEROPORTOS")

#define DEBUG

#define AVIAO_STATE_INIT 0
#define AVIAO_STATE_READY 1
#define AVIAO_STATE_FLYING 2

#define PASSAG_STATE_INIT 0
#define PASSAG_STATE_READY 1
#define PASSAG_STATE_WAITING 2
#define PASSAG_STATE_FLYING 3
#define PASSAG_STATE_REACHED 4

typedef struct {
	DWORD PId;
	int state;

	int Seats;
	int Speed;
	int nPassengers;

	Coords Origin;
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
	int nAvioes;
	int nPassageiros;
} Aeroporto;

typedef struct {
	DWORD PId;
	int state;
	TCHAR Name[MAX_Passag_NAME];
	DWORD AviaoPId;

	Coords Coord;
	Coords Dest;

	HANDLE hPipe;
	HANDLE hThread;
	HANDLE hEvent;
	BOOL ready;
} Passageiro;

typedef struct {
	HBITMAP hBmpAviao;
	BITMAP bmpAviao;
	HDC bmpDCAviao;

	HBITMAP hBmpAeroporto;
	BITMAP bmpAeroporto;
	HDC bmpDCAeroporto;


	HWND hWnd;
	HDC memDC;
	HBITMAP hBitmapDB;
} GUI;

typedef struct {
	BufferCircular* memPar;
	HANDLE hSemEscrita; // posições que estão vazias
	HANDLE hSemLeitura; // posições para ser lidas

	BOOL terminar;
	int aceitarAvioes;

	Aviao* Avioes;
	Aeroporto* Aeroportos;
	Passageiro* Passageiros;
	HANDLE hMutexAvioes;
	HANDLE hMutexAeroportos;
	HANDLE hMutexPassageiros;
	int nAvioes, nAeroportos, nPassageiros;
	int MAX_AVIOES, MAX_AEROPORTOS, MAX_PASSAGEIROS;

	GUI gui;
} Dados;

typedef struct {
	Dados* dados;
	Passageiro* Passageiro;
} DadosPassag;


DWORD WINAPI ThreadConsumidor(LPVOID param);
DWORD WINAPI ThreadHBChecker(LPVOID param);
DWORD WINAPI ThreadNewPassag(LPVOID param);

void error(const TCHAR* msg, int exit_code);
DWORD getRegVal(const TCHAR* ValueName, const int Default);
void init_dados(Dados* dados, HANDLE* hFileMap);
void PrintInfo(Dados* dados);
void PrintMenu(Dados* dados);
void Handler(Dados* dados, CelulaBuffer* cel);

int AddAviao(Dados* dados, AviaoOriginator* newAviao);
int FindAviaobyPId(Dados* dados, DWORD PId);
void RemoveAviao(Dados* dados, int index);

int AddAeroporto(Dados* dados, Aeroporto* newAeroporto);
int FindAeroportobyName(Dados* dados, TCHAR* name);
int AeroportoisIsolated(Dados* dados, Coords coords);

int AddPassageiro(Dados* dados, HANDLE hPipe);
int FindPassageirobyPId(Dados* dados, DWORD PId);
void RemovePassageiro(Dados* dados, int index);
int Embark(Dados* dados, Aviao* aviao);
int Disembark(Dados* dados, Aviao* aviao);
void Crash(Dados* dados, Aviao* aviao);
int ReachedDest(Dados* dados, Aviao* aviao);
void UpdateEmbarked(Dados* dados, DWORD PId, Coords toUpdate);