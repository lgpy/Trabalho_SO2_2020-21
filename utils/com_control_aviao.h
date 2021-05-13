#pragma once

#define FileMap_NAME TEXT("FileMap")
#define SemEscrita_NAME TEXT("SemEscrita")
#define SemLeitura_NAME TEXT("SemLeitura")

#define TAM_CBUFFER 10
#define TAM_BUFFER 50

#define REQ_HEARTBEAT 0

typedef struct {
	int x;
	int y;
} Coords;

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


