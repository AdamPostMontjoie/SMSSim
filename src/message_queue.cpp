#include "message_queue.h"

MessageQueue::MessageQueue(int N) {
  buffer = new Message[N];
  buffer_cnt = 0;
  buffer_size = N;
  buffer_first = 0;
  buffer_last = 0;
}

MessageQueue::~MessageQueue() {
  delete[] buffer;
}

void MessageQueue::append(Message data) {
  unique_lock<mutex> lock(buffer_lock);
  buffer_not_full.wait(lock, [this] { return buffer_cnt < buffer_size; });

  buffer[buffer_last] = data;
  buffer_last = (buffer_last + 1) % buffer_size;
  buffer_cnt++;

  buffer_not_empty.notify_one();
}

Message MessageQueue::remove() {
  unique_lock<mutex> lock(buffer_lock);
  buffer_not_empty.wait(lock, [this] { return buffer_cnt > 0; });

  Message data = buffer[buffer_first];
  buffer_first = (buffer_first + 1) % buffer_size;
  buffer_cnt--;

  buffer_not_full.notify_one();
  return data;
}

bool MessageQueue::isEmpty() {
  lock_guard<mutex> lock(buffer_lock);
  return buffer_cnt == 0;
}
