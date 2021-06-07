#include "Control_Utils.h"

int _tmain(int argc, LPTSTR argv[]) {
	int i;

	HANDLE hFileMap, hThread, hHBCThread, hNPThread; // change names
	Dados dados; // change names

#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif

	init_dados(&dados, &hFileMap);

#ifdef DEBUG
	Aeroporto newAeroporto;
	newAeroporto.Coord.x = 50;
	newAeroporto.Coord.y = 50;
	CopyMemory(newAeroporto.Name, TEXT("a1"), 3 * sizeof(TCHAR));
	AddAeroporto(&dados, &newAeroporto);
	newAeroporto.Coord.x = 100;
	newAeroporto.Coord.y = 100;
	CopyMemory(newAeroporto.Name, TEXT("a2"), 3 * sizeof(TCHAR));
	AddAeroporto(&dados, &newAeroporto);
#endif // DEBUG
	

	hThread = CreateThread(NULL, 0, ThreadConsumidor, &dados, 0, NULL);
	if (hThread == NULL)
		error(ERR_CREATE_THREAD, EXIT_FAILURE);

	hHBCThread = CreateThread(NULL, 0, ThreadHBChecker, &dados, 0, NULL);
	if (hHBCThread == NULL)
		error(ERR_CREATE_THREAD, EXIT_FAILURE);

	hNPThread = CreateThread(NULL, 0, ThreadNewPassag, &dados, 0, NULL);
	if (hNPThread == NULL)
		error(ERR_CREATE_THREAD, EXIT_FAILURE);

	while (!dados.terminar)
		PrintMenu(&dados);

	WaitForSingleObject(hThread, INFINITE);
	//WaitForSingleObject(hHBCThread, INFINITE);
	free(dados.Avioes); free(dados.Aeroportos);
	return EXIT_SUCCESS;
}
