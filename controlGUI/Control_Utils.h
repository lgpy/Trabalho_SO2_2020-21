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
#include "resource.h"

#define DEFAULT_MAX_AVIOES 10
#define DEFAULT_MAX_AEROPORTOS 5

#define MutexAvioes_NAME TEXT("MutexAvioes")
#define MutexAeroportos_NAME TEXT("MutexAeroportos")
#define MutexPassageiros_NAME TEXT("MutexPassageiros")


#define REG_KEY_PATH TEXT("SOFTWARE\\TPSO2")
#define REG_MAX_AVIOES_KEY_NAME TEXT("MAX_AVIOES")
#define REG_MAX_AEROPORTOS_KEY_NAME TEXT("MAX_AEROPORTOS")
#define REG_MAX_AEROPORTOS_KEY_NAME TEXT("MAX_AEROPORTOS")

//#define DEBUG

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
	TCHAR Name[MAX_BUFFER];
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
	BOOL isActive;
	Coords coords;
	TCHAR buffer[100];
} HOVEREVENT;

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

	HANDLE hFileMap;
	HANDLE hThread;
	HANDLE hHBCThread;
	HANDLE hNPThread;

	GUI gui;
} Dados;

typedef struct {
	Dados* dados;
	Passageiro* Passageiro;
} DadosPassag;


#include "Control_Utils.h"

//INIT
void init_dados(Dados* dados);

//THREADS
DWORD WINAPI ThreadConsumidor(LPVOID param);
DWORD WINAPI ThreadHBChecker(LPVOID param);
DWORD WINAPI ThreadPassag(LPVOID param);
DWORD WINAPI ThreadNewPassag(LPVOID param);

//AC Handler
void Handler(Dados* dados, CelulaBuffer* cel);

//UTILS
DWORD getRegVal(Dados* dados, const TCHAR* ValueName, const int Default);
void error(Dados* dados, const TCHAR* msg, int exit_code);

//GUI
BOOL HoverEvent(Dados* dados, int xPos, int yPos, TCHAR* buffer);
BOOL ClickEvent(Dados* dados, int xPos, int yPos, TCHAR* buffer);
int translateCoord(int coord);
void fillAvioes(Dados* dados, HWND hList, Aeroporto* aeroporto);
void fillPassageiros(Dados* dados, HWND hList, Aeroporto* aeroporto, Aviao* aviao);
void fillAeroportos(Dados* dados, HWND hList);
void refresh(Dados* dados, HWND hWnd);

//AVIOES
int FindAviaobyPId(Dados* dados, DWORD PId);
int AddAviao(Dados* dados, AviaoOriginator* newAviao);
void RemoveAviao(Dados* dados, int index);
void AviaoToString(Aviao* aviao, TCHAR* buffer, const int bufferSZ);

//AEROPORTOS
int FindAeroportobyName(Dados* dados, TCHAR* name);
int FindAeroportobyCoords(Dados* dados, Coords coords);
int AddAeroporto(Dados* dados, Aeroporto* newAeroporto);
int AeroportoisIsolated(Dados* dados, Coords coords);
void AeroportoToString(Aeroporto* aeroporto, TCHAR* buffer, const int bufferSZ);

//PASSAGEIROS
int FindPassageirobyPId(Dados* dados, DWORD PId);
int AddPassageiro(Dados* dados, HANDLE hPipe);
void RemovePassageiro(Dados* dados, int index);
void PassageiroToString(Passageiro* passageiro, TCHAR* buffer, const int bufferSZ);
int Embark(Dados* dados, Aviao* aviao);
int Disembark(Dados* dados, Aviao* aviao);
void Crash(Dados* dados, Aviao* aviao);
int ReachedDest(Dados* dados, Aviao* aviao);
void UpdateEmbarked(Dados* dados, DWORD PId, Coords toUpdate);