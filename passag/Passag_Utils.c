#include "Passag_Utils.h"

void error(const TCHAR* msg, int exit_code) {
	_tprintf(TEXT("[FATAL] %s"), msg);
	exit(exit_code);
}

void init(Data* dados) {
	TCHAR buffer[MAX_BUFFER];
	RequestCP req;
	ResponseCP res;

	_tprintf(TEXT("Waiting for Control pipe\n"));
	if (!WaitNamedPipe(NamedPipe_NAME, NMPWAIT_WAIT_FOREVER))
		error(ERR_WAITING_PIPE, EXIT_FAILURE);

	_tprintf(TEXT("Connecting to Control pipe\n"));
	dados->hPipe = CreateFile(NamedPipe_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (dados->hPipe == NULL)
		error(ERR_CONNECT_PIPE, EXIT_FAILURE);

	_tprintf(TEXT("Connected to Control pipe.\n"));

	dados->me.PId = GetCurrentProcessId();
	dados->terminar = 0;


	_stprintf_s(buffer, MAX_BUFFER, Event_CP_PATTERN, dados->me.PId);
	dados->hEvent = CreateEvent(NULL, FALSE, FALSE, buffer);
	if (dados->hEvent == NULL)
		error(ERR_CREATE_EVENT, EXIT_FAILURE);

	_tprintf(TEXT("Nome: "));
	_fgetts(dados->me.Name, 30, stdin);
	dados->me.Name[_tcslen(dados->me.Name) - 1] = '\0';

	req.Type = REQ_INIT;
	CopyMemory(&req.Originator, &dados->me, sizeof(PassagOriginator));

	if (!WriteFile(dados->hPipe, &req, sizeof(RequestCP), NULL, NULL))
		error(ERR_WRITE_PIPE, EXIT_FAILURE);

	if (!ReadFile(dados->hPipe, &res, sizeof(ResponseCP), NULL, NULL))
		error(ERR_READ_PIPE, EXIT_FAILURE);

	req.Type = REQ_AIRPORT;

	_tprintf(TEXT("Aeroporto de origem: "));
	do
	{
		_fgetts(req.buffer, MAX_BUFFER, stdin);
		req.buffer[_tcslen(req.buffer) - 1] = '\0';

		if (!WriteFile(dados->hPipe, &req, sizeof(RequestCP), NULL, NULL))
			error(ERR_WRITE_PIPE, EXIT_FAILURE);

		if (!ReadFile(dados->hPipe, &res, sizeof(ResponseCP), NULL, NULL))
			error(ERR_READ_PIPE, EXIT_FAILURE);
		if (res.Type == RES_AIRPORT_NOTFOUND)
			_tprintf(TEXT("Aeroporto Invalido!\n"));
	} while (res.Type == RES_AIRPORT_NOTFOUND);

	dados->me.Coord.x = res.Coord.x;
	dados->me.Coord.y = res.Coord.y;

	_tprintf(TEXT("Aeroporto de Destino: "));
	do
	{
		_fgetts(req.buffer, MAX_BUFFER, stdin);
		req.buffer[_tcslen(req.buffer) - 1] = '\0';

		if (!WriteFile(dados->hPipe, &req, sizeof(RequestCP), NULL, NULL))
			error(ERR_WRITE_PIPE, EXIT_FAILURE);

		if (!ReadFile(dados->hPipe, &res, sizeof(ResponseCP), NULL, NULL))
			error(ERR_READ_PIPE, EXIT_FAILURE);
		if (res.Type == RES_AIRPORT_NOTFOUND)
			_tprintf(TEXT("Aeroporto Invalido!\n"));
		else if (res.Coord.x == dados->me.Coord.x && res.Coord.y == dados->me.Coord.y)
			_tprintf(TEXT("Escolha outro Aeroporto!\n"));
	} while (res.Type == RES_AIRPORT_NOTFOUND || (res.Coord.x == dados->me.Coord.x && res.Coord.y == dados->me.Coord.y));

	dados->me.Dest.x = res.Coord.x;
	dados->me.Dest.y = res.Coord.y;

	req.Type = REQ_UPDATE;
	CopyMemory(&req.Originator, &dados->me, sizeof(PassagOriginator));
	if (!WriteFile(dados->hPipe, &req, sizeof(RequestCP), NULL, NULL))
		error(ERR_WRITE_PIPE, EXIT_FAILURE);
}

void PrintInfo(Data* dados) {
	_tprintf(TEXT("Current Location: %d, %d\n"), dados->me.Coord.x, dados->me.Coord.y);
	_tprintf(TEXT("Destination: %d, %d\n"), dados->me.Dest.x, dados->me.Dest.y);
}