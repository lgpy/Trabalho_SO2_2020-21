#include "Aviao_Utils.h"

void error(const TCHAR* msg, int exit_code) {
	_tprintf(TEXT("[FATAL] %s"), msg);
	exit(exit_code);
}

void init_dados(HANDLE* hFileMap, DadosThread* dados, DadosHeartBeatThread* dadosHB) {
	TCHAR eventname[TAM_BUFFER];
	*hFileMap = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, FileMap_NAME); // Check if FileMapping already exists
	if (*hFileMap == NULL)
		error(ERR_CONTROL_NOT_RUNNING, EXIT_FAILURE);

	dados->memPar = (BufferCircular*)MapViewOfFile(*hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);	dadosHB->hMutex = dados->memPar;
	if (dados->memPar == NULL)
		error(ERR_MAP_VIEW_OF_FILE, EXIT_FAILURE);

	dados->hSemEscrita = CreateSemaphoreW(NULL, TAM_BUFFER, TAM_BUFFER, SemEscrita_NAME);		dadosHB->hSemEscrita = dados->hSemEscrita;
	dados->hSemLeitura = CreateSemaphoreW(NULL, 0, TAM_BUFFER, SemLeitura_NAME);				dadosHB->hMutex = dados->hMutex;
	dados->hMutex = CreateMutex(NULL, FALSE, dadosMutex_NAME);									dadosHB->hMutex = dados->hMutex;
	_stprintf_s(eventname, TAM_BUFFER, dadosEvent_PATTERN, dados->me->PId);
	dados->hEvent = CreateEvent(NULL, FALSE, FALSE, eventname);
	if (dados->hSemEscrita == NULL || dados->hSemLeitura == NULL)
		error(ERR_CREATE_SEMAPHORE, EXIT_FAILURE);
	if (dados->hMutex == NULL)
		error(ERR_CREATE_MUTEX, EXIT_FAILURE);
	if (dados->hEvent == NULL)
		error(ERR_CREATE_EVENT, EXIT_FAILURE);

	dados->terminar = 0;																		dadosHB->terminar = dados->terminar;
}