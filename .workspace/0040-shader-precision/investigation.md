# 0040 Shader precision: investigation

Issue: #40 · Branch: `k/0040-shader-precision` · Status: refined · Confidence: 95% (proceed)

## The request

Many of the libretro GLSL shaders in the image fail in RetroArch with "Failed to compile shader" on the RG DS Plus,
while the same shaders work under ROCKNIX on the same handheld. The cause is upstream's `d1c7383b`, which rewrites
every shader at build time so that the vertex stage's `#define COMPAT_PRECISION mediump` becomes `highp`. That fixed
the four shaders whose stages disagreed, but broke most of the others: a redefined macro (122 files, by the issue's
check), and uniforms declared at `highp` in the vertex stage and `mediump` in the fragment stage (504 files). Among
the broken ones are Banana's own Settings > Other systems > Picture & sound > Shader choices *LCD* and *Sharp*.

The fork should ship the shaders as libretro wrote them, with the vertex stage raised to `highp` only where the
fragment stage is `highp` too (the issue's *Fix B*), plus a check that every shader in the image preprocesses and
has matching precisions in its two stages.

Decided with the user (question round 1):

- **Mechanism**: a post-install hook of the fork's in `kiwami/external.mk`
  (`RETROARCH_GLSL_SHADERS_POST_INSTALL_TARGET_HOOKS`). It copies the original `.glsl` files back from the package's
  source folder over what upstream's install rewrote, then raises the vertex stages that need it. No upstream file is
  touched, and it doesn't depend on the name of upstream's `RETROARCH_GLSL_SHADERS_VERTEX_HIGHP`.
- **Which shaders**: a rule, not a list. A shader whose fragment stage defines `COMPAT_PRECISION highp` gets its
  vertex stage's `mediump` raised to `highp`. In the pinned sources that is exactly the four shaders `d1c7383b` was
  for (*Current state*), and the rule keeps working when upstream bumps the shaders' pin.
- **Option**: behind a Kconfig option of the fork's in `kiwami/Config.in` (off = upstream's rewrite as it is), set
  on in `kiwami/configs/kiwami.fragment`, as `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER` is.
- **Load Preset can't reach the card** (the issue's *Also found*): a separate issue, not this feature.

Decided with the user (question round 2):

- **Test**: in CI and on an output. `kiwami/tests/ci.sh` fetches the pinned glsl-shaders tarball, runs upstream's
  install command and the fork's hook on it through a small shim makefile, and checks the result with
  `glslangValidator` (`glslang-tools` installed in the fork's CI step). The same script checks a built output's
  target folder for the final check.
- **Name**: `shader-precision`.
- **Upstream**: not prepared. The fix stays in the fork; no upstream variant or text.

Out of scope: Load Preset's file browser (its own issue), Fix A (every stage at `highp`), a fix for upstream,
BananaFront's and the DS emulators' shaders (their own converted copies, not this package).

The issue's text (#40) was written before the round-2 decisions. Its *Proposed fix* says the change goes through a
`kiwami:` hook in upstream's `.mk` and names the four shaders, and its *Also found* reads as part of the feature.
Replacements for those two sections are under *Open questions* 1.

## Oversights in the requirements

1. **Buildroot doesn't reinstall a package when the fork's `.mk` changes.** A change to `kiwami/external.mk` or
   to the new option leaves `output/<board>/build/retroarch-glsl-shaders-*/.stamp_target_installed` in place, and
   `make <board>` keeps the shaders as they were. The same trap as `banana-ui` (the memory's *Rebuild local packages
   before an image*). *Decided*: the plan's device-image steps run `make retroarch-glsl-shaders-reinstall-<board>`
   (or `-dirclean`) before the image, and the final check runs the new test on the output's target folder, which
   shows whether the reinstall happened.
2. **The issue's counts are a model, not Panfrost.** My re-run of the preprocessor part (simplified defines, no
   `GL_FRAGMENT_PRECISION_HIGH`) finds 119 failing vertex stages in the image against the issue's 122, and 0 in the
   sources. The difference is the defines, not the conclusion. The "declared only in the vertex stage" case
   (fakelottes, `stock.glsl`) is unconfirmed on Panfrost. *Decided*: it doesn't need confirming for the fix. Fix B
   puts those shaders back as libretro and ROCKNIX ship them (`mediump` in both stages), which the issue saw load
   (`pixel_transparency`'s table). The manual tests load one shader of each group.
3. **Settings' *CRT* isn't affected; *LCD* and *Sharp* are.** `run-retroarch` points `global.glslp` at
   `handheld/zfast-lcd.glslp`, `crt/zfast-crt.glslp` or `interpolation/sharp-bilinear-simple.glslp`
   (`system/rootfs/usr/libexec/banana/run-retroarch:167-170`). In the image, `zfast_lcd.glsl` and
   `sharp-bilinear-simple.glsl` have `highp` in the vertex stage and `mediump` in the fragment stage, while
   `zfast_crt.glsl` defines `highp` in both stages in the original too (lines 55-58, 121-124), so upstream's rewrite
   leaves it unchanged. The issue doesn't say *CRT* works. *Decided*: noted here. The manual tests include *CRT*,
   because Fix B mustn't break it (the rule leaves it as it is).
4. **A card that already chose *LCD* or *Sharp* starts showing the shader after the update.** Today those games
   fall back to RetroArch's stock shader, so they look unshaded, and the user may not have noticed. After the fix the
   shader runs and costs GPU time it didn't before (*Risks*). This is the intended behaviour; nothing on `/data`
   needs migrating (`global.glslp` only references the image's preset).
5. **Only an RG DS Plus to test on.** The RG DS has the same SoC (RK3566, Mali-G52), Mesa and RetroArch, and both
   boards install the same shaders, so the result carries over. *Decided*: manual tests on the RG DS Plus only. The
   plan says so.
6. **A future upstream fix of its own.** If upstream changes or drops `RETROARCH_GLSL_SHADERS_VERTEX_HIGHP`, the
   fork's hook still restores the originals and applies its rule, so the image stays right. The fork's hook then
   undoes upstream's version, whatever it is. *Decided*: when that happens, `banana-merge-upstream` sees the `.mk`
   change. The fork drops its hook if upstream's fix passes the new test, and the CI test fails if upstream's change
   and the fork's hook disagree.

## Suggestions

1. **No `kiwami:` hook in upstream's file** (taken: round 1). `kiwami/external.mk` is read after upstream's
   package `.mk` (`.buildroot/Makefile:564`; `kiwami/external.mk:5-6` says so for the fork's other additions), and
   the post-install hooks are expanded when the recipe runs (`.buildroot/package/pkg-generic.mk:372`). That's the
   same way the fork adds `KIWAMI_RETROARCH_VERSION_FILE` to RetroArch (`kiwami/external.mk`). Cost: upstream's
   rewrite still runs first, about a second over 684 files.
2. **A rule instead of the four names** (taken: round 1).
3. **Fix B over Fix A, for speed too.** Fix B keeps the fragment stages at `mediump` as libretro wrote them; the
   Mali-G52 (Bifrost) runs FP16 at twice FP32's rate. Fix A would put every fragment stage at `highp` and slow every
   shader, and it still leaves `windowed/shaders/jinc2-sharp.glsl` mismatched. *Decided*: Fix B (the issue already
   recommends it).
4. **Upstream contribution**: declined (round 2).
5. **Load Preset reaching `/data`**: left out of scope (round 1). A separate issue for the fork, to write when the
   user wants it.

## Current state

**Upstream's package.** `buildroot/package/retroarch-glsl-shaders/retroarch-glsl-shaders.mk` pins libretro's
`glsl-shaders` at `ce9a4124` (line 9). Its install copies the whole tree to `/usr/share/libretro/shaders/glsl`
(lines 13-17) and then runs `RETROARCH_GLSL_SHADERS_VERTEX_HIGHP` (lines 25-30): an `awk` over every `.glsl` file
that, between `defined(VERTEX)` and `defined(FRAGMENT)`, rewrites `#define COMPAT_PRECISION mediump` to `highp`. It
also adds a final newline to files that lacked one, so 656 files in the output differ from the sources
(`diff -rq`). The package is `BR2_PACKAGE_RETROARCH_GLSL_SHADERS`, `default y` (`buildroot/package/retroarch/cores.in:91`).
Upstream's history of the package is three commits (`eb40d2e3`, `526f2395`, `d1c7383b`); upstream's head
(`JorreFog/BananaLinux`, read through `gh api`) has the same `.mk` as `main` (blob `94b3176b`). The fork hasn't
changed it (`git diff main...kiwami` empty).

**What `d1c7383b` was for.** In the sources, exactly four shaders define `COMPAT_PRECISION highp` in the fragment
stage (under `GL_FRAGMENT_PRECISION_HIGH`) and `mediump` in the vertex stage: `crt/shaders/zfast_crt_geo.glsl`,
`zfast_crt_geo_svideo.glsl`, `zfast_crt_nogeo_svideo.glsl` (vertex line 47, fragment lines 96-99) and
`interpolation/shaders/pixellate.glsl` (33, 81). That's a scan of every `.glsl` for both stages' `#define
COMPAT_PRECISION` (*Testing*). Every other shader has the same precision in both stages, mostly `mediump`.

**What the rewrite broke** (checked in `output/rgds-plus/target/usr/share/libretro/shaders/glsl`, which is the
upstream rewrite's output):

- `handheld/shaders/pixel_transparency.glsl`: line 70 is `highp` in the image, `mediump` in the source; its
  fragment stage defines `mediump` (source line 130). This is the issue's confirmed failure.
- `handheld/shaders/zfast_lcd.glsl` (vertex line 47 `highp`, fragment line 99 `mediump`) and
  `interpolation/shaders/sharp-bilinear-simple.glsl` (27 `highp`, 81 `mediump`): Settings' *LCD* and *Sharp*.
- `handheld/shaders/color/gba-color.glsl`: `COMPAT_PRECISION` defined `mediump` at the top (line 11) and `highp` in
  the vertex stage (58). `glslangValidator -E` stops on "Macro redefined; different substitutions:
  COMPAT_PRECISION". 109 of the 119 such files I found are under `procedural/`, and seven are `handheld/shaders/color/`.

**How RetroArch compiles them.** The fork's RetroArch master `71f521e`, `gfx/drivers_shader/shader_glsl.c:408-497`:
a shader's own `#version` from 130 to 329 becomes `#version 300 es` on GLES 3, 330 becomes `310 es`, above 330
becomes `320 es`, anything else `100`. A shader without `#version` gets no version line, so GLSL ES 1.00. The define
(`VERTEX`/`FRAGMENT`) and the alias define go in front of the source. Banana uses the `gl` driver
(`system/rootfs/etc/banana/retroarch.cfg:4`) and `video_shader_dir` is the image's folder (line 60, and
`run-retroarch:109`).

**Banana's own shader setting.** `run-retroarch:159-184` writes `/data/retroarch/config/global.glslp` as a
`#reference` to one of the image's presets when Settings' *Shader* changes. *LCD* is `handheld/zfast-lcd.glslp`
(one pass, `shaders/zfast_lcd.glsl`), *CRT* is `crt/zfast-crt.glslp` (`zfast_crt.glsl`), and *Sharp* is
`interpolation/sharp-bilinear-simple.glslp` (`sharp-bilinear-simple.glsl`). BananaFront (`run-bananafront:52-53`)
and the DS emulators use their own converted shaders, not this package.

**What the fork can build on.** `kiwami/external.mk` already adds hooks to upstream's packages after their `.mk`
(`KIWAMI_RETROARCH_VERSION_FILE`, `HOST_ZSTD_OPTS`). `kiwami/Config.in` has the fork's options
(`BR2_PACKAGE_KIWAMI_RETROARCH_MASTER`, `BR2_PACKAGE_KIWAMI_OTA_OFF`), set in `kiwami/configs/kiwami.fragment`
(line 18 for master). `kiwami/tests/ci.sh` gets the Buildroot clone's path and is run by the fork's step in
`.github/workflows/ci.yml:64-65`. `glslangValidator` 16.6.0 is on this machine (Arch's `glslang`); Ubuntu 24.04 has
it as `glslang-tools`.

## What has to change

- `kiwami/Config.in`: a new option (e.g. `BR2_PACKAGE_KIWAMI_GLSL_SHADERS_PRECISION`, depending on
  `BR2_PACKAGE_RETROARCH_GLSL_SHADERS`), off = upstream's rewrite. `kiwami/configs/kiwami.fragment`: it set to `y`.
- `kiwami/external.mk`: under the option, a define added to `RETROARCH_GLSL_SHADERS_POST_INSTALL_TARGET_HOOKS`. It
  copies every `.glsl` in `$(@D)` back over its copy in the target's shader folder, then runs the rule (vertex
  `mediump` → `highp` where the fragment stage defines `highp`). The rule itself can be a script in the fork
  (`kiwami/tools/` or next to the test), so the hook and the test run the same code.
- `kiwami/tests/<name>.sh` (e.g. `glsl-precision.sh`): the check (each `.glsl`, both stages, as RetroArch assembles
  them; `glslangValidator -E`; uniforms in both stages compared by precision), failing on any preprocessor error or
  mismatch. Two modes: the pinned tarball through a shim makefile that includes upstream's `.mk` and the fork's
  hook, or an output's target folder.
- `kiwami/tests/ci.sh`: runs the check in its tarball mode.
- `.github/workflows/ci.yml`: the fork's own step (its `kiwami:` line, 64-65) also installs `glslang-tools`. No
  upstream line changes.
- `kiwami/DEPENDENCIES.md`: `glslang` (the CI's and the developer's checker, never in the image), and the
  glsl-shaders pin under *Upstream's, that the fork depends on* (the fork's hook and test rely on its layout).
- `docs/FORK.md`: the hook, for merges (what to check when upstream's `.mk` or the shaders' pin changes).

No upstream file changes, no rootfs or banana-ui changes, no setting on the device.

## Options

- **A. Fix B as a post-install hook in `kiwami/external.mk`** (recommended, chosen). It restores the sources and
  raises only the matching vertex stages. It touches no upstream file and doesn't rely on upstream's variable name.
  Upstream's rewrite still runs and is then undone, which costs about a second at build time.
- **B. Redefine `RETROARCH_GLSL_SHADERS_VERTEX_HIGHP` in `kiwami/external.mk`.** Simplest, and upstream's rewrite
  never runs. If upstream renames the variable, its rewrite silently comes back; only the CI test would see it.
- **C. A `kiwami:` hook in upstream's `.mk`.** Explicit, but it edits an upstream file that a merge can conflict on.
- **D. Fix A (every stage at `highp`).** Slower fragment stages on the Mali-G52 (FP32 instead of FP16), and
  `jinc2-sharp` still mismatched.

## Risks

- **Performance and footprint**: none in the menu. No rootfs size change (the same files, about 39 MB). Fix B leaves
  fragment stages at libretro's `mediump`, so a shader that loads costs what it costs under ROCKNIX. Games set to
  *LCD* or *Sharp* now run their shader (*Oversights* 4): GPU time they didn't spend before, which Power scaling
  (0043) raises the GPU's floor for if it's late.
- **Conflicts with upstream**: no upstream file touched. Upstream's `.mk` changed three times in its history. A
  change to it or to the pin shows in the CI test and in `banana-merge-upstream`'s checks of the fork's hooks.
- **Hardware and boards**: the same SoC, GPU, Mesa and shaders on `rgds`, `rgds-plus` and `universal`. Whether a
  shader compiles and links on Panfrost only shows on the device. The host check models the GLSL rules, not Panfrost's
  compiler. Only an RG DS Plus to test on.
- **Stale builds**: *Oversights* 1. An image built without reinstalling the package carries upstream's rewritten
  shaders; the test on the output catches it.
- **The shim makefile**: it runs upstream's install command outside Buildroot, so it relies on the `.mk` being a
  plain `generic-package` with `$(@D)` and `$(TARGET_DIR)`. If upstream turns the package into something else, the
  shim breaks loudly in CI, not silently.

## Testing

- **Headless, CI** (`kiwami/tests/ci.sh`): the pinned tarball (version read from upstream's `.mk`, 26 MB) through
  upstream's install and the fork's hook, then the check: 0 preprocessor errors and 0 precision mismatches across the
  684 `.glsl` files. The test also proves the rule picks exactly the four shaders today and that the result equals
  the sources except their vertex `COMPAT_PRECISION` lines. With the option off (upstream's rewrite only), the same
  check fails: the test shows it does detect the bug.
- **On an output** (the final check): the script on `output/<board>/target/usr/share/libretro/shaders/glsl`.
- **Manual, on the RG DS Plus** (the issue's list): Settings > Other systems > Picture & sound > Shader *LCD*,
  *Sharp* and *CRT* in a game; `handheld/pixel_transparency.glslp`; `crt/fakelottes.glslp`; a preset starting with
  `stock.glsl`; `gba-color` (preprocessor group); a crt-royale or xbr preset (GLSL ES 3.00 group);
  `zfast_crt_nogeo_svideo` (the shader `d1c7383b` fixed, which must keep working). Each loaded through Load Preset,
  noting whether the picture changes and whether "Failed to compile shader" shows. For any that fails: RetroArch's
  log, `/run/banana/retroarch.log`, its `[GLSL]` lines, fetched over SSH/SFTP before the next game starts.
- `make test` is unaffected (no banana-ui change).

## Open questions

1. **The issue's text no longer matches two decisions** (the mechanism and *Also found*). *Answered*: replacement
   sections offered to the user with this report, for them to edit into #40 if they want.
2. Every other question was answered in rounds 1 and 2 (*The request*). Nothing is open or blocking.
