# 0040 Shader precision: plan

Issue: #40 · Branch: `k/0040-shader-precision` · Investigation: [investigation.md](investigation.md) · Status: approved

## Summary

The image's libretro GLSL shaders go back to how libretro wrote them. The vertex stage is raised to `highp` only in a
shader whose fragment stage defines `COMPAT_PRECISION highp` and whose vertex stage defines no `highp` of its own: today the four `d1c7383b` was for (zfast_crt_geo,
zfast_crt_geo_svideo, zfast_crt_nogeo_svideo, pixellate). The other 680 compile and link as they do under ROCKNIX,
Settings' *LCD* and *Sharp* among them.

The approach chosen in the investigation and confirmed here:

- `kiwami/glsl-shaders.mk`, included by `kiwami/external.mk`, adds a post-install hook to upstream's
  `retroarch-glsl-shaders` behind the new option `BR2_PACKAGE_KIWAMI_GLSL_SHADERS_PRECISION` (off = upstream's
  rewrite, on in `kiwami.fragment`).
- The hook runs `kiwami/glsl-shaders.sh`. It copies every `.glsl` back from the package's source folder over
  upstream's rewrite, then applies the rule.
- No upstream file changes, except the fork's own CI step in `.github/workflows/ci.yml`, which also installs
  `glslang-tools`.
- A checker, `kiwami/tests/glsl-precision.py`, runs `glslangValidator -E` on each shader's two stages as RetroArch
  assembles them and compares the precision of every uniform both stages declare.
- `kiwami/tests/glsl-shaders.sh` runs upstream's install through a shim makefile on the pinned tarball, with the
  option off and then on. CI runs it from `ci.sh`, and the final check runs it on a built output.

### Decided while planning

- **Layout**: the hook and its option guard in their own `kiwami/glsl-shaders.mk`, the rule in
  `kiwami/glsl-shaders.sh`. The test's shim includes upstream's `.mk` and `kiwami/glsl-shaders.mk`, so CI runs the
  make code the build runs, guard and hook registration included. Only `external.mk`'s one `include` line is left
  to the check on a built output.
- **Steps**: the checker first (with its self-test and the CI package), then the fix with its test on the pinned
  shaders, then the final check.
- **Upstream fixed**: if upstream's install (the option off) starts passing the check, CI fails with "upstream's
  shaders pass the check: see whether the fork's hook is still needed (docs/FORK.md)". Today that half of the test is
  also what shows the checker sees the bug.
- **How RetroArch assembles a stage**, which the checker repeats (RetroArch master `71f521e`,
  `gfx/drivers_shader/shader_glsl.c`):
  - **`#pragma parameter` lines** are blanked (`gl_glsl_strip_parameter_pragmas`, line 614).
  - **The version line**: the first `#version N` in the file is found with `strstr`, and the program starts right
    after its number, so anything before it is dropped (lines 415-423). N is mapped on GLES 3: 130-329 becomes
    `300 es`, 330 `310 es`, above 330 `320 es`, anything else `100` (426-446). A file with no `#version` gets no
    version line, which a GLES compiler reads as `#version 100`, and the checker writes it so.
  - **The defines**: each stage gets RetroArch's six (`#define VERTEX` or `#define FRAGMENT`, `PARAMETER_UNIFORM`,
    `_HAS_ORIGINALASPECT_UNIFORMS`, `_HAS_FRAMETIME_UNIFORMS`, `_HAS_SENSOR_UNIFORMS`, `_HAS_SWAPCOUNT_UNIFORM`;
    lines 554 and 569).
  - **Left out**: the pass aliases' `#define <alias>_ALIAS` (lines 1276-1290). They depend on the preset, and no
    shader's precision depends on them.
  - **Predefined macros**: `glslangValidator` 16.6.0 defines `GL_ES` and `GL_FRAGMENT_PRECISION_HIGH` in both
    stages, at 100 and 300 es (checked), as Mesa does on the Mali-G52.

## Acceptance criteria

- **A1**: given a build with the option on, when the shaders are installed, then every `.glsl` in
  `/usr/share/libretro/shaders/glsl` is byte for byte libretro's at the pinned commit, except in a shader whose
  fragment stage defines `COMPAT_PRECISION highp` and whose vertex stage defines no `highp`. There, the vertex stage's `#define COMPAT_PRECISION mediump`
  lines say `highp` and nothing else differs. At `ce9a4124` that is exactly the four shaders above.
- **A2**: given the option on, every `.glsl` passes the checker. Both stages preprocess, and no uniform declared in
  both has two precisions. The count is 0 preprocessor errors and 0 mismatches over 684 shaders, beyond the faults
  already in libretro's own sources that `kiwami/tests/glsl-known.txt` lists: at `ce9a4124`, `gpu/shaders/powervr2.glsl`'s
  `FrameCount` and `FrameDirection` (*Deviations*). A listed fault no longer found fails the check too.
- **A3**: given the option off, the shaders are what upstream's install makes (the hook doesn't run), and the checker
  fails on them (the bug reproduced). If it passes, CI fails, saying upstream's shaders pass now.
- **A4**: the checker reports a redefined `COMPAT_PRECISION` as a preprocessor error, and a uniform at `highp` in one
  stage and `mediump` in the other as a mismatch naming the uniform, both at GLSL ES 1.00 and 3.00. It takes a
  stage's default precision when a declaration has none, and passes shaders without either fault (its self-test).
- **A5**: the hook stops the build, naming the file, when a shader in the source folder isn't in the installed folder
  (upstream's install changed shape). It also stops when the source folder holds no shader.
- **A6**: every board (`rgds`, `rgds-plus`, `universal`, and `rgds_plus` with `KIWAMI_SERIAL=1`) is configured with
  the option on (`configure.sh` fails on a dropped option).
- **A7**: given an image with the option on, when Settings > Other systems > Picture & sound > Shader is *LCD*,
  *Sharp* or *CRT*, then a RetroArch game shows that shader, without "Failed to compile shader". This includes a card
  set to *LCD* before the update, with nothing on the card changed.
- **A8**: in a RetroArch game, Quick Menu > Shaders > Load Preset loads each of these, and the picture changes
  without an error:
  - `handheld/pixel_transparency.glslp` (ES 1.00, a uniform in both stages)
  - `crt/fakelottes.glslp` (declared only in the vertex stage)
  - `nearest.glslp` (`stock.glsl`)
  - `handheld/gba-color.glslp` (the preprocessor group)
  - `xbr/xbr-lv2.glslp` (GLSL ES 3.00)
- **A9**: `crt/zfast_crt_nogeo_svideo.glslp` and `interpolation/pixellate.glslp`, which `d1c7383b` fixed, still load,
  and a GBA game keeps 60 fps with `zfast_crt_nogeo_svideo` and with *LCD*.
- **A10**: no upstream file changes but the fork's CI step (its `kiwami:` line), and nothing in the vendored or synced
  folders.

## Risks

- **Performance and footprint**: nothing changes in the menu, idle or boot. The rootfs holds the same files, about
  39 MB. The fragment stages stay at libretro's `mediump`, so a shader costs what it costs under ROCKNIX. Games set
  to *LCD* or *Sharp* now run their shader, GPU time they didn't spend while it failed (investigation, *Oversights*
  4). A9 checks a GBA game keeps 60 fps. The build spends about a second more (upstream's rewrite, then the restore),
  and CI about 20 s plus a 26 MB download.
- **Conflicts with upstream**: upstream's `.mk` is untouched. Upstream changed it three times. A change to its
  install, its variable or its pin is caught by `glsl-shaders.sh` in CI: the shim runs upstream's real install, the
  hook stops on a missing file (A5), and the check fails when upstream's own output passes (A3).
  `docs/FORK.md`'s *What the fork shadows* lists the package, so a merge reads its changes. `ci.yml`'s fork step
  changes one line that is already the fork's.
- **Hardware and boards**: whether Panfrost compiles and links a shader only the device shows. The checker models
  the GLSL rules, not Mesa's compiler. M1-M3 cover one shader of each group the investigation found. Only an RG DS
  Plus is at hand: the RG DS has the same SoC, GPU, Mesa and shaders, and isn't tested.
- **A stale output**: Buildroot doesn't reinstall a package when the fork's `.mk` changes. Step 3 runs
  `retroarch-glsl-shaders-dirclean` before the image, and `glsl-shaders.sh --output` on the output shows whether
  the reinstall happened.
- **The shim**: it relies on upstream's package being a plain `generic-package` that uses `$(@D)` and
  `$(TARGET_DIR)`. A different package shape fails the CI test loudly, not silently.
- **The checker's model**: it could pass a shader Mesa rejects for a reason it doesn't model. M1-M3 are the guard,
  and RetroArch's `[GLSL]` log lines show the reason for any failure.

## Steps

Each step is one commit, and leaves the tree building and `kiwami/tests/ci.sh .buildroot` passing (`make test` is
unaffected: no banana-ui change).

### 1. A checker for the shaders' two stages

- **Files**: `kiwami/tests/glsl-precision.py` (new), `kiwami/tests/ci.sh` (changed), `.github/workflows/ci.yml`
  (hook: the fork's existing step), `kiwami/DEPENDENCIES.md` (changed),
  `.workspace/0040-shader-precision/plan.md` (changed: Progress).
- **Change**:
  - `kiwami/tests/glsl-precision.py`, header comment in `image-compare.py`'s style:
    ```
    glsl-precision.py [--known <file>] <shaders dir>   each .glsl's two stages as RetroArch compiles them on GLES 3,
                                      preprocessed by glslangValidator: exit 1 on a preprocessor error or a uniform
                                      both stages declare at two precisions
    glsl-precision.py --self-test     the same on made-up shaders
    ```
    - **`--known <file>`** (added, *Deviations*): faults already in libretro's own sources, one `<path>: <uniform or
      stage>` per line, `#` comments. A fault starting with a listed line is printed as `known: ...` and not counted;
      a listed line matching no fault is printed as `<line>: known, but no longer found` and fails the check.
    - **For each `*.glsl`** under the folder (sorted) and each stage, the assembled source, as in *Decided while
      planning*:
      - `#pragma parameter` lines blanked.
      - The version: the first `#version N` found as text, the program taken from after its number, N mapped
        (130-329 → `300 es`, 330 → `310 es`, > 330 → `320 es`, else `100`). With no `#version`, `#version 100`.
      - Then RetroArch's six defines for the stage, then the program.
    - **Preprocessing**: the source is written to a temporary `.vert` or `.frag` and run through
      `glslangValidator -E`. A non-zero exit is a preprocessor error, reported with its first `ERROR:` line.
    - **Uniforms**: from the preprocessed text, at brace depth 0, two things are collected in order:
      - the `precision <q> <type>;` statements, which update the stage's defaults;
      - the `uniform [lowp|mediump|highp] <type> <name>[[n]][, <name>...];` declarations. An interface block
        (`uniform <Name> {`) is skipped.
    - **A declaration's precision**: its qualifier, or else the stage's default for its base type. That base type is
      `float` for `float`/`vec*`/`mat*`, `int` for `int`/`ivec*`/`uint`/`uvec*`, the sampler's own type for a
      sampler, and none for `bool`/`bvec*`, which have no precision.
      - The vertex stage's defaults are `float`/`int` `highp` and samplers `lowp`.
      - The fragment stage's are `int` `mediump` and samplers `lowp`. `float` has none: one left without a precision
        is reported as `none`.
    - **A mismatch** is a uniform declared in both stages with two precisions. It is reported as
      `<path>: <name>: vertex <q>, fragment <q> (GLSL ES <1.00|3.00>)`.
    - **Output**: one line per fault, then `glsl-precision: <n> shaders, <e> with a preprocessor error, <m> with a
      precision mismatch` (shaders counted, not faults), with `, <k> known faults excused, <g> no longer found` under
      `--known`. Exit 0 when all are 0, 1 otherwise, 2 with no `glslangValidator` (saying to install
      `glslang-tools`) or no shader in the folder.
    - **Speed**: the files run in parallel (a thread pool of `os.cpu_count()` workers, since the work is in the
      subprocesses).
    - `--self-test` writes made-up shaders into a temporary folder, runs the check on each and asserts:
      - both stages `mediump` → pass;
      - `OutputSize` `highp` in the vertex stage and `mediump` in the fragment stage, no `#version` → one mismatch
        naming `OutputSize`, ES 1.00;
      - the same with `#version 130` → the mismatch, ES 3.00, and the assembled header is `#version 300 es`;
      - `COMPAT_PRECISION` defined `mediump` at the top and `highp` in the vertex stage → a preprocessor error
        naming the macro;
      - a fragment stage at `highp` under `GL_FRAGMENT_PRECISION_HIGH` with the vertex stage at `highp` → pass;
      - a vertex uniform with no qualifier (`highp` by default) against a fragment one under
        `precision mediump float;` → a mismatch;
      - `uniform vec2 a, b;` in both stages, one side at another precision → both names reported;
      - `uniform int FrameCount;` in both stages, no qualifier → a mismatch, `highp` against `mediump` (powervr2's
        case);
      - the version mapping (110, 120 → `100`; 329 → `300 es`; 330 → `310 es`; 400 → `320 es`);
      - `--known`: both faults of a shader listed → excused; one of two listed → the other still counted; a listed
        fault not there → no longer found; a listed preprocessor error → excused; the list's comments and blank
        lines skipped;
      - a `#pragma parameter` with `"` in it, and a comment before `#version` → pass (both stripped as RetroArch
        strips them);
      - an empty folder → exit 2.
      It prints `glsl-precision self-test: <n> cases passed`.
  - `kiwami/tests/ci.sh`, after `deps-audit`'s block (line 100): `python3 -I kiwami/tests/glsl-precision.py
    --self-test` when `glslangValidator` is found, else `glsl-precision: skipped, no glslangValidator
    (glslang-tools)`, as `deps-audit` skips without `cc`.
  - `.github/workflows/ci.yml:65`, the fork's step: `run: sudo apt-get install -y glslang-tools && kiwami/tests/ci.sh
    /tmp/br` (the job's *Packages* step ran `apt-get update` already). Its name line keeps its `# kiwami:` comment,
    which gains "and glslang for the shaders' check".
  - `kiwami/DEPENDENCIES.md`, *The fork's own*: glslang (`glslangValidator`):
    - **Version**: Ubuntu 24.04's `glslang-tools` in CI, unpinned, and the developer's own (16.6.0 checked). Only its
      preprocessor (`-E`) is used, which the GLSL ES specification fixes.
    - **Pinned in**: `.github/workflows/ci.yml` (the fork's step).
    - **Source**: https://github.com/KhronosGroup/glslang
    - **Notes**: BSD-3-Clause and others. A test tool only, never in the image.
- **Tests**: `python3 -I kiwami/tests/glsl-precision.py --self-test`; `kiwami/tests/ci.sh .buildroot`. By hand, as a
  sanity check: on `output/rgds-plus/target/usr/share/libretro/shaders/glsl` (upstream's rewrite) it exits 1 with
  about 120 preprocessor errors and about 500 mismatches (the issue's 122 and 504). On the extracted sources it
  reports only the four shaders' mismatches (found: five, *Deviations*). The numbers go in the step's overview, not
  in a test.
- **Commit**: `Shader precision: a checker for the GLSL shaders' two stages, as RetroArch compiles them`

### 2. The shaders as libretro wrote them, the vertex stage at highp only where the fragment's is

- **Files**: `kiwami/glsl-shaders.mk` (new), `kiwami/glsl-shaders.sh` (new), `kiwami/external.mk` (changed),
  `kiwami/Config.in` (changed), `kiwami/configs/kiwami.fragment` (changed), `kiwami/tests/glsl-shaders-shim.mk`
  (new), `kiwami/tests/glsl-shaders.sh` (new), `kiwami/tests/glsl-known.txt` (new), `kiwami/tests/ci.sh` (changed),
  `kiwami/DEPENDENCIES.md` (changed), `docs/FORK.md` (changed), `.workspace/0040-shader-precision/plan.md` (changed:
  Progress).
- **Change**:
  - `kiwami/Config.in`, after `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER`:
    ```
    config BR2_PACKAGE_KIWAMI_GLSL_SHADERS_PRECISION
    	bool "RetroArch's GLSL shaders as libretro wrote them"
    	depends on BR2_PACKAGE_RETROARCH_GLSL_SHADERS
    	help
    	  The shaders in /usr/share/libretro/shaders/glsl as libretro wrote them, the vertex stage at highp only in a
    	  shader whose fragment stage is highp (kiwami/glsl-shaders.sh), instead of upstream's rewrite of every
    	  vertex stage, which most of them then fail to compile or link with. Off, upstream's.
    ```
  - `kiwami/configs/kiwami.fragment`, after RetroArch master's block: `BR2_PACKAGE_KIWAMI_GLSL_SHADERS_PRECISION=y`,
    with a comment (upstream's rewrite breaks most shaders, Settings' *LCD* and *Sharp* among them; off puts it back).
  - `kiwami/glsl-shaders.mk`:
    ```make
    # RetroArch's GLSL shaders (upstream's retroarch-glsl-shaders) as libretro wrote them: after upstream's install,
    # whose rewrite puts every vertex stage at highp, kiwami/glsl-shaders.sh copies the sources back and raises only
    # the vertex stages whose fragment stage is highp. Included by external.mk, and by kiwami/tests/glsl-shaders-shim.mk.
    ifeq ($(BR2_PACKAGE_KIWAMI_GLSL_SHADERS_PRECISION),y)
    define KIWAMI_GLSL_SHADERS_PRECISION
    	sh $(BR2_EXTERNAL_KIWAMI_PATH)/glsl-shaders.sh $(@D) $(TARGET_DIR)/usr/share/libretro/shaders/glsl
    endef
    RETROARCH_GLSL_SHADERS_POST_INSTALL_TARGET_HOOKS += KIWAMI_GLSL_SHADERS_PRECISION
    endif
    ```
    The hooks are expanded when the install step runs (`.buildroot/package/pkg-generic.mk:372`), after upstream's
    `.mk` has been read, as RetroArch's `KIWAMI_RETROARCH_VERSION_FILE` is.
  - `kiwami/external.mk`, at the end: a comment line and `include $(BR2_EXTERNAL_KIWAMI_PATH)/glsl-shaders.mk`.
  - `kiwami/glsl-shaders.sh <source> <installed>`:
    - It stops (exit 1, `glsl-shaders.sh: ...`) when either folder is missing, when the source holds no `.glsl`, or
      when a source `.glsl` isn't installed (naming it: upstream's install changed).
    - Otherwise, for each `.glsl` in the source (`find`, sorted), it copies the file over the installed one
      (`cp -p`, as upstream's `cp -a`). Then it finds the lines to raise with one awk over the file read twice:
      ```sh
      awk 'FNR == 1 { pass++; s = "" }
           /defined\(VERTEX\)/ { s = "v" }  /defined\(FRAGMENT\)/ { s = "f" }
           pass == 1 && /^[ \t]*#[ \t]*define[ \t]+COMPAT_PRECISION[ \t]+highp/ { if (s == "f") hi = 1; if (s == "v") vhi = 1 }
           pass == 2 && hi && !vhi && s == "v" && /^[ \t]*#[ \t]*define[ \t]+COMPAT_PRECISION[ \t]+mediump/ { print FNR }' "$f" "$f"
      ```
    - Each line it prints gets `sed -i "<n>s/mediump/highp/"` in the installed copy (GNU sed keeps a missing final
      newline, so every other byte stays the source's).
    - It ends with one line: `glsl-shaders.sh: <n> shaders as libretro wrote them, the vertex stage at highp in
      <k>: <paths>`.
  - `kiwami/tests/glsl-shaders-shim.mk`: upstream's package outside Buildroot.
    - It defines what the `.mk` uses from Buildroot: `github` as Buildroot's (`https://github.com/$(1)/$(2)/archive/$(3)`),
      `generic-package` empty, `sep` as a newline, and `BR2_EXTERNAL_KIWAMI_PATH` as `kiwami`'s absolute path.
    - It includes `buildroot/package/retroarch-glsl-shaders/retroarch-glsl-shaders.mk` and `kiwami/glsl-shaders.mk`.
    - Two targets:
      - `version`, which prints `$(RETROARCH_GLSL_SHADERS_VERSION)`;
      - `$(SRC)/.install` (so `$(@D)` is the source), which runs `$(RETROARCH_GLSL_SHADERS_INSTALL_TARGET_CMDS)` and
        then `$(foreach hook,$(RETROARCH_GLSL_SHADERS_POST_INSTALL_TARGET_HOOKS),$(call $(hook))$(sep))`, as
        `pkg-generic.mk:364-372` does.
    - It is run as `make -s -f kiwami/tests/glsl-shaders-shim.mk SRC=<dir> TARGET_DIR=<dir>
      [BR2_PACKAGE_KIWAMI_GLSL_SHADERS_PRECISION=y] <SRC>/.install`.
  - `kiwami/tests/glsl-shaders.sh <buildroot dir>`, or `--output <output dir>`:
    - **With a Buildroot folder**, it takes the version from the shim. The tarball is
      `<buildroot>/dl/retroarch-glsl-shaders/retroarch-glsl-shaders-<version>.tar.gz` if it's there (the local
      `.buildroot`); otherwise it is fetched from `https://github.com/libretro/glsl-shaders/archive/<version>.tar.gz`
      (CI). It is extracted into a temporary folder (`--strip-components=1`). Then:
      1. **Off**: the shim without the option into `$T/off`. Then `glsl-precision.py` on it must exit 1. If it exits
         0: `glsl-shaders: upstream's shaders pass the check: see whether the fork's hook is still needed
         (docs/FORK.md)`, exit 1.
      2. **On**: the shim with the option into `$T/on`. `glsl-precision.py --known kiwami/tests/glsl-known.txt` on it
         must exit 0.
      3. **On against the source**: every `.glsl` in `$T/on` equals the source's (`cmp`), except those
         `glsl-shaders.sh` named. Each of those has a fragment stage defining `COMPAT_PRECISION highp` and a vertex
         stage defining none, and differs
         from the source only in lines that read `#define COMPAT_PRECISION mediump` in the source and `highp` in
         `$T/on` (`diff` of the two, every changed pair of that shape). At least one was named.
      4. **On against off**: every file but the `.glsl` ones (presets, textures) is the same in both (`diff -r`
         excluding `*.glsl`).
      5. **A file missing**: with one installed `.glsl` removed from a copy of `$T/on`, `kiwami/glsl-shaders.sh` exits
         1 and names it.
    - **With `--output`**, it runs `glsl-precision.py --known kiwami/tests/glsl-known.txt` on
      `<output>/target/usr/share/libretro/shaders/glsl` only.
  - `kiwami/tests/glsl-known.txt` (added, *Deviations*): the known faults of libretro's sources at the pin, each with
    why. Today `gpu/shaders/powervr2.glsl: FrameCount` and `: FrameDirection`, declared `uniform int` without a
    precision in both stages, so `highp` by default in the vertex stage and `mediump` in the fragment stage. Neither
    upstream's rewrite nor the fork's rule touches them. M2 shows what Panfrost does with them.
    - It ends with `glsl-shaders: upstream's <e> errors and <m> mismatches, the fork's none (<k> vertex stages
      raised)`, or with the output's summary.
  - `kiwami/tests/ci.sh`: replaces step 1's self-test block with the self-test followed by `kiwami/tests/glsl-shaders.sh
    "$BR"`, under the same `glslangValidator` test and skip line.
  - `kiwami/DEPENDENCIES.md`, *Upstream's, that the fork depends on*: libretro's glsl-shaders.
    - **Version**: `ce9a4124`.
    - **Pinned in**: `buildroot/package/retroarch-glsl-shaders/retroarch-glsl-shaders.mk`
      (`RETROARCH_GLSL_SHADERS_VERSION`).
    - **What of the fork's depends on it**: `kiwami/glsl-shaders.mk` and `.sh` restore its `.glsl` files over
      upstream's install, and `kiwami/tests/glsl-shaders.sh` checks that version. A bump is checked by that test in
      CI.
  - `docs/FORK.md`:
    - *What the fork shadows*: `buildroot/package/retroarch-glsl-shaders/` is added to the `git diff $OLD main` list,
      with its paragraph: what the hook does, what to read in an upstream change (its install, the variable, the pin),
      and what to do if upstream fixes it itself. That case is `glsl-shaders.sh`'s "upstream's shaders pass the
      check": drop the fork's option, `.mk`, script and test.
    - *How the build takes in the fork*: one sentence that `kiwami/external.mk` includes `kiwami/glsl-shaders.mk`.
- **Tests**:
  - **The test fails first**: with the hook's include and fragment line written but `glsl-shaders.sh` still a stub
    that changes nothing, `kiwami/tests/glsl-shaders.sh .buildroot` fails at step 2 with the checker's mismatches
    (`.../pixel_transparency.glsl: OutputSize: vertex highp, fragment mediump (GLSL ES 1.00)` among them). With the
    script it passes.
  - `kiwami/tests/ci.sh .buildroot` passes: `sh -n` on the new scripts, every board configured with the new option
    (A6), the checker's self-test and `glsl-shaders.sh`.
  - By hand: `make retroarch-glsl-shaders-dirclean-rgds-plus`, then `make retroarch-glsl-shaders-rgds-plus`; the
    build's log shows `glsl-shaders.sh`'s line naming the four shaders, and the package's own target
    (`output/rgds-plus/per-package/retroarch-glsl-shaders/target/usr/share/libretro/shaders/glsl`) passes
    `glsl-precision.py --known kiwami/tests/glsl-known.txt`. This checks `external.mk`'s include, which the shim
    doesn't read. (`--output` reads `target/`, which per-package directories fill only when the image is built: it
    belongs to the final check, *Deviations*.)
- **Commit**: `Shader precision: RetroArch's GLSL shaders as libretro wrote them, the vertex stage at highp only where the fragment's is`

### 3. Final check

- `kiwami/tests/ci.sh .buildroot` (the checker's self-test, `glsl-shaders.sh`, every board configured).
  `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`: unaffected (nothing of theirs changed),
  run once to confirm.
- The hooks: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` (the commands in `docs/FORK.md`) agree.
  The only upstream file in the diff that this feature adds to is `.github/workflows/ci.yml`, in the fork's own step
  (A10).
- The option off: `kiwami/tests/glsl-shaders.sh`'s off half shows the shaders are then upstream's install's (A3).
  `make test`'s renders are untouched (no banana-ui change), so they aren't compared.
- `kiwami/DEPENDENCIES.md` lists glslang (step 1) and libretro's glsl-shaders (step 2).
- `docs/FORK.md` updated (step 2). `docs/ARCHITECTURE.md` and `CLAUDE.md` don't describe the shaders' install.
- The image: `make retroarch-glsl-shaders-dirclean-rgds-plus`, then `make rgds-plus`.
  `kiwami/tests/glsl-shaders.sh --output output/rgds-plus` passes (A1, A2 in the image). The image is installed on
  the RG DS Plus by its update package, over the current one, on the card whose Shader setting was set to *LCD* in
  M1's first part.
- The manual tests below all passed.
- Every acceptance criterion is met.
- **Commit**: `Shader precision: the final check done`

## Automated tests

| Test | Where | What it checks | Criteria |
|---|---|---|---|
| The checker's self-test | `kiwami/tests/glsl-precision.py --self-test` (run by `ci.sh`) | Made-up shaders: the version mapping and stripping, the defines, a redefined macro, mismatches at ES 1.00 and 3.00, default precisions (`float` and `int`), several declarators, clean shaders passing, `--known`'s excused and gone faults, an empty folder | A4 |
| Upstream's install | `kiwami/tests/glsl-shaders.sh` (run by `ci.sh`), the off half | The pinned shaders through upstream's real install fail the check. If they pass, CI says upstream may have fixed it | A3 |
| The fork's hook | `kiwami/tests/glsl-shaders.sh`, the on half | The same shaders, with the hook, pass the check (`glsl-known.txt`'s faults excused, each still found). Every `.glsl` is libretro's but the raised vertex lines, only in shaders whose fragment stage is `highp`. Other files are as upstream installs them. A missing file stops the hook | A1, A2, A5 |
| The boards' configuration | `kiwami/tests/ci.sh` (its existing configure of every board) | The new option survives on `rgds`, `rgds-plus`, `universal`, and `rgds_plus` with `KIWAMI_SERIAL=1` | A6 |
| The image | `kiwami/tests/glsl-shaders.sh --output output/rgds-plus` (final check) | The built image's shaders pass the check (the include line read, the package reinstalled) | A1, A2 |
| The hooks | `git diff --stat main...HEAD`, `git grep -n 'kiwami:'` (final check) | Only the fork's CI step changed among upstream's files | A10 |

## Manual tests

On the device, for what headless tests can't reach. Logs are fetched over SSH/SFTP (Settings > Network > SSH) or from
the card on a computer (`banana/logs/` on the card is `/data/banana/logs/` on the device). Only the RG DS Plus is at
hand: every test runs on it, and the RG DS isn't tested.

RetroArch's log is `/run/banana/retroarch.log`. It is in RAM, written with `--verbose`, and replaced when the next
game starts (`run-retroarch:375-380`). Copy it over SSH right after quitting each game, before starting another, named
after the test (`retroarch-M1-lcd.log`, ...). The lines to look at are `[GLSL]` and any `Failed`. In RetroArch, turn
on Settings > On-Screen Display > Notifications > Show FPS (Display Framerate) for M3.

| # | Board | Steps | Expected | Logs to collect | Criteria | Outcome |
|---|---|---|---|---|---|---|
| M1 | rgds-plus | **Before installing the new image**, on the current one: Settings > Other systems > Picture & sound > Shader *LCD*, start a GBA game, look, quit. **After installing it** (update package, same card): start the same game without touching Settings. Then set *Sharp*, start it, quit; then *CRT*, start it, quit. | Before: no LCD grid, "Failed to compile shader" (the bug, and the card's setting as an older image left it). After: the LCD grid, then sharp scaling, then scanlines, each with no failure message | `retroarch.log` after each of the four runs (`retroarch-M1-before.log`, `-lcd.log`, `-sharp.log`, `-crt.log`) | A7 | not run |
| M2 | rgds-plus | With Shader *Off* in Settings, start a GBA game. In Quick Menu > Shaders > Load Preset (from `/usr/share/libretro/shaders/glsl`), load in turn `handheld/pixel_transparency.glslp`, `crt/fakelottes.glslp`, `nearest.glslp`, `handheld/gba-color.glslp`, `xbr/xbr-lv2.glslp`, `crt/zfast_crt_nogeo_svideo.glslp`, `interpolation/pixellate.glslp`, going back to the game after each. Last, `gpu/powervr2.glslp` (libretro's own int mismatch, `glsl-known.txt`). | Each of the seven loads with no "Failed to compile shader", and the picture changes as the shader should (`nearest` is plain, `gba-color` changes the colours). `powervr2`: noted either way. If it fails, its `[GLSL]` lines say why, and fixing it is a follow-up outside this feature. | `retroarch.log` after quitting (one run, all eight loads in it: `retroarch-M2.log`) | A8, A9 | not run |
| M3 | rgds-plus | With Show FPS on, in a GBA game: `crt/zfast_crt_nogeo_svideo.glslp` loaded, play a minute; then Settings' *LCD* (M1's), play a minute. | 60 fps (or the game's rate) with each, as `d1c7383b` measured for zfast_crt_nogeo_svideo | The fps seen; `retroarch.log` (`retroarch-M3.log`) | A9 | not run |

Notes from the runs (what the logs showed, what was fixed):

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

- **2026-11-18, step 1: a fifth mismatched shader in libretro's own sources, excused by a known list.** The checker
  finds upstream's image as the issue did (122 shaders with a preprocessor error, 504 with a mismatch). On the
  original sources it finds the four shaders `d1c7383b` was for, and also `gpu/shaders/powervr2.glsl`. That shader
  declares `uniform int FrameDirection; uniform int FrameCount;` without a precision in both stages. An `int`'s
  default is `highp` in a vertex stage and `mediump` in a fragment stage, so the two disagree. The issue's check
  didn't model `int`'s defaults. The fault is libretro's, not upstream's rewrite's, and the rule (which changes only
  `COMPAT_PRECISION`) doesn't touch it. Decided with the user:
  - the checker gets `--known <file>`; the fault is listed in `kiwami/tests/glsl-known.txt` (step 2), and a listed
    fault that disappears fails the check, so a pin bump that fixes it is noticed;
  - `gpu/powervr2.glslp` is loaded in M2 to see what Panfrost does with it, and a fix, if one is needed, is a
    follow-up.
  A2 and steps 1 and 2 are updated to match.
- **2026-11-18, step 2: the rule raises a vertex stage only when it defines no `highp` of its own.** The planned rule
  (the fragment stage defines `COMPAT_PRECISION highp` → raise the vertex stage's `mediump` lines) raised 17
  shaders. 13 of them (`zfast_crt`, `zfast_crt_nogeo`, the nine `simpletex_lcd*`, `mix_frames` and
  `mix_frames_smart`) already define `highp` in their vertex stage under `#ifdef GL_FRAGMENT_PRECISION_HIGH`, so they
  match on the Mali-G52, and the rule changed only their dead `#else` line. Decided with the user: a vertex stage is
  raised only when its fragment stage defines `highp` and it defines none itself. That is exactly the four shaders
  (A1), and every other shader, Settings' *CRT* (`zfast_crt`) included, stays byte for byte libretro's. The test
  checks both conditions for each raised shader. The summary, A1 and step 2's sketch are updated.
- **2026-11-18, step 2: the by-hand check reads the package's per-package target.** With per-package directories,
  `make retroarch-glsl-shaders-rgds-plus` installs into `per-package/retroarch-glsl-shaders/target/`, and
  `output/rgds-plus/target/` is put together from those only by the image's build. So `glsl-shaders.sh --output`
  run after the package alone still saw upstream's shaders. Step 2 checks the per-package target instead (passed),
  and `--output` stays in the final check, after `make rgds-plus`.

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. A checker for the shaders' two stages | done | `Shader precision: a checker for the GLSL shaders' two stages, as RetroArch compiles them` |
| 2. The shaders as libretro wrote them, the vertex stage at highp only where the fragment's is | done | `Shader precision: RetroArch's GLSL shaders as libretro wrote them, the vertex stage at highp only where the fragment's is` |
| 3. Final check | todo | |
