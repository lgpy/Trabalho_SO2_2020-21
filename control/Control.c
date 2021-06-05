#include "Control_Utils.h"

DWORD WINAPI ThreadReqHandler(LPVOID param) {
	DadosPassag* dadosPassag = (DadosPassag*)param;
	RequestCP req;
	ResponseCP res;
	int airportindex;

	while (!dadosPassag->terminar) {//TODO CHANGE
		if (!ReadFile(dadosPassag->dados->Passageiros[dadosPassag->index].Pipe.hPipe, &req, sizeof(RequestCP), NULL, NULL)) {
			_tprintf(TEXT("%s\n"), ERR_READ_PIPE);
			continue;
		}

		switch (req.Type)
		{
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
			if (!WriteFile(dadosPassag->dados->Passageiros[dadosPassag->index].Pipe.hPipe, &res, sizeof(ResponseCP), NULL, NULL)) {
				_tprintf(TEXT("%s\n"), ERR_WRITE_PIPE);
			}
			break;
		case REQ_UPDATE:
			CopyMemory(&dadosPassag->dados->Passageiros[dadosPassag->index].Coord, &req.Originator.Coord, sizeof(Coords));
			CopyMemory(&dadosPassag->dados->Passageiros[dadosPassag->index].Dest, &req.Originator.Dest, sizeof(Coords));
			break;
		case REQ_INIT:
			res.Type = RES_ADDED;
			if (!WriteFile(dadosPassag->dados->Passageiros[dadosPassag->index].Pipe.hPipe, &res, sizeof(ResponseCP), NULL, NULL)) {
				_tprintf(TEXT("%s\n"), ERR_WRITE_PIPE);
			}
			break;
		default:
			break;
		}
	}
	return 0;
}

DWORD WINAPI ThreadNewPassag(LPVOID param) {
	HANDLE hPipe, hThread, hEventTemp;
	Dados* dados = (Dados*)param;
	RequestCP req;
	ResponseCP res;
	int i, airportindex;
	DWORD offset, nBytes;

	for (i = 0; i < MAX_PASSAGEIROS; i++)
	{
		hPipe = CreateNamedPipe(
			NamedPipe_NAME,
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			MAX_PASSAGEIROS,
			sizeof(ResponseCP),
			sizeof(RequestCP),
			1000,
			NULL);

		if (hPipe == INVALID_HANDLE_VALUE)
			error(ERR_CREATE_PIPE, EXIT_FAILURE);

		hEventTemp = CreateEvent(NULL, TRUE, FALSE, NULL);

		if (hEventTemp == NULL)
			error(ERR_CREATE_EVENT, EXIT_FAILURE);

		dados->Passageiros[i].Pipe.hPipe = hPipe;
		dados->Passageiros[i].Pipe.active = FALSE;
		ZeroMemory(&dados->Passageiros[i].Pipe.overlap, sizeof(dados->Passageiros[i].Pipe.overlap));
		dados->Passageiros[i].Pipe.overlap.hEvent = hEventTemp;
		dados->hPassagEvents[i] = hEventTemp;

		ConnectNamedPipe(hPipe, &dados->Passageiros[i].Pipe.overlap);
	}

	while (!dados->terminar) {
		offset = WaitForMultipleObjects(MAX_PASSAGEIROS, dados->hPassagEvents, FALSE, INFINITE);
		i = offset - WAIT_OBJECT_0;
		if (i >= 0 && i < MAX_PASSAGEIROS) {
			_tprintf(TEXT("[ESCRITOR] Leitor %d chegou\n"), i);
			if (GetOverlappedResult(dados->Passageiros[i].Pipe.hPipe, &dados->Passageiros[i].Pipe.hPipe, &nBytes, FALSE)) {
				ResetEvent(dados->hPassagEvents[i]);
				
				dados->Passageiros[i].dadosPassag.dados = dados;
				dados->Passageiros[i].dadosPassag.index = i;
				dados->Passageiros[i].dadosPassag.terminar = 0;

				dados->Passageiros[i].hThread = CreateThread(NULL, 0, ThreadReqHandler, &dados->Passageiros[i].dadosPassag, 0, NULL);
				if (dados->Passageiros[i].hThread == NULL)
					error(ERR_CREATE_THREAD, EXIT_FAILURE);
			}
		}
	}
}

int _tmain(int argc, LPTSTR argv[]) {
	int i;

	HANDLE hFileMap, hThread, hHBCThread, hThreadPipes, hThreadReqHandler; // change names
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

	hThreadPipes = CreateThread(NULL, 0, ThreadNewPassag, &dados, 0, NULL);
	if (hThreadPipes == NULL)
		error(ERR_CREATE_THREAD, EXIT_FAILURE);

	while (dados.terminar == 0)
		PrintMenu(&dados);

	WaitForSingleObject(hThread, INFINITE);
	//WaitForSingleObject(hHBCThread, INFINITE);
	free(dados.Avioes); free(dados.Aeroportos);
	return EXIT_SUCCESS;
}
