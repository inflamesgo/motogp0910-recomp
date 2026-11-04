  // The network page shares the main launcher's pending settings and Save/PLAY
  // transaction. Switching modes or tabs never discards hidden field values.
  void ApplyAccountFonts(){
    SendMessageW(account_status,WM_SETFONT,reinterpret_cast<WPARAM>(account_name_font),TRUE);
    for(auto c:{account_label,account_country,account_intro,account_note})
      SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(small_font),TRUE);
  }
  void BuildNetworkPage(){
    const auto label=LabelControl(6,L"Connection mode",L"Bağlantı modu",0,0,kFormWidth,24);
    network_mode=PageControl(6,L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST,0,26,kFormWidth,310,kLanEnabled);
    help_labels[network_mode]=label;
    auto field=[&](const wchar_t* en,const wchar_t* tr,int x,int y,int width,int id,int limit){
      const auto caption=LabelControl(6,en,tr,x,y,width,24);
      const auto control=PageControl(6,L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL|WS_BORDER,x,y+28,width,30,id);
      SendMessageW(control,EM_SETLIMITTEXT,limit,0);help_labels[control]=caption;return control;
    };
    network_player=field(L"Player name",L"Oyuncu adı",0,88,kColumnWidth,kLanPlayer,15);
    network_room=field(L"Room name (host)",L"Oda adı (kurucu)",kColumnX,88,kColumnWidth,kLanRoom,31);
    network_adapter=field(L"Local IPv4 (optional)",L"Yerel IPv4 (isteğe bağlı)",0,176,kColumnWidth,kLanAdapter,15);
    network_host=field(L"Host IPv4 (optional)",L"Kurucunun IPv4 adresi (isteğe bağlı)",kColumnX,176,kColumnWidth,kLanHost,15);
    network_server=field(L"Online server (HTTPS)",L"Online sunucusu (HTTPS)",0,176,kFormWidth,kOnlineServer,512);
    // Unframed account panel. Keep the existing side-by-side 281 x 30 inputs.
    account_label=LabelControl(6,L"Signed in as",L"Giriş yapılan hesap",0,92,kFormWidth,20);
    account_status=PageControl(6,L"STATIC",L"",SS_LEFTNOWORDWRAP|SS_CENTERIMAGE,0,112,kFormWidth,48);
    account_country=PageControl(6,L"STATIC",L"",SS_LEFTNOWORDWRAP|SS_CENTERIMAGE,0,164,kFormWidth,24);
    account_intro=LabelControl(6,L"Sign in with your NOBK Online account. Your username is shown in the game.",
        L"NOBK Online hesabınla giriş yap. Kullanıcı adın oyunda görünür.",0,96,kFormWidth,32);
    account_user=field(L"Username",L"Kullanıcı adı",0,132,kColumnWidth,kAccountUser,15);
    const auto pass_caption=LabelControl(6,L"Password",L"Şifre",kColumnX,132,kColumnWidth,24);
    account_pass=PageControl(6,L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL|ES_PASSWORD|WS_BORDER,kColumnX,160,kColumnWidth,30,kAccountPass);
    SendMessageW(account_pass,EM_SETLIMITTEXT,128,0);help_labels[account_pass]=pass_caption;
    account_remember=PageControl(6,L"BUTTON",L"",WS_TABSTOP|BS_AUTOCHECKBOX,0,202,kFormWidth,28,kRememberAccount);
    SetCheck(account_remember,remember_account);
    account_signin=PageControl(6,L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,0,244,96,32,kSignIn);
    account_register=PageControl(6,L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,106,244,144,32,kRegister);
    account_forgot=PageControl(6,L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,260,244,184,32,kForgotPassword);
    account_note=LabelControl(6,L"",
        L"",0,290,kFormWidth,48);
    account_manage=PageControl(6,L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,0,208,124,32,kMyAccount);
    account_signout=PageControl(6,L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,134,208,120,32,kSignOut);
    ApplyAccountFonts();
  }
  void PlaceNetworkControl(HWND control,int x,int y){
    for(auto& p:placements)if(p.control==control){
      p.x=x;p.y=y+8; // Same page-content inset as PageControl.
      SetWindowPos(control,nullptr,S(p.x),S(p.y),0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);break;
    }
  }
  void ShowAccountControls(bool online){
    const bool signed_in=account.has_value();
    const auto show=[&](HWND control,bool visible){
      if(!visible&&GetFocus()==control)SetFocus(network_mode);
      EnableWindow(control,!process&&visible);ShowWindow(control,current_page==6&&visible?SW_SHOW:SW_HIDE);
    };
    show(account_label,online&&signed_in);show(account_status,online&&signed_in);
    show(account_country,online&&signed_in&&!account->country.empty());
    show(account_intro,online&&!signed_in);show(account_note,online&&!signed_in);
    for(auto control:{account_user,account_pass}){show(control,online&&!signed_in);show(help_labels.at(control),online&&!signed_in);}
    for(auto control:{account_remember,account_signin,account_register,account_forgot})show(control,online&&!signed_in);
    show(account_manage,online&&signed_in);show(account_signout,online&&signed_in);
    SetWindowTextW(account_status,signed_in?Wide(account->username).c_str():L"");
    SetWindowTextW(account_country,signed_in&&!account->country.empty()?
        (std::wstring(T(L"Country: ",L"Ülke: "))+Wide(account->country)).c_str():L"");
  }
  void AccountSignIn(){
    const auto user=Utf8(Text(account_user));auto password=Utf8(Text(account_pass));
    Status(L"Signing in to NOBK Online…",L"NOBK Online'a giriş yapılıyor…");UpdateWindow(window);
    try{
      remember_account=Checked(account_remember);
      Put(config.table,"launcher_remember_me",remember_account);config.Save();
      account=SignIn(user,password,turkish,remember_account);
    }catch(...){SecureZeroMemory(password.data(),password.size());SetWindowTextW(account_pass,L"");throw;}
    SecureZeroMemory(password.data(),password.size());SetWindowTextW(account_pass,L"");
    UpdateNetworkMode();
    Status(L"Signed in as "+Wide(account->username)+L".",Wide(account->username)+L" hesabıyla giriş yapıldı.",true);
  }
  void AccountSignOut(){
    if(account)SetWindowTextW(account_user,Wide(account->username).c_str());
    ForgetAccount();account.reset();SetWindowTextW(account_pass,L"");UpdateNetworkMode();
    Status(L"Signed out of NOBK Online.",L"NOBK Online hesabından çıkış yapıldı.");
  }
  // The in-memory session is also used when Remember me is unchecked. Online
  // still refuses PLAY without a login; there is no automatic Offline fallback.
  void PrepareOnlineLaunch(){
    if(SendMessageW(network_mode,CB_GETCURSEL,0,0)!=2)return;
    Status(L"Connecting to NOBK Online…",L"NOBK Online'a bağlanılıyor…");UpdateWindow(window);
    std::string name;
    try{name=PrepareOnline(account,remember_account,kCommunityServer,turkish);}
    catch(const SignInRejected&){account.reset();UpdateNetworkMode();throw;}
    SetWindowTextW(network_player,Wide(name).c_str());UpdateNetworkMode();
  }
  void UpdateNetworkMode(){
    if(!network_mode)return;
    help.Pop();
    const auto mode=SendMessageW(network_mode,CB_GETCURSEL,0,0);
    const bool online=mode==2;
    // Room name belongs to hosting after sign-in, not to the login form. LAN
    // keeps its original account-free two-column layout and field values.
    PlaceNetworkControl(help_labels.at(network_room),online?0:kColumnX,online?278:88);
    PlaceNetworkControl(network_room,online?0:kColumnX,online?306:116);
    for(auto control:{network_player,network_room,network_adapter,network_host,network_server}){
      const bool relevant=control==network_server?false:control==network_player?mode==1:
          (control==network_adapter||control==network_host)?mode==1:mode==1||(online&&account.has_value());
      if(!relevant&&GetFocus()==control)SetFocus(network_mode);
      EnableWindow(control,!process&&relevant);
      ShowWindow(control,current_page==6&&relevant?SW_SHOW:SW_HIDE);
      ShowWindow(help_labels.at(control),current_page==6&&relevant?SW_SHOW:SW_HIDE);
    }
    ShowAccountControls(online);
    EnableWindow(network_mode,!process);
    if(current_page==6)RedrawWindow(page_content,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
  }
  void SetNetworkControls(const Settings& s){
    SendMessageW(network_mode,CB_SETCURSEL,s.network_mode=="online"?2:s.network_mode=="lan"?1:0,0);
    for(auto [control,value]:{std::pair{network_player,&s.lan_player_name},{network_room,&s.lan_room_name},
        {network_adapter,&s.lan_bind_ip},{network_host,&s.lan_host_ip},{network_server,&s.online_server}})
      SetWindowTextW(control,Wide(*value).c_str());
    UpdateNetworkMode();
  }
  void ReadNetworkControls(Settings& s)const{
    const auto mode=SendMessageW(network_mode,CB_GETCURSEL,0,0);
    if(mode<0||mode>2)throw std::runtime_error("Invalid connection mode");
    s.network_mode=std::array{"offline","lan","online"}[mode];s.lan_enabled=mode==1;
    s.lan_player_name=Utf8(Text(network_player));s.lan_room_name=Utf8(Text(network_room));
    s.lan_bind_ip=Utf8(Text(network_adapter));s.lan_host_ip=Utf8(Text(network_host));s.online_server=kCommunityServer;
  }
  void LocalizeNetworkHelp(){
    Tip(network_mode,L"Online uses NOBK Online. LAN connects players on the same local or virtual network. Offline uses no multiplayer service.",L"Online, NOBK Online hizmetini kullanır. LAN, aynı yerel veya sanal ağdaki oyuncuları bağlar. Çevrim dışı mod çok oyunculu hizmete bağlanmaz.");
    Tip(network_player,L"Name shown to other players. Up to 15 UTF-8 bytes.",L"Diğer oyuncuların gördüğü ad. En fazla 15 UTF-8 bayt.");
    Tip(network_room,L"Name used when hosting a room. Up to 31 UTF-8 bytes.",L"Oda kurarken kullanılan ad. En fazla 31 UTF-8 bayt.");
    Tip(network_adapter,L"Leave empty for automatic selection, including virtual LAN adapters. Both players must join the same local or virtual LAN.",L"Sanal LAN bağdaştırıcıları dahil otomatik seçim için boş bırak. Oyuncular aynı yerel veya sanal LAN ağına bağlanmalı.");
    Tip(network_host,L"Rooms are discovered automatically. Enter the host's LAN IPv4 only if discovery fails.",L"Odalar otomatik bulunur. Yalnızca oda bulunamazsa kurucunun LAN IPv4 adresini gir.");
    SetWindowTextW(account_signin,T(L"Sign in",L"Giriş yap"));SetWindowTextW(account_register,T(L"Create account",L"Hesap oluştur"));
    SetWindowTextW(account_signout,T(L"Sign out",L"Çıkış yap"));
    SetWindowTextW(account_manage,T(L"My account",L"Hesabım"));
    SetWindowTextW(account_remember,T(L"Remember me",L"Beni hatırla"));
    Tip(account_remember,L"Signs you in automatically next time.",L"Sonraki açılışta otomatik giriş yapar.");
    SetWindowTextW(account_forgot,T(L"Forgot password?",L"Şifremi unuttum"));

    Tip(network_server,L"HTTPS address of the community Online service.",L"Topluluk Online hizmetinin HTTPS adresi.");
  }
