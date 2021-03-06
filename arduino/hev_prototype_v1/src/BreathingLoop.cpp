#include "BreathingLoop.h"
#include "common.h"
/*
The Idea of this code is to unfold the FSM in two: one to assign the transitions and the second one to program the states.
*/

BreathingLoop::BreathingLoop()
{
    uint32_t tnow = static_cast<uint32_t>(millis());
    _calib_time = tnow;
    _fsm_time = tnow;
    _fsm_timeout = 1000;
    _ventilation_mode = VENTILATION_MODES::LAB_MODE_BREATHE;
    _bl_state = BL_STATES::IDLE;
    _running = false;
    _reset = false;

    initCalib();
    resetReadingSums();
}

BreathingLoop::~BreathingLoop()
{
;
}

uint8_t BreathingLoop::getVentilationMode()
{
    return static_cast<uint8_t>(_ventilation_mode);
}

uint8_t BreathingLoop::getFsmState()
{
    return static_cast<uint8_t>(_bl_state);
}

void BreathingLoop::updateReadings()
{
    // calc pressure every 1ms
    // create averages every 10ms
    uint32_t tnow = static_cast<uint32_t>(millis());
    if (tnow - _readings_time > _readings_timeout) {
        _readings_time = tnow;
        _readings_N++;

        _readings_sums.timestamp                += tnow;
        _readings_sums.pressure_air_supply      += static_cast<uint32_t>(analogRead(pin_pressure_air_supply)    );
        _readings_sums.pressure_air_regulated   += static_cast<uint32_t>(analogRead(pin_pressure_air_regulated) );
        _readings_sums.pressure_buffer          += static_cast<uint32_t>(analogRead(pin_pressure_buffer)        );
        _readings_sums.pressure_inhale          += static_cast<uint32_t>(analogRead(pin_pressure_inhale)        );
        _readings_sums.pressure_patient         += static_cast<uint32_t>(analogRead(pin_pressure_patient)       );
        _readings_sums.temperature_buffer       += static_cast<uint32_t>(analogRead(pin_temperature_buffer)     );
#ifdef HEV_FULL_SYSTEM
        _readings_sums.pressure_o2_supply       += static_cast<uint32_t>(analogRead(pin_pressure_o2_supply)     );
        _readings_sums.pressure_o2_regulated    += static_cast<uint32_t>(analogRead(pin_pressure_o2_regulated)  );
        _readings_sums.pressure_diff_patient    += static_cast<uint32_t>(analogRead(pin_pressure_diff_patient)  );
#endif
    }

    // to make sure the readings correspond only to the same fsm mode
    if (_readings_reset) {
        resetReadingSums();
    } else if (tnow - _readings_avgs_time > _readings_avgs_timeout) {
        _readings_avgs.timestamp                = static_cast<uint32_t>(_readings_sums.timestamp                / _readings_N);
        _readings_avgs.pressure_air_supply      = static_cast<uint16_t>(_readings_sums.pressure_air_supply      / _readings_N);
        _readings_avgs.pressure_air_regulated   = static_cast<uint16_t>(_readings_sums.pressure_air_regulated   / _readings_N);
        _readings_avgs.pressure_buffer          = static_cast<uint16_t>(_readings_sums.pressure_buffer          / _readings_N);
        _readings_avgs.pressure_inhale          = static_cast<uint16_t>(_readings_sums.pressure_inhale          / _readings_N);
        _readings_avgs.pressure_patient         = static_cast<uint16_t>(_readings_sums.pressure_patient         / _readings_N);
        _readings_avgs.temperature_buffer       = static_cast<uint16_t>(_readings_sums.temperature_buffer       / _readings_N);
#ifdef HEV_FULL_SYSTEM
        _readings_avgs.pressure_o2_supply       = static_cast<uint16_t>(_readings_sums.pressure_o2_supply       / _readings_N);
        _readings_avgs.pressure_o2_regulated    = static_cast<uint16_t>(_readings_sums.pressure_o2_regulated    / _readings_N);
        _readings_avgs.pressure_diff_patient    = static_cast<uint16_t>(_readings_sums.pressure_diff_patient    / _readings_N);
#endif
        resetReadingSums();
    }
}

readings<uint16_t> BreathingLoop::getReadingAverages()
{
    return _readings_avgs;

}

void BreathingLoop::resetReadingSums()
{
    _readings_reset = false;

    uint32_t tnow = static_cast<uint32_t>(millis());
    _readings_time = tnow;
    _readings_avgs_time = tnow;
    _readings_timeout = 1; //ms
    _readings_avgs_timeout = 10; //ms
    _readings_N = 0;
    
    _readings_sums.timestamp                = 0;
    _readings_sums.pressure_air_supply      = 0;
    _readings_sums.pressure_air_regulated   = 0;
    _readings_sums.pressure_buffer          = 0;
    _readings_sums.pressure_inhale          = 0;
    _readings_sums.pressure_patient         = 0;
    _readings_sums.temperature_buffer       = 0;
    _readings_sums.pressure_o2_supply       = 0;
    _readings_sums.pressure_o2_regulated    = 0;
    _readings_sums.pressure_diff_patient    = 0;
}

//This is used to assign the transitions of the fsm
void BreathingLoop::FSM_assignment( ) {
    uint32_t tnow = static_cast<uint32_t>(millis());
    if (tnow - _fsm_time > _fsm_timeout) {
        BL_STATES next_state;
        switch (_bl_state)
        {
        case BL_STATES::IDLE:
            if (_running == true) {
                // FSM_time = millis();
                next_state = BL_STATES::BUFF_PREFILL;
            } else {
                next_state = BL_STATES::IDLE;
            }
            _reset = false;
            break;
        case BL_STATES::CALIBRATION:
            next_state = BL_STATES::BUFF_PREFILL;
            break;
        case BL_STATES::BUFF_PREFILL:
            next_state = BL_STATES::BUFF_FILL;
            break;
        case BL_STATES::BUFF_FILL:
            next_state = BL_STATES::BUFF_LOADED;
            break;
        case BL_STATES::BUFF_LOADED:
            switch (_ventilation_mode)
            {
            case LAB_MODE_FLUSH:
                next_state = BL_STATES::BUFF_FLUSH;
                break;
            case LAB_MODE_PURGE:
                next_state = BL_STATES::BUFF_PURGE;
                break;
            default:
                next_state = BL_STATES::BUFF_PRE_INHALE;
            }
            break;
        case BL_STATES::BUFF_PRE_INHALE:
            next_state = BL_STATES::INHALE;
            break;
        case BL_STATES::INHALE:
            next_state = BL_STATES::PAUSE;
            break;
        case BL_STATES::PAUSE:
            next_state = BL_STATES::EXHALE_FILL;
            break;
        case BL_STATES::EXHALE_FILL:
            next_state = BL_STATES::EXHALE;
            break;
        case BL_STATES::EXHALE:
            if (_running == false) {
                next_state = BL_STATES::IDLE;
            } else {
                next_state = BL_STATES::BUFF_LOADED;
            }
            break;
        case BL_STATES::BUFF_PURGE:
            if (_running == false) {
                next_state = BL_STATES::IDLE;
            } else {
                next_state = BL_STATES::BUFF_PREFILL;
            }
            break;
        case BL_STATES::BUFF_FLUSH:
            next_state = BL_STATES::IDLE;
            break;
        case BL_STATES::STOP:
            if (_reset == true) {
                next_state = BL_STATES::IDLE;
            } else {
                next_state = BL_STATES::STOP;
            }
            break;
        default:
            next_state = _bl_state;
        }
        _bl_state = next_state;
        _fsm_time = tnow;
        // set flag to discard readings due to the mode change
        _readings_reset = true;
    }
}

void BreathingLoop::FSM_breathCycle()
{
    // basic cycle for testing hardware
    // start = digitalRead(pin_button_0);
    switch (_bl_state) {
        case BL_STATES::IDLE:
            
            if (_running == true) {
                // FSM_time = millis();
            } else {
                _fsm_timeout = 1000;
            }
            // TODO
            // air, o2, purge are based on button states in idle
            _valves_controller.setValves(VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED);
            initCalib();
            break;
        case BL_STATES::CALIBRATION : 
            _valves_controller.setValves(VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, 0.9 * VALVE_STATE::OPEN, 0.9 * VALVE_STATE::OPEN, VALVE_STATE::OPEN);
            calibrate();
            // TODO
            // do calib - measure P_regulated for 10 s and calc mean
            // P_patient, P_buffer and P_inhale shoudl be equal
            // WHERE do I call getCalibrationOffset()?
            _fsm_timeout = _states_timeouts.calibration;
            break;
        case BL_STATES::BUFF_PREFILL:
            // TODO - exhale settable; timeout expert settable
            _valves_controller.setValves(VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, 0.8 * VALVE_STATE::OPEN, VALVE_STATE::CLOSED);
            _fsm_timeout = _states_timeouts.buff_prefill;
            break;
        case BL_STATES::BUFF_FILL:
            // TODO - exhale settable; timeout settable
            _valves_controller.setValves(VALVE_STATE::OPEN, VALVE_STATE::OPEN, VALVE_STATE::CLOSED, 0.8 * VALVE_STATE::OPEN, VALVE_STATE::CLOSED);
            _fsm_timeout = _states_timeouts.buff_fill;
            break;
        case BL_STATES::BUFF_LOADED:
            // TODO - exhale settable
            // Calc pressure and stay in loaded if not ok
            // pressure settable by expert
            _valves_controller.setValves(VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, 0.8 * VALVE_STATE::OPEN, VALVE_STATE::CLOSED);
            _fsm_timeout = _states_timeouts.buff_loaded;
            break;
        case BL_STATES::BUFF_PRE_INHALE:
            // TODO spontaneous trigger can be enabled
            // - can be triggered by : 
                // P_inhale ; 
                // P_diff_patient //=flow
                // P_patient and p_diff_patient
                // P_patient or p_diff_patient
                // with thresholds on each
            _valves_controller.setValves(VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED);
            switch (_ventilation_mode)
            {
                case LAB_MODE_FLUSH:
                    _fsm_timeout = 100;
                    
                    break;
                case LAB_MODE_PURGE:
                    _fsm_timeout =500;
                    
                    break;
                default:
                    _fsm_timeout = _states_timeouts.buff_pre_inhale;
            }
        
            break;
        case BL_STATES::INHALE:
            // TODO : spontaneous trigger
            // if p_diff_patient < thresh (def: 25% below nominal)
            // go to exhale fill
            // TODO : spontaneous trigger
            // if p_inhale > max thresh pressure(def: 50?)
            // go to exhale fill
            _valves_controller.setValves(VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, 0.8*VALVE_STATE::OPEN, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED);
            _fsm_timeout = _states_timeouts.inhale;
            
            break;
        case BL_STATES::PAUSE:
            _valves_controller.setValves(VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED);
            _fsm_timeout = _states_timeouts.pause;
            break;
        case BL_STATES::EXHALE_FILL:
            _valves_controller.setValves(VALVE_STATE::OPEN, VALVE_STATE::OPEN, VALVE_STATE::CLOSED, 0.9 * VALVE_STATE::OPEN, VALVE_STATE::CLOSED);
            _fsm_timeout = _states_timeouts.exhale_fill;
            break;
        case BL_STATES::EXHALE:
            // TODO: exhale timeout based on 
            // (inhale_time* (Exhale/Inhale ratio))  -  fill time
            _states_timeouts.exhale = calculateTimeoutExhale();
            _valves_controller.setValves(VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, 0.9 * VALVE_STATE::OPEN, VALVE_STATE::CLOSED);
            _fsm_timeout = _states_timeouts.exhale;
            break;
        case BL_STATES::BUFF_PURGE:
            _valves_controller.setValves(VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, 0.9 * VALVE_STATE::OPEN, VALVE_STATE::OPEN);
            _fsm_timeout = _states_timeouts.buff_purge;
            break;
        case BL_STATES::BUFF_FLUSH:
            _valves_controller.setValves(VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, 0.9 * VALVE_STATE::OPEN, 0.9 * VALVE_STATE::OPEN, VALVE_STATE::CLOSED);
            _fsm_timeout = _states_timeouts.buff_flush;
            break;
        case BL_STATES::STOP: 
            // TODO : require a reset command to go back to idle
            _valves_controller.setValves(VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED, VALVE_STATE::CLOSED);
            _fsm_timeout = 1000;
            break;
    }

}

void BreathingLoop::doStart()
{
    _running = true;
}

void BreathingLoop::doStop()
{
    _running = false;
}

void BreathingLoop::doReset()
{
    _reset = true;
}

bool BreathingLoop::getRunning()
{
    return _running;
}

void BreathingLoop::calibrate()
{
    // get pressure_air_regulated over 10s calc mean
    uint32_t tnow = static_cast<uint32_t>(millis());
    if (tnow - _calib_time > _calib_timeout) {
        _calib_N++;
        _calib_sum_pressure += static_cast<uint32_t>(analogRead(pin_pressure_air_regulated));
        _calib_avg_pressure  = static_cast<float   >(_calib_sum_pressure / _calib_N);
    }
}


void BreathingLoop::initCalib()
{
    _calib_timeout = 10;
    _calib_time = static_cast<uint32_t>(millis());
    _calib_sum_pressure = 0;
    _calib_avg_pressure = 0;
    _calib_N = 0;
}

float BreathingLoop::getCalibrationOffset()
{
    return _calib_avg_pressure;
}

states_timeouts &BreathingLoop::getTimeouts() {
    return _states_timeouts;
}

// FIXME 1/1 has to be replaced using exhale/inhale ratio
uint32_t BreathingLoop::calculateTimeoutExhale() {
    return static_cast<uint32_t>(_states_timeouts.inhale * ( 1/ 1) ) - _states_timeouts.buff_fill;
}

ValvesController* BreathingLoop::getValvesController()
{
    return &_valves_controller;
}
