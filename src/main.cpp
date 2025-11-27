#include "qpcpp.hpp"
#include "main.hpp"
#include "bsp.hpp"

using namespace QP;
static constexpr unsigned stack_size = 4096;

//============================================================================
// Lab1ActiveObject Class
//============================================================================
class Lab1AO : public QActive {
private:
    QTimeEvt m_blinkTimer; // Timer for blinking state
    QTimeEvt m_ecoTimer;   // Timer for ECO mode (Variant 1)
    
    // Manual History Implementation
    // We store the pointer to the handler function of the last active state
    QStateHandler m_history; 

public:
    Lab1AO() 
      : QActive(Q_STATE_CAST(&Lab1AO::initial)),
        m_blinkTimer(this, SIG_BLINK_TIMEOUT, 0U),
        m_ecoTimer(this, SIG_ECO_TIMEOUT, 0U)
    {
        m_history = Q_STATE_CAST(&Lab1AO::off); // Default history
    }

protected:
    Q_STATE_DECL(initial);
    Q_STATE_DECL(active);  // Super-state
    Q_STATE_DECL(off);
    Q_STATE_DECL(on);
    Q_STATE_DECL(blink);
    Q_STATE_DECL(locked);
};

// Static instance
static Lab1AO l_lab1;
QActive * const AO_Lab1 = &l_lab1;

//============================================================================
// State Machine Implementation
//============================================================================

// Initial Transition
Q_STATE_DEF(Lab1AO, initial) {
    (void)e;
    BSP::init();
    Serial.println("SM: Initial -> Active");
    return tran(&active);
}

// Super-state 'Active'
// Handles global transition to Locked
Q_STATE_DEF(Lab1AO, active) {
    QState status_;
    switch (e->sig) {
        case Q_ENTRY_SIG:
            Serial.println("SM: Active Entry");
            // If we enter Active directly (e.g. from init), go to Off
            // Note: This relies on initial tran targeting 'active'. 
            // Better to target 'off' directly, but here we use init transition in substate logic.
            status_ = Q_RET_HANDLED;
            break;
            
        case Q_INIT_SIG:
            status_ = tran(&off);
            break;

        case SIG_BTN_LOCK:
            Serial.println("EVENT: BTN_LOCK -> Locking system");
            status_ = tran(&locked);
            break;

        default:
            status_ = super(&top);
            break;
    }
    return status_;
}

// State 'Off'
Q_STATE_DEF(Lab1AO, off) {
    QState status_;
    switch (e->sig) {
        case Q_ENTRY_SIG:
            Serial.println("STATE: OFF");
            BSP::ledOff();
            l_lab1.m_history = Q_STATE_CAST(&Lab1AO::off); // Save history
            status_ = Q_RET_HANDLED;
            break;

        case SIG_BTN_MODE:
            Serial.println("EVENT: BTN_MODE -> Switching to ON");
            status_ = tran(&on);
            break;

        default:
            status_ = super(&active);
            break;
    }
    return status_;
}

// State 'On'
Q_STATE_DEF(Lab1AO, on) {
    QState status_;
    switch (e->sig) {
        case Q_ENTRY_SIG:
            Serial.println("STATE: ON (ECO Timer started)");
            BSP::ledOn();
            l_lab1.m_history = Q_STATE_CAST(&Lab1AO::on); // Save history
            // Variant 1: Auto-off after 10 seconds
            l_lab1.m_ecoTimer.armX(10 * BSP::TICKS_PER_SEC, 0); 
            status_ = Q_RET_HANDLED;
            break;

        case Q_EXIT_SIG:
            l_lab1.m_ecoTimer.disarm();
            status_ = Q_RET_HANDLED;
            break;

        case SIG_BTN_MODE:
            Serial.println("EVENT: BTN_MODE -> Switching to BLINK");
            status_ = tran(&blink);
            break;
            
        case SIG_ECO_TIMEOUT:
            Serial.println("EVENT: ECO TIMEOUT -> Auto OFF");
            status_ = tran(&off);
            break;

        default:
            status_ = super(&active);
            break;
    }
    return status_;
}

// State 'Blink'
Q_STATE_DEF(Lab1AO, blink) {
    QState status_;
    switch (e->sig) {
        case Q_ENTRY_SIG:
            Serial.println("STATE: BLINK");
            l_lab1.m_history = Q_STATE_CAST(&Lab1AO::blink); // Save history
            // 2 Hz = 500ms period (250ms ON, 250ms OFF)
            l_lab1.m_blinkTimer.armX(BSP::TICKS_PER_SEC / 4, BSP::TICKS_PER_SEC / 4);
            status_ = Q_RET_HANDLED;
            break;

        case Q_EXIT_SIG:
            l_lab1.m_blinkTimer.disarm();
            status_ = Q_RET_HANDLED;
            break;

        case SIG_BLINK_TIMEOUT:
            BSP::ledToggle();
            status_ = Q_RET_HANDLED;
            break;

        case SIG_BTN_MODE:
            Serial.println("EVENT: BTN_MODE -> Switching to OFF");
            status_ = tran(&off);
            break;

        default:
            status_ = super(&active);
            break;
    }
    return status_;
}

// State 'Locked'
Q_STATE_DEF(Lab1AO, locked) {
    QState status_;
    switch (e->sig) {
        case Q_ENTRY_SIG:
            Serial.println("STATE: LOCKED");
            // Fast flicker for Locked state (10Hz)
            l_lab1.m_blinkTimer.armX(BSP::TICKS_PER_SEC / 20, BSP::TICKS_PER_SEC / 20);
            status_ = Q_RET_HANDLED;
            break;
            
        case Q_EXIT_SIG:
            l_lab1.m_blinkTimer.disarm();
            status_ = Q_RET_HANDLED;
            break;

        case SIG_BLINK_TIMEOUT:
            BSP::ledToggle();
            status_ = Q_RET_HANDLED;
            break;

        case SIG_BTN_LOCK:
            Serial.println("EVENT: BTN_LOCK -> Unlocking (Restoring History)");
            // Return to saved history state
            status_ = tran(l_lab1.m_history);
            break;
            
        // Ignore MODE button in Locked state
        case SIG_BTN_MODE:
            status_ = Q_RET_HANDLED; 
            break;

        default:
            status_ = super(&top);
            break;
    }
    return status_;
}

//============================================================================
// Main Arduino Loops
//============================================================================

void setup() {
    // Швидкий тест LED перед запуском QP
    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);
    delay(1000); // Світлодіод має засвітитися на 1 сек
    digitalWrite(2, LOW);

    QF::init();
    BSP::init();
    //static uint64_t stack[1024];
    // Static event queue
    static QEvt const *lab1QueueSto[10];
    
    AO_Lab1->start(
                    1U, // Priority
                    lab1QueueSto, 
                    Q_DIM(lab1QueueSto),
                    0,
                    stack_size
                );
                   
    QF::run(); // Run the framework
}

void loop() {
    // Empty, QF manages the loop
}
