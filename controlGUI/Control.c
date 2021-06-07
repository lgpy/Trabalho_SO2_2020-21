#include "Control_Utils.h"

TCHAR szProgName[] = TEXT("Ficha8");
LRESULT CALLBACK TrataEventos(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow)
{
	HANDLE hFileMap, hThread, hHBCThread, hNPThread; // change names
	Dados dados;

	init_dados(&dados, &hFileMap);

#ifdef DEBUG
	Aeroporto newAeroporto;
	newAeroporto.Coord.x = 0;
	newAeroporto.Coord.y = 0;
	CopyMemory(newAeroporto.Name, TEXT("a1"), 3 * sizeof(TCHAR));
	AddAeroporto(&dados, &newAeroporto);
	newAeroporto.Coord.x = 999;
	newAeroporto.Coord.y = 999;
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

	HWND hWnd;
	MSG lpMsg;
	WNDCLASSEX wcApp;
	HANDLE hAccel;

	wcApp.cbSize = sizeof(WNDCLASSEX);
	wcApp.hInstance = hInst;

	wcApp.lpszClassName = szProgName;
	wcApp.lpfnWndProc = TrataEventos;

	wcApp.style = CS_HREDRAW | CS_VREDRAW;

	wcApp.hIcon = LoadIcon(NULL, IDI_ASTERISK);
	wcApp.hIconSm = LoadIcon(NULL, IDI_SHIELD);
	wcApp.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcApp.lpszMenuName = NULL;

	wcApp.cbClsExtra = sizeof(Dados);
	wcApp.cbWndExtra = 0;
	wcApp.hbrBackground = CreateSolidBrush(RGB(255, 255, 255));

	if (!RegisterClassEx(&wcApp))
		return(0);

	hWnd = CreateWindow(
		szProgName,
		TEXT("Control"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		600,
		600,
		(HWND)HWND_DESKTOP,
		(HMENU)NULL,
		(HINSTANCE)hInst,
		0);

	LONG_PTR x = SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)&dados);

	HDC hdc;
	RECT rect;

	dados.gui.hBmpAviao = (HBITMAP)LoadImage(NULL, TEXT("Aviao.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	GetObject(dados.gui.hBmpAviao, sizeof(dados.gui.bmpAviao), &dados.gui.bmpAviao);

	dados.gui.hBmpAeroporto = (HBITMAP)LoadImage(NULL, TEXT("Aeroporto.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	GetObject(dados.gui.hBmpAeroporto, sizeof(dados.gui.bmpAeroporto), &dados.gui.bmpAeroporto);

	hdc = GetDC(hWnd);

	dados.gui.bmpDCAviao = CreateCompatibleDC(hdc);
	SelectObject(dados.gui.bmpDCAviao, dados.gui.hBmpAviao);

	dados.gui.bmpDCAeroporto = CreateCompatibleDC(hdc);
	SelectObject(dados.gui.bmpDCAeroporto, dados.gui.hBmpAeroporto);

	ReleaseDC(hWnd, hdc);
	GetClientRect(hWnd, &rect);
	dados.gui.hWnd = hWnd;
	dados.gui.memDC = NULL;

	ShowWindow(hWnd, nCmdShow);

	UpdateWindow(hWnd);

	while (GetMessage(&lpMsg, NULL, 0, 0))
	{
		TranslateMessage(&lpMsg);
		DispatchMessage(&lpMsg);
	}

	return((int)lpMsg.wParam);
}

LRESULT CALLBACK TrataEventos(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam)
{

	HDC hdc;
	PAINTSTRUCT ps;
	RECT rect; RECT rectHover;
	static TRACKMOUSEEVENT tme;

	Dados* dados;
	Aviao* aviaoTemp;
	Aeroporto* aeroportoTemp;

	int i;
	int xPos, yPos;
	int xPosT;
	int yPosT;

	static BOOL Hover = FALSE;
	static int HoverxPos, HoveryPos;
	static TCHAR buffer[MAX_BUFFER];

	dados = (Dados*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (messg)
	{
	case WM_CREATE:
		break;

	case WM_MOUSEMOVE:
		tme.cbSize = sizeof(TRACKMOUSEEVENT);
		tme.dwFlags = TME_HOVER;
		tme.hwndTrack = hWnd;
		TrackMouseEvent(&tme);
	break;

	case WM_MOUSEHOVER:
		aviaoTemp = NULL;
		xPos = GET_X_LPARAM(lParam);
		yPos = GET_Y_LPARAM(lParam);
		xPosT = 0;
		yPosT = 0;

		WaitForSingleObject(dados->hMutexAvioes, INFINITE);
		for (i = 0; i < dados->nAvioes; i++) {
			xPosT = translateCoord(dados->Avioes[i].Coord.x);
			yPosT = translateCoord(dados->Avioes[i].Coord.y);
			if (xPos >= xPosT && xPos <= xPosT + 14 && yPos >= yPosT && yPos <= yPosT + 14) {
				aviaoTemp = &dados->Avioes[i];
				break;
			}
		}
		ReleaseMutex(dados->hMutexAvioes);

		if (aviaoTemp != NULL)
		{
			_stprintf_s(buffer, MAX_BUFFER, TEXT("%lu"), aviaoTemp->PId);
			Hover = TRUE;
			HoverxPos = xPos;
			HoveryPos = yPos;
		}
		InvalidateRect(dados->gui.hWnd, NULL, FALSE);
		break;


	case WM_LBUTTONDOWN:
		aeroportoTemp = NULL;
		xPos = GET_X_LPARAM(lParam);
		yPos = GET_Y_LPARAM(lParam);
		xPosT = 0;
		yPosT = 0;

		WaitForSingleObject(dados->hMutexAeroportos, INFINITE);
		for (i = 0; i < dados->nAeroportos; i++) {
			xPosT = translateCoord(dados->Aeroportos[i].Coord.x);
			yPosT = translateCoord(dados->Aeroportos[i].Coord.y);
			if (xPos >= xPosT && xPos <= xPosT + 14 && yPos >= yPosT && yPos <= yPosT + 14) {
				aeroportoTemp = &dados->Aeroportos[i];
				break;
			}
		}
		ReleaseMutex(dados->hMutexAeroportos);

		if (aeroportoTemp != NULL)
		{
			MessageBox(hWnd, aeroportoTemp->Name, aeroportoTemp->Name, MB_OK | MB_ICONINFORMATION);
		}
		break;

	case WM_PAINT:
		
		hdc = BeginPaint(hWnd, &ps);
		GetClientRect(hWnd, &rect);

		if (dados->gui.memDC == NULL) {
			dados->gui.memDC = CreateCompatibleDC(hdc);
			dados->gui.hBitmapDB = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
			SelectObject(dados->gui.memDC, dados->gui.hBitmapDB);
			DeleteObject(dados->gui.hBitmapDB);
		}

		FillRect(dados->gui.memDC, &rect, CreateSolidBrush(RGB(255, 255, 255)));

		WaitForSingleObject(dados->hMutexAvioes, INFINITE);
		for (i = 0; i < dados->nAvioes; i++)
			BitBlt(dados->gui.memDC, translateCoord(dados->Avioes[i].Coord.x), translateCoord(dados->Avioes[i].Coord.y), dados->gui.bmpAviao.bmWidth, dados->gui.bmpAviao.bmHeight, dados->gui.bmpDCAviao, 0, 0, SRCCOPY);
		ReleaseMutex(dados->hMutexAvioes);

		WaitForSingleObject(dados->hMutexAeroportos, INFINITE);
		for (i = 0; i < dados->nAeroportos; i++)
			BitBlt(dados->gui.memDC, translateCoord(dados->Aeroportos[i].Coord.x), translateCoord(dados->Aeroportos[i].Coord.y), dados->gui.bmpAeroporto.bmWidth, dados->gui.bmpAeroporto.bmHeight, dados->gui.bmpDCAeroporto, 0, 0, SRCCOPY);
		ReleaseMutex(dados->hMutexAeroportos);

		if (Hover)
		{
			GetClientRect(hWnd, &rectHover);
			rectHover.left = HoverxPos;
			rectHover.top = HoveryPos;
			DrawText(dados->gui.memDC, buffer, _tcslen(buffer), &rectHover, DT_NOCLIP);
			Hover = FALSE;
		}

		BitBlt(hdc, rect.left, rect.top, rect.right, rect.bottom, dados->gui.memDC, 0, 0, SRCCOPY);

		EndPaint(hWnd, &ps);
		break;
	case WM_ERASEBKGND:
		return TRUE;
	case WM_COMMAND:
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, messg, wParam, lParam);
		break;
	}
	return(0);
}

int translateCoord(int coord) {
	return (coord + 1) / 2;
}