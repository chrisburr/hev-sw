#!/usr/bin/env python3

# Communication protocol based on HDLC format
# author Peter Svihra <peter.svihra@cern.ch>

import libscrc

# basic format based on HDLC
class commsFormat:
    def __init__(self, infoSize = 0, address = 0x00, control = [0x00, 0x00]):
        self.data_ = bytearray(7 + infoSize)
        self.infoSize_ = infoSize
        self.crc_ = None
        
        self.assignBytes(self.getStart()  , bytes([0x7E]))
        self.assignBytes(self.getAddress(), bytes([address]))
        self.assignBytes(self.getControl(), bytes(control))
        self.assignBytes(self.getStop()   , bytes([0x7E]))
        
    def getStart(self):
        return 0
    def getAddress(self):
        return 1
    def getControl(self):
        return 2
    def getInformation(self):
        return 4
    def getFcs(self):
        return 4 + self.infoSize_
    def getStop(self):
        return 4 + self.infoSize_ + 2
    
    def assignBytes(self, start, values, isCrc = False):
        for idx in range(len(values)):
            self.data_[start + idx] = values[idx]
        if not isCrc:
            self.generateCrc()
        
    # generate checksum
    def generateCrc(self, assign = True):
        self.crc_ = libscrc.x25(bytes(self.data_[self.getAddress():self.getFcs()])).to_bytes(2, byteorder='little')
        if assign:
            self.assignBytes(self.getFcs(), self.crc_, isCrc = True)
            
    def compareCrc(self):
        self.generateCrc(False)
        
        return (self.crc_ == self.fcs_)
    
    def setInformation(self, value, size = 2):
        # convert provided value
        self.assignBytes(self.getInformation(), value.to_bytes(size, byteorder='little'))
    
    def getData(self):
        return self.data_

    def setData(self, data):
        self.infoSize_ = len(data)
        self.data_     = data.to_bytes(self.infoSize_, byteorder='little')
        
# DATA specific formating
class commsDATA(commsFormat):
    def __init__(self):
        super().__init__(infoSize = 8, address = 0x40)

# CMD specific formating
class commsCMD(commsFormat):
    def __init__(self):
        super().__init__(infoSize = 8, address = 0x80)

# ALARM specific formating
class commsALARM(commsFormat):
    def __init__(self):
        super().__init__(infoSize = 4, address = 0xC0)

# ACK specific formating
class commsACK(commsFormat):
    def __init__(self, address):
        super().__init__(control = [0x00, 0x01], address = address)
        
# NACK specific formating
class commsNACK(commsFormat):
    def __init__(self, address):
        super().__init__(control = [0x00, 0x05], address = address)
