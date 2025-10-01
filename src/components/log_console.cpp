#include "log_console.h"

using namespace ftxui;

Component LogConsole() {
  const int msg_count = 6;
  return Renderer([msg_count] {
    Elements lines;
    auto log_messages = LogStreamBuffer::get_messages(msg_count);
    int start_index = std::max(0, (int)log_messages.size() - msg_count);
    for (int i = start_index; i < log_messages.size(); ++i) {
      lines.push_back(text(log_messages[i]));
    }
    return window(text("Log Console"),
                  vbox(lines) | size(HEIGHT, GREATER_THAN, 4) |
                      size(HEIGHT, LESS_THAN, msg_count + 2));
  });
}
