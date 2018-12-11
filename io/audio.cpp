#define MAL_IMPLEMENTATION
#include <windows.h>
#include "audio.h"
using namespace std;

#define FRAME_COUNT (1024)

fifo_buffer_t *fifo_new(size_t size)
{
    uint8_t    *buffer = NULL;
    fifo_buffer_t *buf = (fifo_buffer_t*)calloc(1, sizeof(*buf));
    if (!buf)
        return NULL;
    buffer = (uint8_t*)calloc(1, size + 1);
    if (!buffer)
    {
        free(buf);
        return NULL;
    }
    buf->buffer = buffer;
    buf->size = size + 1;
    return buf;
}

void fifo_write(fifo_buffer_t *buffer, const void *in_buf, size_t size)
{
    size_t first_write = size;
    size_t rest_write = 0;
    if (buffer->end + size > buffer->size)
    {
        first_write = buffer->size - buffer->end;
        rest_write = size - first_write;
    }
    memcpy(buffer->buffer + buffer->end, in_buf, first_write);
    memcpy(buffer->buffer, (const uint8_t*)in_buf + first_write, rest_write);
    buffer->end = (buffer->end + size) % buffer->size;
}


void fifo_read(fifo_buffer_t *buffer, void *in_buf, size_t size)
{
    size_t first_read = size;
    size_t rest_read = 0;
    if (buffer->first + size > buffer->size)
    {
        first_read = buffer->size - buffer->first;
        rest_read = size - first_read;
    }
    memcpy(in_buf, (const uint8_t*)buffer->buffer + buffer->first, first_read);
    memcpy((uint8_t*)in_buf + first_read, buffer->buffer, rest_read);
    buffer->first = (buffer->first + size) % buffer->size;
}

static mal_uint32 audio_callback(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
    return ((Audio*)pDevice->pUserData)->fill_buffer((uint8_t*)pSamples, frameCount);
}

static retro_time_t frame_limit_minimum_time = 0.0;
static retro_time_t frame_limit_last_time = 0.0;

bool Audio::init(double refreshra, retro_system_av_info av)
{
    sem = CreateEvent(NULL, TRUE, FALSE, NULL);
    InitializeCriticalSection(&cs);

    system_rate = av.timing.sample_rate;
    system_fps = av.timing.fps;
    if (fabs(1.0f - system_fps / refreshra) <= 0.05)
        system_rate *= (refreshra / system_fps);
    mal_context_config contextConfig = mal_context_config_init(NULL);
    mal_backend backends[] = {
        mal_backend_wasapi,
        mal_backend_dsound,
        mal_backend_winmm,
        mal_backend_null    // Lowest priority.
    };

    if (mal_context_init(backends, sizeof(backends) / sizeof(backends[0]), &contextConfig, &context) != MAL_SUCCESS)
    {
        printf("Failed to initialize context.");
        return false;
    };

    mal_device_config config = mal_device_config_init_playback(mal_format_f32, 2, 0, audio_callback);
    config.bufferSizeInFrames = (FRAME_COUNT);
    if (mal_device_init(&context, mal_device_type_playback, NULL, &config, this, &device) != MAL_SUCCESS) {
        mal_context_uninit(&context);
        return false;
    }
    client_rate = device.sampleRate;
    resamp_original = (client_rate / system_rate);
    resample = resampler_sinc_init(resamp_original);
    size_t sampsize = mal_device_get_buffer_size_in_bytes(&device);
    _fifo = fifo_new(sampsize); //number of bytes
    output_float = new float[sampsize * 2]; //spare space for resampler
    input_float = new float[FRAME_COUNT * 4];
    if (mal_device_start(&device) != MAL_SUCCESS)return false;
    frame_limit_minimum_time = (retro_time_t)roundf(1000000.0f / (av.timing.fps));

    return true;
}
void Audio::destroy()
{
    {
        mal_device_stop(&device);
        mal_context_uninit(&context);
        fifo_free(_fifo);
        DeleteCriticalSection(&cs);
        CloseHandle(sem);
        delete[] input_float;
        delete[] output_float;
        resampler_sinc_free(resample);
    }
}
void Audio::reset()
{
}

void Audio::mix(const int16_t* samples, size_t size)
{
    struct resampler_data src_data = { 0 };
    size_t written = 0;
    uint32_t in_len = size * sizeof(int16_t);
    double maxdelta = 0.005;
    auto bufferlevel = [this]() {return double((_fifo->size - (int)fifo_write_avail(this->_fifo)) / this->_fifo->size); };
    int newInputFrequency = ((1.0 - maxdelta) + 2.0 * (double)bufferlevel() * maxdelta) * system_rate;
    float drc_ratio = (float)client_rate / (float)newInputFrequency;
    mal_pcm_s16_to_f32(input_float, samples, in_len, mal_dither_mode_triangle);
    src_data.input_frames = size;
    src_data.ratio = drc_ratio;
    src_data.data_in = input_float;
    src_data.data_out = output_float;
    resampler_sinc_process(resample, &src_data);
    int out_len = src_data.output_frames * 2 * sizeof(float);
    while (written < out_len)
    {
        EnterCriticalSection(&cs);
        size_t avail = fifo_write_avail(_fifo);
        if (avail)
        {
            size_t write_amt = out_len - written > avail ? avail : out_len - written;
            fifo_write(_fifo, (const char*)output_float + written, write_amt);
            written += write_amt;
            LeaveCriticalSection(&cs);
        }
        else
        {
            LeaveCriticalSection(&cs);
            HRESULT result = WaitForSingleObject(sem, INFINITE);
            if (result == WAIT_OBJECT_0)
            {
                ResetEvent(sem);
            }

            continue;
        }
    }
}

mal_uint32 Audio::fill_buffer(uint8_t* out, mal_uint32 count) {
    count *= mal_get_bytes_per_frame(mal_format_f32, device.channels);
    EnterCriticalSection(&cs);
    size_t amount = fifo_read_avail(_fifo);
    amount = (count >= amount) ? amount : count;
    fifo_read(_fifo, out, amount);
    SetEvent(sem);
    LeaveCriticalSection(&cs);
    amount /= mal_get_bytes_per_frame(mal_format_f32, device.channels);
    return amount;
}

