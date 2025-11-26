#include "qpcpp.hpp"
#include "main.hpp"
#include "bsp.hpp"
#include "esp_freertos_hooks.h"

using namespace QP;

// Pin definitions
static constexpr uint8_t LED_PIN = 2;
static constexpr uint8_t BTN_MODE_PIN = 18;
static constexpr uint8_t BTN_LOCK_PIN = 19;

// Static immutable events for ISRs
static QEvt const modeEvt = { SIG_BTN_MODE, 0, 0 };
static QEvt const lockEvt = { SIG_BTN_LOCK, 0, 0 };

// Tick Hook for QP framework integration with ESP32 FreeRTOS
static void tickHook_ESP32(void);
static uint8_t const l_TickHook = 0;

static void tickHook_ESP32(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    QTimeEvt::TICK_X_FROM_ISR(0U, &xHigherPriorityTaskWoken, &l_TickHook);
    if(xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// Button Interrupt Handlers
void IRAM_ATTR isrBtnMode() {
    static uint32_t last_millis = 0;
    if (millis() - last_millis > 200) { // Simple debounce
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        
        // Використовуємо POST_FROM_ISR (3 аргументи)
        AO_Lab1->POST_FROM_ISR(&modeEvt, &xHigherPriorityTaskWoken, &l_TickHook);
        
        if(xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
        last_millis = millis();
    }
}

void IRAM_ATTR isrBtnLock() {
    static uint32_t last_millis = 0;
    if (millis() - last_millis > 200) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        
        // Використовуємо POST_FROM_ISR (3 аргументи)
        AO_Lab1->POST_FROM_ISR(&lockEvt, &xHigherPriorityTaskWoken, &l_TickHook);
        
        if(xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
        last_millis = millis();
    }
}

// BSP Implementation
void BSP::init(void) {
    pinMode(LED_PIN, OUTPUT);
    pinMode(BTN_MODE_PIN, INPUT_PULLUP);
    pinMode(BTN_LOCK_PIN, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(BTN_MODE_PIN), isrBtnMode, FALLING);
    attachInterrupt(digitalPinToInterrupt(BTN_LOCK_PIN), isrBtnLock, FALLING);

    Serial.begin(115200);
    Serial.println("BSP Initialized");
    
    // Ініціалізація QS (хоча б заглушка)
    QS_INIT(nullptr);
}

void BSP::ledOn(void) { digitalWrite(LED_PIN, HIGH); }
void BSP::ledOff(void) { digitalWrite(LED_PIN, LOW); }
void BSP::ledToggle(void) { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); }

// Needed for QF initialization
void QF::onStartup(void) {
    esp_register_freertos_tick_hook_for_cpu(tickHook_ESP32, 0); // Core 0
}

extern "C" Q_NORETURN Q_onAssert(char const * const module, int location) {
    Serial.print("QP Assertion Failure: ");
    Serial.print(module);
    Serial.print(":");
    Serial.println(location);
    while(1);
}

//============================================================================
// QS Callbacks (Необхідні для лінковки, навіть якщо QS вимкнено)
//============================================================================
namespace QP {

QSTimeCtr QS::onGetTime(void) {
    return millis();
}

void QS::onFlush(void) {
#ifdef QS_ON
    Serial.flush();
#endif
}

bool QS::onStartup(void const * arg) {
    static uint8_t qsTxBuf[1024]; 
    static uint8_t qsRxBuf[128];
    initBuf(qsTxBuf, sizeof(qsTxBuf));
    rxInitBuf(qsRxBuf, sizeof(qsRxBuf));
    return true;
}

void QS::onCleanup(void) {
}

void QS::onCommand(uint8_t cmdId, uint32_t param1, uint32_t param2, uint32_t param3) {
    (void)cmdId; (void)param1; (void)param2; (void)param3;
}

} // namespace QP