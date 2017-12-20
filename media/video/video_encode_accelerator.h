// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_export.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"

namespace media {

class BitstreamBuffer;
class VideoFrame;

// Video encoder interface.
class MEDIA_EXPORT VideoEncodeAccelerator {
 public:
  // Specification of an encoding profile supported by an encoder.
  struct MEDIA_EXPORT SupportedProfile {
    SupportedProfile();
    ~SupportedProfile();
    VideoCodecProfile profile;
    gfx::Size max_resolution;
    uint32_t max_framerate_numerator;
    uint32_t max_framerate_denominator;
  };
  using SupportedProfiles = std::vector<SupportedProfile>;

  // Enumeration of potential errors generated by the API.
  enum Error {
    // An operation was attempted during an incompatible encoder state.
    kIllegalStateError,
    // Invalid argument was passed to an API method.
    kInvalidArgumentError,
    // A failure occurred at the GPU process or one of its dependencies.
    // Examples of such failures include GPU hardware failures, GPU driver
    // failures, GPU library failures, GPU process programming errors, and so
    // on.
    kPlatformFailureError,
    kErrorMax = kPlatformFailureError
  };

  // Interface for clients that use VideoEncodeAccelerator. These callbacks will
  // not be made unless Initialize() has returned successfully.
  class MEDIA_EXPORT Client {
   public:
    // Callback to tell the client what size of frames and buffers to provide
    // for input and output.  The VEA disclaims use or ownership of all
    // previously provided buffers once this callback is made.
    // Parameters:
    //  |input_count| is the number of input VideoFrames required for encoding.
    //  The client should be prepared to feed at least this many frames into the
    //  encoder before being returned any input frames, since the encoder may
    //  need to hold onto some subset of inputs as reference pictures.
    //  |input_coded_size| is the logical size of the input frames (as reported
    //  by VideoFrame::coded_size()) to encode, in pixels.  The encoder may have
    //  hardware alignment requirements that make this different from
    //  |input_visible_size|, as requested in Initialize(), in which case the
    //  input VideoFrame to Encode() should be padded appropriately.
    //  |output_buffer_size| is the required size of output buffers for this
    //  encoder in bytes.
    virtual void RequireBitstreamBuffers(unsigned int input_count,
                                         const gfx::Size& input_coded_size,
                                         size_t output_buffer_size) = 0;

    // Callback to deliver encoded bitstream buffers.  Ownership of the buffer
    // is transferred back to the VEA::Client once this callback is made.
    // Parameters:
    //  |bitstream_buffer_id| is the id of the buffer that is ready.
    //  |payload_size| is the byte size of the used portion of the buffer.
    //  |key_frame| is true if this delivered frame is a keyframe.
    //  |timestamp| is the same timestamp as in VideoFrame passed to Encode().
    virtual void BitstreamBufferReady(int32_t bitstream_buffer_id,
                                      size_t payload_size,
                                      bool key_frame,
                                      base::TimeDelta timestamp) = 0;

    // Error notification callback. Note that errors in Initialize() will not be
    // reported here, but will instead be indicated by a false return value
    // there.
    virtual void NotifyError(Error error) = 0;

   protected:
    // Clients are not owned by VEA instances and should not be deleted through
    // these pointers.
    virtual ~Client() {}
  };

  // Video encoder functions.

  // Returns a list of the supported codec profiles of the video encoder. This
  // can be called before Initialize().
  virtual SupportedProfiles GetSupportedProfiles() = 0;

  // Initializes the video encoder with specific configuration.  Called once per
  // encoder construction.  This call is synchronous and returns true iff
  // initialization is successful.
  // Parameters:
  //  |input_format| is the frame format of the input stream (as would be
  //  reported by VideoFrame::format() for frames passed to Encode()).
  //  |input_visible_size| is the resolution of the input stream (as would be
  //  reported by VideoFrame::visible_rect().size() for frames passed to
  //  Encode()).
  //  |output_profile| is the codec profile of the encoded output stream.
  //  |initial_bitrate| is the initial bitrate of the encoded output stream,
  //  in bits per second.
  //  |client| is the client of this video encoder.  The provided pointer must
  //  be valid until Destroy() is called.
  // TODO(sheu): handle resolution changes.  http://crbug.com/249944
  virtual bool Initialize(VideoPixelFormat input_format,
                          const gfx::Size& input_visible_size,
                          VideoCodecProfile output_profile,
                          uint32_t initial_bitrate,
                          Client* client) = 0;

  // Encodes the given frame.
  // Parameters:
  //  |frame| is the VideoFrame that is to be encoded.
  //  |force_keyframe| forces the encoding of a keyframe for this frame.
  virtual void Encode(const scoped_refptr<VideoFrame>& frame,
                      bool force_keyframe) = 0;

  // Send a bitstream buffer to the encoder to be used for storing future
  // encoded output.  Each call here with a given |buffer| will cause the buffer
  // to be filled once, then returned with BitstreamBufferReady().
  // Parameters:
  //  |buffer| is the bitstream buffer to use for output.
  virtual void UseOutputBitstreamBuffer(const BitstreamBuffer& buffer) = 0;

  // Request a change to the encoding parameters.  This is only a request,
  // fulfilled on a best-effort basis.
  // Parameters:
  //  |bitrate| is the requested new bitrate, in bits per second.
  //  |framerate| is the requested new framerate, in frames per second.
  virtual void RequestEncodingParametersChange(uint32_t bitrate,
                                               uint32_t framerate) = 0;

  // Destroys the encoder: all pending inputs and outputs are dropped
  // immediately and the component is freed.  This call may asynchronously free
  // system resources, but its client-visible effects are synchronous. After
  // this method returns no more callbacks will be made on the client. Deletes
  // |this| unconditionally, so make sure to drop all pointers to it!
  virtual void Destroy() = 0;

  // Encode tasks include these methods that are used frequently during the
  // session: Encode(), UseOutputBitstreamBuffer(),
  // RequestEncodingParametersChange(), Client::BitstreamBufferReady().
  // If the Client can support running these on a separate thread, it may
  // call this method to try to set up the VEA implementation to do so.
  //
  // If the VEA can support this as well, return true, otherwise return false.
  // If true is returned, the client may submit each of these calls on
  // |encode_task_runner|, and then expect Client::BitstreamBufferReady() to be
  // called on |encode_task_runner| as well; called on |encode_client|, instead
  // of |client| provided to Initialize().
  //
  // One application of this is offloading the GPU main thread. This helps
  // reduce latency and jitter by avoiding the wait.
  virtual bool TryToSetupEncodeOnSeparateThread(
      const base::WeakPtr<Client>& encode_client,
      const scoped_refptr<base::SingleThreadTaskRunner>& encode_task_runner);

 protected:
  // Do not delete directly; use Destroy() or own it with a scoped_ptr, which
  // will Destroy() it properly by default.
  virtual ~VideoEncodeAccelerator();
};

}  // namespace media

namespace std {

// Specialize std::default_delete so that
// std::unique_ptr<VideoEncodeAccelerator> uses "Destroy()" instead of trying to
// use the destructor.
template <>
struct MEDIA_EXPORT default_delete<media::VideoEncodeAccelerator> {
  void operator()(media::VideoEncodeAccelerator* vea) const;
};

}  // namespace std

#endif  // MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_H_
