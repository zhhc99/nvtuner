#pragma once
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

// to buffer log messages to a static shared vector
class LogStreamBuffer : public std::stringbuf {
 public:
  explicit LogStreamBuffer(std::string prefix, std::ofstream& log_file)
      : prefix_(std::move(prefix)), log_file_(log_file) {}
  ~LogStreamBuffer() override { pubsync(); }

  static std::vector<std::string> get_messages(int count);

 protected:
  int sync() override;

 private:
  std::string prefix_;
  std::ofstream& log_file_;

  static std::vector<std::string> log_messages_;
  static std::mutex log_mutex_;
};

// to redirect stream to a LogStreamBuffer
class StreamRedirector {
 public:
  StreamRedirector(std::ostream& stream, std::streambuf* new_buf)
      : stream_(stream), old_buf_(stream.rdbuf(new_buf)) {}

  ~StreamRedirector() { stream_.rdbuf(old_buf_); }

  StreamRedirector(const StreamRedirector&) = delete;
  StreamRedirector& operator=(const StreamRedirector&) = delete;

 private:
  std::ostream& stream_;
  std::streambuf* old_buf_;
};
