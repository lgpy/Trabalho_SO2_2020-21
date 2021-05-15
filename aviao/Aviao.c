#include "Aviao_Utils.h"

DWORD WINAPI ThreadHB(LPVOID param) {
	Dados* dados = (Dados*)param;
	CelulaBuffer cel; // remove buffer?
	cel.rType = REQ_HEARTBEAT;
	while (!dados->terminar)
	{
		WaitForSingleObject(dados->hSemEscrita, INFINITE); // Espera para poder ocupar um slot para Escrita
		WaitForSingleObject(dados->hMutex, INFINITE);

		CopyMemory(&cel.Originator, dados->me, sizeof(AviaoOriginator));
		CopyMemory(&dados->memPar->buffer[dados->memPar->pWrite], &cel, sizeof(CelulaBuffer));
		dados->memPar->pWrite++;
		if (dados->memPar->pWrite == TAM_CBUFFER)
			dados->memPar->pWrite = 0;

		ReleaseMutex(dados->hMutex);
		ReleaseSemaphore(dados->hSemLeitura, 1, NULL); // Liberta um slot para Leitura

		Sleep(1000);
	}
	return 0;
}

DWORD WINAPI ThreadP(LPVOID param) {
	Dados* dados = (Dados*)param;
	while (!dados->terminar)
	{
		WaitForSingleObject(dados->hSemaphore, INFINITE); // change into mutex if includes coords
		WaitForSingleObject(dados->hSemEscrita, INFINITE); // Espera para poder ocupar um slot para Escrita
		WaitForSingleObject(dados->hMutex, INFINITE);

		memcpy(&dados->cell.Originator, dados->me, sizeof(AviaoOriginator));
		CopyMemory(&dados->memPar->buffer[dados->memPar->pWrite], &dados->cell, sizeof(CelulaBuffer));
		dados->memPar->pWrite++;
		if (dados->memPar->pWrite == TAM_CBUFFER)
			dados->memPar->pWrite = 0;

		ReleaseMutex(dados->hMutex);
		ReleaseSemaphore(dados->hSemLeitura, 1, NULL); // Liberta um slot para Leitura
	}
	return 0;
}

DWORD WINAPI ThreadV(LPVOID param) {
	int count, res = -1;
	DadosV* dados = (DadosV*)param;
	while (res != 0)
	{
		count = 0;
		WaitForSingleObject(dados->Dados->hMutex, INFINITE);

		while (count < dados->Dados->me->Speed)
		{
			res = move(dados->Dados->me->Coord.x, dados->Dados->me->Coord.y, dados->Dados->me->Dest.x, dados->Dados->me->Dest.y, &dados->Dados->me->Coord.x, &dados->Dados->me->Coord.y);
			if (res == 0)
				break;
			else if (res == 1)
				count++;
			else if (res == 2)
				_tprintf(TEXT("Erro no movimento\n"));
		}
		_tprintf(TEXT("Posicao: %d %d\n"), dados->Dados->me->Coord.x, dados->Dados->me->Coord.y);
		updatePos(dados->Dados, dados->Dados->me->Coord.x, dados->Dados->me->Coord.y);
		ReleaseMutex(dados->Dados->hMutex);
		Sleep(1000);
	}
	dados->Dados->me->State = STATE_AEROPORTO;
	dados->Dados->cell.rType = REQ_REACHEDDES;
	ReleaseSemaphore(dados->Dados->hSemaphore, 1, NULL);
	return 0;
}

int _tmain(int argc, LPTSTR argv[]) {
	int opt;
	TCHAR buffer[TAM_BUFFER];

	AviaoOriginator me;
	HANDLE hFM_AC, hFM_CA, hPThread, hHBThread, hVThread;

	Dados dados;
	DadosR dadosR;
	DadosV dadosV;
	dadosV.Dados = &dados;
	dadosV.dadosR = &dadosR;


#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif

	
	init(buffer, &me);
	dados.me = &me;
	init_dados(&hFM_AC, &hFM_CA, &dados, &dadosR);
	hPThread = CreateThread(NULL, 0, ThreadP, &dados, 0, NULL);
	if (hPThread == NULL)
		error(ERR_CREATE_THREAD, EXIT_FAILURE);

	do
	{
		requestPos(&dados, &dadosR);
	} while (dadosR.memPar->rType != RES_AIRPORT_FOUND);

	updatePos(&dados, dadosR.memPar->Coord.x, dadosR.memPar->Coord.y);

	me.State = STATE_AEROPORTO;

	hHBThread = CreateThread(NULL, 0, ThreadHB, &dados, 0, NULL);
	if (hHBThread == NULL)
		error(ERR_CREATE_THREAD, EXIT_FAILURE);

	while (TRUE)
	{
		if (me.State == STATE_AEROPORTO)
		{
			opt = -1;
			_tprintf(TEXT("\nPID: %lu\n"), me.PId);
			_tprintf(TEXT("No Aeroporto\n"));
			_tprintf(TEXT("\t1: Escolher destino\n"));
			_tprintf(TEXT("\t2: Embarcar passageiros\n"));
			_tprintf(TEXT("\t3: Iniciar viagem\n"));
			do
			{
				_fgetts(buffer, TAM_BUFFER, stdin);
				opt = _tstoi(buffer);
			} while (!(opt <= 3 && opt >= 1));
			switch (opt)
			{
				case 1:
					requestPos(&dados, &dadosR);
					if (dadosR.memPar->rType == RES_AIRPORT_FOUND)
					{
						updateDes(&dados, dadosR.memPar->Coord.x, dadosR.memPar->Coord.y);
						_tprintf(TEXT("Novo destino: %d %d\n"), me.Dest.x, me.Dest.y);
					}
					else {
						_tprintf(TEXT("Aeroporto Invalido\n"));
					}
					break;
				case 2:
					_tprintf(TEXT("Not Implemented\n"));
					break;
				case 3:
					if (me.Coord.x != me.Dest.x || me.Coord.y != me.Dest.y)
					{
						hVThread = CreateThread(NULL, 0, ThreadV, &dadosV, 0, NULL);
						if (hVThread == NULL)
							error(ERR_CREATE_THREAD, EXIT_FAILURE);
						WaitForSingleObject(hVThread,INFINITE);
					}
					else {
						_tprintf(TEXT("Escolha um destino\n"));
					}
					break;
				default:
					break;
			}
		}
	}

	WaitForSingleObject(hPThread, INFINITE);
	return EXIT_SUCCESS;
}