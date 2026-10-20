  HWND dlc_button{};
  std::future<dlc::Result> dlc_future;
  std::atomic<bool> dlc_cancel{false};
  dlc::Progress dlc_progress;
  bool dlc_busy=false;
  void StopDlc(){dlc_cancel=true;if(dlc_future.valid())dlc_future.wait();}
  void RefreshDlc(){
    if(!dlc_button)return;
    std::wstring text=T(L"Install DLC",L"DLC kur");
    if(dlc_busy){
      if(dlc_cancel)text=T(L"Cancelling…",L"İptal ediliyor…");
      else if(dlc_progress.phase==2)text=T(L"Installing DLC…",L"DLC kuruluyor…");
      else if(dlc_progress.phase==0)text=T(L"Cancel DLC import",L"DLC kurulumunu iptal et");
      else {text=T(L"Cancel (",L"İptal (%");const auto total=dlc_progress.total.load();text+=std::to_wstring(total?std::min<uint64_t>(100,dlc_progress.done.load()*100/total):0);text+=turkish?L")":L"%)";}
    }
    SetWindowTextW(dlc_button,text.c_str());
    ShowWindow(dlc_button,current_page==3&&(dlc_busy||game_ready)?SW_SHOW:SW_HIDE);
    EnableWindow(dlc_button,dlc_busy?!dlc_cancel&&dlc_progress.phase!=2:!process&&!verifying&&!importing_iso&&!steam_busy&&!update_busy&&!update_finishing);
    Tip(dlc_button,L"Install your Xbox 360 DLC from a ZIP, RAR or 7z archive, or an extracted folder. Game files and saves are preserved.",
        L"Xbox 360 DLC'lerini ZIP, RAR veya 7z arşivinden ya da çıkarılmış klasörden kurar. Oyun dosyaları ve kayıtlar korunur.");
  }
  void BuildDlc(){dlc_button=PageControl(3,L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,196,74,184,32,kInstallDlc);RefreshDlc();}
  fs::path PickDlcSource(bool folder){
    IFileOpenDialog* raw=nullptr;
    if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&raw))))throw Error(L"Could not open the file picker.",L"Dosya seçme penceresi açılamadı.");
    struct Release{IFileOpenDialog* dialog;~Release(){dialog->Release();}} release{raw};
    DWORD options=0;raw->GetOptions(&options);raw->SetOptions(options|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST|(folder?FOS_PICKFOLDERS:FOS_FILEMUSTEXIST));
    const COMDLG_FILTERSPEC filters[]{{T(L"DLC archives (*.zip;*.rar;*.7z)",L"DLC arşivleri (*.zip;*.rar;*.7z)"),L"*.zip;*.rar;*.7z"},{T(L"All files (*.*)",L"Tüm dosyalar (*.*)"),L"*.*"}};
    if(!folder)raw->SetFileTypes(2,filters);
    raw->SetTitle(folder?T(L"Select the extracted DLC folder",L"Çıkarılmış DLC klasörünü seç"):T(L"Select your MotoGP 09/10 DLC archive",L"MotoGP 09/10 DLC arşivini seç"));
    const auto result=raw->Show(window);if(result==HRESULT_FROM_WIN32(ERROR_CANCELLED))return {};
    if(FAILED(result))throw std::runtime_error("Could not show the DLC picker");
    IShellItem* item=nullptr;if(FAILED(raw->GetResult(&item)))throw std::runtime_error("Could not read the selected DLC source");
    PWSTR name=nullptr;const auto status=item->GetDisplayName(SIGDN_FILESYSPATH,&name);item->Release();
    if(FAILED(status))throw std::runtime_error("Could not read the selected DLC path");
    fs::path selected;try{selected=fs::weakly_canonical(name);}catch(...){CoTaskMemFree(name);throw;}CoTaskMemFree(name);return selected;
  }
  void StartDlcImport(){
    if(dlc_busy){if(dlc_progress.phase!=2)dlc_cancel=true;RefreshDlc();return;}
    if(process||verifying||importing_iso||steam_busy||update_busy||update_finishing||UpdateGameRunning())return;
    ReadyStatus();if(!game_ready)return;
    HMENU menu=CreatePopupMenu();if(!menu)throw std::runtime_error("Could not create DLC source menu");
    AppendMenuW(menu,MF_STRING,1,T(L"Select archive…",L"Arşiv seç…"));AppendMenuW(menu,MF_STRING,2,T(L"Select folder…",L"Klasör seç…"));
    RECT bounds{};GetWindowRect(dlc_button,&bounds);const auto choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_LEFTALIGN,bounds.left,bounds.bottom,0,window,nullptr);DestroyMenu(menu);if(choice!=1&&choice!=2)return;
    const auto source=PickDlcSource(choice==2);if(source.empty())return;
    const auto application=exe,root=AchievementRoot();dlc_cancel=false;dlc_progress.done=0;dlc_progress.total=0;dlc_progress.phase=0;
    dlc_future=std::async(std::launch::async,[this,source,application,root]{return dlc::Import(source,application,root,dlc_cancel,dlc_progress);});
    dlc_busy=true;
    for(auto control:{path,browse,play,save,defaults,verify_files})EnableWindow(control,FALSE);
    RefreshDlc();RefreshSteam();RefreshUpdatePanel();SetTimer(window,9,200,nullptr);
  }
  void PollDlc(){
    if(!dlc_busy||!dlc_future.valid())return;RefreshDlc();
    if(dlc_future.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
    dlc_busy=false;KillTimer(window,9);
    for(auto control:{path,browse,play,save,defaults,verify_files})EnableWindow(control,!process&&!update_busy&&!update_finishing&&!steam_busy);
    try{
      const auto result=dlc_future.get();
      const auto message=std::wstring(T(L"DLC import complete. Installed: ",L"DLC kurulumu tamamlandı. Kurulan: "))+std::to_wstring(result.installed)+
          T(L". Already installed: ",L". Zaten kurulu: ")+std::to_wstring(result.existing)+T(L".\nThe content will be available the next time you start the game.",L".\nİçerikler oyunu bir sonraki açışında kullanılabilir olacak.");
      MessageBoxW(window,message.c_str(),L"MotoGP 09/10 DLC",MB_OK|MB_ICONINFORMATION);
    }catch(const iso::Cancelled&){}
    catch(const std::exception& error){ErrorBox(error);}
    ReadyStatus();RefreshDlc();RefreshSteam();RefreshUpdatePanel();
  }
