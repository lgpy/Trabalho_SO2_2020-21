#include "Control_Utils.h"

LRESULT CALLBACK TrataEventos(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK TrataEventosCriarAeroporto(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK TrataEventosListagem(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow)
{
	Dados dados;

	init_dados(&dados);

#ifdef DEBUG
	Aeroporto newAeroporto;
	newAeroporto.Coord.x = 0;
	newAeroporto.Coord.y = 0;
	CopyMemory(newAeroporto.Name, TEXT("a1"), 3 * sizeof(TCHAR));
	AddAeroporto(&dados, &newAeroporto);
	newAeroporto.Coord.x = 100;
	newAeroporto.Coord.y = 100;
	CopyMemory(newAeroporto.Name, TEXT("a2"), 3 * sizeof(TCHAR));
	AddAeroporto(&dados, &newAeroporto);
#endif // DEBUG

	dados.hThread = CreateThread(NULL, 0, ThreadConsumidor, &dados, 0, NULL);
	if (dados.hThread == NULL)
		error(&dados, ERR_CREATE_THREAD, EXIT_FAILURE);

	dados.hHBCThread = CreateThread(NULL, 0, ThreadHBChecker, &dados, 0, NULL);
	if (dados.hHBCThread == NULL)
		error(&dados, ERR_CREATE_THREAD, EXIT_FAILURE);

	dados.hNPThread = CreateThread(NULL, 0, ThreadNewPassag, &dados, 0, NULL);
	if (dados.hNPThread == NULL)
		error(&dados, ERR_CREATE_THREAD, EXIT_FAILURE);

	HWND hWnd;
	MSG lpMsg;
	WNDCLASSEX wcApp;

	wcApp.cbSize = sizeof(WNDCLASSEX);
	wcApp.hInstance = hInst;

	TCHAR szProgName[] = TEXT("Control");

	wcApp.lpszClassName = szProgName;
	wcApp.lpfnWndProc = TrataEventos;

	wcApp.style = CS_HREDRAW | CS_VREDRAW;

	wcApp.hIcon = LoadIcon(NULL, IDI_ASTERISK);
	wcApp.hIconSm = LoadIcon(NULL, IDI_SHIELD);
	wcApp.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcApp.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);

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

	//UpdateWindow(hWnd);

	while (GetMessage(&lpMsg, NULL, 0, 0))
	{
		TranslateMessage(&lpMsg);
		DispatchMessage(&lpMsg);
	}

	return((int)lpMsg.wParam);
}

LRESULT CALLBACK TrataEventos(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam)
{
	HMENU hMenu;
	MENUITEMINFO item;

	HDC hdc;
	PAINTSTRUCT ps;
	RECT rect; RECT rectHover;
	static TRACKMOUSEEVENT tme;

	Dados* dados;

	int i;
	int xPos, yPos;

	static HOVEREVENT hoverE = {FALSE};

	static TCHAR clickbuffer[100];

	Response res;

	dados = (Dados*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (messg)
	{
	case WM_CREATE:
		break;

	case WM_MOUSEMOVE:
		if (hoverE.isActive)
		{
			ShowCursor(TRUE);
			hoverE.isActive = FALSE;
			InvalidateRect(dados->gui.hWnd, NULL, FALSE);
		}
		tme.cbSize = sizeof(TRACKMOUSEEVENT);
		tme.dwFlags = TME_HOVER;
		tme.hwndTrack = hWnd;
		TrackMouseEvent(&tme);
	break;

	case WM_MOUSEHOVER:
		xPos = GET_X_LPARAM(lParam);
		yPos = GET_Y_LPARAM(lParam);
		
		if (HoverEvent(dados, xPos, yPos, hoverE.buffer))
		{
			hoverE.isActive = TRUE;
			hoverE.coords.x = xPos;
			hoverE.coords.y = yPos;
			InvalidateRect(dados->gui.hWnd, NULL, FALSE);
			ShowCursor(FALSE);
		}
		break;


	case WM_LBUTTONDOWN:
		xPos = GET_X_LPARAM(lParam);
		yPos = GET_Y_LPARAM(lParam);

		if (ClickEvent(dados, xPos, yPos, clickbuffer))
			MessageBox(hWnd, clickbuffer, TEXT("Informação"), MB_OK | MB_ICONINFORMATION);
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

		if (hoverE.isActive)
		{
			GetClientRect(hWnd, &rectHover);
			rectHover.left = hoverE.coords.x;
			rectHover.top = hoverE.coords.y;
			DrawText(dados->gui.memDC, hoverE.buffer, _tcslen(hoverE.buffer), &rectHover, DT_NOCLIP);
		}

		BitBlt(hdc, rect.left, rect.top, rect.right, rect.bottom, dados->gui.memDC, 0, 0, SRCCOPY);

		EndPaint(hWnd, &ps);
		break;
	case WM_ERASEBKGND:
		return TRUE;

	case WM_CLOSE:
		if (MessageBox(hWnd, TEXT("Are you sure?"), TEXT("Close"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
			dados->terminar = TRUE;
			WaitForSingleObject(dados->hMutexPassageiros, INFINITE);
			for (i = 0; i < dados->nPassageiros; i++)
			{
				res.rType = RES_TERMINATED;
				WriteFile(dados->Passageiros[i].hPipe, &res, sizeof(ResponseCP), NULL, NULL);
			}
			WaitForSingleObject(dados->hMutexAvioes, INFINITE);
			for (i = 0; i < dados->nAvioes; i++) {
				WaitForSingleObject(dados->Avioes[i].hMutexMemPar, INFINITE);
				dados->Avioes[i].memPar->rType = RES_CONTROL_SHUTDOWN;
				ReleaseMutex(dados->Avioes[i].hMutexMemPar);
				SetEvent(dados->Avioes[i].hEvent);
			}
			DestroyWindow(hWnd);
		}
		break;

	case WM_COMMAND:

		switch (LOWORD(wParam))
		{
		case ID_CRIARAEROPORTO:
			DialogBox(NULL, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, TrataEventosCriarAeroporto);
			break;
		case ID_SUSPENDERAVIOES:
			hMenu = GetMenu(hWnd);
			item.cbSize = sizeof(MENUITEMINFO);
			item.fMask = MIIM_TYPE;
			item.dwTypeData = NULL;
			GetMenuItemInfo(hMenu, ID_SUSPENDERAVIOES, FALSE, &item);
			if (dados->aceitarAvioes) {
				dados->aceitarAvioes = FALSE;
				item.dwTypeData = TEXT("Ativar aceitação de aviões");
			}
			else {
				dados->aceitarAvioes = TRUE;
				item.dwTypeData = TEXT("Suspender aceitação de aviões");
			}
			SetMenuItemInfo(hMenu, ID_SUSPENDERAVIOES, FALSE, &item);
			DrawMenuBar(hWnd);
			break;
		case ID_LISTAGEM:
			DialogBox(NULL, MAKEINTRESOURCE(IDD_DIALOG2), hWnd, TrataEventosListagem);
			break;
		}
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

LRESULT CALLBACK TrataEventosCriarAeroporto(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam)
{
	Dados* dados;
	TCHAR buffer[MAX_BUFFER];
	Aeroporto novoAeroporto;
	dados = (Dados*)GetWindowLongPtr((HWND) GetWindowLongPtr(hWnd, GWLP_HWNDPARENT), GWLP_USERDATA);

	switch (messg)
	{
	case WM_COMMAND:

		if (LOWORD(wParam) == IDOK)
		{
			GetDlgItemText(hWnd, IDC_EDIT_NOME, novoAeroporto.Name, MAX_BUFFER);
			GetDlgItemText(hWnd, IDC_EDIT_X, buffer, 4);
			novoAeroporto.Coord.x = _tstoi(buffer)-1;
			GetDlgItemText(hWnd, IDC_EDIT_Y, buffer, 4);
			novoAeroporto.Coord.y = _tstoi(buffer)-1;
			
			if (AddAeroporto(dados, &novoAeroporto) > -1) {
				MessageBox(hWnd, TEXT("Aeroporto adicionado!"), TEXT("Criar novo Aeroporto"), MB_OK | MB_ICONINFORMATION);
				EndDialog(hWnd, 0);
			}
			else {
				MessageBox(hWnd, TEXT("Aeroporto invalido!"), TEXT("Criar novo Aeroporto"), MB_OK | MB_ICONERROR);
				SetDlgItemText(hWnd, IDC_EDIT_NOME, NULL);
				SetDlgItemText(hWnd, IDC_EDIT_X, NULL);
				SetDlgItemText(hWnd, IDC_EDIT_Y, NULL);
			}
		}
		else if (LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hWnd, 0);
			return TRUE;
		}

		break;

	case WM_CLOSE:

		EndDialog(hWnd, 0);
		return TRUE;
	}

	return FALSE;
}

LRESULT CALLBACK TrataEventosListagem(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam)
{

	Aviao* aviao = NULL;
	Aeroporto* aeroporto = NULL;
	Passageiro* passageiro = NULL;

	HWND hList;
	static HWND info;

	Dados* dados;
	TCHAR buffer[200];
	int i;
	dados = (Dados*)GetWindowLongPtr((HWND) GetWindowLongPtr(hWnd, GWLP_HWNDPARENT), GWLP_USERDATA);

	switch (messg)
	{
	case WM_INITDIALOG: //TODO disable refresh button
		info = GetDlgItem(hWnd, IDC_INFO);
		refresh(dados, hWnd);
		SendMessage(info, WM_SETTEXT, 0, (LPARAM) NULL);
		break;

	case WM_COMMAND:

		switch (LOWORD(wParam))
		{
		case IDC_BUTTON_REFRESH:
			refresh(dados, hWnd);
			SendMessage(info, WM_SETTEXT, 0, (LPARAM) NULL);
			break;
		case IDC_LIST_Aeroportos:
			if (HIWORD(wParam) == LBN_SELCHANGE)
			{
				hList = GetDlgItem(hWnd, IDC_LIST_Aeroportos);
				i = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
				SendMessage(hList, LB_GETTEXT, (WPARAM)i, (LPARAM)buffer);
				if (_tcscmp(buffer, TEXT("")) != 0) {
					i = FindAeroportobyName(dados, buffer);
					if (i == -1)
						aeroporto = NULL;
					else {
						aeroporto = &dados->Aeroportos[i];
						AeroportoToString(aeroporto, buffer, 200);
						SendMessage(info, WM_SETTEXT, (WPARAM) 0, (LPARAM) buffer);
					}
				}
				else
					SendMessage(info, WM_SETTEXT, 0, (LPARAM) NULL);

				hList = GetDlgItem(hWnd, IDC_LIST_AVIOES);
				fillAvioes(dados, hList, aeroporto);

				hList = GetDlgItem(hWnd, IDC_LIST_PASSAGEIROS);
				fillPassageiros(dados, hList, aeroporto, NULL);
			}
			break;
		case IDC_LIST_AVIOES:
			if (HIWORD(wParam) == LBN_SELCHANGE)
			{
				hList = GetDlgItem(hWnd, IDC_LIST_Aeroportos);
				i = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
				SendMessage(hList, LB_GETTEXT, (WPARAM)i, (LPARAM)buffer);
				i = FindAeroportobyName(dados, buffer);
				if (_tcscmp(buffer, TEXT("")) != 0) {
					i = FindAeroportobyName(dados, buffer);
					if (i == -1)
						aeroporto = NULL;
					else
						aeroporto = &dados->Aeroportos[i];
				}

				hList = GetDlgItem(hWnd, IDC_LIST_AVIOES);
				i = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
				SendMessage(hList, LB_GETTEXT, (WPARAM)i, (LPARAM)buffer);
				if (_tcscmp(buffer, TEXT("")) != 0) {
					WaitForSingleObject(dados->hMutexAvioes, INFINITE);
					i = FindAviaobyPId(dados, _tstol(buffer));
					if (i == -1)
						aviao = NULL;
					else {
						aviao = &dados->Avioes[i];
						AviaoToString(aviao, buffer, 200);
						SendMessage(info, WM_SETTEXT, 0, (LPARAM)buffer);
					}
				}
				else
					SendMessage(info, WM_SETTEXT, 0, (LPARAM) NULL);

				hList = GetDlgItem(hWnd, IDC_LIST_PASSAGEIROS);
				fillPassageiros(dados, hList, aeroporto, aviao);
				ReleaseMutex(dados->hMutexAvioes);
			}
			break;
		case IDC_LIST_PASSAGEIROS:
			if (HIWORD(wParam) == LBN_SELCHANGE)
			{
				hList = GetDlgItem(hWnd, IDC_LIST_PASSAGEIROS);
				i = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
				SendMessage(hList, LB_GETTEXT, (WPARAM)i, (LPARAM)buffer);
				WaitForSingleObject(dados->hMutexPassageiros, INFINITE);
				i = FindPassageirobyPId(dados, _tstol(buffer));
				if (i != -1) {
					passageiro = &dados->Passageiros[i];
					PassageiroToString(passageiro, buffer, 200);
					SendMessage(info, WM_SETTEXT, 0, (LPARAM)buffer);
				}
				ReleaseMutex(dados->hMutexPassageiros);
			}
			break;
		default:
			break;
		}

		break;

	case WM_CLOSE:

		EndDialog(hWnd, 0);
		return TRUE;
	}

	return FALSE;
}