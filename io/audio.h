#ifndef AUDIO_H
#define AUDIO_H

#include <list>
#include "../stdafx.h"
#include "../3rdparty/mini_al.h"
#include "../3rdparty/libretro.h"
#include "../3rdparty/rthreads.h"
#include "../3rdparty/resampler.h"


#ifdef __cplusplus
extern "C" {
#endif
    struct fifo_buffer
    {
        uint8_t *buffer;
        size_t size;
        size_t first;
        size_t end;
    };

    typedef struct fifo_buffer fifo_buffer_t;

    fifo_buffer_t *fifo_new(size_t size);

    static __forceinline void fifo_clear(fifo_buffer_t *buffer)
    {
        buffer->first = 0;
        buffer->end = 0;
    }

    void fifo_write(fifo_buffer_t *buffer, const void *in_buf, size_t size);

    void fifo_read(fifo_buffer_t *buffer, void *in_buf, size_t size);

    static __forceinline void fifo_free(fifo_buffer_t *buffer)
    {
        if (!buffer)
            return;

        free(buffer->buffer);
        free(buffer);
    }

    static __forceinline size_t fifo_read_avail(fifo_buffer_t *buffer)
    {
        return (buffer->end + ((buffer->end < buffer->first) ? buffer->size : 0)) - buffer->first;
    }

    static __forceinline size_t fifo_write_avail(fifo_buffer_t *buffer)
    {
        return (buffer->size - 1) - ((buffer->end + ((buffer->end < buffer->first) ? buffer->size : 0)) - buffer->first);
    }

    long long microseconds_now();
    long long milliseconds_now();

    class Audio
    {
    public:
        mal_context context;
        mal_device device;
        unsigned client_rate;
        fifo_buffer* _fifo;
        float fps;
        double system_fps;
        double skew;
        double system_rate;
        double resamp_original;
        void* resample;
        float *input_float;
        float *output_float;
        HANDLE sem;
        CRITICAL_SECTION cs;
        bool init(double refreshra, retro_system_av_info av);
        void destroy();
        void reset();
        void mix(const int16_t* samples, size_t sample_count);
        mal_uint32 fill_buffer(uint8_t* pSamples, mal_uint32 samplecount);
    };
#ifdef __cplusplus
}
#endif

#endif AUDIO_H