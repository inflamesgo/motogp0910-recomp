// Standard-library-only regression checks; no game, SDK, GPU or server needed.
#include "../src/online/leaderboard_filter.h"
#include <iostream>
#include <stdexcept>
using namespace motogp::online;
void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
Row Result(uint64_t peer,uint32_t mode){return {peer,static_cast<uint32_t>(peer),"rider",{{6,1,mode}}};}
int main(){try{
 const std::vector<Row> all{Result(1,10),Result(2,2),Result(3,10),Result(4,10),Result(5,10),Result(6,2),Result(7,2)};
 auto tt=SelectModePage(all,BoardMode::TimeTrial,1,10);
 Check(tt.total==4 && tt.rows.size()==4 && tt.rows[1].peer==3 && tt.rows[1].rank==2,"Time Trial uses only its four laps and mode-relative ranks");
 auto champ=SelectModePage(all,BoardMode::Championship,1,10);
 Check(champ.total==0 && champ.rows.empty(),"Unflagged Time Trial/Arcade laps cannot become Championship results");
 auto mixed=all;mixed.push_back(Result(8,0x10000005));mixed.push_back(Result(9,5));mixed.push_back(Result(10,6));
 Check(SelectModePage(mixed,BoardMode::Championship,1,10).rows[0].peer==8,"Championship flag selects Championship");
 auto career=SelectModePage(mixed,BoardMode::Career,1,10);
 Check(career.rows.size()==2 && career.rows[0].peer==9 && career.rows[1].peer==10,"Career includes GP sessions and wildcard, excludes Championship");
 auto arcade=SelectModePage(all,BoardMode::Arcade,2,1);
 Check(arcade.total==3 && arcade.rows.size()==1 && arcade.rows[0].peer==6 && arcade.rows[0].rank==2,"Pagination follows mode filtering");
 std::vector<uint64_t> users{3};
 auto own=SelectModePage(all,BoardMode::TimeTrial,1,10,users);
 Check(own.total==4 && own.rows.size()==1 && own.rows[0].rank==2,"Own-player reads retain the rank within this mode");
 Check(SelectModePage(all,BoardMode::TimeTrial,1,3,{},3).rows.size()==3,"Around-player reads use filtered ranking");
 Check(SelectModePage(all,BoardMode::TimeTrial,1,3,{},2).rows.empty(),"No invented entry when the player has no lap in the selected mode");
 Check(SelectModePage(all,BoardMode::Any,1,10).rows.size()==7,"Standalone overall leaderboard still includes every mode");
 std::vector<Row> large;for(int i=1;i<=105;++i)large.push_back(Result(i,i<=100?2:10));
 unsigned calls=0;
 auto fetched=ReadModeBoard(43,BoardMode::TimeTrial,1,10,{},0,
  [&](uint32_t view,uint32_t start,uint32_t count,std::span<const uint64_t> selected,uint64_t center){
   ++calls;Check(view==169 && selected.empty() && !center,"Read the independent native TT board without applying player pagination early");
   Page page;page.total=large.size();
   for(size_t i=start-1;i<large.size() && page.rows.size()<count;++i)page.rows.push_back(large[i]);
   return page;
  });
 Check(calls==2 && fetched.total==5 && fetched.rows.size()==5 && fetched.rows[0].rank==1,"Fetch and filter all server pages before ranking");
 bool incomplete=false;
 try{ReadModeBoard(43,BoardMode::Championship,1,10,{},0,
  [](uint32_t,uint32_t,uint32_t,std::span<const uint64_t>,uint64_t){return Page{5,{}};});}
 catch(const std::runtime_error&){incomplete=true;}
 Check(incomplete,"Truncated server response cannot masquerade as an empty mode board");
 bool oversized=false;
 try{ReadModeBoard(43,BoardMode::TimeTrial,1,10,{},0,
  [](uint32_t,uint32_t,uint32_t,std::span<const uint64_t>,uint64_t){return Page{10001,{}};});}
 catch(const std::runtime_error&){oversized=true;}
 Check(oversized,"Bounded query capacity rejects implausible totals");
 Check(!MatchesBoardMode(Result(20,UINT32_MAX),BoardMode::Arcade),"Unknown modes do not silently become Arcade");
 Check(!MatchesBoardMode(Row{},BoardMode::TimeTrial),"Missing mode metadata is excluded");
 {BoardModeScope first(BoardMode::TimeTrial);{BoardModeScope second(BoardMode::Championship);Check(requested_board_mode==BoardMode::Championship,"Nested scope");}Check(requested_board_mode==BoardMode::TimeTrial,"Restore nested scope");}
 Check(requested_board_mode==BoardMode::Any,"No filter leaks into the next overall query");
 Check(FrontendBoardMode(4)==BoardMode::Championship && FrontendBoardMode(5)==BoardMode::TimeTrial && FrontendBoardMode(99)==BoardMode::Any,"Frontend mapping");
 std::cout<<"Leaderboard mode selection, pagination, ranks and scope checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
