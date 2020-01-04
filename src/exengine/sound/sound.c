#include <stdio.h>
#include <stdlib.h>
#include <physfs.h>
#include "util/io.h"
#include "sound/sound.h"
#include "sound/stb_vorbis.c"

ex_sound_t ex_sound;
ex_sound_devices_t *ex_sound_outputs = NULL;
ex_sound_devices_t *ex_sound_inputs = NULL;

void ex_sound_init()
{
  // reset AL error stack
  alGetError();

  // get a list of available devices
  ex_sound_outputs = malloc(sizeof(ex_sound_devices_t));
  ex_sound_inputs = malloc(sizeof(ex_sound_devices_t));
  ex_sound_list_devices(ex_sound_outputs, ALC_DEVICE_SPECIFIER);
  ex_sound_list_devices(ex_sound_inputs, ALC_CAPTURE_DEVICE_SPECIFIER);

  // print out available devices
  for (int i=0; i<ex_sound_outputs->len; i++)
    printf("Output Device [%s]\n", ex_sound_outputs->names[i]);
  for (int i=0; i<ex_sound_inputs->len; i++)
    printf("Input Device [%s]\n", ex_sound_inputs->names[i]);

  // set default device as output
  ex_sound_set_output(NULL);
  
  // init and set context
  ex_sound.context = alcCreateContext(ex_sound.output, NULL);
  if (!alcMakeContextCurrent(ex_sound.context)) {
    printf("Failed creating OpenAL context\n");
    return;
  }

  // setup default listener properties
  ALfloat pos[] = {0.0, 0.0, 1.0};
  ALfloat vel[] = {0.0, 0.0, 0.0};
  ALfloat ori[] = {0.0, 0.0, -1.0, 0.0, 1.0, 0.0};
  alListenerfv(AL_POSITION, pos);
  alListenerfv(AL_VELOCITY, vel);
  alListenerfv(AL_ORIENTATION, ori);
  alListenerf(AL_GAIN, 1.0);
}

void ex_sound_list_devices(ex_sound_devices_t *list, const ALenum param)
{
  // clear device list
  list->len = 0;
  for (int i=0; i<EX_DEVICE_LEN; i++)
    list->names[i][0] = '\0';

  // get NULL separated strings of devices
  const ALCchar *devices = alcGetString(NULL, param);
  const ALCchar *device = devices, *next = devices + 1;

  // add devices to the list
  while (device && *device != '\0' && next && *next != '\0') {
    strcpy(&list->names[list->len++][0], device);
    device += (strlen(device) + 1);
    next += (strlen(device) + 2);
  }
}

void ex_sound_set_output(const ALCchar *device)
{
  // set default device
  if (!device) {
    printf("Output device set as default\n");
    ex_sound.output = alcOpenDevice(NULL);
    return;
  }

  // attempt to set requested device
  ex_sound.output = alcOpenDevice(device);

  // check for success 
  ALCenum error;
  error = alGetError();
  if (error != AL_NO_ERROR) {
    printf("Error setting audio output device %s\n", device);
  }

  // success
  printf("Output device set as %s\n", device);
}

ex_source_t* ex_sound_load(const char *path, int type, int looping)
{
  printf("Loading audio file %s\n", path);
  int channels, rate;
  short *data = NULL;
  int32_t decode_len = 0;
  stb_vorbis *decoder = NULL;

  // get file extension
  char buf[512];
  ex_io_get_extension(buf, path);

  // decode ogg data
  if (strcmp(buf, ".ogg") == 0) {
    printf("Decoding ogg format\n");
    size_t len = 0;
    uint8_t *file_data = (uint8_t*)ex_io_read(path, "rb", &len);

    if (type == EX_SOURCE_STATIC) {
      // static source
      // decode all at once
      decode_len = stb_vorbis_decode_memory(file_data, len, &channels, &rate, &data);

      free(file_data);
    
      // loading failed
      if (decode_len <= 0) {
        printf("Failed decoding ogg file %s\n", path);
        return NULL;
      }
    } else {
      // streaming source
      // create a vorbis decoder
      int error;
      decoder = stb_vorbis_open_memory(file_data, len, &error, NULL);

      if (decoder == NULL)
        printf("Failed to create ogg decoder for streaming source %s\n", path);
    }
  } else {
    // unsupported file format
    printf("Failed loading sound file %s\n", path);
    printf("Format \"%s\" is not currently supported\n", buf);
    return NULL;
  }

  // create the source container
  ex_source_t *s = malloc(sizeof(ex_source_t));
  s->channels    = channels;
  s->rate        = rate / 2;
  s->streaming   = type;
  s->looping     = looping;
  s->sample      = 0;
  s->samples     = 0;
  s->stopped     = 0;

  // init the al source
  alGenSources(1, &s->id);
  alSourcef(s->id, AL_PITCH, 1);
  alSourcef(s->id, AL_GAIN, 0.05);
  alSource3f(s->id, AL_POSITION, 0, 0, 0);
  alSource3f(s->id, AL_VELOCITY, 0, 0, 0);
  alSourcei(s->id, AL_LOOPING, s->looping);
  if (type == EX_SOURCE_STREAMING)
    alSourcei(s->id, AL_SOURCE_TYPE, AL_STREAMING);
  else
    alSourcei(s->id, AL_SOURCE_TYPE, AL_STATIC);

  if (s->streaming) {
    // 3 buffer queue
    alGenBuffers(3, &s->buffers[0]);
    
    // set the buffers ready for data
    for (int i=0; i<3; i++)
      s->ready_buffers[i] = s->buffers[i];

    // set decoder
    s->decoder = (void*)decoder;

    // seek to start
    stb_vorbis_seek((stb_vorbis*)s->decoder, 0);

    // get PCM info
    stb_vorbis_info info;
    info = stb_vorbis_get_info((stb_vorbis*)s->decoder);

    // set some vars
    s->samples  = stb_vorbis_stream_length_in_samples((stb_vorbis*)s->decoder);
    s->rate     = info.sample_rate / 2;
    s->channels = info.channels;

    // malloc a buffer for decoding
    s->decode_buffer_shorts = 0x4000;
    s->decode_buffer_bytes  = s->decode_buffer_shorts * sizeof(short);
    s->decode_buffer        = malloc(s->decode_buffer_bytes);
  } else {
    // single buffer
    uint32_t length = decode_len * channels * (sizeof(int16_t) / sizeof(uint8_t));
    alGenBuffers(1, &s->buffers[0]);
    alBufferData(s->buffers[0], AL_FORMAT_STEREO16, data, length, s->rate);
    
    // bind buffer to source
    alSourcei(s->id, AL_BUFFER, s->buffers[0]);
  }

  free(data);

  return s;
}

void ex_sound_restart(ex_source_t *s)
{
  alSourceStop(s->id);
  s->sample  = 0;
  s->stopped = 0;

  // unqueue any buffers that are left
  if (s->streaming) {
    // find out how many buffers are still queued
    ALint buffers_done = 0;
    alGetSourcei(s->id, AL_BUFFERS_PROCESSED, &buffers_done);

    // unqueue those buffers
    ALuint buffers[3];
    alSourceUnqueueBuffers(s->id, buffers_done, buffers);

    for (int i=0; i<3; i++)
      s->ready_buffers[i] = s->buffers[i];
  }

  alSourcePlay(s->id);
}

void ex_sound_play(ex_source_t *s)
{
  // dont continue stopped stream until restarted
  if (s->streaming && s->stopped)
    return;

  // restart source if static
  if (!s->streaming)
    ex_sound_restart(s);

  if (!ex_sound_playing(s))
    alSourcePlay(s->id);

  // only streaming buffers after this
  if (!s->streaming)
    return;

  // find out how many buffers are done
  ALint buffers_done = 0;
  alGetSourcei(s->id, AL_BUFFERS_PROCESSED, &buffers_done);

  // unqueue those buffers
  ALuint buffers[3];
  alSourceUnqueueBuffers(s->id, buffers_done, buffers);

  // add done buffers back to ready buffers
  for (int i=0; i<buffers_done; i++) {
    for (int j=0; j<3; j++) {
      if (s->ready_buffers[j] == -1) {
        s->ready_buffers[j] = buffers[i];
        break;
      }
    }
  }

  short *data   = s->decode_buffer;
  size_t shorts = s->decode_buffer_shorts;
  size_t len    = s->decode_buffer_bytes;

  // decode and queue more PCM data
  for (int i=0; i<3; i++) {
    if (s->ready_buffers[i] == -1)
      continue;

    // get buffer to fill
    ALuint buff = s->ready_buffers[i];
    s->ready_buffers[i] = -1;

    // decode some data
    stb_vorbis_seek((stb_vorbis*)s->decoder, s->sample);
    s->sample += stb_vorbis_get_samples_short_interleaved((stb_vorbis*)s->decoder, s->channels, data, shorts);
    alBufferData(buff, AL_FORMAT_STEREO16, data, len, s->rate);

    // end of stream
    if (s->sample >= s->samples) {
      if (s->looping) {
        s->sample  = 0;
      } else {
        alSourceStop(s->id);
        s->stopped = 1;
        s->sample  = 0;
      }
    }

    // queue buffer
    alSourceQueueBuffers(s->id, 1, &buff);
  }
}

void ex_sound_destroy(ex_source_t *s)
{
  alDeleteSources(1, &s->id);
  
  if (s->streaming) {
    alDeleteBuffers(1, &s->buffers[0]);
    free(s->decode_buffer);
    s->decode_buffer = NULL;
  } else {
    alDeleteBuffers(3, &s->buffers[0]);
  }

  free(s);
  s = NULL;
}

void ex_sound_exit()
{
  alcMakeContextCurrent(NULL);
  alcDestroyContext(ex_sound.context);
  alcCloseDevice(ex_sound.output);

  free(ex_sound_outputs);
  ex_sound_outputs = NULL;

  free(ex_sound_inputs);
  ex_sound_inputs = NULL;
}