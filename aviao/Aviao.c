#include "Aviao_Utils.h"

DWORD WINAPI ThreadHB(LPVOID param) {
	DadosHB* dados = (DadosHB*)param;
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

DWORD WINAPI ThreadP(LPVOID param) {
	DadosP* dados = (DadosP*)param;
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
/*
DWORD WINAPI ThreadRecieve(LPVOID param) {
	DadosRThread* dados = (DadosRThread*)param;
	while (!dados->terminar)
	{
		WaitForSingleObject(dados->hEvent, INFINITE);
		WaitForSingleObject(dados->hMutex, INFINITE);

		ReleaseMutex(dados->hMutex);
	}
	return 0;
}*/

int _tmain(int argc, LPTSTR argv[]) {
	AviaoOriginator me;
	HANDLE hFM_AC, hFM_CA, hPThread, hHBThread/*, hRThread*/;

	DadosP dadosP;
	DadosHB dadosHB;
	DadosR dadosR;

	TCHAR buffer[TAM_BUFFER];

#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif

	
	init(&buffer, &me);
	dadosP.me = &me; dadosHB.me = &me;
	init_dados(&hFM_AC, &hFM_CA, &dadosP, &dadosHB, &dadosR);
	hPThread = CreateThread(NULL, 0, ThreadP, &dadosP, 0, NULL); // if (hHBThread == NULL)

	CopyMemory(dadosP.cell.buffer, buffer, sizeof(TCHAR) * _tcslen(buffer));
	dadosP.cell.buffer[_tcslen(buffer) - 1] = '\0';
	dadosP.cell.rType = REQ_AIRPORT; //REQ_NEW?
	SetEvent(dadosP.hEvent);

	_tprintf(TEXT("Waiting for response\n"));
	WaitForSingleObject(dadosR.hEvent, INFINITE);
	_tprintf(TEXT("Waiting for response\n"));
	WaitForSingleObject(hPThread, INFINITE);
	return EXIT_SUCCESS;
}