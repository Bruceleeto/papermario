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
    -Iassets/us
    
BUILD_DIR := build_pc
TARGET := papermario

# Files that are #included by other .c files (not compiled separately)
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

# World sources
# World sources - exclude all dead code
WORLD_SRCS := $(filter-out %.inc.c $(EXCLUDE_SRCS) src/world/dead/%, $(wildcard src/world/*.c src/world/**/*.c src/world/**/**/*.c src/world/**/**/**/*.c))

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
    src/is_debug.c \
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
    src/worker.c

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
ENTITY_SRCS := $(wildcard src/entity/*.c src/entity/**/*.c src/entity/model/*.c)

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

# OS/libultra 
OS_SRCS := $(wildcard src/os/*.c src/os/nusys/*.c)

# All sources
ALL_SRCS := \
    $(CORE_SRCS) \
    $(AUDIO_SRCS) \
    $(BSS_SRCS) \
    $(CHARSET_SRCS) \
    $(EFFECTS_SRCS) \
    $(ENTITY_SRCS) \
    $(EVT_SRCS) \
    $(FILEMENU_SRCS) \
    $(GCC_SRCS) \
    $(PAUSE_SRCS) \
    $(OS_SRCS) \
    $(BATTLE_SRCS) \
    $(WORLD_SRCS)

# Objects
ALL_OBJS := $(patsubst src/%.c,$(BUILD_DIR)/src/%.o,$(ALL_SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CC) -m32 $(ALL_OBJS) -o $@ -lm

$(BUILD_DIR)/src/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

info:
	@echo "Total sources: $(words $(ALL_SRCS))"
	@echo "Battle: $(words $(BATTLE_SRCS))"
	@echo "World: $(words $(WORLD_SRCS))"
	@echo "Effects: $(words $(EFFECTS_SRCS))"
	@echo "OS: $(words $(OS_SRCS))"
