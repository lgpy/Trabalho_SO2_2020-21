#pragma once

#include "global.h"

#define FileMap_NAME TEXT("ControlFM")
#define SemEscrita_NAME TEXT("SemEscrita")
#define SemLeitura_NAME TEXT("SemLeitura")
#define FileMapCA_PATTERN TEXT("AviaoFM-%lu")
#define Event_CA_PATTERN TEXT("AviaoPEvent-%lu")
#define MutexMempar_CA_PATTERN TEXT("MutexMemparCA-%lu")

#define MAX_CBUFFER 10

#define REQ_AIRPORT 1
#define REQ_HEARTBEAT 2
#define REQ_UPDATEPOS 3
#define REQ_UPDATEDES 4
#define REQ_REACHEDDES 5
#define REQ_EMBARK 6

//#define RES_AIRPORT_AVIOES_FULL 0
#define RES_CONTROL_SHUTDOWN 0
#define RES_AIRPORT_NOTFOUND 1
#define RES_AIRPORT_FOUND 2
#define RES_LOCATION_UPDATED 3

typedef struct {
	int rType;
	Coords Coord;
} Response;

typedef struct {
	DWORD PId;
	int Seats;
	int Speed;
	Coords Coord;
	Coords Dest;
	int State;
} AviaoOriginator;

typedef struct {
	AviaoOriginator Originator;
	int rType;
	TCHAR buffer[MAX_BUFFER];
} CelulaBuffer;

typedef struct {
	int pWrite;
	int pRead;
	DWORD Map[1000][1000];
	CelulaBuffer buffer[MAX_CBUFFER];
} BufferCircular;


