CC := gcc
CFLAGS := -m32 -g -O0 -Wall -std=gnu89 \
    -DLINUX \
    -D_LANGUAGE_C \
    -D_FINALROM \
    -DVERSION=us \
    -DVERSION_US \
    -DF3DEX_GBI_2 \
    -D_MIPS_SZLONG=32 \
    -DNON_MATCHING \
    -DSHIFT \
    -Iver/us/include \
    -Iver/us/build/include \
    -Iinclude \
    -Isrc \
    -Iassets/us \
    -I/usr/include/SDL2

BUILD_DIR := build_pc
TARGET := papermario

# Files that are #included by other .c files (not compiled separately)
EXCLUDE_SRCS := \
    %npc_composer.c \
    %npc_hint_dryite.c \
    %npc_hint_dryite_companion.c \
    %npc_shop_owner.c \
    %npc_merlee.c \
    %npc_moustafa.c \
    %kzn_19_anim1.c \
    %kzn_19_anim2.c \
    %kzn_19_anim3.c \
    %hos_10/narrator.c

# Dead code dirs not compiled by ninja
EXCLUDE_DEAD := \
    src/world/dead/area_flo/flo_21/% \
    src/world/dead/area_flo/flo_22/% \
    src/world/dead/area_flo/flo_23/% \
    src/world/dead/area_flo/flo_24/% \
    src/world/dead/area_flo/flo_25/% \
    src/world/dead/area_kzn/kzn_11/%

# World sources - exclude all dead code
WORLD_SRCS := $(filter-out %.inc.c $(EXCLUDE_SRCS) src/world/dead/%, $(wildcard src/world/*.c src/world/**/*.c src/world/**/**/*.c src/world/**/**/**/*.c))

# Battle sources
BATTLE_SRCS := $(filter-out %.inc.c, $(wildcard src/battle/*.c src/battle/**/*.c src/battle/**/**/*.c src/battle/**/**/**/*.c src/battle/**/**/**/**/*.c))

# Common sources
COMMON_SRCS := $(filter-out %.inc.c, $(wildcard src/common/*.c))

# Core sources
CORE_SRCS := \
    src/101b90_len_8f0.c \
    src/25AF0.c \
    src/38F00.c \
    src/43F0.c \
    src/5B320.c \
    src/77480.c \
    src/7B440.c \
    src/7BB60.c \
    src/7E9D0.c \
    src/8a860_len_3f30.c \
    src/animator.c \
    src/background.c \
    src/background_gfx.c \
    src/cam_main.c \
    src/cam_math.c \
    src/cam_mode_interp.c \
    src/cam_mode_minimal.c \
    src/cam_mode_no_interp.c \
    src/cam_mode_unused_ahead.c \
    src/cam_mode_unused_leading.c \
    src/cam_mode_unused_radial.c \
    src/cam_mode_zone_interp.c \
    src/collision.c \
    src/crash_screen.c \
    src/create_audio_system_obfuscated.c \
    src/curtains.c \
    src/draw_box.c \
    src/draw_img_util.c \
    src/effects.c \
    src/effect_utils.c \
    src/encounter_api.c \
    src/encounter.c \
    src/enemy_ai.c \
    src/entity.c \
    src/entity_model.c \
    src/fio.c \
    src/game_modes.c \
    src/game_states.c \
    src/general_heap_create_obfuscated.c \
    src/global_hud_scripts.c \
    src/heap.c \
    src/heaps.c \
    src/heaps2.c \
    src/heaps3.c \
    src/hud_element.c \
    src/imgfx.c \
    src/input.c \
    src/inspect_icon.c \
    src/inventory.c \
    src/i_spy.c \
    src/item_entity.c \
    src/level_up.c \
    src/load_engine_data_obfuscated.c \
    src/load_obfuscation_shims.c \
    src/main.c \
    src/main_loop.c \
    src/main_pre.c \
    src/menu_hud_scripts.c \
    src/model.c \
    src/model_clear_render_tasks.c \
    src/msg.c \
    src/msg_data.c \
    src/msg_draw.c \
    src/msg_img.c \
    src/npc.c \
    src/npc_collision.c \
    src/npc_follow.c \
    src/obfuscation_shims.c \
    src/pulse_stone.c \
    src/rumble.c \
    src/screen_overlays.c \
    src/screen_render_util.c \
    src/speech_bubble.c \
    src/sprite.c \
    src/sprite_shading.c \
    src/starpoint.c \
    src/startup_screen_fading.c \
    src/state_battle.c \
    src/state_demo.c \
    src/state_file_select.c \
    src/state_intro.c \
    src/state_logos.c \
    src/state_map_transitions.c \
    src/state_pause.c \
    src/state_startup.c \
    src/state_title_screen.c \
    src/state_world.c \
    src/status_icons.c \
    src/status_star_shimmer.c \
    src/texture_memory.c \
    src/trigger.c \
    src/vars_access.c \
    src/windows.c \
    src/worker.c \
    src/battle_heap_create_obfuscated.c \
    src/battle_ui_gfx.c \
    src/world_use_item.c

# Audio
AUDIO_SRCS := \
    src/audio/ambience.c \
    src/audio/bgm_control.c \
    src/audio/bgm_player.c \
    src/audio/core/engine.c \
    src/audio/core/pull_voice.c \
    src/audio/core/reverb.c \
    src/audio/core/syn_driver.c \
    src/audio/core/system.c \
    src/audio/core/tables.c \
    src/audio/core/voice.c \
    src/audio/load_banks.c \
    src/audio/mseq_player.c \
    src/audio/sfx_control.c \
    src/audio/sfx_player.c \
    src/audio/snd_interface.c

# BSS
BSS_SRCS := \
    src/bss/engine1_post_bss.c \
    src/bss/engine1_pre_bss.c \
    src/bss/engine2_post_bss.c \
    src/bss/engine2_pre_bss.c \
    src/bss/main_post_bss.c \
    src/bss/main_pre_bss.c

# Charset
CHARSET_SRCS := src/charset/charset.c

# Effects
EFFECTS_SRCS := $(filter-out %.inc.c %_de.c %_pal.c %_fr.c %_es.c, $(wildcard src/effects/*.c src/effects/gfx/*.c))

# Entity
ENTITY_SRCS := $(wildcard src/entity/*.c src/entity/**/*.c)

# EVT scripting
EVT_SRCS := \
    src/evt/audio_api.c \
    src/evt/cam_api.c \
    src/evt/demo_api.c \
    src/evt/evt.c \
    src/evt/f8f60_len_1560.c \
    src/evt/fx_api.c \
    src/evt/item_api.c \
    src/evt/map_api.c \
    src/evt/model_api.c \
    src/evt/msg_api.c \
    src/evt/npc_api.c \
    src/evt/player_api.c \
    src/evt/script_list.c \
    src/evt/virtual_entity.c

# File menu
FILEMENU_SRCS := \
    src/filemenu/filemenu_common.c \
    src/filemenu/filemenu_createfile.c \
    src/filemenu/filemenu_gfx.c \
    src/filemenu/filemenu_info.c \
    src/filemenu/filemenu_main.c \
    src/filemenu/filemenu_msg.c \
    src/filemenu/filemenu_styles.c \
    src/filemenu/filemenu_yesno.c

# GCC support
GCC_SRCS := \
    src/gcc/divdi3.c \
    src/gcc/moddi3.c \
    src/gcc/udivdi3.c \
    src/gcc/umoddi3.c

# Pause menu
PAUSE_SRCS := \
    src/pause/pause_badges.c \
    src/pause/pause_gfx.c \
    src/pause/pause_items.c \
    src/pause/pause_main.c \
    src/pause/pause_map.c \
    src/pause/pause_partners.c \
    src/pause/pause_spirits.c \
    src/pause/pause_stats.c \
    src/pause/pause_styles.c \
    src/pause/pause_tabs.c

# OS/libultra - explicit list from ninja build
OS_SRCS := \
    src/os/afterprenmi.c \
    src/os/ai.c \
    src/os/aigetlength.c \
    src/os/aigetstatus.c \
    src/os/aisetfrequency.c \
    src/os/aisetnextbuf.c \
    src/os/cartrominit.c \
    src/os/contpfs.c \
    src/os/contquery.c \
    src/os/contramread.c \
    src/os/contramwrite.c \
    src/os/contreaddata.c \
    src/os/controller.c \
    src/os/coss.c \
    src/os/crc.c \
    src/os/createthread.c \
    src/os/destroythread.c \
    src/os/devmgr.c \
    src/os/epidma.c \
    src/os/epilinkhandle.c \
    src/os/epirawdma.c \
    src/os/epirawread.c \
    src/os/epirawwrite.c \
    src/os/epiread.c \
    src/os/epiwrite.c \
    src/os/frustum.c \
    src/os/getactivequeue.c \
    src/os/getthreadpri.c \
    src/os/gettime.c \
    src/os/guLookAt.c \
    src/os/guMtxCat.c \
    src/os/guMtxXFMF.c \
    src/os/guMtxXFML.c \
    src/os/guOrtho.c \
    src/os/guRotate.c \
    src/os/gu_matrix.c \
    src/os/initialize.c \
    src/os/jammesg.c \
    src/os/ldiv.c \
    src/os/lookathil.c \
    src/os/lookatref.c \
    src/os/memset.c \
    src/os/motor.c \
    src/os/osFlash.c \
    src/os/osSiDeviceBusy.c \
    src/os/perspective.c \
    src/os/pfsallocatefile.c \
    src/os/pfschecker.c \
    src/os/pfsdeletefile.c \
    src/os/pfsfilestate.c \
    src/os/pfsfindfile.c \
    src/os/pfsfreeblocks.c \
    src/os/pfsgetstatus.c \
    src/os/pfsinitpak.c \
    src/os/pfsisplug.c \
    src/os/pfsnumfiles.c \
    src/os/pfsreadwritefile.c \
    src/os/pfsrepairid.c \
    src/os/pfsselectbank.c \
    src/os/piacs.c \
    src/os/pigetcmdq.c \
    src/os/pimgr.c \
    src/os/pirawdma.c \
    src/os/position.c \
    src/os/resetglobalintmask.c \
    src/os/rotateRPY.c \
    src/os/setglobalintmask.c \
    src/os/seteventmesg.c \
    src/os/setthreadpri.c \
    src/os/settime.c \
    src/os/settimer.c \
    src/os/siacs.c \
    src/os/sins.c \
    src/os/sirawdma.c \
    src/os/sirawread.c \
    src/os/sirawwrite.c \
    src/os/sp.c \
    src/os/spgetstat.c \
    src/os/sprintf.c \
    src/os/sprawdma.c \
    src/os/spsetpc.c \
    src/os/spsetstat.c \
    src/os/sptask.c \
    src/os/sptaskyield.c \
    src/os/sptaskyielded.c \
    src/os/startthread.c \
    src/os/stopthread.c \
    src/os/thread.c \
    src/os/timerintr.c \
    src/os/vi.c \
    src/os/viblack.c \
    src/os/vigetcurrcontext.c \
    src/os/vigetcurrframebuf.c \
    src/os/vigetmode.c \
    src/os/vigetnextframebuf.c \
    src/os/vimgr.c \
    src/os/vimodempallan1.c \
    src/os/vimodentsclan1.c \
    src/os/vimodepallan1.c \
    src/os/virepeatline.c \
    src/os/visetevent.c \
    src/os/visetmode.c \
    src/os/visetspecial.c \
    src/os/visetyscale.c \
    src/os/viswapbuf.c \
    src/os/viswapcontext.c \
    src/os/virtualtophysical.c \
    src/os/vitbl.c \
    src/os/xldtob.c \
    src/os/xlitob.c \
    src/os/xprintf.c \
    src/os/yieldthread.c

# OS/nusys
NUSYS_SRCS := \
    src/os/nusys/nuboot.c \
    src/os/nusys/nucontdataget.c \
    src/os/nusys/nucontdatalock.c \
    src/os/nusys/nucontinit.c \
    src/os/nusys/nucontmgr.c \
    src/os/nusys/nucontpakmgr.c \
    src/os/nusys/nucontqueryread.c \
    src/os/nusys/nucontrmbcheck.c \
    src/os/nusys/nucontrmbforcestop.c \
    src/os/nusys/nucontrmbforcestopend.c \
    src/os/nusys/nucontrmbmgr.c \
    src/os/nusys/nucontrmbmodeset.c \
    src/os/nusys/nucontrmbstart.c \
    src/os/nusys/nugfxdisplayoff.c \
    src/os/nusys/nugfxdisplayon.c \
    src/os/nusys/nugfxfuncset.c \
    src/os/nusys/nugfxinit.c \
    src/os/nusys/nugfxprenmifuncset.c \
    src/os/nusys/nugfxretracewait.c \
    src/os/nusys/nugfxsetcfb.c \
    src/os/nusys/nugfxswapcfb.c \
    src/os/nusys/nugfxswapcfbfuncset.c \
    src/os/nusys/nugfxtaskallendwait.c \
    src/os/nusys/nugfxtaskmgr.c \
    src/os/nusys/nugfxthread.c \
    src/os/nusys/nupiinit.c \
    src/os/nusys/nupireadromoverlay.c \
    src/os/nusys/nusched.c \
    src/os/nusys/nusicallbackadd.c \
    src/os/nusys/nusicallbackremove.c \
    src/os/nusys/nusimgr.c

# Linux/PC specific
LINUX_SRCS := $(wildcard src/linux/*.c)

# Generated sources
GENERATED_SRCS := ver/us/build/src/fx_wrappers.c

# All sources
ALL_SRCS := \
    $(CORE_SRCS) \
    $(AUDIO_SRCS) \
    $(BSS_SRCS) \
    $(CHARSET_SRCS) \
    $(COMMON_SRCS) \
    $(EFFECTS_SRCS) \
    $(ENTITY_SRCS) \
    $(EVT_SRCS) \
    $(FILEMENU_SRCS) \
    $(GCC_SRCS) \
    $(PAUSE_SRCS) \
    $(OS_SRCS) \
    $(NUSYS_SRCS) \
    $(BATTLE_SRCS) \
    $(WORLD_SRCS) \
    $(LINUX_SRCS) \
    $(GENERATED_SRCS)

# Objects - handle both src/ and ver/ paths
SRC_OBJS := $(patsubst src/%.c,$(BUILD_DIR)/src/%.o,$(filter src/%,$(ALL_SRCS)))
VER_OBJS := $(patsubst ver/%.c,$(BUILD_DIR)/ver/%.o,$(filter ver/%,$(ALL_SRCS)))
ALL_OBJS := $(SRC_OBJS) $(VER_OBJS)

.PHONY: all clean info

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CC) -no-pie -m32 $(ALL_OBJS) -o $@ -lSDL2 -lGL -lm -Wl,--just-symbols=assets_le/pc_rom_addrs.ld


$(BUILD_DIR)/src/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ver/%.o: ver/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

info:
	@echo "Total sources: $(words $(ALL_SRCS))"
	@echo "Core: $(words $(CORE_SRCS))"
	@echo "Audio: $(words $(AUDIO_SRCS))"
	@echo "Battle: $(words $(BATTLE_SRCS))"
	@echo "Common: $(words $(COMMON_SRCS))"
	@echo "World: $(words $(WORLD_SRCS))"
	@echo "Effects: $(words $(EFFECTS_SRCS))"
	@echo "Entity: $(words $(ENTITY_SRCS))"
	@echo "OS: $(words $(OS_SRCS))"
	@echo "Nusys: $(words $(NUSYS_SRCS))"
	@echo "Linux: $(words $(LINUX_SRCS))"
	@echo "Generated: $(words $(GENERATED_SRCS))"
