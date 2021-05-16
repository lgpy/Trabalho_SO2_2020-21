#include "Aviao_Utils.h"

void error(const TCHAR* msg, int exit_code) {
	_tprintf(TEXT("[FATAL] %s"), msg);
	exit(exit_code);
}

void init_dados(Data* dados, DadosHB* dadosHB, DadosP* dadosP, DadosV* dadosV, DadosR* dadosR) {
	TCHAR buffer[TAM_BUFFER];
	// Map file - Aviao to Control
	dados->filemaps.hFM_AC = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, FileMap_NAME); // Check if FileMapping exists
	if (dados->filemaps.hFM_AC == NULL)
		error(ERR_CONTROL_NOT_RUNNING, EXIT_FAILURE);

	// Shared Memory - Aviao to Control
	dados->sharedmem.MemPar_AC = (BufferCircular*)MapViewOfFile(dados->filemaps.hFM_AC, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (dados->sharedmem.MemPar_AC == NULL)
		error(ERR_MAP_VIEW_OF_FILE, EXIT_FAILURE);

	// Map file - Control to Aviao
	_stprintf_s(buffer, TAM_BUFFER, FileMapCA_PATTERN, dados->me.PId);
	dados->filemaps.hFM_CA = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(Response), buffer);
	if (dados->filemaps.hFM_CA == NULL)
		error(ERR_CREATE_FILE_MAPPING, EXIT_FAILURE);

	// Shared Memory - Control to Aviao
	dados->sharedmem.MemPar_CA = (Response*)MapViewOfFile(dados->filemaps.hFM_CA, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (dados->sharedmem.MemPar_CA == NULL)
		error(ERR_MAP_VIEW_OF_FILE, EXIT_FAILURE);
	dados->sharedmem.MemPar_CA->rType = 0; // needed?

	// Synchronization - Semaphores
	_stprintf_s(buffer, TAM_BUFFER, SemaphoreProduce_PATTERN, dados->me.PId);
	dados->semaphores.hSemaphoreProduce = CreateSemaphore(NULL, 0, 1, buffer);
	_stprintf_s(buffer, TAM_BUFFER, SemaphoreReceive_PATTERN, dados->me.PId);
	dados->semaphores.hSemaphoreReceive = CreateSemaphore(NULL, 0, 1, buffer);
	dados->semaphores.hSemEscrita = CreateSemaphore(NULL, TAM_BUFFER, TAM_BUFFER, SemEscrita_NAME);
	dados->semaphores.hSemLeitura = CreateSemaphore(NULL, 0, TAM_BUFFER, SemLeitura_NAME);
	if (dados->semaphores.hSemEscrita == NULL || dados->semaphores.hSemLeitura == NULL || dados->semaphores.hSemaphoreProduce == NULL)
		error(ERR_CREATE_SEMAPHORE, EXIT_FAILURE);

	// Synchronization - Mutexes
	_stprintf_s(buffer, TAM_BUFFER, MutexMe_PATTERN, dados->me.PId);
	dados->mutexes.hMutexMe = CreateMutex(NULL, FALSE, buffer);
	dados->mutexes.hMutexMempar = CreateMutex(NULL, FALSE, MutexMempar_NAME);
	dados->mutexes.hMutexMapa = CreateMutex(NULL, FALSE, MutexMapa_NAME);
	if (dados->mutexes.hMutexMe == NULL || dados->mutexes.hMutexMempar == NULL || dados->mutexes.hMutexMapa == NULL)
		error(ERR_CREATE_MUTEX, EXIT_FAILURE);

	_stprintf_s(buffer, TAM_BUFFER, Event_CA_PATTERN, dados->me.PId);
	dados->events.hEvent_CA = CreateEvent(NULL, FALSE, FALSE, buffer); //change to auto reset?
	if (dados->events.hEvent_CA == NULL)
		error(ERR_CREATE_EVENT, EXIT_FAILURE);

	//pointer stuff
	dadosHB->memPar = dados->sharedmem.MemPar_AC;
	dadosHB->me = &dados->me;
	dadosHB->hSemEscrita = &dados->semaphores.hSemEscrita; // shared 
	dadosHB->hSemLeitura = &dados->semaphores.hSemLeitura; // shared
	dadosHB->hMutexMempar = &dados->mutexes.hMutexMempar; // shared
	dadosHB->hMutexMe = &dados->mutexes.hMutexMe; // not shared
	
	dadosP->memPar = dados->sharedmem.MemPar_AC;
	dadosP->me = &dados->me;
	dadosP->hSemaphoreProduce = &dados->semaphores.hSemaphoreProduce; // not shared
	dadosP->hSemEscrita = &dados->semaphores.hSemEscrita; // shared 
	dadosP->hSemLeitura = &dados->semaphores.hSemLeitura; // shared
	dadosP->hMutexMempar = &dados->mutexes.hMutexMempar; // shared
	dadosP->hMutexMe = &dados->mutexes.hMutexMe; // not shared

	dadosV->Map = &dados->sharedmem.MemPar_AC->Map; // testing if cant make it work just go with mempar
	dadosV->me = &dados->me;
	dadosV->hMutexMapa = &dados->mutexes.hMutexMapa; //shared
	dadosV->hMutexMe = &dados->mutexes.hMutexMe; // not shared
	dadosV->rType = &dadosP->rType;
	dadosV->hSemaphoreProduce = &dados->semaphores.hSemaphoreProduce;

	dadosR->hEvent_CA = &dados->events.hEvent_CA; // not shared
	dadosR->hSemaphoreReceive = &dados->semaphores.hSemaphoreReceive;
	dadosR->MemPar_CA = dados->sharedmem.MemPar_CA;

	//other
	dadosHB->terminar = 0;
	dadosP->terminar = 0;
	dadosR->terminar = 0;
}


void updatePos(DadosP* dados, int x, int y) {
	WaitForSingleObject(*dados->hMutexMe, INFINITE);
	dados->me->Coord.x = x;
	dados->me->Coord.y = y;
	ReleaseMutex(*dados->hMutexMe);
	dados->rType = REQ_UPDATEPOS;
	ReleaseSemaphore(*dados->hSemaphoreProduce, 1, NULL);
}

void updatePosV(DadosV* dados, int x, int y) {
	WaitForSingleObject(*dados->hMutexMe, INFINITE);
	dados->me->Coord.x = x;
	dados->me->Coord.y = y;
	ReleaseMutex(*dados->hMutexMe);
	*dados->rType = REQ_UPDATEPOS;
	ReleaseSemaphore(*dados->hSemaphoreProduce, 1, NULL);
}

void updateDes(DadosP* dados, int x, int y) {
	WaitForSingleObject(*dados->hMutexMe, INFINITE);
	dados->me->Dest.x = x;
	dados->me->Dest.y = y;
	ReleaseMutex(*dados->hMutexMe);
	dados->rType = REQ_UPDATEDES;
	ReleaseSemaphore(*dados->hSemaphoreProduce, 1, NULL);
}

void requestPos(DadosP* dados, HANDLE* hEvent_CA) {
	dados->rType = REQ_AIRPORT; //REQ_NEW?
	_tprintf(TEXT("Nome do Aeroporto: "));
	_fgetts(dados->buffer, TAM_BUFFER, stdin);
	dados->buffer[_tcslen(dados->buffer) - 1] = '\0';
	ReleaseSemaphore(*dados->hSemaphoreProduce, 1, NULL);
}

#define DEBUG

void init(TCHAR* buffer, AviaoOriginator* me) {
	me->PId = GetCurrentProcessId();

#ifndef DEBUG
	_tprintf(TEXT("Seats: "));
	_fgetts(buffer, TAM_BUFFER, stdin);
	me->Seats = _tstoi(buffer);

	_tprintf(TEXT("Speed: "));
	_fgetts(buffer, TAM_BUFFER, stdin);
	me->Speed = _tstoi(buffer);
#else
	me->Seats = 10;
	me->Speed = 1;
#endif // DEBUG
}
