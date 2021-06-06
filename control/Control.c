#include "Control_Utils.h"

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

int _tmain(int argc, LPTSTR argv[]) {
	int i;

	HANDLE hFileMap, hThread, hHBCThread, hNPThread; // change names
	Dados dados; // change names

#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif

	init_dados(&dados, &hFileMap);

#ifdef DEBUG
	Aeroporto newAeroporto;
	newAeroporto.Coord.x = 50;
	newAeroporto.Coord.y = 50;
	CopyMemory(newAeroporto.Name, TEXT("a1"), 3 * sizeof(TCHAR));
	AddAeroporto(&dados, &newAeroporto);
	newAeroporto.Coord.x = 100;
	newAeroporto.Coord.y = 100;
	CopyMemory(newAeroporto.Name, TEXT("a2"), 3 * sizeof(TCHAR));
	AddAeroporto(&dados, &newAeroporto);
#endif // DEBUG
	

	hThread = CreateThread(NULL, 0, ThreadConsumidor, &dados, 0, NULL);
	if (hThread == NULL)
		error(ERR_CREATE_THREAD, EXIT_FAILURE);

	hHBCThread = CreateThread(NULL, 0, ThreadHBChecker, &dados, 0, NULL);
	if (hHBCThread == NULL)
		error(ERR_CREATE_THREAD, EXIT_FAILURE);

	hNPThread = CreateThread(NULL, 0, ThreadNewPassag, &dados, 0, NULL);
	if (hNPThread == NULL)
		error(ERR_CREATE_THREAD, EXIT_FAILURE);

	while (dados.terminar == 0)
		PrintMenu(&dados);

	WaitForSingleObject(hThread, INFINITE);
	//WaitForSingleObject(hHBCThread, INFINITE);
	free(dados.Avioes); free(dados.Aeroportos);
	return EXIT_SUCCESS;
}
