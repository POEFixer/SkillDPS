# Skill DPS

Current version: 1.0.2

Requires POE2Fixer 364 or newer.

A POEFixer plugin for **Path of Exile 2** that writes the game's own per-skill
**DPS numbers directly on your skill-bar icons**, so you can watch your damage
change as you swap gems, supports and gear without opening the character panel.

## What you get

- **DPS on the skill bar.** Each icon shows the evaluated DPS of the skill on
  that slot. The link is read from the slot itself, so it stays right however
  you arrange the bar, including the same skill on several slots.
- **Average-hit skills.** Skills the game presents as "average damage" instead
  of DPS (for example long-cooldown nukes) show their average hit with a `~`.
- **Totems.** A totem-placing skill shows the live DPS of the skills its totems
  cast.
- **DPS list panel (optional).** A small translucent panel listing every
  evaluated skill, minions' skills included, for numbers that do not fit on
  the bar. Drag it anywhere while the plugin's settings tab is open.

### About the numbers

The values come from the game's own skill evaluation: the same numbers the
skills panel shows. The game only evaluates the skill selected in the character
panel and skills in active use (totems, minions), so a skill without a number
simply has not been evaluated yet. Select it in the character panel or use it
once.

## Using it

Install **Skill DPS** from POEFixer's **Plugin Manager** (the first tab under
Plugins); it keeps the plugin up to date as well. Then tick **Enabled** at the
top of the plugin's own tab. The numbers appear on the skill bar as soon as you
are in game; the same tab changes what is shown.

## Settings

**What to show**

| Setting | What it does |
|---|---|
| Numbers on the skill-bar icons | Shows each skill's number on its icon. |
| DPS list panel | Shows the list panel with every evaluated skill. |
| Include minions and totems | Adds friendly minions' skills to the list and lets totem skills show their totems' damage. |
| Update every | How often the numbers are read from the game (50-1000 ms, default 250 ms). |

**Icon numbers**

| Setting | What it does |
|---|---|
| Size | Text size relative to POEFixer's font (70-180 %). |
| Background | Opacity of the dark plate behind each number; 0 % leaves the number with its dark outline only. |
| Position | Top or bottom of the icon. |
| DPS colour / Average colour | Colours of DPS numbers and of `~` average-hit numbers. |

**DPS list panel**

| Setting | What it does |
|---|---|
| Size | Text size of the panel (70-180 %). |
| Background | Opacity of the panel background. |
| Reset position | Moves the panel back to the left side of the screen. |

**Diagnostics** (collapsed) shows the live skill numbers, the detected bar slots
(optionally outlined on screen), a UI-tree capture and a button that writes
`debug_dump.txt` next to the plugin for bug reports.

## Known limits

- Only skills the game has evaluated have a number (see above).
- Which skill sits on a slot is read from the slot's UI element in game memory.
  If a game update moves that field, the plugin searches for it again on its
  own; until it is found, icons show no number (never a wrong one).
- Totem attribution shows the strongest totem skill currently active, not
  necessarily the one placed by that particular slot.
- Settings live in `config/settings.json` in the plugin folder. Settings from
  the original plugin are read as they are.

## Install

Install it from POE2Fixer → Plugins → Plugin Manager (one click, updates included), or download `SkillDPS.zip` from [Releases](https://github.com/POEFixer/SkillDPS/releases/latest) and extract it into the POE2Fixer folder (it creates `Plugins\SkillDPS\`). Then enable the plugin in the Plugins tab.

## Build from source

This repository is the plugin folder from the POE2Fixer source tree (`Plugins/SkillDPS/`) together with the host files it compiles against, kept at the same relative paths:

- `POEFixer/plugin_sdk/` - the POE2Fixer Plugin SDK v6 headers (`PluginAbi.h`, `PluginSDK.h`)
- `POEFixer/imgui/` - the Dear ImGui sources the DLL compiles against
- `POEFixer/core/` - small self-contained helper headers used by the plugin

1. Install Visual Studio 2022 with the "Desktop development with C++" workload (MSVC v143, Windows 10/11 SDK).
2. Open `SkillDPS.sln`, select **Release | x64** and build the solution.
3. The plugin is written to `x64\Release\Plugins\SkillDPS\SkillDPS.dll` (Release builds carry no debug information, see `Directory.Build.targets`).
4. Copy `SkillDPS.dll` into `Plugins\SkillDPS\` next to the POE2Fixer executable.

## Credits

Originally created by Ömer Faruk Arpa (https://github.com/omrfarukarpa/poefixer-skilldps).
Maintained by POEFixer since October 2026.

Licensed under the GNU General Public License v3.0, see [LICENSE](LICENSE).
