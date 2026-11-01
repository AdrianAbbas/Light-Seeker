# LIGHT SEEKER: art sources

All art is original and generated in this repository; no image of anyone else's is used.
The developer's mood references (romantic, luminous fantasy paintings) informed the palette
and light only; they are not copied or committed.

| Art | Source |
|---|---|
| Painted map and scenes | scripted Blender scenes (`tools/painted`), rendered as depth layers and painted by a post-process (Milestone 3) |
| Fallback scenes | drawn in code (`Source/UI/Scenes.cpp`): a luminous wash per place, shown when a scene's painted files are missing |
| Icon | `tools/icon/make_icon.py` |
| Fonts | Cinzel, Cormorant Garamond (SIL Open Font License, `assets/fonts`) |

## Drop-in art

Your own or commissioned art replaces any scene:
`~/Library/Application Support/LightSeeker/scenes/<location>/<place>.png` (and `map.png`),
for example `scenes/celestial-citadel/gates-of-dawn.png`; or a folder of layers (`back`,
`far`, `mid`, `fog`, `near`) for parallax.
