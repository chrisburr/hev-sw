#ifndef COMMSCONSTANTS_H
#define COMMSCONSTANTS_H

#include <Arduino.h>

#define CONST_TIMEOUT_ALARM 5
#define CONST_TIMEOUT_DATA  10
#define CONST_TIMEOUT_CMD   50


#define CONST_MAX_SIZE_RB_RECEIVING 2
#define CONST_MAX_SIZE_RB_SENDING 2
#define CONST_MAX_SIZE_PACKET 64
#define CONST_MAX_SIZE_BUFFER 128
#define CONST_MIN_SIZE_PACKET 7

#define COMMS_FRAME_BOUNDARY 0x7E
#define COMMS_FRAME_ESCAPE   0x7D
#define COMMS_ESCAPE_BIT_SWAP 5

#define COMMS_CONTROL_INFORMATION 0x00
#define COMMS_CONTROL_SUPERVISORY 0x01

#define COMMS_CONTROL_TYPES 0x0F
#define COMMS_CONTROL_ACK   0x00 | COMMS_CONTROL_SUPERVISORY
#define COMMS_CONTROL_NACK  0x04 | COMMS_CONTROL_SUPERVISORY

#define PACKET_TYPE  0xC0
#define PACKET_ALARM 0xC0
#define PACKET_CMD   0x80
#define PACKET_DATA  0x40
#define PACKET_SET   0x20 //set vs get ?

#define HEV_FORMAT_VERSION 0xA2

// struct for all data sent
#pragma pack(1)
// fast values - read every ~10 ms
struct fast_data_format {
    uint8_t  version                = HEV_FORMAT_VERSION;
    uint32_t timestamp              = 0;
    uint8_t  fsm_state              = 0;
    uint16_t pressure_air_supply    = 0;
    uint16_t pressure_air_regulated = 0;
    uint16_t pressure_o2_supply     = 0;
    uint16_t pressure_o2_regulated  = 0;
    uint16_t pressure_buffer        = 0;
    uint16_t pressure_inhale        = 0;
    uint16_t pressure_patient       = 0;
    uint16_t temperature_buffer     = 0;
    uint16_t pressure_diff_patient  = 0;
    uint16_t ambient_pressure       = 0;
    uint16_t ambient_temperature    = 0;
    float airway_pressure           = 0;
    float flow                      = 0;
    float volume                    = 0;
};
#pragma pack()


#pragma pack(1)
// readback values
struct readback_data_format {
    uint8_t  version                  = HEV_FORMAT_VERSION;
    uint32_t timestamp                = 0;
    uint16_t duration_calibration     = 0;
    uint16_t duration_buff_purge      = 0;
    uint16_t duration_buff_flush      = 0;
    uint16_t duration_buff_prefill    = 0;
    uint16_t duration_buff_fill       = 0;
    uint16_t duration_buff_loaded     = 0;
    uint16_t duration_buff_pre_inhale = 0;
    uint16_t duration_inhale          = 0;
    uint16_t duration_pause           = 0;
    uint16_t duration_exhale_fill     = 0;
    uint16_t duration_exhale          = 0;

    uint8_t  valve_air_in             = 0;
    uint8_t  valve_o2_in              = 0;
    uint8_t  valve_inhale             = 0;
    uint8_t  valve_exhale             = 0;
    uint8_t  valve_purge              = 0;
    uint8_t  ventilation_mode         = 0;

    uint8_t valve_inhale_percent      = 0;
    uint8_t valve_exhale_percent      = 0;
    uint8_t valve_air_in_enable       = 0;
    uint8_t valve_o2_in_enable        = 0;
    uint8_t valve_purge_enable        = 0;
    uint8_t inhale_trigger_enable     = 0;
    uint8_t exhale_trigger_enable     = 0;
    uint8_t peep                      = 0; 
    float   inhale_exhate_ratio       = 0.0;
};
#pragma pack()

#pragma pack(1)
struct cycle_data_format {
// per breath values
    uint8_t  version                    = HEV_FORMAT_VERSION;
    uint32_t timestamp                  = 0;

    float respiratory_rate              = 0;

    float tidal_volume                  = 0;
    float exhaled_tidal_volume          = 0;
    float inhaled_tidal_volume          = 0;

    float minute_volume                 = 0;
    float exhaled_minute_volume         = 0;
    float inhaled_minute_volume         = 0;

    float lung_compliance               = 0;
    float static_compliance             = 0;

    uint16_t inhalation_pressure        = 0;
    uint16_t peak_inspiratory_pressure  = 0;
    uint16_t plateau_pressure           = 0;
    uint16_t mean_airway_pressure       = 0;

    uint8_t  fi02_percent               = 0;

    uint16_t apnea_index                = 0;
    uint16_t apnea_time                 = 0;

    uint8_t mandatory_breath            = 0;
};
#pragma pack()

#pragma pack(1)
struct cmd_format {
    uint8_t  version   = HEV_FORMAT_VERSION;
    uint32_t timestamp = 0;
    uint8_t  cmd_type  = 0;
    uint8_t  cmd_code  = 0;
    uint32_t param     = 0;
};
#pragma pack()

#pragma pack(1)
struct alarm_format {
    uint8_t  version    = HEV_FORMAT_VERSION;
    uint32_t timestamp  = 0;
    uint8_t  alarm_type = 0;
    uint8_t  alarm_code = 0;
    uint32_t param      = 0;
};
#pragma pack()

// enum of all transfer types
enum PAYLOAD_TYPE {
    FASTDATA,
    READBACKDATA,
    CYCLEDATA,
    CMD,
    ALARM,
    UNSET
};

// payload consists of type and information
// type is set as address in the protocol
// information is set as information in the protocol
class Payload {
public:
    Payload(PAYLOAD_TYPE type = PAYLOAD_TYPE::UNSET)  {_type = type; } //data_ = nullptr; cmd_ = nullptr; alarm_ = nullptr; }
    Payload(const Payload &other) {
        _type = other._type;
        memcpy(&_fast_data      , &other._fast_data     , sizeof(    fast_data_format));
        memcpy(&_readback_data  , &other._readback_data , sizeof(readback_data_format));
        memcpy(&_cycle_data     , &other._cycle_data    , sizeof(   cycle_data_format));
        memcpy(&_cmd            , &other._cmd           , sizeof(          cmd_format));
        memcpy(&_alarm          , &other._alarm         , sizeof(        alarm_format));
    }
    Payload& operator=(const Payload& other) {
        _type = other._type;
        memcpy(&_fast_data      , &other._fast_data     , sizeof(    fast_data_format));
        memcpy(&_readback_data  , &other._readback_data , sizeof(readback_data_format));
        memcpy(&_cycle_data     , &other._cycle_data    , sizeof(   cycle_data_format));
        memcpy(&_cmd            , &other._cmd           , sizeof(          cmd_format));
        memcpy(&_alarm          , &other._alarm         , sizeof(        alarm_format));
        return *this;
    }

    ~Payload() { unsetAll(); }

    void setType(PAYLOAD_TYPE type) { _type = type; }
    PAYLOAD_TYPE getType() {return _type; }

    // requires argument as new struct
    void setFastData     (fast_data_format      *data  ) { _type = PAYLOAD_TYPE::FASTDATA    ; memcpy(&_fast_data    ,  data, sizeof(    fast_data_format)); }
    void setReadbackData (readback_data_format  *data  ) { _type = PAYLOAD_TYPE::READBACKDATA; memcpy(&_readback_data,  data, sizeof(readback_data_format)); }
    void setCycleData    (cycle_data_format     *data  ) { _type = PAYLOAD_TYPE::CYCLEDATA   ; memcpy(&_cycle_data   ,  data, sizeof(   cycle_data_format)); }
    void setCmd          (cmd_format            *cmd   ) { _type = PAYLOAD_TYPE::CMD         ; memcpy(&_cmd          ,   cmd, sizeof(          cmd_format)); }
    void setAlarm        (alarm_format          *alarm ) { _type = PAYLOAD_TYPE::ALARM       ; memcpy(&_alarm        , alarm, sizeof(        alarm_format)); }

    // get pointers to particular payload types
    fast_data_format     *getFastData     () {return &_fast_data    ; }
    readback_data_format *getReadbackData () {return &_readback_data; }
    cycle_data_format    *getCycleData    () {return &_cycle_data   ; }
    cmd_format           *getCmd          () {return &_cmd          ; }
    alarm_format         *getAlarm        () {return &_alarm        ; }

    void unsetAll()         {unsetFastData(); unsetReadbackData(); unsetCycleData(); unsetAlarm(); unsetCmd(); _type = PAYLOAD_TYPE::UNSET; }
    void unsetFastData()    { memset(&_fast_data    , 0, sizeof(    fast_data_format)); }
    void unsetReadbackData(){ memset(&_readback_data, 0, sizeof(readback_data_format)); }
    void unsetCycleData()   { memset(&_cycle_data   , 0, sizeof(   cycle_data_format)); }
    void unsetCmd()         { memset(&_cmd          , 0, sizeof(          cmd_format)); }
    void unsetAlarm()       { memset(&_alarm        , 0, sizeof(        alarm_format)); }

    void setPayload(PAYLOAD_TYPE type, void* information) {
        setType(type);
        setInformation(information);
    }

    void setInformation(void* information) {
        switch (_type) {
            case PAYLOAD_TYPE::FASTDATA:
                setFastData    (reinterpret_cast<     fast_data_format*>(information));
                break;
            case PAYLOAD_TYPE::READBACKDATA:
                setReadbackData(reinterpret_cast< readback_data_format*>(information));
                break;
            case PAYLOAD_TYPE::CYCLEDATA:
                setCycleData   (reinterpret_cast<    cycle_data_format*>(information));
                break;
            case PAYLOAD_TYPE::CMD:
                setCmd         (reinterpret_cast<           cmd_format*>(information));
                break;
            case PAYLOAD_TYPE::ALARM:
                setAlarm       (reinterpret_cast<         alarm_format*>(information));
                break;
            default:
                break;
        }
    }

    // returns void pointer, in case you know what to do with data or dont care what the format is
    void *getInformation() {
        switch (_type) {
            case PAYLOAD_TYPE::FASTDATA:
                return reinterpret_cast<void*>(getFastData    ());
            case PAYLOAD_TYPE::READBACKDATA:
                return reinterpret_cast<void*>(getReadbackData());
            case PAYLOAD_TYPE::CYCLEDATA:
                return reinterpret_cast<void*>(getCycleData   ());
            case PAYLOAD_TYPE::CMD:
                return reinterpret_cast<void*>(getCmd         ());
            case PAYLOAD_TYPE::ALARM:
                return reinterpret_cast<void*>(getAlarm       ());
            default:
                return nullptr;
        }
    }

    // returns payload information size
    uint8_t getSize()  {
        switch (_type) {
            case PAYLOAD_TYPE::FASTDATA:
                return static_cast<uint8_t>(sizeof(     fast_data_format));
            case PAYLOAD_TYPE::READBACKDATA:
                return static_cast<uint8_t>(sizeof( readback_data_format));
            case PAYLOAD_TYPE::CYCLEDATA:
                return static_cast<uint8_t>(sizeof(    cycle_data_format));
            case PAYLOAD_TYPE::CMD:
                return static_cast<uint8_t>(sizeof(           cmd_format));
            case PAYLOAD_TYPE::ALARM:
                return static_cast<uint8_t>(sizeof(         alarm_format));
            default:
                return 0;
        }
    }

private:
    PAYLOAD_TYPE _type;

    fast_data_format     _fast_data;
    readback_data_format _readback_data;
    cycle_data_format    _cycle_data;
    cmd_format           _cmd;
    alarm_format         _alarm;
};

#endif
