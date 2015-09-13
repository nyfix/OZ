/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Frank Quinn (http://fquinner.github.io)
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
#include <zmq.h>
#include <sys/time.h>
#include <stdint.h>
#include <unistd.h>

int main()
{
    char buffer [223];
    buffer[0] = 'A';
    buffer[1] = '\0';
    uint64_t* seq = (uint64_t*)(buffer + 2);
    uint32_t* s = (uint32_t*)(seq + 1);
    uint32_t* us = (uint32_t*)(s + 1);
    int i = 0;
    int j = 0;
    int hwm = 0;
    void* zmqSocketPublisher  = zmq_socket (zmq_ctx_new (), ZMQ_PUB);
    zmq_setsockopt (zmqSocketPublisher, ZMQ_SNDHWM, &hwm, sizeof(int));
    struct timeval tv;
    zmq_bind (zmqSocketPublisher, "tcp://*:5556");
    *seq = 0;
    while (1)
    {
        (*seq)++;
        gettimeofday(&tv, NULL);
        *s = (uint32_t)tv.tv_sec;
        *us = (uint32_t)tv.tv_usec;

        int i = zmq_send (zmqSocketPublisher, buffer, sizeof(buffer), 0);
        int e = errno;
        if (i == -1)
            printf ("ERROR FOUND: %d\n", e);
        //for (j = 0; j < 1000000; j++);
    }
}
