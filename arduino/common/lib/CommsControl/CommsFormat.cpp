#include "CommsFormat.h"

// constructor to init variables
CommsFormat::CommsFormat(uint8_t infoSize, uint8_t address, uint16_t control) {
    memset(_data, 0, sizeof(_data));

    _info_size   = infoSize;
    _packet_size = infoSize + CONST_MIN_SIZE_PACKET ; // minimum size (start,address,control,fcs,stop)
    if (_packet_size > CONST_MAX_SIZE_PACKET) {
        return;
    }

    assignBytes(getAddress(), &address, 1, false);
    assignBytes(getControl(), reinterpret_cast<uint8_t*>(&control), 2, false);

    // hardcoded defaults
    *getStart()   = COMMS_FRAME_BOUNDARY; // fixed start flag
    *getStop()    = COMMS_FRAME_BOUNDARY; // fixed stop flag

    generateCrc();
}

void CommsFormat::assignBytes(uint8_t* target, uint8_t* source, uint8_t size, bool calcCrc) {
    memcpy(target, source, size);
    if (calcCrc) {
        generateCrc();
    }
}

void CommsFormat::setSequenceSend(uint8_t counter) {
    // sequence send valid only for info frames (not supervisory ACK/NACK)
    if ((*(getControl() + 1) & COMMS_CONTROL_SUPERVISORY) == 0) {
        counter = (counter << 1) & 0xFE;
        assignBytes(getControl() + 1, &counter, 1);
    }
}

uint8_t CommsFormat::getSequenceSend() {
    // sequence send valid only for info frames (not supervisory ACK/NACK)
    if ((*(getControl() + 1) & COMMS_CONTROL_SUPERVISORY) == 0) {
        return (*(getControl() + 1) >> 1) & 0x7F;
    } else {
        return 0xFF;
    }
}

void CommsFormat::setSequenceReceive(uint8_t counter) {
    counter = (counter << 1) & 0xFE;
    assignBytes(getControl()    , &counter, 1);
}

uint8_t CommsFormat::getSequenceReceive() {
    return (*(getControl()) >> 1) & 0x7F;
}

// compare calculated and received CRC value
bool CommsFormat::compareCrc() {
    // generate data crc
    generateCrc(false);

    // get crc from fcs
    uint16_t tmpFcs;
    assignBytes(reinterpret_cast<uint8_t*>(&tmpFcs), getFcs(), 2, false);

    // return comparison
    return tmpFcs == _crc;
}

// calculate CRC value
void CommsFormat::generateCrc(bool assign) {
    // calculate crc
    _crc = uCRC16Lib::calculate(reinterpret_cast<char*>(getAddress()), static_cast<uint16_t>(_info_size + 3));

    // assign crc to fcs
    if (assign) {
        assignBytes(getFcs(), reinterpret_cast<uint8_t*>(&_crc), 2, false);
    }
}

// assign received information to packet
void CommsFormat::setInformation(Payload *pl) {
    assignBytes(getInformation(), reinterpret_cast<uint8_t*>(pl->getInformation()), getInfoSize());
}

void CommsFormat::copyData(uint8_t* data, uint8_t dataSize) {
    _packet_size = dataSize;
    _info_size = dataSize - CONST_MIN_SIZE_PACKET;
    memset(getData(),    0, sizeof(_data));

    assignBytes(getData(), data, dataSize);
}


// STATIC METHODS
// TODO rewrite in a slightly better way using the enum
CommsFormat* CommsFormat::generateALARM(Payload *pl) {
    CommsFormat *tmpComms = new CommsFormat(pl->getSize(), PACKET_ALARM);
    tmpComms->setInformation(pl);
    return tmpComms;
}
CommsFormat* CommsFormat::generateCMD  (Payload *pl) {
    CommsFormat *tmpComms = new CommsFormat(pl->getSize(), PACKET_CMD  );
    tmpComms->setInformation(pl);
    return tmpComms;
}
CommsFormat* CommsFormat::generateDATA (Payload *pl) {
    CommsFormat *tmpComms = new CommsFormat(pl->getSize(), PACKET_DATA );
    tmpComms->setInformation(pl);
    return tmpComms;
}

