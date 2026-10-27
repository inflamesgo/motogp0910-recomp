// Included in the isolated production-panel probe; no public HTTP or game data.
int FriendToastChecks(const char* font_path,const char* output) {
  const std::filesystem::path out=output;
  Check(!std::filesystem::exists(out),"Friend notification uses a fresh fixture");std::filesystem::create_directories(out);
  rex::cvar::SetFlagByName("network_mode","online");
  rex::cvar::SetFlagByName("online_server","https://motogp.nobkonline.com");
  TestContext context;auto window=rex::ui::Window::Create(context,"Friend notification fixture");
  rex::input::InputSystem input(nullptr);
  auto gate=std::make_shared<motogp::settings::InputGate>();gate->Initialize(&input);gate->SetFocused(true);
  motogp::settings::SetGuestGate(nullptr,nullptr,gate.get());
  rex::ui::ImGuiDrawer drawer(window.get(),100);auto& io=drawer.GetIO();
  io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={1920,1080};io.DeltaTime=1.f/60;
  const ImWchar ranges[]{0x20,0x1ff,0};
  auto* font=io.Fonts->AddFontFromFileTTF(font_path,48,nullptr,ranges);io.FontDefault=font;
  font->Flags|=ImFontFlags_LockBakedSizes;
  motogp::nobk::ConfigureFonts(io.Fonts,font_path,font_path,font_path);
  int errors=0;ImGui::GetCurrentContext()->ErrorCallback=[](ImGuiContext*,void* value,const char*){++*static_cast<int*>(value);};
  ImGui::GetCurrentContext()->ErrorCallbackUserData=&errors;
  unsigned char* pixels;int width,height;CaptureDrawer textures;rex::system::AchievementManager manager;
  io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);io.Fonts->SetTexID(1);
  auto preferences=std::make_shared<Preferences>();preferences->sound=false;
  std::unique_ptr<rex::ui::ImGuiDialog> menu;
  auto reset=[&]{menu.reset();motogp::nobk::Shutdown();gate->PollGuest();
    motogp::nobk::Initialize(nullptr,gate);motogp::nobk::SetVisualFixture(false);
    menu=motogp::nobk::CreateMenu(&drawer,nullptr,&manager,font,font,false,&textures,nullptr,preferences);};
  auto frame=[&]{gate->PollGuest();drawer.GetIO();
    Check(io.Fonts->IsBuilt(),"Friend notifications cannot invalidate the once-uploaded font atlas");
    ImGui::NewFrame();menu->Draw();ImGui::Render();
    Check(io.Fonts->IsBuilt()&&errors==0,"Friend notification preserves the static atlas without recovered ImGui errors");};
  auto frames=[&](int count){for(int i=0;i<count;++i)frame();};
  const char* roster=R"({"friends":[{"username":"Alice"},{"username":"Bob"}]})";
  const char* alice=R"({"online":[{"name":"Alice"}],"rooms":[]})";
  const char* both=R"({"online":[{"name":"Alice"},{"name":"Bob"}],"rooms":[]})";
  reset();motogp::nobk::ApplyVisualPresence(alice,roster);frames(30);
  Check(ImGui::GetDrawData()->TotalVtxCount==0,"First presence snapshot seeds online friends without notifications");
  motogp::nobk::ApplyVisualPresence(both,roster);frame();Dump(out/"entering");frames(24);Dump(out/"steady");
  Check(ImGui::GetDrawData()->TotalVtxCount>0&&!gate->Blocking()&&!io.WantCaptureKeyboard&&!io.WantCaptureMouse,
        "Actual offline-to-online event draws a passive friend notification");
  motogp::nobk::ApplyVisualPresence(both,roster);frames(340);
  Check(ImGui::GetDrawData()->TotalVtxCount>0,"Friend notification keeps the native nine-second lifetime instead of the old six-second toast");Dump(out/"progress");
  frames(160);Dump(out/"exit");frames(80);
  Check(ImGui::GetDrawData()->TotalVtxCount==0,"Identical snapshots cannot queue duplicate notifications after expiry");
  motogp::nobk::ApplyVisualPresence(R"({"online":[],"rooms":[],"stale":true})",roster);
  motogp::nobk::ApplyVisualPresence(both,roster);frames(24);
  Check(ImGui::GetDrawData()->TotalVtxCount==0,"Stale live data cannot create a false offline/online transition");
  motogp::nobk::ApplyVisualPresence("{}","{}",false,false);
  motogp::nobk::ApplyVisualPresence(both,roster);frames(24);
  Check(ImGui::GetDrawData()->TotalVtxCount==0,"Failed polling preserves the last trusted presence baseline");
  motogp::nobk::ApplyVisualPresence(R"({"online":[{"name":"ALICE"},{"name":"bOb"}],"rooms":[]})",roster);frames(24);
  Check(ImGui::GetDrawData()->TotalVtxCount==0,"ASCII account case changes cannot create duplicate presence events");
  motogp::nobk::ApplyVisualPresence(alice,roster);frame();
  motogp::nobk::ApplyVisualPresence(both,roster);frames(24);
  Check(ImGui::GetDrawData()->TotalVtxCount>0,"A genuine disconnect and reconnect may produce a new notification");
  reset();motogp::nobk::ApplyVisualPresence(both,"{}",true,false);frames(24);
  motogp::nobk::ApplyVisualPresence(both,roster);frames(24);
  Check(ImGui::GetDrawData()->TotalVtxCount==0,"A delayed first friends list cannot announce accounts that were already online");
  for(int scenario=0;scenario<3;++scenario){
    rex::cvar::SetFlagByName("network_mode",scenario==0?"offline":scenario==1?"lan":"online");
    rex::cvar::SetFlagByName("online_server",scenario==2?"https://custom.invalid":"https://motogp.nobkonline.com");
    reset();motogp::nobk::ApplyVisualPresence(alice,roster);
    motogp::nobk::ApplyVisualPresence(both,roster);frames(30);
    Check(ImGui::GetDrawData()->TotalVtxCount==0,
          "Offline, LAN and custom services suppress even an injected friend-online event");
  }
  std::string long_text;for(int i=0;i<70;++i)long_text+="Çağrı";
  const auto long_layout=motogp::nobk::friend_toast::Measure(font,io.DisplaySize,.5f,long_text);
  Check(long_layout.lines.size()==5&&long_layout.lines.back().ends_with("..."),"Oversized Unicode text is bounded to five readable lines");
  for(const auto& line:long_layout.lines){
    for(const char* p=line.data();p<line.data()+line.size();){unsigned int c=0;const int n=ImTextCharFromUtf8(&c,p,line.data()+line.size());
      Check(n>0&&c!=IM_UNICODE_CODEPOINT_INVALID,"Wrapping never splits a UTF-8 character");p+=n;}
  }
  for(auto size:{ImVec2(1280,720),ImVec2(2560,1080),ImVec2(320,240)}){
    const auto layout=motogp::nobk::friend_toast::Measure(font,size,.5f,"Bob is online in MotoGP 09/10");
    Check(layout.minimum.x>=0&&layout.maximum.x<size.x&&layout.maximum.y<size.y,"Notification card fits narrow and ultrawide viewports");
  }
  const auto a=motogp::nobk::friend_toast::Measure(font,io.DisplaySize,.15f,"Bob is online in MotoGP 09/10");
  const auto b=motogp::nobk::friend_toast::Measure(font,io.DisplaySize,.5f,"Bob is online in MotoGP 09/10");
  const auto c=motogp::nobk::friend_toast::Measure(font,io.DisplaySize,8.85f,"Bob is online in MotoGP 09/10");
  Check(a.minimum.x>b.minimum.x&&a.alpha<b.alpha&&c.minimum.x==b.minimum.x&&c.alpha<b.alpha&&c.progress<b.progress,
        "Native entrance slides/fades, exit only fades, and the lifetime line shrinks");
  const auto named=motogp::nobk::friend_toast::Measure(font,io.DisplaySize,.5f,"Playing MotoGP 09/10",long_text);
  Check(named.name.ends_with("...")&&font->CalcTextSizeA(30*named.scale,FLT_MAX,0,named.name.c_str()).x<=270*named.scale,
        "Long account names remain in their own bounded column above the activity");
  std::vector<uint8_t> photo(64*32*4,180);for(size_t i=3;i<photo.size();i+=4)photo[i]=255;
  auto texture=textures.CreateTexture(64,32,rex::ui::ImmediateTextureFilter::kLinear,false,photo.data());
  drawer.GetIO();Check(io.Fonts->IsBuilt(),"Localized text measurement preserves the uploaded atlas");ImGui::NewFrame();
  motogp::nobk::friend_toast::Draw(ImGui::GetForegroundDrawList(),font,io.DisplaySize,.5f,"Çağrı",long_text,
      reinterpret_cast<ImTextureID>(texture.get()),ImVec2(.25f,0),ImVec2(.75f,1),"ARKADAŞ ÇEVRİMİÇİ");
  ImGui::Render();Check(io.Fonts->IsBuilt()&&errors==0,"Photo and localized text submit valid geometry with the original atlas");Dump(out/"photo-and-wrapped-text");
  drawer.GetIO();ImGui::NewFrame();
  motogp::nobk::friend_toast::Draw(ImGui::GetForegroundDrawList(),font,io.DisplaySize,2.f,"ApexRider","Playing MotoGP 09/10");
  ImGui::Render();Check(io.Fonts->IsBuilt()&&errors==0,"Compact pit-board preview preserves the static atlas");Dump(out/"compact-preview");
  menu.reset();motogp::nobk::Shutdown();gate->PollGuest();motogp::settings::SetGuestGate(nullptr);
  std::cout<<"Friend notification presence, renderer and lifetime checks passed\n";return 0;
}
