
// FOSSA NO ACCESS POINT, SIMPLE

#include "Config.h"
#include <Hash.h>
#include <qrcode.h>
#include <Bitcoin.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <HardwareSerial.h>
#include <JC_Button.h>

String qrData;

float total;

int moneyTimer = 0;

HardwareSerial SerialPort2(2);

TFT_eSPI tft = TFT_eSPI();
Button BTNA(BTN1);

void setup()
{
  Serial.begin(115200);
  Serial.println("La fossa è sveglia");
  Serial.println("charge " + String(charge));
  Serial.println("maxamount " + String(maxamount));
  BTNA.begin();

  tft.init();
  tft.setRotation(1);
  tft.invertDisplay(false);

  SerialPort2.begin(4800, SERIAL_8N1, gettoniera_TX2);
  Serial.println("Gettoniera connessa su IO " + String(gettoniera_TX2));

  pinMode(INHIBITMECH, OUTPUT);
  Serial.println("Controllo abilitazione gettoniera su IO " + String(INHIBITMECH));
}

void loop()
{
  // Turn on machines
  digitalWrite(INHIBITMECH, LOW);
  Serial.println("Gettoniera abilitata");

  moneyTimerFun();
  makeLNURL();
  qrShowCodeLNURL("SCAN ME. TAP SCREEN WHEN FINISHED");
}

void printMessage(String text1, String text2, String text3, int ftcolor, int bgcolor)
{
  tft.fillScreen(bgcolor);
  tft.setTextColor(ftcolor, bgcolor);
  tft.setTextSize(5);
  tft.setCursor(30, 40);
  tft.println(text1);
  tft.setCursor(30, 120);
  tft.println(text2);
  tft.setCursor(30, 200);
  tft.setTextSize(3);
  tft.println(text3);
}

void qrShowCodeLNURL(String message)
{
  tft.fillScreen(TFT_WHITE);
  qrData.toUpperCase();
  const char *qrDataChar = qrData.c_str();
  QRCode qrcoded;
  uint8_t qrcodeData[qrcode_getBufferSize(20)];
  qrcode_initText(&qrcoded, qrcodeData, 11, 0, qrDataChar);

  for (uint8_t y = 0; y < qrcoded.size; y++)
  {
    for (uint8_t x = 0; x < qrcoded.size; x++)
    {
      if (qrcode_getModule(&qrcoded, x, y))
      {
        tft.fillRect(120 + 4 * x, 20 + 4 * y, 4, 4, TFT_BLACK);
      }
      else
      {
        tft.fillRect(120 + 4 * x, 20 + 4 * y, 4, 4, TFT_WHITE);
      }
    }
  }

  tft.setCursor(40, 290);
  tft.setTextSize(2);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.println(message);

  while (true)
  {
    BTNA.read();
    if (BTNA.wasReleased())
      break;
  }
}

void moneyTimerFun()
{
  total = 0;
  printMessage("Feed me fiat", String(charge) + "% charge", "", TFT_WHITE, TFT_BLACK);
  while (true)
  {
    if (SerialPort2.available())
    {
      float coin = (float)SerialPort2.read() / 100;
      Serial.println("Inserita moneta da " + String(coin));
      total = total + coin;
      printMessage(String(coin) + " " + currencyATM, "Total: " + String(total) + " " + currencyATM, "TAP SCREEN WHEN FINISHED", TFT_WHITE, TFT_BLACK);
    }
    BTNA.read();
    if (total >= maxamount || (total > 0 && BTNA.wasReleased()))
      break;
  }

  // Turn off machines
  digitalWrite(INHIBITMECH, HIGH);
  Serial.println("Gettoniera disabilitata");
}

void to_upper(char *arr)
{
  for (size_t i = 0; i < strlen(arr); i++)
  {
    if (arr[i] >= 'a' && arr[i] <= 'z')
    {
      arr[i] = arr[i] - 'a' + 'A';
    }
  }
}

////////////////////////////////////////////
///////////////LNURL STUFF//////////////////
////USING STEPAN SNIGREVS GREAT CRYTPO//////
////////////THANK YOU STEPAN////////////////
////////////////////////////////////////////

void makeLNURL()
{
  int randomPin = random(1000, 9999);
  byte nonce[8];
  for (int i = 0; i < 8; i++)
  {
    nonce[i] = random(256);
  }

  byte payload[51]; // 51 bytes is max one can get with xor-encryption

  total = total * 100;
  size_t payload_len = xor_encrypt(payload, sizeof(payload), (uint8_t *)secretATM.c_str(), secretATM.length(), nonce, sizeof(nonce), randomPin, float(total));
  String preparedURL = baseURLATM + "?atm=1&p=";
  preparedURL += toBase64(payload, payload_len, BASE64_URLSAFE | BASE64_NOPADDING);

  Serial.println(preparedURL);
  char Buf[200];
  preparedURL.toCharArray(Buf, 200);
  char *url = Buf;
  byte *data = (byte *)calloc(strlen(url) * 2, sizeof(byte));
  size_t len = 0;
  int res = convert_bits(data, &len, 5, (byte *)url, strlen(url), 8, 1);
  char *charLnurl = (char *)calloc(strlen(url) * 2, sizeof(byte));
  bech32_encode(charLnurl, "lnurl", data, len);
  to_upper(charLnurl);
  // qrData = "lightning://" + String(charLnurl);
  qrData = charLnurl;
}

int xor_encrypt(uint8_t *output, size_t outlen, uint8_t *key, size_t keylen, uint8_t *nonce, size_t nonce_len, uint64_t pin, uint64_t amount_in_cents)
{
  // check we have space for all the data:
  // <variant_byte><len|nonce><len|payload:{pin}{amount}><hmac>
  if (outlen < 2 + nonce_len + 1 + lenVarInt(pin) + 1 + lenVarInt(amount_in_cents) + 8)
  {
    return 0;
  }

  int cur = 0;
  output[cur] = 1; // variant: XOR encryption
  cur++;

  // nonce_len | nonce
  output[cur] = nonce_len;
  cur++;
  memcpy(output + cur, nonce, nonce_len);
  cur += nonce_len;

  // payload, unxored first - <pin><currency byte><amount>
  int payload_len = lenVarInt(pin) + 1 + lenVarInt(amount_in_cents);
  output[cur] = (uint8_t)payload_len;
  cur++;
  uint8_t *payload = output + cur;                                 // pointer to the start of the payload
  cur += writeVarInt(pin, output + cur, outlen - cur);             // pin code
  cur += writeVarInt(amount_in_cents, output + cur, outlen - cur); // amount
  cur++;

  // xor it with round key
  uint8_t hmacresult[32];
  SHA256 h;
  h.beginHMAC(key, keylen);
  h.write((uint8_t *)"Round secret:", 13);
  h.write(nonce, nonce_len);
  h.endHMAC(hmacresult);
  for (int i = 0; i < payload_len; i++)
  {
    payload[i] = payload[i] ^ hmacresult[i];
  }

  // add hmac to authenticate
  h.beginHMAC(key, keylen);
  h.write((uint8_t *)"Data:", 5);
  h.write(output, cur);
  h.endHMAC(hmacresult);
  memcpy(output + cur, hmacresult, 8);
  cur += 8;

  // return number of bytes written to the output
  return cur;
}
