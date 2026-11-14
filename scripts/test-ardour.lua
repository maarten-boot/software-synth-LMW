-- SPDX-License-Identifier: AGPL-3.0-or-later
-- Spec S9.2, Phase 8c: LMW-Synth in Ardour 9.9, headless (ardour9-lua, Dummy backend). Run by
-- scripts/test-ardour.sh. Prints PASS/FAIL lines and finally "ALL PASS" or "<n> FAILED".
-- Ardour's Lua print() buffers stdout (lost if it crashes), so print to stderr.
print = function(...) local t = {} for i = 1, select("#", ...) do t[#t + 1] = tostring(select(i, ...)) end io.stderr:write(table.concat(t, " ") .. "\n") end
local dir, name = "/work/sess", "lmwtest"
local fails = 0
local function check(ok, what) print((ok and "PASS " or "FAIL ") .. what); if not ok then fails = fails + 1 end end
local function sh(cmd) local f = io.popen(cmd .. " 2>&1"); local r = f:read("a"); f:close(); return (r:gsub("%s+$", "")) end

local s = create_session(dir, name, 48000)
check(s ~= nil, "session created (Dummy backend, 48 kHz)")
local info = ARDOUR.LuaAPI.new_plugin_info("LMW-Synth", ARDOUR.PluginType.VST3)
check(not info:isnil(), "LMW-Synth found by Ardour's VST3 scan")
-- The route group must be ARDOUR.RouteGroup() (nil crashes); a track named "Synth" clashes with an existing port name.
tl = Session:new_midi_track(ARDOUR.ChanCount(ARDOUR.DataType("midi"), 1), ARDOUR.ChanCount(ARDOUR.DataType("audio"), 2),
    true, info, nil, ARDOUR.RouteGroup(), 1, "LMW", ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal, true)
local tr = tl:front()
local ins = tr:nth_plugin(0):to_insert()
local plug = ins:plugin(0)
print("plugin: " .. plug:name() .. ", " .. plug:parameter_count() .. " parameters")
check(plug:name() == "LMW-Synth", "instrument on the MIDI track")

local function find(label)
    for i = 0, plug:parameter_count() - 1 do if plug:parameter_label(i) == label then return i end end
    return nil
end
local function midi(a, b, c)
    local p = ARDOUR.RawMidiParser()
    p:process_byte(a); p:process_byte(b)
    if p:process_byte(c) then ins:write_immediate_event(Evoral.EventType.MIDI_EVENT, p:buffer_size(), p:midi_buffer()) end
end
local function level()   -- highest peak since the last call, dBFS, both audio channels
    local m = tr:peak_meter()
    local l = math.max(m:meter_level(1, ARDOUR.MeterType.MeterPeak), m:meter_level(2, ARDOUR.MeterType.MeterPeak))   -- meter 0 is the MIDI input
    m:reset_max()
    return l
end
tr:input():disconnect_all(nil)   -- the Dummy backend's MIDI inputs can generate events

-- Sound: silent before, a note gives a level, silent again after the release.
sleep(1); level(); sleep(0.5)
local quiet = level()
midi(0x90, 60, 100); sleep(1)
local loud = level()
midi(0x80, 60, 0); sleep(2); level(); sleep(0.5)
local after = level()
print(string.format("meter: before %.1f dB, note C4 %.1f dB, after release %.1f dB", quiet, loud, after))
check(quiet < -80, "silent before the note")
check(loud > -60, "note C4 sounds")
check(after < -80, "silent after the release")

-- Parameters: Ardour -> plugin, read back over 9P (S10.10).
print("9P endpoint: " .. sh("ls ${XDG_RUNTIME_DIR:-/tmp/lmw-synth-$(id -u)}/lmw-synth 2>/dev/null || ls /tmp/lmw-synth-$(id -u)"))
local vol = find("Volume")
check(vol ~= nil, "parameter 'Volume' listed by Ardour")
local function get(i) local v, ok = ARDOUR.LuaAPI.get_processor_param(tr:nth_plugin(0), i, false); return v end
print(string.format("Volume: Ardour %.4f, 9P %s", get(vol), sh("lmw9p cat /params/out/gain")))
check(ARDOUR.LuaAPI.set_processor_param(tr:nth_plugin(0), vol, 0.5), "Ardour sets Volume")
sleep(0.5)
local g = tonumber(sh("lmw9p cat /params/out/gain"))
print("after Ardour set 0.5: 9P out.gain = " .. tostring(g))
check(g ~= nil and math.abs(g - 0.5) < 0.01, "Ardour's change reaches the plugin")
-- A second parameter, set from Ardour, to check after the reload.
local cut = find("VCF 1 Cutoff")
check(cut ~= nil and ARDOUR.LuaAPI.set_processor_param(tr:nth_plugin(0), cut, 0.5), "Ardour sets VCF 1 Cutoff (normalised 0.5)")
sleep(0.5)
local cut9p = sh("lmw9p cat /params/vcf1/cutoff")
print("VCF 1 Cutoff over 9P: " .. cut9p)
-- 9P writes, cables and notes need the plugin's message thread, which in Ardour runs on the GUI's
-- GLib main loop (VST3 IRunLoop); this headless tool has none, so those are tested with the GUI.

-- Save, close, reopen: the parameters come back from Ardour's session.
Session:save_state("", false, false, false, false, false)
local function release() tr, ins, plug, tl, info, tr2 = nil, nil, nil, nil, nil, nil; collectgarbage(); collectgarbage() end
release()   -- drop our references, so the plugin instance can go with the session
close_session()
sleep(1)
local s2 = load_session(dir, name)
check(s2 ~= nil, "session reopened")
tr2 = Session:route_by_name("LMW"):to_track()
check(not tr2:isnil() and tr2:nth_plugin(0):to_insert():plugin(0):name() == "LMW-Synth", "LMW-Synth restored on the track")
sleep(1)
local gain, cutoff = sh("lmw9p cat /params/out/gain"), sh("lmw9p cat /params/vcf1/cutoff")
print("after reload: out.gain " .. gain .. ", vcf1.cutoff " .. cutoff)
check(math.abs(tonumber(gain) - 0.5) < 0.01, "Volume restored")
check(cutoff == cut9p, "VCF 1 Cutoff restored")
-- sound again after the reload
tr = tr2   -- the helpers use these upvalues
tr:input():disconnect_all(nil)
ins = tr:nth_plugin(0):to_insert()
sleep(1); level(); sleep(0.5)
midi(0x90, 64, 100); sleep(1)
local loud2 = level(); midi(0x80, 64, 0)
print(string.format("after reload, note E4: %.1f dB", loud2))
check(loud2 > -60, "plays after the reload (quieter: Volume 0.5, cutoff 600 Hz)")

release()
close_session()
print(fails == 0 and "ALL PASS" or (fails .. " FAILED"))
quit()
