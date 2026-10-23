// Exercises production parsing and ImGui drawing without contacting the service.
#include "../src/gamer_card.cpp"
#include <imgui_internal.h>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <limits>

namespace motogp {
std::mutex& InputStateMutex(){static std::mutex mutex;return mutex;}
namespace quit {bool Pending(){return false;}}
}
namespace {
uint32_t observed_view=0;
int checks=0;
void Check(bool condition,const char* message) {
  if(!condition)throw std::runtime_error(message);
  ++checks;
}
template<class F> void Reject(F&& action,const char* message) {
  bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}
  Check(rejected,message);
}
struct Texture final:rex::ui::ImmediateTexture {Texture(uint32_t w,uint32_t h):ImmediateTexture(w,h){}};
void Dump(const std::filesystem::path& path,const Texture& helmet,const Texture& map) {
  using Json=nlohmann::json;
  auto* data=ImGui::GetDrawData();unsigned char* pixels;int width,height;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
  std::ofstream atlas(path.string()+".rgba",std::ios::binary);
  atlas.write(reinterpret_cast<char*>(pixels),size_t(width)*height*4);
  Json capture={{"size",{data->DisplaySize.x,data->DisplaySize.y}},{"texture",{width,height}},
      {"helmet",uintptr_t(&helmet)},{"map",uintptr_t(&map)},{"lists",Json::array()}};
  for(int l=0;l<data->CmdListsCount;++l) {
    const auto* list=data->CmdLists[l];Json item={{"v",Json::array()},{"i",Json::array()},{"cmd",Json::array()}};
    for(const auto& v:list->VtxBuffer)item["v"].push_back({v.pos.x,v.pos.y,v.uv.x,v.uv.y,v.col});
    for(const auto index:list->IdxBuffer)item["i"].push_back(index);
    for(const auto& c:list->CmdBuffer)item["cmd"].push_back({c.ClipRect.x,c.ClipRect.y,c.ClipRect.z,c.ClipRect.w,
        c.IdxOffset,c.ElemCount,c.VtxOffset,c.GetTexID()});
    capture["lists"].push_back(std::move(item));
  }
  std::ofstream(path.string()+".json")<<capture;
}
}
REX_EXTERN(__imp__sub_82303260) {(void)ctx;(void)base;observed_view=motogp::gamer_card::selected_view;}

int main(int argc,char** argv) {try {
  using namespace motogp::gamer_card;
  using Json=nlohmann::json;
  Check(argc==3,"Supply packaged font and evidence directory");
  const std::filesystem::path output=argv[2];std::filesystem::create_directories(output);
  Json players={{"players",Json::array()}};
  for(const auto row:std::array<std::array<int,4>,5>{{{1,4,4,90},{2,2,4,100},{3,3,5,1},{4,1,3,1},{5,5,4,100}}})
    players["players"].push_back({{"id",row[0]},{"pos",row[1]},{"firsts",row[2]},{"points",row[3]}});
  Json boards={{"views",Json::array()}};
  for(const auto row:std::array<std::array<int,2>,13>{{{1,4},{2,4},{3,4},{43,3},{44,3},{22,2},{23,2},{45,1},{46,1},{24,5},{25,5},{19,5},{18,5}}})
    boards["views"].push_back({{"view",row[0]},{"best",{{"id",row[1]},{"columns",{{"13",1}}}}}});
  const auto ranking=ParseRanking(players,boards);
  const uint32_t expected[]{3,5,2,1,4};
  for(size_t i=0;i<5;++i)Check(ranking->players[i].id==expected[i],"Website order prioritizes records held, then tracks including DLC, points and source position");
  Check(ranking->players[1].tracks.size()==3,"Website rank includes DLC circuits and excludes reserved track slot 17 without filtering season");
  auto invalid=players;invalid["players"][1]["id"]=1;
  Reject([&]{ParseRanking(invalid,boards);},"Duplicate rider IDs cannot create a misleading rank");
  invalid=players;invalid["players"][0]["points"]=-1;
  Reject([&]{ParseRanking(invalid,boards);},"Invalid rank metrics are rejected atomically");
  invalid=players;invalid["players"][0]["pos"]=2;
  Reject([&]{ParseRanking(invalid,boards);},"Duplicate source positions are rejected");
  invalid=boards;invalid["views"].push_back(invalid["views"][0]);
  Reject([&]{ParseRanking(players,invalid);},"Duplicate board IDs cannot inflate held tracks");
  Reject([&]{ParseRanking(Json{},boards);},"Missing ranking data is unavailable rather than fabricated");

  Json identity={{"records",Json::array()}};
  const auto add=[&](int view,int rank,int season,const char* category) {
    identity["records"].push_back({{"view",view},{"rank",rank},{"total",6},{"class",category},
        {"updated",1792670600},{"columns",{{"6",2},{"13",season}}},{"display",{{"text","1:44.83"}}}});
  };
  for(int track=0;track<16;++track)add(43+track,1,0,"MotoGP");
  add(1,1,0,"125cc");add(59,2,0,"MotoGP");add(22,3,0,"250cc");
  auto results=ReadResults(identity,1);
  Check(results&&results->rows.size()==19,"All published fixture boards are parsed");
  auto categories=Categories(results.get(),false);
  auto rows=OrderedRecords(*results,43,categories,0);
  const auto summary=SummarizeRecords(rows);
  Check(summary.tracks_held==16&&summary.records_held==17&&summary.boards_entered==19,"Distinct tracks, records held and boards entered have different correct meanings");
  Check(rows.front()->view==43,"The exact selected board precedes other circuit/category results");
  add(61,1,1,"MotoGP");
  auto mixed=ReadResults(identity,1);
  Check(OrderedRecords(*mixed,0,Categories(mixed.get(),false),0).size()==19,"Temporary 2009 scope excludes known 2010 submissions");
  Check(kTracks[18]=="Silverstone"&&kTracks[19]=="Shanghai"&&kTracks[20]=="Istanbul Park","DLC slot indices preserve the reserved hole");
  Check(AvatarPath("https://www.nobkonline.com/api/avatar/u/test?v=2")=="/api/avatar/u/test?v=2","Versioned public avatar paths retain their query");
  Check(!AvatarPath("https://www.nobkonline.com.evil.invalid/api/avatar/u/test"),"Avatar origin lookalikes are rejected");
  Check(!AvatarPath("/api/avatar/../secret"),"Avatar traversal is rejected");
  std::string long_bio;for(int i=0;i<300;++i)long_bio+="ş";
  Check(ReadBio(Json{{"bio",long_bio}})==long_bio,"Bio limit counts Unicode characters rather than UTF-8 bytes");
  Check(ReadBio(Json{{"bio",long_bio+"ş"}}).empty(),"Overlong bios are rejected");
  Check(ReadBio(Json{{"bio"," \r\nA\tB\nC "}})=="A B\nC","Bio whitespace is normalized without losing paragraphs");
  Check(ReadBio(Json{{"bio","Life is a gimmick 🃏"}})=="Life is a gimmick 🃏","Public bio preserves non-BMP emoji before display conversion");
  Check(SetTime(0,false)=="-"&&SetTime(4102444800LL,false)=="-","Invalid timestamps cannot invent a set date");
  Check(Mode(0x10000002,false)=="Championship"&&Mode(14,false)=="Online","Stored native mode bits are preserved");
  {
    PublicClient client;std::stop_source stop;stop.request_stop();bool cancelled=false;
    try{client.Get("/api/motogp/ranking",256*1024,1,"ranking",stop.get_token());}
    catch(const RequestCancelled&){cancelled=true;}
    Check(cancelled,"A pre-cancelled lookup stops before any network request");
    rex::input::InputSystem input(nullptr);motogp::settings::InputGate settings,card;
    settings.Initialize(&input);card.Initialize(&input);settings.SetFocused(true);card.SetFocused(true);
    motogp::settings::SetGuestGate(&settings,&card);
    for(int i=0;i<30;++i) {
      std::thread first([&]{settings.Open();}),second([&]{card.Open();});first.join();second.join();
      Check(settings.IsOpen()!=card.IsOpen(),"Concurrent F1/card opening acquires only one input gate");
      settings.Close();card.Close();Check(motogp::settings::GuestInputBlocked(),"Closing retains the guest blocker until a neutral input sample");
      motogp::settings::PollGuestInput();Check(!motogp::settings::GuestInputBlocked(),"Neutral input releases both menu blockers completely");
    }
    card.Open();card.SetFocused(false);card.Close();motogp::settings::PollGuestInput();
    Check(motogp::settings::GuestInputBlocked(),"Focus loss cannot prematurely release the game's input blocker");
    card.SetFocused(true);motogp::settings::PollGuestInput();
    Check(!motogp::settings::GuestInputBlocked(),"Refocusing and observing neutral input restores gameplay");
    motogp::settings::SetGuestGate(nullptr);
  }

  auto* context=ImGui::CreateContext();auto& io=ImGui::GetIO();
  motogp::overlay::ConfigureDialogs(ImGui::GetStyle());
  io.IniFilename=nullptr;io.LogFilename=nullptr;io.DeltaTime=1.f/60;
  auto* font=io.Fonts->AddFontFromFileTTF(argv[1],17);
  ConfigureFonts(io.Fonts,argv[1]);
  Check(font&&std::all_of(profile_fonts.begin(),profile_fonts.end(),[](auto* value){return value!=nullptr;}),"Every prepared native profile font loads");
  unsigned char* pixels;int tw,th;io.Fonts->GetTexDataAsRGBA32(&pixels,&tw,&th);io.Fonts->SetTexID(1);
  Check(tw<=8192&&th<=8192,"The preloaded font atlas fits ordinary GPU texture limits");
  Check(emoji_aliases.size()>5000,"Embedded catalogue supports general emoji sequences");
  for(const auto& [sequence,glyph]:emoji_aliases) {
    const auto display=EmojiText(sequence);unsigned int code=0;
    Check(ImTextCharFromUtf8(&code,display.data(),display.data()+display.size())==3&&display.size()==3&&code==glyph,"Every embedded emoji sequence resolves to its exact shared sprite");
  }
  const auto sprite_png=Embedded(202);int sprite_width=0,sprite_height=0;
  const auto sprites=rex::ui::DecodeImageRGBA(sprite_png.data(),sprite_png.size(),sprite_width,sprite_height);
  for(const auto* sample:{"🃏","😀","❤️","👍🏽","🇹🇷","👨‍👩‍👧‍👦","1️⃣"}) {
    const auto display=EmojiText(sample);
    unsigned int alias=0;
    Check(ImTextCharFromUtf8(&alias,display.data(),display.data()+display.size())==3&&display.size()==3&&alias>=0xE000&&alias<0xF900,"An entire emoji cluster maps to one BMP display glyph");
    for(size_t i=0;i<profile_fonts.size();++i) {
      const auto* glyph=profile_fonts[i]->GetFontBaked(float(kFontSizes[i]))->FindGlyphNoFallback(ImWchar(alias));
      Check(glyph&&glyph->Colored&&glyph->Visible,"Every native profile font contains the colored emoji glyph");
      bool identical=true,visible=false;
      const auto sprite=alias-0xE000;
      for(int y=0;y<32;++y)for(int x=0;x<32;++x) {
        const auto* actual=pixels+(size_t(int(glyph->V0*th)+y)*tw+int(glyph->U0*tw)+x)*4;
        const auto* expected=sprites.data()+(size_t(sprite/64*32+y)*sprite_width+sprite%64*32+x)*4;
        identical&=std::memcmp(actual,expected,4)==0;visible|=actual[3]>100;
      }
      Check(identical&&visible,"Emoji UVs preserve every original sprite pixel, including legitimate monochrome family artwork");
    }
  }
  Check(EmojiText("Life is a gimmick 🃏").starts_with("Life is a gimmick "),"Emoji conversion preserves surrounding text");
  Check(ReadHelmet()!=nullptr&&ReadFlags()!=nullptr,"Embedded helmet and flag resources decode in the actual Windows build");
  Texture helmet(1254,1254),map(360,261);
  Snapshot state;state.status=Status::Ready;state.player_id=4;state.ranking=ranking;state.ranking_status=Status::Ready;
  state.player.name="erutoru";state.player.member_since=1790812800;state.results=results;state.preferred_view=43;state.map_track=0;
  for(const auto size:{ImVec2(1280,720),ImVec2(1920,1080),ImVec2(2560,1080),ImVec2(3840,2160),ImVec2(640,480)})
  for(const bool tr:{false,true})for(const bool bio:{false,true})for(int tab=0;tab<2;++tab) {
    io.DisplaySize=size;state.player.bio=bio?long_bio:std::string{};View view;view.tab=tab;
    for(int frame=0;frame<2;++frame) {ImGui::NewFrame();auto action=DrawCard(state,nullptr,nullptr,&helmet,&map,view,tr,font,nullptr,true,1);Check(!action.close&&!action.profile,"Drawing never triggers a spontaneous action");ImGui::Render();}
    auto* window=ImGui::FindWindowByName("##motogp_player_card");
    Check(window&&window->Pos.x>=0&&window->Pos.y>=0&&window->Pos.x+window->Size.x<=size.x+1&&window->Pos.y+window->Size.y<=size.y+1,"License remains inside each tested viewport");
    Check(window->ScrollMax.y==0,"The fixed card never gains an outer scrollbar");
    Check(!context->ColorStack.Size&&!context->StyleVarStack.Size&&!context->FontStack.Size,"Tabs do not leak style or font state");
    const float scale=std::max(.1f,std::min({size.x/1440.f,size.y/900.f,1.35f}));
    Canvas canvas{window->DrawList,window->Pos,scale,font};
    float lo=FLT_MAX,hi=-FLT_MAX;const auto color=ImGui::GetColorU32(kMuted);
    for(const auto& v:window->DrawList->VtxBuffer)if(v.col==color&&v.pos.y>=canvas.Point(0,541).y&&v.pos.y<=canvas.Point(0,561).y) {lo=std::min(lo,v.pos.x);hi=std::max(hi,v.pos.x);}
    Check(lo<hi&&std::abs((lo+hi)*.5f-canvas.Point(680,0).x)<3,"Footer's rendered glyph group keeps the same center across tabs and bio states");
    const auto excerpt=Excerpt(canvas,long_bio,152);
    Check(canvas.Font(15.5f).size<=19.f*scale,"Bio font fits its real baseline spacing at the smallest supported viewport");
    Check(canvas.Font(15).size<=23.f*scale,"Records title and class lines cannot overlap at a small viewport");
    Check(excerpt.more&&excerpt.lines.size()==3,"A 300-character unbroken Unicode bio has a bounded three-line preview");
    for(const auto& line:excerpt.lines)Check(Json::parse(Json(line).dump())==line,"Preview truncation never splits UTF-8 characters");
    if(size.x==1920&&size.y==1080&&bio)
      Dump(output/(std::string(tr?"turkish-":"english-")+(tab?"records":"overview")),helmet,map);
  }
  io.DisplaySize={1920,1080};state.player.name="mrwraith";
  state.player.bio="Life is a gimmick 🃏\n😀 ❤️ 👍🏽 🇹🇷 👨‍👩‍👧‍👦 1️⃣";
  View view;
  for(int frame=0;frame<2;++frame){ImGui::NewFrame();DrawCard(state,nullptr,nullptr,&helmet,&map,view,false,font,nullptr,true,1);ImGui::Render();}
  Dump(output/"emoji-overview",helmet,map);
  state.player.bio+="\n"+long_bio;view.bio_open=true;
  for(int frame=0;frame<2;++frame){ImGui::NewFrame();DrawCard(state,nullptr,nullptr,&helmet,&map,view,false,font,nullptr,true,1);ImGui::Render();}
  Check(ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel),"Overflow bio opens a real modal");
  Dump(output/"emoji-modal",helmet,map);
  view.bio_close=true;ImGui::NewFrame();DrawCard(state,nullptr,nullptr,&helmet,&map,view,false,font,nullptr,true,1);ImGui::Render();
  Check(!view.bio_open&&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel),"Closing bio consumes only the modal state");
  ImGui::DestroyContext(context);

  auto* base=static_cast<uint8_t*>(VirtualAlloc(nullptr,0x84000000ull,MEM_RESERVE,PAGE_NOACCESS));
  Check(base&&VirtualAlloc(base+0x83336000,4096,MEM_COMMIT,PAGE_READWRITE),"Reserve isolated native-selector fixture");
  PPCContext ctx{};selected_view=123;
  for(const uint32_t selection:{0u,43u,378u,379u}) {
    rex::memory::store_and_swap<uint32_t>(base+0x833362D8,selection);ctx.lr=0x8223EDD4;sub_82303260(ctx,base);
    Check(observed_view==(selection>=1&&selection<=378?selection:0)&&selected_view==123,"Only valid leaderboard context is forwarded and temporary context is restored");
    ctx.lr=0x80000000;sub_82303260(ctx,base);Check(observed_view==0&&selected_view==123,"Lobby never inherits the previous leaderboard");
  }
  VirtualFree(base,0,MEM_RELEASE);
  std::cout<<checks<<" Rider License data, native-context and ImGui checks passed\n";
  return 0;
}catch(const std::exception& error){std::cerr<<"Rider License probe failed after "<<checks<<": "<<error.what()<<'\n';return 1;}}
