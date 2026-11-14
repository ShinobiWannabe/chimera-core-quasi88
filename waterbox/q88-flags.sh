# q88-flags.sh - the sources and compile flags both builds share, sourced by
# build-native.sh and build-guest.sh so the two cannot drift apart.
#
# The sources are upstream's CMakeLists.txt list with sound and fmgen on,
# less its system layer (src/sysdepend/sdl2, src/osdepend/unix and the MAME
# layer's SDL audio), which waterbox/port replaces. USE_MONITOR (upstream's
# debugging monitor, which reads the host's terminal) is left off.
#
# Variables in: Q (the upstream tree), PORT (waterbox/port), HERE (waterbox).

S=$Q/src
Q88_SOURCES="
$S/alarm.c $S/control.c $S/debug.c $S/emu.c $S/fname.c $S/getconf.c $S/icon.c $S/menu.c
$S/monitor.c $S/pause.c $S/quasi88.c $S/utility.c
$S/pc88/basic.c $S/pc88/crtcdmac.c $S/pc88/fdc.c $S/pc88/image.c $S/pc88/intr.c $S/pc88/keyboard.c
$S/pc88/memory.c $S/pc88/pc88main.c $S/pc88/pc88sub.c $S/pc88/pio.c $S/pc88/romaji.c $S/pc88/soundbd.c
$S/pc88/suspend.c $S/pc88/z80.c $S/pc88/z80-debug.c
$S/screen/color.c $S/screen/menu-screen.c $S/screen/putimage.c $S/screen/screen.c
$S/screen/screen-8bpp.c $S/screen/screen-16bpp.c $S/screen/screen-32bpp.c
$S/screen/screen-snapshot.c $S/screen/snapshot.c
$S/tk/q8tk-draw.c $S/tk/q8tk-event.c $S/tk/q8tk-glib.c $S/tk/q8tk.c
$S/tk/q8tk/q8tk-accel.c $S/tk/q8tk/q8tk-adjustment.c $S/tk/q8tk/q8tk-button.c $S/tk/q8tk/q8tk-combo.c
$S/tk/q8tk/q8tk-common.c $S/tk/q8tk/q8tk-container.c $S/tk/q8tk/q8tk-entry.c $S/tk/q8tk/q8tk-frame.c
$S/tk/q8tk/q8tk-fselection.c $S/tk/q8tk/q8tk-fviewer.c $S/tk/q8tk/q8tk-grab.c $S/tk/q8tk/q8tk-item.c
$S/tk/q8tk/q8tk-label.c $S/tk/q8tk/q8tk-listbox.c $S/tk/q8tk/q8tk-misc.c $S/tk/q8tk/q8tk-notebook.c
$S/tk/q8tk/q8tk-signal.c $S/tk/q8tk/q8tk-widget.c $S/tk/q8tk/q8tk-window.c
$S/ui/askopenfile-top.c $S/ui/askquit-top.c $S/ui/askreset-top.c $S/ui/askselectdisk-top.c
$S/ui/askspeedup-top.c $S/ui/askstatefile-top.c $S/ui/menu-top.c $S/ui/pause-top.c
$S/ui/statusbar.c $S/ui/toolbar.c
$S/mon/mon-arg.c $S/mon/mon-cmd.c $S/mon/mon-debug.c $S/mon/mon-image.c $S/mon/mon-memory.c
$S/mon/mon-misc.c $S/mon/mon-set.c $S/mon/mon-util.c
$S/ui/menu/menu-about.c $S/ui/menu/menu-common.c $S/ui/menu/menu-cpu.c $S/ui/menu/menu-disk.c
$S/ui/menu/menu-graph.c $S/ui/menu/menu-key.c $S/ui/menu/menu-misc.c $S/ui/menu/menu-mouse.c
$S/ui/menu/menu-reset.c $S/ui/menu/menu-tape.c $S/ui/menu/menu-volume.c
$S/snddrv/xmame/quasi88/mame-quasi88.c $S/snddrv/xmame/quasi88/beep.c $S/snddrv/xmame/quasi88/beepintf.c
$S/snddrv/xmame/quasi88/pcg.c $S/snddrv/xmame/quasi88/pcgintf.c
$S/snddrv/xmame/src/driver.c $S/snddrv/xmame/src/restrack.c $S/snddrv/xmame/src/sound.c
$S/snddrv/xmame/src/sndintrf.c $S/snddrv/xmame/src/streams.c
$S/snddrv/xmame/src/sound/flt_vol.c $S/snddrv/xmame/src/sound/flt_rc.c $S/snddrv/xmame/src/sound/wavwrite.c
$S/snddrv/xmame/src/sound/2203intf.c $S/snddrv/xmame/src/sound/2608intf.c $S/snddrv/xmame/src/sound/ay8910.c
$S/snddrv/xmame/src/sound/fm.c $S/snddrv/xmame/src/sound/ymdeltat.c $S/snddrv/xmame/src/sound/samples.c
$S/snddrv/xmame/quasi88/2203fmgen.cpp $S/snddrv/xmame/quasi88/2608fmgen.cpp
$S/snddrv/fmgen/fmgen.cpp $S/snddrv/fmgen/fmtimer.cpp $S/snddrv/fmgen/opna.cpp $S/snddrv/fmgen/psg.cpp
$PORT/audio.c $PORT/clock.c $PORT/file-op.c $PORT/graph.c $PORT/system.c
$HERE/q88-driver.c
"

# Upstream's defines (CMakeLists.txt: QUASI88_SDL2 aside), sound with fmgen,
# and CHIMERA_CORE for the hooks of patches/.
Q88_DEFINES="-DCHIMERA_CORE -DHAVE_INTPTR_T -DCLIB_DECL= -DUSE_SOUND -DUSE_FMGEN -DPI=M_PI"

# waterbox/port first: its config.h, device.h and filename.h are the system
# layer's headers.
Q88_INCLUDES="-I$PORT -I$HERE -I$S -I$S/pc88 -I$S/screen -I$S/screen/func -I$S/screen/func/macro \
 -I$S/tk -I$S/tk/q8tk -I$S/tk/q8tk/q8tk -I$S/ui -I$S/ui/menu -I$S/mon -I$S/snddrv -I$S/sysdepend -I$S/osdepend \
 -I$S/snddrv/xmame -I$S/snddrv/xmame/quasi88 -I$S/snddrv/xmame/src -I$S/snddrv/xmame/src/sound -I$S/snddrv/fmgen"

# Every local variable starts at zero, as in the NP2kai core: a local the
# code never fully sets cannot carry what was on the stack into the machine
# (and the stack differs between the native build, Linux and Windows).
Q88_ZEROINIT="-ftrivial-auto-var-init=zero"
Q88_CFLAGS="-fcommon -Wno-implicit-function-declaration -Wno-incompatible-pointer-types -Wno-int-conversion $Q88_ZEROINIT"
Q88_CXXFLAGS="-fno-sized-deallocation $Q88_ZEROINIT"

# Per-file flags. pc88main.c's calendar clock reads the host's time; there
# the names are changed to port/clock.c's, which give the Start Time setting
# plus the emulated time, in UTC.
q88_file_flags() {
  case "$1" in
    */pc88/pc88main.c) echo "-Dtime=q88_time -Dlocaltime=q88_localtime -Dmktime=q88_mktime" ;;
  esac
}
