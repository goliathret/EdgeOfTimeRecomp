# Video & Graphics Guide

EdgeOfTimeRecompiled adds two pages to the pause menu's Options bar:

- **Video**, which replaces the original Brightness page and covers display and picture settings.
- **Graphics**, right after it, which covers rendering quality.

Here's a brief doc that covers every setting on both pages, what the quality presets change, what each setting costs, and suggested settings if the game isn't running well.

## Video Settings

Setting|Options|Default|Restart Required?|CVar name
-|-|-|-|-
Fullscreen|Off / On|On|No|`fullscreen`
Resolution|Native / 720p / 1080p / 1440p / 2160p|Native|**Yes**|`eot_resolution`
Picture-in-Picture Resolution|50% / 66% / 100%|50%|**Yes**|`eot_pip_scale`
Render Scale|50% – 200%, in 25% steps|100%|**Yes***|`eot_render_scale`
Aspect Ratio|4:3 / 16:10 / 16:9 / 21:9 / 32:9|16:9|No|`eot_aspect_ratio`
Frame Rate Limit|30 – 240 in steps of 10, or Unlimited|60|No|`eot_fps_limit`
VSync|Off / On|On|No|`eot_vsync`
Brightness|−25% – +25%|0%|No|`eot_brightness`
Contrast|50% – 150%|100%|No|`eot_contrast`
Saturation|0% – 200%|100%|No|`eot_saturation`
Gamma|50% – 200%|100%|No|`eot_gamma`

- **Fullscreen:** fills the display, or runs the game in a window that can be moved and resized.
- **Resolution:** the resolution the game renders at in fullscreen. **Native** is the console's own 1120x632, multiplied by Render Scale. In a window, the game picks the resolution from your display and stretches the picture to fit the window. See [Resolution & Aspect Ratio](#resolution--aspect-ratio).
- **Picture-in-Picture Resolution** some moments show a second scene in a window, such as the ability window. That scene has its own lights and effects, so it's rendered at a reduced size. At 50% it costs a quarter of the pixels.
- **Render Scale:** renders larger or smaller than the console's resolution. It only applies when Resolution is **Native** and the game is fullscreen.
- **Aspect Ratio:** the shape the game builds its picture for in fullscreen. Wider than 16:9 shows more of the scene at the sides instead of stretching it.
- **Frame Rate Limit:** the highest frame rate the game will run at. See [Frame Rate & VSync](#frame-rate--vsync).
- **VSync:** waits for the display before showing each frame. This stops tearing and caps the frame rate at your display's refresh rate.
- **Brightness, Contrast, Saturation, Gamma:** picture adjustments applied as the very last step, so they have **no performance cost**. Pure game aesthetics.
    - Brightness lifts or lowers the whole picture.
    - Contrast stretches or flattens the picture around middle grey.
    - Saturation at 0% is greyscale, and 100% is the game's own colour.
    - Gamma changes the curve between dark and light.

## Graphics Settings

Setting|Options|Default|Restart Required?|Config name
-|-|-|-|-
Quality Preset|Orig / Medium / High / Custom|Orig|Only if it changes Guest AA or Shadow Resolution|`eot_quality_preset`
Guest AA|Off / 2x / 4x / 8x|Off|**Yes**|`eot_msaa`
Host AA|Off / TAA|Off|**No**|`eot_taa`
Anisotropic Filtering|Off / 2x / 4x / 8x / 16x|16x|No|`eot_anisotropy`
Shadow Resolution|Auto / 1024 / 2048 / 4096|Auto|**Yes**|`eot_shadow_map_size`
Shadow Distance|50% – 300%, in 25% steps|100%|**No**|`eot_shadow_distance_scale`
Upscale Filter|Bilinear / Bicubic / Lanczos|Bicubic|No|`eot_upscale`
Field of View|70% – 150%, in 5% steps|100%|**Yes**|`eot_fov_scale`
Bloom|Off / On|On|No|`eot_bloom`
Depth of Field|Off / On|On|No|`eot_depth_of_field`
Motion Blur|Off / On|On|No|`eot_motion_blur`
Radial Blur|Off / On|On|No|`eot_radial_blur`
Colour Grading|Off / On|On|No|`eot_color_grading`

- **Quality Preset:** sets Anti-Aliasing, Anisotropic Filtering, Shadow Resolution and Upscale Filter together. Changing any of those four by hand switches the preset to **Custom**. See [Quality Presets](#quality-presets).
- **Guest AA:** Multi-Sample Anti-Aliasing that runs inside the game's own scene passes. The original 360 version shipped with no anti-aliasing at all, so this is brand new.
    - Only the main scene gets it. Shadows, post effects and the picture-in-picture scenes are left alone.
    - How high you can go depends on your GPU; if it doesn't support 8x, you won't see 8x.
    - The more samples, the more VRAM and bandwidth it eats, so drop this first if you're struggling.
- **Host AA:** Temporal Anti-Aliasing that's done on the port's side instead of inside the game's passes. Instead of taking extra samples per pixel like MSAA, it blends the last few frames together to smooth out edges, which usually makes it cheaper. The tradeoff is that it can look a little soft or smear during fast movement. This is gated at Ultra, but anyone can try it out to see if it fixes anything **Motion Vectors are heavily recommended** to stabilize the image.
- **Anisotropic Filtering:** keeps floors and walls sharp when you look at them at a shallow angle. It's nearly free on any modern GPU.
    - **Off** means "whatever the game asks for", and the console asked for none.
- **Shadow Resolution:** the size of each shadow map. Bigger maps give sharper shadows but use more memory.
    - **Auto** follows your resolution:

Resolution|Auto shadow size
-|-
Native|1024
1080p|2048
1440p|2048
2160p|3072

- **Shadow Distance:** how far away shadows are drawn, compared with what the level asks for. Pushing it further spreads the same shadow map over more ground, so shadows get softer.
- **Upscale Filter:** how the rendered picture is scaled to your screen. **Lanczos** is the sharpest and **Bilinear** the softest. **Bicubic** is the recommended middle ground that practically suits everyone.
- **Field of View:** widens or narrows the camera. 100% is the game's own view.
- **Bloom:** the glow around bright lights. Turning it off keeps the game's tone curve.
- **Depth of Field:** blurs what the camera isn't focused on, including the shallow focus used in cutscenes.
- **Motion Blur:** streaks fast movement, both of the camera and of objects.
- **Radial Blur:** the speed streaks during free falls and dashes.
- **Colour Grading:** the game's own colour grade. **Off** shows the ungraded image.

## Quality Presets

A preset sets four settings at once:

Setting|Low|Medium|High
-|-|-|-
Anti-Aliasing|Off|2x|4x
Anisotropic Filtering|Off (game's own)|8x|16x
Shadow Resolution|1024|2048|4096
Upscale Filter|Bilinear|Bicubic|Bicubic

- **Custom** leaves all four as you set them.
- **Only these four settings** are affected. Resolution, Render Scale, Field of View, Shadow Distance and the effect toggles are never changed by a preset.
- **8x Anti-Aliasing and Lanczos** are only available under Custom. At 4K, Lanczos costs a little more than Bicubic with no visible difference.
- **The Low preset** is the closest match to the original Xbox 360 look: no anti-aliasing, no forced texture filtering, and small shadow maps. For a fully console-like picture, also set Resolution to **Native** at **100%** Render Scale.
- **Config file or command line:** a preset set there is applied at startup and overrides the four values. For example, `--eot_msaa=4` is ignored while the preset is `high`. Set the preset to `custom` to control them one by one.

## Resolution & Aspect Ratio

The console rendered at **1120x632**.

**Resolution presets:** each one is a target height. The width is scaled by the same amount, so the game's roughly 16:9 shape is kept:

Resolution|Internal size (approx.)
-|-
Native|1120x632 × Render Scale
720p|1276x720
1080p|1914x1080
1440p|2552x1440
2160p|3828x2160

**Aspect Ratios** apply in fullscreen:

- **16:9** is the game's own shape.
- **21:9 and 32:9** show more of the scene at the sides. The HUD stays in a centred 16:9 area so it isn't stretched across an ultrawide screen.
- **16:10** is for 16:10 displays, such as most MacBooks and Handheld PC's.
- **4:3** is for 4:3 displays you grandpa

In a window, the picture always follows the window's shape, whatever this setting says. The HUD elements mainly stay locked at 16:9 to avoid stretching and making the GUI feel unnatural for Ultra and Superwide, as well as stretching on Windowed mode.

## Frame Rate & VSync

The game runs one update per frame, so the frame rate limit sets the pace of the whole game. The port handles high frame rates as follows:

- **At 60 and below,** the game's own 60 fps limiter stays in place.
- **Above 60,** that limiter is removed and the port paces the frames instead.
- **With VSync on,** frames are locked to your display's refresh rate. The Frame Rate Limit then only matters if it's **below** your refresh rate.
- **With VSync off,** the Frame Rate Limit is the only thing pacing the game. You may see tearing.

On a display with a variable refresh rate (G-Sync, FreeSync or VRR), turn VSync off and set the Frame Rate Limit to your refresh rate or a little below it. The installer does this for you by default.

It should be noted that almost all 360 titles are heavily unstable running above 60 frames, or most times, even 30. EdgeOfTimeRecompiled offers solutions for fixing those high frame rate issues, but that doesn't mean everything is perfect. Some distant objects still animate at a reduced rate above 60 fps; this is how the console saved work. The port turns that off by default (`eot_update_lod`), so everything animates at the full frame rate, but there may still be various glitches here and there.

## What the Installer Picks

The first-run installer suggests settings from your display. These can differ from the defaults listed above:

Setting|What the installer picks
-|-
Quality Preset|**Medium** on displays 1080 rows or taller, otherwise **Low**
Resolution|720p, 1080p or 1440p to match your display's height
Aspect Ratio|The option closest to your display's shape
Frame Rate Limit|Your display's refresh rate, or 60 if it can't be detected
VSync|**Off** (the frame rate limit does the pacing)
Picture-in-Picture|66% at 720p or Native, otherwise 50%

You can change any of these afterwards on the Video and Graphics pages.

## Performance Costs

These numbers were measured on a development machine with a Radeon PRO 5700 XT. They're GPU time per frame, so lower is better, and 16.7 ms is the budget for 60 fps. Your numbers will differ, but the relative costs are a good guide.

Setting|Cost
-|-
**Anti-Aliasing**|The biggest single cost. At 1440p on the title screen, 4x added about **1 ms** (6.5 → 7.5 ms), and 2x costs about 6.9 ms. In a busy room, dropping from 8x to 4x saved about 0.5 ms. At 1440p, 4x MSAA holds about 110 FPS at the menu and a majority of the game
**Resolution / Render Scale**| Most definitely will cost a lot more frametimes with resolution scaling. Just pick the resolution of your monitor, or pick the closest if you really cannot hit 60 FPS.
**Shadow Resolution**|4096 → 2048 saves about **0.35 ms**.
**Upscale Filter**|Lanczos costs **0.14–0.3 ms** more than Bicubic at 4K based on my testing.
**Picture-in-Picture Resolution**|This won't affect framerates at displays that are already running a low resolution (native/720p)
**Anisotropic Filtering**|Nearly free.
**Brightness / Contrast / Saturation / Gamma**|Free.
**Bloom, Depth of Field, Motion Blur, Radial Blur, Colour Grading**|Small. Turning them all off removes about 27 GPU operations a frame.


## Suggested Settings

Here are four tiers to start from. Pick the one closest to your hardware, then raise settings one at a time if you've got headroom.

### Xbox One-level hardware (Intel UHD 630)

Setting|Value
-|-
Quality Preset|Orig
Resolution|Native
Render Scale|100%
Picture-in-Picture Resolution|50%
Frame Rate Limit|30

This is about as close to the 360 experience as you can get, and it's the floor we're aiming for. Integrated graphics like the UHD 630 get hit hardest by draw calls, so keep the frame rate at 30 here.

### Steam Deck

Setting|Value
-|-
Quality Preset|Medium
Resolution|720p
Picture-in-Picture Resolution|50%
Frame Rate Limit|60

720p fits the Deck's screen, and Medium at 60 is what we're targeting there. If a busy area dips, drop the preset or the PiP resolution before anything else.

### GTX 1070

Setting|Value
-|-
Quality Preset|High
Resolution|1080p
Picture-in-Picture Resolution|50%
Frame Rate Limit|60

Special thanks to Snap for testing. His 1060 managed to get 1080P 60FPS on High for around 80% of areas, so it's probably to assume you will be locked at 60 on slightly better hardware

### RX 5700 XT

Setting|Value
-|-
Quality Preset|Ultra
Resolution|1440p
Picture-in-Picture Resolution|66%
Frame Rate Limit|60

This is the machine the port is developed and tested on, so every number in [Performance Costs](#performance-costs) comes from here.

### If you're getting stutters or low frame rates

1. **Lower Anti-Aliasing first.** It's the biggest cost.
2. **Set Picture-in-Picture Resolution to 50%** if the drops happen when the ability window appears.
3. **Lower the Frame Rate Limit** to a value you can hold steadily. A stable 60 feels better than an unstable 90.
4. **Lower Shadow Resolution** to 2048 or 1024.
5. **Lower Resolution or Render Scale.**
