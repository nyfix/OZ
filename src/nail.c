/*
 * The MIT License (MIT)
 *
 * Original work Copyright (c) 2015 Frank Quinn (http://fquinner.github.io)
 * Modified work Copyright (c) 2020 Bill Torpey (http://btorpey.github.io)
 * and assigned to NYFIX, a division of Itiviti Group AB
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <zmq.h>

int main()
{
   char buffer [223];
   void* zmqSocketSubscriber  = zmq_socket(zmq_ctx_new(), ZMQ_SUB);
   zmq_connect(zmqSocketSubscriber, "tcp://localhost:5556");
   uint64_t* seq = 0;
   uint32_t* s = 0;
   uint32_t* us = 0;
   uint32_t s_last = 0;
   uint64_t msg_count = 0;
   int hwm = 0;
   struct timeval tv;
   zmq_setsockopt(zmqSocketSubscriber, ZMQ_RCVHWM, &hwm, sizeof(int));

   zmq_setsockopt(zmqSocketSubscriber, ZMQ_SUBSCRIBE, "A", 1);
   uint64_t expectedSeq = 1;
   while (1) {
      int size = zmq_recv(zmqSocketSubscriber, buffer, sizeof(buffer), 0);
      int e = errno;
      if (size == -1) {
         printf("ERROR FOUND: %d\n", e);
      }
      uint8_t* copy = calloc(1, size);
      memcpy(copy, buffer, size);
      memcmp(copy, buffer, size);
      msg_count++;
      seq = (uint64_t*)(buffer + 2);
      s = (uint32_t*)(seq + 1);
      us = (uint32_t*)(s + 1);
      gettimeofday(&tv, NULL);

      if ((uint32_t)tv.tv_sec != s_last) {
         printf("Seconds = %lu; msgs = %lu;\n", *s, msg_count);
         s_last = (uint32_t)tv.tv_sec;
         msg_count = 0;
      }
      if (*seq != expectedSeq) {
         printf("Gap from %lu to %lu observed\n", expectedSeq, *seq);
         expectedSeq = *seq;
      }
      expectedSeq++;
   }
}
