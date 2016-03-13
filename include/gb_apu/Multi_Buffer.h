// Multi-channel sound buffer interface, and basic mono and stereo buffers

// Blip_Buffer 0.4.1
#ifndef MULTI_BUFFER_H
#define MULTI_BUFFER_H

#include "gb_apu/Blip_Buffer.h"
#include "types.h"

class Tracked_Blip_Buffer : public Blip_Buffer {
public:
    // Non-zero if buffer still has non-silent samples in it. Requires that you call
    // set_modified() appropriately.
    u32 non_silent() const;
public:
    Tracked_Blip_Buffer();
    long read_samples( s16*, long );
    void remove_silence( long );
    void remove_samples( long );
    void clear();
    void end_frame( s32 );
private:
    s32 last_non_silence;
    void remove_( long );
};

class Stereo_Mixer {
public:
    Tracked_Blip_Buffer* bufs [3];
    s32 samples_read;

    Stereo_Mixer() : samples_read( 0 ) { }
    void read_pairs( s16* out, int count );
private:
    void mix_mono  ( s16* out, int pair_count );
    void mix_stereo( s16* out, int pair_count );
};

// Uses three buffers (one for center) and outputs stereo sample pairs.
class Stereo_Buffer {
public:
    // Buffers used for all channels
    Blip_Buffer* center()   { return &bufs [2]; }
    Blip_Buffer* left()     { return &bufs [0]; }
    Blip_Buffer* right()    { return &bufs [1]; }

public:
    Stereo_Buffer();
    const char* set_sample_rate( long, int msec = blip_default_length );
    void clock_rate( long );
    void bass_freq( int );
    void clear();
    void end_frame( s32 );

    long samples_avail() const { return (bufs [0].samples_avail() - mixer.samples_read) * 2; }
    long read_samples( s16*, long );

private:
    // noncopyable
    Stereo_Buffer( const Stereo_Buffer& );
    Stereo_Buffer& operator = ( const Stereo_Buffer& );

    enum { bufs_size = 3 };
    Tracked_Blip_Buffer bufs [bufs_size];
    Stereo_Mixer mixer;
};

#endif