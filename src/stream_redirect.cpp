#include "stream_redirect.h"

std::vector<std::string> LogStreamBuffer::log_messages_;
std::mutex LogStreamBuffer::log_mutex_;

int LogStreamBuffer::sync() {
  std::string content = str();
  if (content.empty()) {
    return 0;
  }

  str("");

  size_t start = 0;
  while (true) {
    size_t end = content.find('\n', start);
    if (end == std::string::npos) {
      sputn(content.c_str() + start, content.length() - start);
      break;
    }
    std::string prefixed_line = prefix_ + content.substr(start, end - start);

    {
      std::lock_guard<std::mutex> lock(log_mutex_);
      log_messages_.push_back(prefixed_line);
      if (log_messages_.size() > 100) {
        log_messages_.erase(log_messages_.begin(),
                            log_messages_.begin() + log_messages_.size() - 90);
      }
    }

    if (log_file_.is_open()) {
      log_file_ << prefixed_line << std::endl;
    }
    start = end + 1;
  }
  return 0;
}

std::vector<std::string> LogStreamBuffer::get_messages(int count) {
  std::lock_guard<std::mutex> lock(log_mutex_);
  std::vector<std::string> result;
  int start_index = std::max(0, (int)log_messages_.size() - count);
  for (size_t i = start_index; i < log_messages_.size(); ++i) {
    result.push_back(log_messages_[i]);
  }
  return result;
}
