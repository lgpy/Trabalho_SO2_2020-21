#include "Control_Utils.h"

//INIT
void init_dados(Dados* dados, HANDLE* hFileMap) {
	int i, x;
	// Get/Set Registry keys
	dados->MAX_AVIOES = getRegVal(REG_MAX_AVIOES_KEY_NAME, 10);
	dados->MAX_AEROPORTOS = getRegVal(REG_MAX_AEROPORTOS_KEY_NAME, 5);
	dados->MAX_PASSAGEIROS = PIPE_UNLIMITED_INSTANCES;

	// Allocate memory for MAX vals
	dados->Avioes = malloc(sizeof(Aviao) * dados->MAX_AVIOES);
	dados->Aeroportos = malloc(sizeof(Aeroporto) * dados->MAX_AEROPORTOS);
	dados->Passageiros = malloc(sizeof(Passageiro) * dados->MAX_PASSAGEIROS);
	if (dados->Avioes == NULL || dados->Aeroportos == NULL || dados->Passageiros == NULL)
		error(ERR_INSUFFICIENT_MEMORY, EXIT_FAILURE);

	dados->nAeroportos = 0;
	dados->nAvioes = 0;
	dados->nPassageiros = 0;
	dados->aceitarAvioes = TRUE;

	// Mutex for Dados struct
	dados->hMutexAvioes = CreateMutex(NULL, FALSE, MutexAvioes_NAME);
	dados->hMutexAeroportos = CreateMutex(NULL, FALSE, MutexAeroportos_NAME);
	dados->hMutexPassageiros = CreateMutex(NULL, FALSE, MutexPassageiros_NAME);
	if (dados->hMutexAvioes == NULL || dados->hMutexAeroportos == NULL)
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
	dados->hSemEscrita = CreateSemaphore(NULL, MAX_BUFFER, MAX_BUFFER, SemEscrita_NAME);
	dados->hSemLeitura = CreateSemaphore(NULL, 0, MAX_BUFFER, SemLeitura_NAME);
	if (dados->hSemEscrita == NULL || dados->hSemLeitura == NULL)
		error(ERR_CREATE_SEMAPHORE, EXIT_FAILURE);

	for (i = 0; i < 1000; i++)
		for (x = 0; x < 1000; x++)
			dados->memPar->Map[i][x] = NULL;

	dados->memPar->pRead = 0;
	dados->memPar->pWrite = 0;
	dados->terminar = 0;
}

//THREADS
DWORD WINAPI ThreadConsumidor(LPVOID param) {
	Dados* dados = (Dados*)param;
	CelulaBuffer cel;

	while (!dados->terminar)
	{
		WaitForSingleObject(dados->hSemLeitura, INFINITE); // Espera para poder ocupar um slot para Escrita

		CopyMemory(&cel, &dados->memPar->buffer[dados->memPar->pRead], sizeof(CelulaBuffer));
		dados->memPar->pRead++;
		if (dados->memPar->pRead == MAX_CBUFFER)
			dados->memPar->pRead = 0;

		Handler(dados, &cel);

		ReleaseSemaphore(dados->hSemEscrita, 1, NULL); // Liberta um slot para Leitura
	}

	return 0;
}

DWORD WINAPI ThreadHBChecker(LPVOID param) {
	Dados* dados = (Dados*)param;
	int i;
	time_t timenow;
	while (!dados->terminar)
	{
		timenow = time(NULL);
		WaitForSingleObject(dados->hMutexAvioes, INFINITE);
		for (i = 0; i < dados->nAvioes; i++)
		{
			if (difftime(timenow, dados->Avioes[i].lastHB) > 3)
			{
				if (dados->Avioes[i].ready == TRUE)
					_tprintf(TEXT("%lu's heartbeat has stopped\n"), dados->Avioes[i].PId);
				Crash(dados, &dados->Avioes[i]);
				RemoveAviao(dados, i);
			}
		}
		ReleaseMutex(dados->hMutexAvioes);
		Sleep(1000);
	}

	return 0;
}
DWORD WINAPI ThreadPassag(LPVOID param) {
	DadosPassag* dadosPassag = (DadosPassag*)param;
	int airportindex, index, terminar = FALSE;
	TCHAR buffer[MAX_BUFFER];
	RequestCP req;
	ResponseCP res;

	while (!terminar)
	{
		if (!ReadFile(dadosPassag->Passageiro->hPipe, &req, sizeof(RequestCP), NULL, NULL)) {
			_tprintf(TEXT("%s\n"), ERR_READ_PIPE);//TODO remove passageiro?
			terminar = TRUE;
			continue;
		}

		switch (req.Type)
		{
		case REQ_INIT:
			dadosPassag->Passageiro->PId = req.Originator.PId;
			_tcscpy_s(dadosPassag->Passageiro->Name, _countof(dadosPassag->Passageiro->Name), req.Originator.Name);
			_stprintf_s(buffer, MAX_BUFFER, Event_CP_PATTERN, dadosPassag->Passageiro->PId);
			dadosPassag->Passageiro->hEvent = OpenEvent(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, buffer);
			if (dadosPassag->Passageiro->hEvent == NULL) {
				_tprintf(TEXT("%s\n"), ERR_CREATE_EVENT);
				terminar = TRUE;
				WaitForSingleObject(dadosPassag->dados->hMutexPassageiros, INFINITE);
				index = FindPassageirobyPId(dadosPassag->dados, dadosPassag->Passageiro->PId);
				if (index != -1)
					RemovePassageiro(dadosPassag->dados, index);
				ReleaseMutex(dadosPassag->dados->hMutexPassageiros);
				break;
			}
			res.Type = RES_ADDED;
			if (!WriteFile(dadosPassag->Passageiro->hPipe, &res, sizeof(ResponseCP), NULL, NULL)) {
				_tprintf(TEXT("%s\n"), ERR_WRITE_PIPE);
				terminar = TRUE;
				WaitForSingleObject(dadosPassag->dados->hMutexPassageiros, INFINITE);
				index = FindPassageirobyPId(dadosPassag->dados, dadosPassag->Passageiro->PId);
				if (index != -1)
					RemovePassageiro(dadosPassag->dados, index);
				ReleaseMutex(dadosPassag->dados->hMutexPassageiros);
			}
			break;
		case REQ_AIRPORT:
			airportindex = FindAeroportobyName(dadosPassag->dados, req.buffer);
			if (airportindex == -1) {
				res.Type = RES_AIRPORT_NOTFOUND;
			}
			else {
				res.Type = RES_AIRPORT_FOUND;
				res.Coord.x = dadosPassag->dados->Aeroportos[airportindex].Coord.x;
				res.Coord.y = dadosPassag->dados->Aeroportos[airportindex].Coord.y;
			}
			if (!WriteFile(dadosPassag->Passageiro->hPipe, &res, sizeof(ResponseCP), NULL, NULL)) {
				_tprintf(TEXT("%s\n"), ERR_WRITE_PIPE);
				terminar = TRUE;
				WaitForSingleObject(dadosPassag->dados->hMutexPassageiros, INFINITE);
				index = FindPassageirobyPId(dadosPassag->dados, dadosPassag->Passageiro->PId);
				if (index != -1)
					RemovePassageiro(dadosPassag->dados, index);
				ReleaseMutex(dadosPassag->dados->hMutexPassageiros);
			}
			break;
		case REQ_UPDATE:
			CopyMemory(&dadosPassag->Passageiro->Coord, &req.Originator.Coord, sizeof(Coords));
			CopyMemory(&dadosPassag->Passageiro->Dest, &req.Originator.Dest, sizeof(Coords));
			dadosPassag->Passageiro->ready = TRUE;
			terminar = TRUE;
			break;
		default:
			break;
		}
	}
	WaitForSingleObject(dadosPassag->Passageiro->hEvent, INFINITE);
	WaitForSingleObject(dadosPassag->dados->hMutexPassageiros, INFINITE);
	index = FindPassageirobyPId(dadosPassag->dados, dadosPassag->Passageiro->PId);
	if (index != -1)
		RemovePassageiro(dadosPassag->dados, index);
	ReleaseMutex(dadosPassag->dados->hMutexPassageiros);
	free(dadosPassag);
	return 0;
}

DWORD WINAPI ThreadNewPassag(LPVOID param) {
	Dados* dados = (Dados*)param;

	int index;
	DadosPassag* dadosPassag;

	HANDLE hPipe;
	RequestCP req;
	ResponseCP res;

	while (!dados->terminar)
	{
		hPipe = CreateNamedPipe(NamedPipe_NAME,
			PIPE_ACCESS_DUPLEX,
			PIPE_WAIT | PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
			PIPE_UNLIMITED_INSTANCES,
			sizeof(ResponseCP),
			sizeof(RequestCP),
			1000,
			NULL);

		if (hPipe == INVALID_HANDLE_VALUE) {
			_tprintf(TEXT("%s\n"), ERR_CREATE_PIPE);
			continue;
		}

		if (!ConnectNamedPipe(hPipe, NULL)) {
			if (GetLastError() != ERROR_PIPE_CONNECTED)
			{
				_tprintf(TEXT("%s\n"), ERR_CONNECT_PIPE);
				continue;
			}
		}

		dadosPassag = malloc(sizeof(DadosPassag));
		if (dadosPassag == NULL)
			error(ERR_INSUFFICIENT_MEMORY, EXIT_FAILURE);
		WaitForSingleObject(dados->hMutexPassageiros, INFINITE);
		index = AddPassageiro(dados, hPipe);
		if (index == -1) {
			continue;
			ReleaseMutex(dados->hMutexPassageiros);
		}
		dadosPassag->Passageiro = &dados->Passageiros[index];
		dadosPassag->dados = dados;

		dados->Passageiros[index].hThread = CreateThread(NULL, 0, ThreadPassag, dadosPassag, 0, NULL);
		if (dados->Passageiros[index].hThread == NULL) {
			_tprintf(TEXT("%s\n"), ERR_CREATE_THREAD);
			RemovePassageiro(dados, index);
		}
		ReleaseMutex(dados->hMutexPassageiros);
	}

	return 0;
}

//AC Handler
void Handler(Dados* dados, CelulaBuffer* cel) {
	int index, airportindex;
	switch (cel->rType)
	{
	case REQ_HEARTBEAT:
		WaitForSingleObject(dados->hMutexAvioes, INFINITE);
		index = FindAviaobyPId(dados, cel->Originator.PId);
		dados->Avioes[index].lastHB = time(NULL);
		ReleaseMutex(dados->hMutexAvioes);
		break;
	case REQ_AIRPORT:
		airportindex = FindAeroportobyName(dados, cel->buffer);
		WaitForSingleObject(dados->hMutexAvioes, INFINITE);
		index = FindAviaobyPId(dados, cel->Originator.PId);
		if (index == -1)
		{
			index = AddAviao(dados, &cel->Originator);
			if (index == -1) {
				ReleaseMutex(dados->hMutexAvioes);
				return;
			}
		}
		WaitForSingleObject(dados->Avioes[index].hMutexMemPar, INFINITE);

		if (dados->Avioes[index].ready == FALSE)
			dados->Avioes[index].lastHB = time(NULL);

		if (airportindex == -1) {
			dados->Avioes[index].memPar->rType = RES_AIRPORT_NOTFOUND;
		}
		else {
			if (dados->Avioes[index].ready == FALSE)
				dados->Avioes[index].ready = TRUE;
			dados->Avioes[index].memPar->rType = RES_AIRPORT_FOUND;
			dados->Avioes[index].memPar->Coord.x = dados->Aeroportos[airportindex].Coord.x;
			dados->Avioes[index].memPar->Coord.y = dados->Aeroportos[airportindex].Coord.y;
		}
		ReleaseMutex(dados->Avioes[index].hMutexMemPar);
		SetEvent(dados->Avioes[index].hEvent);
		ReleaseMutex(dados->hMutexAvioes);
		break;
	case REQ_UPDATEPOS:
		WaitForSingleObject(dados->hMutexAvioes, INFINITE);
		index = FindAviaobyPId(dados, cel->Originator.PId);
		dados->Avioes[index].Coord.x = cel->Originator.Coord.x;
		dados->Avioes[index].Coord.y = cel->Originator.Coord.y;
		ReleaseMutex(dados->hMutexAvioes);
		UpdateEmbarked(dados, dados->Avioes[index].PId, dados->Avioes[index].Coord); //TODO fix it;
		break;
	case REQ_UPDATEDES:
		WaitForSingleObject(dados->hMutexAvioes, INFINITE);
		index = FindAviaobyPId(dados, cel->Originator.PId);
		dados->Avioes[index].Dest.x = cel->Originator.Dest.x;
		dados->Avioes[index].Dest.y = cel->Originator.Dest.y;
		_tprintf(TEXT("%lu changed its destination, disembarked %d passengers\n"), dados->Avioes[index].PId, Disembark(dados, &dados->Avioes[index]));
		ReleaseMutex(dados->hMutexAvioes);
		break;
	case REQ_REACHEDDES:
		WaitForSingleObject(dados->hMutexAvioes, INFINITE);
		index = FindAviaobyPId(dados, cel->Originator.PId);
		_tprintf(TEXT("%lu reached its destination, disembarked %d passengers\n"), dados->Avioes[index].PId, ReachedDest(dados, &dados->Avioes[index]));
		ReleaseMutex(dados->hMutexAvioes);
		break;
	case REQ_EMBARK:
		WaitForSingleObject(dados->hMutexAvioes, INFINITE);
		index = FindAviaobyPId(dados, cel->Originator.PId);
		_tprintf(TEXT("%lu embarked %d passagers\n"), dados->Avioes[index].PId, Embark(dados, &dados->Avioes[index]));
		ReleaseMutex(dados->hMutexAvioes);
	default:
		break;
	}
}

//UTILS
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
void PrintInfo(Dados* dados) { // Need Mutex
	int i;
	_tprintf(TEXT("Avioes\n"));
	WaitForSingleObject(dados->hMutexAvioes, INFINITE);
	for (i = 0; i < dados->nAvioes; i++)
		_tprintf(TEXT("\t%5lu: %3d, %3d | %3d, %3d | %2d/s | %d/%d seats\n"), dados->Avioes[i].PId, dados->Avioes[i].Coord.x, dados->Avioes[i].Coord.y, dados->Avioes[i].Dest.x, dados->Avioes[i].Dest.y, dados->Avioes[i].Speed, dados->Avioes[i].nPassengers, dados->Avioes[i].Seats);
	ReleaseMutex(dados->hMutexAvioes);

	_tprintf(TEXT("\nAeroportos\n"));
	WaitForSingleObject(dados->hMutexAeroportos, INFINITE);
	for (i = 0; i < dados->nAeroportos; i++)
		_tprintf(TEXT("\t%5s: %3d, %3d\n"), dados->Aeroportos[i].Name, dados->Aeroportos[i].Coord.x, dados->Aeroportos[i].Coord.y);
	ReleaseMutex(dados->hMutexAeroportos);

	_tprintf(TEXT("\nPassageiros\n"));
	WaitForSingleObject(dados->hMutexPassageiros, INFINITE);
	for (i = 0; i < dados->nPassageiros; i++)
		if (dados->Passageiros[i].ready)
			if (dados->Passageiros[i].AviaoPId == NULL)
				_tprintf(TEXT("\t%5s | %3d, %3d | %3d, %3d\n"), dados->Passageiros[i].Name, dados->Passageiros[i].Coord.x, dados->Passageiros[i].Coord.y, dados->Passageiros[i].Dest.x, dados->Passageiros[i].Dest.y);
			else
				_tprintf(TEXT("\t%5s | %5lu\n"), dados->Passageiros[i].Name, dados->Passageiros[i].AviaoPId);
	ReleaseMutex(dados->hMutexPassageiros);
}
void PrintMenu(Dados* dados) {
	static TCHAR buffer[MAX_BUFFER];
	Aeroporto newAeroporto;
	int opt = -1, i = 0;

	_tprintf(TEXT("1: Listar Informa??o\n"));
	_tprintf(TEXT("2: Criar Aeroporto\n"));
	if (dados->aceitarAvioes)
		_tprintf(TEXT("3: Suspender aceita??o de avi?es\n"));
	else
		_tprintf(TEXT("3: Ativar aceita??o de avi?es\n"));
	_tprintf(TEXT("4: Encerrar\n"));
	do
	{
		_fgetts(buffer, MAX_BUFFER, stdin);
		opt = _tstoi(buffer);
	} while (!(opt <= 4 && opt >= 1));
	switch (opt)
	{
	case 1:
		PrintInfo(dados);
		break;
	case 2:
		_tprintf(TEXT("Nome do Aeroporto: "));
		_fgetts(newAeroporto.Name, 30, stdin);
		newAeroporto.Name[_tcslen(newAeroporto.Name) - 1] = '\0';
		_tprintf(TEXT("X: "));
		_fgetts(buffer, MAX_BUFFER, stdin);
		newAeroporto.Coord.x = _tstoi(buffer);
		_tprintf(TEXT("Y: "));
		_fgetts(buffer, MAX_BUFFER, stdin);
		newAeroporto.Coord.y = _tstoi(buffer);
		if (AddAeroporto(dados, &newAeroporto) > -1) {
			_tprintf(TEXT("Aeroporto adicionado\n"));
		}
		else
			_tprintf(TEXT("Aeroporto invalido\n"));
		break;
	case 3:
		if (dados->aceitarAvioes)
			dados->aceitarAvioes = FALSE;
		else
			dados->aceitarAvioes = TRUE;
		break;
	case 4:
		WaitForSingleObject(dados->hMutexAvioes, INFINITE);
		for (i = 0; i < dados->nAvioes; i++)
		{
			WaitForSingleObject(dados->Avioes[i].hMutexMemPar, INFINITE);
			dados->Avioes[i].memPar->rType = RES_CONTROL_SHUTDOWN;
			ReleaseMutex(dados->Avioes[i].hMutexMemPar);
			SetEvent(dados->Avioes[i].hEvent);
		}
		exit(EXIT_SUCCESS);
		break;
	default:
		break;
	}
}
void error(const TCHAR* msg, int exit_code) {
	_tprintf(TEXT("[FATAL] %s"), msg);
	exit(exit_code);
}

//AVIOES
int FindAviaobyPId(Dados* dados, DWORD PId) {
	int i;
	WaitForSingleObject(dados->hMutexAvioes, INFINITE);
	for (i = 0; i < dados->nAvioes; i++)
		if (dados->Avioes[i].PId == PId) {
			ReleaseMutex(dados->hMutexAvioes);
			return i;
		}
	ReleaseMutex(dados->hMutexAvioes);
	return -1;
}
int AddAviao(Dados* dados, AviaoOriginator* newAviao) {
	int add = 0;
	TCHAR buffer[MAX_BUFFER];
	WaitForSingleObject(dados->hMutexAvioes, INFINITE);
	if (dados->nAvioes < dados->MAX_AVIOES && dados->aceitarAvioes == TRUE)
	{
		dados->Avioes[dados->nAvioes].ready = FALSE;
		dados->Avioes[dados->nAvioes].PId = newAviao->PId;
		dados->Avioes[dados->nAvioes].Seats = newAviao->Seats;
		dados->Avioes[dados->nAvioes].Speed = newAviao->Speed;
		dados->Avioes[dados->nAvioes].Dest.x = -1;
		dados->Avioes[dados->nAvioes].Dest.y = -1;
		dados->Avioes[dados->nAvioes].Coord.x = -1;
		dados->Avioes[dados->nAvioes].Coord.x = -1;

		_stprintf_s(buffer, MAX_BUFFER, MutexMempar_CA_PATTERN, dados->Avioes[dados->nAvioes].PId);
		dados->Avioes[dados->nAvioes].hMutexMemPar = OpenMutex(SYNCHRONIZE, FALSE, buffer);
		if (dados->Avioes[dados->nAvioes].hMutexMemPar == NULL)
			add = -1;
		else {
			_stprintf_s(buffer, MAX_BUFFER, FileMapCA_PATTERN, newAviao->PId);
			dados->Avioes[dados->nAvioes].hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, buffer);
			if (dados->Avioes[dados->nAvioes].hFileMap == NULL)
				add = -1;
			else {
				dados->Avioes[dados->nAvioes].memPar = (Response*)MapViewOfFile(dados->Avioes[dados->nAvioes].hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
				if (dados->Avioes[dados->nAvioes].memPar == NULL)
					add = -1;
				else {
					_stprintf_s(buffer, MAX_BUFFER, Event_CA_PATTERN, newAviao->PId);
					dados->Avioes[dados->nAvioes].hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, buffer);
					if (dados->Avioes[dados->nAvioes].hEvent == NULL)
						add = -1;
				}
			}
		}

		if (add == 0)
			add = dados->nAvioes++; // ++dados->nAvioes
	}
	ReleaseMutex(dados->hMutexAvioes);
	return add;
}
void RemoveAviao(Dados* dados, int index) {
	WaitForSingleObject(dados->hMutexAvioes, INFINITE);
	int i = index;
	CloseHandle(dados->Avioes[i].hEvent);
	UnmapViewOfFile(dados->Avioes[i].memPar);
	CloseHandle(dados->Avioes[i].hFileMap);
	for (i; i < dados->nAvioes - 1; i++)
		dados->Avioes[i] = dados->Avioes[i + 1];
	dados->nAvioes--;
	ReleaseMutex(dados->hMutexAvioes);
}

//AEROPORTOS
int FindAeroportobyName(Dados* dados, TCHAR* name) {
	int i;
	WaitForSingleObject(dados->hMutexAeroportos, INFINITE);
	for (i = 0; i < dados->nAeroportos; i++)
		if (_tcscmp(dados->Aeroportos[i].Name, name) == 0) {
			ReleaseMutex(dados->hMutexAeroportos);
			return i;
		}
	ReleaseMutex(dados->hMutexAeroportos);
	return -1;
}
int AddAeroporto(Dados* dados, Aeroporto* newAeroporto) {
	int index;
	if (newAeroporto->Coord.x > 999 || newAeroporto->Coord.y > 999 || newAeroporto->Coord.x < 0 || newAeroporto->Coord.y < 0)
		return -1;
	WaitForSingleObject(dados->hMutexAeroportos, INFINITE);
	if (dados->nAeroportos < dados->MAX_AEROPORTOS)
	{
		index = FindAeroportobyName(dados, newAeroporto->Name);
		if (index != -1) {
			ReleaseMutex(dados->hMutexAeroportos);
			return -1;
		}
		if (!AeroportoisIsolated(dados, newAeroporto->Coord)) {
			ReleaseMutex(dados->hMutexAeroportos);
			return -1;
		}
		dados->Aeroportos[dados->nAeroportos].Coord.x = newAeroporto->Coord.x;
		dados->Aeroportos[dados->nAeroportos].Coord.y = newAeroporto->Coord.y;
		_tcscpy_s(dados->Aeroportos[dados->nAeroportos].Name, _countof(dados->Aeroportos[dados->nAeroportos].Name), newAeroporto->Name);
		ReleaseMutex(dados->hMutexAeroportos);
		return dados->nAeroportos++;
	}
	ReleaseMutex(dados->hMutexAeroportos);
	return -1;
}
int AeroportoisIsolated(Dados* dados, Coords coords) {
	const int lx = coords.x - 9,
		ly = coords.y - 9,
		hx = coords.x + 9,
		hy = coords.y + 9;
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

//PASSAGEIROS
int FindPassageirobyPId(Dados* dados, DWORD PId) {
	int i;
	for (i = 0; i < dados->nPassageiros; i++)
		if (dados->Passageiros[i].PId == PId) {
			ReleaseMutex(dados->hMutexPassageiros);
			return i;
		}
	return -1;
}

int AddPassageiro(Dados* dados, HANDLE hPipe) {
	if (dados->nPassageiros < dados->MAX_PASSAGEIROS) {
		dados->Passageiros[dados->nPassageiros].ready = FALSE;
		dados->Passageiros[dados->nPassageiros].AviaoPId = NULL;
		dados->Passageiros[dados->nPassageiros].hPipe = hPipe;
		return dados->nPassageiros++;
	}
	return -1;
}

void RemovePassageiro(Dados* dados, int index) {
	int i = index;
	if (dados->Passageiros[i].hEvent != NULL)
		CloseHandle(dados->Passageiros[i].hEvent);
	if (dados->Passageiros[i].hPipe != NULL) {
		DisconnectNamedPipe(dados->Passageiros[i].hPipe);
		CloseHandle(dados->Passageiros[i].hPipe);
	}
	for (i; i < dados->nPassageiros - 1; i++)
		dados->Passageiros[i] = dados->Passageiros[i + 1];
	dados->nPassageiros--;
}

int Embark(Dados* dados, Aviao* aviao) {
	int count = 0, i;
	ResponseCP res;
	res.Type = RES_EMBARKED;
	WaitForSingleObject(dados->hMutexPassageiros, INFINITE);
	for (i = 0; i < dados->nPassageiros; i++) {
		if (aviao->nPassengers >= aviao->Seats)
			break;

		if (dados->Passageiros[i].ready)
			if (dados->Passageiros[i].Coord.x == aviao->Coord.x && dados->Passageiros[i].Coord.y == aviao->Coord.y)
				if (dados->Passageiros[i].Dest.x == aviao->Dest.x && dados->Passageiros[i].Dest.y == aviao->Dest.y)
					if (dados->Passageiros[i].AviaoPId == NULL)
					{
						dados->Passageiros[i].AviaoPId = aviao->PId;
						if (!WriteFile(dados->Passageiros[i].hPipe, &res, sizeof(ResponseCP), NULL, NULL)) {
							_tprintf(TEXT("%s\n"), ERR_WRITE_PIPE);
							RemovePassageiro(dados, i);
							aviao->nPassengers--;
							continue;
						}
						count++;
						aviao->nPassengers++;
					}
	}
		
	ReleaseMutex(dados->hMutexPassageiros);
	return count;
}

int Disembark(Dados* dados, Aviao* aviao) {
	int count = 0, i;
	ResponseCP res;
	res.Type = RES_DISEMBARKED;

	WaitForSingleObject(dados->hMutexPassageiros, INFINITE);
	for (i = 0; i < dados->nPassageiros; i++)
		if (dados->Passageiros[i].AviaoPId == aviao->PId)
		{
			dados->Passageiros[i].AviaoPId = NULL;
			if (!WriteFile(dados->Passageiros[i].hPipe, &res, sizeof(ResponseCP), NULL, NULL)) {
				_tprintf(TEXT("%s\n"), ERR_WRITE_PIPE);
				RemovePassageiro(dados, i);
				aviao->nPassengers--;
				continue;
			}
			count++;
		}
	aviao->nPassengers = 0;
	ReleaseMutex(dados->hMutexPassageiros);
	return count;
}

void Crash(Dados* dados, Aviao* aviao) {
	int i;
	ResponseCP res;
	res.Type = RES_CRASHED;

	WaitForSingleObject(dados->hMutexPassageiros, INFINITE);
	for (i = 0; i < dados->nPassageiros; i++)
		if (dados->Passageiros[i].AviaoPId == aviao->PId)
		{
			if (!WriteFile(dados->Passageiros[i].hPipe, &res, sizeof(ResponseCP), NULL, NULL)) {
				_tprintf(TEXT("%s\n"), ERR_WRITE_PIPE);
				RemovePassageiro(dados, i);
				continue;
			}
			RemovePassageiro(dados, i);
		}
	aviao->nPassengers = 0;
	ReleaseMutex(dados->hMutexPassageiros);
}

int ReachedDest(Dados* dados, Aviao* aviao) {
	int count = 0, i;
	ResponseCP res;
	res.Type = RES_REACHEDDEST;

	WaitForSingleObject(dados->hMutexPassageiros, INFINITE);
	for (i = 0; i < dados->nPassageiros; i++)
		if (dados->Passageiros[i].AviaoPId == aviao->PId)
		{
			dados->Passageiros[i].AviaoPId = NULL;
			if (!WriteFile(dados->Passageiros[i].hPipe, &res, sizeof(ResponseCP), NULL, NULL)) {
				_tprintf(TEXT("%s\n"), ERR_WRITE_PIPE);
				RemovePassageiro(dados, i);
				aviao->nPassengers--;
				continue;
			}
			count++;
		}
	aviao->nPassengers = 0;
	ReleaseMutex(dados->hMutexPassageiros);
	return count;
}

void UpdateEmbarked(Dados* dados, DWORD PId, Coords toUpdate) {
	int i;
	ResponseCP res;
	res.Type = RES_UPDATEDPOS;
	CopyMemory(&res.Coord, &toUpdate, sizeof(Coords));

	WaitForSingleObject(dados->hMutexPassageiros, INFINITE);
	for (i = 0; i < dados->nPassageiros; i++)
		if (dados->Passageiros[i].AviaoPId == PId) {
			if (!WriteFile(dados->Passageiros[i].hPipe, &res, sizeof(ResponseCP), NULL, NULL)) {
				_tprintf(TEXT("%s\n"), ERR_WRITE_PIPE);
				RemovePassageiro(dados, i);
				WaitForSingleObject(dados->hMutexAvioes, INFINITE);
				dados->Avioes[FindAviaobyPId(dados, PId)].nPassengers--;
				ReleaseMutex(dados->hMutexAvioes);
			}
		}
	ReleaseMutex(dados->hMutexPassageiros);
}