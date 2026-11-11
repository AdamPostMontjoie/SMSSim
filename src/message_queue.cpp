#include "message_queue.h"

MessageQueue::MessageQueue(int N) {
  // TODO: Allocate the buffer and initialize its size, count, and indices.
}

MessageQueue::~MessageQueue() {
  // TODO: Release the buffer.
}

void MessageQueue::append(Message data) {
  // TODO: Wait for space, add the message, and notify waiting consumers.
}

Message MessageQueue::remove() {
  // TODO: Wait for a message, remove it, and notify waiting producers.
}

bool MessageQueue::isEmpty() {
  // TODO: Check the message count while holding the lock.
}
