#include "Aviao_Utils.h"

void error(const TCHAR* msg, int exit_code) {
	_tprintf(TEXT("[FATAL] %s"), msg);
	exit(exit_code);
}

void init_dados(HANDLE* hFM_AC, HANDLE* hFM_CA, DadosP* dadosP, DadosHB* dadosHB, DadosR* dadosR) {
	TCHAR name[TAM_BUFFER];
	// Map file - Aviao to Control
	*hFM_AC = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, FileMap_NAME); // Check if FileMapping already exists
	if (*hFM_AC == NULL)
		error(ERR_CONTROL_NOT_RUNNING, EXIT_FAILURE);

	dadosP->memPar = (BufferCircular*)MapViewOfFile(*hFM_AC, FILE_MAP_ALL_ACCESS, 0, 0, 0);	dadosHB->hMutex = dadosP->memPar;
	if (dadosP->memPar == NULL)
		error(ERR_MAP_VIEW_OF_FILE, EXIT_FAILURE);

	// Sync stuff - Aviao to Control
	dadosP->hSemEscrita = CreateSemaphoreW(NULL, TAM_BUFFER, TAM_BUFFER, SemEscrita_NAME);
	dadosP->hSemLeitura = CreateSemaphoreW(NULL, 0, TAM_BUFFER, SemLeitura_NAME);
	dadosP->hMutex = CreateMutex(NULL, FALSE, dadosMutex_NAME);
	_stprintf_s(name, TAM_BUFFER, ProduceEvent_PATTERN, dadosP->me->PId);
	dadosP->hEvent = CreateEvent(NULL, FALSE, FALSE, name);
	if (dadosP->hSemEscrita == NULL || dadosP->hSemLeitura == NULL)
		error(ERR_CREATE_SEMAPHORE, EXIT_FAILURE);
	if (dadosP->hMutex == NULL)
		error(ERR_CREATE_MUTEX, EXIT_FAILURE);
	if (dadosP->hEvent == NULL)
		error(ERR_CREATE_EVENT, EXIT_FAILURE);
	
	dadosP->terminar = 0;
	dadosHB->terminar = 0;
	dadosHB->hSemEscrita = dadosP->hSemEscrita;
	dadosHB->hSemLeitura = dadosP->hSemLeitura;
	dadosHB->hMutex = dadosP->hMutex;

	// Map file - Control to Aviao
	_stprintf_s(name, TAM_BUFFER, AVIAO_FM_PATTERN, dadosP->me->PId);
	*hFM_CA = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(Response), name);
	if (*hFM_CA == NULL)
		error(ERR_CREATE_FILE_MAPPING, EXIT_FAILURE);

	dadosR->memPar = (Response*)MapViewOfFile(*hFM_CA, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (dadosR->memPar == NULL)
		error(ERR_MAP_VIEW_OF_FILE, EXIT_FAILURE);

	// Sync stuff - Control to Aviao
	_stprintf_s(name, TAM_BUFFER, AVIAO_REvent_PATTERN, dadosP->me->PId);
	dadosR->hEvent = CreateEvent(NULL, FALSE, FALSE, name);
	if (dadosR->hEvent == NULL)
		error(ERR_CREATE_EVENT, EXIT_FAILURE);
}

//#define DEBUG

void init(char* buffer, AviaoOriginator* me) {
	me->PId = GetCurrentProcessId();
	_tprintf(TEXT("PID: %d\n"), me->PId);

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

	_tprintf(TEXT("aeroporto: "));
	_fgetts(buffer, TAM_BUFFER, stdin);
}