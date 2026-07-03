#include <gtest/gtest.h>

#include <cstring>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include "model/platform_handles.h"
#endif

#ifdef Q_OS_LINUX
#include "model/platform_handles.h"
#endif

#ifdef Q_OS_WIN

TEST(Win32HandleTest, DefaultConstructIsInvalid) {
  Win32Handle handle;
  EXPECT_FALSE(handle.IsValid());
  EXPECT_FALSE(handle);
}

TEST(Win32HandleTest, MoveConstructionTransfersOwnership) {
  HANDLE raw = reinterpret_cast<HANDLE>(0x4);
  Win32Handle src(raw);
  Win32Handle dst = std::move(src);

  EXPECT_FALSE(src.IsValid());
  EXPECT_TRUE(dst.IsValid());
  EXPECT_EQ(dst.Get(), raw);
  dst.Close();
}

TEST(Win32HandleTest, MoveAssignmentTransfersOwnership) {
  HANDLE raw = reinterpret_cast<HANDLE>(0x8);
  Win32Handle src(raw);
  Win32Handle dst;
  dst = std::move(src);

  EXPECT_FALSE(src.IsValid());
  EXPECT_TRUE(dst.IsValid());
  EXPECT_EQ(dst.Get(), raw);
  dst.Close();
}

TEST(Win32HandleTest, CloseInvalidatesHandle) {
  HANDLE raw = reinterpret_cast<HANDLE>(0xC);
  Win32Handle handle(raw);
  EXPECT_TRUE(handle.IsValid());
  handle.Close();
  EXPECT_FALSE(handle.IsValid());
}

TEST(Win32HandleTest, SelfMoveIsSafe) {
  HANDLE raw = reinterpret_cast<HANDLE>(0x10);
  Win32Handle handle(raw);

  handle = std::move(handle);

  EXPECT_TRUE(handle.IsValid());
  handle.Close();
}

TEST(Win32FindHandleTest, MoveSemanticsWork) {
  HANDLE raw = reinterpret_cast<HANDLE>(0x14);
  Win32FindHandle src(raw);
  Win32FindHandle dst = std::move(src);

  EXPECT_FALSE(src.IsValid());
  EXPECT_TRUE(dst.IsValid());
  dst.Close();
}

TEST(Win32HeapBufferTest, AllocAndFree) {
  Win32HeapBuffer buffer(1024);
  EXPECT_TRUE(buffer);
  EXPECT_NE(buffer.Get(), nullptr);
  EXPECT_EQ(buffer.Size(), 1024u);

  buffer.Free();
  EXPECT_FALSE(buffer);
  EXPECT_EQ(buffer.Get(), nullptr);
}

TEST(Win32HeapBufferTest, MoveTransfer) {
  Win32HeapBuffer src(512);
  void* ptr = src.Get();

  Win32HeapBuffer dst = std::move(src);

  EXPECT_FALSE(src);
  EXPECT_TRUE(dst);
  EXPECT_EQ(dst.Get(), ptr);
}

#endif  // Q_OS_WIN

#ifdef Q_OS_LINUX

TEST(InotifyFdTest, DefaultConstructIsInvalid) {
  InotifyFd fd;
  EXPECT_FALSE(fd.IsValid());
  EXPECT_FALSE(fd);
}

TEST(InotifyFdTest, MoveConstructionTransfersOwnership) {
  InotifyFd src(42);
  InotifyFd dst = std::move(src);

  EXPECT_FALSE(src.IsValid());
  EXPECT_TRUE(dst.IsValid());
  EXPECT_EQ(dst.Get(), 42);
  dst.Close();
}

TEST(InotifyFdTest, MoveAssignmentTransfersOwnership) {
  InotifyFd src(100);
  InotifyFd dst;
  dst = std::move(src);

  EXPECT_FALSE(src.IsValid());
  EXPECT_TRUE(dst.IsValid());
  EXPECT_EQ(dst.Get(), 100);
  dst.Close();
}

TEST(InotifyFdTest, CloseInvalidates) {
  InotifyFd fd(7);
  EXPECT_TRUE(fd.IsValid());
  fd.Close();
  EXPECT_FALSE(fd.IsValid());
}

TEST(InotifyFdTest, SelfMoveIsSafe) {
  InotifyFd fd(5);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
  fd = std::move(fd);
#pragma GCC diagnostic pop
  EXPECT_TRUE(fd.IsValid());
  fd.Close();
}

TEST(InotifyWatchTest, DefaultConstructIsInvalid) {
  InotifyWatch watch;
  EXPECT_FALSE(watch.IsValid());
}

TEST(InotifyWatchTest, MoveConstructionTransfersOwnership) {
  InotifyWatch src(3, 15);
  InotifyWatch dst = std::move(src);

  EXPECT_FALSE(src.IsValid());
  EXPECT_TRUE(dst.IsValid());
  EXPECT_EQ(dst.Get(), 15);
  dst.Remove();
}

TEST(InotifyWatchTest, MoveAssignmentTransfersOwnership) {
  InotifyWatch src(3, 20);
  InotifyWatch dst;
  dst = std::move(src);

  EXPECT_FALSE(src.IsValid());
  EXPECT_TRUE(dst.IsValid());
  EXPECT_EQ(dst.Get(), 20);
  dst.Remove();
}

TEST(InotifyWatchTest, SelfMoveIsSafe) {
  InotifyWatch watch(4, 30);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
  watch = std::move(watch);
#pragma GCC diagnostic pop
  EXPECT_TRUE(watch.IsValid());
  watch.Remove();
}

TEST(DirHandleTest, DefaultConstructIsInvalid) {
  DirHandle dir;
  EXPECT_FALSE(dir.IsValid());
}

TEST(DirHandleTest, MoveConstructionTransfersOwnership) {
  DIR* raw = opendir("/tmp");
  if (!raw) {
    GTEST_SKIP() << "Cannot open /tmp for testing";
    return;
  }
  DirHandle src(raw);
  DirHandle dst = std::move(src);

  EXPECT_FALSE(src.IsValid());
  EXPECT_TRUE(dst.IsValid());
}

TEST(DirHandleTest, MoveAssignmentInvalidatesSource) {
  DIR* raw = opendir("/tmp");
  if (!raw) {
    GTEST_SKIP() << "Cannot open /tmp for testing";
    return;
  }
  DirHandle src(raw);
  DirHandle dst;
  dst = std::move(src);

  EXPECT_FALSE(src.IsValid());
  EXPECT_TRUE(dst.IsValid());
}

TEST(InotifyEventParsing, SynthesizedEventBuffer) {
  struct inotify_event event{};
  event.wd = 1;
  event.mask = IN_CREATE | IN_CLOSE_WRITE;
  event.cookie = 0;
  event.len = static_cast<uint32_t>(sizeof(struct inotify_event));

  char buffer[sizeof(struct inotify_event) + 16]{};
  std::memcpy(buffer, &event, sizeof(struct inotify_event));

  const char name[] = "test.txt";
  std::memcpy(buffer + sizeof(struct inotify_event), name, std::size(name));

  auto* parsed = reinterpret_cast<const struct inotify_event*>(buffer);
  EXPECT_EQ(parsed->wd, 1);
  EXPECT_TRUE(parsed->mask & IN_CREATE);
  EXPECT_TRUE(parsed->mask & IN_CLOSE_WRITE);
  EXPECT_STREQ(parsed->name, "test.txt");
}

#endif  // Q_OS_LINUX

TEST(PlatformGuardCompileCheck, Win32GuardCompilesOnAllPlatforms) {
#ifdef Q_OS_WIN
  EXPECT_TRUE(true);
#else
  EXPECT_TRUE(true);
#endif
}

TEST(PlatformGuardCompileCheck, LinuxGuardCompilesOnAllPlatforms) {
#ifdef Q_OS_LINUX
  EXPECT_TRUE(true);
#else
  EXPECT_TRUE(true);
#endif
}
