#pragma once

#include <rex/cvar.h>
#include "keyboard_binding_specs.h"

// Called after config/environment/CLI parsing, before input initialization.
// Direct game starts and the launcher use the same MotoGP keyboard defaults.
inline void ApplyMotogpInputDefaults() {
  if (rex::cvar::GetFlagSource("mnk_mode") == rex::cvar::Source::kDefault) {
    rex::cvar::SetFlagByName("mnk_mode", "true");
  }
  for(unsigned i=0;i<launcher::kBindings.size();++i){const auto& binding=launcher::kBindings[i];
    // Upgrade known shipped presets only. Custom configuration and explicit
    // environment, CLI and live runtime values take priority, including unbinds.
    const auto source=rex::cvar::GetFlagSource(binding.key);
    if(source==rex::cvar::Source::kDefault||(source==rex::cvar::Source::kConfig&&launcher::IsLegacyDefaultBinding(i,rex::cvar::GetFlagByName(binding.key))))rex::cvar::SetFlagByName(binding.key,binding.defaults);
  }
}
