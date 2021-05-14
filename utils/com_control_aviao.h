#pragma once

#define FileMap_NAME TEXT("ControlFM")
#define SemEscrita_NAME TEXT("SemEscrita")
#define SemLeitura_NAME TEXT("SemLeitura")
#define AVIAO_FM_PATTERN TEXT("AviaoFM-%d")
#define AVIAO_REvent_PATTERN TEXT("AviaoPEvent-%d")

#define TAM_CBUFFER 10
#define TAM_BUFFER 50

#define REQ_AIRPORT 1
#define REQ_HEARTBEAT 2

#define RES_AIRPORT_NOTFOUND 1
#define RES_AIRPORT_FOUND 2

typedef struct {
	int x;
	int y;
} Coords;

typedef struct {
	int rType;
	//DWORD PId;
	Coords Coord;
} Response;

typedef struct {
	DWORD PId;
	int Seats;
	int Speed;
	//Coords Coord;
} AviaoOriginator;

typedef struct {
	AviaoOriginator Originator;
	int rType;
	//Coords Coord;
	TCHAR buffer[TAM_BUFFER];
} CelulaBuffer;

typedef struct {
	int pWrite;
	int pRead;
	CelulaBuffer buffer[TAM_CBUFFER];
	DWORD Map[1000][1000];
} BufferCircular;


