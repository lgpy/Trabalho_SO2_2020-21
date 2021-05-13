#include "Aviao_Utils.h"

DWORD WINAPI ThreadHeartBeat(LPVOID param) {
	DadosHeartBeatThread* dados = (DadosHeartBeatThread*)param;
	CelulaBuffer cel;
	while (!dados->terminar)
	{
		cel.rType = REQ_HEARTBEAT;
		cel.buffer[0] = '\0';
		memcpy(&cel.Originator, dados->me, sizeof(AviaoOriginator)); // change into mutex if includes coords

		WaitForSingleObject(dados->hSemEscrita, INFINITE); // Espera para poder ocupar um slot para Escrita
		WaitForSingleObject(dados->hMutex, INFINITE);

		CopyMemory(&dados->memPar->buffer[dados->memPar->pWrite], &cel, sizeof(CelulaBuffer));
		dados->memPar->pWrite++;
		if (dados->memPar->pWrite == TAM_BUFFER)
			dados->memPar->pWrite = 0;

		ReleaseMutex(dados->hMutex);
		ReleaseSemaphore(dados->hSemLeitura, 1, NULL); // Liberta um slot para Leitura

		Sleep(1000);
	}
	return 0;
}

int _tmain(int argc, LPTSTR argv[]) {
	HANDLE hFileMap, hHBThread;

	AviaoOriginator me;
	DadosThread dados;
	DadosHeartBeatThread dadosHB;
	char comando[100];

#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif

	me.PId = GetCurrentProcessId();
	me.Seats = 9999; //get from user
	me.Speed = 10; //get from user
	dados.me = &me; dadosHB.me = &me;
	init_dados(&hFileMap, &dados, &dadosHB);

	hHBThread = CreateThread(NULL, 0, ThreadHeartBeat, &dados, 0, NULL);
	if (hHBThread != NULL) {
		_tprintf(TEXT("Escreva qualquer coisa para sair.\n"));
		_getts_s(comando, 100);
		dados.terminar = 1;
		WaitForSingleObject(hHBThread, INFINITE);
	}
	return EXIT_SUCCESS;
}
