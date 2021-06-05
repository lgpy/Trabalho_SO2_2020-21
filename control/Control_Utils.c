#include "Control_Utils.h"

//INIT
void init_dados(Dados* dados, HANDLE* hFileMap) {
	int i, x;
	// Get/Set Registry keys
	dados->MAX_AVIOES = getRegVal(REG_MAX_AVIOES_KEY_NAME, 10);
	dados->MAX_AEROPORTOS = getRegVal(REG_MAX_AEROPORTOS_KEY_NAME, 5);
	dados->MAX_PASSAGEIROS = 1;

	// Allocate memory for MAX vals
	dados->Avioes = malloc(sizeof(Aviao) * dados->MAX_AVIOES);
	dados->Aeroportos = malloc(sizeof(Aeroporto) * dados->MAX_AEROPORTOS);
	dados->Passageiros = malloc(sizeof(Passageiro));
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
				_tprintf(TEXT("%lu's heartbeat has stopped\n"), dados->Avioes[i].PId);
				RemoveAviao(dados, i);
			}
		}
		ReleaseMutex(dados->hMutexAvioes);
		Sleep(1000);
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
		_tprintf(TEXT("%lu reached its destination, disembarked %d passengers\n"), dados->Avioes[index].PId, Disembark(dados, &dados->Avioes[index]));
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
		_tprintf(TEXT("\t%5lu: %3d, %3d | %3d, %3d | %2d/s | %3d seats\n"), dados->Avioes[i].PId, dados->Avioes[i].Coord.x, dados->Avioes[i].Coord.y, dados->Avioes[i].Dest.x, dados->Avioes[i].Dest.y, dados->Avioes[i].Speed, dados->Avioes[i].Seats);
	ReleaseMutex(dados->hMutexAvioes);

	_tprintf(TEXT("\nAeroportos\n"));
	WaitForSingleObject(dados->hMutexAeroportos, INFINITE);
	for (i = 0; i < dados->nAeroportos; i++)
		_tprintf(TEXT("\t%5s: %3d, %3d\n"), dados->Aeroportos[i].Name, dados->Aeroportos[i].Coord.x, dados->Aeroportos[i].Coord.y);
	ReleaseMutex(dados->hMutexAeroportos);

	_tprintf(TEXT("\nPassageiros\n"));
	WaitForSingleObject(dados->hMutexPassageiros, INFINITE);
	for (i = 0; i < dados->nPassageiros; i++)
		_tprintf(TEXT("\t%s\n"), dados->Passageiros[i].Name);
	ReleaseMutex(dados->hMutexPassageiros);
}
void PrintMenu(Dados* dados) {
	static TCHAR buffer[MAX_BUFFER];
	Aeroporto newAeroporto;
	int opt = -1, i = 0;

	_tprintf(TEXT("1: Listar Informação\n"));
	_tprintf(TEXT("2: Criar Aeroporto\n"));
	if (dados->aceitarAvioes)
		_tprintf(TEXT("3: Suspender aceitação de aviões\n"));
	else
		_tprintf(TEXT("3: Ativar aceitação de aviões\n"));
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
int AddPassageiro(Dados* dados, Passageiro* newPassageiro) {
	Passageiro* newPointer;
	if (dados->nPassageiros < dados->MAX_PASSAGEIROS) {
		newPointer = realloc(dados->Passageiros, sizeof(Passageiro) * (dados->nPassageiros + 1)); // check if reallocated
		if (newPointer == NULL) {
			_tprintf(TEXT("%s\n"), ERR_INSUFFICIENT_MEMORY);
			return -1;
		}
		dados->Passageiros = newPointer;
		dados->MAX_PASSAGEIROS++;
	}
	newPassageiro->terminar = 0;
	newPassageiro->AviaoPId = NULL;
	CopyMemory(&dados->Passageiros[dados->nPassageiros], newPassageiro, sizeof(Passageiro));
	return dados->nPassageiros++;
}
void RemovePassageiro(Dados* dados, int index) {
	WaitForSingleObject(dados->hMutexPassageiros, INFINITE);
	int i = index;
	//TODO free() stuff
	for (i; i < dados->nPassageiros - 1; i++)
		dados->Passageiros[i] = dados->Passageiros[i + 1];
	dados->nPassageiros--;
	ReleaseMutex(dados->hMutexPassageiros);
}

int Embark(Dados* dados, Aviao* aviao) {
	int count = 0, i;
	ResponseCP res;
	res.Type = RES_EMBARKED;
	WaitForSingleObject(dados->hMutexPassageiros, INFINITE);
	for (i = 0; i < dados->nPassageiros; i++)
		if (dados->Passageiros[i].Coord.x == aviao->Coord.x && dados->Passageiros[i].Coord.y == aviao->Coord.y)
			if (dados->Passageiros[i].Dest.x == aviao->Dest.x && dados->Passageiros[i].Dest.y == aviao->Dest.y)
				if (dados->Passageiros[i].AviaoPId == NULL)
				{
					dados->Passageiros[i].AviaoPId = aviao->PId;
					//WriteFile(dados->Passageiros[i].hPipe, &res, sizeof(ResponseCP), NULL, NULL); //TODO writefile gets stuck
					count++;
				}
	ReleaseMutex(dados->hMutexPassageiros);
	return count;
}

int Disembark(Dados* dados, Aviao* aviao) {
	int count = 0, i;
	WaitForSingleObject(dados->hMutexPassageiros, INFINITE);
	for (i = 0; i < dados->nPassageiros; i++)
		if (dados->Passageiros[i].AviaoPId == aviao->PId)
		{
			dados->Passageiros[i].AviaoPId = NULL;
			count++;
		}
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
		if (dados->Passageiros[i].AviaoPId == PId)
			continue; //TODO writefile gets stuck
			/*if (!WriteFile(dados->Passageiros[i].hPipe, &res, sizeof(ResponseCP), NULL, NULL)) {
				_tprintf(TEXT("%s\n"), ERR_WRITE_PIPE);
				dados->Passageiros[i].terminar = 1;
			}*/
	ReleaseMutex(dados->hMutexPassageiros);
	_tprintf(TEXT("UpdateEmbarked\n"));
}