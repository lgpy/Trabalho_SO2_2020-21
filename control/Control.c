#include "Control_Utils.h"



DWORD WINAPI ThreadConsumidor(LPVOID param) {
	Dados* dados = (Dados*)param;
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

DWORD WINAPI ThreadHBChecker(LPVOID param) {
	Dados* dados = (Dados*)param;
	int i;
	time_t timenow;
	while (!dados->terminar)
	{
		WaitForSingleObject(dados->hMutex, INFINITE);
		timenow = time(NULL);
		for (i = 0; i < dados->nAvioes; i++)
		{
			if (difftime(timenow,dados->Avioes[i].lastHB) > 3)
			{
				_tprintf(TEXT("%lu's heartbeat has stopped\n"), dados->Avioes[i].PId);
				RemoveAviao(dados, i);
			}
		}
		ReleaseMutex(dados->hMutex);
		Sleep(2000);
	}

	return 0;
}


int _tmain(int argc, LPTSTR argv[]) {
	int opt;
	TCHAR buffer[TAM_BUFFER];
	Aeroporto newAeroporto;

	HANDLE hFileMap, hThread, hHBCThread; // change names
	Dados dados; // change names

#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif

	init_dados(&dados, &hFileMap);

	newAeroporto.Coord.x = 50;
	newAeroporto.Coord.y = 50;
	CopyMemory(newAeroporto.Name, TEXT("a1"), 3 * sizeof(TCHAR));
	AddAeroporto(&dados, &newAeroporto);
	newAeroporto.Coord.x = 100;
	newAeroporto.Coord.y = 100;
	CopyMemory(newAeroporto.Name, TEXT("a2"), 3 * sizeof(TCHAR));
	AddAeroporto(&dados, &newAeroporto);

	hThread = CreateThread(NULL, 0, ThreadConsumidor, &dados, 0, NULL);
	if (hThread == NULL)
		error(ERR_CREATE_THREAD, EXIT_FAILURE);

	hHBCThread = CreateThread(NULL, 0, ThreadHBChecker, &dados, 0, NULL);
	if (hHBCThread == NULL)
		error(ERR_CREATE_THREAD, EXIT_FAILURE);

	while (TRUE)
	{
		opt = -1;
		_tprintf(TEXT("1: Listar Informação\n"));
		_tprintf(TEXT("2: Criar Aeroporto\n"));
		if (dados.aceitarAvioes)
			_tprintf(TEXT("3: Suspender aceitação de aviões\n"));
		else
			_tprintf(TEXT("3: Ativar aceitação de aviões\n"));
		_tprintf(TEXT("4: Encerrar\n"));
		do
		{
			_fgetts(buffer, TAM_BUFFER, stdin);
			opt = _tstoi(buffer);
		} while (!(opt <= 4 && opt >= 1));
		switch (opt)
		{
		case 1:
			PrintMenu(&dados);
			break;
		case 2:
			_tprintf(TEXT("Nome do Aeroporto: "));
			_fgetts(newAeroporto.Name, TAM_BUFFER, stdin);
			newAeroporto.Name[_tcslen(newAeroporto.Name) - 1] = '\0';
			_tprintf(TEXT("X: "));
			_fgetts(buffer, TAM_BUFFER, stdin);
			newAeroporto.Coord.x = _tstoi(buffer);
			_tprintf(TEXT("Y: "));
			_fgetts(buffer, TAM_BUFFER, stdin);
			newAeroporto.Coord.y = _tstoi(buffer);
			if (AddAeroporto(&dados, &newAeroporto) > -1) {
				_tprintf(TEXT("Aeroporto adicionado\n"));
			}else
				_tprintf(TEXT("Aeroporto invalido\n"));
			break;
		case 3:
			if (dados.aceitarAvioes)
				dados.aceitarAvioes = FALSE;
			else
				dados.aceitarAvioes = TRUE;
			break;
		case 4:
			_tprintf(TEXT("not implemented\n"));
			break;
		default:
			break;
		}
	}

	WaitForSingleObject(hThread, INFINITE);
	free(dados.Avioes); free(dados.Aeroportos);
	return EXIT_SUCCESS;
}
