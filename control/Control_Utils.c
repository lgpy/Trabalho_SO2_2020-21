#include "Control_Utils.h"

void error(const TCHAR* msg, int exit_code) {
	_tprintf(TEXT("[FATAL] %s"), msg);
	exit(exit_code);
}

DWORD getRegVal(const TCHAR* ValueName, const int Default) {
	HKEY key;
	LSTATUS res;
	DWORD type, value = 0, size = sizeof(value);
	res = RegCreateKeyEx(HKEY_CURRENT_USER, REG_KEY_PATH, 0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &key, NULL); // REG_OPTION_VOLATILE to be deleted on system reset.
	if (res != ERROR_SUCCESS)
		error(ERR_REG_CREATE_KEY, EXIT_FAILURE);

	res = RegQueryValueEx(key, ValueName, 0, &type, (LPBYTE)&value, &size);
	if (res == ERROR_SUCCESS) {
		if (type != REG_DWORD)
			error(ERR_REG_KEY_TYPE, EXIT_FAILURE);
		_tprintf(TEXT("[SETUP] Found %s = %d.\n"), ValueName, value);
	}
	else if (res == ERROR_MORE_DATA)
		error(ERR_REG_KEY_TYPE, EXIT_FAILURE);
	else if (res == ERROR_FILE_NOT_FOUND) {
		value = Default;
		res = RegSetValueEx(key, ValueName, 0, REG_DWORD, (LPBYTE)&value, sizeof(value));
		if (res != ERROR_SUCCESS)
			error(ERR_REG_SET_KEY, EXIT_FAILURE);
		_tprintf(TEXT("[SETUP] Set %s = %d.\n"), ValueName, value);
	}

	return value;
}

void init_dados(Dados* dados, HANDLE* hFileMap) {
	int i, x;
	// Get/Set Registry keys
	dados->MAX_AVIOES = getRegVal(REG_MAX_AVIOES_KEY_NAME, 10);
	dados->MAX_AEROPORTOS = getRegVal(REG_MAX_AEROPORTOS_KEY_NAME, 5);

	// Allocate memory for MAX vals
	dados->Avioes = malloc(sizeof(Aviao) * dados->MAX_AVIOES);
	dados->Aeroportos = malloc(sizeof(Aeroporto) * dados->MAX_AEROPORTOS);
	if (dados->Avioes == NULL || dados->Aeroportos == NULL) {
		error(ERR_INSUFFICIENT_MEMORY, EXIT_FAILURE);
	}

	dados->nAeroportos = 0;
	dados->nAvioes = 0;
	dados->aceitarAvioes = TRUE;

	// Mutex for Dados struct
	dados->hMutex = CreateMutex(NULL, FALSE, ControlMutex_NAME);
	if (dados->hMutex == NULL)
		error(ERR_CREATE_MUTEX, EXIT_FAILURE);

	// FileMap for Consumidor-Produtor
	*hFileMap = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, FileMap_NAME); // Check if FileMapping already exists
	if (*hFileMap != NULL)
		error(ERR_CONTROL_ALREADY_RUNNING, EXIT_FAILURE);

	*hFileMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(BufferCircular), FileMap_NAME);
	if (*hFileMap == NULL)
		error(ERR_CREATE_FILE_MAPPING, EXIT_FAILURE);

	dados->memPar = (BufferCircular*)MapViewOfFile(*hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (dados->memPar == NULL)
		error(ERR_MAP_VIEW_OF_FILE, EXIT_FAILURE);

	// Semaphores for Consumidor-Produtor
	dados->hSemEscrita = CreateSemaphore(NULL, TAM_BUFFER, TAM_BUFFER, SemEscrita_NAME);
	dados->hSemLeitura = CreateSemaphore(NULL, 0, TAM_BUFFER, SemLeitura_NAME);
	if (dados->hSemEscrita == NULL || dados->hSemLeitura == NULL)
		error(ERR_CREATE_SEMAPHORE, EXIT_FAILURE);

	for (i = 0; i < 1000; i++)
		for (x = 0; x < 1000; x++)
			dados->memPar->Map[i][x] = NULL;

	dados->memPar->pRead = 0;
	dados->memPar->pWrite = 0;
	dados->terminar = 0;
}

void PrintMenu(Dados* dados) { // Need Mutex
	int i;
	_tprintf(TEXT("Avioes\n"));
	WaitForSingleObject(dados->hMutex, INFINITE);
	for (i = 0; i < dados->nAvioes; i++)
		_tprintf(TEXT("\t%5lu: %3d, %3d | %3d, %3d | %2d/s | %3d seats\n"), dados->Avioes[i].PId, dados->Avioes[i].Coord.x, dados->Avioes[i].Coord.y, dados->Avioes[i].Dest.x, dados->Avioes[i].Dest.y, dados->Avioes[i].Speed, dados->Avioes[i].Seats);

	_tprintf(TEXT("\nAeroportos\n"));
	for (i = 0; i < dados->nAeroportos; i++)
		_tprintf(TEXT("\t%5s: %3d, %3d\n"), dados->Aeroportos[i].Name, dados->Aeroportos[i].Coord.x, dados->Aeroportos[i].Coord.y);
	ReleaseMutex(dados->hMutex);
}

void Handler(Dados* dados, CelulaBuffer* cel) {
	int index, airportindex;
	if (cel->Originator.PId < 5)
	{
		return;
	}
	index = FindAviaobyPId(dados, cel->Originator.PId);
	if (index == -1) {
		index = AddAviao(dados, &cel->Originator);
		if (index == -1)
		{
			return;
		}
	}
	switch (cel->rType)
	{
	case REQ_HEARTBEAT:
		dados->Avioes[index].lastHB = time(NULL);
		break;
	case REQ_AIRPORT:
		airportindex = FindAeroportobyName(dados, cel->buffer);
		if (airportindex == -1) {
			dados->Avioes[index].memPar->rType = RES_AIRPORT_NOTFOUND;
		}
		else {
			dados->Avioes[index].memPar->rType = RES_AIRPORT_FOUND;
			dados->Avioes[index].memPar->Coord.x = dados->Aeroportos[airportindex].Coord.x;
			dados->Avioes[index].memPar->Coord.y = dados->Aeroportos[airportindex].Coord.y;
		}
		SetEvent(dados->Avioes[index].hEvent);
		break;
	case REQ_UPDATEPOS:
		dados->Avioes[index].Coord.x = cel->Originator.Coord.x;
		dados->Avioes[index].Coord.y = cel->Originator.Coord.y;
		break;
	case REQ_UPDATEDES:
		dados->Avioes[index].Dest.x = cel->Originator.Dest.x;
		dados->Avioes[index].Dest.y = cel->Originator.Dest.y;
		break;
	case REQ_REACHEDDES:
		_tprintf(TEXT("%lu reached its destination\n"), dados->Avioes[index].PId);
	default:
		break;
	}
}

int FindAviaobyPId(Dados* dados, DWORD PId) {
	int i;
	for (i = 0; i < dados->nAvioes; i++)
		if (dados->Avioes[i].PId == PId) {
			return i;
		}
	return -1;
}

int FindAeroportobyName(Dados* dados, TCHAR * name) {
	int i;
	for (i = 0; i < dados->nAeroportos; i++)
		if (_tcscmp(dados->Aeroportos[i].Name, name) == 0) {
			ReleaseMutex(dados->hMutex);
			return i;
		}
	return -1;
}

int AeroportoisIsolated(Dados* dados, Coords coords) {
	const int lx = coords.x - 10,
			ly = coords.y - 10,
			hx = coords.x + 10,
			hy = coords.y + 10;
	int i;
	Aeroporto* aeroporto;
	for (i = 0; i < dados->nAeroportos; i++)
	{
		aeroporto = &dados->Aeroportos[i];
		if (aeroporto->Coord.x >= lx && aeroporto->Coord.x <= hx && aeroporto->Coord.y >= ly && aeroporto->Coord.y <= hy)
			return FALSE;
	}
	return TRUE;
}

int AddAviao(Dados* dados, AviaoOriginator* newAviao) {
	TCHAR buffer[TAM_BUFFER];
	if (dados->nAvioes < dados->MAX_AVIOES && dados->aceitarAvioes == TRUE)
	{
		dados->Avioes[dados->nAvioes].PId = newAviao->PId;
		dados->Avioes[dados->nAvioes].Seats = newAviao->Seats;
		dados->Avioes[dados->nAvioes].Speed = newAviao->Speed;
		dados->Avioes[dados->nAvioes].Dest.x = -1;
		dados->Avioes[dados->nAvioes].Dest.y = -1;
		dados->Avioes[dados->nAvioes].Coord.x = -1;
		dados->Avioes[dados->nAvioes].Coord.x = -1;

		_stprintf_s(buffer, TAM_BUFFER, AVIAO_FM_PATTERN, newAviao->PId);
		dados->Avioes[dados->nAvioes].hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, buffer);
		if (dados->Avioes[dados->nAvioes].hFileMap == NULL)
			error(ERR_CONTROL_NOT_RUNNING, EXIT_FAILURE); //maybe not quit program?

		dados->Avioes[dados->nAvioes].memPar = (Response*)MapViewOfFile(dados->Avioes[dados->nAvioes].hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		if (dados->Avioes[dados->nAvioes].memPar == NULL)
			error(ERR_MAP_VIEW_OF_FILE, EXIT_FAILURE); //maybe not quit program?

		_stprintf_s(buffer, TAM_BUFFER, AVIAO_REvent_PATTERN, newAviao->PId);
		dados->Avioes[dados->nAvioes].hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, buffer);
		if (dados->Avioes[dados->nAvioes].hEvent == NULL)
			error(ERR_OPEN_EVENT, EXIT_FAILURE);

		return dados->nAvioes++;
	}
	return -1;
}

int AddAeroporto(Dados* dados, Aeroporto * newAeroporto) {
	int index;
	if (newAeroporto->Coord.x > 999 || newAeroporto->Coord.y > 999 || newAeroporto->Coord.x < 0 || newAeroporto->Coord.y < 0)
		return -1;
	WaitForSingleObject(dados->hMutex, INFINITE);
	if (dados->nAeroportos < dados->MAX_AEROPORTOS)
	{
		index = FindAeroportobyName(dados, newAeroporto->Name);
		if (index != -1) {
			ReleaseMutex(dados->hMutex);
			return -1;
		}
		if (!AeroportoisIsolated(dados, newAeroporto->Coord)) {
			ReleaseMutex(dados->hMutex);
			return -1;
		}
		dados->Aeroportos[dados->nAeroportos].Coord.x = newAeroporto->Coord.x;
		dados->Aeroportos[dados->nAeroportos].Coord.y = newAeroporto->Coord.y;
		_tcscpy_s(dados->Aeroportos[dados->nAeroportos].Name, _countof(dados->Aeroportos[dados->nAeroportos].Name), newAeroporto->Name);
		ReleaseMutex(dados->hMutex);
		return dados->nAeroportos++;
	}
	ReleaseMutex(dados->hMutex);
	return -1;
}

int RemoveAviao(Dados* dados, int index) {
	int i = index;
	CloseHandle(dados->Avioes[i].hEvent);
	UnmapViewOfFile(dados->Avioes[i].memPar);
	CloseHandle(dados->Avioes[i].hFileMap);
	for (i; i < dados->nAvioes-1; i++)
		dados->Avioes[i] = dados->Avioes[i + 1];
	dados->nAvioes--;
}