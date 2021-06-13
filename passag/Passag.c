#include "Passag_Utils.h"


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
	//WaitForSingleObject(dados.hRHThread, INFINITE);
	return EXIT_SUCCESS;
}