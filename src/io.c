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

/*=========================================================================
  =                             Includes                                  =
  =========================================================================*/

// MAMA includes
#include <mama/mama.h>
#include <mama/io.h>
#include <wombat/port.h>
#include <event.h>

// local includes
#include "zmqbridgefunctions.h"

mama_status zmqBridgeMamaIo_create(ioBridge*   result,
                       void*       nativeQueueHandle,
                       uint32_t    descriptor,
                       mamaIoCb    action,
                       mamaIoType  ioType,
                       mamaIo      parent,
                       void*       closure)
{
  return MAMA_STATUS_NOT_IMPLEMENTED;
}

/* Not implemented in the zmq bridge */
mama_status zmqBridgeMamaIo_destroy(ioBridge io)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

/* Not implemented in the zmq bridge */
mama_status zmqBridgeMamaIo_getDescriptor(ioBridge io, uint32_t*   result)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}
