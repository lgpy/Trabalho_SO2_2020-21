#include "Control_Utils.h"

void error(const TCHAR* msg, int exit_code) {
	_tprintf(TEXT("[FATAL] %s"), msg);
	exit(exit_code);
}

DWORD getRegVal(const char* ValueName, const int Default) {
	HKEY key;
	LSTATUS res;
	DWORD type, value = 0, size = sizeof(value);
	res = RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY_PATH, 0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &key, NULL); // REG_OPTION_VOLATILE to be deleted on system reset.
	if (res != ERROR_SUCCESS)
		error(ERR_REG_CREATE_KEY, EXIT_FAILURE);

	res = RegQueryValueExW(key, ValueName, 0, &type, (LPBYTE)&value, &size);
	if (res == ERROR_SUCCESS) {
		if (type != REG_DWORD)
			error(ERR_REG_KEY_TYPE, EXIT_FAILURE);
		_tprintf(TEXT("[SETUP] Found %s = %d.\n"), ValueName, value);
	}
	else if (res == ERROR_MORE_DATA)
		error(ERR_REG_KEY_TYPE, EXIT_FAILURE);
	else if (res == ERROR_FILE_NOT_FOUND) {
		value = Default;
		res = RegSetValueExW(key, ValueName, 0, REG_DWORD, (LPBYTE)&value, sizeof(value));
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

	dados->hSemEscrita = CreateSemaphoreW(NULL, TAM_BUFFER, TAM_BUFFER, SemEscrita_NAME);
	dados->hSemLeitura = CreateSemaphoreW(NULL, 0, TAM_BUFFER, SemLeitura_NAME);
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
	for (i = 0; i < nAvioes; i++)
		_tprintf(TEXT("Aviao %d: %d seats, %d pos/s\n"), Avioes[i].PId, Avioes[i].Seats, Avioes[i].Speed);
	for (i = 0; i < nAeroportos; i++)
		_tprintf(TEXT("Aeroporto %s: %d, %d\n"), Aeroportos[i].Name, Aeroportos[i].Coord.x, Aeroportos[i].Coord.y);
}

int FindAviaobyPId(Aviao* Avioes, int nAvioes, int PId) {
	int i;
	for (i = 0; i < nAvioes; i++)
		if (Avioes[i].PId == PId)
			return i;
	return -1;
}

int AddAviao(Aviao* Avioes, int nAvioes, AviaoOriginator* newAviao) {
	Avioes[nAvioes].PId = newAviao->PId;
	Avioes[nAvioes].Seats = newAviao->Seats;
	Avioes[nAvioes].Speed = newAviao->Speed;
	nAvioes++;
	return nAvioes - 1;
}