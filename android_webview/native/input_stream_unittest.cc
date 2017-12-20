// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "android_webview/native/input_stream_impl.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "jni/InputStreamUnittest_jni.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using android_webview::InputStream;
using android_webview::InputStreamImpl;
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using net::IOBuffer;
using testing::DoAll;
using testing::Ge;
using testing::InSequence;
using testing::Lt;
using testing::Ne;
using testing::NotNull;
using testing::Return;
using testing::SetArgPointee;
using testing::Test;
using testing::_;

class InputStreamTest : public Test {
 public:
  InputStreamTest() {
  }
 protected:
  void SetUp() override {
    env_ = AttachCurrentThread();
    ASSERT_THAT(env_, NotNull());
  }

  scoped_refptr<IOBuffer> DoReadCountedStreamTest(int stream_size,
                                                  int bytes_requested,
                                                  int* bytes_read) {
    ScopedJavaLocalRef<jobject> counting_jstream =
        Java_InputStreamUnittest_getCountingStream(env_, stream_size);
    EXPECT_FALSE(counting_jstream.is_null());

    std::unique_ptr<InputStream> input_stream(
        new InputStreamImpl(counting_jstream));
    scoped_refptr<IOBuffer> buffer = new IOBuffer(bytes_requested);

    EXPECT_TRUE(input_stream->Read(buffer.get(), bytes_requested, bytes_read));
    return buffer;
  }

  JNIEnv* env_;
};

TEST_F(InputStreamTest, ReadEmptyStream) {
  ScopedJavaLocalRef<jobject> empty_jstream =
      Java_InputStreamUnittest_getEmptyStream(env_);
  EXPECT_FALSE(empty_jstream.is_null());

  std::unique_ptr<InputStream> input_stream(new InputStreamImpl(empty_jstream));
  const int bytes_requested = 10;
  int bytes_read = 0;
  scoped_refptr<IOBuffer> buffer = new IOBuffer(bytes_requested);

  EXPECT_TRUE(input_stream->Read(buffer.get(), bytes_requested, &bytes_read));
  EXPECT_EQ(0, bytes_read);
}

TEST_F(InputStreamTest, ReadStreamPartial) {
  const int bytes_requested = 128;
  int bytes_read = 0;
  DoReadCountedStreamTest(bytes_requested * 2, bytes_requested, &bytes_read);
  EXPECT_EQ(bytes_requested, bytes_read);
}

TEST_F(InputStreamTest, ReadStreamCompletely) {
  const int bytes_requested = 42;
  int bytes_read = 0;
  DoReadCountedStreamTest(bytes_requested, bytes_requested, &bytes_read);
  EXPECT_EQ(bytes_requested, bytes_read);
}

TEST_F(InputStreamTest, TryReadMoreThanBuffer) {
  const int buffer_size = 3 * InputStreamImpl::kBufferSize;
  int bytes_read = 0;
  DoReadCountedStreamTest(buffer_size, buffer_size * 2, &bytes_read);
  EXPECT_EQ(buffer_size, bytes_read);
}

TEST_F(InputStreamTest, CheckContentsReadCorrectly) {
  const int bytes_requested = 256;
  int bytes_read = 0;
  scoped_refptr<IOBuffer> buffer =
      DoReadCountedStreamTest(bytes_requested, bytes_requested, &bytes_read);
  EXPECT_EQ(bytes_requested, bytes_read);
  for (int i = 0; i < bytes_requested; ++i) {
    EXPECT_EQ(i, (unsigned char)buffer->data()[i]);
  }
}

TEST_F(InputStreamTest, ReadLargeStreamPartial) {
  const int bytes_requested = 3 * InputStreamImpl::kBufferSize;
  int bytes_read = 0;
  DoReadCountedStreamTest(bytes_requested + 32, bytes_requested, &bytes_read);
  EXPECT_EQ(bytes_requested, bytes_read);
}

TEST_F(InputStreamTest, ReadLargeStreamCompletely) {
  const int bytes_requested = 3 * InputStreamImpl::kBufferSize;
  int bytes_read = 0;
  DoReadCountedStreamTest(bytes_requested, bytes_requested, &bytes_read);
  EXPECT_EQ(bytes_requested, bytes_read);
}

TEST_F(InputStreamTest, DoesNotCrashWhenExceptionThrown) {
  ScopedJavaLocalRef<jobject> throw_jstream =
      Java_InputStreamUnittest_getThrowingStream(env_);
  EXPECT_FALSE(throw_jstream.is_null());

  std::unique_ptr<InputStream> input_stream(new InputStreamImpl(throw_jstream));

  int64_t bytes_skipped;
  EXPECT_FALSE(input_stream->Skip(10, &bytes_skipped));

  int bytes_available;
  EXPECT_FALSE(input_stream->BytesAvailable(&bytes_available));


  const int bytes_requested = 10;
  int bytes_read = 0;
  scoped_refptr<IOBuffer> buffer = new IOBuffer(bytes_requested);
  EXPECT_FALSE(input_stream->Read(buffer.get(), bytes_requested, &bytes_read));
  EXPECT_EQ(0, bytes_read);

  // This closes the stream.
  input_stream.reset(NULL);
}
