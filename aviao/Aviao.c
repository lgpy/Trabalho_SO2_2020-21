#include "Aviao_Utils.h"

DWORD WINAPI ThreadHB(LPVOID param) {
	DadosHB* dados = (DadosHB*)param;
	CelulaBuffer cel; // remove buffer?
	cel.rType = REQ_HEARTBEAT;
	cel.buffer[0] = '\0';
	memcpy(&cel.Originator, dados->me, sizeof(AviaoOriginator)); // change into mutex if includes coords
	while (!dados->terminar)
	{
		WaitForSingleObject(dados->hSemEscrita, INFINITE); // Espera para poder ocupar um slot para Escrita
		WaitForSingleObject(dados->hMutex, INFINITE);

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
	DadosP* dados = (DadosP*)param;
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
		WaitForSingleObject(dados->dadosP->hMutex, INFINITE);

		while (count < dados->dadosP->me->Speed)
		{
			res = move(dados->dadosP->me->Coord.x, dados->dadosP->me->Coord.y, dados->dadosP->me->Dest.x, dados->dadosP->me->Dest.y, &dados->dadosP->me->Coord.x, &dados->dadosP->me->Coord.y);
			if (res == 0)
				break;
			else if (res == 1)
				count++;
			else if (res == 2)
				_tprintf(TEXT("Erro no movimento\n"));
		}
		_tprintf(TEXT("Posicao: %d %d\n"), dados->dadosP->me->Coord.x, dados->dadosP->me->Coord.y);
		updatePos(dados->dadosP, dados->dadosR, dados->dadosP->me, dados->dadosP->me->Coord.x, dados->dadosP->me->Coord.y);
		ReleaseMutex(dados->dadosP->hMutex);
		Sleep(1000);
	}
	dados->dadosP->me->State = STATE_AEROPORTO;
	return 0;
}

int _tmain(int argc, LPTSTR argv[]) {
	int opt;
	TCHAR buffer[TAM_BUFFER];

	AviaoOriginator me;
	HANDLE hFM_AC, hFM_CA, hPThread, hHBThread, hVThread;

	DadosP dadosP;
	DadosHB dadosHB;
	DadosR dadosR;
	DadosV dadosV;
	dadosV.dadosP = &dadosP;
	dadosV.dadosR = &dadosR;


#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif

	
	init(buffer, &me);
	dadosP.me = &me; dadosHB.me = &me;
	init_dados(&hFM_AC, &hFM_CA, &dadosP, &dadosHB, &dadosR);
	hPThread = CreateThread(NULL, 0, ThreadP, &dadosP, 0, NULL); // if (hHBThread == NULL)
	if (hPThread == NULL)
		error(ERR_CREATE_THREAD, EXIT_FAILURE);

	do
	{
		requestPos(&dadosP, &dadosR);
	} while (dadosR.memPar->rType != RES_AIRPORT_FOUND);

	me.Dest.x = dadosR.memPar->Coord.x;
	me.Dest.y = dadosR.memPar->Coord.y;
	updatePos(&dadosP, &dadosR, &me, dadosR.memPar->Coord.x, dadosR.memPar->Coord.y);

	me.State = STATE_AEROPORTO;

	while (TRUE)
	{
		if (me.State == STATE_AEROPORTO)
		{
			opt = -1;
			_tprintf(TEXT("No Aeroporto\n"));
			_tprintf(TEXT("\t0: Escolher destino\n"));
			_tprintf(TEXT("\t1: Embarcar passageiros\n"));
			_tprintf(TEXT("\t2: Iniciar viagem\n"));
			do
			{
				_fgetts(buffer, TAM_BUFFER, stdin);
				opt = _tstoi(buffer);
			} while (!(opt <= 2 && opt >= 0));
			switch (opt)
			{
				case 0:
					requestPos(&dadosP, &dadosR);
					if (dadosR.memPar->rType == RES_AIRPORT_FOUND)
					{
						me.Dest.x = dadosR.memPar->Coord.x;
						me.Dest.y = dadosR.memPar->Coord.y;
						_tprintf(TEXT("Novo destino: %d %d\n"), me.Dest.x, me.Dest.y);
					}
					else {
						_tprintf(TEXT("Aeroporto Invalido\n"));
					}
					break;
				case 1:
					_tprintf(TEXT("Not Implemented\n"));
					break;
				case 2:
					if (me.Coord.x != me.Dest.x || me.Coord.y != me.Dest.y)
					{
						hVThread = CreateThread(NULL, 0, ThreadV, &dadosV, 0, NULL); // if (hHBThread == NULL)
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