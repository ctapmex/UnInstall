#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <tchar.h>
#include <strsafe.h>
#include "plugin.hpp"
#include "memory.h"
#define realloc my_realloc
#ifndef nullptr
#define nullptr NULL
#endif
#ifndef _ASSERTE
#define _ASSERTE(x)
#endif
#include "DlgBuilder.hpp"
#include "eplugin.cpp"
#include "farcolor.hpp"
#include "farkeys.hpp"
#include "farlang.h"
#include "registry.cpp"
#include "uninstall.hpp"

#ifdef FARAPI18
#  define SetStartupInfo SetStartupInfoW
#  define GetPluginInfo GetPluginInfoW
#  define OpenPlugin OpenPluginW
#  define Configure ConfigureW
#endif

#ifdef FARAPI3
#  define SetStartupInfo SetStartupInfoW
#  define GetPluginInfo GetPluginInfoW
#  define Configure ConfigureW
#endif

#ifdef FARAPI17
int WINAPI GetMinFarVersion(void)
{
	return MAKEFARVERSION(1,75,2555);
}
#endif
#ifdef FARAPI18
int WINAPI GetMinFarVersion(void)
{
	return FARMANAGERVERSION;
}
int WINAPI GetMinFarVersionW(void)
{
	return FARMANAGERVERSION;
}
#endif

#ifdef FARAPI3
int WINAPI GetMinFarVersionW(void)
{
  #define MAKEFARVERSION_OLD(major,minor,build) ( ((major)<<8) | (minor) | ((build)<<16))
  
  return MAKEFARVERSION_OLD(FARMANAGERVERSION_MAJOR,FARMANAGERVERSION_MINOR,FARMANAGERVERSION_BUILD);
}

void WINAPI GetGlobalInfoW(struct GlobalInfo *Info)
{
	Info->StructSize = sizeof(GlobalInfo);
	Info->MinFarVersion = FARMANAGERVERSION;

	Info->Version = 	MAKEFARVERSION(1,10,15,0,VS_RELEASE);
	Info->Guid = MainGuid;
	Info->Title = L"UnInstall";
	Info->Description = L"UnInstall";
	Info->Author = L"ConEmu.Maximus5@gmail.com";
}
#endif

void WINAPI SetStartupInfo(const struct PluginStartupInfo *psInfo)
{
	Info = *psInfo;
	FSF = *psInfo->FSF;
	Info.FSF = &FSF;
	InitHeap();
#ifndef FARAPI3
	StringCchCopy(PluginRootKey,ARRAYSIZE(PluginRootKey),Info.RootKey);
	StringCchCat(PluginRootKey,ARRAYSIZE(PluginRootKey),_T("\\UnInstall"));
#endif
	ReadRegistry();
}

void WINAPI GetPluginInfo(struct PluginInfo *Info)
{
	static const TCHAR *PluginMenuStrings[1];
	PluginMenuStrings[0] = GetMsg(MPlugIn);
	Info -> StructSize = sizeof(*Info);

	if(Opt.WhereWork & 2)
		Info -> Flags |= PF_EDITOR;

	if(Opt.WhereWork & 1)
		Info -> Flags |= PF_VIEWER;
#ifdef FARAPI3
	Info ->PluginMenu.Strings = PluginMenuStrings;
	Info ->PluginMenu.Count = ARRAYSIZE(PluginMenuStrings);
	Info ->PluginMenu.Guids = &PluginMenuGuid;
	Info ->PluginConfig.Strings = PluginMenuStrings;
	Info ->PluginConfig.Count = ARRAYSIZE(PluginMenuStrings);
	Info ->PluginConfig.Guids = &PluginConfigGuid;
#else
	Info -> PluginMenuStrings = PluginMenuStrings;
	Info -> PluginConfigStrings = PluginMenuStrings;
	Info -> PluginMenuStringsNumber = ARRAYSIZE(PluginMenuStrings);
	Info -> PluginConfigStringsNumber = ARRAYSIZE(PluginMenuStrings);
#endif
}

void ResizeDialog(HANDLE hDlg)
{
	CONSOLE_SCREEN_BUFFER_INFO con_info;
	GetConsoleScreenBufferInfo(hStdout, &con_info);
	unsigned con_sx = con_info.srWindow.Right - con_info.srWindow.Left + 1;
	int max_items = con_info.srWindow.Bottom - con_info.srWindow.Top + 1 - 7;
	int s = ((ListSize>0) && (ListSize<max_items) ? ListSize : (ListSize>0 ? max_items : 0));
	SMALL_RECT NewPos = { 2, 1, con_sx - 7, s + 2 };
	SMALL_RECT OldPos;
#ifdef FARAPI3
	Info.SendDlgMessage(hDlg,DM_GETITEMPOSITION,LIST_BOX,&OldPos);
#else
	Info.SendDlgMessage(hDlg,DM_GETITEMPOSITION,LIST_BOX,reinterpret_cast<LONG_PTR>(&OldPos));
#endif

	if(NewPos.Right!=OldPos.Right || NewPos.Bottom!=OldPos.Bottom)
	{
		COORD coord;
		coord.X = con_sx - 4;
		coord.Y = s + 4;
#ifdef FARAPI3
		Info.SendDlgMessage(hDlg,DM_RESIZEDIALOG,0,&coord);
#else
		Info.SendDlgMessage(hDlg,DM_RESIZEDIALOG,0,reinterpret_cast<LONG_PTR>(&coord));
#endif
		coord.X = -1;
		coord.Y = -1;
#ifdef FARAPI3
		Info.SendDlgMessage(hDlg,DM_MOVEDIALOG,TRUE,&coord);
		Info.SendDlgMessage(hDlg,DM_SETITEMPOSITION,LIST_BOX,&NewPos);
#else
		Info.SendDlgMessage(hDlg,DM_MOVEDIALOG,TRUE,reinterpret_cast<LONG_PTR>(&coord));
		Info.SendDlgMessage(hDlg,DM_SETITEMPOSITION,LIST_BOX,reinterpret_cast<LONG_PTR>(&NewPos));
#endif
	}
}

#ifdef FARAPI3
static INT_PTR WINAPI DlgProc(HANDLE hDlg,intptr_t Msg,intptr_t Param1,void* Param2)
#else
static LONG_PTR WINAPI DlgProc(HANDLE hDlg,int Msg,int Param1,LONG_PTR Param2)
#endif
{
	static TCHAR Filter[MAX_PATH];
	static TCHAR spFilter[MAX_PATH];
	static FarListTitles ListTitle;

#ifdef FARAPI3
	INPUT_RECORD* record=nullptr;
#endif

	switch(Msg)
	{
		case DN_RESIZECONSOLE:
		{
			Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,FALSE,0);
			ResizeDialog(hDlg);
			Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,TRUE,0);
		}
		return TRUE;
		case DMU_UPDATE:
		{
			int OldPos = static_cast<int>(Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,LIST_BOX,0));

			if(Param1)
				UpDateInfo();

			ListSize = 0;
			int NewPos = -1;

			if(OldPos >= 0 && OldPos < nCount)
			{
				if(!*Filter || strstri(p[OldPos].Keys[DisplayName],Filter))  //без учета регистра в OEM кодировке
					NewPos = OldPos;
			}

			for(int i = 0; i < nCount; i++)
			{
				const TCHAR* DispName = p[i].Keys[DisplayName], *Find;

				if(*Filter)
					Find = strstri(DispName,Filter);
				else
					Find = DispName;

				if(Find != nullptr)  //без учета регистра в OEM кодировке
				{
					FLI[i].Flags &= ~LIF_HIDDEN;

					if(Param2 && (i == OldPos))
					{
						if(FLI[i].Flags & LIF_CHECKED)
						{
							FLI[i].Flags &= ~LIF_CHECKED;
						}
						else
						{
							FLI[i].Flags |= LIF_CHECKED;
						}
					}

					//без учета регистра - а кодировка ANSI
					if(NewPos == -1 && Find == DispName)
						NewPos = i;

					ListSize++;
				}
				else
					FLI[i].Flags |= LIF_HIDDEN;
			}

			if(Param1 == 0 && Param2)
			{
				// Снятие или установка пометки (Ins)
#ifdef FARAPI3
				if(*(int*)Param2 == 1)
#else
				if(Param2 == 1)
#endif
				{
					for(int i = (OldPos+1); i < nCount; i++)
					{
						if(!(FLI[i].Flags & LIF_HIDDEN))
						{
							OldPos = i; break;
						}
					}

					NewPos = OldPos;
				}
				// Снятие или установка пометки (RClick)
#ifdef FARAPI3
				else if(*(int*)Param2  == 2)
#else
				else if(Param2 == 2)
#endif
				{
					NewPos = OldPos;
				}
			}
			else if(NewPos == -1)
			{
				NewPos = OldPos;
			}

			Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,FALSE,0);
#ifdef FARAPI3
			Info.SendDlgMessage(hDlg,DM_LISTSET,LIST_BOX,&FL);
#else
			Info.SendDlgMessage(hDlg,DM_LISTSET,LIST_BOX,reinterpret_cast<LONG_PTR>(&FL));
#endif
			StringCchPrintf(spFilter,ARRAYSIZE(spFilter), GetMsg(MFilter),Filter,ListSize,nCount);
			ListTitle.Title = spFilter;
#ifdef FARAPI3
			ListTitle.TitleSize = lstrlen(spFilter);
			ListTitle.StructSize = sizeof(FarListTitles);
			Info.SendDlgMessage(hDlg,DM_LISTSETTITLES,LIST_BOX,&ListTitle);
#else
			ListTitle.TitleLen = lstrlen(spFilter);
			Info.SendDlgMessage(hDlg,DM_LISTSETTITLES,LIST_BOX,reinterpret_cast<LONG_PTR>(&ListTitle));
#endif
			ResizeDialog(hDlg);
			struct FarListPos FLP;
			FLP.SelectPos = NewPos;
			FLP.TopPos = -1;
#ifdef FARAPI3
			FLP.StructSize = sizeof(FarListPos);
			Info.SendDlgMessage(hDlg,DM_LISTSETCURPOS,LIST_BOX,&FLP);
#else
			Info.SendDlgMessage(hDlg,DM_LISTSETCURPOS,LIST_BOX,reinterpret_cast<LONG_PTR>(&FLP));
#endif
			Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,TRUE,0);
		}
		break;
		case DN_INITDIALOG:
		{
			StringCchCopy(Filter,ARRAYSIZE(Filter),_T(""));
			ListTitle.Bottom = const_cast<TCHAR*>(GetMsg(MBottomLine));
#ifdef FARAPI3
			ListTitle.BottomSize = lstrlen(GetMsg(MBottomLine));
#else
			ListTitle.BottomLen = lstrlen(GetMsg(MBottomLine));
#endif
			//подстраиваемся под размеры консоли
			Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,FALSE,0);
			ResizeDialog(hDlg);
			Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,TRUE,0);
			//заполняем диалог
			Info.SendDlgMessage(hDlg,DMU_UPDATE,1,0);
		}
		break;
#ifdef FARAPI3
		case DN_CONTROLINPUT:
			{
				record=(INPUT_RECORD *)Param2;
				if (record->EventType == MOUSE_EVENT) {
					if (Param1 == LIST_BOX)
					{
						MOUSE_EVENT_RECORD *mer = &record->Event.MouseEvent;
#else
		case DN_MOUSECLICK:
			{
				if(Param1 == LIST_BOX)
				{
					MOUSE_EVENT_RECORD *mer = (MOUSE_EVENT_RECORD *)Param2;
#endif
					if(mer->dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED)
					{
						// find list on-screen coords (excluding frame and border)
						SMALL_RECT list_rect;
#ifdef FARAPI3
						Info.SendDlgMessage(hDlg, DM_GETDLGRECT, 0, &list_rect);
#else
						Info.SendDlgMessage(hDlg, DM_GETDLGRECT, 0, reinterpret_cast<LONG_PTR>(&list_rect));
#endif
						list_rect.Left += 2;
						list_rect.Top += 1;
						list_rect.Right -= 2;
						list_rect.Bottom -= 1;

						if((mer->dwEventFlags == 0) && (mer->dwMousePosition.X > list_rect.Left) && (mer->dwMousePosition.X < list_rect.Right) && (mer->dwMousePosition.Y > list_rect.Top) && (mer->dwMousePosition.Y < list_rect.Bottom))
						{
#ifdef FARAPI3
							INPUT_RECORD irecord;
							irecord.EventType=KEY_EVENT;
							KEY_EVENT_RECORD key = { false,0,KEY_ENTER,0,0 };
							irecord.Event.KeyEvent = key;
							DlgProc(hDlg, DN_CONTROLINPUT, LIST_BOX, &irecord);
#else
							DlgProc(hDlg, DN_KEY, LIST_BOX, KEY_ENTER);
#endif
							return TRUE;
						}

						// pass message to scrollbar if needed
						if((mer->dwMousePosition.X == list_rect.Right) && (mer->dwMousePosition.Y > list_rect.Top) && (mer->dwMousePosition.Y < list_rect.Bottom)) return FALSE;

						return TRUE;
					}
					else if(mer->dwButtonState == RIGHTMOST_BUTTON_PRESSED)
					{
#ifdef FARAPI3
						int k=2;
						Info.SendDlgMessage(hDlg,DMU_UPDATE,0,&k);
#else
						Info.SendDlgMessage(hDlg,DMU_UPDATE,0,2);
#endif
						return TRUE;
					}
					return false;
				}
			}
#ifdef FARAPI3
		else if (record->EventType == KEY_EVENT) {
			switch(record->Event.KeyEvent.wVirtualKeyCode)
#else
			break;
		case DN_KEY:
			switch(Param2)
#endif
			{
#ifdef FARAPI3
			case VK_F8:
#else
			case KEY_F8:
#endif
				{
					if(ListSize)
					{
						TCHAR DlgText[MAX_PATH + 200];
						StringCchPrintf(DlgText, ARRAYSIZE(DlgText), GetMsg(MConfirm), p[Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,LIST_BOX,NULL)].Keys[DisplayName]);

						if(EMessage((const TCHAR * const *) DlgText, 0, 2) == 0)
						{
							if(!DeleteEntry(static_cast<int>(Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,LIST_BOX,NULL))))
								DrawMessage(FMSG_WARNING, 1, "%s",GetMsg(MPlugIn),GetMsg(MDelRegErr),GetMsg(MBtnOk),NULL);

							Info.SendDlgMessage(hDlg,DMU_UPDATE,1,0);
						}
					}
				}
				return TRUE;
#ifdef FARAPI3
			case VK_F9:
#else
			case(KEY_F9|KEY_SHIFT|KEY_ALT):
			case(KEY_F9):
#endif
				{
					Configure(0);
				}
				return TRUE;
#ifdef FARAPI3 //для far3 будет в default.
#else
			case KEY_CTRLR:
				{
					Info.SendDlgMessage(hDlg,DMU_UPDATE,1,0);
				}
				return TRUE;
#endif
#ifdef FARAPI3
			case VK_RETURN:
#else
			case KEY_ENTER:
			case KEY_SHIFTENTER:
#endif
				{
					if(ListSize)
					{
						int liChanged = 0;
						int liSelected = 0, liFirst = -1;

						for(int i = 0; i < nCount; i++)
						{
							if(FLI[i].Flags & LIF_CHECKED)
							{
								if(liFirst == -1)
									liFirst = i;

								liSelected++;
							}
						}

						if(liSelected <= 1)
						{
#ifdef FARAPI3
							int liAction;
							if ((record->Event.KeyEvent.wVirtualKeyCode  == VK_RETURN) && (record->Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED)) {
									liAction = Opt.ShiftEnterAction;
							}else{
								liAction = Opt.EnterAction;
							}
#else
							int liAction = (Param2 == KEY_ENTER) ? Opt.EnterAction : Opt.ShiftEnterAction;
#endif
							int pos = (liFirst == -1) ? static_cast<int>(Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,LIST_BOX,NULL)) : liFirst;
							liChanged = ExecuteEntry(pos, liAction, (Opt.RunLowPriority!=0));
						}
						else
						{
#ifdef FARAPI3
							int liAction;
							if ((record->Event.KeyEvent.wVirtualKeyCode  == VK_RETURN) && (record->Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED)) {
									liAction = Opt.ShiftEnterAction;
							}else{
								liAction = Opt.EnterAction;
							}
#else
							int liAction = (Param2 == KEY_ENTER) ? Opt.EnterAction : Opt.ShiftEnterAction;
#endif
							bool LowPriority = (Opt.RunLowPriority!=0);

							// Обязательно ожидание - два инсталлятора сразу недопускаются
							if(liAction == Action_Menu)
							{
								if(EntryMenu(0, liAction, LowPriority, liSelected) < 0)
									return TRUE;
							}
							else if(liAction == Action_Uninstall)
								liAction = Action_UninstallWait;
							else if(liAction == Action_Modify)
								liAction = Action_ModifyWait;
							else if(liAction == Action_Repair)
								liAction = Action_RepairWait;

							for(int pos = 0; pos < nCount; pos++)
							{
								if(!(FLI[pos].Flags & LIF_CHECKED))
									continue;

								struct FarListPos FLP;
								FLP.SelectPos = pos;
								FLP.TopPos = -1;
#ifdef FARAPI3
								FLP.StructSize = sizeof(FarListPos);
								Info.SendDlgMessage(hDlg,DM_LISTSETCURPOS,LIST_BOX,&FLP);
#else
								Info.SendDlgMessage(hDlg,DM_LISTSETCURPOS,LIST_BOX,reinterpret_cast<LONG_PTR>(&FLP));
#endif
								int li = ExecuteEntry(pos, liAction, LowPriority);

								if(li == -1)
									break; // отмена

								if(li == 1)
									liChanged = 1;
							}
						}

						if(liChanged == 1)
						{
							Info.SendDlgMessage(hDlg,DMU_UPDATE,1,0);
						}
					}
				}
				return TRUE;
#ifdef FARAPI3
			case VK_DELETE:
#else
			case KEY_DEL:
#endif
				{
					if(lstrlen(Filter) > 0)
					{
						StringCchCopy(Filter,ARRAYSIZE(Filter),_T(""));
						Info.SendDlgMessage(hDlg,DMU_UPDATE,0,0);
					}
				}
				return TRUE;
#ifdef FARAPI3
			case VK_F3:
			case VK_F4:
#else
			case KEY_F3:
			case KEY_F4:
#endif
				{
					if(ListSize)
					{
						DisplayEntry(static_cast<int>(Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,LIST_BOX,NULL)));
						Info.SendDlgMessage(hDlg,DM_REDRAW,NULL,NULL);
					}
				}
				return TRUE;
#ifdef FARAPI3
			case VK_F2:
#else
			case KEY_F2:
#endif
				{
					Opt.SortByDate = !Opt.SortByDate;
					Info.SendDlgMessage(hDlg,DMU_UPDATE,1,0);
				}
				return TRUE;
#ifdef FARAPI3
			case VK_BACK:
#else
			case KEY_BS:
#endif
				{
					if(lstrlen(Filter))
					{
						Filter[lstrlen(Filter)-1] = '\0';
						Info.SendDlgMessage(hDlg,DMU_UPDATE,0,0);
					}
				}
				return TRUE;
			default:
				{
#ifdef FARAPI3
					//case KEY_CTRLR:
					if ((record->Event.KeyEvent.wVirtualKeyCode=='R')&&((record->Event.KeyEvent.dwControlKeyState & RIGHT_CTRL_PRESSED)||(record->Event.KeyEvent.dwControlKeyState & LEFT_CTRL_PRESSED))) {
						Info.SendDlgMessage(hDlg,DMU_UPDATE,1,0);
						return TRUE;
					}
#endif
					// KEY_INS
#ifdef FARAPI3
					if((record->Event.KeyEvent.wVirtualKeyCode==VK_INSERT)&&(!(record->Event.KeyEvent.dwControlKeyState &(LEFT_ALT_PRESSED |LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED |RIGHT_CTRL_PRESSED |SHIFT_PRESSED ))))
					{
						int k=1;
						Info.SendDlgMessage(hDlg,DMU_UPDATE,0,&k);
						return TRUE;
					}
#else
					if (Param2==KEY_INS) {
						{
							int k=1;
							Info.SendDlgMessage(hDlg,DMU_UPDATE,0,1);
							return TRUE;
						}
#endif
						//-- KEY_SHIFTINS  KEY_CTRLV
#ifdef FARAPI3
						if (
							((record->Event.KeyEvent.wVirtualKeyCode=='V')&&((record->Event.KeyEvent.dwControlKeyState & RIGHT_CTRL_PRESSED)||(record->Event.KeyEvent.dwControlKeyState & LEFT_CTRL_PRESSED)))
							||((record->Event.KeyEvent.wVirtualKeyCode==VK_INSERT)&&(record->Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED))){

#else
						if ((Param2==KEY_SHIFTINS)||(Param2==KEY_CTRLV)) {
#endif
							static TCHAR bufData[MAX_PATH];
#ifdef FARAPI3
							size_t size = FSF.PasteFromClipboard(FCT_STREAM,nullptr,0);
							TCHAR *bufP = new TCHAR[size];
							FSF.PasteFromClipboard(FCT_STREAM,bufP,size);
#else
							TCHAR * bufP = FSF.PasteFromClipboard();
#endif
							if(bufP)
							{
								StringCchCopy(bufData,ARRAYSIZE(bufData),bufP);
#ifdef FARAPI3
								delete[] bufP;
#else
								FSF.DeleteBuffer(bufP);
#endif
								unQuote(bufData);
								FSF.LStrlwr(bufData);

								for(int i = lstrlen(bufData); i >= 1; i--)
									for(int j = 0; j < nCount; j++)
										if(strnstri(p[j].Keys[DisplayName],bufData,i))
										{
											lstrcpyn(Filter,bufData,i+1);
											Info.SendDlgMessage(hDlg,DMU_UPDATE,0,0);
											return TRUE;
										}
							}
							return TRUE;
						}
						//--default
#ifdef FARAPI3
						if(record->Event.KeyEvent.wVirtualKeyCode >= VK_SPACE && record->Event.KeyEvent.wVirtualKeyCode <= VK_DIVIDE && record->Event.KeyEvent.uChar.UnicodeChar!=0)
#else
						if(Param2 >= KEY_SPACE && Param2 < KEY_FKEY_BEGIN)
#endif
						{
							struct FarListInfo ListInfo;
#ifdef FARAPI3
							ListInfo.StructSize = sizeof(FarListInfo);
							Info.SendDlgMessage(hDlg,DM_LISTINFO,LIST_BOX,&ListInfo);
#else
							Info.SendDlgMessage(hDlg,DM_LISTINFO,LIST_BOX,reinterpret_cast<LONG_PTR>(&ListInfo));
#endif

							if((lstrlen(Filter) < sizeof(Filter)) && ListInfo.ItemsNumber)
							{
								int filterL = lstrlen(Filter);

#ifdef FARAPI3
								Filter[filterL] = FSF.LLower(record->Event.KeyEvent.uChar.UnicodeChar);
#else
								Filter[filterL] = FSF.LLower(static_cast<unsigned>(Param2));
#endif
								Filter[filterL+1] = '\0';
								Info.SendDlgMessage(hDlg,DMU_UPDATE,0,0);
								return TRUE;
							}
						}
					}
				}
				}
#ifdef FARAPI3
			}
#endif

			return FALSE;
		case DN_CTLCOLORDIALOG:
#ifdef FARAPI3
			Info.AdvControl(&MainGuid,ACTL_GETCOLOR,COL_MENUTEXT, Param2);
			return true;
#else
			return Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void *)COL_MENUTEXT);
#endif
		case DN_CTLCOLORDLGLIST:
			if(Param1 == LIST_BOX)
			{
#ifdef FARAPI3
				FarDialogItemColors *fdic=(FarDialogItemColors *)Param2;
				FarColor *Colors = fdic->Colors;
#else
				FarListColors *Colors = (FarListColors *)Param2;
#endif
				int ColorIndex[] = { COL_MENUBOX, COL_MENUBOX, COL_MENUTITLE, COL_MENUTEXT, COL_MENUHIGHLIGHT, COL_MENUBOX, COL_MENUSELECTEDTEXT, COL_MENUSELECTEDHIGHLIGHT, COL_MENUSCROLLBAR, COL_MENUDISABLEDTEXT, COL_MENUARROWS, COL_MENUARROWSSELECTED, COL_MENUARROWSDISABLED };
				int Count = ARRAYSIZE(ColorIndex);
#ifdef FARAPI3
				if(Count > fdic->ColorsCount)
					Count = fdic->ColorsCount;
#else
				if(Count > Colors->ColorCount)
					Count = Colors->ColorCount;
#endif
				for(int i = 0; i < Count; i++){
#ifdef FARAPI3
					FarColor fc;
					if (static_cast<BYTE>(Info.AdvControl(&MainGuid, ACTL_GETCOLOR, ColorIndex[i],&fc))) {
						Colors[i] = fc;
					}
#else
					Colors->Colors[i] = static_cast<BYTE>(Info.AdvControl(Info.ModuleNumber, ACTL_GETCOLOR, reinterpret_cast<void *>(ColorIndex[i])));
#endif
				}
				return TRUE;
			}
			break;
	}

	return Info.DefDlgProc(hDlg,Msg,Param1,Param2);
}

#ifdef FARAPI3
HANDLE WINAPI OpenW(const struct OpenInfo *oInfo)
#else
HANDLE WINAPI OpenPlugin(int /*OpenFrom*/, INT_PTR /*Item*/)
#endif
{
	ReadRegistry();
	struct FarDialogItem DialogItems[1];
	ZeroMemory(DialogItems, sizeof(DialogItems));
	p = NULL;
	FLI = NULL;
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	UpDateInfo();
	DialogItems[0].Type = DI_LISTBOX;
	DialogItems[0].Flags = DIF_LISTNOAMPERSAND ;
#ifdef FARAPI3
	DialogItems[0].Flags |=DIF_LISTTRACKMOUSEINFOCUS;
#endif
	DialogItems[0].X1 = 2;
	DialogItems[0].Y1 = 1;
#ifdef FARAPI17
	Info.DialogEx(Info.ModuleNumber,-1,-1,0,0,"Contents",DialogItems,ARRAYSIZE(DialogItems),0,0,DlgProc,0);
#endif
#ifdef FARAPI18
	HANDLE h_dlg = Info.DialogInit(Info.ModuleNumber,-1,-1,0,0,L"Contents",DialogItems,ARRAYSIZE(DialogItems),0,0,DlgProc,0);

	if(h_dlg != INVALID_HANDLE_VALUE)
	{
		Info.DialogRun(h_dlg);
		Info.DialogFree(h_dlg);
	}

#endif
#ifdef FARAPI3
	HANDLE h_dlg = Info.DialogInit(&MainGuid,&ContentsGuid,-1,-1,0,0,L"Contents",DialogItems,ARRAYSIZE(DialogItems),0,0,DlgProc,0);

	if(h_dlg != INVALID_HANDLE_VALUE)
	{
		Info.DialogRun(h_dlg);
		Info.DialogFree(h_dlg);
	}

#endif
	FLI = (FarListItem *) realloc(FLI, 0);
	p = (KeyInfo *) realloc(p, 0);
	return NULL;
}

#ifdef FARAPI3
intptr_t WINAPI Configure(const struct ConfigureInfo *cInfo)
{
	PluginDialogBuilder Config(Info, MainGuid,ConfigGuid, MPlugIn, _T("Configuration"));
#else
int WINAPI Configure(int ItemNumber)
{
	PluginDialogBuilder Config(Info, MPlugIn, _T("Configuration"));
#endif
	FarDialogItem *p1, *p2;
	BOOL bShowInViewer = (Opt.WhereWork & 1) != 0;
	BOOL bShowInEditor = (Opt.WhereWork & 2) != 0;
	//BOOL bEnterWaitCompletion = (Opt.EnterFunction != 0);
	BOOL bUseElevation = (Opt.UseElevation != 0);
	BOOL bLowPriority = (Opt.RunLowPriority != 0);
	BOOL bForceMsiUse = (Opt.ForceMsiUse != 0);
	Config.AddCheckbox(MShowInEditor, &bShowInEditor);
	Config.AddCheckbox(MShowInViewer, &bShowInViewer);
	//Config.AddCheckbox(MEnterWaitCompletion, &bEnterWaitCompletion);
	Config.AddCheckbox(MUseElevation, &bUseElevation);
	Config.AddCheckbox(MLowPriority, &bUseElevation);
	Config.AddCheckbox(MForceMsiUse, &bForceMsiUse);
	Config.AddSeparator();
	FarList AEnter, AShiftEnter;
	AEnter.ItemsNumber = AShiftEnter.ItemsNumber = 7;
	AEnter.Items = (FarListItem*)calloc(AEnter.ItemsNumber,sizeof(FarListItem));
	AShiftEnter.Items = (FarListItem*)calloc(AEnter.ItemsNumber,sizeof(FarListItem));

#ifdef FARAPI3
	AEnter.StructSize = sizeof(FarList);
	AShiftEnter.StructSize = sizeof(FarList);
#endif	
	for(size_t i = 0; i < AEnter.ItemsNumber; i++)
	{
#if defined FARAPI18 || defined FARAPI3
		AEnter.Items[i].Text = GetMsg(MActionUninstallWait+i);
		AShiftEnter.Items[i].Text = AEnter.Items[i].Text;
#else
		StringCchCopy(AEnter.Items[i].Text,ARRAYSIZE(AEnter.Items[i].Text),GetMsg(MActionUninstallWait+i));
		StringCchCopy(AShiftEnter.Items[i].Text,ARRAYSIZE(AShiftEnter.Items[i].Text),AEnter.Items[i].Text);
#endif
	}

	p1 = Config.AddText(MEnterAction); p2 = Config.AddComboBox(23, &AEnter, &Opt.EnterAction);
	Config.MoveItemAfter(p1,p2);
	p1 = Config.AddText(MShiftEnterAction); p2 = Config.AddComboBox(23, &AShiftEnter, &Opt.ShiftEnterAction);
	Config.MoveItemAfter(p1,p2);
	Config.AddOKCancel(MBtnOk, MBtnCancel);

	if(Config.ShowDialog())
	{
		Opt.WhereWork = (bShowInViewer ? 1 : 0) | (bShowInEditor ? 2 : 0);
		//Opt.EnterFunction = bEnterWaitCompletion;
		Opt.UseElevation = bUseElevation;
		Opt.RunLowPriority = bLowPriority;
		Opt.ForceMsiUse = bForceMsiUse;
#ifdef FARAPI3
		PluginSettings settings(MainGuid,Info.SettingsControl);
		settings.Set(0,L"WhereWork",Opt.WhereWork);
		settings.Set(0,L"EnterAction",Opt.EnterAction);
		settings.Set(0,L"ShiftEnterAction",Opt.ShiftEnterAction);
		settings.Set(0,L"UseElevation",Opt.UseElevation);
		settings.Set(0,L"RunLowPriority",Opt.RunLowPriority);
		settings.Set(0,L"ForceMsiUse",Opt.ForceMsiUse);
#else
		SetRegKey(HKCU,_T(""),_T("WhereWork"),(DWORD) Opt.WhereWork);
		SetRegKey(HKCU,_T(""),_T("EnterAction"),(DWORD) Opt.EnterAction);
		SetRegKey(HKCU,_T(""),_T("ShiftEnterAction"),(DWORD) Opt.ShiftEnterAction);
		SetRegKey(HKCU,_T(""),_T("UseElevation"),(DWORD) Opt.UseElevation);
		SetRegKey(HKCU,_T(""),_T("RunLowPriority"),(DWORD) Opt.RunLowPriority);
		SetRegKey(HKCU,_T(""),_T("ForceMsiUse"),(DWORD) Opt.ForceMsiUse);
#endif
	}

	return FALSE;
}
