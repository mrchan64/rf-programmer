/* 
 * Bluetooth Programmer pair to be attached on the ST-LINK side
 * Acts as a BLE Central and transmits swo, sck, and tx, rx signals
 */
#include "mbed.h"
#include "ble/BLE.h"
#include "ble/DiscoveredService.h"
#include "ble/DiscoveredCharacteristic.h"
#include "BufferedSerial.h"

/* BLE Characteristic UUIDs */
#define UUID_TX       0x0101
#define UUID_RX       0x0102
#define UUID_SWI      0x0103
#define UUID_SWO      0x0104
#define UUID_SWCK     0x0105

/* BLE Service UUIDs */
#define UUID_UART     0x0111
#define UUID_SWD      0x0112

/* CONSTANTS */
#define BAUD_RATE     9600
#define BLE_SCAN_INT  1000
#define BLE_SCAN_WIN  200
#define BLE_SCAN_TO   0
#define LED_RATE      1
#define LED_ATT_RATE  0.25

/* PAYLOAD LENGTH */
#define PAYLOAD_LEN 1

/* CONNECTION NAME */
#define DEVICE_NAME "Mouse Receiver"
#define SLN 0x08

BLE  ble;
BufferedSerial pc(USBTX, USBRX); // should be longer buffer
DigitalOut led(LED1);
Ticker ledCLK;
Ticker testCLK;
Timeout t;

/* Connection storage vars */
uint8_t curr_conn_handle;
DiscoveredCharacteristic tx_char_disc, rx_char_disc, swi_char_disc, swo_char_disc, swck_char_disc;
DiscoveredCharacteristicDescriptor  tx_char_desc(NULL,GattAttribute::INVALID_HANDLE,GattAttribute::INVALID_HANDLE,UUID::ShortUUIDBytes_t(0));
DiscoveredCharacteristicDescriptor  rx_char_desc(NULL,GattAttribute::INVALID_HANDLE,GattAttribute::INVALID_HANDLE,UUID::ShortUUIDBytes_t(0));
DiscoveredCharacteristicDescriptor  swi_char_desc(NULL,GattAttribute::INVALID_HANDLE,GattAttribute::INVALID_HANDLE,UUID::ShortUUIDBytes_t(0));
DiscoveredCharacteristicDescriptor  swo_char_desc(NULL,GattAttribute::INVALID_HANDLE,GattAttribute::INVALID_HANDLE,UUID::ShortUUIDBytes_t(0));
DiscoveredCharacteristicDescriptor  swck_char_desc(NULL,GattAttribute::INVALID_HANDLE,GattAttribute::INVALID_HANDLE,UUID::ShortUUIDBytes_t(0));

bool checkAdvName(const uint8_t* advData, uint8_t advDataLen);
void scanCallback(const Gap::AdvertisementCallbackParams_t *params);
void cCallback(const Gap::ConnectionCallbackParams_t *params);
void dcCallback(const Gap::DisconnectionCallbackParams_t *params);
void hvxCallBack(const GattHVXCallbackParams *params) ;
void toggleLED();

/* Callback for scan return */
void scanCallback(const Gap::AdvertisementCallbackParams_t *params) {
  const uint8_t* p = params->advertisingData;
  uint8_t sp = params->advertisingDataLen;
  if(checkAdvName(p, sp)){
    ble.stopScan();
    ble.connect(params->peerAddr, BLEProtocol::AddressType::RANDOM_STATIC, NULL, NULL);
  }
}

/* THIS CHUNK ALL FOR SERVICE DISCOVERY */
void dsc(const DiscoveredService *service){
  // pc.printf("Service UUID: %x\n\r", service->getUUID().getShortUUID());
}

uint8_t wowl = 0;
void dcc(const DiscoveredCharacteristic *chars){
  // pc.printf("Characteristic UUID: %x\n\r", chars->getUUID().getShortUUID());
  switch(chars->getUUID().getShortUUID()){
    case UUID_TX:
      tx_char_disc = *chars;
      break;
    case UUID_RX:
      rx_char_disc = *chars;
      break;
    case UUID_SWI:
      swi_char_disc = *chars;
      break;
    case UUID_SWO:
      swo_char_disc = *chars;
      break;
    case UUID_SWCK:
      swck_char_disc = *chars;
      break;
    default:
      // pc.printf("proper dcc not found\n\r");
  }
}
/* END */

/* THIS CHUNK ALL FOR DISCOVERY TERMINATION */
static void dcdc(const CharacteristicDescriptorDiscovery::DiscoveryCallbackParams_t *params) {
  pc.printf("dcdc %x\n\r", params->characteristic.getUUID().getShortUUID());

  switch(params->characteristic.getUUID().getShortUUID()){
    case UUID_TX:
      tx_char_desc = params->descriptor;
      pc.printf("tx h %x\n\r", params->descriptor.getAttributeHandle());
      break;
    case UUID_RX:
      rx_char_desc = params->descriptor;
      pc.printf("rx h %x\n\r", params->descriptor.getAttributeHandle());
      break;
    case UUID_SWI:
      swi_char_desc = params->descriptor;
      pc.printf("swi h %x\n\r", params->descriptor.getAttributeHandle());
      break;
    case UUID_SWO:
      swo_char_desc = params->descriptor;
      pc.printf("swo h %x\n\r", params->descriptor.getAttributeHandle());
      break;
    case UUID_SWCK:
      swck_char_desc = params->descriptor;
      pc.printf("swck h %x\n\r", params->descriptor.getAttributeHandle());
      break;
    default:
      // pc.printf("proper dcdc not found\n\r");
  }
}

static void send_rx_cccd(){
  DiscoveredCharacteristic::Properties_t properties = rx_char_disc.getProperties();
  uint16_t cccd_value = (properties.notify() << 0) | (properties.indicate() << 1);

  ble_error_t error = ble.gattClient().write(
    GattClient::GATT_OP_WRITE_REQ,
    rx_char_disc.getConnectionHandle(),
    rx_char_desc.getAttributeHandle(),
    2,
    (uint8_t *) &cccd_value
  );
  if(error){
    // pc.printf("Could not write CCCD\n\r");
  }
}

static void ddtcb(const CharacteristicDescriptorDiscovery::TerminationCallbackParams_t *params) {
  // pc.printf("ddtcb\n\r");
  if(params->characteristic.getUUID().getShortUUID() == UUID_RX){
    send_rx_cccd();
  }
  // testCLK.attach(&test_func, 1);
}

static void discoveryTerminationCallBack(Gap::Handle_t connectionHandle) {
  ble.gattClient().discoverCharacteristicDescriptors(tx_char_disc, dcdc, ddtcb);
  ble.gattClient().discoverCharacteristicDescriptors(rx_char_disc, dcdc, ddtcb);
  ble.gattClient().discoverCharacteristicDescriptors(swi_char_disc, dcdc, ddtcb);
  ble.gattClient().discoverCharacteristicDescriptors(swo_char_disc, dcdc, ddtcb);
  ble.gattClient().discoverCharacteristicDescriptors(swck_char_disc, dcdc, ddtcb);
}
/* END */

void cCallback(const Gap::ConnectionCallbackParams_t *params) {
  pc.printf("Connected! \n\r");
  ledCLK.detach();
  ledCLK.attach(&toggleLED, LED_ATT_RATE);
  ble.gattClient().launchServiceDiscovery(params->handle, dsc, dcc);
  curr_conn_handle = params->handle;
}

void dcCallback(const Gap::DisconnectionCallbackParams_t *params) {
  // restart scanning for peripheral
  pc.printf("Disconnected!\n\r");
  ble.startScan(scanCallback);
  ledCLK.detach();
  ledCLK.attach(&toggleLED, LED_RATE);
}


void toggleLED(){
  led = !led;
}

/* Checks Device Name (if exists) */
bool checkAdvName(const uint8_t* advData, uint8_t advDataLen){
  uint8_t i = 0;
  char* devName = "";
  while(i < advDataLen){
    uint8_t dat_size = advData[i];
    if(dat_size==0)break;
    if(GapAdvertisingData::SHORTENED_LOCAL_NAME==advData[i+1]){
      devName = (char*) (advData+i+2);
      break;
    }
    i += dat_size + 1;
  }
  return strcmp(DEVICE_NAME, devName)==0;
}

void onDataWriteCallBack(const GattWriteCallbackParams *params) {
  // pc.printf("wawkefjowjowef\n\r");
}

/* for when data comes in from the mouse */
void onDataReadCallBack(const GattReadCallbackParams *params) {
  pc.printf("hi\n\r");
}

/* HVX callback (when characteristic value gets updated??) */
void hvxCallBack(const GattHVXCallbackParams *params) {
  if(params->handle == rx_char_disc.getValueHandle()){
    pc.putc(params->data[0]);
  }
}

int main(void){

  ledCLK.attach(&toggleLED, LED_RATE);
  pc.baud(BAUD_RATE);

  pc.printf("\n\rStarting scan\n\r");

  // initialization
  ble.init();
  ble.onConnection(cCallback);
  ble.onDisconnection(dcCallback);
  ble.gattClient().onServiceDiscoveryTermination(discoveryTerminationCallBack);
  ble.gattClient().onHVX(hvxCallBack);
  ble.gattClient().onDataWrite(onDataWriteCallBack);
  ble.gattClient().onDataRead(onDataReadCallBack);

  // start scan
  ble.setScanParams(BLE_SCAN_INT, BLE_SCAN_WIN, BLE_SCAN_TO, false);
  ble.startScan(scanCallback);

  while(true){
    ble.waitForEvent();
  }

}