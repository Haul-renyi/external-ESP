#include <iostream>
#include <stdio.h>
#include <math.h>
#include <tchar.h>
#include <Windows.h>
#include <tlhelp32.h>
#include<windows.h>


#define TOP_HEIGHT 25

/**
  ========================================================================
  Copyright(c) 2020-2021 ****
  作者：Hual
  版本：V1.0.0
  引用：从网上找到的源码，进行理解所写 ，对世界坐标转屏幕坐标进行了优化 原用的是GDI绘制的esp 
  ========================================================================
  文件名：main.cpp
  功能描述：用GDI+绘制esp
  ========================================================================
**/

//窗口变量
HWND  g_hWnd_Overlay;//创造的窗口句柄
HANDLE g_hProcess;//进程句柄
RECT  g_winRect = { 0 };//用来储存游戏窗口的宽和高


struct pData
{
	DWORD BaseEntity;//基址
	float Position[3];//坐标
	int HP;//血量
	int Team;//阵营
	char DrawHP[32];//用于储存画血量的值
	char Drawdistance[32];//用于储存画距离的值
};



//更新游戏只需要改这些
const DWORD MY_Base = 0xD2FB84;//本人基址
const DWORD EL_Base = 0x4D43AB4;//敌人基址
const DWORD WordMatrix_Base = 0x4D353F4;//矩阵基址


const DWORD HP_offset = 0x100;
const DWORD TeamFlag_offset = 0xF4;
const DWORD Player_offset = 0x10;
const DWORD Pos_offset = 0x138;



DWORD _clientBase;//客户端模块基址
float g_Matrix[4][4];//矩阵

//***创建窗口声明***/

//创建的窗口消息回调函数
LRESULT CALLBACK WindowProc_Overlay(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

/*
*函数说明：创造一个窗口
*参数：无
*返回值：无
*/
void CreateOverlayWindow();

/*
*函数说明：判断游戏是不是置顶窗口
*参数：无
*返回值：是置顶窗口 返回TRUE  不是置顶窗口 返回FALSE
*/
BOOL GameIsForegroundWindow();

/*
*函数说明：初始化MFC中的GUI
*参数：无
*返回值：无
*/
void Clear();

/*
*函数说明：取模块地址
*参数：LPSTRModuleName 模块名 Pid 进程ID
*返回值：成功返回模块句柄  失败返回NULL
*/
DWORD GetModuleBaseAddress(const TCHAR* LPSTRModuleName, DWORD __DwordProcessId);


/***Esp实现声明***/

/*
*函数说明：读取游戏数据
*参数：index 序号 MY 本人 EL 敌人
*返回值：无
*/
void ReadData(int index, pData* MY, pData* EL);

/*
*函数说明：世界坐标转屏幕坐标
*参数：
*返回值：成功返回TURE 失败返回FALSE
*/
BOOL WordToScreen(float from[3], float to[3]);

/*
*函数说明：获取距离
*参数：MyPos 本人位置  ObjPos 敌人位置
*返回值：返回离敌人多远
*/
float GetDistance3D(float MyPos[3], float ObjPos[3]);

/*
*函数说明：透视功能的主体
*参数：MY 本人 EL 敌人  都是前面定义的结构体
*返回值：无
*/
void Esp(pData* MY, pData* EL);


/*
*函数说明：绘制透视功能
*参数：
*返回值：无
*/
void DrawEsp(float Enemy_x, float Enemy_y, float Enemy_y2, float distance, pData* MY, pData* EL);

int  main(int argc, char* argv[])
{
	//游戏窗口句柄
	HWND hWnd = ::FindWindow(NULL, _T("Counter-Strike: Global Offensive"));
	if (hWnd == NULL)
		return -1;

	//获取进程ID
	DWORD dwPid;
	GetWindowThreadProcessId(hWnd, &dwPid);
	//进程句柄
	g_hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPid);
	//取client 模块地址
	_clientBase = GetModuleBaseAddress(_T("client_panorama.dll"), dwPid);

	//获取窗口大小
	GetWindowRect(hWnd, &g_winRect);
	g_winRect.top += TOP_HEIGHT;
	//创造窗口
	CreateOverlayWindow();

	//定义一个MSG结构体
	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));//结构体清零

	pData MY = { 0 };//本人
	pData EL = { 0 };//敌方


	while (msg.message != WM_QUIT)//当消息不为退出的消息时
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))//该函数为一个消息检查线程消息队列，并将该消息（如果存在）放于指定的结构
		{
			TranslateMessage(&msg);//用于将虚拟键消息转换为字符消息
			DispatchMessage(&msg);//该函数分发一个消息给窗口程序。通常消息从GetMessage函数获得或者TranslateMessage函数传递的。
													//消息被分发到回调函数（过程函数)，作用是消息传递给操作系统，然后操作系统去调用我们的回调函数，也就是说我们在窗体的过程函数中处理消息。
		}
		if (GameIsForegroundWindow() == TRUE)//设置创建的窗口为置顶窗口
		{
			SetWindowPos(g_hWnd_Overlay, HWND_TOPMOST, g_winRect.left, g_winRect.top, (g_winRect.right - g_winRect.left), (g_winRect.bottom - g_winRect.top), SWP_SHOWWINDOW);
		}
		else
		{
			SetWindowPos(g_hWnd_Overlay, HWND_BOTTOM, g_winRect.left, g_winRect.top, (g_winRect.right - g_winRect.left), (g_winRect.bottom - g_winRect.top), SWP_SHOWWINDOW);
		}

		//重新获取游戏窗口大小 设置窗口的位置和大小
		GetWindowRect(hWnd, &g_winRect);
		g_winRect.top += TOP_HEIGHT;
		MoveWindow(g_hWnd_Overlay, g_winRect.left, g_winRect.top, (g_winRect.right - g_winRect.left), (g_winRect.bottom - g_winRect.top), TRUE);

		//初始化GUI  画刷等 准备绘图
		Clear();

		//开始绘制ESP
		for (int i = 0; i < 32; i++)
		{
			ReadData(i, &MY, &EL);

			if (EL.HP <= 0)//死亡跳出
				continue;
			if (EL.BaseEntity == 0)//没人跳出
				continue;
			if (MY.Team == EL.Team)//是队友跳出
				continue;

			Esp(&MY, &EL);
		}



	}

}


void Esp(pData* MY, pData* EL)
{
	float EnemyXY[3];
	WordToScreen(EL->Position, EnemyXY);


	float Distance = GetDistance3D(MY->Position, EL->Position);


	DrawEsp(EnemyXY[0], EnemyXY[1], EnemyXY[2], Distance, MY, EL);
}

void DrawEsp(float Enemy_x, float Enemy_y, float Enemy_y2, float distance, pData* MY, pData* EL)
{
	//Enemy_y -= 10;//微调

	float Rect_h = Enemy_y - Enemy_y2;//方框高度
	float Rect_w = Rect_h * 0.526515151552;//方框宽度


	float Rect_x = (Enemy_x - (Rect_w / 2));//相机X- 方框宽度 / 2
	float Rect_y = Enemy_y2;//相机Y2

	float Line_x = (g_winRect.right - g_winRect.left) / 2;
	float Line_y = g_winRect.bottom - g_winRect.top;
	//--------------------------
	HWND hWnd = GetForegroundWindow();//获取当前窗口
	if (hWnd == ::FindWindow(NULL, _T("Counter-Strike: Global Offensive")))
	{
		HDC hDc = GetDC(g_hWnd_Overlay);//!!!!!!!!!!!!!!!!!!!此处为overlay窗口句柄
		HPEN hPen = CreatePen(PS_SOLID, 2, 0x0000FF);//画笔一旦创建后就无法修改
		HBRUSH hBrush = (HBRUSH)GetStockObject(NULL_BRUSH);//空画刷
		SelectObject(hDc, hPen);
		SelectObject(hDc, hBrush);

		Rectangle(hDc, Rect_x, Rect_y, Rect_x + Rect_w, Rect_y + Rect_h);
		MoveToEx(hDc, Line_x, Line_y, NULL);
		LineTo(hDc, Enemy_x, Enemy_y);

		SetTextColor(hDc, RGB(0, 0, 255));
		SetBkMode(hDc, NULL_BRUSH);     //设置字体
		TextOutA(hDc, Rect_x, Rect_y - 20, EL->DrawHP, strlen(EL->DrawHP));
		SetTextColor(hDc, RGB(240, 230, 140));
		SetBkMode(hDc, NULL_BRUSH);     //设置字体
		sprintf_s(EL->Drawdistance, "距离 [ %.0f ]", distance / 10.f);
		TextOutA(hDc, Rect_x, Rect_y - 40, EL->Drawdistance, strlen(EL->Drawdistance));

		DeleteObject(hBrush);
		DeleteObject(hPen);

		ReleaseDC(g_hWnd_Overlay, hDc);//!!!!!!!!!!!!!!!!!!!此处为overlay窗口句柄
	}


}


void ReadData(int index, pData* MY, pData* EL)
{
	//ReadProcessMemory 根据进程句柄读入该进程的某个内存空间
	ReadProcessMemory(g_hProcess, (PBYTE*)(_clientBase + MY_Base), &MY->BaseEntity, sizeof(DWORD), 0);//读本人基址
	ReadProcessMemory(g_hProcess, (PBYTE*)(MY->BaseEntity + Pos_offset), &MY->Position, sizeof(float[3]), 0);//读本人坐标
	ReadProcessMemory(g_hProcess, (PBYTE*)(MY->BaseEntity + TeamFlag_offset), &MY->Team, sizeof(int), 0);//读本人阵营
	ReadProcessMemory(g_hProcess, (PBYTE*)(_clientBase + EL_Base + (index * Player_offset)), &EL->BaseEntity, sizeof(DWORD), 0);//读敌人基址
	ReadProcessMemory(g_hProcess, (PBYTE*)(EL->BaseEntity + HP_offset), &EL->HP, sizeof(int), 0);//读敌人血量
	sprintf_s(EL->DrawHP, "HP [ %d ]", EL->HP);
	ReadProcessMemory(g_hProcess, (PBYTE*)(EL->BaseEntity + Pos_offset), &EL->Position, sizeof(float[3]), 0);//读敌人坐标
	ReadProcessMemory(g_hProcess, (PBYTE*)(EL->BaseEntity + TeamFlag_offset), &EL->Team, sizeof(int), 0);//读敌人阵营
	ReadProcessMemory(g_hProcess, (PBYTE*)(_clientBase + WordMatrix_Base), &g_Matrix, sizeof(g_Matrix), 0);//读矩阵基址
}



float GetDistance3D(float MyPos[3], float ObjPos[3])
{
	return sqrt
	(
		pow(double(ObjPos[0] - MyPos[0]), 2.0) +
		pow(double(ObjPos[1] - MyPos[1]), 2.0) +
		pow(double(ObjPos[2] - MyPos[2]), 2.0)
	);
}



BOOL WordToScreen(float from[3], float to[3])
{
	/***竖矩阵算法***/
	float w = g_Matrix[3][0] * from[0] + g_Matrix[3][1] * from[1] + g_Matrix[3][2] * from[2] + g_Matrix[3][3];//相机Z

	float ScreenWidth = (g_winRect.right - g_winRect.left);
	float ScreenHeight = (g_winRect.bottom - g_winRect.top);

	if (w > 0.01)
	{
		float inverseWidth = 1 / w;//缩放比例
		to[0] = (float)((ScreenWidth / 2) + (0.5 * ((g_Matrix[0][0] * from[0] + g_Matrix[0][1] * from[1] + g_Matrix[0][2] * from[2] + g_Matrix[0][3]) * inverseWidth) * ScreenWidth + 0.5));
		to[1] = (float)((ScreenHeight / 2) - (0.5 * ((g_Matrix[1][0] * from[0] + g_Matrix[1][1] * from[1] + g_Matrix[1][2] * (from[2] - 8) + g_Matrix[1][3]) * inverseWidth) * ScreenHeight));
		to[2] = (float)((ScreenHeight / 2) - (0.5 * ((g_Matrix[1][0] * from[0] + g_Matrix[1][1] * from[1] + g_Matrix[1][2] * (from[2] + 78) + g_Matrix[1][3]) * inverseWidth) * ScreenHeight));
		return true;
	}

	return false;
}

//取模块API
DWORD GetModuleBaseAddress(const TCHAR* LPSTRModuleName, DWORD __DwordProcessId)
{
	MODULEENTRY32 lpModuleEntry = { 0 };
	HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, __DwordProcessId);
	if (!hSnapShot)
		return NULL;
	lpModuleEntry.dwSize = sizeof(lpModuleEntry);
	BOOL __RunModule = Module32First(hSnapShot, &lpModuleEntry);
	while (__RunModule)
	{
		if (!_tcscmp(lpModuleEntry.szModule, LPSTRModuleName))
		{
			CloseHandle(hSnapShot);
			return (DWORD)lpModuleEntry.modBaseAddr;
		}
		__RunModule = Module32Next(hSnapShot, &lpModuleEntry);
	}
	CloseHandle(hSnapShot);
	return NULL;
}














//窗口API

void CreateOverlayWindow()
{
	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof(WNDCLASSEX)); //将wc结构体清0
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;                                                                                 //窗口风格： 如果移动或大小调整更改了工作区的宽度和高度，则重新绘制整个窗口
	wc.lpfnWndProc = WindowProc_Overlay;                                                                                               //窗口回调函数
	wc.hInstance = GetModuleHandle(0);                                                                                        //窗口实例句柄
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);                                                                         //系统标准箭头
	wc.hbrBackground = (HBRUSH)RGB(0, 0, 0);																				//使用空画刷 窗口背景为透明
	wc.lpszClassName = _T("Overlay");                                                                                             //窗口类名
	//注册窗口
	RegisterClassEx(&wc);

	//创建窗口
	g_hWnd_Overlay = CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT, wc.lpszClassName, _T("OverlayWindow"), WS_POPUP, g_winRect.left, g_winRect.top,
		(g_winRect.right - g_winRect.left), (g_winRect.bottom - g_winRect.top), NULL, NULL, wc.hInstance, NULL);

	//设置透明度
	SetLayeredWindowAttributes(g_hWnd_Overlay, RGB(0, 0, 0), 0, ULW_COLORKEY);
	//显示窗口
	ShowWindow(g_hWnd_Overlay, SW_SHOW);
	//更新窗口
	UpdateWindow(g_hWnd_Overlay);
}

LRESULT CALLBACK WindowProc_Overlay(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{

	case WM_PAINT:
		break;
	case WM_CREATE:
		//DwmExtendFrameIntoClientArea(hWnd, &margin);
		break;
	case WM_DESTROY://窗口销毁后
		PostQuitMessage(0);//该函数向系统表明有个线程有终止请求。通常用来响应WM_DESTROY消息
		break;
	case WM_CLOSE://用户点击关闭
		DestroyWindow(g_hWnd_Overlay);//销毁创建的窗口
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);//调用缺省的窗口过程来为应用程序没有处理的任何窗口消息提供缺省的处理
		break;
	}
	return 0;
}

void Clear()
{
	RECT rect = { 0,0,g_winRect.right - g_winRect.left,g_winRect.bottom - g_winRect.top };
	HWND hWnd = GetForegroundWindow();//获取当前窗口
	if (hWnd == FindWindow(NULL, _T("Counter-Strike: Global Offensive")))
	{
		HDC hDc = GetDC(g_hWnd_Overlay);//!!!!!!!!!!!!!!!!!!!此处为overlay窗口句柄
		HBRUSH hBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);//空画刷
		SelectObject(hDc, hBrush);

		FillRect(hDc, &rect, hBrush);

		DeleteObject(hBrush);
		ReleaseDC(g_hWnd_Overlay, hDc);//!!!!!!!!!!!!!!!!!!!此处为overlay窗口句柄
	}
}



BOOL GameIsForegroundWindow()
{
	HWND hWnd = GetForegroundWindow();//取一个前台窗口的句柄（用户当前工作的窗口）。
	if (hWnd == FindWindow(NULL, _T("Counter-Strike: Global Offensive")))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}

}