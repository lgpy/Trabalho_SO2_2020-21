#include "Control_Utils.h"

//INIT
void init_dados(Dados* dados) {
	int i, x;
	// Get/Set Registry keys
	dados->MAX_AVIOES = getRegVal(dados, REG_MAX_AVIOES_KEY_NAME, 10);
	dados->MAX_AEROPORTOS = getRegVal(dados, REG_MAX_AEROPORTOS_KEY_NAME, 5);
	dados->MAX_PASSAGEIROS = PIPE_UNLIMITED_INSTANCES;

	// Allocate memory for MAX vals
	dados->Avioes = malloc(sizeof(Aviao) * dados->MAX_AVIOES);
	dados->Aeroportos = malloc(sizeof(Aeroporto) * dados->MAX_AEROPORTOS);
	dados->Passageiros = malloc(sizeof(Passageiro) * dados->MAX_PASSAGEIROS);
	if (dados->Avioes == NULL || dados->Aeroportos == NULL || dados->Passageiros == NULL)
		error(dados, ERR_INSUFFICIENT_MEMORY, EXIT_FAILURE);

	dados->nAeroportos = 0;
	dados->nAvioes = 0;
	dados->nPassageiros = 0;
	dados->aceitarAvioes = TRUE;

	// Mutex for Dados struct
	dados->hMutexAvioes = CreateMutex(NULL, FALSE, MutexAvioes_NAME);
	dados->hMutexAeroportos = CreateMutex(NULL, FALSE, MutexAeroportos_NAME);
	dados->hMutexPassageiros = CreateMutex(NULL, FALSE, MutexPassageiros_NAME);
	if (dados->hMutexAvioes == NULL || dados->hMutexAeroportos == NULL)
		error(dados, ERR_CREATE_MUTEX, EXIT_FAILURE);

	// FileMap for Consumidor-Produtor
	dados->hFileMap = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, FileMap_NAME); // Check if FileMapping already exists
	if (dados->hFileMap != NULL)
		error(dados, ERR_CONTROL_ALREADY_RUNNING, EXIT_FAILURE);

	dados->hFileMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(BufferCircular), FileMap_NAME);
	if (dados->hFileMap == NULL)
		error(dados, ERR_CREATE_FILE_MAPPING, EXIT_FAILURE);

	dados->memPar = (BufferCircular*)MapViewOfFile(dados->hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (dados->memPar == NULL)
		error(dados, ERR_MAP_VIEW_OF_FILE, EXIT_FAILURE);

	// Semaphores for Consumidor-Produtor
	dados->hSemEscrita = CreateSemaphore(NULL, MAX_BUFFER, MAX_BUFFER, SemEscrita_NAME);
	dados->hSemLeitura = CreateSemaphore(NULL, 0, MAX_BUFFER, SemLeitura_NAME);
	if (dados->hSemEscrita == NULL || dados->hSemLeitura == NULL)
		error(dados, ERR_CREATE_SEMAPHORE, EXIT_FAILURE);

	for (i = 0; i < 1000; i++)
		for (x = 0; x < 1000; x++)
			dados->memPar->Map[i][x] = NULL;

	dados->memPar->pRead = 0;
	dados->memPar->pWrite = 0;
	dados->terminar = FALSE;
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
	TCHAR buffer[MAX_BUFFER];
	
	time_t timenow;
	while (!dados->terminar)
	{
		timenow = time(NULL);
		WaitForSingleObject(dados->hMutexAvioes, INFINITE);
		for (i = 0; i < dados->nAvioes; i++)
		{
			if (difftime(timenow, dados->Avioes[i].lastHB) > 3)
			{
				if (dados->Avioes[i].state != AVIAO_STATE_INIT) {
					_stprintf_s(buffer, MAX_BUFFER, TEXT("%lu's heartbeat has stopped"), dados->Avioes[i].PId);
					MessageBox(dados->gui.hWnd, buffer, TEXT("Heartbeat"), MB_OK | MB_ICONWARNING);
				}
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
			MessageBox(dadosPassag->dados->gui.hWnd, ERR_READ_PIPE, TEXT("Error"), MB_OK | MB_ICONWARNING); //TODO remove passageiro?
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
				MessageBox(dadosPassag->dados->gui.hWnd, ERR_CREATE_EVENT, TEXT("Error"), MB_OK | MB_ICONWARNING);
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
				MessageBox(dadosPassag->dados->gui.hWnd, ERR_WRITE_PIPE, TEXT("Error"), MB_OK | MB_ICONWARNING);
				terminar = TRUE;
				WaitForSingleObject(dadosPassag->dados->hMutexPassageiros, INFINITE);
				index = FindPassageirobyPId(dadosPassag->dados, dadosPassag->Passageiro->PId);
				if (index != -1)
					RemovePassageiro(dadosPassag->dados, index);
				ReleaseMutex(dadosPassag->dados->hMutexPassageiros);
			}
			dadosPassag->Passageiro->state = PASSAG_STATE_READY;
			break;
		case REQ_AIRPORT:
			airportindex = FindAeroportobyName(dadosPassag->dados, req.buffer);
			if (airportindex == -1) {
				res.Type = RES_AIRPORT_NOTFOUND;
			}
			else {
				res.Type = RES_AIRPORT_FOUND;
				if (dadosPassag->Passageiro->state == PASSAG_STATE_READY)
				{
					dadosPassag->Passageiro->state = PASSAG_STATE_WAITING;
					dadosPassag->dados->Aeroportos[airportindex].nPassageiros++;
				}
				res.Coord.x = dadosPassag->dados->Aeroportos[airportindex].Coord.x;
				res.Coord.y = dadosPassag->dados->Aeroportos[airportindex].Coord.y;
			}
			if (!WriteFile(dadosPassag->Passageiro->hPipe, &res, sizeof(ResponseCP), NULL, NULL)) {
				MessageBox(dadosPassag->dados->gui.hWnd, ERR_WRITE_PIPE, TEXT("Error"), MB_OK | MB_ICONWARNING);
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
			MessageBox(dados->gui.hWnd, ERR_CREATE_PIPE, TEXT("Error"), MB_OK | MB_ICONWARNING);
			continue;
		}

		if (!ConnectNamedPipe(hPipe, NULL)) {
			if (GetLastError() != ERROR_PIPE_CONNECTED)
			{
				MessageBox(dados->gui.hWnd, ERR_CONNECT_PIPE, TEXT("Error"), MB_OK | MB_ICONWARNING);
				continue;
			}
		}

		dadosPassag = malloc(sizeof(DadosPassag));
		if (dadosPassag == NULL)
			error(dados, ERR_INSUFFICIENT_MEMORY, EXIT_FAILURE);
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
			MessageBox(dados->gui.hWnd, ERR_CREATE_THREAD, TEXT("Error"), MB_OK | MB_ICONWARNING);
			RemovePassageiro(dados, index);
		}
		ReleaseMutex(dados->hMutexPassageiros);
	}

	return 0;
}

//AC Handler
void Handler(Dados* dados, CelulaBuffer* cel) {
	int index, airportindex, count;
	switch (cel->rType)
	{
	case REQ_HEARTBEAT:
		//OutputDebugString(TEXT("REQ_HEARTBEAT\n"));
		WaitForSingleObject(dados->hMutexAvioes, INFINITE);
		index = FindAviaobyPId(dados, cel->Originator.PId);
		dados->Avioes[index].lastHB = time(NULL);
		ReleaseMutex(dados->hMutexAvioes);
		break;
	case REQ_AIRPORT:
		OutputDebugString(TEXT("REQ_AIRPORT\n"));
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

		if (dados->Avioes[index].state == AVIAO_STATE_INIT)
			dados->Avioes[index].lastHB = time(NULL);

		if (airportindex == -1) {
			dados->Avioes[index].memPar->rType = RES_AIRPORT_NOTFOUND;
		}
		else {
			dados->Avioes[index].memPar->rType = RES_AIRPORT_FOUND;
			dados->Avioes[index].memPar->Coord.x = dados->Aeroportos[airportindex].Coord.x;
			dados->Avioes[index].memPar->Coord.y = dados->Aeroportos[airportindex].Coord.y;
		}
		ReleaseMutex(dados->Avioes[index].hMutexMemPar);
		SetEvent(dados->Avioes[index].hEvent);
		ReleaseMutex(dados->hMutexAvioes);
		break;
	case REQ_UPDATEPOS:
		OutputDebugString(TEXT("REQ_UPDATEPOS\n"));
		WaitForSingleObject(dados->hMutexAvioes, INFINITE);
		index = FindAviaobyPId(dados, cel->Originator.PId);

		if (dados->Avioes[index].state == AVIAO_STATE_INIT)
		{
			airportindex = FindAeroportobyCoords(dados, cel->Originator.Coord);
			dados->Aeroportos[airportindex].nAvioes++;
			dados->Avioes[index].state = AVIAO_STATE_READY;
		}
		else if (dados->Avioes[index].state == AVIAO_STATE_READY) {
			airportindex = FindAeroportobyCoords(dados, dados->Avioes[index].Coord);
			CopyMemory(&dados->Avioes[index].Origin, &dados->Aeroportos[airportindex].Coord, sizeof(Coords));
			dados->Aeroportos[airportindex].nAvioes--;
			dados->Avioes[index].state = AVIAO_STATE_FLYING;
		}

		dados->Avioes[index].Coord.x = cel->Originator.Coord.x;
		dados->Avioes[index].Coord.y = cel->Originator.Coord.y;
		ReleaseMutex(dados->hMutexAvioes);
		InvalidateRect(dados->gui.hWnd, NULL, FALSE);
		UpdateEmbarked(dados, dados->Avioes[index].PId, dados->Avioes[index].Coord); //TODO fix it;
		break;
	case REQ_UPDATEDES:
		OutputDebugString(TEXT("REQ_UPDATEDES\n"));
		WaitForSingleObject(dados->hMutexAvioes, INFINITE);
		index = FindAviaobyPId(dados, cel->Originator.PId);
		dados->Avioes[index].Dest.x = cel->Originator.Dest.x;
		dados->Avioes[index].Dest.y = cel->Originator.Dest.y;
		Disembark(dados, &dados->Avioes[index]);
		//_tprintf(TEXT("%lu changed its destination, disembarked %d passengers\n"), dados->Avioes[index].PId, Disembark(dados, &dados->Avioes[index]));
		ReleaseMutex(dados->hMutexAvioes);
		break;
	case REQ_REACHEDDES:
		OutputDebugString(TEXT("REQ_REACHEDDES\n"));
		WaitForSingleObject(dados->hMutexAvioes, INFINITE);
		index = FindAviaobyPId(dados, cel->Originator.PId);
		airportindex = FindAeroportobyCoords(dados, dados->Avioes[index].Coord);
		dados->Avioes[index].state = AVIAO_STATE_READY;
		dados->Aeroportos[airportindex].nAvioes++;
		ReachedDest(dados, &dados->Avioes[index]);
		//_tprintf(TEXT("%lu reached its destination, disembarked %d passengers\n"), dados->Avioes[index].PId, ReachedDest(dados, &dados->Avioes[index]));
		ReleaseMutex(dados->hMutexAvioes);
		break;
	case REQ_EMBARK:
		OutputDebugString(TEXT("REQ_EMBARK\n"));
		WaitForSingleObject(dados->hMutexAvioes, INFINITE);
		index = FindAviaobyPId(dados, cel->Originator.PId);
		count = Embark(dados, &dados->Avioes[index]);
		WaitForSingleObject(dados->Avioes[index].hMutexMemPar, INFINITE);
		dados->Avioes[index].memPar->rType = RES_EMBARKED_COUNT;
		dados->Avioes[index].memPar->count = count;
		ReleaseMutex(dados->Avioes[index].hMutexMemPar);
		SetEvent(dados->Avioes[index].hEvent);
		//_tprintf(TEXT("%lu embarked %d passagers\n"), dados->Avioes[index].PId, Embark(dados, &dados->Avioes[index]));
		ReleaseMutex(dados->hMutexAvioes);
	default:
		break;
	}
}

//UTILS
DWORD getRegVal(Dados* dados, const TCHAR* ValueName, const int Default) {
	HKEY key;
	LSTATUS res;
	DWORD type, value = 0, size = sizeof(value);
	res = RegCreateKeyEx(HKEY_CURRENT_USER, REG_KEY_PATH, 0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &key, NULL); // REG_OPTION_VOLATILE to be deleted on system reset.
	if (res != ERROR_SUCCESS)
		error(dados, ERR_REG_CREATE_KEY, EXIT_FAILURE);

	res = RegQueryValueEx(key, ValueName, 0, &type, (LPBYTE)&value, &size);
	if (res == ERROR_SUCCESS) {
		if (type != REG_DWORD)
			error(dados, ERR_REG_KEY_TYPE, EXIT_FAILURE);
		//_tprintf(TEXT("[SETUP] Found %s = %d.\n"), ValueName, value); //TODO idk
	}
	else if (res == ERROR_MORE_DATA)
		error(dados, ERR_REG_KEY_TYPE, EXIT_FAILURE);
	else if (res == ERROR_FILE_NOT_FOUND) {
		value = Default;
		res = RegSetValueEx(key, ValueName, 0, REG_DWORD, (LPBYTE)&value, sizeof(value));
		if (res != ERROR_SUCCESS)
			error(dados, ERR_REG_SET_KEY, EXIT_FAILURE);
		//_tprintf(TEXT("[SETUP] Set %s = %d.\n"), ValueName, value); //TODO idk
	}

	return value;
}
void error(Dados* dados, const TCHAR* msg, int exit_code) {
	TCHAR buffer[100];
	_stprintf_s(buffer, 100, TEXT("%s"), msg);
	MessageBox(dados->gui.hWnd, buffer, TEXT("Error"), MB_OK | MB_ICONERROR);
	exit(exit_code);
}

//GUI
BOOL HoverEvent(Dados* dados, int xPos, int yPos, TCHAR* buffer) {
	int xPosT, yPosT, i;
	WaitForSingleObject(dados->hMutexAvioes, INFINITE);
	for (i = 0; i < dados->nAvioes; i++) {
		if (dados->Avioes[i].state == AVIAO_STATE_FLYING)
		{
			xPosT = translateCoord(dados->Avioes[i].Coord.x);
			yPosT = translateCoord(dados->Avioes[i].Coord.y);
			if (xPos >= xPosT && xPos <= xPosT + 14 && yPos >= yPosT && yPos <= yPosT + 14) {
				_stprintf_s(buffer, 100, TEXT("ID: %lu\nOrigem: %s\nDestino: %s\nPassageiros: %d"),
					dados->Avioes[i].PId,
					dados->Aeroportos[FindAeroportobyCoords(dados, dados->Avioes[i].Origin)].Name,
					dados->Aeroportos[FindAeroportobyCoords(dados, dados->Avioes[i].Dest)].Name,
					dados->Avioes[i].nPassengers);
				ReleaseMutex(dados->hMutexAvioes);
				return TRUE;
			}
		}
	}
	ReleaseMutex(dados->hMutexAvioes);
	return FALSE;
}
BOOL ClickEvent(Dados* dados, int xPos, int yPos, TCHAR* buffer) {
	int xPosT, yPosT, i;
	WaitForSingleObject(dados->hMutexAeroportos, INFINITE);
	for (i = 0; i < dados->nAeroportos; i++) {
		xPosT = translateCoord(dados->Aeroportos[i].Coord.x);
		yPosT = translateCoord(dados->Aeroportos[i].Coord.y);
		if (xPos >= xPosT && xPos <= xPosT + 14 && yPos >= yPosT && yPos <= yPosT + 14) {
			_stprintf_s(buffer, 100, TEXT("Nome: %s\nAvioes: %d\nPassageiros: %d"),
				dados->Aeroportos[i].Name,
				dados->Aeroportos[i].nAvioes,
				dados->Aeroportos[i].nPassageiros);
			ReleaseMutex(dados->hMutexAeroportos);
			return TRUE;
		}
	}
	ReleaseMutex(dados->hMutexAeroportos);
	return FALSE;
}
int translateCoord(int coord) {
	return (coord + 1) / 2;
}
void fillAvioes(Dados* dados, HWND hList, Aeroporto* aeroporto) {
	TCHAR buffer[MAX_BUFFER];
	int i;

	SendMessage(hList, LB_RESETCONTENT, 0, 0);
	SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)TEXT(""));

	WaitForSingleObject(dados->hMutexAvioes, INFINITE);
	if (aeroporto == NULL)
	{

		for (i = 0; i < dados->nAvioes; i++) {
			if (dados->Avioes[i].state != AVIAO_STATE_INIT)
			{
				_stprintf_s(buffer, MAX_BUFFER, TEXT("%lu"), dados->Avioes[i].PId);
				SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)buffer);
			}
		}
	}
	else {
		for (i = 0; i < dados->nAvioes; i++) {
			if (dados->Avioes[i].state != AVIAO_STATE_INIT)
				if (aeroporto->Coord.x == dados->Avioes[i].Coord.x && aeroporto->Coord.y == dados->Avioes[i].Coord.y)
				{
					_stprintf_s(buffer, MAX_BUFFER, TEXT("%lu"), dados->Avioes[i].PId);
					SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)buffer);
				}
		}
	}
	ReleaseMutex(dados->hMutexAvioes);
}
void fillPassageiros(Dados* dados, HWND hList, Aeroporto* aeroporto, Aviao* aviao) {
	TCHAR buffer[MAX_BUFFER];
	int i;

	SendMessage(hList, LB_RESETCONTENT, 0, 0);

	WaitForSingleObject(dados->hMutexPassageiros, INFINITE);
	if (aviao != NULL)
	{
		for (i = 0; i < dados->nPassageiros; i++) {
			if (dados->Passageiros[i].ready)
				if (aviao->PId == dados->Passageiros[i].AviaoPId)
				{
					_stprintf_s(buffer, MAX_BUFFER, TEXT("%lu"), dados->Passageiros[i].PId);
					SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)buffer);
				}
		}
	}
	else if (aeroporto != NULL) {
		for (i = 0; i < dados->nPassageiros; i++) {
			if (dados->Passageiros[i].ready)
				if (aeroporto->Coord.x == dados->Passageiros[i].Coord.x && aeroporto->Coord.y == dados->Passageiros[i].Coord.y)
				{
					_stprintf_s(buffer, MAX_BUFFER, TEXT("%lu"), dados->Passageiros[i].PId);
					SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)buffer);
				}
		}
	}
	else {
		for (i = 0; i < dados->nPassageiros; i++) {
			if (dados->Passageiros[i].ready) {
				_stprintf_s(buffer, MAX_BUFFER, TEXT("%lu"), dados->Passageiros[i].PId);
				SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)buffer);
			}
		}
	}
	ReleaseMutex(dados->hMutexPassageiros);
}
void fillAeroportos(Dados* dados, HWND hList) {
	int i;
	SendMessage(hList, LB_RESETCONTENT, 0, 0);
	SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)TEXT(""));

	WaitForSingleObject(dados->hMutexAeroportos, INFINITE);
	for (i = 0; i < dados->nAeroportos; i++)
		SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)dados->Aeroportos[i].Name);
	ReleaseMutex(dados->hMutexAeroportos);
}
void refresh(Dados* dados, HWND hWnd) {
	HWND hList;

	hList = GetDlgItem(hWnd, IDC_LIST_Aeroportos);
	fillAeroportos(dados, hList);

	hList = GetDlgItem(hWnd, IDC_LIST_AVIOES);
	fillAvioes(dados, hList, NULL);

	hList = GetDlgItem(hWnd, IDC_LIST_PASSAGEIROS);
	fillPassageiros(dados, hList, NULL, NULL);
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
		dados->Avioes[dados->nAvioes].state = AVIAO_STATE_INIT;
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

		if (add == 0) {
			add = dados->nAvioes++; // ++dados->nAvioes
			InvalidateRect(dados->gui.hWnd, NULL, FALSE);
		}
	}else
		add = -1;
	ReleaseMutex(dados->hMutexAvioes);
	return add;
}
void RemoveAviao(Dados* dados, int index) {
	int airportindex;
	WaitForSingleObject(dados->hMutexAvioes, INFINITE);
	airportindex = FindAeroportobyCoords(dados, dados->Avioes[index].Coord);
	if (airportindex != -1)
		dados->Aeroportos[airportindex].nAvioes--;
	int i = index;
	CloseHandle(dados->Avioes[i].hEvent);
	UnmapViewOfFile(dados->Avioes[i].memPar);
	CloseHandle(dados->Avioes[i].hFileMap);
	for (i; i < dados->nAvioes - 1; i++)
		dados->Avioes[i] = dados->Avioes[i + 1];
	dados->nAvioes--;
	ReleaseMutex(dados->hMutexAvioes);
	InvalidateRect(dados->gui.hWnd, NULL, FALSE);
}
void AviaoToString(Aviao* aviao, TCHAR* buffer, const int bufferSZ) {
	switch (aviao->state)
	{
	case AVIAO_STATE_READY:
		_stprintf_s(buffer, bufferSZ, TEXT("Aviao: %lu\nVelocidade %d/s\nNº de Passageiros: %d/%d\nLocalização: %d, %d\nDestino: %d, %d"),
			aviao->PId,
			aviao->Speed,
			aviao->nPassengers, aviao->Seats,
			aviao->Coord.x, aviao->Coord.y,
			aviao->Dest.x, aviao->Dest.y);
		break;
	case AVIAO_STATE_FLYING:
		_stprintf_s(buffer, bufferSZ, TEXT("Aviao: %lu\nVelocidade %d/s\nNº de Passageiros: %d/%d\nLocalização: %d, %d\nOrigem: %d, %d\nDestino: %d, %d"),
			aviao->PId,
			aviao->Speed,
			aviao->nPassengers, aviao->Seats,
			aviao->Coord.x, aviao->Coord.y,
			aviao->Origin.x, aviao->Origin.y,
			aviao->Dest.x, aviao->Dest.y);
		break;
	default:
		break;
	}
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

int FindAeroportobyCoords(Dados* dados, Coords coords) {
	int i;
	WaitForSingleObject(dados->hMutexAeroportos, INFINITE);
	for (i = 0; i < dados->nAeroportos; i++)
		if (dados->Aeroportos[i].Coord.x == coords.x && dados->Aeroportos[i].Coord.y == coords.y) {
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
		dados->Aeroportos[dados->nAeroportos].nAvioes = 0;
		dados->Aeroportos[dados->nAeroportos].nPassageiros = 0;
		_tcscpy_s(dados->Aeroportos[dados->nAeroportos].Name, _countof(dados->Aeroportos[dados->nAeroportos].Name), newAeroporto->Name);
		ReleaseMutex(dados->hMutexAeroportos);
		InvalidateRect(dados->gui.hWnd, NULL, FALSE);
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
void AeroportoToString(Aeroporto* aeroporto, TCHAR* buffer, const int bufferSZ) {
	_stprintf_s(buffer, bufferSZ, TEXT("Aeroporto: %s\nNº de Aviões: %d\nNº de Passageiros: %d\nLocalização: %d, %d"),
		aeroporto->Name,
		aeroporto->nAvioes,
		aeroporto->nPassageiros,
		aeroporto->Coord.x, aeroporto->Coord.y);
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
		dados->Passageiros[dados->nPassageiros].state = PASSAG_STATE_INIT;
		dados->Passageiros[dados->nPassageiros].ready = FALSE;
		dados->Passageiros[dados->nPassageiros].AviaoPId = NULL;
		dados->Passageiros[dados->nPassageiros].hPipe = hPipe;
		return dados->nPassageiros++;
	}
	return -1;
}

void RemovePassageiro(Dados* dados, int index) {
	int i = index, airportindex;
	if (dados->Passageiros[i].state != PASSAG_STATE_INIT)
	{
		CloseHandle(dados->Passageiros[i].hEvent);
		DisconnectNamedPipe(dados->Passageiros[i].hPipe);
		CloseHandle(dados->Passageiros[i].hPipe);
	}

	if (dados->Passageiros[i].state == PASSAG_STATE_WAITING)
	{
		airportindex = FindAeroportobyCoords(dados, dados->Passageiros[i].Coord);
		dados->Aeroportos[airportindex].nPassageiros--;
	}else if (dados->Passageiros[i].state == PASSAG_STATE_REACHED)
	{
		airportindex = FindAeroportobyCoords(dados, dados->Passageiros[i].Dest);
		dados->Aeroportos[airportindex].nPassageiros--;
	}

	for (i; i < dados->nPassageiros - 1; i++)
		dados->Passageiros[i] = dados->Passageiros[i + 1];
	dados->nPassageiros--;
}

void PassageiroToString(Passageiro* passageiro, TCHAR* buffer, const int bufferSZ) {
	/*switch (passageiro->state)
	{
	case PASSAG_STATE_FLYING:
		_stprintf_s(buffer, bufferSZ, TEXT("Passageiro: %lu\nNome: %s\nAvião: %lu\nLocalização: %d, %d\nDestino: %d, %d"),
			passageiro->PId,
			passageiro->Name,
			passageiro->AviaoPId,
			passageiro->Coord.x, passageiro->Coord.y,
			passageiro->Dest.x, passageiro->Dest.y);
		break;
	case PASSAG_STATE_READY:
	case PASSAG_STATE_WAITING:
	case PASSAG_STATE_REACHED:
		_stprintf_s(buffer, bufferSZ, TEXT("Passageiro: %lu\nNome: %s\nAvião: %lu\nLocalização: %d, %d\nDestino: %d, %d"),
			passageiro->PId,
			passageiro->Name,
			passageiro->AviaoPId,
			passageiro->Coord.x, passageiro->Coord.y,
			passageiro->Dest.x, passageiro->Dest.y);
		break;
	default:
		break;
	}*/
	_stprintf_s(buffer, bufferSZ, TEXT("Passageiro: %lu\nNome: %s\nAvião: %lu\nLocalização: %d, %d\nDestino: %d, %d"),
		passageiro->PId,
		passageiro->Name,
		passageiro->AviaoPId,
		passageiro->Coord.x, passageiro->Coord.y,
		passageiro->Dest.x, passageiro->Dest.y);
}

int Embark(Dados* dados, Aviao* aviao) {
	int count = 0, i;
	ResponseCP res;
	res.Type = RES_EMBARKED;
	_stprintf_s(res.buffer, MAX_BUFFER, TEXT("%lu"), aviao->PId);
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
							MessageBox(dados->gui.hWnd, ERR_WRITE_PIPE, TEXT("Error"), MB_OK | MB_ICONWARNING);
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
				MessageBox(dados->gui.hWnd, ERR_WRITE_PIPE, TEXT("Error"), MB_OK | MB_ICONWARNING);
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
				MessageBox(dados->gui.hWnd, ERR_WRITE_PIPE, TEXT("Error"), MB_OK | MB_ICONWARNING);
				RemovePassageiro(dados, i);
				continue;
			}
			RemovePassageiro(dados, i);
		}
	aviao->nPassengers = 0;
	ReleaseMutex(dados->hMutexPassageiros);
}
int ReachedDest(Dados* dados, Aviao* aviao) {
	int count = 0, i, airportindex;
	ResponseCP res;
	res.Type = RES_REACHEDDEST;

	WaitForSingleObject(dados->hMutexPassageiros, INFINITE);
	for (i = 0; i < dados->nPassageiros; i++)
		if (dados->Passageiros[i].AviaoPId == aviao->PId)
		{
			if (dados->Passageiros[i].state == PASSAG_STATE_FLYING)
			{
				airportindex = FindAeroportobyCoords(dados, dados->Passageiros[i].Dest);
				dados->Passageiros[i].state = PASSAG_STATE_REACHED;
				dados->Aeroportos[airportindex].nPassageiros++;
			}
			dados->Passageiros[i].AviaoPId = NULL;
			if (!WriteFile(dados->Passageiros[i].hPipe, &res, sizeof(ResponseCP), NULL, NULL)) {
				MessageBox(dados->gui.hWnd, ERR_WRITE_PIPE, TEXT("Error"), MB_OK | MB_ICONWARNING);
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
	int i, airportindex;
	ResponseCP res;
	res.Type = RES_UPDATEDPOS;
	CopyMemory(&res.Coord, &toUpdate, sizeof(Coords));

	WaitForSingleObject(dados->hMutexPassageiros, INFINITE);
	for (i = 0; i < dados->nPassageiros; i++)
		if (dados->Passageiros[i].AviaoPId == PId) {
			if (dados->Passageiros[i].state == PASSAG_STATE_WAITING)
			{
				airportindex = FindAeroportobyCoords(dados, dados->Passageiros[i].Coord);
				dados->Passageiros[i].state = PASSAG_STATE_FLYING;
				dados->Aeroportos[airportindex].nPassageiros--;
			}
			CopyMemory(&dados->Passageiros[i].Coord, &toUpdate, sizeof(Coords));
			if (!WriteFile(dados->Passageiros[i].hPipe, &res, sizeof(ResponseCP), NULL, NULL)) {
				MessageBox(dados->gui.hWnd, ERR_WRITE_PIPE, TEXT("Error"), MB_OK | MB_ICONWARNING);
				RemovePassageiro(dados, i);
				WaitForSingleObject(dados->hMutexAvioes, INFINITE);
				dados->Avioes[FindAviaobyPId(dados, PId)].nPassengers--;
				ReleaseMutex(dados->hMutexAvioes);
			}
		}
	ReleaseMutex(dados->hMutexPassageiros);
}