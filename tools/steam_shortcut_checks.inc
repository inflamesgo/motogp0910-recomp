// Synthetic binary fixtures only. Never reads or writes a real Steam account.
void CheckSteamShortcuts(){
  using namespace launcher::steam;
  const std::string exe="\"C:\\Games\\MotoGP\\MotoGP09-10Launcher.exe\"";
  const std::string directory="\"C:\\Games\\MotoGP\\\"";
  CheckUI(ShortcutId("123456789","")==0xCBF43926u,"Steam shortcut IDs use the standard CRC32 with the high bit set");
  auto first=AddShortcut({},exe,directory,"launcher.ico");
  CheckUI(AddShortcut(first,exe,directory,"launcher.ico")==first,"Adding the same launcher twice is idempotent");

  auto tree=Reader(first).Read();auto* list=Find(tree,"shortcuts");
  CheckUI(list&&list->children.size()==1,"A new VDF contains one launcher shortcut");
  auto& ours=list->children.front().children;
  SetText(ours,"LaunchOptions","--user-option");SetInt(ours,"LastPlayTime",12345);
  auto* tags=Find(ours,"tags");SetText(tags->children,"0","Favorites");
  // Raw float, color, pointer, wide-string and uint64 fields must survive.
  for(auto type:{3,4,5,6,7}){
    Value raw{static_cast<uint8_t>(type),"raw"+std::to_string(type)};
    if(type==5)raw.data={0x30,0x01,0,0};
    else raw.data.assign(type==7?8:4,0x53);
    ours.push_back(std::move(raw));
  }
  const auto original_ours=Write(ours);
  Value unrelated{0,"9"};SetText(unrelated.children,"AppName",std::string(kName));
  SetText(unrelated.children,"Exe","\"D:\\Another installation\\launcher.exe\"");
  SetText(unrelated.children,"LaunchOptions","retain me");
  const auto original_other=Write(unrelated.children);
  list->children.push_back(std::move(unrelated));
  const auto fixture=Write(tree);
  CheckUI(Write(Reader(fixture).Read())==fixture,"Binary KeyValues roundtrip preserves every raw field byte");
  auto updated=Reader(AddShortcut(fixture,exe,directory,"launcher.ico")).Read();
  auto* entries=Find(updated,"shortcuts");
  CheckUI(entries&&entries->children.size()==2,"An unrelated shortcut with the same display name is preserved");
  CheckUI(Write(entries->children[0].children)==original_ours,"User tags, launch options, history and raw launcher fields survive");
  CheckUI(Write(entries->children[1].children)==original_other,"Unrelated shortcut fields remain byte exact");
  CheckUI(entries->children[0].key=="0"&&entries->children[1].key=="1","Shortcut entries use consecutive Steam indices");

  for(size_t size=1;size<fixture.size();++size){
    bool rejected=false;
    try{Reader(std::span<const uint8_t>(fixture.data(),size)).Read();}catch(const std::exception&){rejected=true;}
    CheckUI(rejected,"Every truncated binary fixture is rejected");
  }
  auto invalid=fixture;invalid[0]=9;bool rejected=false;
  try{AddShortcut(invalid,exe,directory,"launcher.ico");}catch(const std::exception&){rejected=true;}
  CheckUI(rejected,"Unknown binary field types fail without producing replacement bytes");
  invalid=fixture;invalid.push_back(0);rejected=false;
  try{Reader(invalid).Read();}catch(const std::exception&){rejected=true;}
  CheckUI(rejected,"Trailing binary data is rejected");
  auto duplicate=Reader(first).Read();auto* shortcuts=Find(duplicate,"shortcuts");
  shortcuts->children.push_back(shortcuts->children.front());rejected=false;
  try{AddShortcut(Write(duplicate),exe,directory,"launcher.ico");}catch(const std::exception&){rejected=true;}
  CheckUI(rejected,"Duplicate launcher entries fail instead of deleting user data");
}
