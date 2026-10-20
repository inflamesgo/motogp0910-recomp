// Synthetic filesystem fixtures only; no original game bytes are required.
void CheckIsoImport(App& app,const fs::path& output){
  const auto scratch=output/(L"iso-probe-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
  fs::create_directory(scratch);
  struct Cleanup {fs::path root;~Cleanup(){std::error_code error;fs::remove_all(root,error);}} cleanup{scratch};
  const auto image_path=scratch/L"fixture.iso";
  auto fixture=[](size_t partition=0){
    std::vector<uint8_t> image(partition+36*2048);auto* descriptor=image.data()+partition+32*2048;
    std::memcpy(descriptor,"MICROSOFT*XBOX*MEDIA",20);std::memcpy(descriptor+2028,"MICROSOFT*XBOX*MEDIA",20);
    auto put=[](uint8_t* target,uint32_t value){for(int i=0;i<4;++i)target[i]=uint8_t(value>>(8*i));};
    put(descriptor+20,33);put(descriptor+24,64);
    auto* node=image.data()+partition+33*2048;node[2]=8;put(node+4,34);put(node+8,4);node[13]=11;std::memcpy(node+14,"default.xex",11);
    node+=32;put(node+4,35);node[12]=0x10;node[13]=9;std::memcpy(node+14,"xenondata",9);return image;
  };
  auto write=[&](const std::vector<uint8_t>& bytes){std::ofstream file(image_path,std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!file)throw std::runtime_error("Could not write ISO fixture");};
  std::atomic<bool> cancel{false};
  auto reject=[&](std::vector<uint8_t> bytes){write(bytes);bool rejected=false;try{iso::Image image(image_path);iso::Inspect(image,cancel);}catch(const Error&){rejected=true;}CheckUI(rejected,"Malformed ISO is rejected before extraction");};
  for(size_t partition:{size_t(0),size_t(0xFB20),size_t(0x20600)}){
    write(fixture(partition));iso::Image image(image_path);const auto plan=iso::Inspect(image,cancel);
    CheckUI(plan.files.size()==1&&plan.directories.size()==1&&plan.bytes==4&&plan.files[0].offset==partition+34*2048,"XDVDFS partition offsets, file extents and empty directories are parsed");
  }
  auto damaged=fixture();damaged.resize(65550);reject(damaged);
  damaged=fixture();damaged[32*2048+20]=255;damaged[32*2048+21]=255;damaged[32*2048+22]=255;damaged[32*2048+23]=255;reject(damaged);
  damaged=fixture();damaged[33*2048+32+2]=8;reject(damaged); // Cyclic directory tree.
  damaged=fixture();damaged[33*2048+13]=255;reject(damaged);
  for(const std::string name:{"../evil.bin","CON","file:stream","ends.","bad\\path"}){
    damaged=fixture();damaged[33*2048+13]=static_cast<uint8_t>(name.size());std::memcpy(damaged.data()+33*2048+14,name.data(),name.size());reject(damaged);
  }
  damaged=fixture();damaged[33*2048+32+13]=11;std::memcpy(damaged.data()+33*2048+32+14,"DEFAULT.XEX",11);reject(damaged);
  write(fixture());cancel=true;bool cancelled=false;
  try{iso::Image image(image_path);iso::Inspect(image,cancel);}catch(const iso::Cancelled&){cancelled=true;}
  CheckUI(cancelled,"ISO inspection honors cancellation");cancel=false;
  const auto destination=scratch/L"destination";fs::create_directory(destination);fs::create_directory(destination/L"MotoGP09-10-game");
  {std::ofstream file(destination/L"MotoGP09-10-game"/L"keep.txt");file<<"keep";}
  iso::Progress progress;bool rejected=false;
  try{iso::Import(image_path,destination,scratch,{},cancel,progress);}catch(const Error&){rejected=true;}
  CheckUI(rejected&&fs::is_regular_file(destination/L"MotoGP09-10-game"/L"keep.txt")&&
      std::distance(fs::directory_iterator(destination),fs::directory_iterator{})==1,"Wrong-title ISO is rejected and staging is removed without changing an existing installation");
  const auto original=app.Text(app.path);const auto original_game=app.GameRoot();const bool language=app.turkish;app.turkish=false;app.ShowPage(3);
  const auto base=scratch/L"game";fs::create_directory(base);SetWindowTextW(app.path,base.c_str());app.RefreshGameFilesAction();
  CheckUI(app.Text(app.verify_files)==L"Select ISO"&&IsWindowVisible(app.title_update_notice),"Missing base game offers Select ISO with a visible update warning");
  fs::create_directory(base/L"xenondata");fs::copy_file(original_game/L"default.xex",base/L"default.xex");{std::ofstream file(base/L"xenondata"/L"data.bin");file<<"fixture";}
  app.RefreshGameFilesAction();CheckUI(app.Text(app.verify_files)==L"Select title update"&&app.Text(app.title_update_notice)==L"Title update is missing","Base game without TU offers the next installation step");
  {std::ofstream file(base/L"default.xexp");file<<"invalid";}
  app.RefreshGameFilesAction();CheckUI(app.Text(app.title_update_notice)==L"Title update is invalid","Present but unsupported TU is distinguished from a missing update");
  app.turkish=true;app.RefreshGameFilesAction();CheckUI(app.Text(app.verify_files)==L"Oyun güncellemesi seç"&&app.Text(app.title_update_notice)==L"Oyun güncellemesi geçersiz","Adaptive game-file action and TU warning are localized");
  app.ShowPage(0);CheckUI(!IsWindowVisible(app.title_update_notice),"TU warning stays on General");
  app.turkish=language;SetWindowTextW(app.path,original.c_str());app.ShowPage(3);app.ReadyStatus();
}
