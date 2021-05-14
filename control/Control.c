#include "Control_Utils.h"

void Handler(DadosThread* dados, CelulaBuffer* cel) {
	Response res;
	int index, airportindex;
	if (cel->Originator.PId < 5)
	{
		return;
	}
	index = FindAviaobyPId(dados->Avioes, dados->nAvioes, cel->Originator.PId);
	if (index == -1) {
		index = AddAviao(dados->Avioes, dados->nAvioes, dados->MAX_AVIOES, &cel->Originator);
		if (index == -1)
		{
			// cant add it maxed out
		}
	}
	switch (cel->rType)
	{
		case REQ_HEARTBEAT:
			dados->Avioes[index].lastHB = time(NULL);
			break;
		case REQ_AIRPORT:
			airportindex = FindAeroportobyName(dados->Aeroportos, dados->nAeroportos, cel->buffer);
			if (airportindex == -1)
				dados->Avioes[index].memPar->rType = RES_AIRPORT_NOTFOUND;
			else
				dados->Avioes[index].memPar->rType = RES_AIRPORT_FOUND;
				dados->Avioes[index].memPar->Coord.x = dados->Aeroportos[index].Coord.x;
				dados->Avioes[index].memPar->Coord.y = dados->Aeroportos[index].Coord.y;
				
				if (SetEvent(dados->Avioes[index].hEvent))
					_tprintf(TEXT("sent response\n"));
		default:
			break;
	}
}

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

	hThread = CreateThread(NULL, 0, ThreadConsumidor, &dados, 0, NULL); // if (hThread == NULL)

	CopyMemory(dados.Aeroportos[0].Name, TEXT("a1"), sizeof(TCHAR) * 3);
	dados.Aeroportos[0].Coord.x = 50;
	dados.Aeroportos[0].Coord.y = 50;
	CopyMemory(dados.Aeroportos[1].Name, TEXT("a2"), sizeof(TCHAR) * 3);
	dados.Aeroportos[1].Coord.x = 100;
	dados.Aeroportos[1].Coord.y = 100;
	dados.nAeroportos += 2;

	PrintMenu(dados.Avioes, dados.nAvioes, dados.Aeroportos, dados.nAeroportos);

	WaitForSingleObject(hThread, INFINITE);
	free(dados.Avioes); free(dados.Aeroportos);
	return EXIT_SUCCESS;
}
