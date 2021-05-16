#include "Aviao_Utils.h"

DWORD WINAPI ThreadHB(LPVOID param) {
	DadosHB* dados = (DadosHB*)param;
	CelulaBuffer cel; // remove buffer?
	cel.rType = REQ_HEARTBEAT;
	while (!dados->terminar)
	{
		WaitForSingleObject(*dados->hSemEscrita, INFINITE); // Espera para poder ocupar um slot para Escrita

		WaitForSingleObject(*dados->hMutexMe, INFINITE);
		CopyMemory(&cel.Originator, dados->me, sizeof(AviaoOriginator));
		ReleaseMutex(*dados->hMutexMe);

		WaitForSingleObject(*dados->hMutexMempar, INFINITE);

		CopyMemory(&dados->memPar->buffer[dados->memPar->pWrite], &cel, sizeof(CelulaBuffer));
		dados->memPar->pWrite++;
		if (dados->memPar->pWrite == TAM_CBUFFER)
			dados->memPar->pWrite = 0;

		ReleaseMutex(*dados->hMutexMempar);
		ReleaseSemaphore(*dados->hSemLeitura, 1, NULL); // Liberta um slot para Leitura

		Sleep(1000);
	}
	return 0;
}

DWORD WINAPI ThreadP(LPVOID param) {
	DadosP* dados = (DadosP*)param; // incluir uma cell nos dados desta thread
	CelulaBuffer cel;
	while (!dados->terminar)
	{
		WaitForSingleObject(*dados->hSemaphoreProduce, INFINITE); // change into mutex if includes coords
		WaitForSingleObject(*dados->hSemEscrita, INFINITE); // Espera para poder ocupar um slot para Escrita

		cel.rType = dados->rType;
		CopyMemory(cel.buffer, dados->buffer, sizeof(dados->buffer)); //maybe doesnt work
		WaitForSingleObject(*dados->hMutexMe, INFINITE);
		CopyMemory(&cel.Originator, dados->me, sizeof(AviaoOriginator));
		ReleaseMutex(*dados->hMutexMe);

		WaitForSingleObject(*dados->hMutexMempar, INFINITE);

		CopyMemory(&dados->memPar->buffer[dados->memPar->pWrite], &cel, sizeof(CelulaBuffer));
		dados->memPar->pWrite++;
		if (dados->memPar->pWrite == TAM_CBUFFER)
			dados->memPar->pWrite = 0;

		ReleaseMutex(*dados->hMutexMempar);
		ReleaseSemaphore(*dados->hSemLeitura, 1, NULL); // Liberta um slot para Leitura
	}
	return 0;
}

DWORD WINAPI ThreadV(LPVOID param) {
	int count, res;
	DadosV* dados = (DadosV*)param;
	Coords newC;
	do
	{
		count = 0;

		WaitForSingleObject(*dados->hMutexMe, INFINITE);
		while (count < dados->me->Speed)
		{
			WaitForSingleObject(*dados->hMutexMapa, INFINITE);
			res = move(dados->me->Coord.x, dados->me->Coord.y, dados->me->Dest.x, dados->me->Dest.y, &newC.x, &newC.y);
			//dados->memPar->Map
			if (res == 0)
			{
				ReleaseMutex(*dados->hMutexMapa);
				break;
			}
			if (res == 1) {
				count++;
				dados->me->Coord.x = newC.x;
				dados->me->Coord.y = newC.y; 
				_tprintf(TEXT("Posicao: %d %d\n"), dados->me->Coord.x, dados->me->Coord.y);
				updatePosV(dados, dados->me->Coord.x, dados->me->Coord.y);
			}
			else if (res == 2)
				_tprintf(TEXT("Erro no movimento\n"));

			ReleaseMutex(*dados->hMutexMapa); // TBD
		}
		ReleaseMutex(*dados->hMutexMe);
		Sleep(1000);
	} while (res != 0);
	dados->me->State = STATE_AEROPORTO;
	return 0;
}

int _tmain(int argc, LPTSTR argv[]) {
	int opt;
	TCHAR buffer[TAM_BUFFER];
	Data dados;
	DadosHB dadosHB;
	DadosP dadosP;
	DadosV dadosV;

#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif

	
	init(buffer, &dados.me);
	init_dados(&dados, &dadosHB, &dadosP, &dadosV);
	dados.threads.hPThread = CreateThread(NULL, 0, ThreadP, &dadosP, 0, NULL);
	if (dados.threads.hPThread == NULL)
		error(ERR_CREATE_THREAD, EXIT_FAILURE);

	do
	{
		requestPos(&dadosP, &dados.events.hEvent_CA);
	} while (dados.sharedmem.MemPar_CA->rType != RES_AIRPORT_FOUND);

	updatePos(&dadosP, dados.sharedmem.MemPar_CA->Coord.x, dados.sharedmem.MemPar_CA->Coord.y);
	dados.me.State = STATE_AEROPORTO;

	dados.threads.hHBThread = CreateThread(NULL, 0, ThreadHB, &dadosHB, 0, NULL);
	if (dados.threads.hHBThread == NULL)
		error(ERR_CREATE_THREAD, EXIT_FAILURE);

	while (TRUE)
	{
		if (dados.me.State == STATE_AEROPORTO)
		{
			opt = -1;
			_tprintf(TEXT("\nPID: %lu\n"), dados.me.PId);
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
					requestPos(&dadosP, &dados.events.hEvent_CA);
					if (dados.sharedmem.MemPar_CA->rType == RES_AIRPORT_FOUND)
					{
						updateDes(&dadosP, dados.sharedmem.MemPar_CA->Coord.x, dados.sharedmem.MemPar_CA->Coord.y);
						_tprintf(TEXT("Novo destino: %d %d\n"), dados.me.Dest.x, dados.me.Dest.y);
					}
					else {
						_tprintf(TEXT("Aeroporto Invalido\n"));
					}
					break;
				case 2:
					_tprintf(TEXT("Not Implemented\n"));
					break;
				case 3:
					WaitForSingleObject(dados.mutexes.hMutexMe, INFINITE);
					if (dados.me.Coord.x != dados.me.Dest.x || dados.me.Coord.y != dados.me.Dest.y)
					{
						ReleaseMutex(dados.mutexes.hMutexMe);
						dados.threads.hVThread = CreateThread(NULL, 0, ThreadV, &dadosV, 0, NULL);
						if (dados.threads.hVThread == NULL)
							error(ERR_CREATE_THREAD, EXIT_FAILURE);
						WaitForSingleObject(dados.threads.hVThread,INFINITE);
						dadosP.rType = REQ_REACHEDDES;
						ReleaseSemaphore(dados.semaphores.hSemaphoreProduce, 1, NULL);
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
	return EXIT_SUCCESS;
}