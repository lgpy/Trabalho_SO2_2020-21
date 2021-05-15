#include "Aviao_Utils.h"

void error(const TCHAR* msg, int exit_code) {
	_tprintf(TEXT("[FATAL] %s"), msg);
	exit(exit_code);
}

void init_dados(HANDLE* hFM_AC, HANDLE* hFM_CA, Dados* dados, DadosR* dadosR) {
	TCHAR buffer[TAM_BUFFER];
	// Map file - Aviao to Control
	*hFM_AC = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, FileMap_NAME); // Check if FileMapping already exists
	if (*hFM_AC == NULL)
		error(ERR_CONTROL_NOT_RUNNING, EXIT_FAILURE);

	dados->memPar = (BufferCircular*)MapViewOfFile(*hFM_AC, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (dados->memPar == NULL)
		error(ERR_MAP_VIEW_OF_FILE, EXIT_FAILURE);

	// Sync stuff - Aviao to Control
	dados->hSemEscrita = CreateSemaphore(NULL, TAM_BUFFER, TAM_BUFFER, SemEscrita_NAME);
	dados->hSemLeitura = CreateSemaphore(NULL, 0, TAM_BUFFER, SemLeitura_NAME);
	dados->hMutex = CreateMutex(NULL, FALSE, dadosMutex_NAME);
	_stprintf_s(buffer, TAM_BUFFER, ProduceSemaphore_PATTERN, dados->me->PId);
	dados->hSemaphore = CreateSemaphore(NULL, 0, 1, buffer);
	if (dados->hSemEscrita == NULL || dados->hSemLeitura == NULL || dados->hSemaphore == NULL)
		error(ERR_CREATE_SEMAPHORE, EXIT_FAILURE);
	if (dados->hMutex == NULL)
		error(ERR_CREATE_MUTEX, EXIT_FAILURE);
	
	dados->terminar = 0;

	// Map file - Control to Aviao
	_stprintf_s(buffer, TAM_BUFFER, AVIAO_FM_PATTERN, dados->me->PId);
	*hFM_CA = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(Response), buffer);
	if (*hFM_CA == NULL)
		error(ERR_CREATE_FILE_MAPPING, EXIT_FAILURE);

	dadosR->memPar = (Response*)MapViewOfFile(*hFM_CA, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (dadosR->memPar == NULL)
		error(ERR_MAP_VIEW_OF_FILE, EXIT_FAILURE);

	dadosR->memPar->rType = -1;

	// Sync stuff - Control to Aviao
	_stprintf_s(buffer, TAM_BUFFER, AVIAO_REvent_PATTERN, dados->me->PId);
	dadosR->hEvent = CreateEvent(NULL, FALSE, FALSE, buffer); //change to auto reset?
	if (dadosR->hEvent == NULL)
		error(ERR_CREATE_EVENT, EXIT_FAILURE);
}

void updatePos(Dados* dados, int x, int y) {
	dados->cell.rType = REQ_UPDATEPOS;
	dados->me->Coord.x = x;
	dados->me->Coord.y = y;
	ReleaseSemaphore(dados->hSemaphore, 1, NULL);
}

void updateDes(Dados* dados, int x, int y) {
	dados->cell.rType = REQ_UPDATEDES;
	dados->me->Dest.x = x;
	dados->me->Dest.y = y;
	ReleaseSemaphore(dados->hSemaphore, 1, NULL);
}

void requestPos(Dados* dados, DadosR* dadosR) {
	_tprintf(TEXT("Nome do Aeroporto: "));
	_fgetts(dados->cell.buffer, TAM_BUFFER, stdin);
	dados->cell.buffer[_tcslen(dados->cell.buffer) - 1] = '\0';
	dados->cell.rType = REQ_AIRPORT; //REQ_NEW?
	ReleaseSemaphore(dados->hSemaphore, 1, NULL);
	WaitForSingleObject(dadosR->hEvent, INFINITE);
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
