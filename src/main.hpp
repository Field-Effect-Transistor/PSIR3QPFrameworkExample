//  main.hpp

#ifndef MAIN_HPP
#define MAIN_HPP

#include "qpcpp.hpp"

enum Lab1Signals {
    SIG_BTN_MODE = QP::Q_USER_SIG,
    SIG_BTN_LOCK,
    SIG_BLINK_TIMEOUT, // For 2Hz blink
    SIG_ECO_TIMEOUT    // For Variant 1: 10s auto-off
};

// Opaque pointer to the Active Object
extern QP::QActive * const AO_Lab1;

#endif // MAIN_HPP
