#pragma once

#include <CoreAudio/CoreAudio.h>
#include <vector>

namespace audio_engine {

struct MacOutputDevice {
    AudioDeviceID id;
    UInt32 transport;
};

inline bool isDisplayOutputTransport(UInt32 transport) {
    return transport == kAudioDeviceTransportTypeHDMI ||
           transport == kAudioDeviceTransportTypeDisplayPort;
}

// Follow the system's audio device except for monitor audio, which the player
// excludes. Prefer the built-in speaker when the system selects a monitor.
inline AudioDeviceID choosePlaybackOutputDevice(AudioDeviceID system_default,
                                                const std::vector<MacOutputDevice>& devices) {
    for (const auto& device : devices)
        if (device.id == system_default && !isDisplayOutputTransport(device.transport))
            return device.id;
    for (const auto& device : devices)
        if (device.transport == kAudioDeviceTransportTypeBuiltIn)
            return device.id;
    for (const auto& device : devices)
        if (!isDisplayOutputTransport(device.transport))
            return device.id;
    return kAudioObjectUnknown;
}

inline UInt32 getOutputDeviceTransport(AudioDeviceID device) {
    UInt32 transport = 0;
    UInt32 bytes = sizeof(transport);
    AudioObjectPropertyAddress address{kAudioDevicePropertyTransportType,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    AudioObjectGetPropertyData(device, &address, 0, nullptr, &bytes, &transport);
    return transport;
}

inline AudioDeviceID getSystemDefaultOutputDeviceID() {
    AudioDeviceID system_default = kAudioObjectUnknown;
    UInt32 bytes = sizeof(system_default);
    AudioObjectPropertyAddress address{kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr,
                                  &bytes, &system_default) != noErr)
        return kAudioObjectUnknown;
    return system_default;
}

inline AudioDeviceID getPlaybackOutputDeviceID() {
    const AudioDeviceID system_default = getSystemDefaultOutputDeviceID();
    if (system_default != kAudioObjectUnknown &&
        !isDisplayOutputTransport(getOutputDeviceTransport(system_default)))
        return system_default;

    AudioObjectPropertyAddress address{kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    UInt32 bytes = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, nullptr, &bytes) != noErr)
        return kAudioObjectUnknown;
    std::vector<AudioDeviceID> ids(bytes / sizeof(AudioDeviceID));
    if (ids.empty() || AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr,
                                                 &bytes, ids.data()) != noErr)
        return kAudioObjectUnknown;

    std::vector<MacOutputDevice> outputs;
    for (const auto id : ids) {
        AudioObjectPropertyAddress property{kAudioDevicePropertyDeviceIsAlive,
            kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
        UInt32 alive = 0;
        bytes = sizeof(alive);
        if (AudioObjectGetPropertyData(id, &property, 0, nullptr, &bytes, &alive) != noErr || !alive)
            continue;
        property.mSelector = kAudioDevicePropertyStreams;
        property.mScope = kAudioObjectPropertyScopeOutput;
        bytes = 0;
        if (AudioObjectGetPropertyDataSize(id, &property, 0, nullptr, &bytes) == noErr && bytes > 0)
            outputs.push_back({id, getOutputDeviceTransport(id)});
    }
    return choosePlaybackOutputDevice(system_default, outputs);
}

// Keep macOS media-volume keys on the same output as playback after excluding
// a monitor. Recheck the default so a concurrent user switch to headphones wins.
// Call from the device worker, never from getters or the render callback.
template <typename ReadDefault, typename WriteDefault>
inline OSStatus synchronizeSystemOutputWithPlaybackDevice(MacOutputDevice expected_default,
                                                         MacOutputDevice playback_device,
                                                         ReadDefault read_default,
                                                         WriteDefault write_default) {
    if (playback_device.id == kAudioObjectUnknown || playback_device.id == expected_default.id ||
        !isDisplayOutputTransport(expected_default.transport) ||
        isDisplayOutputTransport(playback_device.transport) ||
        read_default() != expected_default.id)
        return noErr;
    return write_default(playback_device.id);
}

inline OSStatus synchronizeSystemOutputWithPlaybackDevice(AudioDeviceID expected_default,
                                                         AudioDeviceID playback_device) {
    return synchronizeSystemOutputWithPlaybackDevice(
        {expected_default, getOutputDeviceTransport(expected_default)},
        {playback_device, getOutputDeviceTransport(playback_device)},
        getSystemDefaultOutputDeviceID, [](AudioDeviceID device) {
            AudioObjectPropertyAddress address{kAudioHardwarePropertyDefaultOutputDevice,
                kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
            return AudioObjectSetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr,
                                              sizeof(device), &device);
        });
}

} // namespace audio_engine
