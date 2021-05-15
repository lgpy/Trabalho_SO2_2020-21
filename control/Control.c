#include "Control_Utils.h"



DWORD WINAPI ThreadConsumidor(LPVOID param) {
	DadosThread* dados = (DadosThread*)param;
	CelulaBuffer cel;

	while (!dados->terminar)
	{
		WaitForSingleObject(dados->hSemLeitura, INFINITE); // Espera para poder ocupar um slot para Escrita
		WaitForSingleObject(dados->hMutex, INFINITE);

		CopyMemory(&cel, &dados->memPar->buffer[dados->memPar->pRead], sizeof(CelulaBuffer));
		dados->memPar->pRead++;
		if (dados->memPar->pRead == TAM_CBUFFER)
			dados->memPar->pRead = 0;

		Handler(dados, &cel);

		ReleaseMutex(dados->hMutex);
		ReleaseSemaphore(dados->hSemEscrita, 1, NULL); // Liberta um slot para Leitura
	}

	return 0;
}


int _tmain(int argc, LPTSTR argv[]) {
	TCHAR comando[100];

	HANDLE hFileMap, hThread; // change names
	DadosThread dados; // change names

#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif

	init_dados(&hFileMap, &dados);
	dados.MAX_AVIOES = getRegVal(REG_MAX_AVIOES_KEY_NAME, 10);
	dados.MAX_AEROPORTOS = getRegVal(REG_MAX_AEROPORTOS_KEY_NAME, 5);
	dados.Avioes = malloc(sizeof(Aviao) * dados.MAX_AVIOES);
	dados.Aeroportos = malloc(sizeof(Aeroporto) * dados.MAX_AEROPORTOS);
	if (dados.Avioes == NULL || dados.Aeroportos == NULL) {
		error(ERR_INSUFFICIENT_MEMORY, EXIT_FAILURE);
	}
	dados.nAeroportos = 0; dados.nAvioes = 0;

	CopyMemory(dados.Aeroportos[0].Name, TEXT("a1"), sizeof(TCHAR) * 3);
	dados.Aeroportos[0].Coord.x = 50;
	dados.Aeroportos[0].Coord.y = 50;
	CopyMemory(dados.Aeroportos[1].Name, TEXT("a2"), sizeof(TCHAR) * 3);
	dados.Aeroportos[1].Coord.x = 100;
	dados.Aeroportos[1].Coord.y = 100;
	dados.nAeroportos += 2;

	hThread = CreateThread(NULL, 0, ThreadConsumidor, &dados, 0, NULL); // if (hThread == NULL)
	if (hThread == NULL)
		error(ERR_CREATE_THREAD, EXIT_FAILURE);

	WaitForSingleObject(hThread, INFINITE);
	free(dados.Avioes); free(dados.Aeroportos);
	return EXIT_SUCCESS;
}
