  // Included inside App. Network work never runs on the window thread.
  HWND update_notice{},update_panel{},update_notes{},update_action{},update_cancel{};
  std::optional<updates::Release> update_release;
  std::future<updates::CheckResult> update_check;
  std::future<updates::StagedUpdate> update_download;
  std::future<std::unique_ptr<updates::Installation>> update_prepare;
  std::unique_ptr<updates::Installation> update_installation;
  std::optional<updates::StagedUpdate> update_staged;
  std::atomic_bool update_cancelled{false};
  std::atomic<uint64_t> update_received{0},update_total{0};
  bool update_finishing=false,update_busy=false,update_preparing=false,update_handoff=false,update_open=false;
  std::vector<std::pair<HWND,bool>> update_disabled_controls;
  int update_panel_height=0,update_animation_from=0;
  ULONGLONG update_animation_start=0;
  static constexpr int kUpdateWidth=324,kUpdateHeight=310;

  void StopUpdates(){
    update_cancelled=true;
    if(update_check.valid())update_check.wait();
    if(update_download.valid()){
      try{auto staged=update_download.get();updates::DiscardStaged(staged);}catch(...){}
    }
    if(update_prepare.valid()){
      try{update_installation=update_prepare.get();}catch(...){}
    }
    update_installation.reset();
    if(update_staged&&!update_handoff)updates::DiscardStaged(*update_staged);
  }
  void SetUpdatePreparation(bool preparing){
    update_preparing=preparing;
    if(preparing){
      for(auto c:{page_view,path,browse,save,defaults,language,verify_files,play}){update_disabled_controls.emplace_back(c,IsWindowEnabled(c)!=FALSE);EnableWindow(c,FALSE);}
    }else{
      for(auto [c,enabled]:update_disabled_controls)EnableWindow(c,enabled);
      update_disabled_controls.clear();
    }
  }
  bool UpdateShortcut(const MSG& message){
    if(message.message==WM_KEYDOWN&&message.wParam==VK_ESCAPE&&update_open){ToggleUpdatePanel();return true;}return false;
  }
  bool UpdateGameRunning()const{return process||updates::GameRunning(exe);}
  void LayoutUpdatePanel(){
    if(!update_panel)return;
    SetWindowPos(update_panel,HWND_TOP,S(kWidth-kUpdateWidth-18),S(kCaptionHeight+18),S(kUpdateWidth),S(update_panel_height),SWP_NOACTIVATE|SWP_NOCOPYBITS);
    if(update_notice)SetWindowPos(update_notice,HWND_TOP,S(kWidth-138),0,S(46),S(kCaptionHeight),SWP_NOACTIVATE);
  }
  void ToggleUpdatePanel(){
    if(!update_release)return;
    CancelCapture();help.Pop();update_open=!update_open;update_animation_from=update_panel_height;update_animation_start=GetTickCount64();
    ShowWindow(update_panel,SW_SHOW);SetWindowPos(update_panel,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    SetTimer(window,5,16,nullptr);
    if(!update_open&&IsChild(update_panel,GetFocus()))SetFocus(update_notice);
  }
  void AnimateUpdatePanel(){
    const double t=std::min(1.0,(GetTickCount64()-update_animation_start)/180.0),ease=1.0-std::pow(1.0-t,3.0);
    const int target=update_open?kUpdateHeight:0;
    update_panel_height=static_cast<int>(update_animation_from+(target-update_animation_from)*ease);
    LayoutUpdatePanel();
    if(t>=1){KillTimer(window,5);if(!update_open)ShowWindow(update_panel,SW_HIDE);}
  }
  void DrawUpdateNotice(HDC dc,HWND control,bool pressed){
    DrawSurface(dc,control);RECT r;GetClientRect(control,&r);
    Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    if(pressed){Gdiplus::SolidBrush shade(Gdiplus::Color(45,255,255,255));g.FillRectangle(&shade,0,0,r.right,r.bottom);}
    Gdiplus::Pen pen(Gdiplus::Color(255,255,106,19),static_cast<float>(1.5*scale));
    const float cx=r.right*.5f,cy=r.bottom*.5f,unit=static_cast<float>(scale);
    // Download arrow and tray, drawn as vectors for every launcher DPI scale.
    g.DrawLine(&pen,cx,cy-7*unit,cx,cy+2*unit);
    g.DrawLine(&pen,cx-4*unit,cy-2*unit,cx,cy+2*unit);
    g.DrawLine(&pen,cx+4*unit,cy-2*unit,cx,cy+2*unit);
    g.DrawLine(&pen,cx-6*unit,cy+4*unit,cx-6*unit,cy+7*unit);
    g.DrawLine(&pen,cx-6*unit,cy+7*unit,cx+6*unit,cy+7*unit);
    g.DrawLine(&pen,cx+6*unit,cy+7*unit,cx+6*unit,cy+4*unit);
  }
  void RefreshUpdatePanel(){
    if(!update_panel||!update_release)return;
    SetWindowTextW(update_notice,T(L"Launcher update",L"Launcher güncellemesi"));
    auto notes=update_release->NotesFor(turkish?"tr":"en");
    if(notes.empty())notes=turkish?"Bu sürüm için değişiklik notu yok.":"No release notes for this version.";
    auto wide=Wide(notes);std::wstring multiline;
    for(size_t i=0;i<wide.size();++i){if(wide[i]==L'\n'&&(i==0||wide[i-1]!=L'\r'))multiline+=L'\r';multiline+=wide[i];}
    if(Text(update_notes)!=multiline)SetWindowTextW(update_notes,multiline.c_str());
    const bool game=UpdateGameRunning();
    std::wstring action=T(L"Update now",L"Şimdi güncelle");
    if(update_preparing)action=T(L"Preparing installation…",L"Kurulum hazırlanıyor…");
    else if(update_busy&&update_cancelled)action=T(L"Cancelling…",L"İptal ediliyor…");
    else if(update_busy){
      const auto total=update_total.load(),received=update_received.load();
      action=T(L"Downloading…",L"İndiriliyor…");
      if(total)action+=L" "+std::to_wstring(std::min<uint64_t>(100,received*100/total))+L"%";
    }else if(game)action=T(L"Close the game to update",L"Güncellemek için oyunu kapat");
    if(Text(update_action)!=action)SetWindowTextW(update_action,action.c_str());
    EnableWindow(update_action,!update_finishing&&!update_busy&&!game&&update_release->CanInstall());
    SetWindowTextW(update_cancel,T(L"Cancel",L"İptal"));
    ShowWindow(update_cancel,update_busy?SW_SHOW:SW_HIDE);EnableWindow(update_cancel,!update_preparing&&!update_cancelled);
    EnableWindow(play,!update_finishing&&!update_busy&&!process&&!verifying);
    InvalidateRect(update_panel,nullptr,FALSE);
  }
  static LRESULT CALLBACK UpdatePanelProc(HWND control,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    auto& app=*reinterpret_cast<App*>(data);
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_PAINT){
      PAINTSTRUCT ps;auto dc=BeginPaint(control,&ps);RECT bounds;GetClientRect(control,&bounds);
      PaintBuffered(dc,bounds,[&](HDC target,RECT){
        app.DrawSurface(target,control);
        auto full=bounds;full.bottom=app.S(kUpdateHeight);app.Rounded(target,full,kCard,kBorder,10);
        SetBkMode(target,TRANSPARENT);SetTextColor(target,kText);SelectObject(target,app.bold);
        RECT title{app.S(16),app.S(15),app.S(kUpdateWidth-44),app.S(61)};
        auto text=std::wstring(app.T(L"New launcher version: v",L"Yeni launcher sürümü: v"))+(app.update_release?Wide(app.update_release->version):L"");
        DrawTextW(target,text.c_str(),-1,&title,DT_WORDBREAK|DT_NOPREFIX|DT_END_ELLIPSIS);
        SelectObject(target,app.font);SetTextColor(target,kMuted);RECT close{app.S(kUpdateWidth-36),app.S(12),app.S(kUpdateWidth-12),app.S(38)};
        DrawTextW(target,L"×",-1,&close,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
      });EndPaint(control,&ps);return 0;
    }
    if(message==WM_LBUTTONUP){POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
      if(p.x>=app.S(kUpdateWidth-42)&&p.y<app.S(44))app.ToggleUpdatePanel();return 0;}
    if(message==WM_KEYDOWN&&wp==VK_ESCAPE){if(app.update_open)app.ToggleUpdatePanel();return 0;}
    if(message==WM_CTLCOLORSTATIC||message==WM_CTLCOLOREDIT){SetTextColor(reinterpret_cast<HDC>(wp),kMuted);SetBkColor(reinterpret_cast<HDC>(wp),kCard);return reinterpret_cast<LRESULT>(app.card);}
    if(message==WM_COMMAND||message==WM_DRAWITEM)return SendMessageW(app.window,message,wp,lp);
    if(message==WM_NCDESTROY)RemoveWindowSubclass(control,UpdatePanelProc,id);
    return DefSubclassProc(control,message,wp,lp);
  }
  void BuildUpdatePanel(){
    update_notice=Control(L"BUTTON",T(L"Launcher update",L"Launcher güncellemesi"),WS_TABSTOP|BS_PUSHBUTTON,kWidth-138,-kCaptionHeight,46,kCaptionHeight,kUpdateNotice);
    ShowWindow(update_notice,SW_HIDE);
    WNDCLASSW cls{};cls.lpfnWndProc=DefWindowProcW;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"MotoGPUpdatePanel";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    if(!GetClassInfoW(cls.hInstance,cls.lpszClassName,&cls)&&!RegisterClassW(&cls))throw std::runtime_error("Cannot create update panel class");
    update_panel=Control(cls.lpszClassName,L"",WS_CLIPCHILDREN,kWidth-kUpdateWidth-18,18,kUpdateWidth,kUpdateHeight);
    SetWindowSubclass(update_panel,UpdatePanelProc,1,reinterpret_cast<DWORD_PTR>(this));
    SetWindowLongPtrW(update_panel,GWL_EXSTYLE,WS_EX_CONTROLPARENT);
    update_notes=Control(L"EDIT",L"",WS_TABSTOP|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,16,68,kUpdateWidth-32,154,0,update_panel);
    update_action=Control(L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,16,234,kUpdateWidth-32,34,kUpdateNow,update_panel);
    update_cancel=Control(L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,104,274,116,28,kUpdateCancel,update_panel);
    ShowWindow(update_panel,SW_HIDE);ShowWindow(update_cancel,SW_HIDE);LayoutUpdatePanel();
  }
  void StartUpdateCheck(){
    update_finishing=updates::InstallationActive(exe,false);
    if(update_finishing){EnableWindow(play,FALSE);SetTimer(window,4,150,nullptr);}
    if(!updates::Configured(updates::kConfiguration))return;
    try{update_check=std::async(std::launch::async,[this]{return updates::CheckForUpdate(updates::kConfiguration,&update_cancelled);});SetTimer(window,4,150,nullptr);}catch(...){}
  }
  void StartUpdateDownload(){
    if(!update_release||update_finishing||update_busy||UpdateGameRunning()||!update_release->CanInstall())return;
    update_cancelled=false;update_received=0;update_total=0;update_busy=true;RefreshUpdatePanel();
    const auto release=*update_release;
    try{update_download=std::async(std::launch::async,[this,release]{return updates::DownloadVerified(updates::kConfiguration,release,&update_cancelled,[this](uint64_t received,uint64_t total){update_received=received;update_total=total;});});}
    catch(...){update_busy=false;RefreshUpdatePanel();throw;}
  }
  void PollUpdates(){
    try{
      if(update_finishing&&!updates::InstallationActive(exe,false)){
        update_finishing=false;EnableWindow(play,!update_busy&&!process&&!verifying);
        if(!update_check.valid()&&!update_release){KillTimer(window,4);return;}
      }
      if(update_check.valid()&&update_check.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        auto result=update_check.get();
        if(result.status==updates::CheckStatus::Available){update_release=std::move(result.release);ShowWindow(update_notice,SW_SHOW);LayoutUpdatePanel();}
        // Startup errors are quiet. No timer retries or button-triggered checks.
        if(!update_release&&!update_finishing){KillTimer(window,4);return;}
      }
      if(update_download.valid()&&update_download.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        update_staged=update_download.get();
        if(update_cancelled){updates::DiscardStaged(*update_staged);update_staged.reset();update_busy=false;}
        else{
          if(UpdateGameRunning())throw std::runtime_error("Close the game before installing this update");
          SaveSettings();SetUpdatePreparation(true);
          auto staged=*update_staged;const auto root=exe;
          std::vector<fs::path> protected_roots{GameRoot(),AchievementRoot()};
          auto metadata=achievements::MetadataRoot(exe,Get(config.table,"metadata_root",std::string{}));if(!metadata.empty())protected_roots.push_back(metadata);
          for(auto key:{"cache_root","update_data_root","metadata_root"}){
            auto value=Get(config.table,key,std::string{});if(!value.empty()){auto p=fs::path(Wide(value));protected_roots.push_back(p.is_absolute()?p:exe/p);}
          }
          const bool tr=turkish;
          update_prepare=std::async(std::launch::async,[this,staged,root,protected_roots,tr]{return updates::PrepareInstallation(staged,root,protected_roots,tr,&update_cancelled);});
        }
      }
      if(update_prepare.valid()&&update_prepare.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        update_installation=update_prepare.get();
        if(UpdateGameRunning())throw std::runtime_error("Close the game before installing this update");
        updates::CommitInstallation(*update_installation);update_handoff=true;
        PostMessageW(window,WM_CLOSE,0,0);return;
      }
    }catch(const std::exception& e){
      update_installation.reset();if(update_staged){updates::DiscardStaged(*update_staged);update_staged.reset();}
      update_busy=false;SetUpdatePreparation(false);
      if(!update_cancelled)MessageBoxW(window,(std::wstring(T(L"Could not update the launcher.\n",L"Launcher güncellenemedi.\n"))+ErrorDetails(e)).c_str(),T(L"Launcher update",L"Launcher güncellemesi"),MB_OK|MB_ICONERROR);
    }
    RefreshUpdatePanel();
  }
