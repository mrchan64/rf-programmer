/* 
 * Bluetooth Programmer pair to be attached on the mouse side
 * Acts as a BLE Peripheral and transmits swo, sck, and tx, rx signals
 */
#include "mbed.h"
#include "ble/BLE.h"
#include "BufferedSerial.h"

/* BLE Characteristic UUIDs */
#define UUID_TX     0x0101
#define UUID_RX     0x0102
#define UUID_SWI    0x0103
#define UUID_SWO    0x0104
#define UUID_SWCK   0x0105

/* BLE Service UUIDs */
#define UUID_UART   0x0111
#define UUID_SWD    0x0112

/* CONSTANTS */
#define BAUD_RATE    9600

/* PAYLOAD LENGTH */
#define PAYLOAD_LEN 1

/* CONNECTION NAME */
#define DEVICE_NAME "Mouse Receiver\0"
#define LED_RATE      1
#define LED_ATT_RATE  0.25

BLE  ble;
RawSerial pc(USBTX, USBRX); // should be longer buffer
DigitalOut led(LED1);
Ticker ledCLK, testCLK;

const uint8_t manufacturerData[2] = {0x12, 0x34};
const uint16_t serviceList[2] = {UUID_UART, UUID_SWD};

void cCallback(const Gap::ConnectionCallbackParams_t *params);
void dcCallback(const Gap::DisconnectionCallbackParams_t *params);
void toggleLED();

/* UART Service setup */
uint8_t txPayload[PAYLOAD_LEN] = {0};
uint8_t rxPayload[PAYLOAD_LEN] = {0};

GattCharacteristic  txChar (UUID_TX, txPayload, 1, PAYLOAD_LEN, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE);
                    
GattCharacteristic  rxChar (UUID_RX, rxPayload, 1, PAYLOAD_LEN, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY);
                    
GattCharacteristic *uartChars[] = {&txChar, &rxChar};

GattService         uartService(UUID_UART, uartChars, sizeof(uartChars) / sizeof(GattCharacteristic *));

/* SWD Service setup */
uint8_t iPayload[PAYLOAD_LEN] = {0};
uint8_t oPayload[PAYLOAD_LEN] = {0};
uint8_t ckPayload[PAYLOAD_LEN] = {0};

GattCharacteristic  swiChar (UUID_SWI, iPayload, 1, PAYLOAD_LEN, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE);

GattCharacteristic  swoChar (UUID_SWO, oPayload, 1, PAYLOAD_LEN, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY);
                    
GattCharacteristic  swckChar (UUID_SWCK, ckPayload, 1, PAYLOAD_LEN, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE);
                    
GattCharacteristic *swdChars[] = {&swiChar, &swoChar, &swckChar};

GattService         swdService(UUID_SWD, swdChars, sizeof(swdChars) / sizeof(GattCharacteristic *));


bool connected = false;
void cCallback(const Gap::ConnectionCallbackParams_t *params) {
  pc.printf("Connected!\n\r");
  connected = true;
  ledCLK.detach();
  ledCLK.attach(&toggleLED, LED_ATT_RATE);
}

void dcCallback(const Gap::DisconnectionCallbackParams_t *params) {
  // restart scanning for peripheral
  ble.startAdvertising();
  ledCLK.detach();
  ledCLK.attach(&toggleLED, LED_RATE);
}

// Callback for when BLE Write is triggered (from BLE service)
void dataWrittenCallback(const GattWriteCallbackParams *Handler){   
  pc.printf("PLEAASE\n\r");
  
  // TX (incoming data from other programming chip)
  // (outputting data from TX pin)
  // (outputting data to RX pin on STM MCU)
  if (Handler->handle == txChar.getValueAttribute().getHandle()) {
    uint8_t buf[PAYLOAD_LEN];   // output of read value
    uint16_t bytesRead, i;         // bytes read (should be as long as PAYLOAD_LEN)
    ble.readCharacteristicValue(txChar.getValueAttribute().getHandle(), buf, &bytesRead);
    // Stores data in payload ??? idk what the point of this is maybe for write with response?
    memset(txPayload, 0, PAYLOAD_LEN);
    memcpy(txPayload, buf, PAYLOAD_LEN);

    // write data out TX pin to the microcontroller
    for(i = 0; i < bytesRead; i++){
      pc.putc(buf[i]);
    }
    // END OF TX SERVICE STUFF  
  }
  pc.printf("um\n\r");
}

// Callback for when STM MCU sends out Serial Data
// (outgoing data from STM's TX pin)
// (incoming data from RX pin)
// (outputting data to RX char to other programming chip)
void rxCallback(void){
  uint8_t buf[PAYLOAD_LEN];
  while(pc.readable())    
  {
    buf[0] = pc.getc();
    ble.updateCharacteristicValue(rxChar.getValueAttribute().getHandle(), buf, PAYLOAD_LEN); 
    // pc.printf("u %d %c\n\r", sizeof(buf), buf[0]);
  }
}

void toggleLED(){
  led = !led;
}

int main(void){
  ledCLK.attach(&toggleLED, LED_RATE);
  pc.baud(BAUD_RATE);

  pc.attach(&rxCallback, Serial::RxIrq);

  // initialization
  ble.init();
  ble.gap().onConnection(cCallback);
  ble.gap().onDisconnection(dcCallback);

  // Callback for when BLE service characteristic is updated
  ble.onDataWritten(dataWrittenCallback);

  // setup advertising 
  ble.gap().clearAdvertisingPayload();
  ble.accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED);
  ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
  ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::SHORTENED_LOCAL_NAME, (const uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME) - 1);
  ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME, (const uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME) - 1);
  ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::MANUFACTURER_SPECIFIC_DATA, (const uint8_t *)manufacturerData, sizeof(manufacturerData));
  ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, (const uint8_t *)serviceList, sizeof(serviceList));

  // 100ms; in multiples of 0.625ms. 
  ble.gap().setAdvertisingInterval(160);

  // add UART and SWD Service to BLE
  ble.gattServer().addService(uartService);
  ble.gattServer().addService(swdService);

  // set device name
  ble.gap().setDeviceName((const uint8_t *)DEVICE_NAME);

  ble.gap().startAdvertising(); 
  pc.printf("Advertising Start \r\n");
  
  while(1)
  {
    ble.waitForEvent(); 
  }
}