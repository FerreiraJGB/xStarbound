#ifndef STAR_VOICE_HPP
#define STAR_VOICE_HPP
#include "StarJson.hpp"
#include "StarBiMap.hpp"
#include "StarException.hpp"
#include "StarGameTypes.hpp"
#include "StarMaybe.hpp"
#include "StarThread.hpp"
#include "StarDataStreamDevices.hpp"
#include "StarApplicationController.hpp"

#include <queue>

struct OpusDecoder;
typedef std::unique_ptr<OpusDecoder, void(*)(OpusDecoder*)> OpusDecoderPtr;
struct OpusEncoder;
typedef std::unique_ptr<OpusEncoder, void(*)(OpusEncoder*)> OpusEncoderPtr;

namespace Star {

STAR_EXCEPTION(VoiceException, StarException);

enum class VoiceInputMode : uint8_t { VoiceActivity, PushToTalk };
extern EnumMap<VoiceInputMode> const VoiceInputModeNames;

enum class VoiceChannelMode: uint8_t { Mono = 1, Stereo = 2 };
extern EnumMap<VoiceChannelMode> const VoiceChannelModeNames;

STAR_CLASS(Voice);
STAR_CLASS(VoiceAudioStream);
STAR_CLASS(ApplicationController);

struct VoiceAudioChunk {
  std::unique_ptr<int16_t[]> data;
  size_t remaining;
  size_t offset = 0;

  VoiceAudioChunk(int16_t* ptr, size_t size) {
    data.reset(ptr);
    remaining = size;
    offset = 0;
  }

  inline size_t takeSamples(std::vector<int16_t>& out, size_t count) {
    size_t toRead = std::min<size_t>(count, remaining);
    int16_t* start = data.get() + offset;
    out.insert(out.end(), start, start + toRead);
    offset += toRead;
    remaining -= toRead;
    return toRead;
  }

  //this one's unsafe
  inline int16_t takeSample() {
    --remaining;
    return *(data.get() + offset++);
  }

  inline bool exhausted() { return remaining == 0; }
};


class Voice {
public:
  // Individual speakers are represented by their connection ID.
  typedef ConnectionId SpeakerId;

  class Speaker {
  public:
    SpeakerId speakerId = 0;
    EntityId entityId = 0;

    Vec2F position = Vec2F();
    String name = "Unnamed";

    OpusDecoderPtr decoderMono;
    OpusDecoderPtr decoderStereo;
    VoiceAudioStreamPtr audioStream;
    Mutex mutex;

    atomic<bool> muted = false;
    atomic<bool> playing = false;
    atomic<float> decibelLevel = 0.0f;
    atomic<Array<float, 2>> channelVolumes = Array<float, 2>::filled(1.0f);

    unsigned int minimumPlaySamples = 4096;

    Speaker(SpeakerId speakerId);
  };

  typedef std::shared_ptr<Speaker> SpeakerPtr;

  // Get pointer to the singleton Voice instance, if it exists.  Otherwise,
  // returns nullptr.
  static Voice* singletonPtr();

  // Gets reference to Voice singleton, throws VoiceException if root
  // is not initialized.
  static Voice& singleton();

  Voice(ApplicationControllerPtr appController);
  ~Voice();

  Voice(Voice const&) = delete;
  Voice& operator=(Voice const&) = delete;

  void init();

  void loadJson(Json const& config);
  Json saveJson() const;

  void save() const;
  void scheduleSave();

  // Sets the local speaker ID and returns the local speaker. Must be called upon loading into a world.
  SpeakerPtr setLocalSpeaker(SpeakerId speakerId);
  SpeakerPtr speaker(SpeakerId speakerId);

  // Called when receiving input audio data from SDL, on its own thread.
  void readAudioData(uint8_t* stream, int len);

  // Called to mix voice audio with the game.
  void mix(int16_t* buffer, size_t frames, unsigned channels);

  typedef function<float(unsigned, Vec2F, float)> PositionalAttenuationFunction;
  void update(PositionalAttenuationFunction positionalAttenuationFunction = {});

  void setDeviceName(Maybe<String> device);

  int send(DataStreamBuffer& out, size_t budget);
  bool receive(SpeakerPtr speaker, std::string_view view);

  // Must be called every frame with input state, expires after 1s.
  void setInput(bool input = true);

  inline int encoderChannels() const {
    return m_channelMode == VoiceChannelMode::Mono ? 1 : 2;
  }
private:
  static Voice* s_singleton;

  static OpusDecoder* createDecoder(int channels);
  static OpusEncoder* createEncoder(int channels);
  void resetEncoder();
  void openDevice();
  void closeDevice();

  bool playSpeaker(SpeakerPtr const& speaker, int channels);

  SpeakerId m_speakerId = 0;
  SpeakerPtr m_clientSpeaker;
  HashMap<SpeakerId, SpeakerPtr> m_speakers;

  Mutex m_activeSpeakersMutex;
  HashSet<SpeakerPtr> m_activeSpeakers;

  OpusEncoderPtr m_encoder;

  float m_outputVolume = 1.0f;
  float m_inputVolume = 1.0f;
  float m_threshold = -50.0f;
  
  int64_t m_lastSentTime = 0;
  int64_t m_lastInputTime = 0;
  int64_t m_lastThresholdTime = 0;
  int64_t m_nextSaveTime = 0;
  bool m_enabled = true;
  bool m_inputEnabled = true;

  int m_deviceChannels = 1;
  bool m_deviceOpen = false;
  Maybe<String> m_deviceName;
  VoiceInputMode m_inputMode;
  VoiceChannelMode m_channelMode;

  ApplicationControllerPtr m_applicationController;

  struct EncodedChunk {
    std::unique_ptr<unsigned char[]> data;
    size_t size;

    EncodedChunk(unsigned char* _data, size_t len) {
      data.reset(_data);
      size = len;
    }
  };

  std::vector<ByteArray> m_encodedChunks;
  size_t m_encodedChunksLength = 0;

  std::queue<VoiceAudioChunk> m_capturedChunks;
  size_t m_capturedChunksFrames = 0;

  Mutex m_captureMutex;
};
  
}

#endif