#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include "message.h"
#include <condition_variable>
#include <thread>
#include <mutex>

struct Message;
using namespace std;

class MessageQueue {
 public:
  MessageQueue(int N);  // constructor to initialize locks and conditional variables
  ~MessageQueue();  // destructor

  void append(Message data);
  Message remove();
  bool isEmpty();

 private:
  Message *buffer;
  int buffer_size;
  int buffer_cnt;
  int buffer_first;
  int buffer_last;

  mutex buffer_lock;      // lock
  condition_variable buffer_not_full;   // Condition indicating buffer is not full
  condition_variable buffer_not_empty;  // Condition indicating buffer is not empty
};

#endif
