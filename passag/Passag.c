#include "Passag_Utils.h"

DWORD WINAPI ThreadResponseHandler(LPVOID param) {
	Data* dados = (Data*)param;

	RequestCP req;
	ResponseCP res;

	while (!dados->terminar)
	{
		if (!ReadFile(dados->hPipe, &res, sizeof(ResponseCP), NULL, NULL)) {
			_tprintf(TEXT("%s\n"), ERR_READ_PIPE);
			dados->terminar = 1;
			continue;
		}

		switch (res.Type)
		{
		case RES_EMBARKED:
			_tprintf(TEXT("Embarcou\n"));
			break;
		case RES_UPDATEDPOS:
			_tprintf(TEXT("Posicao: %d %d\n"), res.Coord.x, res.Coord.y);
			break;
		case RES_DISEMBARKED:
			_tprintf(TEXT("Desembarcou\n"));
			break;
		case RES_REACHEDDEST:
			_tprintf(TEXT("Chegou ao destino\n"));
			dados->terminar = 1;
			break;
		case RES_CRASHED:
			_tprintf(TEXT("Aviao crashou\n"));
			dados->terminar = 1;
			break;
		default:
			break;
		}
	}
	return 0;
}



int _tmain(int argc, LPTSTR argv[]) {
	Data dados;
	TCHAR buffer[MAX_BUFFER];
	int opt;

#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif

	init(&dados);
	PrintInfo(&dados);

	dados.hRHThread = CreateThread(NULL, 0, ThreadResponseHandler, &dados, 0, NULL);
	if (dados.hRHThread == NULL)
		error(ERR_CREATE_THREAD, EXIT_FAILURE);

	while (!dados.terminar) {
		_fgetts(buffer, MAX_BUFFER, stdin);
		if (_tcscmp(buffer, TEXT("fim\n")) == 0) {
			dados.terminar = TRUE;
		}
	}

	SetEvent(dados.hEvent);	
	return EXIT_SUCCESS;
}