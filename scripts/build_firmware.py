#!/usr/bin/env python3
"""Build an experimental AI shell candidate; physical voice validation is pending.
This deliberately writes candidate.bin outside release packages, not an install recommendation.
"""
import argparse,hashlib,json,shutil,struct,subprocess,tempfile
from pathlib import Path
from firmware_format import ROOT,elf_sections,encode_bl,encode_bw,OSA,VERSION_OFFSET,COMBINED_VERSION_OFFSET,verify_resc,jshash
from toolchain import tool
BASE=ROOT/'_local/inputs/v0121-base/SOGOUOTA.BIN'
RECORD_PAGES=ROOT/'_local/inputs/record-pages.bin'
BASE_SHA='c484e9468bae8de8ef379965290749da7cffe2eebc5a5881accffb67a7da7741'
VERSION=141
RAM=0x2002f99c
OUT=ROOT/'build/V0141'
EVIDENCE=ROOT/'_local/reports'
PACKAGE_DIR=ROOT/'_local/firmware/V0141'
ROOTS=['music_play_mode','music_play_mode_name','music_cycle_play_mode','music_keys_event','music_keys_poll','music_keys_reset','volume_sheet_bind','volume_sheet_request','volume_sheet_open','volume_sheet_gesture','volume_sheet_message','volume_sheet_destroy','record_feature_request','record_navigate','music_pause_to','music_navigation_pending','music_request_ai','music_page_name','music_page_init','music_page_deinit','music_page_message','music_page_gesture','music_close','music_poll','music_action','music_select','music_text','music_count','music_revision','music_visible','music_loaded','music_event','music_working_bytes','music_progress','music_delete_prepare','music_delete_confirm','music_delete_cancel','music_delete_state','record_mode_poll','record_mode_request','record_mode_state','record_mode_pending','record_mode_error','record_mode_active','record_page_init','record_home_page','record_ai_allowed','xs_quiescent','xa_quiescent','ai_boot_brand','ai_shutdown_brand','ai_boot_deinit','ai_shutdown_deinit','ai_power_animation_tick','ai_power_connected','ai_power_disconnected','ai_battery_draw','ai_home_name','ai_menu_name','ai_home_init','ai_menu_init','ai_page_deinit','ai_page_message','ai_page_gesture',
       'ai_boot_hook','ai_initial_page','ai_key_hook','ai_app_message','ai_state_address','ai_state_size','ai_page_replace','ai_page_push',
       'xo_parse_bootstrap','xo_bootstrap_body','xo_headers','xo_hello','xo_listen','xo_abort','xo_audio_pack','xo_audio_unpack','xo_http_init','xo_http_feed','xo_http_eof','xs_start','xs_worker','xs_status','xs_state_value','xs_error_value','xs_activation_code','ai_animation_tick','xs_caption_page','xo_parse_event','xw_mask','xw_feed','xw_next','xw_url_parse','xw_upgrade','xw_key','ai_standby_hook','ai_screen_wake','xo_session_working_bytes','xo_session_allocation_sizes','xo_session_layout','xp_identity','xp_time','xp_mac','xp_set_time','xp_ntp_open','xt_sync','xt_poll','xt_wait','xt_state_value','xt_request','xt_response','xa_capture_open','xa_capture_read','xa_capture_close','xa_capture_report','xa_playback_open','xa_playback_write','xa_playback_finish','xa_playback_keepalive','xa_playback_parked','xa_playback_close','xc_open','xc_encode','xc_decode','xc_close','xv_open','xv_close','xv_action','xv_tick','xv_receive','xv_tts','xv_working_bytes','xs_voice_status','xs_voice_request']


def build():
    icons=json.loads((ROOT/'assets/music-play-modes/manifest.json').read_text());assert hashlib.sha256((ROOT/'patches/xiaozhi-main/play_mode_icons.h').read_bytes()).hexdigest()==icons['header_sha256']
    animation=json.loads((ROOT/'assets/qingfeng-boot/manifest.json').read_text())
    assert hashlib.sha256((ROOT/'patches/xiaozhi-main/boot_frames.h').read_bytes()).hexdigest()==animation['header_sha256']
    for source in animation['sources']:assert hashlib.sha256((ROOT/'assets/qingfeng-boot'/source['filename']).read_bytes()).hexdigest()==source['sha256']
    base=BASE.read_bytes();assert hashlib.sha256(base).hexdigest()==BASE_SHA
    host=bytearray(base[8:-8]);old=json.loads((ROOT/'build-support/v0096-layout.json').read_text())
    baseline=json.loads((ROOT/'build-support/v0121-manifest.json').read_text())
    baseline_hooks={int(h['offset'],16):h for h in baseline['hooks']}
    original_end=verify_resc(host,OSA,0x700000)['digest_offset']
    start=baseline['append_file_offset'];va=0x18000000+start
    linker=tool('ld.lld')
    with tempfile.TemporaryDirectory(prefix='c208-ai-main-') as tmp:
        td=Path(tmp);(td/'addresses.h').write_text('#define C208_WIFI_BOOT '+old['symbols']['wifi_boot_hook']+'u\n#define C208_AI_VERSION "V'+f'{VERSION:04d}'+'"\n')
        root=ROOT/'build-support/tls/official-verified-root.der';dn=ROOT/'build-support/tls/official-verified-root.dn'
        provenance=json.loads((root.parent/'verified-root-provenance.json').read_text());assert hashlib.sha256(root.read_bytes()).hexdigest()==provenance['root_der_sha256']
        (td/'official_root.h').write_text('static const unsigned char C208_ROOT_DER[]={'+','.join(str(c) for c in root.read_bytes())+'};\nstatic const unsigned char C208_ROOT_DN[]={'+','.join(str(c) for c in dn.read_bytes())+'};\n')
        objects=[]
        sources=[ROOT/'patches/xiaozhi-main'/name for name in ('input.c','native.c','record_mode.c','music.c','music_page.c','music_keys.c','battery.c','power_ui.c','volume_sheet.c','bridge.S')]+[ROOT/'patches/xiaozhi-official'/name for name in ('official.c','http.c','arm_runtime.c','arm_runtime.S','identity.c','platform.c','session.c','audio.c','clock.c','websocket.c','codec.c','voice.c','level.c')]
        for source in sources:
            obj=td/(source.name+'.o');objects.append(obj)
            subprocess.run([tool('clang'),'--target=armv7m-none-eabi','-mcpu=cortex-m4','-mthumb','-mfloat-abi=soft',
                '-Os','-ffreestanding','-fno-builtin','-fstack-usage','-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables',
                '-ffunction-sections','-fdata-sections','-Wall','-Wextra','-Werror','-I'+str(td),'-I'+str(ROOT/'_local/inputs'),'-I'+str(ROOT/'third_party/opus-1.5.2/include'),'-I'+str(ROOT/'third_party/bearssl-0.6/inc'),'-I'+str(ROOT/'patches/xiaozhi-official/compat'),'-c',str(source),'-o',str(obj)],check=True,capture_output=True)
        (td/'link.ld').write_text(f'''SECTIONS {{
 . = 0x{va:x}; .text : {{ *(.text*) *(.rodata*) . = ALIGN(4); }}
 . = 0x{RAM:x}; .bss : {{ *(.bss*) *(COMMON) . = ALIGN(4); }}
 .data : {{ *(.data*) }}
 /DISCARD/ : {{ *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) }}
}}''')
        tlslib=ROOT/'_local/build/tls/libxiaozhi-official.a'
        if not tlslib.is_file():raise ValueError('Build the ARM TLS archive first: scripts/build_xiaozhi_tls.py')
        tls_report=json.loads((ROOT/'_local/build/tls/arm-tls-size.json').read_text());assert hashlib.sha256(tlslib.read_bytes()).hexdigest()==tls_report['library_sha256']
        opuslib=ROOT/'_local/build/opus/libopus-fixed.a';opus_report=json.loads(opuslib.with_name('build.json').read_text());assert hashlib.sha256(opuslib.read_bytes()).hexdigest()==opus_report['library_sha256']
        elf=td/'main.elf';subprocess.run([linker,'--gc-sections','--entry=ai_boot_hook',*['--undefined='+name for name in ROOTS],'-T',str(td/'link.ld'),*map(str,objects),str(tlslib),str(opuslib),'-o',str(elf)],check=True,capture_output=True)
        image=elf.read_bytes();stack_usage=''.join(p.read_text() for p in sorted(td.glob('*.su')))
    named,sections=elf_sections(image);symbols={};code=b'';bss=0
    for name,s in named:
        if name=='.text':code=image[s[4]:s[4]+s[5]];assert s[3]==va
        if name=='.bss':bss=s[5];assert s[3]==RAM
        if name=='.data':assert s[5]==0,'nonzero initialized RAM needs a loader'
        if s[1] in (4,9):assert not s[5]
        if s[1]==2:
            strings_section=sections[s[6]];strings=image[strings_section[4]:strings_section[4]+strings_section[5]]
            for at in range(s[4],s[4]+s[5],s[9]):
                no,value,size,info,other,owner=struct.unpack_from('<IIIBBH',image,at)
                name=strings[no:].split(b'\0')[0].decode()
                if name and owner:symbols[name]=value
    assert code and all(name in symbols for name in ROOTS)
    allocation_sizes=struct.unpack_from('<3I',code,symbols['xo_session_allocation_sizes']-va)
    assert allocation_sizes[1]==tls_report['TLS_context_bytes_including_record_buffers'],'TLS archive/header ABI mismatch; rebuild scripts/build_xiaozhi_tls.py'
    if 'adapter_source_sha256' in tls_report:
        for name,digest in tls_report['adapter_source_sha256'].items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest,'Rebuild TLS adapter after source changes' 
    ram_bytes=(bss+511)&~511;assert ram_bytes==3072,'Keep the working V0140 reset/heap layout unchanged'
    table=bytearray(host[baseline['page_table_offset']:baseline['page_table_offset']+61*20])+bytearray(20)
    stock=RECORD_PAGES.read_bytes();assert len(stock)==40
    for page in (8,9):table[page*20:(page+1)*20]=stock[(page-8)*20:(page-7)*20]
    struct.pack_into('<I',table,8*20+4,symbols['record_page_init'])
    for page,prefix in ((59,'ai_home'),(60,'ai_menu')):
        struct.pack_into('<5I',table,page*20,symbols[prefix+'_name'],symbols[prefix+'_init'],symbols['ai_page_deinit'],symbols['ai_page_message'],symbols['ai_page_gesture'])
    struct.pack_into('<5I',table,61*20,symbols['music_page_name'],symbols['music_page_init'],symbols['music_page_deinit'],symbols['music_page_message'],symbols['music_page_gesture'])
    for page,target in ((0,'ai_boot_deinit'),(1,'ai_shutdown_deinit')):
        struct.pack_into('<I',table,page*20+8,symbols[target])
    table_at=start+len(code);end=table_at+len(table);clear_end=max(end+4,original_end+4)
    assert clear_end<0x720000 and not any(host[original_end+4:clear_end])
    changes=[]
    def patch(at,wanted,data,why):
        if at in baseline_hooks:wanted=bytes.fromhex(baseline_hooks[at]['after'])
        assert len(wanted)==len(data) and host[at:at+len(wanted)]==wanted,(hex(at),why)
        host[at:at+len(wanted)]=data;changes.append({'offset':hex(at),'before':wanted.hex(),'after':data.hex(),'purpose':why})
    patch(0x5b478,bytes.fromhex('bbf13a0f'),bytes.fromhex('bbf13d0f'),'extend stock dispatcher to MP3 page 61')
    patch(0x3f4f0,encode_bl(0x1803f4f0,int(old['symbols']['wifi_boot_hook'],16)&~1),encode_bl(0x1803f4f0,symbols['ai_boot_hook']&~1),'initialize AI shell, retain original Wi-Fi worker')
    for at,target in ((0x55fe6,'ai_boot_brand'),(0x573d4,'ai_shutdown_brand')):
        patch(at,encode_bl(0x18000000+at,0x1805b8b4),encode_bl(0x18000000+at,symbols[target]&~1),'brand original two-second boot/shutdown page; preserve native timer')
    for at,target,wanted in ((0x50b70,'ai_power_disconnected','002000f0'),(0x50b78,'ai_power_connected','012000f0')):
        patch(at,bytes.fromhex(wanted),encode_bw(0x18000000+at,symbols[target]&~1),'observe PMIC power event and retain original application sleep flag')
    patch(0x50cf4,bytes.fromhex('2de9f041'),encode_bw(0x18050cf4,symbols['ai_key_hook']&~1),'record key belongs to AI; preserve all other keys')
    patch(0x519f0,bytes.fromhex('70b54378'),encode_bw(0x180519f0,symbols['ai_app_message']&~1),'application-level record/MIC selector; preserve MCU driver')
    patch(0x51454,bytes.fromhex('38b51f4c'),encode_bw(0x18051454,symbols['ai_screen_wake']&~1),'restore AI input ownership after native display wake; retain stock hardware wake')
    patch(0x51800,bytes.fromhex('70b52d4c'),encode_bw(0x18051800,symbols['ai_standby_hook']&~1),'Wi-Fi connected/connecting/AP: blank display without deep standby')
    patch(0x5b614,bytes.fromhex('10b5e6f7'),encode_bw(0x1805b614,symbols['ai_initial_page']&~1),'boot to AI home; retain original factory startup condition')
    patch(0x5b534,struct.pack('<I',0x18000000+old['page_table_offset']),struct.pack('<I',0x18000000+table_at),'AI home/menu with retained settings and system pages')
    for at,target in ((0x5b53c,'ai_page_replace'),(0x5b5b8,'ai_page_push')):
        patch(at,encode_bw(0x18000000+at,int(old['symbols']['trim96_page_replace' if at==0x5b53c else 'trim96_page_push'],16)&~1),encode_bw(0x18000000+at,symbols[target]&~1),'route retired business pages to AI home/menu, retain settings/system pages')
    patch(0x354a4,struct.pack('<I',RAM),struct.pack('<I',RAM+ram_bytes),'reserve bounded AI state before heap')
    old_bss=struct.unpack_from('<II',host,0x5887a0);patch(0x5887a0,struct.pack('<II',*old_bss),struct.pack('<II',old_bss[0],old_bss[1]-baseline['ram_bytes']+ram_bytes),'clear AI state at reset')
    host[start:clear_end]=bytes(clear_end-start)
    host[start:start+len(code)]=code;host[table_at:end]=table
    host[VERSION_OFFSET]=VERSION;host[COMBINED_VERSION_OFFSET]=VERSION
    struct.pack_into('<I',host,OSA+0x54,end-OSA-512);struct.pack_into('<I',host,end,jshash(host[OSA:end]))
    allowed=[(int(c['offset'],16),int(c['offset'],16)+len(bytes.fromhex(c['after']))) for c in changes]
    allowed+=[(VERSION_OFFSET,VERSION_OFFSET+1),(COMBINED_VERSION_OFFSET,COMBINED_VERSION_OFFSET+1),(OSA+0x54,OSA+0x58),(start,clear_end)]
    assert all(a==b or any(lo<=i<hi for lo,hi in allowed) for i,(a,b) in enumerate(zip(base[8:-8],host)))
    result=base[:8]+host+struct.pack('<II',0x56000000|VERSION,0)
    manifest={'version':VERSION,'base_version':121,'original_base_version':118,'previous_version':140,'previous_package_sha256':'84dc9fa13f091306110be54de977f6e17b362245718ccc1678f656f7f6b2a7c4','settings_reference_version':132,'build_revision':'v0141-play-modes-time-row','status':'EXPERIMENTAL: four playback modes and aligned time-row mode icon; physical validation pending',
        'release_ready':False,'voice_ready':False,'package_bytes':len(result),'package_sha256':hashlib.sha256(result).hexdigest(),
        'base_package_sha256':BASE_SHA,'append_file_offset':start,'append_bytes':end-start,'page_table_offset':table_at,
        'symbols':{name:hex(value) for name,value in symbols.items()},'hooks':changes,'ram_start':hex(RAM),'ram_bytes':ram_bytes,
        'home_layout':{'type':'smile_and_natural_height_labels','font_line_height':30,'stock_24px_title_hidden':True,'fixed_label_height':False,'emoji_frames':12,'emoji_frame_pixels':[64,64],'key_feedback_is_not_voice_capture':True,'recording_wave_is_state_animation':True,'reconnect_footer_only_on_failure_or_disconnect':True,'activation_code_separate_and_persistent':True,'home_back_icon_hidden':True,'home_back_callback':False,'home_list_rows':0,'menu_gesture':'left','screen_pixels':[240,240],'horizontal_margin':8},
        'settings_only':{'base_version':130,'icons':['gear','microphone'],'font':'0x2000d2c4','style_lifetime':'native_page_style_pool','volume_sheet_removed':False,'V0131_list_changes_removed':True,'extra_UI_threads':False},
        'volume_sheet':{'pages':[59,61],'rectangle':[0,120,240,120],'upper_half_unchanged':True,'outside_background_alpha':0,'native_body_opacity_offset':8,'native_border_opacity_offset':15,'native_draw_replay_required':True,'drag_handle_rectangle':[98,125,44,3],'drag_handle_optional':True,'transition_animation':False,'native_style_pool':True,'additional_style_slots':3,'created_on_swipe_only':True,'static_pixel_buffers':False,'system_RAM_reservation_unchanged':3072,'control_state_bytes':76,'coordinate_swipe_fallback':True,'touch_callback_chains_and_restores':True,'widgets_created_in_display_task':True,'UI_request_message':250,'style_bytes_cleared_before_destroy':False,'native_getter':'0x18041f44','native_setter':'0x18041f50','native_range':[0,100],'button_and_drag_step':10,'opening_release_guard_ms':150,'new_threads':False},
        'boot_animation':{**animation,'shutdown_changed':False,'fallback_original_brand_on_image_failure':True},
        'power_ui':{'name':'清风惹尘丶','boot_and_shutdown':True,'original_duration_ms':2000,'original_timers_and_power_events_preserved':True,'charging_source':'original PMIC application callbacks 0x18050b70/0x18050b78; independent of Wi-Fi sleep flag','battery_percent_source':'native 0x18051320','battery_rectangle':[156,8,80,30],'header_rectangle':[8,8,144,30],'battery_heap_bytes':4800},
        'local_host_version_gate':{'address':'0x1803fc52','bytes':host[0x3fc52:0x3fc54].hex(),'same_version':True,'downgrade':True},
        'home_page':59,'menu_page':60,'record_page':8,'record_menu_page':9,'conversation_mode_picker':{'modes':['按住说话','单击开停','连续对话'],'default':'按住说话','selection_opens_page':True,'selected_subtitle_font_pixels':18,'selected_font_source':'shipped 30px C208 font subset scaled to 18px','chosen_marker':True,'mode_saved_for_current_boot':True,'continuous_single_press_start_stop':True,'toggle_double_press_continuous_retained':True},'menu_entries':['本机设置','对话模式'],'recording_restored':True,'added_MP3':True,'music_page':61,'music_directory':'/Music','music_file_filter':{'ignore_leading_dot':True,'includes_macOS_AppleDouble':True,'case_insensitive_MP3_extension':True,'deletes_hidden_files':False},'music_layout':{'type':'fullscreen_recording_player','screen_pixels':[240,240],'menu_rows':0,'play_modes':['单曲循环','全部循环','顺序播放','随机播放'],'default_play_mode':2,'mode_icon_rectangle':[36,105,28,28],'mode_icon_touch_rectangle':[32,101,36,35],'time_rectangle':[76,104,156,30],'time_alignment':'left','time_icon_gap':12,'play_mode_persistence':'current_boot','mode_icon_pixel_heap_bytes':0,'song_list_on_title_click':True,'song_list_on_header_click':True,'previous_touch_rectangle':[0,136,80,104],'next_touch_rectangle':[160,136,80,104],'page_single_tap':'native_control_action','page_double_tap':False,'page_long_press':'native_control_action','record_physical_key':2,'record_key_single':'play_pause','record_key_double':'next_track_and_play','record_key_long':'previous_track_and_play','double_tap_window_ms':320,'music_worker_period_ms':40,'other_worker_period_ms':500,'playback_touch_source':'native_control_callback','touch_message':False,'short_taps_between_LVGL_polls':False,'long_press_ms':600,'extra_GUI_tasks':False,'key_service':'existing_clock_worker','back_icon_hidden':True,'list_header_alignment':'left','battery_rectangle':[156,2,80,30],'stop_button_visible':False,'battery_percent':True,'battery_charging_indicator':True,'battery_heap_bytes':4800,'carousel_left':[61,59,60,61],'carousel_right':[61,60,59,61],'playing_to_AI_confirmation':True,'pause_and_save_before_handoff':True,'return_from_carousel_stays_paused':False,'settings_keep_playing':True,'settings_preserve_audio_owner':True,'AI_switch_still_confirms_and_pauses':True,'return_from_AI_stays_paused':True,'list_rows':6,'long_press_delete_confirmation':True,'delete_cancel_default':True,'native_play_pause_images':True,'native_previous_next_images':True,'native_progress_arcs':True,'in_place_refresh':True,'physical_device_verified':False},'music_uses_existing_player':True,'new_player_threads':False,'music_working_bytes':struct.unpack_from('<I',code,symbols['music_working_bytes']-va)[0],'added_Bluetooth':False,'new_coordinator_thread':False,'mode_poll_worker':'existing V0118 xztime thread','settings_page':13,'wifi_page':58,'removed_business_entries':['Sogou companion/QR/transcription','memo','legacy microphone effects'],
        'drivers_preserved_from_base':True,'raw_drivers_physically_deleted':False,
        'before_osa_preserved':host[:OSA]==base[8:8+OSA],'recovery_and_dsp_preserved':host[0x720000:]==base[8+0x720000:-8],
        'resc':[verify_resc(host,OSA,0x700000),verify_resc(host,0x720000,0x80000)],
        'blocked_page_ids':[6,7,19,*range(34,56)],
        'source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for folder in ('patches/xiaozhi-main','patches/xiaozhi-official') for p in sorted((ROOT/folder).rglob('*')) if p.is_file()},
        'physical_device_verified':False,'official_protocol_linked':True,'official_bootstrap_url':'https://api.tenclass.net/xiaozhi/ota/','cloud_firmware_ignored':True,
        'automatic_network_time':{'on_wifi_connect':True,'retry_seconds':60,'refresh_seconds':21600,'servers':['ntp.aliyun.com','time.cloudflare.com'],'protocol':'SNTP connected UDP/123; unauthenticated bootstrap','rtc_epoch':'UTC','rtc_time_t_bytes':8,'timezone_preserved':True,'valid_rtc_fallback':True,'worker_stack_bytes':3072,'diagnostic_file':'XZTIME.TXT'},
        'connection_memory':{'old_single_workspace_bytes':101232,'allocations_bytes':list(allocation_sizes),'working_bytes':sum(allocation_sizes),'largest_allocation_bytes':max(allocation_sizes),'saved_bytes':101232-sum(allocation_sizes),'bootstrap_and_voice_share_phase':True,'TLS_input_full_record':True,'TLS_output_plaintext_bytes':1024,'worker_stack_unchanged_bytes':65536,'physical_free_heap_measured':False,'allocation_fault_log':'XZMEM.TXT','retry_cooldown_ms':250},
        'reply_audio_finish':{'hold_output_between_replies':True,'normal_end_stops_DMA_DAI_codec':False,'normal_end_toggles_analog_mute':False,'ES8156_automute_register':'0x13','automute_mask':'0x01','disable_automute_while_owned':True,'restore_automute_on_close':True,'strict_I2C_message_counts':True,'DMA_zeroed_before_prepare':True,'fade_frames':160,'quiet_frames_before_idle':512,'rate':16000,'drain_uses_native_pointers':True,'idle_zero_DMA_requires_session_ticks':True,'late_TTS_stop_keeps_quiet_DMA':True,'resume_after_gap_fades_in':True,'native_XRUN_STOP_not_repeated':True,'timeout_ms':1500,'retained_DMA_bytes':8192,'physical_pop_eliminated_verified':False},
        'native_PCM_boundary_fix':{'native_prepare':'0x1803d2b8','capture_ring_frames':1280,'capture_boundary':'0x50000000','playback_ring_frames':2048,'playback_boundary':'0x40000000','largest_accepted_boundary':'0x7fffffff','unsigned_wrap_math':True,'old_fake_small_boundary_fixtures_replaced':True,'actual_native_prepare_replayed':True,'physical_microphone_verified':False},
        'capture_start_timing_fix':{'tick_refreshed_after_native_setup':True,'fault_log':'XZAUDIO.TXT','failed_key_preserves_error':True},
        'voice_pipeline':{'integrated':True,'runtime_readiness_depends_on_codec_and_native_devices':True,'Opus':'1.5.2 fixed point','Opus_library_sha256':opus_report['library_sha256'],'capture_rate':16000,'capture_channels':4,'uplink_channels':1,'uplink_frame_ms':20,'decode_rate':16000,'max_reply_frame_ms':120,'PCM_waveform':False,'recording_state_animation':True,'manual_RMS_disabled':True,'automatic_turn_VAD_retained':True,'waveform_pixel_heap_bytes':0,'waveform_heap_saved_bytes':6144,'waveform_readonly_flash_bytes':36936,'waveform_frames':6,'waveform_period_ms':150,'manual_and_hold_and_continuous':True,'automatic_silence_ms':900,'queue_packets':12,'physical_verified':False},
        'websocket_reply_text':{'WSS_and_hostname_verification':True,'server_sentence_text':True,'STT_text':True,'caption_limit_bytes':1536,'unicode_characters_per_page':18,'auto_page_ms':2500,'audio_playback_linked':True},
        'wake_reconnect':{'on_real_screen_wake':True,'only_foreground_AI_home':True,'states':['IDLE','FAILED','CLOSED'],'wait_for_wifi':True,'wait_for_worker_cleanup':True,'preserve_healthy_session':True,'attempts_per_wake':1,'new_retry_thread':False,'cancel_on_page_exit_USB_or_mode_switch':True,'physical_device_verified':False},
        'wake_input_recovery':{'native_wake_hook':'0x18051454','clear_stale_owned_and_down':True,'sync_voice_generation_and_phase':True,'page_reentry_supported':True,'native_record_activity_restored':True,'physical_tested':False},
        'screen_off_wifi':{'retain_radio_while_connected_or_AP':True,'display_can_sleep':True,'USB_and_shutdown_behavior_retained':True},
        'native_official_activation_linked':True,'native_TLS_linked':True,'identity_files':['XZAUTH0.CFG','XZAUTH1.CFG'],'worker_stack_bytes':65536,'TLS_root_sha256':provenance['root_der_sha256'],'stack_usage':stack_usage,
        'missing':['hardware verification of TLS, RTC epoch, socket error mapping and identity fsync','real official activation with C208 board identity','real device microphone channel/gain, realtime Opus speed and audible speaker output','board end-to-end voice test']}
    assert manifest['before_osa_preserved'] and manifest['recovery_and_dsp_preserved']
    for lo,hi in [(0x43000,0x46c00),(0x56738,0x56c30),(0x61000,0x65100),(0x720000,0x800000)]:assert host[lo:hi]==base[8+lo:8+hi]
    assert not any(name.startswith(('mf_','hp_')) for name in symbols)
    assert host[0x3fc52:0x3fc54]==bytes.fromhex('f5e7')
    return result,manifest,image

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output-directory',type=Path,default=OUT);args=parser.parse_args()
    if args.output_directory.exists():raise ValueError('Refusing to overwrite an existing candidate directory')
    try:package,m,elf=build()
    except subprocess.CalledProcessError as error:print((error.stderr or b'').decode());raise
    args.output_directory.mkdir(parents=True);(args.output_directory/'SOGOUOTA.BIN').write_bytes(package)
    (args.output_directory/'manifest.json').write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n');(args.output_directory/'main.elf').write_bytes(elf)
    print(json.dumps({key:m[key] for key in ('version','status','release_ready','package_sha256','ram_bytes')},ensure_ascii=False,indent=2))
