#include <Arduino.h>

// LNURLDevices ATM details
String baseURLATM = "https://lnbits.com/lnurldevice/api/v1/lnurl/xxxx";
String secretATM = "XXXXXXXXXXXXXXXXXXXXxxx";
String currencyATM = "EUR";

// Coin and Bill Acceptor amounts
int charge = 10; // % you will charge people for service, set in LNbits extension
int maxamount = 1; // max amount per withdraw