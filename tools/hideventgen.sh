SELF_DIR=$(cd "$(dirname "$0")" ; pwd -P)
TARGET_DIR=${SELF_DIR}/../IOHIDFamily
#EVENT_DATA_FILE=/tmp/hideventdata.json
EVENT_DATA_FILE=${SELF_DIR}/hideventdata.plist

#plutil -convert json -r -o /tmp/hideventdata.json ${SELF_DIR}/hideventdata.plist

HEADER=$(cat <<'ENDSCRIPT'
/*
*
* @APPLE_LICENSE_HEADER_START@
*
* Copyright (c) 2017 Apple Computer, Inc.  All Rights Reserved.
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

ENDSCRIPT
)

function gen_file {
    FILE=${TARGET_DIR}/${2}
    echo "${HEADER}" > $FILE
    printf "\n//\n"                                                                 >> $FILE
    printf "// DO NOT EDIT THIS FILE. IT IS AUTO-GENERATED\n"                       >> $FILE
    printf "//\n\n"                                                                 >> $FILE
    echo "${2}" | awk '{ x=gsub(/\./,"_",$1); print "#ifndef _" toupper($x)}'       >> $FILE
    echo "${2}" | awk '{ x=gsub(/\./,"_",$1); print "#define _" toupper($x) "\n"}'  >> $FILE
    python ${SELF_DIR}/hideventdata.py  -t ${1} -f ${EVENT_DATA_FILE}               >> $FILE
    echo "#endif"                                                                   >> $FILE
}

gen_file struct  IOHIDEventStructDefs.h
gen_file macro   IOHIDEventMacroDefs.h
gen_file fields  IOHIDEventFieldDefs.h

cat > ${SELF_DIR}/hidutil/HIDEvent.h <<EOM
//
//  HIDEvent.h
//  hidutil-internal
//

#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDEvent.h>

@interface HIDEvent : NSObject {
IOHIDEventRef eventRef;
}

@property               NSNumber *timestamp;
@property               NSNumber *sender;
@property (readonly)    NSNumber *typeval;
@property (readonly)    NSNumber *latency;
@property               NSNumber *flags;
@property (readonly)    NSString *typestr;

- (id)initWithEvent:(IOHIDEventRef)event;

@end

HIDEvent *createHIDEvent(IOHIDEventRef event);

EOM

cat > ${SELF_DIR}/hidutil/HIDEvent.m <<EOM
//
//  HIDEvent.m
//  hidutil-internal
//

#import "HIDEvent.h"

@implementation HIDEvent

- (id)initWithEvent:(IOHIDEventRef)event {
    self = [super init];
    
    if (self) {
        self->eventRef = event;
        CFRetain(self->eventRef);
    }
    
    return self;
}

- (void)dealloc {
    if (self->eventRef) {
        CFRelease(self->eventRef);
    }
}

-(void)setTimestamp:(NSNumber *)timestamp {
    IOHIDEventSetTimeStamp(self->eventRef, timestamp.unsignedLongLongValue);
}

- (NSNumber *)timestamp {
    return [NSNumber numberWithUnsignedLongLong:IOHIDEventGetTimeStamp(self->eventRef)];
}

- (void)setSender:(NSNumber *)sender {
    IOHIDEventSetSenderID(self->eventRef, sender.unsignedLongLongValue);
}

- (NSNumber *)sender {
    return [NSNumber numberWithUnsignedLongLong:IOHIDEventGetSenderID(self->eventRef)];
}

- (NSNumber *)typeval {
    return [NSNumber numberWithInt:IOHIDEventGetType(self->eventRef)];
}

- (NSNumber *)latency {
    return [NSNumber numberWithUnsignedLongLong:IOHIDEventGetLatency(self->eventRef, kMicrosecondScale)];
}

- (void)setFlags:(NSNumber *)flags {
    IOHIDEventSetEventFlags(self->eventRef, flags.unsignedIntValue);
}

- (NSNumber *)flags {
    return [NSNumber numberWithInt:IOHIDEventGetEventFlags(self->eventRef)];
}

- (NSString *)typestr {
    return [[NSString stringWithUTF8String:IOHIDEventGetTypeString(IOHIDEventGetType(self->eventRef))] lowercaseString];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"timestamp:%llu sender:0x%llx typeval:%d typestr:%@ latency:%llu flags:0x%08x", self.timestamp.unsignedLongLongValue, self.sender.unsignedLongLongValue, self.typeval.unsignedIntValue, self.typestr, self.latency.unsignedLongLongValue, self.flags.unsignedIntValue];
}

@end


HIDEvent *createHIDEvent(IOHIDEventRef event)
{
    HIDEvent *ev = NULL;
    
    if (!event) {
        return ev;
    }
    
    switch (IOHIDEventGetType(event)) {
        case kIOHIDEventTypeNULL:
            ev = [[HIDNULLEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeVendorDefined:
            ev = [[HIDVendorDefinedEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeButton:
            ev = [[HIDButtonEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeKeyboard:
            ev = [[HIDKeyboardEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeTranslation:
            ev = [[HIDTranslationEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeRotation:
            ev = [[HIDRotationEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeScroll:
            ev = [[HIDScrollEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeScale:
            ev = [[HIDScaleEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeVelocity:
            ev = [[HIDVelocityEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeOrientation:
            ev = [[HIDOrientationEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeDigitizer:
            ev = [[HIDDigitizerEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeAmbientLightSensor:
            ev = [[HIDAmbientLightSensorEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeAccelerometer:
            ev = [[HIDAccelerometerEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeProximity:
            ev = [[HIDProximityEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeTemperature:
            ev = [[HIDTemperatureEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeNavigationSwipe:
            ev = [[HIDNavigationSwipeEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypePointer:
            ev = [[HIDPointerEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeProgress:
            ev = [[HIDProgressEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeMultiAxisPointer:
            ev = [[HIDMultiAxisPointerEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeGyro:
            ev = [[HIDGyroEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeCompass:
            ev = [[HIDCompassEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeDockSwipe:
            ev = [[HIDDockSwipeEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeSymbolicHotKey:
            ev = [[HIDSymbolicHotKeyEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypePower:
            ev = [[HIDPowerEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeLED:
            ev = [[HIDLEDEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeFluidTouchGesture:
            ev = [[HIDFluidTouchGestureEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeBoundaryScroll:
            ev = [[HIDBoundaryScrollEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeBiometric:
            ev = [[HIDBiometricEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeUnicode:
            ev = [[HIDUnicodeEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeAtmosphericPressure:
            ev = [[HIDAtmosphericPressureEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeForce:
            ev = [[HIDForceEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeMotionActivity:
            ev = [[HIDMotionActivityEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeMotionGesture:
            ev = [[HIDMotionGestureEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeGameController:
            ev = [[HIDGameControllerEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeHumidity:
            ev = [[HIDHumidityEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeBrightness:
            ev = [[HIDBrightnessEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeGenericGesture:
            ev = [[HIDGenericGestureEvent alloc] initWithEvent:event];
            break;
        default:
            ev = [[HIDEvent alloc] initWithEvent:event];
    }
    
    return ev;
}

EOM



python ${SELF_DIR}/hideventdata.py  -t objects       -f ${EVENT_DATA_FILE}  >> ${SELF_DIR}/hidutil/HIDEvent.m
python ${SELF_DIR}/hideventdata.py  -t objectHeaders -f ${EVENT_DATA_FILE}  >> ${SELF_DIR}/hidutil/HIDEvent.h

