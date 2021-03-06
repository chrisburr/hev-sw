#!/usr/bin/env python3

# Communication protocol between rasp and arduino based on HDLC format
# author Peter Svihra <peter.svihra@cern.ch>

import serial
from serial.tools import list_ports

import threading
import time

import commsFormat
import commsConstants
from collections import deque

import binascii
import logging
logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')


# communication class that governs talking between devices
class commsControl():
    def __init__(self, port, baudrate = 115200, queueSizeReceive = 16, queueSizeSend = 16):
        
        self._serial = None
        self.openSerial(port, baudrate)

        # send queues are FIFO ring-buffers of the defined size
        self._alarms   = deque(maxlen = queueSizeSend)
        self._commands = deque(maxlen = queueSizeSend)
        self._data     = deque(maxlen = queueSizeSend)
        
        # received queue and observers to be notified on update
        self._payloadrecv = deque(maxlen = queueSizeReceive)
        self._observers = []
        
        # needed to find packet frames
        self._received = []
        self._foundStart = False
        self._timeLastTransmission = int(round(time.time() * 1000))
        
        # packet counter checker
        self._sequenceSend    = 0
        self._sequenceReceive = 0
        
        # initialize of the multithreading
        self._lockSerial = threading.Lock()
        self._receiving = True
        threading.Thread(target=self.receiver, daemon=True).start()
        
        self._sending   = True
        self._datavalid = threading.Event()  # callback for send process
        self._dvlock    = threading.Lock()      # make callback threadsafe
        threading.Thread(target=self.sender, daemon=True).start()
    
    # open serial port
    def openSerial(self, port, baudrate = 115200, timeout = 2):
        if port is not None:
            self._serial = serial.Serial(port = port, baudrate=baudrate, timeout = timeout, dsrdtr = True)
        else:
            try:
                self._serial.close()
            except:
                logging.warning("Serial device not open")
            self._serial = None
        
    def sender(self):
        while self._sending:
            self._datavalid.wait()
            if self._serial is not None:
                if not self._serial.in_waiting > 0:
                    self.sendQueue(self._alarms  ,  10)
                    self.sendQueue(self._commands,  50)
                    self.sendQueue(self._data    , 200)
            if self.finishedSending():
                self._datavalid.clear()
                    
    def finishedSending(self):
        with self._dvlock:
            try:
                if (len(self._alarms) + len(self._data) + len(self._commands)) > 0:
                    return False
                else:
                    return True
            except:
                return True

    def receiver(self):
        while self._receiving:
            if self._serial is not None:
                if self._serial.in_waiting > 0:
                    with self._lockSerial:
                        logging.debug("Receiving data...")
                        data = self._serial.read(self._serial.in_waiting)
                        self.processPacket(data)

    def sendQueue(self, queue, timeout):
        with self._dvlock:
            if len(queue) > 0:
                logging.debug(f'Queue length: {len(queue)}')
                currentTime = int(round(time.time() * 1000))
                if currentTime > (self._timeLastTransmission + timeout):
                    with self._lockSerial:
                        self._timeLastTransmission = currentTime
                        queue[0].setSequenceSend(self._sequenceSend)
                        self.sendPacket(queue[0])
                    
    def getQueue(self, payloadType):
        if   payloadType == commsConstants.PAYLOAD_TYPE.ALARM:
            return self._alarms
        elif payloadType == commsConstants.PAYLOAD_TYPE.CMD:
            return self._commands
        elif payloadType == commsConstants.PAYLOAD_TYPE.DATA:
            return self._data
        else:
            return None
    
    def getInfoType(self, address):
        address &= 0xC0
        if address == 0xC0:
            return commsConstants.PAYLOAD_TYPE.ALARM
        elif address == 0x80:
            return commsConstants.PAYLOAD_TYPE.CMD
        elif address == 0x40:
            return commsConstants.PAYLOAD_TYPE.DATA
        else:
            return commsConstants.PAYLOAD_TYPE.UNSET

    def processPacket(self, data):
        for byte in data:
            byte = bytes([byte])
            # TODO: this could be written in more pythonic way
            # force read byte by byte
            self._received.append(byte)
#             logging.debug(byte)
            # find starting flag of the packet
            if not self._foundStart and byte == bytes([0x7E]):
                self._foundStart    = True
                self._receivedStart = len(self._received)
            # find ending flag of the packet
            elif byte == bytes([0x7E]) :
                decoded = self.decoder(self._received, self._receivedStart)
                if decoded is not None:
                    logging.debug(binascii.hexlify(decoded))
                    tmpComms = commsFormat.commsFromBytes(decoded)
                    if tmpComms.compareCrc():
                        control     = tmpComms.getData()[tmpComms.getControl()+1]
                        self._sequenceReceive = (tmpComms.getData()[tmpComms.getControl()] >> 1) & 0x7F
                        
                        # get type of payload and corresponding queue
                        payloadType = self.getInfoType(tmpComms.getData()[tmpComms.getAddress()])
                        tmpQueue   = self.getQueue(payloadType)

                        # get type of packet
                        ctrlFlag    = control & 0x0F
                        if ctrlFlag == 0x05:
                            logging.debug("Received NACK")
                            # received NACK
                        elif ctrlFlag == 0x01:
                            logging.debug("Received ACK")
                            # received ACK
                            self.finishPacket(tmpQueue)
                        else:
                            sequenceReceive = ((control >> 1) & 0x7F) + 1
                            address = tmpComms.getData()[tmpComms.getAddress():tmpComms.getControl()]
                            
                            if self.receivePacket(payloadType, tmpComms):
                                logging.debug("Preparing ACK")
                                commsResponse = commsFormat.commsACK(address = address[0])
                            else:
                                logging.debug("Preparing NACK")
                                commsResponse = commsFormat.commsNACK(address = address)
                            commsResponse.setSequenceReceive(sequenceReceive)
                            self.sendPacket(commsResponse)
                    
                self._received.clear()
                
                self._foundStart    = False
                self._receivedStart = -1
        
    def writePayload(self, payload):
        payloadType = payload.getType()
        if   payloadType == commsConstants.PAYLOAD_TYPE.ALARM:
            tmpComms = commsFormat.generateAlarm(payload)
        elif payloadType == commsConstants.PAYLOAD_TYPE.CMD:
            tmpComms = commsFormat.generateCmd(payload)
        elif payloadType == commsConstants.PAYLOAD_TYPE.DATA:
            tmpComms = commsFormat.generateData(payload)
        else:
            return False        
        tmpComms.setInformation(payload)
        
        with self._dvlock:
            queue = self.getQueue(payloadType)
            queue.append(tmpComms)
            self._datavalid.set()
            
        return True
        
    def sendPacket(self, comms):
        logging.debug("Sending data...")
        logging.debug(binascii.hexlify(self.encoder(comms.getData())))
        self._serial.write(self.encoder(comms.getData()))
    
    def finishPacket(self, queue):
        try:
            with self._dvlock:
                if len(queue) > 0:
                    # 0x7F to deal with possible overflows (0 should follow after 127)
                    if ((queue[0].getSequenceSend() + 1) & 0x7F) == self._sequenceReceive:
                        self._sequenceSend = (self._sequenceSend + 1) % 128
                        queue.popleft()
        except:
            logging.debug("Queue is probably empty")
            
    def receivePacket(self, payloadType, commsPacket):
        if   payloadType == commsConstants.PAYLOAD_TYPE.ALARM:
            payload = commsConstants.AlarmFormat()
        elif payloadType == commsConstants.PAYLOAD_TYPE.CMD:
            payload = commsConstants.CommandFormat()
        elif payloadType == commsConstants.PAYLOAD_TYPE.DATA:
            payload = commsConstants.DataFormat()
        else:
            return False
        
        payload.fromByteArray(commsPacket.getData()[commsPacket.getInformation():commsPacket.getFcs()])
        self.payloadrecv = payload
        return True

    # escape any 0x7D or 0x7E with 0x7D and swap bit 5
    def escapeByte(self, byte):
        if byte == 0x7D or byte == 0x7E:
            return [0x7D, byte ^ (1<<5)]
        else:
            return [byte]

    # encoding data according to the protocol - escape any 0x7D or 0x7E with 0x7D and swap 5 bit
    def encoder(self, data):
        try:
            stream = [escaped for byte in data[1:-1] for escaped in self.escapeByte(byte)]
            result = bytearray([data[0]] + stream + [data[-1]])
            return result
        except:
            return None
    
    # decoding data according to the defined protocol - escape any 0x7D or 0x7E with 0x7D and swap 5 bit
    def decoder(self, data, start):
        try:
            packets = data[start:-1]

            indRemove = [idx for idx in range(len(packets)) if packets[idx] == bytes([0x7D])]
            indChange = [idx+1 for idx in indRemove]

            stream = [packets[idx][0] ^ (1<<5) if idx in indChange else packets[idx][0] for idx in range(len(packets)) if idx not in indRemove]
            result = bytearray([data[start - 1][0]] + stream + [data[-1][0]])
            return result
        except:
            return None
        
    # callback to dependants to read the received payload
    @property
    def payloadrecv(self):
        return self._payloadrecv

    @payloadrecv.setter
    def payloadrecv(self, payload):
        self._payloadrecv.append(payload)
        logging.debug(f"Pushed {payload} to FIFO")
        for callback in self._observers:
            # peek at the leftmost item, don't pop until receipt confirmed
            callback(self._payloadrecv[0])

    def bind_to(self, callback):
        self._observers.append(callback)

    def pop_payloadrecv(self):
        # from callback. confirmed receipt, pop value
        poppedval = self._payloadrecv.popleft()
        logging.debug(f"Popped {poppedval} from FIFO")
        if len(self._payloadrecv) > 0:
            # purge full queue if Dependant goes down when it comes back up
            for callback in self._observers:
                callback(self._payloadrecv[0])
        
if __name__ == "__main__" :
    # example dependant
    class Dependant(object):
        def __init__(self, lli):
            self._llipacket = None
            self._lli = lli
            self._lli.bind_to(self.update_llipacket)

        def update_llipacket(self, payload):
#             logging.debug(f"payload received: {payload!r}")
            self._llipacket = payload
            # pop from queue - protects against Dependant going down and not receiving packets
            self._lli.pop_payloadrecv()

    # get port number for arduino, mostly for debugging
    for port in list_ports.comports():
        try:
            if "ARDUINO" in port.manufacturer.upper():
                port = port.device
        except:
            pass

    commsCtrl = commsControl(port = port)
    example = Dependant(commsCtrl)
    
    payloadSend = commsConstants.CommandFormat()
    payloadSend.cmdCode = 3
    payloadSend.toByteArray()
    
    def burst(payload):
        for _ in range(16):
            commsCtrl.writePayload(payload)
    
#     commsCtrl.writePayload(payloadSend)

#     commsCtrl.payloadrecv = "testpacket1"
#     commsCtrl.payloadrecv = "testpacket2"

#     LEDs = [3,5,7]
#     for _ in range(30):
#         for led in LEDs:
#             commsCtrl.registerData(led)
#         time.sleep(5)

#     while True:
#         pass