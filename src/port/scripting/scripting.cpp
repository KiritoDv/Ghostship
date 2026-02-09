#include "scripting.h"
#include "port/hooks/Events.h"
#include <spdlog/spdlog.h>

#include <fstream>
#include <filesystem>
#include "sm64.h"
#include "macros.h"
#include "port/ui/UIWidgets.hpp"
#include "src/buffers/gfx_output_buffer.h"
#include "src/buffers/buffers.h"
#include "src/buffers/zbuffer.h"
#include "src/buffers/framebuffers.h"
#include "src/game/main.h"
#include "src/game/obj_behaviors_2.h"
#include "src/game/segment7.h"
#include "src/game/mario_actions_stationary.h"
#include "src/game/mario_actions_moving.h"
#include "src/game/sound_init.h"
#include "src/game/skybox.h"
#include "src/game/hud.h"
#include "src/game/mario_actions_automatic.h"
#include "src/game/interaction.h"
#include "src/game/mario_step.h"
#include "src/game/camera.h"
#include "src/game/envfx_bubbles.h"
#include "src/game/mario_actions_submerged.h"
#include "src/game/ingame_menu.h"
#include "src/game/mario_misc.h"
#include "src/game/screen_transition.h"
#include "src/game/moving_texture.h"
#include "src/game/level_geo.h"
#include "src/game/area.h"
#include "src/game/object_collision.h"
#include "src/game/platform_displacement.h"
#include "src/game/rumble_init.h"
#include "src/game/mario_actions_object.h"
#include "src/game/level_update.h"
#include "src/game/decompress.h"
#include "src/game/mario.h"
#include "src/game/rendering_graph_node.h"
#include "src/game/save_file.h"
#include "src/game/print.h"
#include "src/game/geo_misc.h"
#include "src/game/shadow.h"
#include "src/game/spawn_sound.h"
#include "src/game/object_helpers.h"
#include "src/game/memory.h"
#include "src/game/macro_special_objects.h"
#include "src/game/debug_course.h"
#include "src/game/behavior_actions.h"
#include "src/game/segment2.h"
#include "src/game/mario_actions_cutscene.h"
#include "src/game/profiler.h"
#include "src/game/envfx_snow.h"
#include "src/game/game_init.h"
#include "src/game/object_list_processor.h"
#include "src/game/mario_actions_airborne.h"
#include "src/game/spawn_object.h"
#include "src/game/obj_behaviors.h"
#include "src/game/paintings.h"
#include "src/audio/playback.h"
#include "src/audio/internal.h"
#include "src/audio/synthesis.h"
#include "src/audio/external.h"
#include "src/audio/load.h"
#include "src/audio/data.h"
#include "src/audio/mixer.h"
#include "src/audio/effects.h"
#include "src/audio/heap.h"
#include "src/audio/seqplayer.h"
#include "src/menu/intro_geo.h"
#include "src/menu/debug_level_select.h"
#include "src/menu/title_screen.h"
#include "src/menu/star_select.h"
#include "src/menu/file_select.h"
#include "src/port/ui/SaveEditor.h"
#include "src/port/ui/Notification.h"
#include "src/port/ui/GhostshipMenu.h"
#include "src/port/ui/GhostshipInputEditorWindow.h"
#include "src/port/ui/MenuTypes.h"
#include "src/port/ui/enhancementTypes.h"
#include "src/port/ui/Menu.h"
#include "src/port/ui/cvar_prefixes.h"
#include "src/port/ui/GhostshipModals.h"
#include "src/port/ui/ResolutionEditor.h"
#include "src/port/ui/InputViewer.h"
#include "src/port/Engine.h"
#include "src/port/scripting/scripting.h"
#include "src/port/GameExtractor.h"
#include "src/port/build.h"
#include "src/port/ShipUtils.h"
#include "src/port/Matrix.h"
#include "src/port/CoreMath.h"
#include "src/port/game/GeoLayoutParser.h"
#include "src/port/audio/GameAudio.h"
#include "src/port/hooks/impl/EventSystem.h"
#include "src/port/hooks/Events.h"
#include "src/port/mods/utils/GfxPrint.h"
#include "src/port/mods/BetterLevelSelect.h"
#include "src/port/mods/PortEnhancements.h"
#include "src/port/data/SaveConversion.h"
#include "src/port/data/Saves.h"
#include "src/port/interpolation/matrix.h"
#include "src/port/interpolation/FrameInterpolation.h"
#include "src/port/console/DevConsole.h"
#include "src/goddard/gd_math.h"
#include "src/goddard/skin_movement.h"
#include "src/goddard/old_menu.h"
#include "src/goddard/gd_types.h"
#include "src/goddard/gd_macros.h"
#include "src/goddard/bad_declarations.h"
#include "src/goddard/renderer.h"
#include "src/goddard/gd_memory.h"
#include "src/goddard/objects.h"
#include "src/goddard/sfx.h"
#include "src/goddard/dynlist_proc.h"
#include "src/goddard/skin.h"
#include "src/goddard/draw_objects.h"
#include "src/goddard/debug_utils.h"
#include "src/goddard/gd_main.h"
#include "src/goddard/shape_helper.h"
#include "src/goddard/particles.h"
#include "src/goddard/dynlists/dynlist_macros.h"
#include "src/goddard/dynlists/dynlists.h"
#include "src/goddard/dynlists/animdata.h"
#include "src/goddard/joints.h"
#include "src/engine/level_script.h"
#include "src/engine/geo_layout.h"
#include "src/engine/behavior_script.h"
#include "src/engine/graph_node.h"
#include "src/engine/surface_collision.h"
#include "src/engine/math_util.h"
#include "src/engine/surface_load.h"
#include "command_macros_base.h"
#include "surface_terrains.h"
#include "special_presets.h"
#include "sm64.h"
#include "macro_presets.h"
#include "skybox_table.h"
#include "config.h"
#include "gfx_dimensions.h"
#include "types.h"
#include "align_asset_macro.h"
#include "mario_animation_ids.h"
#include "mario_geo_switch_case_ids.h"
#include "sounds.h"
#include "level_misc_macros.h"
#include "model_ids.h"
#include "platform_info.h"
#include "level_headers.h"
#include "level_table.h"
#include "dr_mp3.h"
#include "make_const_nonconst.h"
#include "PR/libaudio.h"
#include "PR/ucode.h"
#include "PR/gu.h"
#include "dialog_ids.h"
#include "portable-file-dialogs.h"
#include "segments.h"
#include "special_preset_names.h"
#include "macros.h"
#include "eu_translation.h"
#include "helper_macros.h"
#include "object_constants.h"
#include "object_fields.h"
#include "segment_symbols.h"
#include "macro_preset_names.h"
#include "prevent_bss_reordering.h"
#include "demo_table.h"
#include "variables.h"
#include "level_commands.h"
#include "course_table.h"
#include "moving_texture_macros.h"
#include "assets/textures/generic.h"
#include "assets/textures/intro_raw.h"
#include "assets/textures/machine.h"
#include "assets/textures/outside.h"
#include "assets/textures/mountain.h"
#include "assets/textures/inside.h"
#include "assets/textures/ipl3_raw.h"
#include "assets/textures/fire.h"
#include "assets/textures/water.h"
#include "assets/textures/snow.h"
#include "assets/textures/effect.h"
#include "assets/textures/segment2.h"
#include "assets/textures/skyboxes/bbh.h"
#include "assets/textures/skyboxes/cloud_floor.h"
#include "assets/textures/skyboxes/bbh.table.h"
#include "assets/textures/skyboxes/bidw.h"
#include "assets/textures/skyboxes/clouds.h"
#include "assets/textures/skyboxes/ssl.h"
#include "assets/textures/skyboxes/water.h"
#include "assets/textures/skyboxes/wdw.h"
#include "assets/textures/skyboxes/bitfs.h"
#include "assets/textures/skyboxes/bits.h"
#include "assets/textures/skyboxes/ccm.h"
#include "assets/textures/cave.h"
#include "assets/textures/title_screen_bg.h"
#include "assets/textures/spooky.h"
#include "assets/textures/grass.h"
#include "assets/textures/sky.h"
#include "assets/bin/debug_level_select.h"
#include "assets/bin/effect.h"
#include "assets/bin/segment2.h"
#include "assets/bin/title_screen_bg.h"
#include "assets/actors/poundable_pole.h"
#include "assets/actors/exclamation_box_outline/geo.h"
#include "assets/actors/small_key/geo.h"
#include "assets/actors/water_mine.h"
#include "assets/actors/monty_mole_hole.h"
#include "assets/actors/springboard.h"
#include "assets/actors/mips.h"
#include "assets/actors/tree/geo.h"
#include "assets/actors/koopa_shell/geo.h"
#include "assets/actors/bully/anims.h"
#include "assets/actors/bully/geo.h"
#include "assets/actors/butterfly/anims.h"
#include "assets/actors/butterfly/geo.h"
#include "assets/actors/capswitch.h"
#include "assets/actors/pokey.h"
#include "assets/actors/cyan_fish.h"
#include "assets/actors/mushroom_1up/geo.h"
#include "assets/actors/ukiki.h"
#include "assets/actors/mad_piano.h"
#include "assets/actors/cyan_fish/anims.h"
#include "assets/actors/cyan_fish/geo.h"
#include "assets/actors/book.h"
#include "assets/actors/spiny_egg.h"
#include "assets/actors/haunted_cage/geo.h"
#include "assets/actors/scuttlebug/anims.h"
#include "assets/actors/scuttlebug/geo.h"
#include "assets/actors/butterfly.h"
#include "assets/actors/bullet_bill.h"
#include "assets/actors/lakitu_cameraman/anims.h"
#include "assets/actors/lakitu_cameraman/geo.h"
#include "assets/actors/koopa_flag.h"
#include "assets/actors/bobomb/anims.h"
#include "assets/actors/bobomb/geo.h"
#include "assets/actors/flame.h"
#include "assets/actors/yoshi_egg/geo.h"
#include "assets/actors/exclamation_box/geo.h"
#include "assets/actors/exclamation_box_outline.h"
#include "assets/actors/snowman/anims.h"
#include "assets/actors/snowman/geo.h"
#include "assets/actors/water_wave.h"
#include "assets/actors/mr_i_eyeball.h"
#include "assets/actors/impact_smoke.h"
#include "assets/actors/mario_cap.h"
#include "assets/actors/sparkle_animation.h"
#include "assets/actors/hoot.h"
#include "assets/actors/water_mine/geo.h"
#include "assets/actors/chair.h"
#include "assets/actors/coin.h"
#include "assets/actors/spiny/anims.h"
#include "assets/actors/spiny/geo.h"
#include "assets/actors/dirt.h"
#include "assets/actors/seaweed.h"
#include "assets/actors/chuckya/anims.h"
#include "assets/actors/chuckya/geo.h"
#include "assets/actors/piranha_plant/anims.h"
#include "assets/actors/piranha_plant/geo.h"
#include "assets/actors/king_bobomb.h"
#include "assets/actors/yoshi_egg.h"
#include "assets/actors/snufit.h"
#include "assets/actors/bowser_key.h"
#include "assets/actors/bowling_ball/geo.h"
#include "assets/actors/peach.h"
#include "assets/actors/heart/geo.h"
#include "assets/actors/mr_i_iris.h"
#include "assets/actors/water_bubble.h"
#include "assets/actors/amp.h"
#include "assets/actors/manta.h"
#include "assets/actors/blargg.h"
#include "assets/actors/chain_chomp.h"
#include "assets/actors/klepto/anims.h"
#include "assets/actors/klepto/geo.h"
#include "assets/actors/thwomp/geo.h"
#include "assets/actors/boo_castle.h"
#include "assets/actors/penguin/anims.h"
#include "assets/actors/penguin/geo.h"
#include "assets/actors/cannon_lid.h"
#include "assets/actors/breakable_box.h"
#include "assets/actors/bomb/geo.h"
#include "assets/actors/cannon_barrel/geo.h"
#include "assets/actors/skeeter.h"
#include "assets/actors/leaves/geo.h"
#include "assets/actors/swoop.h"
#include "assets/actors/piranha_plant.h"
#include "assets/actors/water_wave/geo.h"
#include "assets/actors/water_ring/anims.h"
#include "assets/actors/water_ring/geo.h"
#include "assets/actors/bowser.h"
#include "assets/actors/monty_mole.h"
#include "assets/actors/haunted_cage.h"
#include "assets/actors/sushi.h"
#include "assets/actors/warp_pipe/geo.h"
#include "assets/actors/treasure_chest.h"
#include "assets/actors/manta/anims.h"
#include "assets/actors/manta/geo.h"
#include "assets/actors/goomba.h"
#include "assets/actors/bookend/anims.h"
#include "assets/actors/bookend/geo.h"
#include "assets/actors/toad.h"
#include "assets/actors/scuttlebug.h"
#include "assets/actors/transparent_star/geo.h"
#include "assets/actors/mad_piano/anims.h"
#include "assets/actors/mad_piano/geo.h"
#include "assets/actors/white_particle/geo.h"
#include "assets/actors/spiny.h"
#include "assets/actors/cannon_base/geo.h"
#include "assets/actors/chair/anims.h"
#include "assets/actors/chair/geo.h"
#include "assets/actors/tree.h"
#include "assets/actors/heave_ho.h"
#include "assets/actors/snufit/geo.h"
#include "assets/actors/chillychief/anims.h"
#include "assets/actors/chillychief/geo.h"
#include "assets/actors/dorrie/anims.h"
#include "assets/actors/dorrie/geo.h"
#include "assets/actors/hoot/anims.h"
#include "assets/actors/hoot/geo.h"
#include "assets/actors/yellow_sphere/geo.h"
#include "assets/actors/yoshi.h"
#include "assets/actors/bubba.h"
#include "assets/actors/bub.h"
#include "assets/actors/fwoosh.h"
#include "assets/actors/bubble.h"
#include "assets/actors/blue_fish/anims.h"
#include "assets/actors/blue_fish/geo.h"
#include "assets/actors/bubble/geo.h"
#include "assets/actors/wiggler.h"
#include "assets/actors/spiny_egg/anims.h"
#include "assets/actors/spiny_egg/geo.h"
#include "assets/actors/burn_smoke/geo.h"
#include "assets/actors/number/geo.h"
#include "assets/actors/sand.h"
#include "assets/actors/skeeter/anims.h"
#include "assets/actors/skeeter/geo.h"
#include "assets/actors/whomp/anims.h"
#include "assets/actors/whomp/geo.h"
#include "assets/actors/metal_box/geo.h"
#include "assets/actors/water_bubble/geo.h"
#include "assets/actors/wooden_signpost/geo.h"
#include "assets/actors/mips/anims.h"
#include "assets/actors/mips/geo.h"
#include "assets/actors/bookend.h"
#include "assets/actors/bomb.h"
#include "assets/actors/blue_fish.h"
#include "assets/actors/sparkle_animation/geo.h"
#include "assets/actors/moneybag.h"
#include "assets/actors/tornado/geo.h"
#include "assets/actors/sparkle.h"
#include "assets/actors/star/geo.h"
#include "assets/actors/wooden_signpost.h"
#include "assets/actors/king_bobomb/anims.h"
#include "assets/actors/king_bobomb/geo.h"
#include "assets/actors/mr_i_eyeball/geo.h"
#include "assets/actors/power_meter.h"
#include "assets/actors/heave_ho/anims.h"
#include "assets/actors/heave_ho/geo.h"
#include "assets/actors/coin/geo.h"
#include "assets/actors/koopa_flag/anims.h"
#include "assets/actors/koopa_flag/geo.h"
#include "assets/actors/lakitu_enemy/anims.h"
#include "assets/actors/lakitu_enemy/geo.h"
#include "assets/actors/mist/geo.h"
#include "assets/actors/purple_switch.h"
#include "assets/actors/burn_smoke.h"
#include "assets/actors/breakable_box/geo.h"
#include "assets/actors/bird/anims.h"
#include "assets/actors/bird/geo.h"
#include "assets/actors/capswitch/geo.h"
#include "assets/actors/koopa/anims.h"
#include "assets/actors/koopa/geo.h"
#include "assets/actors/springboard/geo.h"
#include "assets/actors/impact_ring/geo.h"
#include "assets/actors/toad/anims.h"
#include "assets/actors/toad/geo.h"
#include "assets/actors/bowser_flame.h"
#include "assets/actors/amp/anims.h"
#include "assets/actors/amp/geo.h"
#include "assets/actors/stomp_smoke/geo.h"
#include "assets/actors/mr_i_iris/geo.h"
#include "assets/actors/walk_smoke/geo.h"
#include "assets/actors/lakitu_cameraman.h"
#include "assets/actors/flame/geo.h"
#include "assets/actors/eyerok/anims.h"
#include "assets/actors/eyerok/geo.h"
#include "assets/actors/metal_box.h"
#include "assets/actors/blue_coin_switch/geo.h"
#include "assets/actors/swoop/anims.h"
#include "assets/actors/swoop/geo.h"
#include "assets/actors/chillychief.h"
#include "assets/actors/pebble.h"
#include "assets/actors/koopa.h"
#include "assets/actors/mario.h"
#include "assets/actors/spindrift/anims.h"
#include "assets/actors/spindrift/geo.h"
#include "assets/actors/boo_castle/geo.h"
#include "assets/actors/explosion/geo.h"
#include "assets/actors/pokey/geo.h"
#include "assets/actors/clam_shell/anims.h"
#include "assets/actors/clam_shell/geo.h"
#include "assets/actors/door.h"
#include "assets/actors/book/geo.h"
#include "assets/actors/impact_smoke/geo.h"
#include "assets/actors/treasure_chest/geo.h"
#include "assets/actors/mist.h"
#include "assets/actors/boo/geo.h"
#include "assets/actors/clam_shell.h"
#include "assets/actors/bobomb.h"
#include "assets/actors/sushi/anims.h"
#include "assets/actors/sushi/geo.h"
#include "assets/actors/blargg/anims.h"
#include "assets/actors/blargg/geo.h"
#include "assets/actors/leaves.h"
#include "assets/actors/warp_pipe.h"
#include "assets/actors/mushroom_1up.h"
#include "assets/actors/bullet_bill/geo.h"
#include "assets/actors/yoshi/anims.h"
#include "assets/actors/yoshi/geo.h"
#include "assets/actors/white_particle_small.h"
#include "assets/actors/yellow_sphere.h"
#include "assets/actors/goomba/anims.h"
#include "assets/actors/goomba/geo.h"
#include "assets/actors/stomp_smoke.h"
#include "assets/actors/monty_mole/anims.h"
#include "assets/actors/monty_mole/geo.h"
#include "assets/actors/impact_ring.h"
#include "assets/actors/chain_ball.h"
#include "assets/actors/klepto.h"
#include "assets/actors/bird.h"
#include "assets/actors/fwoosh/geo.h"
#include "assets/actors/water_ring.h"
#include "assets/actors/seaweed/anims.h"
#include "assets/actors/seaweed/geo.h"
#include "assets/actors/white_particle.h"
#include "assets/actors/star.h"
#include "assets/actors/cannon_barrel.h"
#include "assets/actors/moneybag/anims.h"
#include "assets/actors/moneybag/geo.h"
#include "assets/actors/snowman.h"
#include "assets/actors/dirt/geo.h"
#include "assets/actors/test_platform.h"
#include "assets/actors/exclamation_box.h"
#include "assets/actors/water_splash.h"
#include "assets/actors/penguin.h"
#include "assets/actors/smoke.h"
#include "assets/actors/explosion.h"
#include "assets/actors/yellow_sphere_small.h"
#include "assets/actors/bowser_flame/geo.h"
#include "assets/actors/chain_ball/geo.h"
#include "assets/actors/yellow_sphere_small/geo.h"
#include "assets/actors/dorrie.h"
#include "assets/actors/walk_smoke.h"
#include "assets/actors/bowser/anims.h"
#include "assets/actors/bowser/geo.h"
#include "assets/actors/poundable_pole/geo.h"
#include "assets/actors/spindrift.h"
#include "assets/actors/unagi/anims.h"
#include "assets/actors/unagi/geo.h"
#include "assets/actors/thwomp.h"
#include "assets/actors/test_platform/geo.h"
#include "assets/actors/mario/geo.h"
#include "assets/actors/blue_coin_switch.h"
#include "assets/actors/whirlpool.h"
#include "assets/actors/checkerboard_platform.h"
#include "assets/actors/chain_chomp/anims.h"
#include "assets/actors/chain_chomp/geo.h"
#include "assets/actors/water_splash/geo.h"
#include "assets/actors/flyguy.h"
#include "assets/actors/tornado.h"
#include "assets/actors/ukiki/anims.h"
#include "assets/actors/ukiki/geo.h"
#include "assets/actors/bubba/geo.h"
#include "assets/actors/heart.h"
#include "assets/actors/bub/anims.h"
#include "assets/actors/bub/geo.h"
#include "assets/actors/purple_switch/geo.h"
#include "assets/actors/small_key.h"
#include "assets/actors/transparent_star.h"
#include "assets/actors/bowser_key/anims.h"
#include "assets/actors/bowser_key/geo.h"
#include "assets/actors/lakitu_enemy.h"
#include "assets/actors/koopa_shell.h"
#include "assets/actors/sparkle/geo.h"
#include "assets/actors/checkerboard_platform/geo.h"
#include "assets/actors/flyguy/anims.h"
#include "assets/actors/flyguy/geo.h"
#include "assets/actors/whomp.h"
#include "assets/actors/door/anims.h"
#include "assets/actors/door/geo.h"
#include "assets/actors/boo.h"
#include "assets/actors/unagi.h"
#include "assets/actors/bully.h"
#include "assets/actors/peach/anims.h"
#include "assets/actors/peach/geo.h"
#include "assets/actors/cannon_base.h"
#include "assets/actors/mario_cap/geo.h"
#include "assets/actors/eyerok.h"
#include "assets/actors/warp_collision.h"
#include "assets/actors/chuckya.h"
#include "assets/levels/sl/geo.h"
#include "assets/levels/jrb/geo.h"
#include "assets/levels/totwc.h"
#include "assets/levels/cotmc/geo.h"
#include "assets/levels/bbh.h"
#include "assets/levels/ssl/geo.h"
#include "assets/levels/wf.h"
#include "assets/levels/bitdw.h"
#include "assets/levels/intro/geo.h"
#include "assets/levels/cotmc.h"
#include "assets/levels/lll/geo.h"
#include "assets/levels/sl.h"
#include "assets/levels/ttm/geo.h"
#include "assets/levels/bowser_3.h"
#include "assets/levels/bob/geo.h"
#include "assets/levels/ttc/geo.h"
#include "assets/levels/ddd/geo.h"
#include "assets/levels/jrb.h"
#include "assets/levels/rr/geo.h"
#include "assets/levels/sa.h"
#include "assets/levels/ending.h"
#include "assets/levels/ddd.h"
#include "assets/levels/hmc.h"
#include "assets/levels/castle_grounds/geo.h"
#include "assets/levels/bowser_1/geo.h"
#include "assets/levels/ccm/geo.h"
#include "assets/levels/castle_inside.h"
#include "assets/levels/vcutm/geo.h"
#include "assets/levels/ttc.h"
#include "assets/levels/castle_grounds.h"
#include "assets/levels/sa/geo.h"
#include "assets/levels/ssl.h"
#include "assets/levels/ttm.h"
#include "assets/levels/thi.h"
#include "assets/levels/bits/geo.h"
#include "assets/levels/ending/geo.h"
#include "assets/levels/castle_courtyard/geo.h"
#include "assets/levels/bob.h"
#include "assets/levels/menu.h"
#include "assets/levels/menu/geo.h"
#include "assets/levels/wdw/geo.h"
#include "assets/levels/bowser_1.h"
#include "assets/levels/rr.h"
#include "assets/levels/bitfs/geo.h"
#include "assets/levels/lll.h"
#include "assets/levels/intro.h"
#include "assets/levels/wdw.h"
#include "assets/levels/hmc/geo.h"
#include "assets/levels/totwc/geo.h"
#include "assets/levels/bitdw/geo.h"
#include "assets/levels/pss/geo.h"
#include "assets/levels/bbh/geo.h"
#include "assets/levels/castle_courtyard.h"
#include "assets/levels/wmotr.h"
#include "assets/levels/pss.h"
#include "assets/levels/bitfs.h"
#include "assets/levels/vcutm.h"
#include "assets/levels/bowser_3/geo.h"
#include "assets/levels/thi/geo.h"
#include "assets/levels/castle_inside/geo.h"
#include "assets/levels/bits.h"
#include "assets/levels/bowser_2/geo.h"
#include "assets/levels/wf/geo.h"
#include "assets/levels/wmotr/geo.h"
#include "assets/levels/bowser_2.h"
#include "assets/levels/ccm.h"
#include "textures.h"
#include "seq_ids.h"
#include "behavior_data.h"
#include "geo_commands.h"
#include "texts_table.h"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <utility>

using namespace UIWidgets;

struct Asset {
    std::string path;

    explicit Asset(const char* path) : path(path) {}
    explicit Asset(std::string path) : path(std::move(path)) {}

    template<typename T>
    T* Get() {
        if(this == nullptr){
            return nullptr;
        }
        return (T*) path.data();
    }

    std::string tostring() const {
        return path;
    }

    static Asset Register(const std::string path) {
        std::string full = "__OTR__" + path;
        return Asset{ full };
    }
};

namespace fs = std::filesystem;

ScriptingLayer* ScriptingLayer::Instance = new ScriptingLayer();
sol::state lua;

std::vector<std::pair<EventID, ListenerID>> RegisteredListeners;
std::vector<std::pair<std::string, EventID>> RegisteredEvents;

std::optional<std::string> LoadFromO2R(const std::string& path, const std::shared_ptr<Ship::Archive>& archive = nullptr) {
    auto loader = Ship::Context::GetInstance()->GetResourceManager();
    auto init = std::make_shared<Ship::ResourceInitData>();
    init->Type = static_cast<uint32_t>(SM64::ResourceType::Text);
    init->ByteOrder = Ship::Endianness::Native;
    init->Format = RESOURCE_FORMAT_BINARY;
    std::shared_ptr<SM64::Text> res;
    
    if (archive == nullptr) {
        res = std::static_pointer_cast<SM64::Text>(Ship::Context::GetInstance()->GetResourceManager()->LoadResource(path, true, init));
    } else {
        auto file = archive->LoadFile(path);
        res = std::static_pointer_cast<SM64::Text>(loader->GetResourceLoader()->LoadResource(path, file, init));
    }

    if (res == nullptr) {
        return std::nullopt;
    }

    return *static_cast<std::string*>(res->GetRawPointer());
}

int ScriptingLayer::Require(lua_State* L) {
    std::string path = sol::stack::get<std::string>(L, 1);

    std::optional<std::string> result = LoadFromO2R(path);

    if(!result.has_value()){
        const auto error = "Failed to include " + path;
        SPDLOG_ERROR(error);
        sol::stack::push(L, error);
        return 0;
    }

    const auto& script = result.value();
    luaL_loadbuffer(
        L, script.data(), script.size(), path.c_str());

    return 1;
}

void ScriptingLayer::Load(const std::string& path, uint32_t bindings, const std::shared_ptr<Ship::Archive>& archive) {
    auto result = LoadFromO2R(path, archive);

    if(!result.has_value()){
        return;
    }

    try {
        sol::environment env(lua, sol::create, lua.globals());
        lua.safe_script(result.value(), env);
    } catch (const sol::error& e) {
        SPDLOG_ERROR(std::string(e.what()));
        return;
    }
}

void BindStructs() {
    #include "bindings/v1/structs.gen"
}

void BindEnums() {
    #include "bindings/v1/enums.gen"
}

void BindExterns() {
    #include "bindings/v1/externs.gen"
}

void BindEvents() {
    #include "bindings/v1/events.gen"
}

void ScriptingLayer::Init() {
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::table, sol::lib::string);

    lua["os"] = sol::lua_nil;
    lua["io"] = sol::lua_nil;
    lua["debug"] = sol::lua_nil;

    lua.clear_package_loaders();
    lua.add_package_loader(ScriptingLayer::Require);

    lua["RegisterListener"] = [](EventID eventId, const sol::function& callback, uint32_t priority) {
        auto lid = EventSystem::Instance->RegisterListener(eventId, callback, (EventPriority) priority);
        RegisteredListeners.emplace_back(eventId, lid);
        return lid;
    };

    lua.new_usertype<Asset>("Asset",
        "Register", &Asset::Register,
        "Load8",  &Asset::Get<u8>,
        "Load16", &Asset::Get<u16>,
        "Load32", &Asset::Get<u32>,
        "LoadVtx", &Asset::Get<Vtx>,
        "LoadGfx", &Asset::Get<Gfx>,
        sol::meta_function::to_string, &Asset::tostring
    );

    lua["gNextMasterDisp"] = []() -> Gfx* {
        return gDisplayListHead++;
    };

    lua["gRefMasterDisp"] = []() -> Gfx* {
        return (Gfx*) &gDisplayListHead;
    };
    //
    // lua["gRefGfxMatrix"] = []() -> Matrix* {
    //     return (Matrix*) &gGfxMatrix;
    // };

    lua["gDPSetPrimColor"] = [](uint8_t m, uint8_t l, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        gDPSetPrimColor(gDisplayListHead++, m, l, r, g, b, a);
    };

    lua["Game"] = lua.create_table();
    lua["Assets"] = lua.create_table();
    lua["Events"] = lua.create_table();
    lua["UIWidgets"] = lua.create_table();

    for (const auto& [name, id] : RegisteredEvents) {
        lua["Events"][name] = id;
    }

    BindStructs();
    BindEnums();
    BindExterns();
    BindEvents();
}

void ScriptingLayer::Clean() {
    for (const auto& [eventId, listenerId] : RegisteredListeners) {
        EventSystem::Instance->UnregisterListener(eventId, listenerId);
    }
    RegisteredListeners.clear();
}

void ScriptingLayer::Reload() {
    this->Clean();
    lua.collect_garbage();
    lua = sol::state();
    this->Init();
}

extern "C" void BindEvent(const char* name, EventID id) {
    RegisteredEvents.emplace_back(name, id);
}