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

void init_dados(HANDLE* hFileMap, DadosThread* dados) {
	*hFileMap = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, FileMap_NAME); // Check if FileMapping already exists
	if (*hFileMap != NULL)
		error(ERR_CONTROL_ALREADY_RUNNING, EXIT_FAILURE);

	*hFileMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(BufferCircular), FileMap_NAME);
	if (*hFileMap == NULL)
		error(ERR_CREATE_FILE_MAPPING, EXIT_FAILURE);

	dados->memPar = (BufferCircular*)MapViewOfFile(*hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (dados->memPar == NULL)
		error(ERR_MAP_VIEW_OF_FILE, EXIT_FAILURE);

	dados->hSemEscrita = CreateSemaphore(NULL, TAM_BUFFER, TAM_BUFFER, SemEscrita_NAME);
	dados->hSemLeitura = CreateSemaphore(NULL, 0, TAM_BUFFER, SemLeitura_NAME);
	dados->hMutex = CreateMutex(NULL, FALSE, ControlMutex_NAME);
	if (dados->hSemEscrita == NULL || dados->hSemLeitura == NULL)
		error(ERR_CREATE_SEMAPHORE, EXIT_FAILURE);
	if (dados->hMutex == NULL)
		error(ERR_CREATE_MUTEX, EXIT_FAILURE);

	dados->memPar->pRead = 0;
	dados->memPar->pWrite = 0;
	dados->terminar = 0;
}

void PrintMenu(Aviao* Avioes, const int nAvioes, Aeroporto* Aeroportos, const int nAeroportos) { // Need Mutex
	int i;
	_tprintf(TEXT("Avioes\n"));
	for (i = 0; i < nAvioes; i++)
		_tprintf(TEXT("\t%5lu: %3d %3d | %2d/s | %3d seats\n"), Avioes[i].PId, Avioes[i].Coord.x, Avioes[i].Coord.y, Avioes[i].Speed, Avioes[i].Seats);

	_tprintf(TEXT("\nAeroportos\n"));
	for (i = 0; i < nAeroportos; i++)
		_tprintf(TEXT("\t%5s: %3d, %3d\n"), Aeroportos[i].Name, Aeroportos[i].Coord.x, Aeroportos[i].Coord.y);
}

void Handler(DadosThread* dados, CelulaBuffer* cel) {
	Response res;
	int index, airportindex;
	if (cel->Originator.PId < 5)
	{
		return;
	}
	index = FindAviaobyPId(dados->Avioes, dados->nAvioes, cel->Originator.PId);
	if (index == -1) {
		index = AddAviao(dados->Avioes, &dados->nAvioes, dados->MAX_AVIOES, &cel->Originator);
		if (index == -1)
		{
			// cant add it maxed out
		}
	}
	switch (cel->rType)
	{
	case REQ_HEARTBEAT:
		dados->Avioes[index].lastHB = time(NULL);
		break;
	case REQ_AIRPORT:
		airportindex = FindAeroportobyName(dados->Aeroportos, dados->nAeroportos, cel->buffer);
		if (airportindex == -1) {
			dados->Avioes[index].memPar->rType = RES_AIRPORT_NOTFOUND;
		}
		else {
			dados->Avioes[index].memPar->rType = RES_AIRPORT_FOUND;
			dados->Avioes[index].memPar->Coord.x = dados->Aeroportos[airportindex].Coord.x;
			dados->Avioes[index].memPar->Coord.y = dados->Aeroportos[airportindex].Coord.y;
		}
		if (SetEvent(dados->Avioes[index].hEvent) != 0)
			_tprintf(TEXT("sent response to %d\n"), dados->Avioes[index].PId);
		break;
	case REQ_UPDATEPOS:
		dados->Avioes[index].memPar->rType = RES_LOCATION_UPDATED;
		dados->Avioes[index].Coord.x = cel->Originator.Coord.x;
		dados->Avioes[index].Coord.y = cel->Originator.Coord.y;
		PrintMenu(dados->Avioes, dados->nAvioes, dados->Aeroportos, dados->nAeroportos); //move to looping thread w/1sec sleep ?
		break;
	default:
		break;
	}
}

int FindAviaobyPId(Aviao* Avioes, const int nAvioes, int PId) {
	int i;
	for (i = 0; i < nAvioes; i++)
		if (Avioes[i].PId == PId)
			return i;
	return -1;
}

int FindAeroportobyName(Aeroporto* Aeroportos, const int nAeroportos, TCHAR * name) {
	int i;
	for (i = 0; i < nAeroportos; i++)
		if (_tcscmp(Aeroportos[i].Name, name) == 0)
			return i;
	return -1;
}

int AddAviao(Aviao* Avioes, int * nAvioes, const int Max_Avioes, AviaoOriginator* newAviao) {
	TCHAR buffer[TAM_BUFFER];
	if (*nAvioes < Max_Avioes)
	{
		Avioes[*nAvioes].PId = newAviao->PId;
		Avioes[*nAvioes].Seats = newAviao->Seats;
		Avioes[*nAvioes].Speed = newAviao->Speed;
		Avioes[*nAvioes].Coord.x = -1;
		Avioes[*nAvioes].Coord.x = -1;

		_stprintf_s(buffer, TAM_BUFFER, AVIAO_FM_PATTERN, newAviao->PId);
		Avioes[*nAvioes].hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, buffer);
		if (Avioes[*nAvioes].hFileMap == NULL)
			error(ERR_CONTROL_NOT_RUNNING, EXIT_FAILURE); //maybe not quit program?

		Avioes[*nAvioes].memPar = (Response*)MapViewOfFile(Avioes[*nAvioes].hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		if (Avioes[*nAvioes].memPar == NULL)
			error(ERR_MAP_VIEW_OF_FILE, EXIT_FAILURE); //maybe not quit program?

		_stprintf_s(buffer, TAM_BUFFER, AVIAO_REvent_PATTERN, newAviao->PId);
		Avioes[*nAvioes].hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, buffer);
		if (Avioes[*nAvioes].hEvent == NULL)
			error(ERR_OPEN_EVENT, EXIT_FAILURE);

		return (*nAvioes)++;
	}
	return -1;
}

int AddAeroporto(Aeroporto* Aeroportos, int * nAeroportos, const int Max_Aeroportos, TCHAR* name, Coords coords) {
	if (nAeroportos < Max_Aeroportos)
	{
		Aeroportos[*nAeroportos].Coord.x = coords.x;
		Aeroportos[*nAeroportos].Coord.y = coords.y;
		memcpy(Aeroportos[*nAeroportos].Name, name, _tcslen(name) * sizeof(TCHAR));
		return (*nAeroportos)++;
	}
	return -1;
}