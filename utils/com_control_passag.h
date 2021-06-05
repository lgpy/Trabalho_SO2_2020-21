#pragma once

#include "global.h"

#define NamedPipe_NAME TEXT("\\\\.\\pipe\\NPipeCP")
#define MAX_Passag_NAME 30

#define REQ_INIT 0
#define REQ_AIRPORT 1
#define REQ_UPDATE 2

#define RES_ADDED 0
#define RES_AIRPORT_NOTFOUND 1
#define RES_AIRPORT_FOUND 2
#define RES_EMBARKED 3
#define RES_UPDATEDPOS 4
#define RES_DISEMBARKED 5

typedef struct {
	DWORD PId;
	TCHAR Name[MAX_Passag_NAME];
	Coords Coord;
	Coords Dest;
} PassagOriginator;

typedef struct {
	int Type;
	Coords Coord;
} ResponseCP;

typedef struct {
	int Type;
	TCHAR buffer[MAX_BUFFER];
	PassagOriginator Originator;
} RequestCP;
