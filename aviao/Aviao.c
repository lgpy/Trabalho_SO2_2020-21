#include "Aviao_Utils.h"

DWORD WINAPI ThreadHeartBeat(LPVOID param) {
	DadosHeartBeatThread* dados = (DadosHeartBeatThread*)param;
	CelulaBuffer cel; // remove buffer?
	while (!dados->terminar)
	{
		cel.rType = REQ_HEARTBEAT;
		cel.buffer[0] = '\0';
		memcpy(&cel.Originator, dados->me, sizeof(AviaoOriginator)); // change into mutex if includes coords

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

DWORD WINAPI ThreadProdutor(LPVOID param) {
	DadosThread* dados = (DadosThread*)param;
	while (!dados->terminar)
	{
		WaitForSingleObject(dados->hEvent, INFINITE); // change into mutex if includes coords
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


int _tmain(int argc, LPTSTR argv[]) {
	HANDLE hFileMap, hThread, hHBThread;

	AviaoOriginator me;
	DadosThread dados;
	DadosHeartBeatThread dadosHB;
	TCHAR comando[TAM_BUFFER];

#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif

	me.PId = GetCurrentProcessId();
	_tprintf(TEXT("PID: %d\n"), me.PId);
	me.Seats = 9999; //get from user
	me.Speed = 10; //get from user
	dados.me = &me; dadosHB.me = &me;
	init_dados(&hFileMap, &dados, &dadosHB);
	hThread = CreateThread(NULL, 0, ThreadProdutor, &dados, 0, NULL); // if (hHBThread == NULL)

	//hHBThread = CreateThread(NULL, 0, ThreadHeartBeat, &dadosHB, 0, NULL); // if (hHBThread == NULL)

	_tprintf(TEXT("aeroporto: "));
	_fgetts(comando, TAM_BUFFER, stdin);
	CopyMemory(dados.cell.buffer, comando, sizeof(TCHAR) * _tcslen(comando));
	dados.cell.buffer[_tcslen(comando) - 1] = '\0';
	dados.cell.rType = REQ_AIRPORT;
	SetEvent(dados.hEvent);

	WaitForSingleObject(hThread, INFINITE);

	return EXIT_SUCCESS;
}
