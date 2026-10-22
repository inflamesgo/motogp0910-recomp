#pragma once
#include "client.h"
#include <algorithm>
#include <stdexcept>

namespace motogp::online {
enum class BoardMode : uint32_t { Any, Arcade, Career, Championship, TimeTrial };
inline thread_local BoardMode requested_board_mode=BoardMode::Any;
struct BoardModeScope {
 BoardMode previous=requested_board_mode;
 explicit BoardModeScope(BoardMode mode){requested_board_mode=mode;}
 ~BoardModeScope(){requested_board_mode=previous;}
};
inline BoardMode FrontendBoardMode(uint32_t frontend){
 switch(frontend){case 2:return BoardMode::Arcade;case 3:return BoardMode::Career;
 case 4:return BoardMode::Championship;case 5:return BoardMode::TimeTrial;default:return BoardMode::Any;}
}
inline bool MatchesBoardMode(const Row& row,BoardMode mode){
 if(mode==BoardMode::Any)return true;
 const auto found=std::find_if(row.columns.begin(),row.columns.end(),[](const Column& c){return c.id==6;});
 if(found==row.columns.end() || found->type!=1 || found->bits>UINT32_MAX)return false;
 const uint32_t bits=static_cast<uint32_t>(found->bits),race=bits&~0x10000000u;
 if(race>16)return false;
 if(mode==BoardMode::Championship)return (bits&0x10000000u)!=0;
 if(bits&0x10000000u)return false;
 switch(mode){case BoardMode::Arcade:return race==2;case BoardMode::Career:return race>=3&&race<=6;
 case BoardMode::TimeTrial:return race==10;default:return false;}
}
// Apply selection after collecting the board, before pagination and rank lookup.
// Filtering just the first ten rows loses eligible riders on later pages.
inline Page SelectModePage(const std::vector<Row>& all,BoardMode mode,uint32_t start,uint32_t count,
                          std::span<const uint64_t> users={},uint64_t around=0){
 Page page;std::vector<Row> ranked;
 for(const auto& row:all)if(MatchesBoardMode(row,mode)){
  auto copy=row;copy.rank=static_cast<uint32_t>(ranked.size()+1);ranked.push_back(std::move(copy));
 }
 page.total=static_cast<uint32_t>(ranked.size());
 if(!users.empty()){
  for(auto& row:ranked)if(std::find(users.begin(),users.end(),row.peer)!=users.end() && page.rows.size()<count)page.rows.push_back(std::move(row));
  return page;
 }
 if(around){
  const auto found=std::find_if(ranked.begin(),ranked.end(),[&](const Row& row){return row.peer==around;});
  if(found==ranked.end())return page;
  start=found->rank>count/2?found->rank-count/2:1;
 }
 const size_t offset=std::max(1u,start)-1;
 for(size_t i=offset;i<ranked.size() && page.rows.size()<count;++i)page.rows.push_back(std::move(ranked[i]));
 return page;
}
// Fetch is the existing authenticated statistics reader. Keep native upload IDs
// and the public overall leaderboard unchanged.
template<class Fetch> Page ReadModeBoard(uint32_t view,BoardMode mode,uint32_t start,uint32_t count,
                                        std::span<const uint64_t> users,uint64_t around,Fetch fetch){
 if(mode==BoardMode::Any)return fetch(view,start,count,users,around);
 const uint32_t source=mode==BoardMode::TimeTrial && view>=1 && view<=63?view+126:view;
 std::vector<Row> all;
 for(uint32_t offset=1;;offset+=100){
  auto page=fetch(source,offset,100,std::span<const uint64_t>{},uint64_t(0));
  if(page.total>10000)throw std::runtime_error("Mode leaderboard exceeds client query capacity");
  if(page.rows.empty()){
   if(all.size()<page.total)throw std::runtime_error("Incomplete mode leaderboard");
   break;
  }
  for(auto& row:page.rows)all.push_back(std::move(row));
  if(all.size()>=page.total)break;
  if(offset>=9901)throw std::runtime_error("Incomplete mode leaderboard");
 }
 return SelectModePage(all,mode,start,count,users,around);
}

}
