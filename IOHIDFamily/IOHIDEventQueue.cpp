/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <IOKit/system.h>
#include <IOKit/IOLib.h>
#include <IOKit/IODataQueueShared.h>
#include "IOHIDEventQueue.h"

enum {
    kHIDQueueStarted    = 0x01,
    kHIDQueueDisabled   = 0x02
};
    
#define super IOSharedDataQueue
OSDefineMetaClassAndStructors( IOHIDEventQueue, super )

//---------------------------------------------------------------------------
// Factory methods.

IOHIDEventQueue * IOHIDEventQueue::withCapacity( UInt32 size )
{
    IOHIDEventQueue * queue = new IOHIDEventQueue;
    
    if ( queue && !queue->initWithCapacity(size) )
    {
        queue->release();
        queue = 0;
        goto exit;
    }

    queue->_state               = 0;
    queue->_lock                = IOLockAlloc();
    queue->_numEntries          = size / DEFAULT_HID_ENTRY_SIZE;
    queue->_currentEntrySize    = DEFAULT_HID_ENTRY_SIZE;
    queue->_maxEntrySize        = DEFAULT_HID_ENTRY_SIZE;
   
exit: 
    return queue;
}

IOHIDEventQueue * IOHIDEventQueue::withEntries( UInt32 numEntries,
                                                UInt32 entrySize)
{
    IOHIDEventQueue * queue = NULL;
    UInt32 size = numEntries*entrySize;
    
    if ( numEntries > UINT32_MAX / entrySize )
        return NULL;

    if ( size < MIN_HID_QUEUE_CAPACITY )
        size = MIN_HID_QUEUE_CAPACITY;
    
    if ( size > MAX_HID_QUEUE_CAPACITY )
        size = MAX_HID_QUEUE_CAPACITY;
    
    queue = IOHIDEventQueue::withCapacity(size);
    if (queue) {
        queue->_numEntries = numEntries;
    }
    
    return queue;
}

void IOHIDEventQueue::free()
{
    if (_lock)
    {
        IOLockLock(_lock);
        IOLock*	 tempLock = _lock;
        _lock = NULL;
        IOLockUnlock(tempLock);
        IOLockFree(tempLock);
    }
    
    OSSafeReleaseNULL(_descriptor);
    OSSafeReleaseNULL(_elementSet);
    
    super::free();
}

Boolean IOHIDEventQueue::initWithEntries(UInt32 numEntries, UInt32 entrySize)
{
    UInt32 size = numEntries*entrySize;
    
    if ( numEntries > UINT32_MAX / entrySize )
        return false;

    if ( size < MIN_HID_QUEUE_CAPACITY )
        size = MIN_HID_QUEUE_CAPACITY;
    
    if ( size > MAX_HID_QUEUE_CAPACITY )
        size = MAX_HID_QUEUE_CAPACITY;
        
    return super::initWithCapacity(size);
}

//---------------------------------------------------------------------------
// Add data to the queue.

Boolean IOHIDEventQueue::enqueue( void * data, UInt32 dataSize )
{
    Boolean ret = true;
    
    if ( _lock )
        IOLockLock(_lock);

    // if we are not started, then dont enqueue
    // for now, return true, since we dont wish to push an error back
    if ((_state & (kHIDQueueStarted | kHIDQueueDisabled)) == kHIDQueueStarted) {
        ret = super::enqueue(data, dataSize);
        if (!ret) {
            _enqueueErrorCount++;
            //Send notification for queue full
            sendDataAvailableNotification();
        }
    }

    if ( _lock )
        IOLockUnlock(_lock);

    return ret;
}


//---------------------------------------------------------------------------
// Start the queue.

void IOHIDEventQueue::start() 
{
    if ( _lock )
        IOLockLock(_lock);

    if ( _state & kHIDQueueStarted )
        goto START_END;

    if ( _currentEntrySize != _maxEntrySize )
    {
        mach_port_t port = notifyMsg ? ((mach_msg_header_t *)notifyMsg)->msgh_remote_port : MACH_PORT_NULL;
        
        // Free the existing queue data
        if (dataQueue) {
            IOFreeAligned(dataQueue, round_page(getQueueSize() + DATA_QUEUE_MEMORY_HEADER_SIZE + DATA_QUEUE_MEMORY_APPENDIX_SIZE));
            dataQueue = NULL;
            if (notifyMsg) {
                IOFree(notifyMsg, sizeof(mach_msg_header_t));
                notifyMsg = NULL;
            }
        }
        if (_reserved) {
            IOFree(_reserved, sizeof(struct ExpansionData));
            _reserved = NULL;
        }
        
        OSSafeReleaseNULL(_descriptor);
        
        // init the queue again.  This will allocate the appropriate data.
        if ( !initWithEntries(_numEntries, _maxEntrySize) ) {
            goto START_END;
        }
        
        _currentEntrySize = _maxEntrySize;
        
        // RY: since we are initing the queue, we should reset the port as well
        if ( port ) 
            setNotificationPort(port);
    }
    else if ( dataQueue )
    {
        dataQueue->head = 0;
        dataQueue->tail = 0;
    }

    _state |= kHIDQueueStarted;

START_END:
    if ( _lock )
        IOLockUnlock(_lock);

}

void IOHIDEventQueue::stop()
{
    if ( _lock )
        IOLockLock(_lock);

    _state &= ~kHIDQueueStarted;

    if ( _lock )
        IOLockUnlock(_lock);
}

void IOHIDEventQueue::enable() 
{
    if ( _lock )
        IOLockLock(_lock);

    _state &= ~kHIDQueueDisabled;

    if ( _lock )
        IOLockUnlock(_lock);
}

void IOHIDEventQueue::disable()
{
    if ( _lock )
        IOLockLock(_lock);

    _state |= kHIDQueueDisabled;

    if ( _lock )
        IOLockUnlock(_lock);
}

Boolean IOHIDEventQueue::isStarted()
{
    bool ret;
    
    if ( _lock )
        IOLockLock(_lock);

    ret = (_state & kHIDQueueStarted) != 0;

    if ( _lock )
        IOLockUnlock(_lock);
        
    return ret;
}

void IOHIDEventQueue::setOptions(IOHIDQueueOptionsType flags) 
{
    if ( _lock )
        IOLockLock(_lock);

	_options = flags;

    if ( _lock )
        IOLockUnlock(_lock);
}

IOHIDQueueOptionsType IOHIDEventQueue::getOptions() 
{ 
	return _options;
}

//---------------------------------------------------------------------------
// Add element to the queue.

void IOHIDEventQueue::addElement( IOHIDElementPrivate * element )
{
    UInt32 elementSize;
    
    if ( !element )
        return;
        
    if ( !_elementSet )
    {
        _elementSet = OSSet::withCapacity(4);
    }
    
    if ( _elementSet->containsObject( element ) )
        return;
        
    elementSize = element->getElementValueSize() + sizeof(void *);
    
    if ( _maxEntrySize < elementSize )
        _maxEntrySize = elementSize;
}

//---------------------------------------------------------------------------
// Remove element from the queue.

void IOHIDEventQueue::removeElement( IOHIDElementPrivate * element )
{
    OSCollectionIterator *      iterator;
    IOHIDElementPrivate *       temp;
    UInt32                      size        = 0;
    UInt32                      maxSize     = DEFAULT_HID_ENTRY_SIZE;
    
    if ( !element || !_elementSet || !_elementSet->containsObject( element ))
        return;
        
    _elementSet->removeObject( element );
    
    if ( NULL != (iterator = OSCollectionIterator::withCollection(_elementSet)) )
    {
        while ( NULL != (temp = (IOHIDElementPrivate *)iterator->getNextObject()) )
        {
            size = temp->getElementValueSize() + sizeof(void *);
            
            if ( maxSize < size )
                maxSize = size;   
        }
    
        iterator->release();
    }
        
    _maxEntrySize = maxSize;
}

//---------------------------------------------------------------------------
// get entry size from the queue.

UInt32 IOHIDEventQueue::getEntrySize( )
{
    return _maxEntrySize;
}


//---------------------------------------------------------------------------
// get a mem descriptor.  replacing default behavior

IOMemoryDescriptor * IOHIDEventQueue::getMemoryDescriptor()
{
    if (!_descriptor)
        _descriptor = super::getMemoryDescriptor();

    return _descriptor;
}

//---------------------------------------------------------------------------
//
bool IOHIDEventQueue::serialize(OSSerialize * serializer) const
{
    bool ret = false;
    
    if (serializer->previouslySerialized(this)) {
        return true;
    }
    
    OSDictionary *dict = OSDictionary::withCapacity(2);
    if (dict) {
        OSNumber *num = OSNumber::withNumber(dataQueue->head, 32);
        if (num) {
            dict->setObject("head", num);
            num->release();
        }
        num = OSNumber::withNumber(dataQueue->tail, 32);
        if (num) {
            dict->setObject("tail", num);
            num->release();
        }
        num = OSNumber::withNumber(_enqueueErrorCount, 64);
        if (num) {
            dict->setObject("EnqueueErrorCount", num);
            num->release();
        }
        num = OSNumber::withNumber(_reserved->queueSize, 64);
        if (num) {
            dict->setObject("QueueSize", num);
            num->release();
        }
        ret = dict->serialize(serializer);
        dict->release();
    }
    
    return ret;
}

OSMetaClassDefineReservedUnused(IOHIDEventQueue,  0);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  1);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  2);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  3);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  4);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  5);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  6);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  7);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  8);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  9);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 10);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 11);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 12);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 13);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 14);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 15);
