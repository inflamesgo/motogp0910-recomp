#pragma once
#include <cstdint>

namespace motogp::discord {
enum class Menu : uint8_t {
  Unknown, Main, Arcade, Career, Championship, TimeTrial, Multiplayer,
  Network, LAN, SplitScreen, Settings, Leaderboards, Selection, Lobby, Race
};
// Verified against the patched title's RTTI and native screen manager.
// Explicit types only: nearby addresses are not assumed to be UI screens.
inline Menu IdentifyMenu(uint32_t vtable) {
  static constexpr struct {uint32_t vtable;Menu menu;} screens[]{
    {0x8200CF7C,Menu::Main}, // MainMenu
    {0x82017284,Menu::Arcade}, // ArcadeBikeMenu
    {0x82017374,Menu::Arcade}, // ArcadeClassMenu
    {0x8201789C,Menu::Arcade}, // ArcadeMainMenu
    {0x82017B2C,Menu::Arcade}, // ArcadeTrackMenu
    {0x8201841C,Menu::Arcade}, // ArcadeTrackSeason
    {0x820186B4,Menu::Arcade}, // ArcadeTransmissionMenu
    {0x82018B74,Menu::Career}, // EventCalendarScreen
    {0x8201B77C,Menu::Career}, // CareerBranding
    {0x8201C054,Menu::Career}, // CareerCareerMenu
    {0x8201C7F4,Menu::Career}, // CareerDecision
    {0x8201CDFC,Menu::Career}, // CareerChooseHomeTrack
    {0x8201D194,Menu::Career}, // CareerCreateRider
    {0x8201DEDC,Menu::Career}, // CareerCreateTeam
    {0x8201E8C4,Menu::Career}, // CareerNewCareerScreen
    {0x8201EF2C,Menu::Career}, // CareerRaceDifficulty
    {0x8201F9AC,Menu::Career}, // CareerResearch
    {0x8202027C,Menu::Career}, // CareerSelectNationality
    {0x820236D4,Menu::Career}, // CareerBikeManufacturer
    {0x820248DC,Menu::Career}, // CareerResearchApplyStaff
    {0x82026314,Menu::Career}, // CareerResearchBrakesTyres
    {0x82026364,Menu::Career}, // CareerResearchEngine
    {0x820263B4,Menu::Career}, // CareerResearchExhaustFilters
    {0x82026404,Menu::Career}, // CareerResearchFrameSuspension
    {0x82026454,Menu::Career}, // CareerResearchGearbox
    {0x82026F04,Menu::Career}, // CareerChooseStaffType
    {0x820274DC,Menu::Career}, // CareerListOfStaff
    {0x8202837C,Menu::Career}, // CareerManageStaff
    {0x82028E5C,Menu::Career}, // CareerChooseColor
    {0x820297A4,Menu::Career}, // CareerTeamAppearance
    {0x8202A134,Menu::Career}, // CareerTeamHelmetDesign
    {0x8202A854,Menu::Career}, // CareerTeamLeatherDesign
    {0x8202AFA4,Menu::Career}, // CareerTeamLiveryDesign
    {0x8202B944,Menu::Career}, // CareerTeamOffer
    {0x8202C254,Menu::Career}, // CareerTeamRiderNumber
    {0x8202C844,Menu::Career}, // CareerTeamSponsorOffer
    {0x8202DCDC,Menu::Career}, // CareerTeamSponsorship
    {0x8203008C,Menu::TimeTrial}, // TimetrialChooseBike
    {0x82030184,Menu::TimeTrial}, // TimetrialChooseClass
    {0x8203046C,Menu::TimeTrial}, // TimetrialChooseTrack
    {0x8203107C,Menu::TimeTrial}, // TimetrialChooseTransmission
    {0x82031634,Menu::Settings}, // OptionsControls
    {0x8203208C,Menu::Settings}, // OptionsCredits
    {0x82032784,Menu::Settings}, // OptionsDevDiaries
    {0x82032DFC,Menu::Settings}, // OptionsDisplay
    {0x82033544,Menu::Settings}, // OptionsExtras
    {0x820338B4,Menu::Settings}, // OptionsGameSettings
    {0x8203411C,Menu::Settings}, // OptionsOptions
    {0x8203448C,Menu::Settings}, // OptionsSounds
    {0x82034D2C,Menu::Network}, // MultiplayerChoosebike
    {0x82035344,Menu::Network}, // MultiplayerChooseTransmission
    {0x82035884,Menu::Network}, // MultiplayerCreateMatch
    {0x82035E0C,Menu::Network}, // MultiplayerCustomMatch
    {0x8203639C,Menu::Leaderboards}, // MultiplayerLeaderboardClass
    {0x82036974,Menu::Leaderboards}, // MultiplayerLeaderboardSelect
    {0x82036F14,Menu::Leaderboards}, // MultiplayerLeaderboards
    {0x8203799C,Menu::Lobby}, // MultiplayerLobby
    {0x82038EAC,Menu::Lobby}, // MultiplayerLobbySettings
    {0x82039434,Menu::Network}, // MultiplayerLogin
    {0x82039E5C,Menu::Network}, // MP_MainMenu
    {0x8203A0C4,Menu::Multiplayer}, // MultiplayerMultiplayer
    {0x8203A60C,Menu::Network}, // MultiplayerNetBusy
    {0x8203AAEC,Menu::Network}, // MultiplayerOnline
    {0x8203AFBC,Menu::Network}, // MultiplayerRankedUnRanked
    {0x8203B41C,Menu::Network}, // MultiplayerSearchResults
    {0x8203BE3C,Menu::Network}, // MultiplayerSettingsBase
    {0x8203C464,Menu::SplitScreen}, // MP_Splitscreen_CreateMatch
    {0x8203CF24,Menu::SplitScreen}, // SplitscreenChooseTrack
    {0x8203CF6C,Menu::SplitScreen}, // MP_Splitscreen_GameMode
    {0x8203CFB4,Menu::SplitScreen}, // MP_Splitscreen_RaceMode
    {0x8203D7FC,Menu::LAN}, // MultiplayerSystemLink
    {0x8203E474,Menu::Lobby}, // MultiplayerVoteClass
    {0x8203E664,Menu::Lobby}, // MultiplayerVoteTrack
    {0x8203E76C,Menu::Lobby}, // MultiplayerVoteTracksAndLaps
    {0x8203E884,Menu::Lobby}, // MultiplayerVoteWeather
    {0x8203E954,Menu::SplitScreen}, // SplitscreenChooseBike
    {0x8203EA54,Menu::Selection}, // BaseBikePreview
    {0x8203EE1C,Menu::Selection}, // BaseBikeSelection
    {0x8203F4F4,Menu::Selection}, // BaseChooseClass
    {0x8203FB4C,Menu::Selection}, // BaseClassSelection
    {0x8203FB9C,Menu::Selection}, // BaseEndOfSeason
    {0x82040504,Menu::Selection}, // BaseSeasonSelection
    {0x82040C64,Menu::Selection}, // BaseTrackSeason
    {0x82041A04,Menu::Selection}, // BaseTrackSelection
    {0x8204223C,Menu::Selection}, // BaseTransmissionSelection
    {0x82042A14,Menu::Championship}, // VanillaMainMenu
    {0x82042F14,Menu::Championship}, // VanillaTrackSeason
    {0x820431E4,Menu::Career}, // CareerEventHistory
    {0x820439FC,Menu::Career}, // CareerEventCalendar2
    {0x82071E88,Menu::Race}, // ArcadeBonusTimeScreen
    {0x820760DC,Menu::Race}, // CareerContinueRaceScreen
    {0x820761F0,Menu::Race}, // ChampStandingScreen
    {0x82076724,Menu::Race}, // ContinueRaceScreen
    {0x820798A4,Menu::Race}, // NextRaceWeekendScreen
    {0x82079C70,Menu::Race}, // StartingLightsScreen
    {0x82079CA4,Menu::Race}, // GameQuittingScreen
    {0x82079CD8,Menu::Race}, // RaceScoreScreen
    {0x82079D0C,Menu::Race}, // SponsorPaymentScreen
    {0x82079D40,Menu::Race}, // NewSponsorScreen
    {0x82079D74,Menu::Race}, // NewManufacturerScreen
    {0x82079DA8,Menu::Race}, // EventRewardScreen
    {0x82079FD4,Menu::Race}, // PauseMenuScreen
    {0x8207A3B4,Menu::Race}, // QualifyMonitorScreen
    {0x8207AC10,Menu::Race}, // RaceLineupScreen
    {0x8207BABC,Menu::Race}, // RaceResultsScreen
    {0x8207D508,Menu::Race}, // ReplayPanelScreen
    {0x8207D990,Menu::Race}, // SpectatePanelScreen
    {0x8207FA4C,Menu::Race}, // StartingGridScreen
    {0x820803DC,Menu::Race}, // TimeTrialContinueRaceScreen
    {0x82080A84,Menu::Race}, // VanillaContinueRaceScreen
  };
  for(const auto& screen:screens)if(screen.vtable==vtable)return screen.menu;
  return Menu::Unknown;
}
template<class Reader> Menu CurrentMenu(Reader& read) {
  // Native GetScreen(8), 8235A150: highest occupied active slot, except slot 3.
  // Read active records, never the adjacent records fading out after a change.
  for(int slot=7;slot>=0;--slot) {
    if(slot==3)continue;
    const auto record=read(0x8336F4B0+uint32_t(slot)*4).value_or(0);
    const auto object=record?read(record).value_or(0):0;
    const auto vtable=object?read(object).value_or(0):0;
    const auto menu=IdentifyMenu(vtable);
    // Unrecognized native popups inherit the underlying recognized menu.
    if(menu!=Menu::Unknown)return menu;
  }
  return Menu::Unknown;
}
inline Menu MenuContext(Menu menu,Menu previous) {
  switch(menu) {
    case Menu::Race:case Menu::Unknown:case Menu::Settings:case Menu::Leaderboards:return previous;
    case Menu::Selection:return previous==Menu::Main||previous==Menu::Multiplayer?Menu::Unknown:previous;
    case Menu::Network:case Menu::Lobby:return previous==Menu::LAN?Menu::LAN:Menu::Network;
    default:return menu;
  }
}
inline bool CanShowLobby(Menu menu) {
  return menu==Menu::Lobby||menu==Menu::Unknown;
}
} // namespace motogp::discord
