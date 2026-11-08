// SPDX-License-Identifier: AGPL-3.0-or-later
// Decode raw MIDI bytes into the events the engine uses (Spec S3.2–S3.4).
// Works on the bytes in place — no copies, no allocation — so it is safe on the audio thread
// (Spec S10.2). Unknown and system messages (incl. SysEx) decode to Kind::None.
#pragma once
#include <cstdint>

namespace lmw::dsp {

struct MidiEvent {
    enum class Kind { None, NoteOn, NoteOff, AllNotesOff, Sustain, PitchBend, ModWheel } kind = Kind::None;
    int channel = 1;        // 1..16
    int note = 0;
    float value = 0.0f;     // NoteOn: velocity 0..1; Sustain: 1 = down, 0 = up; PitchBend: -1..+1; ModWheel: 0..1
};

inline MidiEvent parseMidi(const std::uint8_t* d, int size) noexcept {
    MidiEvent e;
    if (d == nullptr || size < 1 || d[0] < 0x80 || d[0] >= 0xF0) return e;
    const int status = d[0] & 0xF0;
    e.channel = (d[0] & 0x0F) + 1;
    const int a = size > 1 ? d[1] & 0x7F : 0;
    const int b = size > 2 ? d[2] & 0x7F : 0;
    switch (status) {
        case 0x90:
            if (size < 3) break;
            e.note = a;
            if (b > 0) { e.kind = MidiEvent::Kind::NoteOn; e.value = static_cast<float>(b) / 127.0f; }
            else       { e.kind = MidiEvent::Kind::NoteOff; }          // note-on with velocity 0
            break;
        case 0x80:
            if (size < 3) break;
            e.kind = MidiEvent::Kind::NoteOff; e.note = a;
            break;
        case 0xB0:
            if (size < 3) break;
            if (a == 64)                   { e.kind = MidiEvent::Kind::Sustain; e.value = b >= 64 ? 1.0f : 0.0f; }
            else if (a == 120 || a == 123) { e.kind = MidiEvent::Kind::AllNotesOff; }   // all sound / notes off
            else if (a == 1)               { e.kind = MidiEvent::Kind::ModWheel; e.value = static_cast<float>(b) / 127.0f; }
            break;
        case 0xE0:
            if (size < 3) break;
            e.kind = MidiEvent::Kind::PitchBend;
            e.value = static_cast<float>(((b << 7) | a) - 8192) / 8192.0f;
            break;
        default: break;
    }
    return e;
}

} // namespace lmw::dsp
